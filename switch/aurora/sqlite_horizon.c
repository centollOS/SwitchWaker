// sqlite on Horizon. Its Unix VFS names two POSIX calls newlib lacks; the SD
// card has no users or file ownership, so everything runs as user 0 and an
// ownership change has nothing to do. And it locks database files with
// fcntl, which newlib does not support: every first read failed with
// "disk I/O error". One process uses the caches, so the default VFS becomes
// sqlite's lock-free "unix-none" before anything opens a database.
//
// stat() of a file this process has open fails on Horizon: libnx's fsdev
// stat opens the file (read mode) to read its size, and the file system
// refuses a second open of a file that is open for writing. sqlite stats the
// database (open read/write) when it creates the rollback journal, to give the
// journal the database's permissions (os_unix.c getFileMode): with
// journal_mode=PERSIST every first write failed with SQLITE_IOERR_FSTAT (1802)
// on the console. The VFS's open, close and stat system calls are wrapped:
// the files sqlite has open are remembered, and a stat of one of them that
// fails is answered with fstat() of sqlite's own descriptor (same size and
// mode). Built on the host with COS_SQLITE_HORIZON_HOST_TEST by
// sqlite_kill_test.c, which emulates Horizon's stat to check this.
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "sqlite3.h"

#if !defined(COS_SQLITE_HORIZON_HOST_TEST)
uid_t geteuid(void) { return 0; }

int fchown(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}
#endif

// The files sqlite has open through its Unix VFS (path -> descriptor). Two
// caches with a database and a journal each, plus the bundled seed database.
enum { kMaxOpen = 16, kMaxPath = 512 };
static struct {
    int fd;
    char path[kMaxPath];
} s_open[kMaxOpen];
static sqlite3_mutex* s_openMutex;

typedef int (*OpenFn)(const char*, int, int);
typedef int (*CloseFn)(int);
typedef int (*StatFn)(const char*, struct stat*);
static OpenFn s_realOpen;
static CloseFn s_realClose;
static StatFn s_realStat;

static int horizon_open(const char* path, int flags, int mode) {
    const int fd = s_realOpen(path, flags, mode);
    if (fd >= 0 && strlen(path) < kMaxPath) {
        sqlite3_mutex_enter(s_openMutex);
        for (int i = 0; i < kMaxOpen; i++) {
            if (s_open[i].path[0] == '\0') {
                s_open[i].fd = fd;
                strcpy(s_open[i].path, path);
                break;
            }
        }
        sqlite3_mutex_leave(s_openMutex);
    }
    return fd;
}

static int horizon_close(int fd) {
    sqlite3_mutex_enter(s_openMutex);
    for (int i = 0; i < kMaxOpen; i++) {
        if (s_open[i].path[0] != '\0' && s_open[i].fd == fd) {
            s_open[i].path[0] = '\0';
            break;
        }
    }
    sqlite3_mutex_leave(s_openMutex);
    return s_realClose(fd);
}

static int horizon_stat(const char* path, struct stat* st) {
    const int rc = s_realStat(path, st);
    if (rc == 0)
        return 0;
    const int err = errno;
    int fd = -1;
    sqlite3_mutex_enter(s_openMutex);
    for (int i = 0; i < kMaxOpen; i++) {
        if (s_open[i].path[0] != '\0' && strcmp(s_open[i].path, path) == 0) {
            fd = s_open[i].fd;
            break;
        }
    }
    sqlite3_mutex_leave(s_openMutex);
    if (fd >= 0 && fstat(fd, st) == 0)
        return 0;
    errno = err;
    return rc;
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

// The lock-free VFS as the default, with the open/close/stat wrappers. The
// system call table is shared by every unix VFS of the process.
static void cos_sqlite_horizon_setup(void) {
    sqlite3_vfs* vfs = sqlite3_vfs_find("unix-none");
    if (vfs == NULL)
        return;
    s_openMutex = sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_APP1);
    s_realOpen = (OpenFn)vfs->xGetSystemCall(vfs, "open");
    s_realClose = (CloseFn)vfs->xGetSystemCall(vfs, "close");
    s_realStat = (StatFn)vfs->xGetSystemCall(vfs, "stat");
    if (s_realOpen != NULL && s_realClose != NULL && s_realStat != NULL) {
        vfs->xSetSystemCall(vfs, "open", (sqlite3_syscall_ptr)horizon_open);
        vfs->xSetSystemCall(vfs, "close", (sqlite3_syscall_ptr)horizon_close);
        vfs->xSetSystemCall(vfs, "stat", (sqlite3_syscall_ptr)horizon_stat);
    }
    sqlite3_vfs_register(vfs, 1);
}

#if !defined(COS_SQLITE_HORIZON_HOST_TEST)
// Runs before main; this object is always linked, since sqlite3.o needs the
// two functions above. SQLITE_CONFIG_LOG must come before sqlite3_initialize,
// which sqlite3_vfs_find calls.
__attribute__((constructor)) static void use_lock_free_vfs(void) {
    sqlite3_config(SQLITE_CONFIG_LOG, log_to_stderr, NULL);
    cos_sqlite_horizon_setup();
}
#endif
