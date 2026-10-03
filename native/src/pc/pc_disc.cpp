// Disc check of the run harness (docs/NATIVE_PORT_PHASE4_6.md, step 6.0, decision H9).
//
// COS_DISC must name a readable GameCube disc image (.iso) of GZLE01 revision 0: the disc header
// (boot.bin) holds the game code and maker at 0x00, the version byte at 0x07 and the GameCube
// magic 0xC2339F3D at 0x1C, all big-endian. Anything else exits 14 before the SDK starts.
// Opening the disc for the game (aurora_dvd_open) is step 6.1; the SHA-1 check of the image on
// first use is done by native/tools/run.sh (main.dol SHA-1 against the supported revision).
#include "pc_internal.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pc {

int checkDisc() {
    const char* path = gConfig.disc;
    if (path == nullptr) {
        writef(STDERR_FILENO, "[cos] DISC: COS_DISC is not set (path of the GZLE01 .iso)\n");
        return PC_EXIT_DISC;
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        writef(STDERR_FILENO, "[cos] DISC: cannot open COS_DISC=%s: %s\n", path, strerror(errno));
        return PC_EXIT_DISC;
    }
    struct stat st;
    unsigned char header[0x20];
    ssize_t got = -1;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
#if defined(__SWITCH__)
        // Horizon's C library has no pread.
        got = lseek(fd, 0, SEEK_SET) == 0 ? read(fd, header, sizeof(header)) : -1;
#else
        got = pread(fd, header, sizeof(header), 0);
#endif
    }
    close(fd);
    if (got != (ssize_t)sizeof(header)) {
        writef(STDERR_FILENO, "[cos] DISC: %s is not a regular file with a disc header\n", path);
        return PC_EXIT_DISC;
    }
    if (memcmp(header, "CISO", 4) == 0) {
        writef(STDERR_FILENO, "[cos] DISC: %s is a CISO image; the supported disc is the plain .iso "
                              "(decision H9)\n", path);
        return PC_EXIT_DISC;
    }
    const uint32_t magic = ((uint32_t)header[0x1C] << 24) | ((uint32_t)header[0x1D] << 16) |
                           ((uint32_t)header[0x1E] << 8) | (uint32_t)header[0x1F];
    if (magic != 0xC2339F3Du) {
        writef(STDERR_FILENO, "[cos] DISC: %s has no GameCube disc magic (0x%08x at 0x1C)\n", path,
               magic);
        return PC_EXIT_DISC;
    }
    char id[7];
    for (int i = 0; i < 6; i++) {
        id[i] = (header[i] >= 0x20 && header[i] < 0x7F) ? (char)header[i] : '?';
    }
    id[6] = '\0';
    const unsigned int version = header[7];
    if (strcmp(id, "GZLE01") != 0 || version != 0) {
        writef(STDERR_FILENO, "[cos] DISC: %s is %s revision %u; the supported disc is GZLE01 "
                              "revision 0\n", path, id, version);
        return PC_EXIT_DISC;
    }
    writef(STDERR_FILENO, "[cos] disc: %s GZLE01 revision 0, %lld bytes\n", path,
           (long long)st.st_size);
    return 0;
}

} // namespace pc
