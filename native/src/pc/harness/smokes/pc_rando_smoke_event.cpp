// COS_SMOKE=rando COS_RANDO_SMOKE=event (docs/RANDOMIZER_PLAN.md step R0.11): the event_list.dat
// in-place patcher (features/rando/pc_rando_event.h) through the archive-load hook (R0.10), run as
//   run.sh rando --stage Orichh:0 --env COS_RANDO_SMOKE=event [--env COS_RANDO_FORCE=1]
// The location is wwrando's "Windfall Island - Maggie's Father - Give 20 Skull Necklaces":
// Orichh/Stage.arc/Event04C:Gp1_Get_Itm/Actor002/Action001, action 011get_item with prm0 0xEE
// (its location key, pcr::location_keys(PCR_LOC_WINDFALL_ISLAND_MAGGIES_FATHER_GIVE_20_SKULL_NECKLACES),
// gives the path, the global action index, the integer-list slot and the D9 fingerprint).
// From the first frame, before the stage loads, the smoke registers a .dat patcher. For Orichh's
// Stage.arc event_list.dat it opens the raw (big-endian, not yet swapped) bytes and, under
// pc_rando(), makes wwrando's change_event_item with item 0x07 (pc_rando_evl_change_item, fingerprint
// 011get_item / 0xEE).
// Once the play scene has run 10 frames it reads the action back the way the game does, through
// dEvent_manager_c (setData has swapped the lists by then): the event by name (getEventIdx), its
// actor slot 2 (dEvDtEvent_c::getStaff), the staff's start cut, its next cut, the cut's first data
// and that data's value through dEvent_manager_c::getSubstance:
//   1. with pc_rando() 0 (no COS_RANDO_FORCE): the patcher never ran (no file dispatched) and the
//      game reads 011get_item / prm0 0xEE;
//   2. with pc_rando() 1: the patcher saw the file (stage Orichh, archive Stage), the edit returned
//      ok at the key's global action index and integer slot, and the game reads 011get_item /
//      prm0 0x07;
//   3. the game then runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_event.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_resources.h"

#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "event";
constexpr int kLoc = PCR_LOC_WINDFALL_ISLAND_MAGGIES_FATHER_GIVE_20_SKULL_NECKLACES;
constexpr uint8_t kNewItem = 0x07; // Piece of Heart (not a song, so the action stays 011get_item)

bool sRegistered = false;
unsigned int sEndFrame = 0;
const pcr::LocKey* sKey = nullptr;
int sSeen = 0;          // Orichh Stage event_list.dat files dispatched
int sCalls = 0;         // patcher calls of any .dat
bool sDirOk = false;
int sOpen = -1, sEdit = -1;
int sAction = -1, sSlot = -1;

const pcr::LocKey* findKey() {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys(kLoc);
    for (size_t i = 0; i < keys.size; i++) {
        if (keys[i].kind == pcr::KeyKind::Event) {
            return &keys[i];
        }
    }
    return nullptr;
}

void patcher(PcRandoResFile* f) {
    sCalls++;
    if (sKey == nullptr || strcmp(f->stage, sKey->stage) != 0 || strcmp(f->arc, sKey->arc) != 0 ||
        strcmp(f->name, "event_list.dat") != 0) {
        return;
    }
    sSeen++;
    sDirOk = strcmp(f->dir, "/res/Stage/Orichh/") == 0;
    PcRandoEvl evl;
    sOpen = pc_rando_evl_open(&evl, f->data, f->size);
    if (sOpen != PCR_EVL_OK) {
        writef(STDERR_FILENO, "[cos] rando-smoke event: open %s\n", pc_rando_evl_result_name(sOpen));
        return;
    }
    sEdit = pc_rando_evl_change_item(&evl, sKey->event, sKey->actor, sKey->action, kNewItem, sKey->action_name,
                                     sKey->prm0, &sAction, &sSlot);
    writef(STDERR_FILENO,
           "[cos] rando-smoke event: %s/%s/%s (%u events, %u actions, %u ints) %s actor %u action %u -> 0x%02X: %s "
           "(global action %d, int slot %d; key %u / %u)\n",
           f->stage, f->arc, f->name, evl.eventNum, evl.actionNum, evl.intNum, sKey->event, sKey->actor, sKey->action,
           kNewItem, pc_rando_evl_result_name(sEdit), sAction, sSlot, sKey->gaction, sKey->int_index);
}

