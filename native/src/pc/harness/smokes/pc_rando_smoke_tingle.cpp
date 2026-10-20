// COS_SMOKE=rando COS_RANDO_SMOKE=tingle (docs/RANDOMIZER_PLAN.md step R3.6): the Tingle Statues
// (docs/RANDO_HOOKS.md "## R3.6"), on Outset:
//   native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=tingle
//
// Each check runs with the randomizer off (the original game; skipped under COS_RANDO_FORCE=1, where
// it cannot be turned off) and on, and asserts both outcomes. In the original game a statue is owned
// when its dungeon chest (opened flag 0xF of stage 3-7) is open; under the randomizer when its event
// bit (0x6A04-0x6A40) is set, which its item function sets.
//
// - Direct calls, in the first frame:
//   * execItemGet(0xA3..0xA7): no bit in the original game; under the randomizer each sets its bit,
//     pc_rando_tingle_statue_owned follows, and the quest-screen count goes 1..5;
//   * on a daNpc_Tc_c that is only storage (the functions read no other member):
//     existTcMonument, next_msgStatusBlue(0xDDD), getMsgRed (a blue-type brother with a dummy tower
//     pointer: stopTower does nothing for that type), checkAllMonumentFee and checkAllMonumentPay.
//     With all 5 statues owned, checkAllMonumentPay pays 5 and checkAllMonumentFee is then true:
//     Ankle's reward state;
//   * daAgbsw0_c::MoveCheck conditions 9-D on storage only (those cases read only the save);
//   * the quest-screen texts 503/703 in the message overlay while the randomizer is on.
// - The Tingle Island statue for the Dragon Roost Cavern (Obj_Vtil, type 0) spawned next to the
//   player: created only when the statue is owned the active way, refused otherwise.
//
// Not run here: daNpc_Tc_HIO_c's constructor (a static object of d_a_npc_tc.cpp, built before main,
// whose fields nothing reads), daAgb_c::FlagsSend (the GBA link) and dMenu_Collect_c::screenSet (the
// quest screen's textures); the census and RANDO_HOOKS.md cover them, and they call the same
// pc_rando_tingle_statue_owned / pc_rando_tingle_statue_count checked here.
//
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_tingle.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "d/actor/d_a_agbsw0.h"
#include "d/actor/d_a_npc_tc.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_create_iter.h"
#include "f_pc/f_pc_create_req.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_math.h"

#include <cstdio>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "tingle";

template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
#define TINGLE_ROB(TAG, CLASS, TYPE, MEMBER)                                                          \
    struct TAG {                                                                                     \
        using type = TYPE CLASS::*;                                                                  \
        friend type get(TAG);                                                                        \
    };                                                                                               \
    template struct Rob<TAG, &CLASS::MEMBER>;
TINGLE_ROB(TcType, daNpc_Tc_c, u8, mType)
TINGLE_ROB(TcTower, daNpc_Tc_c, daObjSmplbg::Act_c*, m_tower_actor)
TINGLE_ROB(TcRedTalked, daNpc_Tc_c, bool, field_0x810)
#undef TINGLE_ROB

bool sForced = false;

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

// The records the checks change, restored after each one.
dSv_event_c sEvents;
dSv_memory_c sStages[PCR_TINGLE_STATUE_COUNT];

dSv_event_c& events() {
    return g_dComIfG_gameInfo.save.getEvent();
}
dSv_memory_c& stageSave(int i) {
    return g_dComIfG_gameInfo.save.getSavedata().getSave(PCR_TINGLE_FIRST_STAGE + i);
}
void saveRecords() {
    sEvents = events();
    for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
        sStages[i] = stageSave(i);
    }
}
void restoreRecords() {
    events() = sEvents;
    for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
        stageSave(i) = sStages[i];
    }
}

// Ownership set up for a check: the vanilla chests (opened flag 0xF) and/or the randomizer's bits,
// for the statues in `mask` (bit i = statue i). The fee bits (Ankle's payments) are cleared.
void setOwned(unsigned mask, bool chests, bool bits) {
    static const u16 kFee[PCR_TINGLE_STATUE_COUNT] = {
        dSv_event_flag_c::UNK_1240, dSv_event_flag_c::UNK_1D08, dSv_event_flag_c::UNK_1D04,
        dSv_event_flag_c::UNK_1D01, dSv_event_flag_c::UNK_1D02,
    };
    for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
        const bool on = (mask >> i) & 1;
        stageSave(i) = sStages[i]; // the boot's records: no statue chest open (checked at the start)
        if (on && chests) {
            dComIfGs_onSaveTbox(PCR_TINGLE_FIRST_STAGE + i, PCR_TINGLE_CHEST_FLAG);
        }
        dComIfGs_offEventBit(pc_rando_tingle_bit(i));
        if (on && bits) {
            dComIfGs_onEventBit(pc_rando_tingle_bit(i));
        }
        dComIfGs_offEventBit(kFee[i]);
    }
}

void check(const char* what, bool on, bool ok) {
    char buf[200];
    snprintf(buf, sizeof buf, "%s: %s", what, on ? "randomizer" : "original game");
    randoSmokeCheck(kName, buf, ok);
}

int bitsOwned() {
    int n = 0;
    for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
        n += dComIfGs_isEventBit(pc_rando_tingle_bit(i)) ? 1 : 0;
    }
    return n;
}

void checkItemFuncs(bool on) {
    rando(on);
    setOwned(0, false, false);
    bool seq = true;
    for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
        execItemGet((u8)(PCR_TINGLE_FIRST_ITEM + i));
        if (on) {
            seq = seq && dComIfGs_isEventBit(pc_rando_tingle_bit(i)) &&
                  pc_rando_tingle_statue_owned(PCR_TINGLE_FIRST_STAGE + i, PCR_TINGLE_CHEST_FLAG) &&
                  !pc_rando_tingle_statue_owned(PCR_TINGLE_FIRST_STAGE + i, 0xE) &&
                  pc_rando_tingle_statue_count() == i + 1;
        }
    }
    if (on) {
        check("execItemGet 0xA3-0xA7 set 0x6A04-0x6A40, owned check and count follow", on, seq);
        check("stage 2 and 8 are never a statue", on,
              !pc_rando_tingle_statue_owned(2, 0xF) && !pc_rando_tingle_statue_owned(8, 0xF));
    } else {
        check("execItemGet 0xA3-0xA7 set no statue bit", on, bitsOwned() == 0);
    }
    restoreRecords();
}

// daNpc_Tc_c / daAgbsw0_c functions on storage only (see the header comment).
alignas(daNpc_Tc_c) unsigned char sTcStorage[sizeof(daNpc_Tc_c)];
alignas(daAgbsw0_c) unsigned char sAgbStorage[sizeof(daAgbsw0_c)];

daNpc_Tc_c* tc() {
    memset(sTcStorage, 0, sizeof sTcStorage);
    daNpc_Tc_c* t = reinterpret_cast<daNpc_Tc_c*>(sTcStorage);
    t->*get(TcType()) = daNpc_Tc_c::TYPE_BLUE;
    t->*get(TcTower()) = reinterpret_cast<daObjSmplbg::Act_c*>(sTcStorage); // never dereferenced
    return t;
}
daAgbsw0_c* agb() {
    memset(sAgbStorage, 0, sizeof sAgbStorage);
    return reinterpret_cast<daAgbsw0_c*>(sAgbStorage);
}

// The NPC checks with every statue owned the way `chests`/`bits` say; `owned` is whether the active
// mode should count them.
void checkNpc(bool on, bool chests, bool bits, bool owned, const char* how) {
    char what[160];
    rando(on);
    setOwned(0x1F, chests, bits);

    snprintf(what, sizeof what, "%s: existTcMonument", how);
    check(what, on, tc()->existTcMonument() == owned);

    u32 msg = 0xDDD;
    tc()->next_msgStatusBlue(&msg);
    snprintf(what, sizeof what, "%s: next_msgStatusBlue 0xDDD -> %s", how, owned ? "0xDDE" : "0xDF9");
    check(what, on, msg == (owned ? 0xDDEu : 0xDF9u));

    for (int c = 9; c <= 0xD; c++) {
        snprintf(what, sizeof what, "%s: daAgbsw0_c::MoveCheck(0x%X)", how, c);
        check(what, on, (agb()->MoveCheck((s16)c) == FALSE) == owned);
    }

    dComIfGs_onEventBit(dSv_event_flag_c::UNK_0B80);
    snprintf(what, sizeof what, "%s: getMsgRed (statues to trade) -> %s", how, owned ? "0xDEE" : "0xDE6");
    check(what, on, tc()->getMsgRed() == (owned ? 0xDEEu : 0xDE6u));

    snprintf(what, sizeof what, "%s: checkAllMonumentFee before paying is false", how);
    check(what, on, !tc()->checkAllMonumentFee());
    const int paid = tc()->checkAllMonumentPay();
    snprintf(what, sizeof what, "%s: checkAllMonumentPay pays %d", how, owned ? 5 : 0);
    check(what, on, paid == (owned ? 5 : 0));
    snprintf(what, sizeof what, "%s: checkAllMonumentFee after paying is %s (Ankle's reward state)", how,
             owned ? "true" : "false");
    check(what, on, tc()->checkAllMonumentFee() == owned);
    {
        daNpc_Tc_c* t = tc();
        t->*get(TcRedTalked()) = false;
        snprintf(what, sizeof what, "%s: getMsgRed after paying -> 0xDE6", how);
        check(what, on, t->getMsgRed() == 0xDE6u);
    }
    restoreRecords();
}

void checkMessages(bool on) {
    rando(on);
    const PcRandoMsg* name = pc_rando_msg_find(PCR_TINGLE_MSG_NAME);
    const PcRandoMsg* desc = pc_rando_msg_find(PCR_TINGLE_MSG_DESC);
    if (on) {
        check("quest-screen texts 503 and 703 in the message overlay", on, name != nullptr && desc != nullptr);
        check("pc_rando_tingle_msg_text 503 is \"Tingle Statues\"", on,
              strcmp(pc_rando_tingle_msg_text(PCR_TINGLE_MSG_NAME), "Tingle Statues") == 0);
    } else {
        check("no overlay text for 503 and 703", on, name == nullptr && desc == nullptr);
    }
}

// --- the Tingle Island statue (Obj_Vtil type 0: Dragon Roost Cavern) ---

struct Spawn {
    const char* what;
    bool on;
    bool chests;
    bool bits;
    bool created;
};
const Spawn kSpawns[] = {
    {"Obj_Vtil DRC, chest open", false, true, false, true},
    {"Obj_Vtil DRC, bit set", false, false, true, false},
    {"Obj_Vtil DRC, bit set", true, false, true, true},
    {"Obj_Vtil DRC, chest open", true, true, false, false},
};
constexpr int kSpawnCount = sizeof kSpawns / sizeof kSpawns[0];
constexpr unsigned int kCreateFrames = 300;
constexpr unsigned int kDeleteFrames = 120;

enum State { kStart, kSpawn, kWait, kDelete, kEnd };
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

void nextTask() {
    restoreRecords();
    rando(false);
    do {
        sTask++;
    } while (sTask < kSpawnCount && sForced && !kSpawns[sTask].on);
    sState = sTask < kSpawnCount ? kSpawn : kEnd;
    sSince = 0;
}

void spawn(const Spawn& t) {
    rando(t.on);
    setOwned(0x1F, t.chests, t.bits);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const s16 yaw = player->shape_angle.y;
    cXyz pos = player->current.pos;
    pos.x += 300.0f * cM_ssin(yaw);
    pos.z += 300.0f * cM_scos(yaw);
    csXyz angle(0, (s16)(yaw + 0x8000), 0);
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(static_cast<base_process_class*>(player)->mLyTg.mpLayer);
    sId = fopAcM_create(fpcNm_Obj_Vtil_e, 0 /* type 0: DRC */, &pos, fopAcM_GetRoomNo(player), &angle);
    fpcLy_SetCurrentLayer(saved);
    sSince = 0;
    sState = kWait;
}

void result(const Spawn& t, bool created) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s: %s", t.what, t.created ? "created" : "refused");
    check(buf, t.on, created == t.created);
}

} // namespace

void randoSmokeTingle(unsigned int frames) {
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
        randoSmokeCheck(kName, "item overrides registered for 0xA3-0xA7",
                        pc_rando_item_override_get(0xA3) != nullptr && pc_rando_item_override_get(0xA7) != nullptr &&
                            pc_rando_item_override_get(0xA8) == nullptr);
        saveRecords();
        bool noChest = true;
        for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
            noChest = noChest && !dComIfGs_isSaveTbox(PCR_TINGLE_FIRST_STAGE + i, PCR_TINGLE_CHEST_FLAG);
        }
        randoSmokeCheck(kName, "boot save: no statue chest open", noChest);
        randoSmokeCheck(kName, "quest-screen icon " PCR_TINGLE_QUEST_ICON " is in the item icon archive",
                        JKRGetResource('TIMG', PCR_TINGLE_QUEST_ICON, dComIfGp_getItemIconArchive()) != nullptr);
        for (int pass = sForced ? 1 : 0; pass < 2; pass++) {
            const bool on = pass == 1;
            checkItemFuncs(on);
            // Vanilla chests count only in the original game, the bits only under the randomizer.
            checkNpc(on, true, false, !on, "chests open");
            checkNpc(on, false, true, on, "bits set");
            checkNpc(on, false, false, false, "none owned");
            checkMessages(on);
        }
        rando(false);
        nextTask();
        return;
    }
    const Spawn& t = kSpawns[sTask];
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
            sState = kDelete;
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
        } else {
            return;
        }
    }
    // kDelete
    if (creating == nullptr && executing == nullptr) {
        nextTask();
        return;
    }
    if (sSince > kDeleteFrames) {
        randoSmokeCheck(kName, "spawned statue deleted", false);
        nextTask();
        return;
    }
    if ((sSince & 15) == 0) {
        fpcM_Delete(executing != nullptr ? executing : creating->mpRes);
    }
}

} // namespace pc
