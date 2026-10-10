// COS_TOOL_WATCH=1 (with COS_RUN_DIR): a log of what the player's items do, for
// native/tools/tool_sweep.py. Not a smoke: it runs beside any target (usually `run` with a COS_INPUT
// script and COS_BOOT_ITEMS), from pc_frame_end every game frame, and only reads game state.
// The bow's arrows left the bow aimed at the ground for months (centollOS/SwitchWaker#1) while
// every item run passed: those runs checked that the game did not crash, not what the item did.
//
// Once the player is in the PLAY scene, kSettleFrames later the actors alive then are the
// baseline; every actor created afterwards (an arrow, a boomerang, a bomb, an explosion, bait,
// a fairy...) is tracked until it is gone. <COS_RUN_DIR>/tool_watch.txt, one record per line:
//   P <frame> proc=<daPyProc> equip=<item> pos=<x>,<y>,<z> speedF=<f> life=<quarters>
//     magic=<n> arrows=<n> bombs=<n>       every kPlayerEvery frames and when proc or equip changes
//   + <frame> <id> <name> <proc> pos=<x>,<y>,<z> pdist=<distance to the player>
//   A <frame> <id> <name> pos=<x>,<y>,<z> pdist=<f>                     every kActorEvery frames
//   - <frame> <id> <name> life=<frames> pos=<last x,y,z> travel=<max distance from its birth>
//     pmax=<max distance to the player> pend=<last distance to the player>
// COS_TOOL_PLACE=<x>,<y>,<z>,<angle y in degrees>: at the baseline the player is put there, facing
// that way (the one write to game state), so a case can stand in front of a target in a test room
// (a hookshot target in K_Teste, a grappling post in K_Testa) instead of walking there.
// The names are the dStage names of the process profiles (pc_actor_sweep.cpp's stageName), or
// "-"; <proc> is the process name number.
#include "pc_internal.h"

#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int daPy_lk_c::debugCurProc() const {
    return mCurProc;
}

int daPy_lk_c::debugEquipItem() const {
    return mEquipItem;
}

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 30;
constexpr unsigned int kPlayerEvery = 10;
constexpr unsigned int kActorEvery = 5;
constexpr int kMaxBase = 1024;
constexpr int kMaxTracked = 256;

struct Tracked {
    fpc_ProcID id;
    int proc;
    unsigned int born;
    cXyz birth, last;
    float travel, pmax, pend;
    bool seen;
};

int sFd = -2; // -2: not decided yet, -1: off
unsigned int sReady = 0;  // frames with the player in the PLAY scene
bool sBaselineDone = false;
bool sPlaceDone = false;
fpc_ProcID sBase[kMaxBase];
int sNumBase = 0;
Tracked sTracked[kMaxTracked];
int sNumTracked = 0;
int sLastProc = -1, sLastEquip = -1;
cXyz sPlayerPos;
unsigned int sFrame = 0;

const char* stageName(int proc) {
    const char* name = dStage_getName((s16)proc, -1);
    if (name == nullptr || (unsigned char)name[0] >= 0x80) {
        name = dStage_getName((s16)proc, 0);
    }
    return name != nullptr && (unsigned char)name[0] < 0x80 ? name : "-";
}

bool inBase(fpc_ProcID id) {
    for (int i = 0; i < sNumBase; i++) {
        if (sBase[i] == id) {
            return true;
        }
    }
    return false;
}

void* collectBase(void* proc, void*) {
    if (fopAc_IsActor(proc) && sNumBase < kMaxBase) {
        sBase[sNumBase++] = fopAcM_GetID(proc);
    }
    return nullptr;
}

