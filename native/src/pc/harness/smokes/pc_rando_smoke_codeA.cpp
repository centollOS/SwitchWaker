// COS_SMOKE=rando COS_RANDO_SMOKE=codeA (docs/RANDOMIZER_PLAN.md step R2.6): code locations A, the 15
// locations whose item is a constant in an actor's code (docs/RANDO_HOOKS.md "## R2.6"):
//   native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=codeA                 (T1)
//   native/tools/run.sh rando --stage Fairy04:0 --env COS_RANDO_SMOKE=codeA --env COS_CODEA_T2=on
//
// T1, once the play scene has run 10 frames, with the randomizer off (the original game; skipped under
// COS_RANDO_FORCE=1, where it cannot be turned off) and on with a test placement that gives each of
// the 15 locations an item none of them has in the original game (kPlaced):
//   - every hunk's vanilla constant (kSites) is the location's vanilla item in R0.5's key table, and
//     each location has one Code key (fingerprint KEY_FP_ITEM);
//   - pc_rando_loc_item gives the placed item on, the vanilla one off;
//   - direct calls on actors that are only zeroed storage (the functions read nothing else):
//     * daBigelf_c::next_msgStatus(0x2EE8) for types 0-6: on, the placed item (6 and over: the red
//       rupee failsafe); off, the next tier by the wallet/bomb bag/quiver held (both tiers checked);
//     * daBigelf_c::demoProcFlDelete for type 6 (the Big Octo fairy, flag 0x1 set, mStaffId -1, past
//       its particle frame): off, the max and current magic counts get 32; on, nothing;
//     * daNpc_Ho_c::next_msgStatus for messages 0x271E, 0x275C, 0x2743 and 0x2751: mItemNum;
//     * daNpc_Ji1_c::createItem in its Piece of Heart branch: mCreateItemNo and the demo item;
//     * daNpc_Tc_c::cutPresentProc without an event (no "ItemType": type 0, Tingle's first gift):
//       the demo item (dComIfGp_event_getGtItm). The created demo items are deleted afterwards;
//   - item_func_max_mp_up1: off, the max magic count gets 32; on, max and current are both 32.
//   The save, the event bits and the play counts these touch are restored after each call.
// Not called: ItemType 1/2 of cutPresentProc (Tingle's second gift, Ankle) and Ivan's demoProc case
// 13 need a running event's staff data; their sites are covered by the key-table check, the census
// and RANDO_HOOKS.md.
//
// T2 (COS_CODEA_T2=off|on, on stage Fairy04, the Outset Great Fairy, type 1): the fairy's own arrival
// event (BIGELF_ARRIVAL, ordered when the player is within 900 units) is driven with A every 20
// frames until it ends. off: the original game gives the 1000 Rupee Wallet on a file without a
// wallet; on (placement: the Outset fairy gives a Piece of Heart): the demo item is the Piece of Heart,
// max life grows by one quarter heart and the wallet is unchanged. Both: her event bit (0x3010) is set.
//
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_lockeys.h"

#include "d/actor/d_a_bigelf.h"
#include "d/actor/d_a_npc_ho.h"
#include "d/actor/d_a_npc_ji1.h"
#include "d/actor/d_a_npc_tc.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "codeA";

template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
#define CODEA_ROB(TAG, CLASS, TYPE, MEMBER)                                                           \
    struct TAG {                                                                                     \
        using type = TYPE CLASS::*;                                                                  \
        friend type get(TAG);                                                                        \
    };                                                                                               \
    template struct Rob<TAG, &CLASS::MEMBER>;
CODEA_ROB(HoItemNum, daNpc_Ho_c, u8, mItemNum)
#undef CODEA_ROB

// The hunks' vanilla constants (game/src/d/actor/*), one per location.
struct Site {
    int loc;
    u8 vanilla;
    u8 placed; // the test placement: an item none of the 15 has in the original game
    const char* what;
};
const Site kSites[] = {
    {PCR_LOC_NORTHERN_FAIRY_ISLAND_GREAT_FAIRY, dItemNo_MAX_RUPEE_UP1_e, dItemNo_BOMB_BAG_e, "Northern GF"},
    {PCR_LOC_OUTSET_ISLAND_GREAT_FAIRY, dItemNo_MAX_RUPEE_UP2_e, dItemNo_BOW_e, "Outset GF"},
    {PCR_LOC_EASTERN_FAIRY_ISLAND_GREAT_FAIRY, dItemNo_MAX_BOMB_UP1_e, dItemNo_HOOKSHOT_e, "Eastern GF"},
    {PCR_LOC_SOUTHERN_FAIRY_ISLAND_GREAT_FAIRY, dItemNo_MAX_BOMB_UP2_e, dItemNo_BOOMERANG_e, "Southern GF"},
    {PCR_LOC_WESTERN_FAIRY_ISLAND_GREAT_FAIRY, dItemNo_MAX_ARROW_UP1_e, dItemNo_GRAPPLING_HOOK_e, "Western GF"},
    {PCR_LOC_THORNED_FAIRY_ISLAND_GREAT_FAIRY, dItemNo_MAX_ARROW_UP2_e, dItemNo_SKULL_HAMMER_e, "Thorned GF"},
    {PCR_LOC_OUTSET_ISLAND_ORCA_HIT_500_TIMES, dItemNo_HEART_PIECE_e, dItemNo_HEART_CONTAINER_e, "Orca 500 hits"},
    {PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_FIRST_GIFT, dItemNo_TINGLE_TUNER_e, dItemNo_DEKU_LEAF_e, "Tingle 1st"},
    {PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_SECOND_GIFT, dItemNo_COLLECT_MAP_35_e, dItemNo_IRON_BOOTS_e, "Tingle 2nd"},
    {PCR_LOC_TINGLE_ISLAND_ANKLE_REWARD_FOR_ALL_TINGLE_STATUES, dItemNo_TINGLE_RUPEE_6_e, dItemNo_POWER_BRACELETS_e, "Ankle"},
    {PCR_LOC_WINDFALL_ISLAND_IVAN_CATCH_KILLER_BEES, dItemNo_HEART_PIECE_e, dItemNo_MIRROR_SHIELD_e, "Ivan"},
    {PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_CATCH_KILLER_BEES, dItemNo_PURPLE_RUPEE_e, dItemNo_GREEN_RUPEE_e, "Marie bees"},
    {PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_1_JOY_PENDANT, dItemNo_RED_RUPEE_e, dItemNo_BLUE_RUPEE_e, "Marie 1"},
    {PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_21_JOY_PENDANTS, dItemNo_CABANA_DEED_e, dItemNo_ORANGE_RUPEE_e, "Marie 21"},
    {PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_40_JOY_PENDANTS, dItemNo_HEROS_CHARM_e, dItemNo_SILVER_RUPEE_e, "Marie 40"},
};
constexpr int kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

const Site& site(int loc) {
    for (const Site& s : kSites) {
        if (s.loc == loc) {
            return s;
        }
    }
    abort();
}

bool sForced = false;
uint8_t sItems[PCR_LOC_COUNT];

void check(const char* what, bool on, bool ok) {
    char buf[200];
    snprintf(buf, sizeof buf, "%s: %s", what, on ? "randomizer" : "original game");
    randoSmokeCheck(kName, buf, ok);
}

// The placement: every location its vanilla item (R0.5's table), the 15 their kPlaced item;
// `outsetHeart` gives the Outset Great Fairy a Piece of Heart instead (T2).
void setPlacement(bool outsetHeart) {
    for (int loc = 0; loc < PCR_LOC_COUNT; loc++) {
        pcr::Span<pcr::LocKey> keys = pcr::location_keys(loc);
        sItems[loc] = keys.size != 0 ? keys[0].vanilla : (uint8_t)dItemNo_NONE_e;
    }
    for (const Site& s : kSites) {
        sItems[s.loc] = s.placed;
    }
    if (outsetHeart) {
        sItems[PCR_LOC_OUTSET_ISLAND_GREAT_FAIRY] = dItemNo_HEART_PIECE_e;
    }
}

