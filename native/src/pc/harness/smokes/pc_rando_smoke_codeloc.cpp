// COS_SMOKE=rando COS_RANDO_SMOKE=codeloc (docs/RANDOMIZER_PLAN.md step R9.1): the location sweep's
// T1 of the code locations (the locations whose item a game function picks: R0.5's Code keys), under
// the seed the run has loaded, e.g.
//   run.sh rando --stage sea:11 --env COS_RANDO_SMOKE=codeloc \
//                --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_rotation.json
// native/tools/rando/location_sweep.py runs it for the "code" kind and parses its per-location lines
//   [cos] rando-smoke codeloc: T1 <PCR_LOC_*> <site>: gives 0x.. (want 0x..): ok|FAIL
// The steps' own smokes (codeA, windfall, codec, shop, bossitem, addchest) check the same sites with
// a synthetic placement, each location a different non-vanilla item, plus the randomizer-off arm; this
// one checks them with whatever seed is loaded (identity, rotation, a ww_export seed), which is what
// the sweep needs. "want" is the item the seed places there: pc_rando_loc_item(loc), or the vanilla
// item when pc_rando() is 0 (no placement). Once the play scene has run 10 frames:
//   - every Code key: pc_rando_loc_item(loc, the key's vanilla item) gives the placed item (and the
//     seed has an item there);
//   - the site lookups the hunks call: pc_rando_code_c_item for every code-C row (auction,
//     Battlesquid, Barrel Shooting, Bird-Man, Goron trader, mailbox, Withered Trees, Zunari, Doc
//     Bandam), pc_rando_people_item (the townspeople), pc_rando_kane_item (Kane's gate),
//     pc_rando_boss_item_for_stage (the 6 Heart Containers), pc_rando_added_chests_demo_item (the
//     Master Sword, Jabun and Mother Isle cutscene items), and the 4 Beedle shop entries' mItemNo
//     (d_shop.cpp shopItemData_*, R2.9's R0.8 edits; the placed item or its progressive tier);
//   - direct calls on actors that are only zeroed storage, as the codeA smoke makes them:
//     daBigelf_c::next_msgStatus(0x2EE8) for the 6 Great Fairies (types 0-5; with the randomizer off
//     the lower tier of the wallet / bomb bag / quiver the file holds), daNpc_Ho_c::next_msgStatus for
//     Mrs. Marie's 4 messages (mItemNum), daNpc_Ji1_c::createItem in its Piece of Heart branch (Orca,
//     500 hits) and its Hurricane Spin branch (Orca, 10 Knight's Crests), daNpc_Tc_c::cutPresentProc
//     for Tingle's first gift (the demo item). The demo items created are deleted 5 frames later.
// Locations whose hunk only calls pc_rando_loc_item with its own constant (Lenzo, Maggie, Salvage
// Corp, Ivan, Tingle's second gift and Ankle, Medli, the Forest Haven Deku Leaf, the shop slots'
// next_msgStatus / createShopList) are covered by the lookup row; their constants are checked by the
// census and the steps' smokes. Their T2 (playing the NPC's quest or minigame) is not part of the
// sweep: sweep_targets.json lists the code kind without T2.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_boss_items.h"
#include "features/rando/pc_rando_code_c.h"
#include "features/rando/pc_rando_lockeys.h"

#include "d/actor/d_a_bigelf.h"
#include "d/actor/d_a_npc_ho.h"
#include "d/actor/d_a_npc_ji1.h"
#include "d/actor/d_a_npc_tc.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_shop.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <cstdio>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "codeloc";
constexpr u8 kNoItem = 0xFE; // pc_rando_loc_item's answer for "no item at this location"

template <typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
#define CODELOC_ROB(TAG, CLASS, TYPE, MEMBER)                                                         \
    struct TAG {                                                                                     \
        using type = TYPE CLASS::*;                                                                  \
        friend type get(TAG);                                                                        \
    };                                                                                               \
    template struct Rob<TAG, &CLASS::MEMBER>;
CODELOC_ROB(CodelocHoItemNum, daNpc_Ho_c, u8, mItemNum)
#undef CODELOC_ROB

const char* locName(int loc) {
    const pcr::LocInfo* info = pcr::location_info(loc);
    return info != nullptr ? info->enum_name : "?";
}

// The item the loaded seed puts at `loc` (vanilla without the randomizer).
u8 want(int loc, u8 vanilla) {
    return pc_rando() ? pc_rando_loc_item(loc, kNoItem) : vanilla;
}

// One per-location line for the sweep tool. `alsoTier`: the progressive tier of the placed item is
// accepted too (sites that hand the item to code that converts it).
bool row(int loc, const char* site, int got, u8 wanted, bool alsoTier = false) {
    bool ok = got == wanted && wanted != kNoItem;
    if (!ok && alsoTier && pc_rando()) {
        ok = got == pc_rando_convert_progressive_item_id(wanted);
    }
    char what[200];
    snprintf(what, sizeof what, "T1 %s %s: gives 0x%02X (want 0x%02X)", locName(loc), site, got & 0xFF, wanted);
    return randoSmokeCheck(kName, what, ok);
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

// What the direct calls change, restored after each one.
dSv_info_c sSave;
void saveState() {
    sSave = g_dComIfG_gameInfo.save;
}
void restoreState() {
    g_dComIfG_gameInfo.save = sSave;
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    dComIfGp_event_setItemPartnerId(fpcM_ERROR_PROCESS_ID_e);
}

fpc_ProcID sDemoItems[8];
int sDemoItemCount = 0;
void keepDemoItem() {
    const fpc_ProcID id = g_dComIfG_gameInfo.play.getEvent()->mPtItem;
    if (id != fpcM_ERROR_PROCESS_ID_e && sDemoItemCount < 8) {
        sDemoItems[sDemoItemCount++] = id;
    }
}

template <typename T>
T* storage(unsigned char* buf, size_t size) {
    memset(buf, 0, size);
    return reinterpret_cast<T*>(buf);
}
alignas(daBigelf_c) unsigned char sElf[sizeof(daBigelf_c)];
alignas(daNpc_Ho_c) unsigned char sHo[sizeof(daNpc_Ho_c)];
alignas(daNpc_Ji1_c) unsigned char sJi1[sizeof(daNpc_Ji1_c)];
alignas(daNpc_Tc_c) unsigned char sTc[sizeof(daNpc_Tc_c)];

int sChecked[PCR_LOC_COUNT];

void checkLookups() {
    pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    for (size_t i = 0; i < keys.size; i++) {
        const pcr::LocKey& k = keys[i];
        if (k.kind != pcr::KeyKind::Code) {
            continue;
        }
        char site[96];
        snprintf(site, sizeof site, "lookup (key %zu)", i);
        row(k.loc, site, pc_rando_loc_item(k.loc, k.vanilla), want(k.loc, k.vanilla));
        sChecked[k.loc]++;
    }
}

void checkCodeC() {
    for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
        char site[96];
        snprintf(site, sizeof site, "pc_rando_code_c_item %s[%d]", pcr::code_c_site_name(s.site), s.index);
        row(s.loc, site, pc_rando_code_c_item(s.site, s.index, s.vanilla), want(s.loc, s.vanilla));
    }
}

