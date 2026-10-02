// POSIX calls sqlite's Unix VFS names that newlib on Horizon lacks. The SD
// card has no users or file ownership: everything runs as user 0 and an
// ownership change has nothing to do.
#include <sys/types.h>

uid_t geteuid(void) { return 0; }

int fchown(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}