void rando(bool on, bool outsetHeart = false) {
    if (sForced) {
        return;
    }
    pc_rando_deactivate();
    if (on) {
        setPlacement(outsetHeart);
        pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
        pc_rando_set_active(nullptr, nullptr);
    }
}

u8 expect(int loc, bool on) {
    // Forced (COS_RANDO_FORCE=1): on, but no placement, so every location gives its vanilla item.
    const Site& s = site(loc);
    return on && !sForced ? s.placed : s.vanilla;
}

// What the calls change, restored after each one.
dSv_info_c sSave;
void saveState() {
    sSave = g_dComIfG_gameInfo.save;
}
void restoreState() {
    g_dComIfG_gameInfo.save = sSave;
    g_dComIfG_gameInfo.play.mItemMagicCount = 0;
    g_dComIfG_gameInfo.play.mItemMaxMagicCount = 0;
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    dComIfGp_event_setItemPartnerId(fpcM_ERROR_PROCESS_ID_e);
}

alignas(daBigelf_c) unsigned char sElf[sizeof(daBigelf_c)];
alignas(daNpc_Ho_c) unsigned char sHo[sizeof(daNpc_Ho_c)];
alignas(daNpc_Ji1_c) unsigned char sJi1[sizeof(daNpc_Ji1_c)];
alignas(daNpc_Tc_c) unsigned char sTc[sizeof(daNpc_Tc_c)];

template <typename T>
T* storage(unsigned char* buf, size_t size) {
    memset(buf, 0, size);
    return reinterpret_cast<T*>(buf);
}
daBigelf_c* elf(u32 type) {
    daBigelf_c* e = storage<daBigelf_c>(sElf, sizeof sElf);
    fopAcM_SetParam(e, type);
    e->mStaffId = -1;
    return e;
}

fpc_ProcID sDemoItems[8];
int sDemoItemCount = 0;
void keepDemoItem() {
    const fpc_ProcID id = g_dComIfG_gameInfo.play.getEvent()->mPtItem;
    if (id != fpcM_ERROR_PROCESS_ID_e && sDemoItemCount < 8) {
        sDemoItems[sDemoItemCount++] = id;
    }
}

void checkKeys() {
    bool ok = true;
    for (const Site& s : kSites) {
        pcr::Span<pcr::LocKey> keys = pcr::location_keys(s.loc);
        const bool good = keys.size == 1 && keys[0].kind == pcr::KeyKind::Code && keys[0].vanilla == s.vanilla &&
                          (keys[0].flags & pcr::KEY_FP_ITEM) != 0 && keys[0].disc_byte == s.vanilla;
        if (!good) {
            fprintf(stderr, "[cos] rando-smoke codeA: key of %s: %d key(s), vanilla 0x%02X, hunk 0x%02X\n", s.what,
                    (int)keys.size, keys.size ? keys[0].vanilla : 0, s.vanilla);
        }
        ok = ok && good;
    }
    randoSmokeCheck(kName, "the 15 hunks' vanilla items are R0.5's Code keys (disc bytes)", ok);
}

void checkLookup(bool on) {
    bool ok = true;
    for (const Site& s : kSites) {
        ok = ok && pc_rando_loc_item(s.loc, s.vanilla) == expect(s.loc, on);
    }
    check("pc_rando_loc_item for the 15", on, ok);
}

