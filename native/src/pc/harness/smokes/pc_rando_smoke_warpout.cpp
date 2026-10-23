// COS_SMOKE=rando COS_RANDO_SMOKE=warpout (docs/RANDOMIZER_PLAN.md step R6.13): the boss warp-out
// goes to the island the arena is reached from under entrance rando.
//   native/tools/run.sh rando --stage M_DragB:0:0 --env COS_RANDO_SMOKE=warpout \
//       --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_abc_full.json \
//       --env COS_WARPOUT_EXPECT=sea:45:229
// R0_6_abc_full.json randomizes dungeon entrances: Dragon Roost Cavern is behind Headstone Island, so
// Gohma's warp out lands at sea room 45 spawn 229 (wwrando: R6_13_warpout.txt's case logic; the
// vanilla event says sea room 13 spawn 211).
//
// 1. Once the play scene runs on a boss arena stage: the stage's WARP_WIND_AFTER event exists, its
//    DIRECTOR has the NEXT action, and pc_rando_warpout_dest for the stage gives COS_WARPOUT_EXPECT
//    (stage:room:spawn), or nothing when COS_WARPOUT_EXPECT is "vanilla" (no seed, COS_RANDO_FORCE,
//    entrances not randomized).
// 2. The harness orders WARP_WIND_AFTER (fopAcM_orderOtherEventId from the player) and ends any cut
//    that waits for an actor a debug boot does not run (kStuckFrames) except the DIRECTOR's NEXT.
//    When the next stage is requested it is the expected one: COS_WARPOUT_EXPECT, or the event's own
//    values for "vanilla"; pc_rando_warpout_redirects grew by one exactly when redirected.
// 3. The play scene then runs on that stage and room with the player spawned.
// On M2tower (Helmaroc King, --stage M2tower:0:16), instead of 1-2: room 0's live SCLS[1] holds
// COS_WARPOUT_EXPECT and the archive patcher wrote it once (vanilla: not written), then the harness
// takes that exit as the door does (dLib_setNextStageBySclsNum(1, 0)) and checks 3.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_warpout.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "d/d_lib.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "warpout";
constexpr unsigned int kStartFrames = 300;
constexpr unsigned int kEventFrames = 3000;
constexpr unsigned int kStuckFrames = 150;
constexpr unsigned int kArriveFrames = 1800;
constexpr unsigned int kSettleFrames = 30;
constexpr int kMaxStaff = 32;

enum State { kInit, kOrder, kRun, kArrive, kDone };
State sState = kInit;
unsigned int sSince = 0;
s16 sEventIdx = -1;
int sDirector = -1;
int sRedirectsBefore = 0;
bool sExpectVanilla = true;
bool sHelmaroc = false;
std::string sExpStage;
int sExpRoom = -1;
int sExpSpawn = -1;
unsigned int sSettled = 0;
int sStaffCut[kMaxStaff];
unsigned int sStaffSince[kMaxStaff];

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    writef(STDERR_FILENO, "[cos] rando-smoke warpout: %s\n", buf);
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

struct PlayLayerScope {
    layer_class* saved;
    PlayLayerScope() : saved(fpcLy_CurrentLayer()) {
        if (void* play = fpcM_Search(isPlayScene, nullptr)) {
            fpcLy_SetCurrentLayer(&static_cast<process_node_class*>(play)->mLayer);
        }
    }
    ~PlayLayerScope() { fpcLy_SetCurrentLayer(saved); }
};

[[noreturn]] void finish() {
    setDrivenPad(false, 0, 0, 0);
    sState = kDone;
    randoSmokeEnd(kName);
}

// The action named `name` in the staff's action chain of event `ev`, read through the cut list.
bool staffHasCut(dEvDtBase_c& list, int staffIdx, const char* name) {
    dEvDtStaff_c* staff = list.getStaffP(staffIdx);
    int cut = (int)staff->getStartCut();
    for (int guard = 0; cut >= 0 && cut < list.getCutNum() && guard < 64; guard++) {
        dEvDtCut_c* c = list.getCutP(cut);
        if (strcmp(c->getName(), name) == 0) {
            return true;
        }
        cut = c->getNext();
    }
    return false;
}

// The DIRECTOR NEXT action's string/int property `name` (the vanilla values), via the data list.
bool nextProps(dEvDtBase_c& list, int staffIdx, std::string* stage, int* room, int* spawn) {
    dEvDtStaff_c* staff = list.getStaffP(staffIdx);
    int cut = (int)staff->getStartCut();
    for (int guard = 0; cut >= 0 && cut < list.getCutNum() && guard < 64; guard++) {
        dEvDtCut_c* c = list.getCutP(cut);
        if (strcmp(c->getName(), "NEXT") == 0) {
            bool haveStage = false, haveSpawn = false;
            *room = 0;
            for (int d = (int)c->getDataTop(); d >= 0 && d < list.getDataNum();) {
                dEvDtData_c* data = list.getDataP(d);
                if (strcmp(data->getName(), "Stage") == 0 && data->getType() == dEvDtData_c::TYPE_STRING) {
                    *stage = list.getSDataP(data->getIndex());
                    haveStage = true;
                } else if (strcmp(data->getName(), "RoomNo") == 0 && data->getType() == dEvDtData_c::TYPE_INT) {
                    *room = *list.getIDataP(data->getIndex());
                } else if (strcmp(data->getName(), "StartCode") == 0 && data->getType() == dEvDtData_c::TYPE_INT) {
                    *spawn = *list.getIDataP(data->getIndex());
                    haveSpawn = true;
                }
                d = data->getNext();
            }
            return haveStage && haveSpawn;
        }
        cut = c->getNext();
    }
    return false;
}

bool parseExpect(const char* s) {
    if (s == nullptr || s[0] == '\0' || strcmp(s, "vanilla") == 0) {
        sExpectVanilla = true;
        return true;
    }
    char stage[16] = {};
    int room = -1, spawn = -1;
    if (sscanf(s, "%15[^:]:%d:%d", stage, &room, &spawn) != 3) {
        return false;
    }
    sExpectVanilla = false;
    sExpStage = stage;
    sExpRoom = room;
    sExpSpawn = spawn;
    return true;
}

// Helmaroc King (M2tower room 0): no warp-out event; SCLS[1] (the door to Ganondorf's room) is the
// exit wwrando rewrites. Checks the live SCLS[1] and the patcher's status, then takes that exit as the
// door does (dLib_setNextStageBySclsNum).
bool initHelmaroc() {
    sHelmaroc = true;
    stage_scls_info_dummy_class* info = dComIfGp_roomControl_getStatusRoomDt(0) != nullptr
                                            ? dComIfGp_roomControl_getStatusRoomDt(0)->getSclsInfo()
                                            : nullptr;
    if (!randoSmokeCheck(kName, "M2tower room 0 has SCLS[1]", info != nullptr && info->num > 1)) {
        return false;
    }
    stage_scls_info_class* entries = info->m_entries;
    const stage_scls_info_class& e = entries[1];
    char dest[9] = {};
    memcpy(dest, e.mStage, 8);
    line("M2tower room 0 SCLS[1]: %s room %d spawn %d", dest, e.mRoom, e.mStart);
    PcRandoWarpoutStatus st;
    pc_rando_warpout_status(&st);
    line("patcher: %d write(s), %d skip(s)", st.m2towerPatched, st.m2towerSkipped);
    PcRandoWarpOut w;
    const int got = pc_rando_warpout_dest(pc_rando_seed(), "M2tower", &w);
    if (sExpectVanilla) {
        randoSmokeCheck(kName, "no warp-out override and SCLS[1] not written (vanilla)",
                        got == 0 && st.m2towerPatched == 0);
        sExpStage = dest;
        sExpRoom = e.mRoom;
        sExpSpawn = e.mStart;
    } else {
        randoSmokeCheck(kName, "pc_rando_warpout_dest(M2tower) gives COS_WARPOUT_EXPECT",
                        got == 1 && sExpStage == w.stage && sExpRoom == w.room && sExpSpawn == w.spawn);
        randoSmokeCheck(kName, "SCLS[1] was written once and holds COS_WARPOUT_EXPECT",
                        st.m2towerPatched == 1 && sExpStage == dest && sExpRoom == e.mRoom && sExpSpawn == e.mStart);
        randoSmokeCheck(kName, "the expectation is not the R3.15 exit (sea room 1 spawn 0; else the run proves nothing)",
                        !(sExpStage == "sea" && sExpRoom == 1 && sExpSpawn == 0));
    }
    sRedirectsBefore = pc_rando_warpout_redirects();
    PlayLayerScope layer;
    dLib_setNextStageBySclsNum(1, 0);
    return true;
}

