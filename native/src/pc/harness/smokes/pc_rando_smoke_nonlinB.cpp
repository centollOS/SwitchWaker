// COS_SMOKE=rando COS_RANDO_SMOKE=nonlinB (docs/RANDOMIZER_PLAN.md step R3.10): the hunks of
// wwrando's make_game_nonlinear.asm items 12-22 (docs/RANDO_HOOKS.md "## R3.10"), on Outset:
//   native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=nonlinB
//
// Each check runs the hunk's code with the randomizer off and on (pc_rando_set_active /
// pc_rando_deactivate) and asserts both results: the original game's, and wwrando's. With
// COS_RANDO_FORCE=1 the randomizer cannot be turned off, so only the rando half runs (and the actors
// that only the original game creates, Akabe without a sword and young Komali, are skipped).
//
// - Direct calls, in the first frame: dComIfG_play_c::getLayerNo for Hyrule (with the start stage name
//   switched to "Hyrule" for the call), dMenu_Fmap_c::checkMarkCheck1/2 (on a menu object holding only
//   its marker panes) and dDoor_hkyo_c::proc for the west and north doors (on a door helper whose
//   brk animation runs on a local frame control).
// - Actors spawned next to the player, one at a time, then deleted (as the actor sweep does):
//   * hunks in an actor's create: created or refused under the expected switch (Obj_Ajav, Obj_Auzu,
//     Obj_Eayogn, NPC_BMS1 and its low price look), and MO2's lock-on flag (a Moblin with a lantern and no sword,
//     rouya_mode from daMo2_Create);
//   * hunks in member functions: the actor is created with the randomizer off, then the function is
//     called with it off and on (Akabe chk_appear, Auzu is_exist (created with it on), Co1 init_CO1_0, De1 getMsg,
//     Bm1 init_BMD_1, Os eventOrderCheck and finish02NpcAction, Hsh actionDeleteEvent), or the actor
//     runs a few frames each way (Os execute: out of the player's room it gives up its home room).
// The save bits a check needs (sword, pearls, Endless Night, ...) are set for the check and the
// player's event and collection records are restored after each one.
//
// Not asserted here: daMo2_Execute's rouya_mode and d_a_kb's two hunks. They only feed file-static
// state and static functions of their units (the jail capture and a pig's anger when hit or carried
// without a sword), which no smoke can reach without a fight; RANDO_HOOKS.md and the hook census
// cover them.
//
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"

#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "d/actor/d_a_mo2.h"
#include "d/actor/d_a_npc_bm1.h"
#include "d/actor/d_a_npc_bms1.h"
#include "d/actor/d_a_npc_co1.h"
#include "d/actor/d_a_npc_de1.h"
#include "d/actor/d_a_npc_os.h"
#include "d/actor/d_a_obj_akabe.h"
#include "d/actor/d_a_obj_auzu.h"
#include "d/actor/d_a_obj_hsehi1.h"
#include "d/d_com_inf_game.h"
#include "d/d_door.h"
#include "d/d_menu_fmap.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "f_pc/f_pc_create_iter.h"
#include "f_pc/f_pc_create_req.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_math.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "nonlinB";

// Private members the checks read or set, through explicit instantiation (which access checking
// does not apply to): get(Tag()) is the member pointer.
template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
#define NONLINB_ROB(TAG, CLASS, TYPE, MEMBER)                                                         \
    struct TAG {                                                                                     \
        using type = TYPE CLASS::*;                                                                  \
        friend type get(TAG);                                                                        \
    };                                                                                               \
    template struct Rob<TAG, &CLASS::MEMBER>;