void checkTables() {
    // d_a_npc_people l_get_item_no and l_item_id_sa3 (pc_rando_hooks_windfall.cpp's order).
    static const int kPeople[] = {
        PCR_LOC_WINDFALL_ISLAND_SAM_DECORATE_THE_TOWN,    PCR_LOC_WINDFALL_ISLAND_POMPIE_AND_VERA_SECRET_MEETING_PHOTO,
        PCR_LOC_WINDFALL_ISLAND_MINENCO_MISS_WINDFALL_PHOTO, PCR_LOC_WINDFALL_ISLAND_LINDA_AND_ANTON,
        PCR_LOC_WINDFALL_ISLAND_KREEB_LIGHT_UP_LIGHTHOUSE, PCR_LOC_WINDFALL_ISLAND_KAMO_FULL_MOON_PHOTO,
    };
    static const int kKane[] = {
        PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SHOP_GURU_STATUE_ON_GATE,
        PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_POSTMAN_STATUE_ON_GATE,
        PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_FLAGS_ON_GATE,
        PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_IDOLS_ON_GATE,
    };
    char site[96];
    for (int i = 0; i < (int)(sizeof kPeople / sizeof kPeople[0]); i++) {
        const u8 v = pcr::location_keys(kPeople[i])[0].vanilla;
        snprintf(site, sizeof site, "pc_rando_people_item(%d)", i);
        row(kPeople[i], site, pc_rando() ? pc_rando_people_item(i, v) : v, want(kPeople[i], v));
    }
    for (int i = 0; i < (int)(sizeof kKane / sizeof kKane[0]); i++) {
        const u8 v = pcr::location_keys(kKane[i])[0].vanilla;
        snprintf(site, sizeof site, "pc_rando_kane_item(%d)", i);
        row(kKane[i], site, pc_rando() ? pc_rando_kane_item(i, v) : v, want(kKane[i], v));
    }
    // The 6 boss Heart Containers, keyed by the dungeon's stage number.
    for (int stage = 2; stage <= 7; stage++) {
        const int loc = pc_rando_boss_loc(stage);
        snprintf(site, sizeof site, "pc_rando_boss_item_for_stage(%d)", stage);
        row(loc, site, pc_rando_boss_item_for_stage(stage), want(loc, PC_RANDO_BOSS_VANILLA_ITEM));
    }
    // d_a_demo00 l_itemNo[0, 5, 8]: the cutscene items of three added-chest locations.
    static const struct {
        int argID;
        int loc;
    } kDemo[] = {
        {0, PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER},
        {5, PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE},
        {8, PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE},
    };
    for (const auto& d : kDemo) {
        const u8 v = pcr::location_keys(d.loc)[0].vanilla;
        snprintf(site, sizeof site, "pc_rando_added_chests_demo_item(%d)", d.argID);
        row(d.loc, site, pc_rando_added_chests_demo_item(d.argID, v), want(d.loc, v));
    }
    // Beedle's 4 shop entries (R2.9: the placed item written into mItemNo at activation).
    const ShopItems_c__ItemData* const kShop[PCR_SHOP_SLOT_COUNT] = {
        &shopItemData_Feedbag, &shopItemData_emptybottle, &shopItemData_kakera_heart, &shopItemData_map};
    for (int slot = 0; slot < PCR_SHOP_SLOT_COUNT; slot++) {
        const int loc = pc_rando_shop_slot_loc(slot);
        snprintf(site, sizeof site, "shop item data slot %d mItemNo", slot);
        row(loc, site, kShop[slot]->mItemNo, want(loc, pc_rando_shop_slot_vanilla(slot)), true);
    }
}

void checkBigelf() {
    static const int kLoc[6] = {
        PCR_LOC_NORTHERN_FAIRY_ISLAND_GREAT_FAIRY, PCR_LOC_OUTSET_ISLAND_GREAT_FAIRY,
        PCR_LOC_EASTERN_FAIRY_ISLAND_GREAT_FAIRY,  PCR_LOC_SOUTHERN_FAIRY_ISLAND_GREAT_FAIRY,
        PCR_LOC_WESTERN_FAIRY_ISLAND_GREAT_FAIRY,  PCR_LOC_THORNED_FAIRY_ISLAND_GREAT_FAIRY,
    };
    // The lower tier the original game gives with wallet 0, 30 bombs, 30 arrows held.
    static const u8 kLowerTier[3] = {dItemNo_MAX_RUPEE_UP1_e, dItemNo_MAX_BOMB_UP1_e, dItemNo_MAX_ARROW_UP1_e};
    for (u32 type = 0; type < 6; type++) {
        saveState();
        dComIfGs_setWalletSize(0);
        dComIfGs_setBombMax(30);
        dComIfGs_setArrowMax(30);
        daBigelf_c* e = storage<daBigelf_c>(sElf, sizeof sElf);
        fopAcM_SetParam(e, type);
        e->mStaffId = -1;
        u32 msg = 0x2EE8;
        e->next_msgStatus(&msg);
        const u8 w = pc_rando() ? want(kLoc[type], 0) : kLowerTier[type / 2];
        char site[96];
        snprintf(site, sizeof site, "daBigelf_c::next_msgStatus type %u", type);
        const int got = e->mGivenItem == dComIfGp_event_getGtItm() && msg == 0x2EE9 ? e->mGivenItem : -1;
        if (got < 0) {
            fprintf(stderr, "[cos] rando-smoke codeloc: fairy type %u: given 0x%02X, gt 0x%02X, msg 0x%X\n", type,
                    e->mGivenItem, dComIfGp_event_getGtItm(), msg);
        }
        row(kLoc[type], site, got, w);
        restoreState();
    }
}

