// R6.13: boss warp-out under entrance rando (docs/RANDOMIZER_PLAN.md; pc_rando_hooks.h). The game
// side of features/rando/pc_rando_warpout.h: dEvDt_Next_Stage (game/src/d/d_event_data.cpp) asks
// pc_rando_warpout_next_stage under pc_rando() before it calls dComIfGp_setNextStage, and the warp out
// of a boss arena goes to the island the arena is reached from, as wwrando's
// update_boss_warp_out_destination writes it into the WARP_WIND_AFTER event's DIRECTOR NEXT action.
#include "d/dolzel.h" // IWYU pragma: keep

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_warpout.h"

#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

// The stages whose WARP_WIND_AFTER event wwrando rewrites (BOSS_EXITS without Helmaroc King).
const char* const kWarpStages[] = {"M_DragB", "kinBOSS", "SirenB", "M_DaiB", "kazeB"};

int sRedirects = 0;

bool isWarpStage(const char* stage) {
    for (const char* s : kWarpStages) {
        if (strcmp(stage, s) == 0) {
            return true;
        }
    }
    return false;
}

// True when staffIdx is the DIRECTOR of the event named WARP_WIND_AFTER in the loaded event list
// (the event dEvDt_Next_Stage's NEXT action belongs to).
bool isWarpOutDirector(int staffIdx) {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtBase_c& list = mgr->mList;
    if (list.getHeaderP() == NULL || staffIdx < 0 || staffIdx >= list.getStaffNum()) {
        return false;
    }
    if (strcmp(list.getStaffP(staffIdx)->getName(), "DIRECTOR") != 0) {
        return false;
    }
    for (s32 i = 0; i < list.getEventNum(); i++) {
        dEvDtEvent_c* ev = list.getEventP(i);
        if (strcmp(ev->getName(), "WARP_WIND_AFTER") != 0) {
            continue;
        }
        for (s32 j = 0; j < ev->getNStaff(); j++) {
            if (ev->getStaff(j) == staffIdx) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

extern "C" int pc_rando_warpout_next_stage(int staffIdx, const char** stage, int* startCode, int* roomNo) {
    if (!pc_rando() || stage == NULL || startCode == NULL || roomNo == NULL) {
        return 0;
    }
    const char* cur = dComIfGp_getStartStageName();
    if (cur == NULL || !isWarpStage(cur) || !isWarpOutDirector(staffIdx)) {
        return 0;
    }
    PcRandoWarpOut w;
    if (!pc_rando_warpout_dest(pc_rando_seed(), cur, &w)) {
        return 0;
    }
    char buf[160];
    const int n = snprintf(buf, sizeof buf,
                           "[cos] rando: warpout: %s WARP_WIND_AFTER: %s room %d spawn %d -> %s room %d spawn %d\n",
                           cur, *stage != NULL ? *stage : "(null)", *roomNo, *startCode, w.stage, w.room, w.spawn);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
    *stage = w.stage;
    *startCode = w.spawn;
    *roomNo = w.room;
    sRedirects++;
    return 1;
}

extern "C" int pc_rando_warpout_redirects(void) {
    return sRedirects;
}