using PaneRow3 = fopMsgM_pane_class[3];
using PaneRow2 = fopMsgM_pane_class[2];
NONLINB_ROB(OsOrder, daNpc_Os_c, s8, field_0x7A5)
NONLINB_ROB(OsSwitch, daNpc_Os_c, u32, field_0x794)
NONLINB_ROB(OsStep, daNpc_Os_c, s8, field_0x7A9)
NONLINB_ROB(OsBrk, daNpc_Os_c, s8, field_0x7A2)
NONLINB_ROB(FmapCk1, dMenu_Fmap_c, PaneRow3, mCk1xPanes)
NONLINB_ROB(FmapCk3, dMenu_Fmap_c, PaneRow2, mCk3xPanes)
NONLINB_ROB(AnmCtrl, mDoExt_baseAnm, J3DFrameCtrl*, mFrameCtrl)
#undef NONLINB_ROB

bool sForced = false;

// Switches the randomizer for the next call (a no-op under COS_RANDO_FORCE, where it stays on).
void rando(bool on) {
    if (sForced) {
        return;
    }
    if (on) {
        pc_rando_set_active(nullptr, nullptr);
    } else {
        pc_rando_deactivate();
    }
}

// One assert per hunk: `vanilla` is the original game's outcome (skipped when forced), `wwr` the
// randomizer's.
void check(const char* what, bool vanilla, bool wwr) {
    char buf[160];
    if (!sForced) {
        snprintf(buf, sizeof buf, "%s: original game", what);
        randoSmokeCheck(kName, buf, vanilla);
    }
    snprintf(buf, sizeof buf, "%s: randomizer", what);
    randoSmokeCheck(kName, buf, wwr);
}

// The player's records the checks change, restored after each one.
dSv_event_c sEvents;
dSv_player_collect_c sCollect;

dSv_event_c& events() {
    return g_dComIfG_gameInfo.save.getEvent();
}
dSv_player_collect_c& collect() {
    return g_dComIfG_gameInfo.save.getPlayer().getCollect();
}
void saveRecords() {
    sEvents = events();
    sCollect = collect();
}
void restoreRecords() {
    events() = sEvents;
    collect() = sCollect;
}
void noSword() {
    for (u8 i = 0; i < 4; i++) {
        dComIfGs_offCollect(0, i);
    }
}
void offSymbol(u8 no) {
    collect().offSymbol(no);
}

// ---- Direct calls

void checkLayer() {
    // dComIfG_play_c::getLayerNo, Hyrule: with 8 Triforce shards the original picks layer|4 (no
    // enemies); the randomizer keeps the day/night layer (0 or 1).
    dStage_startStage_c* start = dComIfGp_getStartStage();
    const dStage_startStage_c saved = *start;
    start->set("Hyrule", saved.getPoint(), saved.getRoomNo(), -1);
    collect().mTriforce = 0xFF;
    rando(false);
    const int v = dComIfG_play_c::getLayerNo(0);
    rando(true);
    const int r = dComIfG_play_c::getLayerNo(0);
    rando(false);
    *start = saved;
    check("getLayerNo Hyrule with 8 shards (layer|4 / day-night layer)", (v & 4) != 0, r == 0 || r == 1);
    restoreRecords();
}

