// COS_SMOKE=rando COS_RANDO_SMOKE=progressive (docs/RANDOMIZER_PLAN.md step R3.3): progressive items
// in a booted game, e.g.
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=progressive [--env COS_RANDO_FORCE=1]
// Once the play scene has run 10 frames:
//   1. plain boot only, before activation: item_func_bow/bomb_bag set 30/30, item_func_deku_leaf adds
//      16 magic, execItemGet(0x3E) gives the full-power Master Sword at once, and message 278
//      (101+0xB1) is not in the overlay;
//   2. activation: the 20 progressive ids reach R0.8's overrides and message 278 is in the overlay;
//   3. for 7 families, from an empty save (30 bombs and 30 arrows, as the rando new game): every id of
//      the family is given in turn, at least 3 times each, and every give checks
//      pc_rando_convert_progressive_item_id (the next tier) before and the save after (sword
//      0x38->0x39->0x3A->0x3E, shield 0x3B->0x3C, bow 0x27->0x35->0x36, wallet, bomb bag, quiver,
//      picto box); past the last tier nothing changes;
//   4. the no-ops: item_func_bow/bomb_bag keep 60/60, item_func_deku_leaf adds no magic;
//   5. magic meter: 0xB1 adds 16/16 to the magic counters (normal_magic_meter_item_func), then the
//      meter grows to 16; 0xB1 again converts to 0xB2 and the meter grows to 32; then nothing;
//   6. the hunks in game/: a demo item (fopAcM_createDemoItem) and a shop item (daShopItem_c::_create)
//      created as the Hero's Shield with the Hero's Shield owned come up as the Mirror Shield 0x3C,
//      with its item model; a field item (daItem_c::_daItem_create) created as the Picto Box with the
//      Picto Box owned comes up as the Deluxe Picto Box 0x26 (whose field model the smoke borrows
//      from the green rupee for the test, as allow_all_items_to_be_field_items would);
//   7. 10 frames later (the deleted actors free their resources by arc name): the field model is put
//      back; deactivation (plain boot): message 278 is gone; the game runs 30 more frames.
// Not run here: the dProcGetItem_init and daNpc_Bs1_c::next_msgStatus hunks, which need an item-get
// event and Beedle's shop; they call the same conversion (census and code review).
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_progressive.h"

#include "d/actor/d_a_itembase.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "progressive";

enum Phase { kInit, kMagic16, kMagic32, kActors, kCleanup, kDone };
Phase sPhase = kInit;
unsigned int sPhaseFrame = 0;
unsigned int sEndFrame = 0;

struct Spawn {
    const char* what;
    fpc_ProcID id;
    u8 expect;
    bool done;
};
Spawn sSpawns[3];
dItem_data_field_item_res sFieldBackup;

dSv_player_c& player() {
    return g_dComIfG_gameInfo.save.getPlayer();
}

void resetMagicCounters() {
    dComIfGp_setItemMagicCount(-dComIfGp_getItemMagicCount());
    dComIfGp_setItemMaxMagicCount(-dComIfGp_getItemMaxMagicCount());
}

// The save as the rando new game leaves it for the progressive families (nothing owned, 30/30).
void resetSave() {
    dComIfGs_setCollect(0, 0);
    dComIfGs_setCollect(1, 0);
    player().getGetItem().mItemFlags[dInvSlot_BOW_e] = 0;
    player().getGetItem().mItemFlags[dInvSlot_CAMERA_e] = 0;
    dComIfGs_setWalletSize(0);
    dComIfGs_setBombNum(30);
    dComIfGs_setBombMax(30);
    dComIfGs_setArrowNum(30);
    dComIfGs_setArrowMax(30);
    dComIfGs_setMaxMagic(0);
    dComIfGs_setMagic(0);
    resetMagicCounters();
}

// The save byte(s) of a family as one comparable value.
int stateOf(int family) {
    switch (family) {
    case PCR_PROG_SWORD: return dComIfGs_checkCollect(0);
    case PCR_PROG_SHIELD: return dComIfGs_checkCollect(1);
    case PCR_PROG_BOW: return player().getGetItem().mItemFlags[dInvSlot_BOW_e];
    case PCR_PROG_WALLET: return dComIfGs_getWalletSize();
    case PCR_PROG_BOMB_BAG: return dComIfGs_getBombMax() | dComIfGs_getBombNum() << 8;
    case PCR_PROG_QUIVER: return dComIfGs_getArrowMax() | dComIfGs_getArrowNum() << 8;
    case PCR_PROG_PICTO_BOX: return player().getGetItem().mItemFlags[dInvSlot_CAMERA_e];
    default: return -1;
    }
}