void checkHo() {
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
        char site[96];
        snprintf(site, sizeof site, "daNpc_Ho_c::next_msgStatus(0x%X)", m.msg);
        row(m.loc, site, h->*get(CodelocHoItemNum()), want(m.loc, pcr::location_keys(m.loc)[0].vanilla));
        restoreState();
    }
}

void checkJi1(bool hurricane) {
    saveState();
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_0F10);
    daNpc_Ji1_c* j = storage<daNpc_Ji1_c>(sJi1, sizeof sJi1);
    j->current.pos = dComIfGp_getLinkPlayer()->current.pos;
    j->current.roomNo = dComIfGp_getLinkPlayer()->current.roomNo;
    if (hurricane) {
        j->field_0xD7B = 1; // the Hurricane Spin branch (field_0xD7C 0: not the sword)
    }
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    {
        PlayLayerScope layer;
        j->createItem();
    }
    keepDemoItem();
    const int loc = hurricane ? PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS : PCR_LOC_OUTSET_ISLAND_ORCA_HIT_500_TIMES;
    const u8 v = hurricane ? (u8)dItemNo_HURRICANE_SPIN_e : (u8)dItemNo_HEART_PIECE_e;
    const int got = j->mCreateItemNo == dComIfGp_event_getGtItm() ? j->mCreateItemNo : -1;
    if (got < 0) {
        fprintf(stderr, "[cos] rando-smoke codeloc: Ji1 createItem: mCreateItemNo 0x%02X, gt 0x%02X\n", j->mCreateItemNo,
                dComIfGp_event_getGtItm());
    }
    row(loc, hurricane ? "daNpc_Ji1_c::createItem (Hurricane Spin branch)" : "daNpc_Ji1_c::createItem (500 hits)", got,
        want(loc, v));
    restoreState();
}

void checkTc() {
    saveState();
    daNpc_Tc_c* t = storage<daNpc_Tc_c>(sTc, sizeof sTc);
    t->current.pos = dComIfGp_getLinkPlayer()->current.pos;
    t->current.roomNo = dComIfGp_getLinkPlayer()->current.roomNo;
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    {
        PlayLayerScope layer;
        t->cutPresentProc(-1);
    }
    keepDemoItem();
    const int loc = PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_FIRST_GIFT;
    row(loc, "daNpc_Tc_c::cutPresentProc (ItemType 0)", dComIfGp_event_getGtItm(),
        want(loc, pcr::location_keys(loc)[0].vanilla));
    restoreState();
}

enum State { kChecks, kCleanup };
State sState = kChecks;
unsigned int sSince = 0;

} // namespace

void randoSmokeCodeloc(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sState == kChecks) {
        fprintf(stderr, "[cos] rando-smoke codeloc: pc_rando() %d, stage %s room %d\n", pc_rando(),
                dComIfGp_getStartStageName(), dComIfGp_roomControl_getStayNo());
        checkLookups();
        checkCodeC();
        checkTables();
        checkBigelf();
        checkHo();
        checkJi1(false);
        checkJi1(true);
        checkTc();
        int locs = 0;
        for (int loc = 0; loc < PCR_LOC_COUNT; loc++) {
            locs += sChecked[loc] != 0;
        }
        char what[96];
        snprintf(what, sizeof what, "%d location(s) with Code keys checked", locs);
        randoSmokeCheck(kName, what, locs > 0);
        sState = kCleanup;
        sSince = 0;
        return;
    }
    if (++sSince < 5) {
        return;
    }
    for (int i = 0; i < sDemoItemCount; i++) {
        if (fopAc_ac_c* a = fopAcM_SearchByID(sDemoItems[i])) {
            fopAcM_delete(a);
        }
    }
    dComIfGp_event_setItemPartnerId(fpcM_ERROR_PROCESS_ID_e);
    randoSmokeEnd(kName);
}

} // namespace pc