void checkFmap() {
    // dMenu_Fmap_c::checkMarkCheck1/2 only show and hide the marker panes; the object holds just
    // those (not constructed: the two functions read nothing else).
    alignas(dMenu_Fmap_c) static unsigned char storage[sizeof(dMenu_Fmap_c)];
    dMenu_Fmap_c* fmap = reinterpret_cast<dMenu_Fmap_c*>(storage);
    J2DPane panes[5];
    fopMsgM_pane_class* ck1 = fmap->*get(FmapCk1());
    fopMsgM_pane_class* ck3 = fmap->*get(FmapCk3());
    for (int i = 0; i < 3; i++) {
        ck1[i].pane = &panes[i];
    }
    ck3[0].pane = &panes[3];
    ck3[1].pane = &panes[4];
    auto hideAll = [&] {
        for (J2DPane& p : panes) {
            p.hide();
        }
    };
    auto anyShown = [&](int from, int to) {
        for (int i = from; i < to; i++) {
            if (panes[i].isVisible()) {
                return true;
            }
        }
        return false;
    };
    // Early-game marker: met the King of Red Lions, no pearl placed, none owned.
    dComIfGs_onEventBit(dSv_event_flag_c::MET_KORL);
    dComIfGs_offEventBit(dSv_event_flag_c::PLACED_DINS_PEARL);
    hideAll();
    rando(false);
    fmap->checkMarkCheck1();
    const bool v1 = anyShown(0, 3);
    hideAll();
    rando(true);
    fmap->checkMarkCheck1();
    const bool r1 = anyShown(0, 3);
    check("checkMarkCheck1 (early markers shown / returns at once)", v1, !r1);
    // Late-game markers: event 0x1820.
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_1820);
    hideAll();
    rando(false);
    fmap->checkMarkCheck2();
    const bool v2 = anyShown(3, 5);
    hideAll();
    rando(true);
    fmap->checkMarkCheck2();
    const bool r2 = anyShown(3, 5);
    rando(false);
    check("checkMarkCheck2 (late markers shown / returns at once)", v2, !r2);
    restoreRecords();
}

void checkDoor() {
    // dDoor_hkyo_c::proc, TotG hub doors: m11 3 (west) and 2 (north). The helper starts on glow index
    // 3, so setAnm(3) returns before it would touch the model (never dereferenced) and only the brk
    // animation's frame control (a local one here) runs; setAnm(0) just records 0.
    J3DFrameCtrl ctrl;
    mDoExt_brkAnm brk;
    brk.*get(AnmCtrl()) = &ctrl;
    dDoor_hkyo_c door;
    door.mpModel = reinterpret_cast<J3DModel*>(&ctrl);
    door.mpBrkAnm = &brk;
    dDoor_info_c* info = static_cast<dDoor_info_c*>(dComIfGp_getPlayer(0)); // for the blink sound
    auto run = [&](u8 kind, bool on) {
        door.m11 = kind;
        door.mAnmIdx = 3;
        rando(on);
        door.proc(info);
        return door.mAnmIdx;
    };
    // West door: no Command Melody, west servant (0x1704) not back.
    collect().offTact(mDoAud_MELODY_COMMAND_MELODY_e);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1704);
    const u8 v3 = run(3, false);
    const u8 r3 = run(3, true);
    check("hkyo west door without Command Melody (dark / glows)", v3 == 0, r3 == 3);
    // North door: west servant not back, north servant (0x1B01) not back.
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1B01);
    const u8 v2 = run(2, false);
    const u8 r2 = run(2, true);
    rando(false);
    check("hkyo north door before the west servant (dark / glows)", v2 == 0, r2 == 3);
    brk.*get(AnmCtrl()) = nullptr;
    restoreRecords();
}

// ---- Spawned actors

enum Expect { kCreated, kRefused };

struct Task {
    const char* what;
    s16 proc;
    u32 params;
    s8 argument;
    bool far;                                 // placed far from the player (see kFarDistance)
    bool randoOn;                             // the randomizer while it is created
    bool needsVanilla;                        // skipped under COS_RANDO_FORCE
    void (*setup)();                          // save bits before the creation
    Expect expect;
    bool (*probe)(fopAc_ac_c*, unsigned int); // each frame once it runs; true when done
};

bool probeMo2Vanilla(fopAc_ac_c* a, unsigned int) {
    randoSmokeCheck(kName, "MO2 with a lantern, no sword: original game, no battle lock-on (jail)",
                    (a->attention_info.flags & fopAc_Attn_LOCKON_BATTLE_e) == 0);
    return true;
}
bool probeMo2Rando(fopAc_ac_c* a, unsigned int) {
    randoSmokeCheck(kName, "MO2 with a lantern, no sword: randomizer, battle lock-on (no jail)",
                    (a->attention_info.flags & fopAc_Attn_LOCKON_BATTLE_e) != 0);
    return true;
}