// The action and its prm0 as dEvent_manager_c reads them. False when the chain cannot be followed.
bool readBack(char* cutName, size_t cap, int* prm0) {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    const s16 idx = mgr->getEventIdx(sKey->event, 0xFF);
    dEvDtEvent_c* ev = mgr->getEventData(idx);
    if (ev == nullptr || sKey->actor >= ev->getNStaff()) {
        return false;
    }
    dEvDtBase_c& list = mgr->mList;
    dEvDtStaff_c* staff = list.getStaffP(ev->getStaff(sKey->actor));
    s32 cut = (s32)staff->getStartCut();
    for (int i = 0; i < sKey->action; i++) {
        if (cut < 0 || cut >= list.getCutNum()) {
            return false;
        }
        cut = (s32)list.getCutP(cut)->getNext();
    }
    if (cut < 0 || cut >= list.getCutNum()) {
        return false;
    }
    dEvDtCut_c* c = list.getCutP(cut);
    snprintf(cutName, cap, "%.32s", c->getName());
    const s32 data = (s32)c->getDataTop();
    if (data < 0 || data >= list.getDataNum()) {
        return false;
    }
    dEvDtData_c* d = list.getDataP(data);
    if (strcmp(d->getName(), "prm0") != 0) {
        return false;
    }
    int* v = (int*)mgr->getSubstance(d, dEvDtData_c::TYPE_INT);
    if (v == nullptr) {
        return false;
    }
    *prm0 = *v;
    writef(STDERR_FILENO, "[cos] rando-smoke event: dEvent_manager_c reads %s staff %s cut %d '%s' %s = 0x%02X\n",
           sKey->event, staff->getName(), (int)cut, cutName, d->getName(), *prm0);
    return true;
}

} // namespace

void randoSmokeEvent(unsigned int frames) {
    if (!sRegistered) {
        sRegistered = true;
        sKey = findKey();
        pc_rando_register_res_patcher(patcher, PCR_RES_DAT);
        writef(STDERR_FILENO, "[cos] rando-smoke event: pc_rando() %d, key %s\n", pc_rando(),
               sKey != nullptr ? sKey->event : "(missing)");
    }
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    sEndFrame = frames + 30;

    if (!randoSmokeCheck(kName, "location key Gp1_Get_Itm actor 2 action 1 (011get_item, 0xEE)",
                         sKey != nullptr && strcmp(sKey->event, "Gp1_Get_Itm") == 0 && sKey->actor == 2 &&
                             sKey->action == 1 && sKey->prm0 == 0xEE)) {
        return;
    }
    char cutName[40] = "";
    int prm0 = -1;
    const bool read = readBack(cutName, sizeof cutName, &prm0);
    randoSmokeCheck(kName, "dEvent_manager_c follows Gp1_Get_Itm actor 2 action 1 to prm0", read);
    randoSmokeCheck(kName, "the action is still 011get_item", strcmp(cutName, "011get_item") == 0);

    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: no file dispatched, the patcher never ran", sCalls == 0 && sSeen == 0);
        randoSmokeCheck(kName, "vanilla: prm0 reads 0xEE", prm0 == 0xEE);
        return;
    }
    randoSmokeCheck(kName, "Orichh Stage event_list.dat dispatched once (dir /res/Stage/Orichh/)", sSeen == 1 && sDirOk);
    randoSmokeCheck(kName, "the raw file opens (big-endian, not swapped yet)", sOpen == PCR_EVL_OK);
    randoSmokeCheck(kName, "change_event_item ok", sEdit == PCR_EVL_OK);
    randoSmokeCheck(kName, "edit at the key's global action and int slot",
                    sAction == sKey->gaction && sSlot == sKey->int_index);
    randoSmokeCheck(kName, "prm0 reads 0x07", prm0 == kNewItem);
}

} // namespace pc
