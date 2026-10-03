// Milestones and the game frame counter (docs/NATIVE_PORT_PHASE4_6.md, step 6.0 and the milestone
// table M0-M14). A milestone is logged as
//   [cos] MILESTONE <name> frame=<game frames> retrace=<VI retraces> ms=<since start>
// and ends the process with exit 0 when it is COS_MILESTONE. The code that reaches each milestone
// calls pc_milestone (static-init from pc_smoke.cpp; the others from their steps, 6.1 on).
#include "pc_internal.h"

#include <dolphin/vi.h>

#include <atomic>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

// M0-M14, in table order (M11/M14 are both the real new-game flow).
const char* const kMilestones[] = {
    "static-init",    // M0
    "aurora-up",      // M1
    "heaps",          // M2
    "gfx-create",     // M3
    "frame-loop",     // M4
    "logo-scene",     // M5
    "logo-res",       // M6
    "opening",        // M7
    "title-stage",    // M8
    "title",          // M9
    "file-select",    // M10
    "new-game",       // M11
    "outset-debug",   // M12
    "outset-control", // M13
    "outset-real",    // M14
};

std::atomic<unsigned int> sFrames{0};

} // namespace

bool isKnownMilestone(const char* name) {
    for (const char* m : kMilestones) {
        if (strcmp(m, name) == 0) {
            return true;
        }
    }
    return false;
}

void printMilestones(int fd) {
    for (const char* m : kMilestones) {
        writef(fd, " %s", m);
    }
    writef(fd, "\n");
}

} // namespace pc

using namespace pc;

extern "C" {

void pc_milestone(const char* name) {
    writef(STDERR_FILENO, "[cos] MILESTONE %s frame=%u retrace=%u ms=%llu\n", name,
           sFrames.load(std::memory_order_relaxed), (unsigned int)VIGetRetraceCount(),
           (unsigned long long)elapsedMs());
    if (gConfig.milestone != nullptr && strcmp(gConfig.milestone, name) == 0) {
        pc_exit(PC_EXIT_REACHED);
    }
}

void pc_copydate_loaded(int status, const char* copydate) {
    // The placeholder m_Do_main.cpp starts with; DVDReadPrio replaces it with the disc's date.
    bool read = status != 0 && copydate != nullptr && copydate[0] != '?';
    writef(STDERR_FILENO, "[cos] gfx-create: LOAD_COPYDATE status %d, COPYDATE \"%s\"\n", status,
           copydate != nullptr ? copydate : "(null)");
    if (!read) {
        writef(STDERR_FILENO, "[cos] gfx-create: /COPYDATE was not read\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    pc_milestone("gfx-create");
}

void pc_frame_tick(void) {
    unsigned int n = sFrames.fetch_add(1, std::memory_order_relaxed) + 1;
    if (gConfig.frames != 0 && n >= gConfig.frames) {
        writef(STDERR_FILENO, "[cos] FRAMES %u done retrace=%u ms=%llu\n", n,
               (unsigned int)VIGetRetraceCount(), (unsigned long long)elapsedMs());
        writePacing(STDERR_FILENO);
        pc_exit(PC_EXIT_REACHED);
    }
}

unsigned int pc_frame_count(void) {
    return sFrames.load(std::memory_order_relaxed);
}

} // extern "C"