bool probeAkabe(fopAc_ac_c* a, unsigned int) {
    auto* wall = static_cast<daObjAkabe::Act_c*>(a);
    rando(false);
    const bool v = wall->chk_appear();
    rando(true);
    const bool r = wall->chk_appear();
    rando(false);
    check("Akabe chk_appear, sword-only wall, no sword (appears / never)", v, !r);
    return true;
}

bool probeAuzu(fopAc_ac_c* a, unsigned int) {
    auto* pool = static_cast<daObjAuzu::Act_c*>(a);
    rando(false);
    const bool v = pool->is_exist();
    rando(true);
    const bool r = pool->is_exist();
    rando(false);
    check("Auzu is_exist without Endless Night (no whirlpool / whirlpool)", pool->mType == 0 && !v, r);
    return true;
}

bool probeCo1(fopAc_ac_c* a, unsigned int) {
    auto* komali = static_cast<daNpc_Co1_c*>(a);
    rando(false);
    const bool v = komali->init_CO1_0();
    rando(true);
    const bool r = komali->init_CO1_0();
    rando(false);
    check("Co1 init_CO1_0 without Din's Pearl (stays / leaves)", v, !r);
    return true;
}

bool probeDe1(fopAc_ac_c* a, unsigned int) {
    auto* tree = static_cast<daNpc_De1_c*>(a);
    tree->mMet = 0;
    rando(false);
    const u32 v = tree->getMsg();
    tree->mMet = 0;
    rando(true);
    const u32 r1 = tree->getMsg();
    const u32 r2 = tree->getMsg();
    rando(false);
    check("De1 getMsg (story line / Island Koroks 0x13A8 then 0x13AC)", v != 0x13A8 && v != 0x13AC,
          r1 == 0x13A8 && r2 == 0x13AC);
    return true;
}

bool probeBm1(fopAc_ac_c* a, unsigned int) {
    auto* kogoli = static_cast<daNpc_Bm1_c*>(a);
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_1620);
    rando(false);
    const bool v = kogoli->init_BMD_1();
    rando(true);
    const bool r = kogoli->init_BMD_1();
    rando(false);
    check("Bm1 init_BMD_1 with Medli awake 0x1620 (Kogoli gone / stays)", !v, r);
    return true;
}

bool probeOs(fopAc_ac_c* a, unsigned int n) {
    auto* os = static_cast<daNpc_Os_c*>(a);
    constexpr s8 kOtherRoom = 43;
    const s8 stay = (s8)dComIfGp_roomControl_getStayNo();
    if (n == 0) {
        // eventOrderCheck, east servant (argument 0) in the hub room 7 with its switch set.
        const s8 room = fopAcM_GetRoomNo(os);
        const u32 sw = os->*get(OsSwitch());
        os->*get(OsSwitch()) = 0x7E;
        dComIfGs_onSwitch(0x7E, 7);
        dComIfGs_offEventBit(dSv_event_flag_c::UNK_2510);
        fopAcM_SetRoomNo(os, 7);
        os->*get(OsOrder()) = -1;
        rando(false);
        os->eventOrderCheck();
        const s8 v = os->*get(OsOrder());
        os->*get(OsOrder()) = -1;
        rando(true);
        os->eventOrderCheck();
        const s8 r = os->*get(OsOrder());
        rando(false);
        os->*get(OsOrder()) = -1;
        fopAcM_SetRoomNo(os, room);
        os->*get(OsSwitch()) = sw;
        dComIfGs_offSwitch(0x7E, 7);
        check("Os eventOrderCheck, east servant in room 7 (event 7 / none)", v == 7, r == -1);

        // finish02NpcAction, first step, west servant (0x1B01) not back: beam off (brk 1) / on (brk 7).
        dComIfGs_offEventBit(dSv_event_flag_c::UNK_1B01);
        os->*get(OsStep()) = 0;
        rando(false);
        os->finish02NpcAction(nullptr);
        const s8 vb = os->*get(OsBrk());
        os->*get(OsStep()) = 0;
        rando(true);
        os->finish02NpcAction(nullptr);
        const s8 rb = os->*get(OsBrk());
        rando(false);
        check("Os finish02NpcAction without 0x1B01 (beam off / on)", vb == 1, rb == 7);
        restoreRecords();

        // execute: placed in another room than the player's, it keeps that home room (original)
        // or drops it and takes the room of the ground under it again (randomizer).
        rando(false);
        fopAcM_SetHomeRoomNo(os, kOtherRoom);
        fopAcM_SetRoomNo(os, kOtherRoom);
        return false;
    }
    if (n == 3) {
        const bool v = fopAcM_GetHomeRoomNo(os) == kOtherRoom;
        if (!sForced) {
            randoSmokeCheck(kName, "Os execute out of the player's room: original game, keeps its home room", v);
        }
        rando(true);
        fopAcM_SetHomeRoomNo(os, kOtherRoom);
        fopAcM_SetRoomNo(os, kOtherRoom);
        return false;
    }
    if (n == 6) {
        const s8 home = fopAcM_GetHomeRoomNo(os);
        rando(false);
        randoSmokeCheck(kName, "Os execute out of the player's room: randomizer, home room reset to the ground's",
                        home == stay);
        return true;
    }
    return false;
}

bool probeHsh(fopAc_ac_c* a, unsigned int) {
    auto* tablet = static_cast<daObj_hsh_c*>(a);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_2510);
    tablet->mAppearDeleteTimer = 0;
    rando(false);
    tablet->actionDeleteEvent(0);
    const bool v = dComIfGs_isEventBit(dSv_event_flag_c::UNK_2510);
    tablet->mAppearDeleteTimer = 0;
    rando(true);
    tablet->actionDeleteEvent(0);
    const bool r = dComIfGs_isEventBit(dSv_event_flag_c::UNK_2510);
    rando(false);
    check("Hsh actionDeleteEvent (0x2510 untouched / set)", !v, r);
    return true;
}

bool probeBms1(fopAc_ac_c* a, unsigned int) {
    auto* shop = static_cast<daNpc_Bms1_c*>(a);
    randoSmokeCheck(kName, "BMS1 without Nayru's Pearl: randomizer, low-price look (hair 0.4)",
                    shop->mHairScaleL.x == 0.4f);
    return true;
}

void setupMo2() {
    noSword();
}
void setupAkabe() {
    noSword();
}
void setupNoEndlessNight() {
    dComIfGs_offEventBit(dSv_event_flag_c::ENDLESS_NIGHT);
}
void setupDin() {
    collect().onSymbol(dSymbol_DIN_e);
}
void setupNoDin() {
    offSymbol(dSymbol_DIN_e);
}
void setupBms1() {
    // Endless Night set, Nayru's Pearl not owned: the original shop keeper (type 0) is not there.
    dComIfGs_onEventBit(dSv_event_flag_c::ENDLESS_NIGHT);
    offSymbol(dSymbol_NAYRU_e);
}
void setupBm1() {
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1620);
}
void setupOs() {
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1710);
}
void setupNone() {}

const Task kTasks[] = {
    // MO2 mode 1 (with a lantern), no path, no spawn switch.
    {"MO2 lantern Moblin", fpcNm_MO2_e, 0xFFFF0001, -1, false, false, true, setupMo2, kCreated, probeMo2Vanilla},
    {"MO2 lantern Moblin", fpcNm_MO2_e, 0xFFFF0001, -1, false, true, false, setupMo2, kCreated, probeMo2Rando},
    {"Obj_Akabe sword-only wall", fpcNm_Obj_Akabe_e, 0xFF, -1, false, false, true, setupAkabe, kCreated, probeAkabe},
    {"Obj_Ajav without Endless Night", fpcNm_Obj_Ajav_e, 0xFF, -1, true, false, true, setupNoEndlessNight, kRefused, nullptr},
    {"Obj_Ajav without Endless Night", fpcNm_Obj_Ajav_e, 0xFF, -1, true, true, false, setupNoEndlessNight, kCreated, nullptr},
    {"Obj_Auzu without Endless Night", fpcNm_Obj_Auzu_e, 0, -1, true, false, true, setupNoEndlessNight, kRefused, nullptr},
    {"Obj_Auzu without Endless Night", fpcNm_Obj_Auzu_e, 0, -1, true, true, false, setupNoEndlessNight, kCreated, probeAuzu},
    {"NPC_CO1 young Komali", fpcNm_NPC_CO1_e, 0, -1, false, false, true, setupNoDin, kCreated, probeCo1},
    {"Obj_Eayogn with Din's Pearl", fpcNm_Obj_Eayogn_e, 0, -1, true, false, true, setupDin, kCreated, nullptr},
    {"Obj_Eayogn with Din's Pearl", fpcNm_Obj_Eayogn_e, 0, -1, true, true, false, setupDin, kRefused, nullptr},
    {"NPC_BMS1 bomb shop", fpcNm_NPC_BMS1_e, 0, -1, false, false, true, setupBms1, kRefused, nullptr},
    {"NPC_BMS1 bomb shop", fpcNm_NPC_BMS1_e, 0, -1, false, true, false, setupBms1, kCreated, probeBms1},
    {"NPC_DE1 Great Deku Tree", fpcNm_NPC_DE1_e, 0, -1, false, false, false, setupNone, kCreated, probeDe1},
    {"NPC_BM5 Kogoli", fpcNm_NPC_BM5_e, 0x1, -1, false, false, false, setupBm1, kCreated, probeBm1},
    {"NPC_OS east servant", fpcNm_NPC_OS_e, 0, 0, false, false, false, setupOs, kCreated, probeOs},
    {"Obj_HSH Command Melody tablet", fpcNm_Obj_HSH_e, 0xFFFFFF, 1, false, false, false, setupNone, kCreated, probeHsh},
};
constexpr int kTaskCount = sizeof(kTasks) / sizeof(kTasks[0]);
constexpr unsigned int kCreateFrames = 600; // to finish creating
constexpr unsigned int kProbeFrames = 60;   // for a probe to finish
constexpr unsigned int kDeleteFrames = 120;
constexpr float kSpawnDistance = 300.0f;
// The stone door, the whirlpool and the lava floor: their collision must not push the player into the
// sea, and the whirlpool pulls the player in within 3750 units (L_radius 2500 x 1.5).
constexpr float kFarDistance = 20000.0f;

enum State { kStart, kSpawn, kWait, kProbe, kDelete, kEnd };
State sState = kStart;
int sTask = -1;
fpc_ProcID sId = fpcM_ERROR_PROCESS_ID_e;
unsigned int sSince = 0;
unsigned int sEndFrame = 0;

void* judgeCreatingById(void* req, void* id) {
    create_request* r = static_cast<create_request*>(req);
    return r->mBsPcId == *(fpc_ProcID*)id ? r : nullptr;
}
void* judgeExecutingById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}
// The whirlpool's wave tag (KYTAG01), which it creates without a parent link and does not delete.
void deleteWhirlpoolTag(base_process_class* proc) {
    if (fpcM_GetName(proc) == fpcNm_Obj_Auzu_e) {
        fpc_ProcID tag = static_cast<daObjAuzu::Act_c*>(proc)->mKytagPcId;
        if (void* p = fpcM_Search(judgeExecutingById, &tag)) {
            fpcM_Delete(p);
        }
    }
}

// Deletes the actors the spawned one created (MO2's lantern, ...).
void* deleteChild(void* proc, void* parent) {
    if (fopAc_IsActor(proc) && fopAcM_GetLinkId(static_cast<fopAc_ac_c*>(proc)) == *(fpc_ProcID*)parent) {
        fpcM_Delete(proc);
    }
    return nullptr;
}