void* visit(void* proc, void*) {
    if (!fopAc_IsActor(proc)) {
        return nullptr;
    }
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(proc);
    const fpc_ProcID id = fopAcM_GetID(actor);
    if (fopAcM_GetName(actor) == fpcNm_PLAYER_e || inBase(id)) {
        return nullptr;
    }
    const cXyz pos = actor->current.pos;
    const float pdist = (pos - sPlayerPos).abs();
    for (int i = 0; i < sNumTracked; i++) {
        Tracked& t = sTracked[i];
        if (t.id != id) {
            continue;
        }
        t.seen = true;
        t.last = pos;
        t.pend = pdist;
        t.travel = std::max(t.travel, (pos - t.birth).abs());
        t.pmax = std::max(t.pmax, pdist);
        if ((sFrame - t.born) % kActorEvery == 0) {
            writef(sFd, "A %u %u %s pos=%.1f,%.1f,%.1f pdist=%.1f\n", sFrame, (unsigned)id,
                   stageName(t.proc), pos.x, pos.y, pos.z, pdist);
        }
        return nullptr;
    }
    if (sNumTracked == kMaxTracked) {
        return nullptr;
    }
    Tracked& t = sTracked[sNumTracked++];
    t.id = id;
    t.proc = fopAcM_GetName(actor);
    t.born = sFrame;
    t.birth = t.last = pos;
    t.travel = 0.0f;
    t.pmax = t.pend = pdist;
    t.seen = true;
    writef(sFd, "+ %u %u %s %d pos=%.1f,%.1f,%.1f pdist=%.1f\n", sFrame, (unsigned)id,
           stageName(t.proc), t.proc, pos.x, pos.y, pos.z, pdist);
    return nullptr;
}

} // namespace

void toolWatchFrame(unsigned int frames) {
    if (sFd == -2) {
        const char* on = getenv("COS_TOOL_WATCH");
        sFd = on != nullptr && strcmp(on, "0") != 0 ? openRunFile("tool_watch.txt") : -1;
    }
    if (sFd < 0) {
        return;
    }
    sFrame = frames;
    daPy_lk_c* player = daPy_getPlayerLinkActorClass();
    if (player == nullptr || fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        sReady = 0;
        return;
    }
    sPlayerPos = player->current.pos;
    sReady++;
    if (!sBaselineDone) {
        if (sReady < kSettleFrames) {
            return;
        }
        fpcM_Search(collectBase, nullptr);
        sBaselineDone = true;
        writef(sFd, "# baseline %u: %d actors\n", frames, sNumBase);
    }

    // COS_TOOL_PLACE, once no event runs (the stage's start event places the player itself).
    if (!sPlaceDone && !dComIfGp_event_runCheck()) {
        sPlaceDone = true;
        float x, y, z, deg;
        const char* place = getenv("COS_TOOL_PLACE");
        if (place != nullptr && sscanf(place, "%f,%f,%f,%f", &x, &y, &z, &deg) == 4) {
            const s16 yaw = (s16)(int)(deg * 65536.0f / 360.0f);
            player->debugPlace(cXyz(x, y, z), yaw);
            sPlayerPos = player->current.pos;
            writef(sFd, "# %u placed at %.1f,%.1f,%.1f angle y 0x%04x\n", frames, x, y, z, (unsigned)(u16)yaw);
        }
    }

    const int proc = player->debugCurProc();
    const int equip = player->debugEquipItem();
    if (proc != sLastProc || equip != sLastEquip || frames % kPlayerEvery == 0) {
        writef(sFd, "P %u proc=%d equip=%d pos=%.1f,%.1f,%.1f speedF=%.1f life=%u magic=%u arrows=%u bombs=%u\n",
               frames, proc, equip, sPlayerPos.x, sPlayerPos.y, sPlayerPos.z, player->speedF,
               (unsigned)dComIfGs_getLife(), (unsigned)dComIfGs_getMagic(),
               (unsigned)dComIfGs_getArrowNum(), (unsigned)dComIfGs_getBombNum());
        sLastProc = proc;
        sLastEquip = equip;
    }

    for (int i = 0; i < sNumTracked; i++) {
        sTracked[i].seen = false;
    }
    fpcM_Search(visit, nullptr);
    for (int i = 0; i < sNumTracked;) {
        Tracked& t = sTracked[i];
        if (t.seen) {
            i++;
            continue;
        }
        writef(sFd, "- %u %u %s life=%u pos=%.1f,%.1f,%.1f travel=%.1f pmax=%.1f pend=%.1f\n", frames,
               (unsigned)t.id, stageName(t.proc), frames - t.born, t.last.x, t.last.y, t.last.z,
               t.travel, t.pmax, t.pend);
        sTracked[i] = sTracked[--sNumTracked];
    }
}

} // namespace pc
