// COS_SMOKE=rando COS_RANDO_SMOKE=windfall (docs/RANDOMIZER_PLAN.md step R2.7): the Windfall NPC code
// locations (docs/RANDO_HOOKS.md "## R2.7"), on Windfall:
//   native/tools/run.sh rando --stage sea:11 --env COS_RANDO_SMOKE=windfall
//
// Each check runs with the randomizer off (the original game; skipped under COS_RANDO_FORCE=1, where
// it cannot be turned off) and on, with a test placement that puts a non-vanilla item at each of the
// 15 code locations of this step, and asserts both outcomes.
//
// - T1: the item each hunk uses. d_a_npc_people's two tables go through pc_rando_people_item (Sam,
//   Pompie and Vera, Minenco, Linda and Anton, Kreeb, Kamo) and pc_rando_kane_item (Kane x4); Lenzo's
//   two slots, Maggie's two and the Salvage Corp's read pc_rando_loc_item in their hunks. Maggie's
//   daNpc_Kp1_c::event_action is also run for real (on an actor that is only storage): it creates
//   the demo item, whose item number is read back from the live actor, and the free item sets 0x6A01.
// - Probes: the "done" checks of each NPC, on storage actors (the functions read no other member),
//   with the NPC's quest done the vanilla way (the vanilla item owned / reserve slot set) or the
//   randomizer way (its event bit set). The NPC must be in its done state only for the active way:
//   * Pompie and Vera 0x6904: daNpcPeople_c::is1GetMap20;
//   * Kamo 0x6910: daNpcPeople_c::XyCheckCB (UM3 accepts a photo only while not done);
//   * Lenzo 0x6920: daNpcPhoto_c::XyEventCB (firefly hand-over event only while not done) and
//     daNpcPhoto_c::getMsg (done: the photo shop message 0x3786, else his first order 0x2A39);
//   * Maggie 0x6A01: daNpc_Kp1_c::getMsg (done: 0x1E87/0x1E88, else 0x1E8A); with Moblin's Letter
//     delivered (her quest complete) the original game says 0x1E83, the randomizer ignores it;
//   * the cafe postman 0x6A02: daNpc_Bm1_c::getMsg_BMC_2 (done: 0x1EF9, else 0x1EF0);
//   * the Salvage Corp 0x6980: daNpcSv_c's constructor (done: mTalkFlags 0x20), placement-new'd;
//   * pc_rando_people_item_given sets exactly 0x6904/0x6908/0x6910 for indices 1/2/5.
//
// Not run here (census and RANDO_HOOKS.md cover them, and they use the same bits): the people and
// Lenzo phase_1/createInit/_execute/setWaitAnm/getMsg sites, daNpcPhoto_c::executeTalk,
// daNpcSv_c::eventGetItemInit, daNpc_Bm1_c::init_BMC_2 and eventOrder (it orders a
// real event, which a storage actor cannot take part in), and daNpcPeople_c::eventGetItemInit and
// next_msgStatus (they need a running event / message flow).
//
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"

#include "d/actor/d_a_itembase.h"
#include "d/actor/d_a_npc_bm1.h"
#include "d/actor/d_a_npc_kp1.h"
#include "d/actor/d_a_npc_people.h"
#include "d/actor/d_a_npc_photo.h"
#include "d/actor/d_a_npc_sv.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"

#include <cstdio>
#include <cstring>
#include <new>

namespace pc {

namespace {

constexpr const char* kName = "windfall";

template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
#define WINDFALL_ROB(TAG, CLASS, TYPE, MEMBER)                                                        \
    struct TAG {                                                                                     \
        using type = TYPE CLASS::*;                                                                  \
        friend type get(TAG);                                                                        \
    };                                                                                               \
    template struct Rob<TAG, &CLASS::MEMBER>;
WINDFALL_ROB(PeopleNpcNo, daNpcPeople_c, u8, mNpcNo)
WINDFALL_ROB(PeopleNight, daNpcPeople_c, u8, mbIsNight)
WINDFALL_ROB(Bm1Talking, daNpc_Bm1_c, s8, m87C)
#undef WINDFALL_ROB

constexpr u8 kNpcUm3 = 0x0B; // NPC_UM3 (Kamo) in d_a_npc_people.cpp's NpcNo_e
constexpr u16 kLenzoReg = 0xC407; // l_save_dat.field_0x06 (d_a_npc_photo.cpp)
constexpr s16 kLenzoFireflyEvent = 0x1234; // sentinel in mPhotoGetItem2EventIdx

// --- the test placement ---
struct Loc {
    const char* name;
    int loc;
    u8 vanilla;
    u8 placed;
};
// Maggie's two get items with models (they are created as demo items); the rest are distinct ids
// only compared.
const Loc kPeople[6] = {
    {"Sam", PCR_LOC_WINDFALL_ISLAND_SAM_DECORATE_THE_TOWN, dItemNo_HEART_PIECE_e, 0x50},
    {"Pompie and Vera", PCR_LOC_WINDFALL_ISLAND_POMPIE_AND_VERA_SECRET_MEETING_PHOTO, dItemNo_COLLECT_MAP_20_e, 0x51},
    {"Minenco", PCR_LOC_WINDFALL_ISLAND_MINENCO_MISS_WINDFALL_PHOTO, dItemNo_COLLECT_MAP_16_e, 0x52},
    {"Linda and Anton", PCR_LOC_WINDFALL_ISLAND_LINDA_AND_ANTON, dItemNo_HEART_PIECE_e, 0x53},
    {"Kreeb", PCR_LOC_WINDFALL_ISLAND_KREEB_LIGHT_UP_LIGHTHOUSE, dItemNo_HEART_PIECE_e, 0x54},
    {"Kamo", PCR_LOC_WINDFALL_ISLAND_KAMO_FULL_MOON_PHOTO, dItemNo_COLLECT_MAP_15_e, 0x55},
};
const Loc kKane[4] = {
    {"Kane Shop Guru", PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SHOP_GURU_STATUE_ON_GATE, dItemNo_PURPLE_RUPEE_e, 0x56},
    {"Kane Postman", PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_POSTMAN_STATUE_ON_GATE, dItemNo_PURPLE_RUPEE_e, 0x57},
    {"Kane Six Flags", PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_FLAGS_ON_GATE, dItemNo_RED_RUPEE_e, 0x59},
    {"Kane Six Idols", PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_IDOLS_ON_GATE, dItemNo_RED_RUPEE_e, 0x5A},
};
const Loc kDirect[5] = {
    {"Lenzo assistant", PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BECOME_LENZOS_ASSISTANT, dItemNo_JOY_PENDANT_e, 0x5B},
    {"Lenzo firefly", PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BRING_FOREST_FIREFLY, dItemNo_DELUXE_PICTO_BOX_e, 0x5C},
    {"Maggie free item", PCR_LOC_WINDFALL_ISLAND_MAGGIE_FREE_ITEM, dItemNo_MAGGIES_LETTER_e, dItemNo_HEART_CONTAINER_e},
    {"Maggie delivery reward", PCR_LOC_WINDFALL_ISLAND_MAGGIE_DELIVERY_REWARD, dItemNo_HEART_PIECE_e, dItemNo_GRAPPLING_HOOK_e},
    {"Salvage Corp", PCR_LOC_THE_GREAT_SEA_SALVAGE_CORP_GIFT, dItemNo_COLLECT_MAP_26_e, 0x5D},
};
u8 sPlacement[PCR_LOC_COUNT];

bool sForced = false;

void rando(bool on) {
    if (sForced) {
        return;
    }
    if (on) {
        pc_rando_set_loc_items(sPlacement, PCR_LOC_COUNT);
        pc_rando_set_active(nullptr, nullptr);
    } else {
        pc_rando_deactivate();
    }
}

void check(const char* what, bool on, bool ok) {
    char buf[240];
    snprintf(buf, sizeof buf, "%s: %s", what, on ? "randomizer" : "original game");
    randoSmokeCheck(kName, buf, ok);
}

// --- save records, restored after each check ---
alignas(dSv_info_c) unsigned char sSave[sizeof(dSv_info_c)];
u8 sSelect0 = 0;
void saveRecords() {
    memcpy(sSave, &g_dComIfG_gameInfo.save, sizeof sSave);
    sSelect0 = g_dComIfG_gameInfo.play.getSelectItem(0);
}
void restoreRecords() {
    memcpy(&g_dComIfG_gameInfo.save, sSave, sizeof sSave);
    g_dComIfG_gameInfo.play.setSelectItem(0, sSelect0);
}

const u16 kBits[] = {PC_RANDO_BIT_POMPIE_VERA, PC_RANDO_BIT_MINENCO,   PC_RANDO_BIT_KAMO,
                     PC_RANDO_BIT_LENZO_DELUXE, PC_RANDO_BIT_SALVAGE_CORP, PC_RANDO_BIT_MAGGIE,
                     PC_RANDO_BIT_POSTMAN};

bool noBits() {
    for (u16 b : kBits) {
        if (dComIfGs_isEventBit(b)) {
            return false;
        }
    }
    return true;
}

// --- storage actors ---
template <typename T>
T* storage() {
    alignas(16) static unsigned char buf[sizeof(T)];
    memset(buf, 0, sizeof buf);
    return reinterpret_cast<T*>(buf);
}

// --- T1 ---
void checkItems(bool on) {
    rando(on);
    char what[160];
    for (int i = 0; i < 6; i++) {
        const Loc& l = kPeople[i];
        const int got = pc_rando_people_item(i, l.vanilla);
        snprintf(what, sizeof what, "T1 %s: l_get_item_no[%d] -> 0x%02X (got 0x%02X)", l.name, i,
                 on ? l.placed : l.vanilla, got);
        check(what, on, got == (on ? l.placed : l.vanilla));
    }
    for (int i = 0; i < 4; i++) {
        const Loc& l = kKane[i];
        const int got = pc_rando_kane_item(i, l.vanilla);
        snprintf(what, sizeof what, "T1 %s: l_item_id_sa3[%d] -> 0x%02X (got 0x%02X)", l.name, i,
                 on ? l.placed : l.vanilla, got);
        check(what, on, got == (on ? l.placed : l.vanilla));
    }
    for (const Loc& l : kDirect) {
        const int got = pc_rando_loc_item(l.loc, l.vanilla);
        snprintf(what, sizeof what, "T1 %s -> 0x%02X (got 0x%02X)", l.name, on ? l.placed : l.vanilla, got);
        check(what, on, got == (on ? l.placed : l.vanilla));
    }
    check("T1 an index outside the tables keeps the vanilla item", on,
          pc_rando_people_item(6, 0x77) == 0x77 && pc_rando_people_item(-1, 0x77) == 0x77 &&
              pc_rando_kane_item(4, 0x77) == 0x77);
    if (on) {
        for (int i = 0; i < 6; i++) {
            restoreRecords();
            pc_rando_people_item_given(i);
            const u16 want = pc_rando_people_item_bit(i);
            const bool only = want == 0 ? noBits() : dComIfGs_isEventBit(want);
            snprintf(what, sizeof what, "pc_rando_people_item_given(%d) sets %s", i,
                     want == 0 ? "no bit" : (want == PC_RANDO_BIT_POMPIE_VERA ? "0x6904"
                                             : want == PC_RANDO_BIT_MINENCO ? "0x6908" : "0x6910"));
            check(what, on, only);
        }
        check("bit table: indices 1/2/5 -> 0x6904/0x6908/0x6910", on,
              pc_rando_people_item_bit(1) == 0x6904 && pc_rando_people_item_bit(2) == 0x6908 &&
                  pc_rando_people_item_bit(5) == 0x6910 && pc_rando_people_item_bit(0) == 0 &&
                  pc_rando_people_item_bit(3) == 0 && pc_rando_people_item_bit(4) == 0);
    }
    restoreRecords();
}

// --- probes ---
// One NPC state: its quest done the vanilla way (`item`), the randomizer way (`bit`), or not.
void setDone(void (*vanilla)(), u16 bit, bool item, bool bitOn) {
    restoreRecords();
    if (item) {
        vanilla();
    }
    if (bitOn) {
        dComIfGs_onEventBit(bit);
    }
}

void ownMap20() { dComIfGs_onGetCollectMap(dItemNo_NONE_e - dItemNo_COLLECT_MAP_20_e); }
void ownMap15() { dComIfGs_onGetCollectMap(dItemNo_NONE_e - dItemNo_COLLECT_MAP_15_e); }
void ownDeluxe() { dComIfGs_setItem(dInvSlot_CAMERA_e, dItemNo_DELUXE_PICTO_BOX_e); }
void maggieLetterGiven() { dComIfGs_onGetItemReserve(14); }
void moblinLetterGiven() { dComIfGs_onGetItemReserve(15); }
void ownMap26() { dComIfGs_onGetCollectMap(dItemNo_NONE_e - dItemNo_COLLECT_MAP_26_e); }

struct Probe {
    const char* what;
    void (*vanilla)();
    u16 bit;
    // returns 1 when the NPC is in its done state
    bool (*done)();
};

bool pompieDone() { return storage<daNpcPeople_c>()->is1GetMap20() != FALSE; }

bool kamoDone() {
    daNpcPeople_c* p = storage<daNpcPeople_c>();
    p->*get(PeopleNpcNo()) = kNpcUm3;
    p->*get(PeopleNight()) = 0;
    dComIfGs_setPictureNum(1);
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_2310);
    g_dComIfG_gameInfo.play.setSelectItem(0, dItemNo_PICTO_BOX_e);
    return p->XyCheckCB(0) == 0; // a photo is still accepted only while not done
}

bool lenzoFireflyDone() {
    daNpcPhoto_c* p = storage<daNpcPhoto_c>();
    p->mPhotoGetItem2EventIdx = kLenzoFireflyEvent;
    dComIfGs_setEventReg(kLenzoReg, 6);
    g_dComIfG_gameInfo.play.setSelectItem(0, dItemNo_FIREFLY_BOTTLE_e);
    return p->XyEventCB(0) != kLenzoFireflyEvent;
}

bool lenzoMsgDone() {
    daNpcPhoto_c* p = storage<daNpcPhoto_c>();
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_1601);
    dComIfGs_onTmpBit(dSv_event_tmp_flag_c::UNK_0301);
    dComIfGs_setEventReg(kLenzoReg, 0);
    const u32 msg = p->daNpcPhoto_c::getMsg(); // qualified: getMsg is virtual and storage has no vtable
    if (msg != 0x3786 && msg != 0x2A39) {
        char what[80];
        snprintf(what, sizeof what, "Lenzo getMsg is 0x3786 or 0x2A39 (got 0x%X)", msg);
        randoSmokeCheck(kName, what, false);
    }
    return msg == 0x3786;
}

bool maggieDone() {
    daNpc_Kp1_c* k = storage<daNpc_Kp1_c>();
    k->mItemNo = 0xFF;
    const u32 msg = k->daNpc_Kp1_c::getMsg();
    if (msg != 0x1E87 && msg != 0x1E88 && msg != 0x1E8A) {
        char what[80];
        snprintf(what, sizeof what, "Maggie getMsg is 0x1E87/0x1E88/0x1E8A (got 0x%X)", msg);
        randoSmokeCheck(kName, what, false);
    }
    return msg == 0x1E87 || msg == 0x1E88;
}

bool postmanDone() {
    daNpc_Bm1_c* b = storage<daNpc_Bm1_c>();
    b->*get(Bm1Talking()) = 0;
    const u32 msg = b->getMsg_BMC_2();
    if (msg != 0x1EF9 && msg != 0x1EF0) {
        char what[80];
        snprintf(what, sizeof what, "postman getMsg_BMC_2 is 0x1EF9 or 0x1EF0 (got 0x%X)", msg);
        randoSmokeCheck(kName, what, false);
    }
    return msg == 0x1EF9;
}

// The constructor is all of the check (its base classes only initialise members); the object is
// never created as a process, so it is not destroyed either.
bool salvageCorpDone() {
    alignas(16) static unsigned char buf[sizeof(daNpcSv_c)];
    memset(buf, 0, sizeof buf);
    daNpcSv_c* sv = new (buf) daNpcSv_c();
    return (sv->mTalkFlags & 0x20) != 0;
}

const Probe kProbes[] = {
    {"Pompie and Vera (is1GetMap20)", ownMap20, PC_RANDO_BIT_POMPIE_VERA, pompieDone},
    {"Kamo (XyCheckCB)", ownMap15, PC_RANDO_BIT_KAMO, kamoDone},
    {"Lenzo firefly (XyEventCB)", ownDeluxe, PC_RANDO_BIT_LENZO_DELUXE, lenzoFireflyDone},
    {"Lenzo (getMsg)", ownDeluxe, PC_RANDO_BIT_LENZO_DELUXE, lenzoMsgDone},
    {"Maggie (getMsg)", maggieLetterGiven, PC_RANDO_BIT_MAGGIE, maggieDone},
    {"cafe postman (getMsg_BMC_2)", moblinLetterGiven, PC_RANDO_BIT_POSTMAN, postmanDone},
    {"Salvage Corp (daNpcSv_c::daNpcSv_c)", ownMap26, PC_RANDO_BIT_SALVAGE_CORP, salvageCorpDone},
};

void checkProbes(bool on) {
    rando(on);
    char what[200];
    for (const Probe& p : kProbes) {
        static const struct {
            const char* how;
            bool item, bit;
        } kStates[] = {{"nothing done", false, false}, {"vanilla item owned", true, false},
                       {"event bit set", false, true}};
        for (const auto& s : kStates) {
            setDone(p.vanilla, p.bit, s.item, s.bit);
            const bool want = on ? s.bit : s.item;
            const bool got = p.done();
            snprintf(what, sizeof what, "%s, %s: %s", p.what, s.how, want ? "done" : "not done");
            check(what, on, got == want);
        }
    }
    // Maggie with Moblin's Letter delivered (reserve 15 set, the letter no longer held): the original
    // game says her "quest complete" line; the randomizer never acts as if the quest is complete.
    restoreRecords();
    moblinLetterGiven();
    {
        daNpc_Kp1_c* k = storage<daNpc_Kp1_c>();
        k->mItemNo = 0xFF;
        const u32 msg = k->daNpc_Kp1_c::getMsg();
        snprintf(what, sizeof what, "Maggie, Moblin's Letter delivered: %s (got 0x%X)",
                 on ? "free item offer 0x1E8A" : "quest complete 0x1E83", msg);
        check(what, on, msg == (on ? 0x1E8Au : 0x1E83u));
    }
    restoreRecords();
}