void nextTask() {
    restoreRecords();
    rando(false);
    do {
        sTask++;
    } while (sTask < kTaskCount && sForced && kTasks[sTask].needsVanilla);
    sState = sTask < kTaskCount ? kSpawn : kEnd;
    sSince = 0;
}

void spawn(const Task& t) {
    t.setup();
    rando(t.randoOn);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const s16 yaw = player->shape_angle.y;
    cXyz pos = player->current.pos;
    const float distance = t.far ? kFarDistance : kSpawnDistance;
    pos.x += distance * cM_ssin(yaw);
    pos.z += distance * cM_scos(yaw);
    csXyz angle(0, (s16)(yaw + 0x8000), 0);
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(static_cast<base_process_class*>(player)->mLyTg.mpLayer);
    sId = fopAcM_create(t.proc, t.params, &pos, fopAcM_GetRoomNo(player), &angle, nullptr, t.argument);
    fpcLy_SetCurrentLayer(saved);
    sSince = 0;
    sState = kWait;
}

void result(const Task& t, bool created) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s: %s, %s", t.what, t.randoOn ? "randomizer" : "original game",
             t.expect == kCreated ? "created" : "refused");
    randoSmokeCheck(kName, buf, created == (t.expect == kCreated));
}

} // namespace

void randoSmokeNonlinB(unsigned int frames) {
    if (!randoSmokeInPlay(30)) {
        return;
    }
    if (sState == kEnd) {
        if (sEndFrame == 0) {
            sEndFrame = frames + 30;
        } else if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    if (sState == kStart) {
        sForced = pc_rando_forced() != 0;
        randoSmokeCheck(kName, sForced ? "forced boot: pc_rando() is 1" : "plain boot: pc_rando() is 0",
                        pc_rando() == (sForced ? 1 : 0));
        saveRecords();
        checkLayer();
        checkFmap();
        checkDoor();
        nextTask();
        return;
    }
    const Task& t = kTasks[sTask];
    sSince++;
    if (sState == kSpawn) {
        spawn(t);
        if (sId == fpcM_ERROR_PROCESS_ID_e) {
            result(t, false);
            nextTask();
        }
        return;
    }
    fpc_ProcID id = sId;
    create_request* creating = static_cast<create_request*>(fpcCtIt_Judge(judgeCreatingById, &id));
    base_process_class* executing = fpcM_Search(judgeExecutingById, &id);
    if (sState == kWait) {
        if (executing != nullptr) {
            result(t, true);
            sState = t.probe != nullptr && t.expect == kCreated ? kProbe : kDelete;
            sSince = 0;
        } else if (creating == nullptr) {
            result(t, false);
            nextTask();
            return;
        } else if (sSince > kCreateFrames) {
            randoSmokeCheck(kName, t.what, false);
            fpcM_Delete(creating->mpRes);
            sState = kDelete;
            sSince = 0;
            return;
        }
    }
    if (sState == kProbe) {
        if (executing == nullptr) {
            randoSmokeCheck(kName, "probed actor still there", false);
            nextTask();
            return;
        }
        if (t.probe(static_cast<fopAc_ac_c*>(executing), sSince)) {
            sState = kDelete;
            sSince = 0;
        } else if (sSince > kProbeFrames) {
            randoSmokeCheck(kName, "probe finished in time", false);
            sState = kDelete;
            sSince = 0;
        }
        if (sState != kDelete) {
            return;
        }
    }
    // kDelete
    if (creating == nullptr && executing == nullptr) {
        nextTask();
        return;
    }
    if (sSince > kDeleteFrames) {
        randoSmokeCheck(kName, "spawned actor deleted", false);
        nextTask();
        return;
    }
    if ((sSince & 15) == 0) {
        if (executing != nullptr) {
            deleteWhirlpoolTag(executing);
        }
        fpcM_Search(deleteChild, &id);
        fpcM_Delete(executing != nullptr ? executing : creating->mpRes);
    }
}

} // namespace pc
