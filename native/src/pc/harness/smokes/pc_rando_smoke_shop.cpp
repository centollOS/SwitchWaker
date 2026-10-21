// COS_SMOKE=rando COS_RANDO_SMOKE=shop (docs/RANDOMIZER_PLAN.md step R2.9): Beedle's shops with
// placed items, in Beedle's shop ship stage Obshop:
//   run.sh rando --stage Obshop:1 --env COS_RANDO_SMOKE=shop    (Rock Spire special shop, Masked Beedle)
//   run.sh rando --stage Obshop:2 --env COS_RANDO_SMOKE=shop    (a Great Sea shop: the 20 Rupee slot)
// (The plan says sea:16; Rock Spire Isle's room holds only the ship's outside, Beedle and his shop
// list are in Obshop room 1.)
//   1. vanilla, before activation (plain boot): the shop lists the disc's items (Rock Spire: Empty
//      Bottle 0x50, Piece of Heart 0x07, Treasure Chart 4 0xE1; Great Sea: Bait Bag 0x2C first), the
//      shop data hold their boot bytes (uniqueness bits set) and every shop item's model stands at
//      daShopItem_c::mData's Y offset;
//   2. activation with a placement that differs from vanilla in the 4 shop slots only (Rock Spire:
//      Grappling Hook 0x25, Iron Boots 0x29, Telescope 0x20; Great Sea: Spoils Bag 0x24), then the
//      same room is entered again so Beedle builds his list anew: the shop lists the placed items,
//      R0.8 holds the 4 mItemNo edits and the cleared uniqueness bits, and every shop item with a
//      Y offset of 0.0 in daShopItem_c::mData stands at 20.0 (fix_shop_item_y_offsets; the Iron
//      Boots are such an item);
//   3. buying through the real dialog with a driven pad (walk to Beedle, A to talk, the stick to pick
//      a slot, A to choose it, A for "I'll buy it", A through the item-get demo):
//        Rock Spire: slot 0 (Grappling Hook, not owned): the save owns it afterwards, the slot is sold
//        out and event bit 0x2020 is set; then slot 2 (Telescope, already owned before the shop
//        opened): the purchase goes through without a crash, the slot is sold out and 0x2008 is set;
//        Great Sea: slot 0 (Spoils Bag), with the Bait Bag already owned (so vanilla's "Bait Bag
//        owned" test would have sold the slot out at creation): the save owns the Spoils Bag, the
//        slot is sold out, event bit 0x6902 is set and the Bait Bag test did not change; entering the
//        room once more, the 20 Rupee slot shows its replacement (Hyoi Pear 0x83, as vanilla does
//        once the Bait Bag is owned);
//   4. deactivation: the shop data hold their boot bytes again; the game runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_lockeys.h"

#include "d/actor/d_a_npc_bs1.h"
#include "d/actor/d_a_shop_item.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_shop.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_math.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "shop";
constexpr unsigned int kSettle = 30;
constexpr unsigned int kBuyFrames = 2400; // per purchase, from the walk to the end of the item demo

enum Phase { kVanilla, kReenter, kPlaced, kWalk, kTalk, kAfterBuy, kReenter2, kReplaced, kEnd, kDone };
Phase sPhase = kVanilla;
unsigned int sSince = 0;
unsigned int sEndFrame = 0;
int sRoom = -1;
int sType = -1;         // Beedle's mType: 0 Great Sea shop, 1 Rock Spire (Masked Beedle)
fpc_ProcID sBeedleId = fpcM_ERROR_PROCESS_ID_e;
int sBuyStep = 0;       // index into the purchase list
uint8_t sLocItems[PCR_LOC_COUNT];
u32 sLastMsg = 0xFFFFFFFF;

struct Buy {
    int slot;        // shop slot (0-2)
    uint8_t item;    // the placed item
    bool owned;      // owned before the shop opened
    u16 bit;         // event bit set when sold out (0: none)
};
// Rock Spire: slot 0 (500 Rupees), slot 2 (900 Rupees).
const Buy kRockSpireBuys[] = {{0, 0x25, false, 0x2020}, {2, 0x20, true, 0x2008}};
// Great Sea: slot 0 (20 Rupees).
const Buy kSeaBuys[] = {{0, 0x24, false, 0}};

const Buy* buys(int* n) {
    if (sType == 1) {
        *n = 2;
        return kRockSpireBuys;
    }
    *n = 1;
    return kSeaBuys;
}

void* findBeedle(void* p, void* out) {
    fopAc_ac_c* a = (fopAc_ac_c*)p;
    if (fopAcM_GetName(a) == fpcNm_NPC_BS1_e) {
        *(daNpc_Bs1_c**)out = (daNpc_Bs1_c*)a;
        return p;
    }
    return nullptr;
}

daNpc_Bs1_c* beedle() {
    daNpc_Bs1_c* b = nullptr;
    fopAcIt_Judge(findBeedle, &b);
    return b;
}

daShopItem_c* slotItem(daNpc_Bs1_c* b, int i) {
    return (daShopItem_c*)fopAcM_SearchByID(b->mShopItems.mItemActorProcessIds[i]);
}

// The shop item's model Y offset over its actor (set_mtx: translate pos, rotate (zero for shop items),
// translate mData's offset).
float slotYOffset(daShopItem_c* it) {
    if (it == nullptr || it->mpModel == nullptr) {
        return -9999.0f;
    }
    MtxP m = it->mpModel->getBaseTRMtx();
    return m[1][3] - it->current.pos.y;
}

const ShopItems_c__ItemData* const kShopData[PCR_SHOP_SLOT_COUNT] = {
    &shopItemData_Feedbag, &shopItemData_emptybottle, &shopItemData_kakera_heart, &shopItemData_map};
const u8 kBootCond[PCR_SHOP_SLOT_COUNT] = {0x22, 0x20, 0x22, 0x22};
const u32 kBootItem[PCR_SHOP_SLOT_COUNT] = {0x2C, 0x50, 0x07, 0xE1};

bool shopDataIs(bool placed) {
    for (int i = 0; i < PCR_SHOP_SLOT_COUNT; i++) {
        const u32 item = placed ? pc_rando_loc_item(pc_rando_shop_slot_loc(i), kBootItem[i]) : kBootItem[i];
        const u8 cond = placed ? (u8)(kBootCond[i] & ~Shop_BuyCond_MUST_NOT_OWN_ITEM_e) : kBootCond[i];
        if (kShopData[i]->mItemNo != item || kShopData[i]->mBuyConditions != cond) {
            writef(STDERR_FILENO, "[cos] shop: shop data %d: item 0x%X cond 0x%X, want 0x%X 0x%X\n", i,
                   (unsigned)kShopData[i]->mItemNo, kShopData[i]->mBuyConditions, (unsigned)item, cond);
            return false;
        }
    }
    return true;
}

void logShop(daNpc_Bs1_c* b, const char* when) {
    for (int i = 0; i < 3; i++) {
        daShopItem_c* it = slotItem(b, i);
        writef(STDERR_FILENO, "[cos] shop: %s: slot %d item 0x%02X y offset %.1f sold out %d\n", when, i,
               it != nullptr ? it->m_itemNo : 0xFFFF, slotYOffset(it), (int)b->mShopItems.isSoldOutItem(i));
    }
}

void enterRoomAgain() {
    dComIfGp_setNextStage("Obshop", 0, (s8)sRoom);
}

// Vanilla items: the identity placement (every location its original item).
void fillPlacement() {
    for (int loc = 0; loc < PCR_LOC_COUNT; loc++) {
        auto keys = pcr::location_keys(loc);
        sLocItems[loc] = keys.size > 0 ? keys[0].vanilla : 0xFF;
    }
    if (sType == 1) {
        sLocItems[PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM] = 0x25;
        sLocItems[PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM] = 0x29;
        sLocItems[PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM] = 0x20;
    } else {
        sLocItems[PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM] = 0x24;
    }
}

// Every slot's model Y offset is mData's (fixed: with fix_shop_item_y_offsets applied).
bool yOffsetsAre(daNpc_Bs1_c* b, bool fixed) {
    bool ok = true;
    for (int i = 0; i < 3; i++) {
        daShopItem_c* it = slotItem(b, i);
        if (it == nullptr) {
            return false;
        }
        const float base = daShopItem_c::mData[it->m_itemNo].field_0x0C.y;
        const float want = fixed && base == 0.0f && (it->m_itemNo < 0x10 || it->m_itemNo > 0x12) ? 20.0f : base;
        ok = ok && slotYOffset(it) > want - 0.01f && slotYOffset(it) < want + 0.01f;
    }
    return ok;
}

void setPhase(Phase p) {
    sPhase = p;
    sSince = 0;
}

// Walk at Beedle until A says "Speak", then A.
void walkToBeedle(daNpc_Bs1_c* b, fopAc_ac_c* player) {
    if (dComIfGp_getDoStatus() == dActStts_SPEAK_e) {
        setDrivenPad(true, sSince % 10 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        return;
    }
    const s16 a = cM_atan2s(b->current.pos.x - player->current.pos.x, b->current.pos.z - player->current.pos.z);
    const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
    setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 50.0f), (int8_t)(-cM_scos(st) * 50.0f));
}

// In the shop dialog: pick `slot` with stick pulses, then A (choose, then "I'll buy it", then through
// the item's messages).
void driveShop(daNpc_Bs1_c* b, int slot, bool bought) {
    const int sel = b->mShopItems.mSelectedItemIdx;
    if (!bought && sel != slot && b->m835 == 3) {
        // m835 3: shop_talk, browsing (A on 0x2F47/0xF3E gets there): the stick moves the cursor. The
        // shops' select_list_data: from no item, left goes to slot 2 and right to slot 0; from a slot,
        // left and right go to the neighbours.
        const bool right = sel < 0 ? slot != 2 : slot > sel;
        const unsigned int t = sSince % 20;
        setDrivenPad(true, 0, t < 5 ? (right ? 110 : -110) : 0, 0);
        return;
    }
    setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
}

} // namespace

void randoSmokeShop(unsigned int frames) {
    if (!randoSmokeInPlay(kSettle)) {
        return;
    }
    if (sPhase == kDone) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return;
    }
    sSince++;
    daNpc_Bs1_c* b = beedle();

    switch (sPhase) {
    case kVanilla: {
        if (b == nullptr) {
            randoSmokeCheck(kName, "Beedle is in the room (Obshop:1 or Obshop:2)", false);
            randoSmokeEnd(kName);
        }
        sRoom = fopAcM_GetRoomNo(b);
        sType = b->mType;
        sBeedleId = fopAcM_GetID(b);
        writef(STDERR_FILENO, "[cos] shop: Beedle type %d shop index %d room %d\n", sType, b->mShopIndex, sRoom);
        logShop(b, "vanilla");
        randoSmokeCheck(kName, "plain boot: pc_rando() is 0", pc_rando() == 0);
        randoSmokeCheck(kName, "vanilla: shop data hold their boot bytes", shopDataIs(false));
        if (sType == 1) {
            daShopItem_c* s0 = slotItem(b, 0);
            daShopItem_c* s1 = slotItem(b, 1);
            daShopItem_c* s2 = slotItem(b, 2);
            randoSmokeCheck(kName, "vanilla: Rock Spire lists 0x50 0x07 0xE1",
                            s0 && s1 && s2 && s0->m_itemNo == 0x50 && s1->m_itemNo == 0x07 && s2->m_itemNo == 0xE1);
        } else {
            daShopItem_c* s0 = slotItem(b, 0);
            randoSmokeCheck(kName, "vanilla: the 20 Rupee slot lists the Bait Bag 0x2C", s0 && s0->m_itemNo == 0x2C);
        }

        randoSmokeCheck(kName, "vanilla: Y offsets are daShopItem_c::mData's", yOffsetsAre(b, false));

        // Before the shop opens again: rupees and wallet for the purchases, the "already owned" items.
        dComIfGs_setWalletSize(2);
        dComIfGs_setRupee(4000);
        int n = 0;
        const Buy* list = buys(&n);
        for (int i = 0; i < n; i++) {
            if (list[i].owned) {
                execItemGet(list[i].item);
            }
        }
        if (sType == 0) {
            execItemGet(dItemNo_BAIT_BAG_e); // vanilla would sell the slot out at creation
        }

        fillPlacement();
        pc_rando_set_loc_items(sLocItems, PCR_LOC_COUNT);
        pc_rando_set_active(nullptr, nullptr);
        randoSmokeCheck(kName, "activated", pc_rando() == 1);
        randoSmokeCheck(kName, "activation: shop data hold the placed items, uniqueness bits cleared",
                        shopDataIs(true));
        enterRoomAgain();
        setPhase(kReenter);
        return;
    }
    case kReenter:
    case kReenter2:
        // Wait for the new room's Beedle (a new process), then let it settle.
        if (b == nullptr || fopAcM_GetID(b) == sBeedleId) {
            sSince = 0;
            if (frames % 120 == 0) {
                writef(STDERR_FILENO, "[cos] shop: waiting for the room to come back\n");
            }
            return;
        }
        if (sSince < kSettle) {
            return;
        }
        sBeedleId = fopAcM_GetID(b);
        setPhase(sPhase == kReenter ? kPlaced : kReplaced);
        return;
    case kPlaced: {
        logShop(b, "placed");
        bool ok = true;
        for (int i = 0; i < 3; i++) {
            daShopItem_c* it = slotItem(b, i);
            uint8_t want;
            if (sType == 1) {
                want = sLocItems[pc_rando_shop_slot_loc(i + 1)];
            } else {
                want = i == 0 ? sLocItems[PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM]
                              : (it != nullptr ? it->m_itemNo : 0); // food slots: not randomized
            }
            ok = ok && it != nullptr && it->m_itemNo == want;
        }
        randoSmokeCheck(kName, sType == 1 ? "placed: Rock Spire lists 0x25 0x29 0x20"
                                          : "placed: the 20 Rupee slot lists 0x24 (Bait Bag owned)",
                        ok);
        randoSmokeCheck(kName, "placed: Y offsets of 0.0 read as 20.0", yOffsetsAre(b, true));
        if (sType == 1) {
            // Iron Boots: mData's Y offset is 0.0.
            randoSmokeCheck(kName, "placed: the Iron Boots (mData Y offset 0.0) stand at 20.0",
                            slotItem(b, 1) != nullptr && slotYOffset(slotItem(b, 1)) > 19.99f &&
                                slotYOffset(slotItem(b, 1)) < 20.01f);
        }
        setPhase(kWalk);
        return;
    }
    case kWalk:
    case kTalk:
    case kAfterBuy: {
        int n = 0;
        const Buy* list = buys(&n);
        const Buy& buy = list[sBuyStep];
        const bool soldOut = b->mShopItems.isSoldOutItem(buy.slot) != 0;
        const bool event = dComIfGp_event_runCheck() != 0;
        if (b->m738 != sLastMsg) {
            sLastMsg = b->m738;
            writef(STDERR_FILENO, "[cos] shop: frame %u: msg 0x%X selected %d talk mode %d event %d\n", frames,
                   (unsigned)b->m738, b->mShopItems.mSelectedItemIdx, (int)b->m835, (int)event);
        }
        if (sSince % 60 == 0) {
            writef(STDERR_FILENO, "[cos] shop: frame %u: phase %d msg 0x%X talk mode %d send %d do %d selected %d\n",
                   frames, (int)sPhase, (unsigned)b->m738, (int)b->m835, (int)dComIfGp_checkMesgSendButton(),
                   (int)dComIfGp_getDoStatus(), b->mShopItems.mSelectedItemIdx);
        }
        if (sPhase == kWalk) {
            if (event) {
                setPhase(kTalk);
                return;
            }
            walkToBeedle(b, player);
        } else if (sPhase == kTalk) {
            driveShop(b, buy.slot, soldOut);
            if (soldOut) {
                setPhase(kAfterBuy);
            } else if (!event && sSince > 30) {
                setPhase(kWalk); // the talk ended before the purchase: talk again
            }
        } else {
            // Through the item-get demo and "thanks" until Beedle is back in shop mode (or the talk ended).
            driveShop(b, buy.slot, true);
            if (sSince > 60 &&
                (dComIfGs_checkGetItem(buy.item) || buy.owned)) {
                char what[128];
                snprintf(what, sizeof what, "bought slot %d (0x%02X%s): owned, sold out%s", buy.slot, buy.item,
                         buy.owned ? ", already owned" : "", buy.bit ? ", its event bit" : "");
                randoSmokeCheck(kName, what,
                                dComIfGs_checkGetItem(buy.item) && soldOut &&
                                    (buy.bit == 0 || dComIfGs_isEventBit(buy.bit)));
                if (sType == 0) {
                    randoSmokeCheck(kName, "bought the 20 Rupee slot: event bit 0x6902 set",
                                    pc_rando_shop_bait_slot_sold_out() == 1);
                    randoSmokeCheck(kName, "bought the 20 Rupee slot: the Bait Bag is still owned",
                                    dComIfGs_checkGetItem(dItemNo_BAIT_BAG_e) != 0);
                }
                if (++sBuyStep >= n) {
                    if (sType == 0) {
                        setDrivenPad(true, 0, 0, 0);
                        enterRoomAgain();
                        setPhase(kReenter2);
                    } else {
                        setPhase(kEnd);
                    }
                } else {
                    setPhase(kTalk);
                }
                return;
            }
        }
        if (sSince > kBuyFrames) {
            logShop(b, "timeout");
            char what[96];
            snprintf(what, sizeof what, "buy slot %d within %u frames", buy.slot, kBuyFrames);
            randoSmokeCheck(kName, what, false);
            setPhase(kEnd);
        }
        return;
    }
    case kReplaced: {
        logShop(b, "after the purchase");
        daShopItem_c* s0 = slotItem(b, 0);
        randoSmokeCheck(kName, "re-entered: the 20 Rupee slot shows its replacement (Hyoi Pear 0x83)",
                        s0 != nullptr && s0->m_itemNo == dItemNo_HYOI_PEAR_e);
        setPhase(kEnd);
        return;
    }
    case kEnd:
        setDrivenPad(true, 0, 0, 0);
        pc_rando_deactivate();
        randoSmokeCheck(kName, "deactivated: shop data hold their boot bytes", pc_rando() == 0 && shopDataIs(false));
        sEndFrame = frames + 30;
        setPhase(kDone);
        return;
    case kDone:
        return;
    }
}

} // namespace pc
