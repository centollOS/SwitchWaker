// COS_SMOKE=rando COS_RANDO_SMOKE=swop (docs/RANDOMIZER_PLAN.md step R3.7): the randomizer's custom
// actors SwOp (game/src/d/actor/d_a_switch_op.cpp) and DngSw (d_a_dungeon_flag_sw.cpp) in a booted
// game, e.g. run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=swop. Once the play scene has run 10
// frames (T = that frame), in the player's room, with the room's zone switches 0xC0-0xDF cleared:
//   1. "SwOp" and "DngSw" resolve through l_objectName (dStage_searchName, as stage data does) to
//      fpcNm_SWITCH_OP_e/fpcNm_DUNGEON_FLAG_SW_e (0x1F6/0x1F7 on USA), whose profiles exist; 9 SwOps
//      and the DngSw below are created;
//   2. truth table: 6 continuous SwOps without delay, one per operation (AND NAND OR NOR XOR XNOR)
//      over switches 0xC0-0xC1, set 0xD0-0xD5; the inputs go 00 10 11 01 00, 10 frames each, and
//      every output follows its operation (set and cleared again);
//   3. delay: a one-shot AND over 0xC2-0xC3 with delay 20 sets 0xD8 20-23 frames after its inputs;
//      it keeps it set (the REL's one-shot-with-delay state sets it again every frame, so clearing
//      it by hand comes back);
//   4. a continuous OR over 0xC4 with delay 20: an input cleared before the delay ends sets nothing;
//      set again, the output comes 20-23 frames later and stays set when the input clears (the REL
//      ends a continuous SwOp with a delay and no event there);
//   5. a one-shot OR without delay over 0xC0-0xC1 sets 0xDB and keeps it when the inputs clear;
//   6. DngSw (switch 0xDA, stage mask bit 3, condition 0 = map) leaves 0xDA clear until stage 3's
//      map bit is set, then sets it and deletes itself.
// Exit 0 when all hold, else 1. The event path of SwOp (x_rot EVNT index) needs stage data and is
// checked by the steps that add it (R3.17, R3.18).
#include "pc_rando_smokes.h"

#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_profile.h"

#include <cstdio>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "swop";
constexpr int kOps = 6;
constexpr int kOutBase = 0xD0; // truth-table outputs 0xD0-0xD5
constexpr int kOutDelay = 0xD8;
constexpr int kOutContDelay = 0xD9;
constexpr int kOutDng = 0xDA;
constexpr int kOutOneShot = 0xDB;
constexpr int kDngStage = 3;

unsigned int sT0 = 0;
int sRoom = 0;
// Frame (relative to T) at which each watched output was first seen set, -1 while not.
int sDelaySetAt = -1, sContDelaySetAt = -1, sDngSetAt = -1, sOneShotSetAt = -1;

u32 swopParam(int op, bool continuous, int out, int first, int num) {
    return (u32)op | (continuous ? 0x10u : 0u) | ((u32)out << 8) | ((u32)first << 16) | ((u32)num << 24);
}

bool isSw(int sw) {
    return dComIfGs_isSwitch(sw, sRoom) != 0;
}

void setSw(int sw, bool on) {
    if (on) {
        dComIfGs_onSwitch(sw, sRoom);
    } else {
        dComIfGs_offSwitch(sw, sRoom);
    }
}

void spawnSwop(fopAc_ac_c* player, u32 param, u8 evnt, u8 delay) {
    cXyz pos = player->current.pos;
    csXyz angle(evnt, 0, delay);
    char name[] = "SwOp";
    fopAcM_create(name, param, &pos, sRoom, &angle);
}

struct Count {
    s16 proc;
    int n;
};

void* countProc(void* proc, void* data) {
    Count* c = static_cast<Count*>(data);
    if (fpcM_GetName(proc) == c->proc) {
        c->n++;
    }
    return nullptr;
}

int countActors(s16 proc) {
    Count c = {proc, 0};
    fopAcM_Search(countProc, &c);
    return c.n;
}

bool expected(int op, bool a, bool b) {
    const int on = (a ? 1 : 0) + (b ? 1 : 0);
    switch (op) {
    case 0: return on == 2;  // AND
    case 1: return on < 2;   // NAND
    case 2: return on > 0;   // OR
    case 3: return on == 0;  // NOR
    case 4: return on == 1;  // XOR
    default: return on != 1; // XNOR
    }
}

const char* const kOpNames[kOps] = {"AND", "NAND", "OR", "NOR", "XOR", "XNOR"};
// Inputs (0xC0, 0xC1) per 10-frame window starting at T+10.
const bool kCombo[5][2] = {{false, false}, {true, false}, {true, true}, {false, true}, {false, false}};

void checkTruth(int window) {
    const bool a = kCombo[window][0], b = kCombo[window][1];
    for (int op = 0; op < kOps; op++) {
        char what[96];
        snprintf(what, sizeof what, "%s(%d,%d) -> 0x%02X is %d", kOpNames[op], a, b, kOutBase + op,
                 expected(op, a, b));
        randoSmokeCheck(kName, what, isSw(kOutBase + op) == expected(op, a, b));
    }
}

void watch(int t, int sw, int& at) {
    if (at < 0 && isSw(sw)) {
        at = t;
        writef(STDERR_FILENO, "[cos] rando-smoke swop: switch 0x%02X set at T+%d\n", sw, t);
    }
}

} // namespace

void randoSmokeSwop(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    if (sT0 == 0) {
        sT0 = frames;
        sRoom = fopAcM_GetRoomNo(player);
        writef(STDERR_FILENO, "[cos] rando-smoke swop: room %d\n", sRoom);

        dStage_objectNameInf* swop = dStage_searchName("SwOp");
        dStage_objectNameInf* dng = dStage_searchName("DngSw");
        randoSmokeCheck(kName, "l_objectName SwOp -> fpcNm_SWITCH_OP_e",
                        swop != nullptr && swop->procname == fpcNm_SWITCH_OP_e);
        randoSmokeCheck(kName, "l_objectName DngSw -> fpcNm_DUNGEON_FLAG_SW_e",
                        dng != nullptr && dng->procname == fpcNm_DUNGEON_FLAG_SW_e);
        process_profile_definition* p1 = fpcPf_Get(fpcNm_SWITCH_OP_e);
        process_profile_definition* p2 = fpcPf_Get(fpcNm_DUNGEON_FLAG_SW_e);
        randoSmokeCheck(kName, "profiles registered",
                        p1 != nullptr && p1->mProcName == fpcNm_SWITCH_OP_e && p2 != nullptr &&
                            p2->mProcName == fpcNm_DUNGEON_FLAG_SW_e);

        for (int sw = 0xC0; sw <= 0xDF; sw++) {
            setSw(sw, false);
        }
        randoSmokeCheck(kName, "stage 3 map bit clear at start",
                        !g_dComIfG_gameInfo.save.getSavedata().getSave(kDngStage).getBit().isDungeonItemMap());

        for (int op = 0; op < kOps; op++) {
            spawnSwop(player, swopParam(op, true, kOutBase + op, 0xC0, 2), 0xFF, 0);
        }
        spawnSwop(player, swopParam(0, false, kOutDelay, 0xC2, 2), 0xFF, 20);
        spawnSwop(player, swopParam(2, true, kOutContDelay, 0xC4, 1), 0xFF, 20);
        spawnSwop(player, swopParam(2, false, kOutOneShot, 0xC0, 2), 0xFF, 0);
        cXyz pos = player->current.pos;
        char dngName[] = "DngSw";
        fopAcM_create(dngName, (0u << 24) | ((1u << kDngStage) << 8) | (u32)kOutDng, &pos, sRoom);
        return;
    }

    const int t = (int)(frames - sT0);
    watch(t, kOutDelay, sDelaySetAt);
    watch(t, kOutContDelay, sContDelaySetAt);
    watch(t, kOutDng, sDngSetAt);
    watch(t, kOutOneShot, sOneShotSetAt);

    if (t >= 10 && t <= 60 && (t - 10) % 10 == 0) {
        const int window = (t - 10) / 10;
        if (window > 0) {
            checkTruth(window - 1);
        }
        if (window < 5) {
            setSw(0xC0, kCombo[window][0]);
            setSw(0xC1, kCombo[window][1]);
        }
    }

    switch (t) {
    case 10:
        writef(STDERR_FILENO, "[cos] rando-smoke swop: %d SwOp, %d DngSw at T+10\n", countActors(fpcNm_SWITCH_OP_e),
               countActors(fpcNm_DUNGEON_FLAG_SW_e));
        randoSmokeCheck(kName, "9 SwOp and 1 DngSw created",
                        countActors(fpcNm_SWITCH_OP_e) == 9 && countActors(fpcNm_DUNGEON_FLAG_SW_e) == 1);
        randoSmokeCheck(kName, "nothing set before the inputs",
                        sDelaySetAt < 0 && sContDelaySetAt < 0 && sDngSetAt < 0 && sOneShotSetAt < 0);
        setSw(0xC2, true);
        setSw(0xC3, true);
        setSw(0xC4, true);
        break;
    case 12:
        g_dComIfG_gameInfo.save.getSavedata().getSave(kDngStage).getBit().onDungeonItemMap();
        break;
    case 20:
        // Continuous OR with delay 20: the input goes away before the delay ends.
        setSw(0xC4, false);
        break;
    case 25: {
        char what[96];
        snprintf(what, sizeof what, "DngSw: 0x%02X set after the map bit (at T+%d) and deleted", kOutDng,
                 sDngSetAt);
        randoSmokeCheck(kName, what, sDngSetAt >= 12 && sDngSetAt <= 16 && countActors(fpcNm_DUNGEON_FLAG_SW_e) == 0);
        break;
    }
    case 50: {
        char what[96];
        snprintf(what, sizeof what, "one-shot AND delay 20: 0x%02X set at T+%d (inputs at T+10)", kOutDelay,
                 sDelaySetAt);
        randoSmokeCheck(kName, what, sDelaySetAt >= 30 && sDelaySetAt <= 33);
        setSw(0xC2, false);
        break;
    }
    case 60:
        randoSmokeCheck(kName, "continuous OR delay 20: input cleared during the delay sets nothing",
                        sContDelaySetAt < 0);
        randoSmokeCheck(kName, "one-shot AND delay: output kept after an input clears", isSw(kOutDelay));
        // The REL's quirk: a one-shot SwOp with a delay and no event sets its switch every frame.
        setSw(kOutDelay, false);
        setSw(0xC4, true);
        break;
    case 65:
        randoSmokeCheck(kName, "one-shot AND delay: output set again after a manual clear", isSw(kOutDelay));
        break;
    case 90: {
        char what[96];
        snprintf(what, sizeof what, "continuous OR delay 20: 0x%02X set at T+%d (input again at T+60)",
                 kOutContDelay, sContDelaySetAt);
        randoSmokeCheck(kName, what, sContDelaySetAt >= 80 && sContDelaySetAt <= 83);
        setSw(0xC4, false);
        break;
    }
    case 100: {
        randoSmokeCheck(kName, "continuous OR delay 20: output kept after the input clears (REL quirk)",
                        isSw(kOutContDelay));
        char what[96];
        snprintf(what, sizeof what, "one-shot OR: 0x%02X set at T+%d (0xC0 at T+20) and kept", kOutOneShot,
                 sOneShotSetAt);
        randoSmokeCheck(kName, what, sOneShotSetAt >= 20 && sOneShotSetAt <= 22 && isSw(kOutOneShot));
        randoSmokeEnd(kName);
    }
    default:
        break;
    }
}

} // namespace pc
