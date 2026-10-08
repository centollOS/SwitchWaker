// Disc check of the run harness (docs/NATIVE_PORT_PHASE4_6.md, step 6.0, decision H9).
//
// COS_DISC must name a readable GameCube disc image (.iso) of GZLE01 revision 0: the disc header
// (boot.bin) holds the game code and maker at 0x00, the version byte at 0x07 and the GameCube
// magic 0xC2339F3D at 0x1C, all big-endian. Anything else exits 14 before the SDK starts.
// Opening the disc for the game (aurora_dvd_open) is step 6.1; the SHA-1 check of the image on
// first use is done by native/tools/run.sh (main.dol SHA-1 against the supported revision).
#include "pc_internal.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

namespace pc {

namespace {
char sDiscError[512];
}

void discMessage(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sDiscError, sizeof sDiscError, fmt, ap);
    va_end(ap);
    writef(STDERR_FILENO, "[cos] DISC: %s\n", sDiscError);
}

void exitDisc() {
#if defined(__SWITCH__)
    char text[1024];
    snprintf(text, sizeof text,
             "SwitchWaker needs your own disc of The Wind Waker (USA, GZLE01, revision 0) as an uncompressed "
             "image at sdmc:/switch/switchwaker/GZLE01.iso.\n\n%s\n\n"
             "SwitchWaker necesita tu propio disco de The Wind Waker (EE. UU., GZLE01, revision 0) como "
             "imagen sin comprimir en sdmc:/switch/switchwaker/GZLE01.iso.",
             sDiscError);
    cos_switch_show_error(text);
#endif
    pc_exit(PC_EXIT_DISC);
    __builtin_unreachable();
}

int checkDisc() {
    const char* path = gConfig.disc;
    if (path == nullptr) {
        discMessage("COS_DISC is not set (path of the GZLE01 .iso)");
        return PC_EXIT_DISC;
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        discMessage("cannot open COS_DISC=%s: %s", path, strerror(errno));
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
        discMessage("%s is not a regular file with a disc header", path);
        return PC_EXIT_DISC;
    }
    if (memcmp(header, "CISO", 4) == 0) {
        discMessage("%s is a CISO image; convert it to a plain .iso (decision H9)", path);
        return PC_EXIT_DISC;
    }
    const uint32_t magic = ((uint32_t)header[0x1C] << 24) | ((uint32_t)header[0x1D] << 16) |
                           ((uint32_t)header[0x1E] << 8) | (uint32_t)header[0x1F];
    if (magic != 0xC2339F3Du) {
        discMessage("%s is not a GameCube disc image (magic 0x%08x at 0x1C; compressed?)", path, magic);
        return PC_EXIT_DISC;
    }
    char id[7];
    for (int i = 0; i < 6; i++) {
        id[i] = (header[i] >= 0x20 && header[i] < 0x7F) ? (char)header[i] : '?';
    }
    id[6] = '\0';
    const unsigned int version = header[7];
    if (strcmp(id, "GZLE01") != 0 || version != 0) {
        discMessage("%s is %s revision %u; the supported disc is GZLE01 revision 0", path, id, version);
        return PC_EXIT_DISC;
    }
    writef(STDERR_FILENO, "[cos] disc: %s GZLE01 revision 0, %lld bytes\n", path,
           (long long)st.st_size);
    return 0;
}

} // namespace pc
