// COS_SMOKE=rando (docs/RANDOMIZER_PLAN.md step R0.1): the one in-game smoke of the randomizer,
// which dispatches on COS_RANDO_SMOKE=<name> to the functions pc_rando_smokes.def lists (see
// pc_rando_smokes.h). Without COS_RANDO_SMOKE, or with an unknown name, it lists the names and
// exits with PC_EXIT_USAGE.
#include "pc_rando_smokes.h"

#include "d/d_com_inf_game.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

struct Entry {
    const char* name;
    void (*fn)(unsigned int);
};

const Entry kEntries[] = {
#define PC_RANDO_SMOKE(name, fn) {#name, fn},
#include "pc_rando_smokes.def"
#undef PC_RANDO_SMOKE
};

enum State { kOff, kRun, kDone };
State sState = kOff;
void (*sFn)(unsigned int) = nullptr;
unsigned int sPlayFrames = 0;
int sFailures = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void usage(const char* name) {
    writef(STDERR_FILENO, "[cos] rando-smoke: %s COS_RANDO_SMOKE%s%s; known:", name == nullptr ? "no" : "unknown",
           name == nullptr ? "" : "=", name == nullptr ? "" : name);
    for (const Entry& e : kEntries) {
        writef(STDERR_FILENO, " %s", e.name);
    }
    writef(STDERR_FILENO, "\n");
    pc_exit(PC_EXIT_USAGE);
}

} // namespace

bool randoSmokeInPlay(unsigned int settle) {
    return sPlayFrames > settle;
}

bool randoSmokeCheck(const char* smoke, const char* what, bool ok) {
    writef(STDERR_FILENO, "[cos] rando-smoke %s: %s: %s\n", smoke, what, ok ? "ok" : "FAIL");
    if (!ok) {
        sFailures++;
    }
    return ok;
}

void randoSmokeEnd(const char* smoke) {
    sState = kDone;
    writef(STDERR_FILENO, "[cos] rando-smoke %s: %s (%d failed)\n", smoke, sFailures == 0 ? "PASS" : "FAIL",
           sFailures);
    pc_exit(sFailures == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void randoSmokeFrame(unsigned int frames) {
    if (sState == kOff) {
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "rando") != 0) {
            sState = kDone;
            return;
        }
        const char* name = getenv("COS_RANDO_SMOKE");
        if (name == nullptr || name[0] == '\0') {
            usage(nullptr);
        }
        for (const Entry& e : kEntries) {
            if (strcmp(e.name, name) == 0) {
                sFn = e.fn;
            }
        }
        if (sFn == nullptr) {
            usage(name);
        }
        writef(STDERR_FILENO, "[cos] rando-smoke: running %s\n", name);
        sState = kRun;
    }
    if (sState != kRun) {
        return;
    }
    if (dComIfGp_getPlayer(0) != nullptr && fpcM_Search(isPlayScene, nullptr) != nullptr) {
        sPlayFrames++;
    } else {
        sPlayFrames = 0;
    }
    sFn(frames);
}

} // namespace pc
