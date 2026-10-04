// sqlite on Horizon. Its Unix VFS names two POSIX calls newlib lacks; the SD
// card has no users or file ownership, so everything runs as user 0 and an
// ownership change has nothing to do. And it locks database files with
// fcntl, which newlib does not support: every first read failed with
// "disk I/O error". One process uses the caches, so the default VFS becomes
// sqlite's lock-free "unix-none" before anything opens a database.
#include <stdio.h>
#include <sys/types.h>

#include "sqlite3.h"

uid_t geteuid(void) { return 0; }

int fchown(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

// sqlite's own error log (SQLITE_CONFIG_LOG) on stderr, which the run log
// keeps: a failing statement then says which check failed ("database
// corruption at line N") and which system call returned which errno
// ("os_unix.c:N: (errno) open(path)"), where the API only returns a code.
// The first 64 messages, then every 1000th with the count: a cache that kept
// failing once wrote ~40 000 of these lines in one run.
static void log_to_stderr(void* unused, int code, const char* message) {
    static unsigned long count; // two threads may race here; a miscount is harmless
    (void)unused;
    ++count;
    if (count <= 64)
        fprintf(stderr, "[sqlite] (%d) %s\n", code, message);
    else if (count % 1000 == 0)
        fprintf(stderr, "[sqlite] (%d) %s (%lu sqlite messages so far, most not shown)\n", code, message, count);
}

// Runs before main; this object is always linked, since sqlite3.o needs the
// two functions above. SQLITE_CONFIG_LOG must come before sqlite3_initialize,
// which sqlite3_vfs_find calls.
__attribute__((constructor)) static void use_lock_free_vfs(void) {
    sqlite3_config(SQLITE_CONFIG_LOG, log_to_stderr, NULL);
    sqlite3_vfs* vfs = sqlite3_vfs_find("unix-none");
    if (vfs != NULL)
        sqlite3_vfs_register(vfs, 1);
}