void checkBigelf(bool on) {
    static const int kLoc[6] = {
        PCR_LOC_NORTHERN_FAIRY_ISLAND_GREAT_FAIRY, PCR_LOC_OUTSET_ISLAND_GREAT_FAIRY,
        PCR_LOC_EASTERN_FAIRY_ISLAND_GREAT_FAIRY,  PCR_LOC_SOUTHERN_FAIRY_ISLAND_GREAT_FAIRY,
        PCR_LOC_WESTERN_FAIRY_ISLAND_GREAT_FAIRY,  PCR_LOC_THORNED_FAIRY_ISLAND_GREAT_FAIRY,
    };
    // Lower tiers held (wallet 0, 30 bombs, 30 arrows) and upper tiers held (wallet 1, 60, 60).
    for (int upper = 0; upper < 2; upper++) {
        for (u32 type = 0; type <= 7; type++) {
            saveState();
            dComIfGs_setWalletSize(upper ? 1 : 0);
            dComIfGs_setBombMax(upper ? 60 : 30);
            dComIfGs_setArrowMax(upper ? 60 : 30);
            daBigelf_c* e = elf(type);
            u32 msg = 0x2EE8;
            e->next_msgStatus(&msg);
            u8 want;
            if (type >= 6) {
                want = dItemNo_RED_RUPEE_e;
            } else if (on) {
                want = expect(kLoc[type], true);
            } else {
                static const u8 kTier[3][2] = {
                    {dItemNo_MAX_RUPEE_UP1_e, dItemNo_MAX_RUPEE_UP2_e},
                    {dItemNo_MAX_BOMB_UP1_e, dItemNo_MAX_BOMB_UP2_e},
                    {dItemNo_MAX_ARROW_UP1_e, dItemNo_MAX_ARROW_UP2_e},
                };
                want = kTier[type / 2][upper];
            }
            char what[160];
            snprintf(what, sizeof what, "daBigelf_c::next_msgStatus(0x2EE8) type %u, %s tiers held -> 0x%02X", type,
                     upper ? "upper" : "lower", want);
            const bool ok = e->mGivenItem == want && dComIfGp_event_getGtItm() == want && msg == 0x2EE9;
            if (!ok) {
                fprintf(stderr, "[cos] rando-smoke codeA: got 0x%02X gt 0x%02X msg 0x%X\n", e->mGivenItem,
                        dComIfGp_event_getGtItm(), msg);
            }
            check(what, on, ok);
            restoreState();
        }
    }

    // The Big Octo fairy's magic, given at the end of her FL_DELETE cut.
    for (u32 type = 5; type <= 6; type++) {
        saveState();
        g_dComIfG_gameInfo.play.mItemMagicCount = 0;
        g_dComIfG_gameInfo.play.mItemMaxMagicCount = 0;
        daBigelf_c* e = elf(type);
        e->mTimer = 0x1B; // incremented to 0x1C: past the particle frame
        e->setFlag(0x1);
        e->demoProcFlDelete();
        const s16 add = (type == 6 && !on) ? 0x20 : 0;
        char what[160];
        snprintf(what, sizeof what, "daBigelf_c::demoProcFlDelete type %u: magic counts +0x%X", type, add);
        check(what, on,
              dComIfGp_getItemMaxMagicCount() == add && dComIfGp_getItemMagicCount() == add && !e->chkFlag(0x1));
        restoreState();
    }
}

void checkHo(bool on) {
    static const struct {
        u32 msg;
        int loc;
    } kMsgs[] = {
        {0x271E, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_CATCH_KILLER_BEES},
        {0x275C, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_1_JOY_PENDANT},
        {0x2743, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_21_JOY_PENDANTS},
        {0x2751, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_40_JOY_PENDANTS},
    };
    for (const auto& m : kMsgs) {
        saveState();
        daNpc_Ho_c* h = storage<daNpc_Ho_c>(sHo, sizeof sHo);
        u32 msg = m.msg;
        h->next_msgStatus(&msg);
        const u8 want = expect(m.loc, on);
        char what[160];
        snprintf(what, sizeof what, "daNpc_Ho_c::next_msgStatus(0x%X) -> mItemNum 0x%02X", m.msg, want);
        check(what, on, h->*get(HoItemNum()) == want);
        restoreState();
    }
}

void checkJi1(bool on) {
    saveState();
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_0F10);
    daNpc_Ji1_c* j = storage<daNpc_Ji1_c>(sJi1, sizeof sJi1);
    j->current.pos = dComIfGp_getLinkPlayer()->current.pos;
    j->current.roomNo = dComIfGp_getLinkPlayer()->current.roomNo;
    j->createItem();
    keepDemoItem();
    const u8 want = expect(PCR_LOC_OUTSET_ISLAND_ORCA_HIT_500_TIMES, on);
    char what[160];
    snprintf(what, sizeof what, "daNpc_Ji1_c::createItem (500 hits) -> 0x%02X", want);
    check(what, on, j->mCreateItemNo == want && dComIfGp_event_getGtItm() == want);
    restoreState();
}

void checkTc(bool on) {
    saveState();
    daNpc_Tc_c* t = storage<daNpc_Tc_c>(sTc, sizeof sTc);
    t->current.pos = dComIfGp_getLinkPlayer()->current.pos;
    t->current.roomNo = dComIfGp_getLinkPlayer()->current.roomNo;
    t->cutPresentProc(-1);
    keepDemoItem();
    const u8 want = expect(PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_FIRST_GIFT, on);
    char what[160];
    snprintf(what, sizeof what, "daNpc_Tc_c::cutPresentProc (ItemType 0) -> 0x%02X", want);
    check(what, on, dComIfGp_event_getGtItm() == want);
    restoreState();
}

void checkMagicUp(bool on) {
    saveState();
    g_dComIfG_gameInfo.play.mItemMagicCount = 5;
    g_dComIfG_gameInfo.play.mItemMaxMagicCount = 3;
    item_func_max_mp_up1();
    // Original: max += 32 (3 + 32), current untouched; randomizer: both set to 32.
    const bool ok = on ? (dComIfGp_getItemMaxMagicCount() == 32 && dComIfGp_getItemMagicCount() == 32)
                       : (dComIfGp_getItemMaxMagicCount() == 35 && dComIfGp_getItemMagicCount() == 5);
    check("item_func_max_mp_up1", on, ok);
    restoreState();
}

void runT1(bool on) {
    rando(on);
    checkLookup(on);
    checkBigelf(on);
    checkHo(on);
    checkJi1(on);
    checkTc(on);
    checkMagicUp(on);
}

// --- T2: the Outset Great Fairy's arrival event ---
enum State { kT1, kCleanup, kT2Start, kT2Event, kDone };
State sState = kT1;
unsigned sSince = 0;
int sT2 = 0; // 0 none, 1 off, 2 on
u16 sLife0 = 0;
u8 sWallet0 = 0;
u8 sGtSeen = dItemNo_NONE_e;
bool sEventSeen = false;
uint64_t sStartNs = 0;

constexpr unsigned kTalkFrames = 20;
constexpr unsigned kStartFrames = 600;
constexpr double kEventSeconds = 240.0;

fopAc_ac_c* findElf() {
    return fopAcM_SearchByName(fpcNm_BIGELF_e);
}

} // namespace