bool init() {
    const char* stage = dComIfGp_getStartStageName();
    line("stage %s room %d, pc_rando() %d, seed %s, entrances randomized %d", stage,
         dComIfGp_roomControl_getStayNo(), pc_rando(), pc_rando_seed() != nullptr ? "yes" : "no",
         pc_rando_warpout_enabled(pc_rando_seed()));
    if (!randoSmokeCheck(kName, "COS_WARPOUT_EXPECT parses (stage:room:spawn or vanilla)",
                         parseExpect(getenv("COS_WARPOUT_EXPECT")))) {
        return false;
    }
    if (strcmp(stage, "M2tower") == 0) {
        return initHelmaroc();
    }
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtBase_c& list = mgr->mList;
    sEventIdx = mgr->getEventIdx("WARP_WIND_AFTER", 0xFF);
    dEvDtEvent_c* ev = mgr->getEventData(sEventIdx);
    if (!randoSmokeCheck(kName, "the stage has WARP_WIND_AFTER", ev != nullptr)) {
        return false;
    }
    for (int i = 0; i < ev->getNStaff(); i++) {
        if (strcmp(list.getStaffP(ev->getStaff(i))->getName(), "DIRECTOR") == 0) {
            sDirector = ev->getStaff(i);
        }
    }
    if (!randoSmokeCheck(kName, "WARP_WIND_AFTER has a DIRECTOR with a NEXT action",
                         sDirector >= 0 && staffHasCut(list, sDirector, "NEXT"))) {
        return false;
    }
    std::string vStage;
    int vRoom = -1, vSpawn = -1;
    if (!randoSmokeCheck(kName, "the NEXT action has Stage, RoomNo and StartCode",
                         nextProps(list, sDirector, &vStage, &vRoom, &vSpawn))) {
        return false;
    }
    line("the event's own destination: %s room %d spawn %d", vStage.c_str(), vRoom, vSpawn);

    PcRandoWarpOut w;
    const int got = pc_rando_warpout_dest(pc_rando_seed(), stage, &w);
    if (got) {
        line("pc_rando_warpout_dest(%s): %s room %d spawn %d", stage, w.stage, w.room, w.spawn);
    } else {
        line("pc_rando_warpout_dest(%s): unchanged", stage);
    }
    if (sExpectVanilla) {
        randoSmokeCheck(kName, "no warp-out override (vanilla)", got == 0);
        sExpStage = vStage;
        sExpRoom = vRoom;
        sExpSpawn = vSpawn;
    } else {
        randoSmokeCheck(kName, "pc_rando_warpout_dest gives COS_WARPOUT_EXPECT",
                        got == 1 && sExpStage == w.stage && sExpRoom == w.room && sExpSpawn == w.spawn);
        randoSmokeCheck(kName, "the expectation differs from the event's own destination (else the run proves nothing)",
                        !(sExpStage == vStage && sExpRoom == vRoom && sExpSpawn == vSpawn));
    }
    sRedirectsBefore = pc_rando_warpout_redirects();
    for (int i = 0; i < kMaxStaff; i++) {
        sStaffCut[i] = -1;
        sStaffSince[i] = 0;
    }
    return true;
}

void order() {
    PlayLayerScope layer;
    fopAcM_orderOtherEventId(dComIfGp_getPlayer(0), sEventIdx, 0xFF, 0xFFFF, 1);
}

