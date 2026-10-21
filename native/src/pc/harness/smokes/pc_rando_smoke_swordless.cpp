// COS_SMOKE=rando COS_RANDO_SMOKE=swordless (docs/RANDOMIZER_PLAN.md step R6.3): the Swordless mode's
// game side (game_hooks/pc_rando_hooks_sword.cpp, the phase_4 hunks of d_s_play.cpp), e.g.
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=swordless [--env COS_RANDO_FORCE=1]
// The run hops between stages (dComIfGp_setNextStage) and checks each stop kSettle frames after the
// play scene's player appeared. The save starts with no sword owned (collect byte 0 is 0) and none
// equipped, and the bow owned (so phase_4 gives it back when it is not in its slot):
//   0. boot (sea): not forced: pc_rando() is 0 and 1128/1590 have no overlay record; then -> GTower;
//   1. GTower, not forced: vanilla phase_4 gives no sword; then the smoke activates a Swordless config
//      (forced: at stop 0 already) and checks 1128/1590 hold wwrando's update_text_for_swordless texts;
//      -> GTower;
//   2. GTower, Swordless: the Hero's Sword (0x38) is equipped (give_temporary_sword_during_ganondorf_
//      fight_in_swordless) and the bow slot is empty; -> sea;
//   3. sea: the temporary sword is removed (remove_temporary_sword_when_loading_stage_in_swordless) and
//      the bow given back; the bow slot is emptied; -> Ojhous;
//   4. Ojhous: the Hero's Sword is equipped again (give_temporary_sword_in_orcas_house_in_swordless) and
//      the bow slot was not given back on this load; -> sea;
//   5. sea: removed again, the bow given back; then the save owns a sword (collect bit 0x01) with the Hero's Sword equipped;
//      -> sea;
//   6. sea: an owned sword is not removed. A Start-with-sword config is then activated: 1128/1590
//      leave the overlay.
// Each stop's checks use the state at the first frame its play scene is found (phase_4 has run, the
// actors not yet). GTower's opening event (debug hop) gives the Full Power Master Sword and keeps it
// equipped while the scene runs, so after a GTower stop the stop's arrival state is put back in the
// frames between the GTower scene's end and the next scene's phase_4. Event bit 0x2D01 is set for the GTower hops (a debug hop into GTower needs it, as
// native/tools/combat_sweep.py boots it). The save state the smoke changes is restored before it ends. Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_msg.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "swordless";
constexpr unsigned int kSettle = 30;
constexpr unsigned int kArriveFrames = 1500;

struct Stop {
    const char* stage;
    s8 room;
    s16 point;
};
// The stage each stop hops to next (index = the stop just checked).
const Stop kHops[] = {
    {"GTower", 0, 1}, // after 0
    {"GTower", 0, 1}, // after 1
    {"sea", 44, 0},   // after 2
    {"Ojhous", 0, 0}, // after 3
    {"sea", 44, 0},   // after 4
    {"sea", 44, 0},   // after 5
};

enum State { kInit, kArrive, kDone };
State sState = kInit;
int sStop = 0;
unsigned int sSince = 0;
unsigned int sPresent = 0;
// The sword, collect byte 0 and bow at the first frame the stop's play scene is found (its phase_4 has
// run; its actors have not: GTower's opening demo in a debug hop gives the Full Power Master Sword,
// d_a_demo00 l_itemNo[2]).
u8 sArriveEquip = 0xFF;
u8 sArriveCollect = 0;
u8 sArriveBow = 0xFF;
// After a GTower stop: the state to put back once the GTower scene is gone (before the next phase_4).
bool sRestore = false;
bool sCaptured = false;
u8 sRestoreEquip = 0xFF;
u8 sRestoreCollect = 0;
u8 sRestoreBow = 0xFF;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
bool sForced = false;

// Saved at boot, restored at the end.
u8 sBootEquip = 0xFF;
u8 sBootCollect = 0;
u8 sBootBow = 0xFF;
bool sBootEvent = false;
dSv_player_get_item_c sBootGetItem;

// GTower's opening needs this event bit in a debug hop (as native/tools/combat_sweep.py boots it).
constexpr u16 kGTowerEvent = 0x2D01;

bool check(const char* what, bool ok) {
    return randoSmokeCheck(kName, what, ok);
}

void* findPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(findPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

// The equipped-sword byte alone (dComIfGs_setSelectEquip would also mark the sword as owned).
void setEquip(u8 item) {
    g_dComIfG_gameInfo.save.getPlayer().getPlayerStatusA().setSelectEquip(0, item);
}

u8 equipNow() {
    return dComIfGs_getSelectEquip(0);
}

u8 equip() {
    return sArriveEquip;
}

dSv_player_collect_c& collect() {
    return g_dComIfG_gameInfo.save.getPlayer().getCollect();
}

u8 collectNow() {
    return collect().checkCollect(0);
}

u8 collect0() {
    return sArriveCollect;
}

void setCollect0(u8 v) {
    collect().setCollect(0, v);
}

u8 bowNow() {
    return dComIfGs_getItem(dInvSlot_BOW_e);
}

u8 bow() {
    return sArriveBow;
}

bool textsAbsent() {
    return pc_rando_msg_find(1128) == nullptr && pc_rando_msg_find(1590) == nullptr;
}

// Overlay record `no` holds `want` (encoded bytes with their NUL).
bool recordIs(u16 no, const char* want, size_t len) {
    const PcRandoMsg* m = pc_rando_msg_find(no);
    return m != nullptr && m->bytes != nullptr && m->len == len && memcmp(m->bytes, want, len) == 0;
}

void checkTexts() {
    static const char k1590[] = "\x1A\x05\x00\x00\x00"
                                "! Do not run! Trust in the\npower of the Skull Hammer!";
    static const char k1128[] = "\x1A\x05\x00\x00\x00"
                                ", you may not have the\nMaster Sword, but do not be afraid!\n\n\n"
                                "The hammer of the dead is all you\nneed to crush your foe...\n\n\n"
                                "Even as his ball of fell magic bears down\non you, you can "
                                "\x1A\x06\xFF\x00\x00\x01"
                                "knock it back\nwith an empty bottle"
                                "\x1A\x06\xFF\x00\x00\x00"
                                "!\n\n...I am sure you will have a shot at victory!";
    check("Swordless: pc_rando_swordless() is 1", pc_rando_swordless() == 1);
    check("Swordless: 1128 is update_text_for_swordless's text", recordIs(1128, k1128, sizeof k1128));
    check("Swordless: 1590 is update_text_for_swordless's text", recordIs(1590, k1590, sizeof k1590));
    char buf[0x200];
    fopMsgM_messageGet(buf, 1590);
    check("Swordless: messageGet 1590 reads the overlay", strstr(buf, "Trust in the") != nullptr);
}

void activate(u8 mode) {
    PcRandoConfig cfg;
    pc_rando_config_defaults(&cfg);
    cfg.sword_mode = mode;
    pc_rando_set_active(nullptr, &cfg);
}

void report(const char* what, u8 got, u8 want) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s (0x%02X, want 0x%02X)", what, got, want);
    check(buf, got == want);
}

void finish() {
    setEquip(sBootEquip);
    setCollect0(sBootCollect);
    dComIfGs_setItem(dInvSlot_BOW_e, sBootBow);
    g_dComIfG_gameInfo.save.getPlayer().getGetItem() = sBootGetItem;
    if (!sBootEvent) {
        dComIfGs_offEventBit(kGTowerEvent);
    }
    if (!sForced) {
        pc_rando_deactivate();
        check("deactivated: no overlay record for 1128/1590", textsAbsent());
    }
    sState = kDone;
    randoSmokeEnd(kName);
}

void hop() {
    const Stop& s = kHops[sStop];
    writef(STDERR_FILENO, "[cos] rando-smoke %s: stop %d done: to %s room %d point %d\n", kName, sStop, s.stage, s.room,
           s.point);
    sOldScene = playSceneId();
    dComIfGp_setNextStage(s.stage, s.point, s.room, -1);
    sStop++;
    sState = kArrive;
    sCaptured = false;
    sSince = 0;
    sPresent = 0;
}

// Undoes what GTower's opening event gave (it keeps equipping the sword while the GTower scene runs),
// so the next stop sees phase_4's state.
void restore() {
    setEquip(sRestoreEquip);
    setCollect0(sRestoreCollect);
    dComIfGs_setItem(dInvSlot_BOW_e, sRestoreBow);
    sRestore = false;
}

// The checks of stop sStop (its stage is loaded), then the setup for the next one.
void runStop() {
    const char* stage = dComIfGp_getStartStageName();
    writef(STDERR_FILENO, "[cos] rando-smoke %s: stop %d: stage %s, pc_rando() %d, sword 0x%02X, collect 0x%02X, bow 0x%02X\n",
           kName, sStop, stage, pc_rando(), equip(), collect0(), bow());
    switch (sStop) {
    case 0:
        sBootEquip = equip();
        sBootCollect = collect0();
        sBootBow = bow();
        sBootGetItem = g_dComIfG_gameInfo.save.getPlayer().getGetItem();
        dComIfGs_onGetItem(dInvSlot_BOW_e, 0); // the bow is owned, so phase_4 gives it back after GTower
        sBootEvent = dComIfGs_isEventBit(kGTowerEvent);
        dComIfGs_onEventBit(kGTowerEvent);
        if (!sForced) {
            check("plain: pc_rando() is 0", pc_rando() == 0);
            check("plain: pc_rando_swordless() is 0", pc_rando_swordless() == 0);
            check("plain: no overlay record for 1128/1590", textsAbsent());
        } else {
            activate(PC_RANDO_SWORD_SWORDLESS);
            checkTexts();
            sStop = 1; // the plain GTower stop is skipped
        }
        setCollect0(0);
        setEquip(dItemNo_NONE_e);
        break;
    case 1:
        check("plain GTower: at GTower", strcmp(stage, "GTower") == 0);
        report("plain GTower: no sword is given", equip(), dItemNo_NONE_e);
        report("plain GTower: the bow is taken", bow(), dItemNo_NONE_e);
        activate(PC_RANDO_SWORD_SWORDLESS);
        checkTexts();
        break;
    case 2:
        check("Swordless GTower: at GTower", strcmp(stage, "GTower") == 0);
        report("Swordless GTower: the Hero's Sword is equipped", equip(), dItemNo_SWORD_e);
        report("Swordless GTower: the bow is taken", bow(), dItemNo_NONE_e);
        check("Swordless GTower: no sword is owned", collect0() == 0);
        break;
    case 3:
        check("Swordless sea: at sea", strcmp(stage, "sea") == 0);
        report("Swordless sea: the temporary sword is removed", equip(), dItemNo_NONE_e);
        report("Swordless sea: the bow is given back", bow(), dItemNo_BOW_e);
        dComIfGs_setItem(dInvSlot_BOW_e, dItemNo_NONE_e); // Ojhous must not give it back
        break;
    case 4:
        check("Swordless Ojhous: at Ojhous", strcmp(stage, "Ojhous") == 0);
        report("Swordless Ojhous: the Hero's Sword is equipped", equip(), dItemNo_SWORD_e);
        report("Swordless Ojhous: the bow is not given back on this load", bow(), dItemNo_NONE_e);
        break;
    case 5:
        check("Swordless sea: at sea", strcmp(stage, "sea") == 0);
        report("Swordless sea (from Ojhous): the temporary sword is removed", equip(), dItemNo_NONE_e);
        report("Swordless sea (from Ojhous): the bow is given back", bow(), dItemNo_BOW_e);
        setCollect0(0x01);
        setEquip(dItemNo_SWORD_e);
        break;
    case 6:
        report("Swordless sea: an owned sword is kept", equip(), dItemNo_SWORD_e);
        activate(PC_RANDO_SWORD_START);
        check("Start with sword: pc_rando_swordless() is 0", pc_rando_swordless() == 0);
        check("Start with sword: no overlay record for 1128/1590", textsAbsent());
        finish();
        return;
    default:
        break;
    }
    sRestore = strcmp(stage, "GTower") == 0;
    sRestoreEquip = sArriveEquip;
    sRestoreCollect = sArriveCollect;
    sRestoreBow = sArriveBow;
    hop();
}

} // namespace

void randoSmokeSwordless(unsigned int frames) {
    switch (sState) {
    case kInit:
        if (!randoSmokeInPlay(kSettle)) {
            return;
        }
        sForced = pc_rando_forced() != 0;
        sArriveEquip = equipNow();
        sArriveCollect = collectNow();
        sArriveBow = bowNow();
        runStop();
        return;
    case kArrive: {
        sSince++;
        const fpc_ProcID scene = playSceneId();
        if (sRestore && scene != sOldScene) {
            if (scene != fpcM_ERROR_PROCESS_ID_e) {
                check("the GTower scene is gone for a frame before the next one runs", false);
            }
            restore();
        }
        if (!sCaptured && scene != fpcM_ERROR_PROCESS_ID_e && scene != sOldScene) {
            // The new play scene is up: its phase_4 has run, its actors (and GTower's demo) not yet.
            sCaptured = true;
            sArriveEquip = equipNow();
            sArriveCollect = collectNow();
            sArriveBow = bowNow();
            writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u: scene up (player %s): sword 0x%02X, collect 0x%02X, bow 0x%02X\n",
                   kName, frames, dComIfGp_getPlayer(0) != nullptr ? "spawned" : "not yet", sArriveEquip, sArriveCollect,
                   sArriveBow);
        }
        if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || dComIfGp_getPlayer(0) == nullptr) {
            sPresent = 0;
            if (sSince >= kArriveFrames) {
                char what[96];
                snprintf(what, sizeof what, "stop %d (%s) reached", sStop, kHops[sStop - 1].stage);
                check(what, false);
                finish();
            }
            return;
        }
        if (++sPresent < kSettle) {
            return;
        }
        runStop();
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc
