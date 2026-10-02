// Copies files to and from a Switch SD card over MTP (the console's own USB
// file transfer, or haze/DBI). Built and run by scripts/switch/push.sh.
//
//   switch_mtp push LOCAL REMOTE_DIR [VERIFY_COPY]
//   switch_mtp pull REMOTE_DIR NAME LOCAL
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

static LIBMTP_folder_t* find_child(LIBMTP_folder_t* first, const char* name) {
    for (LIBMTP_folder_t* folder = first; folder != NULL; folder = folder->sibling) {
        if (strcmp(folder->name, name) == 0)
            return folder;
    }
    return NULL;
}

// Resolves a slash-separated directory from the storage root, creating any
// missing components when create is set. Returns 0 if it cannot.
static uint32_t resolve_dir(uint32_t storage, const char* path, int create) {
    LIBMTP_folder_t* folders = LIBMTP_Get_Folder_List_For_Storage(g_device, storage);
    LIBMTP_folder_t* level = folders;
    uint32_t parent = 0;
    char* copy = strdup(path);
    for (char* name = strtok(copy, "/"); name != NULL; name = strtok(NULL, "/")) {
        LIBMTP_folder_t* match = find_child(level, name);
        if (match != NULL) {
            parent = match->folder_id;
            level = match->child;
            continue;
        }
        if (!create) {
            parent = 0;
            break;
        }
        char* created_name = strdup(name);
        parent = LIBMTP_Create_Folder(g_device, created_name, parent, storage);
        free(created_name);
        if (parent == 0)
            break;
        level = NULL;
    }
    free(copy);
    LIBMTP_destroy_folder_t(folders);
    return parent;
}

static uint32_t find_file(uint32_t storage, uint32_t dir, const char* name) {
    uint32_t id = 0;
    LIBMTP_file_t* files = LIBMTP_Get_Files_And_Folders(g_device, storage, dir);
    for (LIBMTP_file_t* file = files; file != NULL;) {
        LIBMTP_file_t* next = file->next;
        if (id == 0 && file->filetype != LIBMTP_FILETYPE_FOLDER &&
            strcmp(file->filename, name) == 0)
            id = file->item_id;
        LIBMTP_destroy_file_t(file);
        file = next;
    }
    return id;
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
    const int is_pull = argc == 5 && strcmp(argv[1], "pull") == 0;
    if (!is_push && !is_pull) {
        fprintf(stderr, "usage: %s push LOCAL REMOTE_DIR [VERIFY_COPY]\n"
                        "       %s pull REMOTE_DIR NAME LOCAL\n", argv[0], argv[0]);
        return 2;
    }
    LIBMTP_Init();
    g_device = LIBMTP_Get_First_Device();
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
        result = is_push ? push(storage, argv[2], argv[3], argc == 5 ? argv[4] : NULL)
                         : pull(storage, argv[2], argv[3], argv[4]);
    }
    LIBMTP_Release_Device(g_device);
    return result;
}