// A debug boot has no warp actor and no Gohma corpse: a staff stuck in one cut gets it ended, as its
// actor would. The DIRECTOR runs its own cuts.
void driveStuckCuts() {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtEvent_c* ev = mgr->getEventData(sEventIdx);
    if (ev == nullptr) {
        return;
    }
    dEvDtBase_c& list = mgr->mList;
    for (int i = 0; i < ev->getNStaff() && i < kMaxStaff; i++) {
        const int staffIdx = ev->getStaff(i);
        if (staffIdx == sDirector) {
            continue;
        }
        dEvDtStaff_c* staff = list.getStaffP(staffIdx);
        const int cut = (int)staff->getCurrentCut();
        if (cut != sStaffCut[i]) {
            sStaffCut[i] = cut;
            sStaffSince[i] = 0;
            continue;
        }
        if (++sStaffSince[i] < kStuckFrames) {
            continue;
        }
        sStaffSince[i] = 0;
        line("staff %s waits in cut %d '%.32s'; the harness ends it", staff->getName(), cut,
             list.getCutP(cut)->getName());
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

void checkNextStage() {
    const char* name = dComIfGp_getNextStageName();
    const int room = dComIfGp_getNextStageRoomNo();
    const int point = dComIfGp_getNextStagePoint();
    line("next stage requested after %u frame(s): %s room %d point %d", sSince, name, room, point);
    char what[160];
    snprintf(what, sizeof what, "the warp out goes to %s room %d spawn %d", sExpStage.c_str(), sExpRoom, sExpSpawn);
    randoSmokeCheck(kName, what, sExpStage == name && room == sExpRoom && point == sExpSpawn);
    const int redirects = pc_rando_warpout_redirects() - sRedirectsBefore;
    if (sHelmaroc) {
        randoSmokeCheck(kName, "the event hook did not run (0 redirects)", redirects == 0);
    } else {
        randoSmokeCheck(kName, sExpectVanilla ? "the hook left it (0 redirects)" : "the hook redirected it (1 redirect)",
                        redirects == (sExpectVanilla ? 0 : 1));
    }
}

} // namespace

void randoSmokeWarpout(unsigned int frames) {
    (void)frames;
    if (sState == kDone) {
        return;
    }
    sSince++;
    switch (sState) {
    case kInit:
        if (!randoSmokeInPlay(10) || dComIfGp_getPlayer(0) == nullptr) {
            return;
        }
        if (!init()) {
            finish();
        }
        sSince = 0;
        if (sHelmaroc) {
            sState = kRun;
            return;
        }
        sState = kOrder;
        order();
        return;
    case kOrder: {
        dEvt_control_c* ctl = g_dComIfG_gameInfo.play.getEvent();
        if (dComIfGp_event_runCheck() && ctl->mEventId == sEventIdx) {
            line("WARP_WIND_AFTER started after %u frame(s)", sSince);
            sState = kRun;
            sSince = 0;
            return;
        }
        if (sSince >= kStartFrames) {
            randoSmokeCheck(kName, "WARP_WIND_AFTER started", false);
            finish();
        }
        if (!dComIfGp_event_runCheck()) {
            order();
        }
        return;
    }
    case kRun:
        if (dComIfGp_isEnableNextStage()) {
            checkNextStage();
            sState = kArrive;
            sSince = 0;
            return;
        }
        if (!sHelmaroc) {
            driveStuckCuts();
        }
        if (sSince >= kEventFrames) {
            randoSmokeCheck(kName, "WARP_WIND_AFTER requested the next stage", false);
            finish();
        }
        return;
    case kArrive: {
        const char* stage = dComIfGp_getStartStageName();
        const bool there = sExpStage == stage && dComIfGp_roomControl_getStayNo() == sExpRoom &&
                           dComIfGp_getPlayer(0) != nullptr && !dComIfGp_isEnableNextStage();
        sSettled = there ? sSettled + 1 : 0;
        if (sSettled >= kSettleFrames) {
            line("arrived: %s room %d after %u frame(s)", stage, dComIfGp_roomControl_getStayNo(), sSince);
            randoSmokeCheck(kName, "the play scene runs on the destination stage and room", true);
            finish();
        }
        if (sSince >= kArriveFrames) {
            line("still on %s room %d", stage, dComIfGp_roomControl_getStayNo());
            randoSmokeCheck(kName, "the play scene runs on the destination stage and room", false);
            finish();
        }
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc
