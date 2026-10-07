// SDL 3 file I/O and paths for the Switch shim, over newlib stdio and the
// SD card. Aurora's base and preference paths are the app's data directory,
// written without the "sdmc:" device: an NRO started from the SD card has it
// as the default device, and sqlite (Aurora's caches) treats a path not
// starting with '/' as relative and prepends the working directory.
#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sdl3_shim.h"

#define DATA_ROOT "/switch/switchwaker/"

struct SDL_IOStream {
    FILE* file;
    SDL_IOStatus status;
};

SDL_IOStream* SDL_IOFromFile(const char* file, const char* mode) {
    FILE* handle = file != NULL && mode != NULL ? fopen(file, mode) : NULL;
    if (handle == NULL) {
        sdl3_shim_set_error("could not open %s: %s", file != NULL ? file : "(null)",
                            strerror(errno));
        return NULL;
    }
    SDL_IOStream* stream = calloc(1, sizeof *stream);
    if (stream == NULL) {
        fclose(handle);
        return NULL;
    }
    stream->file = handle;
    stream->status = SDL_IO_STATUS_READY;
    return stream;
}

bool SDL_CloseIO(SDL_IOStream* context) {
    if (context == NULL)
        return false;
    const bool closed = fclose(context->file) == 0;
    free(context);
    return closed;
}

bool SDL_FlushIO(SDL_IOStream* context) {
    return context != NULL && fflush(context->file) == 0;
}

size_t SDL_ReadIO(SDL_IOStream* context, void* ptr, size_t size) {
    if (context == NULL)
        return 0;
    const size_t read = fread(ptr, 1, size, context->file);
    if (read < size)
        context->status = ferror(context->file) ? SDL_IO_STATUS_ERROR : SDL_IO_STATUS_EOF;
    return read;
}

size_t SDL_WriteIO(SDL_IOStream* context, const void* ptr, size_t size) {
    if (context == NULL)
        return 0;
    const size_t written = fwrite(ptr, 1, size, context->file);
    if (written < size)
        context->status = SDL_IO_STATUS_ERROR;
    return written;
}

Sint64 SDL_SeekIO(SDL_IOStream* context, Sint64 offset, SDL_IOWhence whence) {
    if (context == NULL)
        return -1;
    const int origin = whence == SDL_IO_SEEK_CUR ? SEEK_CUR
                       : whence == SDL_IO_SEEK_END ? SEEK_END
                                                   : SEEK_SET;
    if (fseeko(context->file, (off_t)offset, origin) != 0)
        return -1;
    context->status = SDL_IO_STATUS_READY;
    return (Sint64)ftello(context->file);
}

Sint64 SDL_TellIO(SDL_IOStream* context) {
    return context != NULL ? (Sint64)ftello(context->file) : -1;
}

Sint64 SDL_GetIOSize(SDL_IOStream* context) {
    if (context == NULL)
        return -1;
    struct stat info;
    return fstat(fileno(context->file), &info) == 0 ? (Sint64)info.st_size : -1;
}

SDL_IOStatus SDL_GetIOStatus(SDL_IOStream* context) {
    return context != NULL ? context->status : SDL_IO_STATUS_ERROR;
}

bool SDL_ReadU32LE(SDL_IOStream* src, Uint32* value) {
    Uint8 bytes[4];
    if (SDL_ReadIO(src, bytes, sizeof bytes) != sizeof bytes)
        return false;
    if (value != NULL)
        *value = (Uint32)bytes[0] | (Uint32)bytes[1] << 8 | (Uint32)bytes[2] << 16 |
                 (Uint32)bytes[3] << 24;
    return true;
}

bool SDL_WriteU8(SDL_IOStream* dst, Uint8 value) {
    return SDL_WriteIO(dst, &value, 1) == 1;
}

bool SDL_WriteU32LE(SDL_IOStream* dst, Uint32 value) {
    const Uint8 bytes[4] = {(Uint8)value, (Uint8)(value >> 8), (Uint8)(value >> 16),
                            (Uint8)(value >> 24)};
    return SDL_WriteIO(dst, bytes, sizeof bytes) == sizeof bytes;
}

bool SDL_WriteS32LE(SDL_IOStream* dst, Sint32 value) {
    return SDL_WriteU32LE(dst, (Uint32)value);
}

const char* SDL_GetBasePath(void) { return DATA_ROOT; }

char* SDL_GetPrefPath(const char* org, const char* app) {
    (void)org;
    (void)app;
    mkdir(DATA_ROOT, 0777);
    return strdup(DATA_ROOT);
}

// Creates every missing component, as SDL does.
bool SDL_CreateDirectory(const char* path) {
    if (path == NULL)
        return false;
    char partial[512];
    const size_t length = strlen(path);
    if (length >= sizeof partial)
        return false;
    for (size_t i = 1; i <= length; ++i) {
        if (path[i] == '/' || path[i] == '\0') {
            memcpy(partial, path, i);
            partial[i] = '\0';
            if (mkdir(partial, 0777) != 0 && errno != EEXIST) {
                struct stat info;
                // "sdmc:" itself cannot be created; it only has to exist.
                if (stat(partial, &info) != 0 && partial[i - 1] != ':') {
                    sdl3_shim_set_error("could not create %s: %s", partial, strerror(errno));
                    return false;
                }
            }
        }
    }
    return true;
}

bool SDL_GetPathInfo(const char* path, SDL_PathInfo* info) {
    struct stat status;
    if (path == NULL || stat(path, &status) != 0) {
        if (info != NULL)
            memset(info, 0, sizeof *info);
        return false;
    }
    if (info != NULL) {
        memset(info, 0, sizeof *info);
        info->type = S_ISDIR(status.st_mode)   ? SDL_PATHTYPE_DIRECTORY
                     : S_ISREG(status.st_mode) ? SDL_PATHTYPE_FILE
                                               : SDL_PATHTYPE_OTHER;
        info->size = (Uint64)status.st_size;
        info->modify_time = (SDL_Time)status.st_mtime * 1000000000LL;
        info->create_time = info->modify_time;
        info->access_time = info->modify_time;
    }
    return true;
}

bool SDL_RemovePath(const char* path) {
    if (path == NULL)
        return false;
    if (remove(path) == 0 || rmdir(path) == 0 || errno == ENOENT)
        return true;
    sdl3_shim_set_error("could not remove %s: %s", path, strerror(errno));
    return false;
}
