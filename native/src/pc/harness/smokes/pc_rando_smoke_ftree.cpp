// COS_SMOKE=rando COS_RANDO_SMOKE=ftree (docs/RANDOMIZER_PLAN.md step R2.8, T2): the Withered Trees'
// heart part as a placed item, on an island with a withered tree, e.g.
//   run.sh rando --stage sea:42 --env COS_RANDO_SMOKE=ftree --env COS_RANDO_FORCE=1
// COS_RANDO_FORCE=1 is needed: the tree only runs under pc_rando() (R3.9: before Farore's Pearl).
// From the first frame the placement is the vanilla items with "The Great Sea - Withered Trees" set to
// COS_FTREE_ITEM (default 0xA3, the Goddess Tingle Statue of the R0.6 rotation placement: an item whose
// field model no stage loads, the case fastCreateItem cannot create). Once the play scene has run 30
// frames and no event runs (A through any opening talk):
//   1. launch: the room's withered tree (daObjFtree) gets mLaunchHeart = 1, as when its last watering
//      ends; launch_heart_part must create (custom_createItem) a field item of the placed id, which on
//      the frame it exists gets the momentum (flag set, actor status 0x40) and then flies up (its y
//      rises more than 100 above where it appeared) and lands;
//   2. place: that item is deleted, the "picked up" bit 0x2E20 cleared and 0x0102 set, as on a reload
//      after the last tree; place_heart_part must create it again, without momentum (its y never
//      rises more than 5 above where it appeared);
//   3. give: the player is put on the item; touching it must start the item-get demo of the placed
//      id (dComIfGp_event_getGtItm()), A goes through the messages, the event ends, and the tree marks
//      the heart part picked up (bit 0x2E20), and the item's item_func ran (for a Tingle Statue: one more
//      statue owned, R3.6). The item is moved onto the player,
//      since where the tree puts it may be out of his reach.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_seedfile.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"

#include "d/actor/d_a_item.h"
#include "d/actor/d_a_obj_ftree.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstdlib>

namespace pc {

namespace {

constexpr const char* kName = "ftree";

enum State { kInit, kWaitFree, kLaunch, kFly, kDelete, kPlace, kRest, kGive, kEvent, kDone };

State sState = kInit;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
uint8_t sItems[PC_RANDO_SEED_LOCATIONS];
uint8_t sExpect = 0xA3;
daObjFtree::Act_c* sTree = nullptr;
bool sSeen = false;
float sY0 = 0.0f;
float sYMax = 0.0f;
bool sSawEvent = false;
bool sStartedItemDemo = false;
int sStatuesBefore = 0; // Tingle Statues owned before the give
int sTreeReg = -1; // the tree index launch_heart_part stored in register 0x9B07

void line(const char* what) {
    fprintf(stderr, "[cos] rando-smoke %s: %s\n", kName, what);
}

daItem_c* heartItem() {
    if (sTree == nullptr || sTree->mSpawnedHeartPieceProcessId == fpcM_ERROR_PROCESS_ID_e) {
        return nullptr;
    }
    fopAc_ac_c* a = nullptr;
    if (!fopAcM_SearchByID(sTree->mSpawnedHeartPieceProcessId, &a) || a == nullptr) {
        return nullptr;
    }
    return (daItem_c*)a;
}

// The item exists: checks its id once and tracks its height.
bool track(const char* phase) {
    daItem_c* item = heartItem();
    if (item == nullptr) {
        return false;
    }
    if (!sSeen) {
        sSeen = true;
        sY0 = sYMax = item->current.pos.y;
        char buf[160];
        snprintf(buf, sizeof buf, "%s: field item of the placed id 0x%02X (name %d, id 0x%02X)", phase,
                 (unsigned)sExpect, (int)fopAcM_GetName(item), (unsigned)item->getItemNo());
        randoSmokeCheck(kName, buf, fopAcM_GetName(item) == fpcNm_ITEM_e && item->getItemNo() == sExpect);
    }
    if (item->current.pos.y > sYMax) {
        sYMax = item->current.pos.y;
    }
    return true;
}

void setPlacement() {
    for (int i = 0; i < PC_RANDO_SEED_LOCATIONS; i++) {
        pcr::Span<pcr::LocKey> keys = pcr::location_keys(i);
        sItems[i] = keys.size > 0 ? keys[0].vanilla : 0xFF;
    }
    if (const char* e = getenv("COS_FTREE_ITEM")) {
        sExpect = (uint8_t)strtoul(e, nullptr, 0);
    }
    sItems[PCR_LOC_THE_GREAT_SEA_WITHERED_TREES] = sExpect;
    pc_rando_set_loc_items(sItems, PC_RANDO_SEED_LOCATIONS);
}

} // namespace

void randoSmokeFtree(unsigned int frames) {
    (void)frames;
    if (sState == kInit) {
        if (!pc_rando()) {
            line("needs COS_RANDO_FORCE=1 (the tree runs under pc_rando() only)");
            randoSmokeCheck(kName, "pc_rando() is 1", false);
            randoSmokeEnd(kName);
        }
        setPlacement();
        sState = kWaitFree;
        sSince = 0;
        return;
    }
    if (!randoSmokeInPlay(30) || dComIfGp_getPlayer(0) == nullptr) {
        return;
    }
    ++sSince;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    switch (sState) {
    case kInit:
        return;
    case kDone:
        setDrivenPad(true, 0, 0, 0);
        if (sSince == 30) {
            randoSmokeCheck(kName, "give: the tree marks the heart part picked up (0x2E20)",
                            dComIfGs_isEventBit(dSv_event_flag_c::UNK_2E20) &&
                                sTree->mSpawnedHeartPieceProcessId == fpcM_ERROR_PROCESS_ID_e);
            // the item's item_func ran: a Tingle Statue (R3.6) sets its statue's bit (other ids: not
            // checked, many have no owned state)
            if (sExpect >= 0xA3 && sExpect <= 0xA7) {
                randoSmokeCheck(kName, "give: item_func ran (one more Tingle Statue)",
                                pc_rando_tingle_statue_count() == sStatuesBefore + 1);
            }
            randoSmokeEnd(kName);
        }
        return;
    case kWaitFree:
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince < 30) {
            return;
        }
        {
            fopAc_ac_c* a = nullptr;
            fopAcM_SearchByName(fpcNm_Obj_Ftree_e, &a);
            sTree = (daObjFtree::Act_c*)a;
        }
        if (!randoSmokeCheck(kName, "a withered tree in the room", sTree != nullptr) ||
            !randoSmokeCheck(kName, "the tree runs (mMode != 0)", sTree->mMode != 0)) {
            randoSmokeEnd(kName);
        }
        randoSmokeCheck(kName, "lookup gives the placed item",
                        pc_rando_code_c_item(PC_RANDO_CODE_C_WITHERED_TREE, 0, dItemNo_HEART_PIECE_e) == sExpect &&
                            pc_rando_code_c_item(PC_RANDO_CODE_C_WITHERED_TREE, 1, dItemNo_HEART_PIECE_e) == sExpect);
        dComIfGs_offEventBit(dSv_event_flag_c::UNK_2E20);
        sTree->m64A[0] = 0;
        sTree->mLaunchHeart = 1;
        sState = kLaunch;
        sSince = 0;
        return;
    case kLaunch:
        if (sTree->mLaunchHeart == 0 && sTree->mSpawnedHeartPieceProcessId != fpcM_ERROR_PROCESS_ID_e) {
            randoSmokeCheck(kName, "launch: heart part created (mHeartPlaced)", sTree->mHeartPlaced == 1);
            sTreeReg = dComIfGs_getEventReg(dSv_event_flag_c::UNK_9B07);
            sState = kFly;
            sSince = 0;
            sSeen = false;
        } else if (sSince > 60) {
            randoSmokeCheck(kName, "launch: launch_heart_part created an item", false);
            randoSmokeEnd(kName);
        }
        return;
    case kFly:
        track("launch");
        if (sSince == 120) {
            daItem_c* item = heartItem();
            randoSmokeCheck(kName, "launch: the item exists after 120 frames", item != nullptr);
            randoSmokeCheck(kName, "launch: momentum given (flag set)", sTree->m64A[0] == 1);
            randoSmokeCheck(kName, "launch: actor status 0x40", item != nullptr && (item->actor_status & 0x40) != 0);
            char buf[128];
            snprintf(buf, sizeof buf, "launch: it flew up (rose %.1f)", sYMax - sY0);
            randoSmokeCheck(kName, buf, sSeen && sYMax - sY0 > 100.0f);
            if (item != nullptr) {
                fpcM_Delete(item);
            }
            sState = kDelete;
            sSince = 0;
        }
        return;
    case kDelete:
        if (sSince == 5) {
            // as on a reload after the last tree: not picked up, all trees watered
            dComIfGs_offEventBit(dSv_event_flag_c::UNK_2E20);
            dComIfGs_onEventBit(dSv_event_flag_c::UNK_0102);
            sTree->mSpawnedHeartPieceProcessId = fpcM_ERROR_PROCESS_ID_e;
            sTree->m64A[0] = 0;
            sTree->mHeartPlaced = 0;
            dComIfGs_setEventReg(dSv_event_flag_c::UNK_9B07, (u8)sTreeReg);
            sState = kPlace;
            sSince = 0;
            sSeen = false;
        }
        return;
    case kPlace:
        if (sTree->mHeartPlaced != 0 && sTree->mSpawnedHeartPieceProcessId != fpcM_ERROR_PROCESS_ID_e) {
            randoSmokeCheck(kName, "place: flag set at create (no momentum)", sTree->m64A[0] == 1);
            sState = kRest;
            sSince = 0;
        } else if (sSince > 60) {
            randoSmokeCheck(kName, "place: place_heart_part created an item", false);
            randoSmokeEnd(kName);
        }
        return;
    case kRest:
        track("place");
        if (sSince == 60) {
            char buf[128];
            snprintf(buf, sizeof buf, "place: the item stays put (rose %.1f)", sYMax - sY0);
            randoSmokeCheck(kName, buf, sSeen && sYMax - sY0 <= 5.0f);
            sStatuesBefore = pc_rando_tingle_statue_count();
            sState = kGive;
            sSince = 0;
        }
        return;
    case kGive: {
        setDrivenPad(true, 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            sSawEvent = true;
            sStartedItemDemo = dComIfGp_event_getGtItm() == sExpect;
            sState = kEvent;
            sSince = 0;
            return;
        }
        daItem_c* item = heartItem();
        if (item != nullptr && sSince % 10 == 1) {
            // the item comes to the player (it lies where the tree put it, maybe out of reach)
            item->current.pos = player->current.pos;
            item->old.pos = player->current.pos;
        }
        if (item != nullptr && sSince % 60 == 1) {
            char buf[160];
            snprintf(buf, sizeof buf, "give: player (%.0f,%.0f,%.0f) item (%.0f,%.0f,%.0f) status 0x%X",
                     player->current.pos.x, player->current.pos.y, player->current.pos.z, item->current.pos.x,
                     item->current.pos.y, item->current.pos.z, (unsigned)item->actor_status);
            line(buf);
        }
        if (sSince > 300) {
            randoSmokeCheck(kName, "give: touching the item starts the item-get demo", false);
            randoSmokeEnd(kName);
        }
        return;
    }
    case kEvent:
        setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (!dComIfGp_event_runCheck()) {
            char buf[128];
            snprintf(buf, sizeof buf, "give: item-get demo of the placed id (0x%02X)",
                     (unsigned)dComIfGp_event_getGtItm());
            randoSmokeCheck(kName, buf, sSawEvent && (sStartedItemDemo || dComIfGp_event_getGtItm() == sExpect));
            sState = kDone;
            sSince = 0;
            return;
        }
        if (sSince > 3600) {
            randoSmokeCheck(kName, "give: the event ends", false);
            randoSmokeEnd(kName);
        }
        return;
    }
}

} // namespace pc