void randoSmokeCodeA(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(10)) {
        return;
    }
    switch (sState) {
    case kT1: {
        sForced = pc_rando_forced() != 0;
        checkKeys();
        if (!sForced) {
            runT1(false);
        }
        runT1(true);
        sState = kCleanup;
        sSince = 0;
        return;
    }
    case kCleanup: {
        if (++sSince < 5) {
            return;
        }
        for (int i = 0; i < sDemoItemCount; i++) {
            if (fopAc_ac_c* a = fopAcM_SearchByID(sDemoItems[i])) {
                fopAcM_delete(a);
            }
        }
        dComIfGp_event_setItemPartnerId(fpcM_ERROR_PROCESS_ID_e);
        const char* t2 = getenv("COS_CODEA_T2");
        sT2 = t2 == nullptr ? 0 : strcmp(t2, "on") == 0 ? 2 : strcmp(t2, "off") == 0 ? 1 : 0;
        if (sT2 == 1 && sForced) {
            randoSmokeCheck(kName, "COS_CODEA_T2=off needs the randomizer off (no COS_RANDO_FORCE)", false);
            sT2 = 0;
        }
        if (sT2 == 0) {
            if (!sForced) {
                rando(false);
            }
            randoSmokeEnd(kName);
        }
        if (sForced) {
            // Forced: no placement, the Outset fairy gives her vanilla item (5000 Rupee Wallet).
        } else {
            rando(sT2 == 2, true);
        }
        sState = kT2Start;
        sSince = 0;
        return;
    }
    case kT2Start: {
        sSince++;
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        fopAc_ac_c* e = findElf();
        if (e == nullptr) {
            if (sSince > 60) {
                randoSmokeCheck(kName, "T2: a Great Fairy in the stage (boot Fairy04)", false);
                randoSmokeEnd(kName);
            }
            return;
        }
        if (sSince == 1) {
            fprintf(stderr, "[cos] rando-smoke codeA: T2 fairy type %u at (%.0f,%.0f,%.0f), player (%.0f,%.0f,%.0f)\n",
                    fopAcM_GetParam(e) & 0xFF, e->current.pos.x, e->current.pos.y, e->current.pos.z,
                    player->current.pos.x, player->current.pos.y, player->current.pos.z);
        }
        if (dComIfGp_event_runCheck()) {
            if (dComIfGp_evmng_startCheck("BIGELF_ARRIVAL")) {
                sLife0 = dComIfGs_getMaxLife();
                sWallet0 = dComIfGs_getWalletSize();
                sState = kT2Event;
                sSince = 0;
                sStartNs = monotonicNs();
            } else {
                setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0); // another event
            }
            return;
        }
        // Her hunt orders the event once the player is within 900 units (XZ): walk at her (stick up;
        // the camera looks along the fountain's axis from the entrance).
        setDrivenPad(true, 0, 0, 100);
        if (sSince % 30 == 0) {
            fprintf(stderr, "[cos] rando-smoke codeA: T2 player (%.0f,%.0f,%.0f)\n", player->current.pos.x,
                    player->current.pos.y, player->current.pos.z);
        }
        if (sSince > kStartFrames) {
            randoSmokeCheck(kName, "T2: the fairy's arrival event starts", false);
            randoSmokeEnd(kName);
        }
        return;
    }
    case kT2Event: {
        sSince++;
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (dComIfGp_event_getGtItm() != dItemNo_NONE_e) {
            sGtSeen = dComIfGp_event_getGtItm();
        }
        if (!dComIfGp_event_runCheck()) {
            setDrivenPad(true, 0, 0, 0);
            const bool on = sT2 == 2;
            if (sForced) {
                check("T2 Outset GF (forced, no placement): 5000 Rupee Wallet whatever wallet is held", true,
                      sGtSeen == dItemNo_MAX_RUPEE_UP2_e && dComIfGs_getWalletSize() == 2);
            } else if (on) {
                check("T2 Outset GF placed Piece of Heart: demo item, max life +1, wallet unchanged", on,
                      sGtSeen == dItemNo_HEART_PIECE_e && dComIfGs_getMaxLife() == sLife0 + 1 &&
                          dComIfGs_getWalletSize() == sWallet0);
            } else {
                check("T2 Outset GF: 1000 Rupee Wallet on a file without a wallet", on,
                      sWallet0 == 0 && sGtSeen == dItemNo_MAX_RUPEE_UP1_e && dComIfGs_getWalletSize() == 1);
            }
            check("T2 her event bit 0x3010 set", on || sForced, dComIfGs_isEventBit(dSv_event_flag_c::UNK_3010));
            fprintf(stderr, "[cos] rando-smoke codeA: T2 gt 0x%02X life %u->%u wallet %u->%u, %u frames\n", sGtSeen,
                    sLife0, dComIfGs_getMaxLife(), sWallet0, dComIfGs_getWalletSize(), sSince);
            if (!sForced) {
                rando(false);
            }
            randoSmokeEnd(kName);
        }
        if ((monotonicNs() - sStartNs) / 1e9 > kEventSeconds) {
            randoSmokeCheck(kName, "T2: the fairy's event ends", false);
            eventWatchReport(frames);
            randoSmokeEnd(kName);
        }
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc
