// Copies files to and from a Switch SD card over MTP (the console's own USB
// file transfer, or haze/DBI). Built and run by scripts/switch/push.sh.
//
//   switch_mtp push LOCAL REMOTE_DIR [VERIFY_COPY]
//   switch_mtp push-many REMOTE_DIR LOCAL...
//   switch_mtp pull REMOTE_DIR NAME LOCAL
//
// push-many sends many files in one session and skips any whose remote copy
// already has the same size (game data: 415 RELs, a 1.4 GB disc image).
//
// The console's MTP server renumbers objects in every session, so each command
// resolves REMOTE_DIR by name, and push replaces, sends and (optionally) reads
// the file back within one session. stock mtp-sendfile cannot infer the
// storage of a parent folder on this server, so storage and parent are explicit.
#include <libmtp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static LIBMTP_mtpdevice_t* g_device;

static int fail(const char* what) {
    fprintf(stderr, "switch_mtp: %s\n", what);
    if (g_device != NULL)
        LIBMTP_Dump_Errorstack(g_device);
    return 1;
}

// Returns the id of the named child of dir (a folder when want_folder is set,
// a file otherwise), or 0. LIBMTP_FILES_AND_FOLDERS_ROOT lists the storage root.
static uint32_t find_child(uint32_t storage, uint32_t dir, const char* name, int want_folder) {
    uint32_t id = 0;
    LIBMTP_file_t* files = LIBMTP_Get_Files_And_Folders(g_device, storage, dir);
    for (LIBMTP_file_t* file = files; file != NULL;) {
        LIBMTP_file_t* next = file->next;
        const int is_folder = file->filetype == LIBMTP_FILETYPE_FOLDER;
        if (id == 0 && is_folder == want_folder && strcmp(file->filename, name) == 0)
            id = file->item_id;
        LIBMTP_destroy_file_t(file);
        file = next;
    }
    return id;
}

// Resolves a slash-separated directory from the storage root, creating any
// missing components when create is set. Returns 0 if it cannot.
static uint32_t resolve_dir(uint32_t storage, const char* path, int create) {
    uint32_t dir = LIBMTP_FILES_AND_FOLDERS_ROOT;
    char* copy = strdup(path);
    for (char* name = strtok(copy, "/"); name != NULL; name = strtok(NULL, "/")) {
        uint32_t child = find_child(storage, dir, name, 1);
        if (child == 0 && create) {
            char* created_name = strdup(name);
            const uint32_t parent = dir == LIBMTP_FILES_AND_FOLDERS_ROOT ? 0 : dir;
            child = LIBMTP_Create_Folder(g_device, created_name, parent, storage);
            free(created_name);
        }
        dir = child;
        if (dir == 0)
            break;
    }
    free(copy);
    return dir == LIBMTP_FILES_AND_FOLDERS_ROOT ? 0 : dir;
}

static uint32_t find_file(uint32_t storage, uint32_t dir, const char* name) {
    return find_child(storage, dir, name, 0);
}

static int push(uint32_t storage, const char* local, const char* remote_dir,
                const char* verify_copy) {
    struct stat info;
    if (stat(local, &info) != 0) {
        perror(local);
        return 1;
    }
    const char* slash = strrchr(local, '/');
    const char* name = slash != NULL ? slash + 1 : local;

    const uint32_t dir = resolve_dir(storage, remote_dir, 1);
    if (dir == 0)
        return fail("could not find or create the remote directory");
    const uint32_t old = find_file(storage, dir, name);
    if (old != 0 && LIBMTP_Delete_Object(g_device, old) != 0)
        return fail("could not delete the previous copy");

    LIBMTP_file_t* file = LIBMTP_new_file_t();
    file->filename = strdup(name);
    file->filesize = (uint64_t)info.st_size;
    file->filetype = LIBMTP_FILETYPE_UNKNOWN;
    file->parent_id = dir;
    file->storage_id = storage;
    const int sent = LIBMTP_Send_File_From_File(g_device, local, file, NULL, NULL);
    const uint32_t id = file->item_id;
    LIBMTP_destroy_file_t(file);
    if (sent != 0)
        return fail("send failed");
    printf("sent %s/%s (%lld bytes)\n", remote_dir, name, (long long)info.st_size);

    if (verify_copy != NULL &&
        LIBMTP_Get_File_To_File(g_device, id, verify_copy, NULL, NULL) != 0)
        return fail("could not read the sent file back");
    return 0;
}

static int push_many(uint32_t storage, const char* remote_dir, int count, char** locals) {
    const uint32_t dir = resolve_dir(storage, remote_dir, 1);
    if (dir == 0)
        return fail("could not find or create the remote directory");
    // One listing up front: name, id and size of what is already there.
    LIBMTP_file_t* existing = LIBMTP_Get_Files_And_Folders(g_device, storage, dir);
    int sent = 0, skipped = 0;
    for (int i = 0; i < count; ++i) {
        struct stat info;
        if (stat(locals[i], &info) != 0) {
            perror(locals[i]);
            return 1;
        }
        const char* slash = strrchr(locals[i], '/');
        const char* name = slash != NULL ? slash + 1 : locals[i];
        uint32_t old = 0;
        uint64_t old_size = 0;
        for (LIBMTP_file_t* file = existing; file != NULL; file = file->next) {
            if (file->filetype != LIBMTP_FILETYPE_FOLDER && strcmp(file->filename, name) == 0) {
                old = file->item_id;
                old_size = file->filesize;
                break;
            }
        }
        if (old != 0 && old_size == (uint64_t)info.st_size) {
            ++skipped;
            continue;
        }
        if (old != 0 && LIBMTP_Delete_Object(g_device, old) != 0)
            return fail("could not delete a previous copy");
        LIBMTP_file_t* file = LIBMTP_new_file_t();
        file->filename = strdup(name);
        file->filesize = (uint64_t)info.st_size;
        file->filetype = LIBMTP_FILETYPE_UNKNOWN;
        file->parent_id = dir;
        file->storage_id = storage;
        const int result = LIBMTP_Send_File_From_File(g_device, locals[i], file, NULL, NULL);
        LIBMTP_destroy_file_t(file);
        if (result != 0)
            return fail("send failed");
        ++sent;
        if (sent % 50 == 0 || info.st_size > (64 << 20))
            printf("  %s/%s (%lld bytes)\n", remote_dir, name, (long long)info.st_size);
    }
    while (existing != NULL) {
        LIBMTP_file_t* next = existing->next;
        LIBMTP_destroy_file_t(existing);
        existing = next;
    }
    printf("%s: %d sent, %d already present\n", remote_dir, sent, skipped);
    return 0;
}

static int pull(uint32_t storage, const char* remote_dir, const char* name,
                const char* local) {
    const uint32_t dir = resolve_dir(storage, remote_dir, 0);
    const uint32_t id = dir != 0 ? find_file(storage, dir, name) : 0;
    if (id == 0) {
        fprintf(stderr, "switch_mtp: %s/%s not found\n", remote_dir, name);
        return 3;
    }
    if (LIBMTP_Get_File_To_File(g_device, id, local, NULL, NULL) != 0)
        return fail("download failed");
    printf("pulled %s/%s\n", remote_dir, name);
    return 0;
}

int main(int argc, char** argv) {
    const int is_push = argc >= 4 && argc <= 5 && strcmp(argv[1], "push") == 0;
    const int is_push_many = argc >= 4 && strcmp(argv[1], "push-many") == 0;
    const int is_pull = argc == 5 && strcmp(argv[1], "pull") == 0;
    if (!is_push && !is_push_many && !is_pull) {
        fprintf(stderr, "usage: %s push LOCAL REMOTE_DIR [VERIFY_COPY]\n"
                        "       %s push-many REMOTE_DIR LOCAL...\n"
                        "       %s pull REMOTE_DIR NAME LOCAL\n", argv[0], argv[0], argv[0]);
        return 2;
    }
    LIBMTP_Init();
    // Uncached: libmtp refuses per-folder listings on a cached device.
    LIBMTP_raw_device_t* raw = NULL;
    int raw_count = 0;
    if (LIBMTP_Detect_Raw_Devices(&raw, &raw_count) == LIBMTP_ERROR_NONE && raw_count > 0)
        g_device = LIBMTP_Open_Raw_Device_Uncached(&raw[0]);
    free(raw);
    if (g_device == NULL) {
        fprintf(stderr, "switch_mtp: no MTP device; enable USB file transfer on the console\n");
        return 1;
    }
    int result = 1;
    if (LIBMTP_Get_Storage(g_device, LIBMTP_STORAGE_SORTBY_NOTSORTED) != 0 ||
        g_device->storage == NULL) {
        result = fail("the device reports no storage");
    } else {
        // DBI and haze can expose several storages; use the SD card.
        uint32_t storage = g_device->storage->id;
        for (LIBMTP_devicestorage_t* s = g_device->storage; s != NULL; s = s->next) {
            if (s->StorageDescription != NULL && strstr(s->StorageDescription, "SD") != NULL) {
                storage = s->id;
                break;
            }
        }
        if (is_push)
            result = push(storage, argv[2], argv[3], argc == 5 ? argv[4] : NULL);
        else if (is_push_many)
            result = push_many(storage, argv[2], argc - 3, argv + 3);
        else
            result = pull(storage, argv[2], argv[3], argv[4]);
    }
    LIBMTP_Release_Device(g_device);
    return result;
}