// The state each family is in after tier t was given.
int expectAfter(int family, int t) {
    switch (family) {
    case PCR_PROG_SWORD: { static const int v[] = {1, 3, 7, 15}; return v[t]; }
    case PCR_PROG_SHIELD: return t == 0 ? 1 : 3;
    case PCR_PROG_BOW: return t == 0 ? 1 : t == 1 ? 3 : 7;
    case PCR_PROG_WALLET: return t + 1;
    case PCR_PROG_BOMB_BAG:
    case PCR_PROG_QUIVER: return t == 0 ? (60 | 60 << 8) : (99 | 99 << 8);
    case PCR_PROG_PICTO_BOX: return t == 0 ? 1 : 3;
    default: return -1;
    }
}

void checkFamily(int family) {
    static const char* names[PCR_PROG_COUNT] = {"sword", "shield", "bow", "wallet", "bomb bag", "quiver",
                                                "picto box", "magic meter"};
    u8 ids[5];
    int nids = 0;
    for (int i = 0; i < PCR_PROG_ID_COUNT; i++) {
        const u8 id = pc_rando_progressive_id(i);
        if (pc_rando_progressive_family(id) == family) {
            ids[nids++] = id;
        }
    }
    const int ntiers = pc_rando_progressive_tier_count(family);
    const int gives = nids * 3 > ntiers + 1 ? nids * 3 : ntiers + 1;
    bool convOk = true;
    bool saveOk = true;
    char seq[96] = "";
    for (int k = 0; k < gives; k++) {
        const u8 id = ids[k % nids];
        const int t = k < ntiers ? k : -1;
        const int before = stateOf(family);
        const int conv = pc_rando_convert_progressive_item_id(id);
        const int want = t >= 0 ? pc_rando_progressive_tier_id(family, t) : pc_rando_progressive_tier_id(family, 0);
        convOk = convOk && conv == want;
        if (k <= ntiers) {
            char part[12];
            snprintf(part, sizeof part, k == 0 ? "%02X" : "->%02X", conv);
            strncat(seq, part, sizeof seq - strlen(seq) - 1);
        }
        execItemGet(id);
        const int after = stateOf(family);
        saveOk = saveOk && (t >= 0 ? after == expectAfter(family, t) : after == before);
        if (!(convOk && saveOk)) {
            printf("[cos] rando-smoke %s: %s give %d (id 0x%02X): convert 0x%02X want 0x%02X, state 0x%X -> 0x%X\n",
                   kName, names[family], k, id, conv, want, before, after);
            break;
        }
    }
    char what[160];
    snprintf(what, sizeof what, "%s: %d gives over %d ids, tiers %s, then nothing", names[family], gives, nids, seq);
    randoSmokeCheck(kName, what, convOk && saveOk);
}

bool everyOverride() {
    for (int i = 0; i < PCR_PROG_ID_COUNT; i++) {
        if (pc_rando_item_override_get(pc_rando_progressive_id(i)) == nullptr) {
            return false;
        }
    }
    return pc_rando_progressive_family(0x3D) == PCR_PROG_SWORD;
}

void vanillaChecks() {
    resetSave();
    dComIfGs_setArrowNum(60);
    dComIfGs_setArrowMax(60);
    item_func_bow();
    randoSmokeCheck(kName, "plain: item_func_bow sets 30/30 arrows",
                    dComIfGs_getArrowNum() == 30 && dComIfGs_getArrowMax() == 30);
    dComIfGs_setBombNum(60);
    dComIfGs_setBombMax(60);
    item_func_bomb_bag();
    randoSmokeCheck(kName, "plain: item_func_bomb_bag sets 30/30 bombs",
                    dComIfGs_getBombNum() == 30 && dComIfGs_getBombMax() == 30);
    item_func_deku_leaf();
    randoSmokeCheck(kName, "plain: item_func_deku_leaf adds 16/16 magic",
                    dComIfGp_getItemMagicCount() == 16 && dComIfGp_getItemMaxMagicCount() == 16);
    resetMagicCounters();
    execItemGet(0x3E);
    randoSmokeCheck(kName, "plain: execItemGet(0x3E) gives the full-power sword at once",
                    dComIfGs_checkCollect(0) == 8);
    randoSmokeCheck(kName, "plain: message 278 not in the overlay",
                    pc_rando_msg_find(PCR_PROG_MAGIC_MSG_NO) == nullptr);
}

