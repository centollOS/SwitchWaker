// sqlite on Horizon. Its Unix VFS names two POSIX calls newlib lacks; the SD
// card has no users or file ownership, so everything runs as user 0 and an
// ownership change has nothing to do. And it locks database files with
// fcntl, which newlib does not support: every first read failed with
// "disk I/O error". One process uses the caches, so the default VFS becomes
// sqlite's lock-free "unix-none" before anything opens a database.
#include <sys/types.h>

#include "sqlite3.h"

uid_t geteuid(void) { return 0; }

int fchown(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

// Runs before main; this object is always linked, since sqlite3.o needs the
// two functions above.
__attribute__((constructor)) static void use_lock_free_vfs(void) {
    sqlite3_vfs* vfs = sqlite3_vfs_find("unix-none");
    if (vfs != NULL)
        sqlite3_vfs_register(vfs, 1);
}