// --- Maggie's event_action, for real ---
struct Spawn {
    int action;
    u8 expect;
    fpc_ProcID id;
    bool done;
};
Spawn sSpawns[2];
bool sSpawnOn = false;
bool sBitAfter[2] = {};

void spawnMaggie(bool on) {
    rando(on);
    restoreRecords();
    sSpawnOn = on;
    fopAc_ac_c* link = dComIfGp_getPlayer(0);
    for (int a = 0; a < 2; a++) {
        daNpc_Kp1_c* k = storage<daNpc_Kp1_c>();
        k->current.pos = link->current.pos;
        k->current.pos.y += 1500.0f; // out of reach
        k->current.pos.x += 100.0f * a;
        k->mEventAction = (s8)a;
        const Loc& l = kDirect[2 + a];
        const bool ok = k->event_action();
        sSpawns[a] = {a, on ? l.placed : l.vanilla, g_dComIfG_gameInfo.play.getEvent()->mPtItem, !ok};
        sBitAfter[a] = dComIfGs_isEventBit(PC_RANDO_BIT_MAGGIE);
        char what[160];
        snprintf(what, sizeof what, "Maggie event_action(%d) creates its item", a);
        check(what, on, ok);
    }
    dComIfGp_event_setItemPartnerId(fpcM_ERROR_PROCESS_ID_e);
    check("Maggie free item sets 0x6A01 only under the randomizer", on, sBitAfter[0] == on && sBitAfter[1] == on);
}

// True when both spawns were read (or failed).
bool pollMaggie() {
    bool all = true;
    for (Spawn& s : sSpawns) {
        if (s.done) {
            continue;
        }
        fopAc_ac_c* ac = nullptr;
        char what[200];
        if (s.id == fpcM_ERROR_PROCESS_ID_e || !fopAcM_SearchByID(s.id, &ac)) {
            snprintf(what, sizeof what, "T1 Maggie event_action(%d): demo item alive", s.action);
            check(what, sSpawnOn, false);
            s.done = true;
            continue;
        }
        if (ac == nullptr) {
            all = false;
            continue;
        }
        const u8 got = static_cast<daItemBase_c*>(ac)->getItemNo();
        snprintf(what, sizeof what, "T1 Maggie event_action(%d): demo item is 0x%02X (got 0x%02X)", s.action,
                 s.expect, got);
        check(what, sSpawnOn, got == s.expect);
        fopAcM_delete(ac);
        s.done = true;
    }
    return all;
}

enum Phase { kStart, kSpawnOff, kPollOff, kSpawnOn, kPollOn, kEnd };
Phase sPhase = kStart;
unsigned int sPhaseFrame = 0;
unsigned int sEndFrame = 0;

} // namespace

void randoSmokeWindfall(unsigned int frames) {
    if (!randoSmokeInPlay(30)) {
        return;
    }
    switch (sPhase) {
    case kStart: {
        sForced = pc_rando_forced() != 0;
        randoSmokeCheck(kName, sForced ? "forced boot: pc_rando() is 1" : "plain boot: pc_rando() is 0",
                        pc_rando() == (sForced ? 1 : 0));
        memset(sPlacement, 0, sizeof sPlacement);
        for (int i = 0; i < PCR_LOC_COUNT; i++) {
            sPlacement[i] = 0x01; // every other location: a green rupee
        }
        for (const Loc& l : kPeople) sPlacement[l.loc] = l.placed;
        for (const Loc& l : kKane) sPlacement[l.loc] = l.placed;
        for (const Loc& l : kDirect) sPlacement[l.loc] = l.placed;
        if (sForced) {
            pc_rando_set_loc_items(sPlacement, PCR_LOC_COUNT);
        }
        saveRecords();
        randoSmokeCheck(kName, "boot save: none of the R2.7 event bits set", noBits());
        for (int pass = sForced ? 1 : 0; pass < 2; pass++) {
            const bool on = pass == 1;
            checkItems(on);
            checkProbes(on);
        }
        sPhase = sForced ? kSpawnOn : kSpawnOff;
        break;
    }
    case kSpawnOff:
    case kSpawnOn:
        spawnMaggie(sPhase == kSpawnOn);
        sPhase = sPhase == kSpawnOff ? kPollOff : kPollOn;
        sPhaseFrame = frames;
        break;
    case kPollOff:
    case kPollOn:
        if (pollMaggie() || frames - sPhaseFrame > 300) {
            if (frames - sPhaseFrame > 300) {
                randoSmokeCheck(kName, "Maggie's demo items created in time", false);
            }
            restoreRecords();
            sPhase = sPhase == kPollOff ? kSpawnOn : kEnd;
            if (sPhase == kEnd) {
                rando(false);
                sEndFrame = frames + 30;
            }
        }
        break;
    case kEnd:
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        break;
    }
}

} // namespace pc