void noopChecks() {
    dComIfGs_setArrowNum(60);
    dComIfGs_setArrowMax(60);
    item_func_bow();
    randoSmokeCheck(kName, "rando: item_func_bow keeps 60/60 arrows",
                    dComIfGs_getArrowNum() == 60 && dComIfGs_getArrowMax() == 60);
    dComIfGs_setBombNum(60);
    dComIfGs_setBombMax(60);
    item_func_bomb_bag();
    randoSmokeCheck(kName, "rando: item_func_bomb_bag keeps 60/60 bombs",
                    dComIfGs_getBombNum() == 60 && dComIfGs_getBombMax() == 60);
    resetMagicCounters();
    item_func_deku_leaf();
    randoSmokeCheck(kName, "rando: item_func_deku_leaf adds no magic",
                    dComIfGp_getItemMagicCount() == 0 && dComIfGp_getItemMaxMagicCount() == 0);
}

fpc_ProcID spawnShop(u8 itemNo, cXyz* pos, int room) {
    return fopAcM_create(fpcNm_ShopItem_e, itemNo | 2 << 8, pos, room, nullptr, nullptr);
}

void spawnActors() {
    fopAc_ac_c* link = dComIfGp_getPlayer(0);
    cXyz pos = link->current.pos;
    pos.y += 1500.0f; // out of reach
    const int room = fopAcM_GetRoomNo(link);
    dComIfGs_setCollect(1, 1);                                  // Hero's Shield owned
    player().getGetItem().mItemFlags[dInvSlot_CAMERA_e] = 1;    // Picto Box owned
    sFieldBackup = dItem_data::field_item_res[0x26];
    dItem_data::field_item_res[0x26] = dItem_data::field_item_res[dItemNo_GREEN_RUPEE_e];
    sSpawns[0] = {"demo item 0x3B (fopAcM_createDemoItem)", fopAcM_createDemoItem(&pos, 0x3B, -1, nullptr, room, nullptr, 0),
                  0x3C, false};
    pos.x += 100.0f;
    sSpawns[1] = {"shop item 0x3B (daShopItem_c::_create)", spawnShop(0x3B, &pos, room), 0x3C, false};
    pos.x += 100.0f;
    sSpawns[2] = {"field item 0x23 (daItem_c::_daItem_create)",
                  fopAcM_createItem(&pos, 0x23, -1, room, 0, nullptr, 0, nullptr), 0x26, false};
}

// Returns true when every spawned actor was checked (or failed).
bool pollActors() {
    bool all = true;
    for (Spawn& s : sSpawns) {
        if (s.done) {
            continue;
        }
        fopAc_ac_c* ac = nullptr;
        if (s.id == fpcM_ERROR_PROCESS_ID_e || !fopAcM_SearchByID(s.id, &ac)) {
            char what[160];
            snprintf(what, sizeof what, "%s: created", s.what);
            randoSmokeCheck(kName, what, false);
            s.done = true;
            continue;
        }
        if (ac == nullptr) { // still creating
            all = false;
            continue;
        }
        const u8 got = static_cast<daItemBase_c*>(ac)->getItemNo();
        char what[200];
        const char* arc = &s == &sSpawns[2] ? dItem_data::getFieldArc(s.expect) : dItem_data::getArcname(s.expect);
        snprintf(what, sizeof what, "%s comes up as 0x%02X (got 0x%02X, params 0x%08X), model %s", s.what, s.expect,
                 got, fopAcM_GetParam(ac), arc != nullptr ? arc : "-");
        const bool modelOk = &s != &sSpawns[0] || (dItem_data::getBmdIdx(s.expect) != -1 &&
                                                   dItem_data::getArcname(s.expect) != nullptr);
        randoSmokeCheck(kName, what, got == s.expect && modelOk);
        fopAcM_delete(ac);
        s.done = true;
    }
    return all;
}

} // namespace

void randoSmokeProgressive(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    const int forced = pc_rando_forced();
    switch (sPhase) {
    case kInit:
        if (!forced) {
            vanillaChecks();
            pc_rando_set_active(nullptr, nullptr);
        }
        randoSmokeCheck(kName, "active: the 20 progressive ids have overrides", everyOverride());
        {
            const PcRandoMsg* m = pc_rando_msg_find(PCR_PROG_MAGIC_MSG_NO);
            randoSmokeCheck(kName, "active: message 278 (101+0xB1) in the overlay, item-get box, item 0xB1",
                            m != nullptr && m->has_entry && m->entry.text_box_type == 9 &&
                                m->entry.display_item_id == 0xB1 && m->bytes != nullptr);
        }
        resetSave();
        for (int f = 0; f < PCR_PROG_MAGIC_METER; f++) {
            checkFamily(f);
        }
        noopChecks();
        resetSave();
        randoSmokeCheck(kName, "magic: 0xB1 converts to 0xB1 with no meter", pc_rando_convert_progressive_item_id(0xB1) == 0xB1);
        execItemGet(0xB2);
        randoSmokeCheck(kName, "magic: 0xB2 (any tier id) gives the normal meter: +16/+16 to the magic counters",
                        dComIfGp_getItemMagicCount() == 16 && dComIfGp_getItemMaxMagicCount() == 16);
        sPhase = kMagic16;
        sPhaseFrame = frames;
        break;
    case kMagic16:
        if (dComIfGs_getMaxMagic() == 16 && dComIfGp_getItemMaxMagicCount() == 0) {
            randoSmokeCheck(kName, "magic: the meter grew to 16", true);
            randoSmokeCheck(kName, "magic: 0xB1 now converts to 0xB2", pc_rando_convert_progressive_item_id(0xB1) == 0xB2);
            execItemGet(0xB1);
            randoSmokeCheck(kName, "magic: 0xB1 gives the upgrade (+32 to the max counter)",
                            dComIfGp_getItemMaxMagicCount() == 32);
            sPhase = kMagic32;
            sPhaseFrame = frames;
        } else if (frames - sPhaseFrame > 600) {
            randoSmokeCheck(kName, "magic: the meter grew to 16", false);
            sPhase = kMagic32;
        }
        break;
    case kMagic32:
        if (dComIfGs_getMaxMagic() == 32 && dComIfGp_getItemMaxMagicCount() == 0) {
            randoSmokeCheck(kName, "magic: the meter grew to 32", true);
            randoSmokeCheck(kName, "magic: full meter converts to the base id 0xB1",
                            pc_rando_convert_progressive_item_id(0xB1) == 0xB1);
            execItemGet(0xB1);
            randoSmokeCheck(kName, "magic: full meter, 0xB1 gives nothing",
                            dComIfGp_getItemMaxMagicCount() == 0 && dComIfGs_getMaxMagic() == 32);
            spawnActors();
            sPhase = kActors;
            sPhaseFrame = frames;
        } else if (frames - sPhaseFrame > 600) {
            randoSmokeCheck(kName, "magic: the meter grew to 32", false);
            spawnActors();
            sPhase = kActors;
            sPhaseFrame = frames;
        }
        break;
    case kActors:
        if (pollActors() || frames - sPhaseFrame > 300) {
            for (Spawn& s : sSpawns) {
                if (!s.done) {
                    char what[160];
                    snprintf(what, sizeof what, "%s: created within 300 frames", s.what);
                    randoSmokeCheck(kName, what, false);
                }
            }
            sPhase = kCleanup;
            sPhaseFrame = frames;
        }
        break;
    case kCleanup:
        // The deleted actors free their resources on a later frame, by their item's arc name: the
        // borrowed field model (and the item tables, which deactivation restores) stay until then.
        if (frames - sPhaseFrame >= 10) {
            dItem_data::field_item_res[0x26] = sFieldBackup;
            if (!forced) {
                pc_rando_deactivate();
                randoSmokeCheck(kName, "deactivate: message 278 gone", pc_rando_msg_find(PCR_PROG_MAGIC_MSG_NO) == nullptr);
                randoSmokeCheck(kName, "deactivate: 0xB1 conversion unused (hunks off)", pc_rando() == 0);
            }
            sPhase = kDone;
            sEndFrame = frames + 30;
        }
        break;
    case kDone:
        break;
    }
}

} // namespace pc
