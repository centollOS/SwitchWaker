// COS_SMOKE=rando COS_RANDO_SMOKE=evloc (docs/RANDOMIZER_PLAN.md step R2.4): the 11 event item
// locations (features/rando/pc_rando_place_event.h) and Hurricane Spin, e.g.
//   run.sh rando --stage Orichh:0 --env COS_RANDO_SMOKE=evloc \
//                --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_rotation.json \
//                [--env COS_EVLOC_T2=Gp1_Get_Itm]
// A relative COS_RANDO_PLACEMENT is relative to the repository root (regress.sh runs from there).
// Once the play scene has run 10 frames:
//   T1, for every Event key (pcr::location_keys()) of the boot stage (at least one must exist):
//     - pc_rando() 1: the patcher placed it (pc_rando_place_event_result), with
//       pc_rando_loc_item(loc, vanilla), at the key's global action index; pc_rando() 0: the
//       patcher never saw it;
//     - the action as dEvent_manager_c reads it (event by name, staff slot, start cut and next
//       cuts, the cut's first data through getSubstance): pc_rando() 1: "059get_dance" with prm0 =
//       item - 0x6D for a song, else "011get_item" with prm0 = item (also for the vanilla item: the
//       two 046pget actions and Mail Sorting's "011_get_item" are renamed as wwrando does);
//       pc_rando() 0: the disc's name and prm0;
//   - Orca's code site: daNpc_Ji1_c::createItem on zeroed storage in its Hurricane Spin branch
//     (field_0xD7B 1) creates pc_rando_loc_item(Orca, 0xAA) (0xAA when off); the demo item is
//     deleted afterwards, the save restored;
//   - Hurricane Spin, pc_rando() 1: 0xAA has an item-get override; execItemGet(0xAA) sets event bit
//     0x6901 and pc_rando_hurricane_spin_owned() then gives 1 (the save is restored); its get model
//     (dItem_data::item_resource[0xAA]) has an archive (R2.11, plan D12). pc_rando() 0: 0x6901
//     stays clear.
//   T2 (COS_EVLOC_T2=<event name>, an event of a key of the boot stage): once no event runs, the
//   event is ordered as its own actor would (fopAcM_orderOtherEventId from the player, priority 1,
//   again every frame until it starts, at most kStartFrames), then A every 20 frames until the
//   event manager reports it finished (dComIfGp_evmng_endCheck; the smoke then resets it as the
//   ordering actor would) or it ends. A staff that waits in one cut for kStuckFrames (conducting a
//   song: 034tact; Tott's PATTEN, which he only plays in an event he ordered) gets that cut ended by
//   the smoke, except the location's own action, which the player runs. Checks: the item the action gives is the placed one (rando;
//   vanilla: the disc's): an item is given through execItemGet (a pass-through item-get override
//   on its id, or on its progressive tier, pc_rando_convert_progressive_item_id) and the save
//   changed (item_func ran), and dComIfGp_event_getGtItm is that id (the get-item demo's item;
//   without the randomizer the override registry is not consulted, so only these two); a song
//   through 059get_dance: the song's bit (dComIfGs_isTact) goes from clear to set. Hurricane Spin: bit 0x6901 goes from clear to set.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_event.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_place_event.h"

#include "d/actor/d_a_npc_ji1.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <dolphin/pad.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern void (*item_func_ptr[0x100])();

namespace pc {

namespace {

constexpr const char* kName = "evloc";
constexpr unsigned int kStartFrames = 240;
constexpr unsigned int kEventFrames = 6000;
constexpr unsigned int kTalkFrames = 20;

enum State { kInit, kCleanup, kT2Wait, kT2Order, kT2Run, kDone };
State sState = kInit;
unsigned int sSince = 0;

const pcr::LocKey* sT2Key = nullptr;
s16 sT2Idx = -1;
int sT2Item = -1;       // the item the action gives (placed or vanilla)
int sT2Conv = -1;       // the id execItemGet runs for (the progressive tier)
bool sT2Song = false;
bool sT2Got = false;    // the pass-through override ran
PcRandoItemFunc sPrevOverride = nullptr;
bool sHadTact = false;
bool sHadSpin = false;
fpc_ProcID sDemoItem = fpcM_ERROR_PROCESS_ID_e;
int sForcedCuts = 0;

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    writef(STDERR_FILENO, "[cos] rando-smoke evloc: %s\n", buf);
}

bool isSong(int item) {
    return item >= PCR_EVL_SONG_FIRST && item <= PCR_EVL_SONG_LAST;
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

// Every harness call into the process tree from the play scene's layer.
struct PlayLayerScope {
    layer_class* saved;
    PlayLayerScope() : saved(fpcLy_CurrentLayer()) {
        if (void* play = fpcM_Search(isPlayScene, nullptr)) {
            fpcLy_SetCurrentLayer(&static_cast<process_node_class*>(play)->mLayer);
        }
    }
    ~PlayLayerScope() { fpcLy_SetCurrentLayer(saved); }
};

// The item the key's action gives in this run.
uint8_t itemOf(const pcr::LocKey& k) {
    return pc_rando() ? pc_rando_loc_item(k.loc, k.vanilla) : k.vanilla;
}

// The action of key k and its prm0 as dEvent_manager_c reads them. False when the chain cannot be
// followed.
bool readBack(const pcr::LocKey& k, char* cutName, size_t cap, int* prm0, int* cutIdx) {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    const s16 idx = mgr->getEventIdx(k.event, 0xFF);
    dEvDtEvent_c* ev = mgr->getEventData(idx);
    if (ev == nullptr || k.actor >= ev->getNStaff()) {
        return false;
    }
    dEvDtBase_c& list = mgr->mList;
    dEvDtStaff_c* staff = list.getStaffP(ev->getStaff(k.actor));
    s32 cut = (s32)staff->getStartCut();
    for (int i = 0; i < k.action; i++) {
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
    *cutIdx = (int)cut;
    line("dEvent_manager_c reads %s staff %d (%s) cut %d '%s' prm0 = %d", k.event, (int)k.actor, staff->getName(),
         (int)cut, cutName, *prm0);
    return true;
}

void t1() {
    const char* stage = dComIfGp_getStartStageName();
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    int n = 0;
    char what[200];
    for (size_t i = 0; i < keys.size; i++) {
        const pcr::LocKey& k = keys[i];
        if (k.kind != pcr::KeyKind::Event || strcmp(k.stage, stage) != 0) {
            continue;
        }
        n++;
        const uint8_t item = itemOf(k);
        const int res = pc_rando_place_event_result((int)i);
        if (pc_rando()) {
            snprintf(what, sizeof what, "%s/%s %s: placed 0x%02X (vanilla 0x%02X) at global action %u", k.stage, k.arc,
                     k.event, item, k.vanilla, (unsigned)k.gaction);
            randoSmokeCheck(kName, what,
                            res == PCR_EVLOC_PLACED && pc_rando_place_event_item((int)i) == item &&
                                pc_rando_place_event_action((int)i) == (int)k.gaction);
        } else {
            snprintf(what, sizeof what, "%s/%s %s: vanilla, not patched", k.stage, k.arc, k.event);
            randoSmokeCheck(kName, what, res == PCR_EVLOC_NOT_SEEN);
        }
        char cut[40] = "";
        int prm0 = -1, cutIdx = -1;
        const bool read = readBack(k, cut, sizeof cut, &prm0, &cutIdx);
        const char* wantName = !pc_rando() ? k.action_name : isSong(item) ? "059get_dance" : "011get_item";
        const int wantPrm0 = !pc_rando() ? k.prm0 : isSong(item) ? item - PCR_EVL_SONG_FIRST : item;
        snprintf(what, sizeof what, "%s actor %u action %u reads '%s' prm0 %d (want '%s' %d)", k.event,
                 (unsigned)k.actor, (unsigned)k.action, cut, prm0, wantName, wantPrm0);
        randoSmokeCheck(kName, what,
                        read && strcmp(cut, wantName) == 0 && prm0 == wantPrm0 && cutIdx == (int)k.gaction);
    }
    snprintf(what, sizeof what, "%s: %d event key(s) checked", stage, n);
    randoSmokeCheck(kName, what, n > 0);
}

dSv_info_c sSave;

alignas(daNpc_Ji1_c) unsigned char sJi1[sizeof(daNpc_Ji1_c)];

void checkOrcaSite() {
    sSave = g_dComIfG_gameInfo.save;
    memset(sJi1, 0, sizeof sJi1);
    daNpc_Ji1_c* j = reinterpret_cast<daNpc_Ji1_c*>(sJi1);
    j->current.pos = dComIfGp_getLinkPlayer()->current.pos;
    j->current.roomNo = dComIfGp_getLinkPlayer()->current.roomNo;
    j->field_0xD7B = 1; // the Hurricane Spin branch (field_0xD7C 0: not the sword)
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    {
        PlayLayerScope layer;
        j->createItem();
    }
    sDemoItem = g_dComIfG_gameInfo.play.getEvent()->mPtItem;
    const u8 want = pc_rando_loc_item(PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS, dItemNo_HURRICANE_SPIN_e);
    char what[160];
    snprintf(what, sizeof what, "daNpc_Ji1_c::createItem (Hurricane Spin branch) -> 0x%02X (pc_rando() %d)", want,
             pc_rando());
    randoSmokeCheck(kName, what, j->mCreateItemNo == want && dComIfGp_event_getGtItm() == want);
    g_dComIfG_gameInfo.save = sSave;
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
}

void checkHurricaneSpin() {
    char what[160];
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: event bit 0x6901 clear", !dComIfGs_isEventBit(PC_RANDO_BIT_HURRICANE_SPIN));
        return;
    }
    randoSmokeCheck(kName, "0xAA has an item-get override", pc_rando_item_override_get(dItemNo_HURRICANE_SPIN_e) != nullptr);
    const char* arc = dItem_data::item_resource[dItemNo_HURRICANE_SPIN_e].mArcname;
    snprintf(what, sizeof what, "item_resource[0xAA] has a get model archive (%s)", arc != nullptr ? arc : "null");
    randoSmokeCheck(kName, what, arc != nullptr && arc[0] != '\0');
    sSave = g_dComIfG_gameInfo.save;
    dComIfGs_offEventBit(PC_RANDO_BIT_HURRICANE_SPIN);
    const int before = pc_rando_hurricane_spin_owned();
    execItemGet(dItemNo_HURRICANE_SPIN_e);
    const bool bit = dComIfGs_isEventBit(PC_RANDO_BIT_HURRICANE_SPIN);
    const int after = pc_rando_hurricane_spin_owned();
    snprintf(what, sizeof what, "execItemGet(0xAA) sets 0x6901 (owned %d -> %d)", before, after);
    randoSmokeCheck(kName, what, before == 0 && bit && after == 1);
    g_dComIfG_gameInfo.save = sSave;
}

// T2's pass-through override: records that execItemGet ran for the item, then gives it as the
// override that was there (or the vanilla table) would.
void t2ItemFunc() {
    sT2Got = true;
    if (sPrevOverride != nullptr) {
        sPrevOverride();
    } else if (sT2Conv >= 0) {
        item_func_ptr[sT2Conv & 0xFF]();
    }
}

void removeOverride() {
    if (sT2Conv >= 0 && !sT2Song) {
        pc_rando_item_override((uint8_t)sT2Conv, sPrevOverride);
    }
    sPrevOverride = nullptr;
}

// The save data an item_func writes: dSv_save_c without the clock (dSv_player_status_b_c).
struct SaveSnap {
    u8 save[sizeof(dSv_save_c)];
};
SaveSnap sSnap;
void snapSave(SaveSnap& s) {
    dSv_save_c& sv = g_dComIfG_gameInfo.save.getSavedata();
    memcpy(s.save, &sv, sizeof s.save);
    const size_t clock = (size_t)((u8*)&sv.getPlayer().getPlayerStatusB() - (u8*)&sv);
    memset(s.save + clock, 0, sizeof(dSv_player_status_b_c));
}

bool t2Setup(const char* name) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    for (size_t i = 0; i < keys.size; i++) {
        if (keys[i].kind == pcr::KeyKind::Event && strcmp(keys[i].event, name) == 0 &&
            strcmp(keys[i].stage, dComIfGp_getStartStageName()) == 0) {
            sT2Key = &keys[i];
        }
    }
    if (!randoSmokeCheck(kName, "T2: the event is a location of the boot stage", sT2Key != nullptr)) {
        return false;
    }
    sT2Idx = dComIfGp_getPEvtManager()->getEventIdx(sT2Key->event, 0xFF);
    if (!randoSmokeCheck(kName, "T2: the event manager knows the event", sT2Idx >= 0)) {
        return false;
    }
    sT2Item = itemOf(*sT2Key);
    sT2Song = isSong(sT2Item);
    sT2Conv = sT2Song ? -1 : pc_rando() ? pc_rando_convert_progressive_item_id(sT2Item) : sT2Item;
    line("T2: %s (event %d) gives 0x%02X%s%s", sT2Key->event, (int)sT2Idx, sT2Item, sT2Song ? " (song)" : "",
         sT2Item == dItemNo_HURRICANE_SPIN_e ? " (Hurricane Spin)" : "");
    if (sT2Song) {
        g_dComIfG_gameInfo.save.getPlayer().getCollect().offTact((u8)(sT2Item - PCR_EVL_SONG_FIRST));
        sHadTact = dComIfGs_isTact((u8)(sT2Item - PCR_EVL_SONG_FIRST));
    } else {
        sPrevOverride = pc_rando_item_override_get((uint8_t)sT2Conv);
        pc_rando_item_override((uint8_t)sT2Conv, t2ItemFunc);
    }
    if (sT2Item == dItemNo_HURRICANE_SPIN_e) {
        dComIfGs_offEventBit(PC_RANDO_BIT_HURRICANE_SPIN);
        sHadSpin = pc_rando_hurricane_spin_owned() != 0;
    }
    snapSave(sSnap);
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    return true;
}

void t2Check() {
    char what[160];
    if (sT2Song) {
        const bool tact = dComIfGs_isTact(sT2Item - PCR_EVL_SONG_FIRST);
        snprintf(what, sizeof what, "T2: song 0x%02X learned through 059get_dance", sT2Item);
        randoSmokeCheck(kName, what, !sHadTact && tact);
        return;
    }
    const u8 gt = dComIfGp_event_getGtItm();
    snprintf(what, sizeof what, "T2: the get-item demo item is 0x%02X (want 0x%02X)", (unsigned)gt, sT2Conv);
    randoSmokeCheck(kName, what, gt == sT2Conv);
    if (pc_rando()) {
        // the override registry is only consulted under pc_rando() (R0.8)
        snprintf(what, sizeof what, "T2: execItemGet ran for 0x%02X (item 0x%02X)", sT2Conv, sT2Item);
        randoSmokeCheck(kName, what, sT2Got);
    }
    SaveSnap now;
    snapSave(now);
    snprintf(what, sizeof what, "T2: item 0x%02X changed the save (item_func ran)", sT2Conv);
    randoSmokeCheck(kName, what, memcmp(&now, &sSnap, sizeof now) != 0);
    if (sT2Item == dItemNo_HURRICANE_SPIN_e) {
        randoSmokeCheck(kName, "T2: Hurricane Spin set bit 0x6901 (spin attack usable)",
                        !sHadSpin && pc_rando_hurricane_spin_owned() == 1);
    }
}

void finish() {
    removeOverride();
    setDrivenPad(false, 0, 0, 0);
    sState = kDone;
    randoSmokeEnd(kName);
}

// The harness stands in for what a debug boot cannot do (conduct a song with the Wind Waker, an
// NPC that only plays its part in an event it ordered itself): a staff whose current cut has not
// changed for kStuckFrames gets that cut ended (dComIfGp_evmng_cutEnd), as its actor would. The
// location's own action is never ended this way: the player has to run it.
constexpr unsigned int kStuckFrames = 150;
constexpr int kMaxStaff = PCR_EVL_EVENT_ACTORS;
int sStaffCut[kMaxStaff];
unsigned int sStaffSince[kMaxStaff];

void resetStuckCuts() {
    for (int i = 0; i < kMaxStaff; i++) {
        sStaffCut[i] = -1;
        sStaffSince[i] = 0;
    }
}

void driveStuckCuts() {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtEvent_c* ev = mgr->getEventData(sT2Idx);
    if (ev == nullptr) {
        return;
    }
    dEvDtBase_c& list = mgr->mList;
    for (int i = 0; i < ev->getNStaff() && i < kMaxStaff; i++) {
        const int staffIdx = ev->getStaff(i);
        dEvDtStaff_c* staff = list.getStaffP(staffIdx);
        const int cut = (int)staff->getCurrentCut();
        if (cut != sStaffCut[i]) {
            sStaffCut[i] = cut;
            sStaffSince[i] = 0;
            continue;
        }
        if (++sStaffSince[i] < kStuckFrames || cut == (int)sT2Key->gaction) {
            continue;
        }
        sStaffSince[i] = 0;
        line("T2: staff %s waits in cut %d '%.32s'; the harness ends it", staff->getName(), cut,
             list.getCutP(cut)->getName());
        sForcedCuts++;
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

void order() {
    PlayLayerScope layer;
    fopAcM_orderOtherEventId(dComIfGp_getPlayer(0), sT2Idx, 0xFF, 0xFFFF, 1);
}

} // namespace

void randoSmokeEvloc(unsigned int frames) {
    (void)frames;
    if (sState == kDone || !randoSmokeInPlay(10)) {
        return;
    }
    if (dComIfGp_getPlayer(0) == nullptr) {
        return;
    }
    sSince++;
    switch (sState) {
    case kInit:
        line("stage %s room %d, pc_rando() %d, %d event key(s)", dComIfGp_getStartStageName(),
             dComIfGp_roomControl_getStayNo(), pc_rando(), pc_rando_place_event_key_count());
        randoSmokeCheck(kName, "11 event keys", pc_rando_place_event_key_count() == 11);
        t1();
        checkOrcaSite();
        checkHurricaneSpin();
        sState = kCleanup;
        sSince = 0;
        return;
    case kCleanup: {
        if (sSince < 5) {
            return;
        }
        if (fopAc_ac_c* a = fopAcM_SearchByID(sDemoItem)) {
            fopAcM_delete(a);
        }
        dComIfGp_event_setItemPartnerId(fpcM_ERROR_PROCESS_ID_e);
        const char* t2 = getenv("COS_EVLOC_T2");
        if (t2 == nullptr || t2[0] == '\0') {
            finish();
            return;
        }
        if (!t2Setup(t2)) {
            finish();
            return;
        }
        sState = kT2Wait;
        sSince = 0;
        return;
    }
    case kT2Wait:
        // any opening event first (A through it), then the player stands for a moment
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= 30) {
            sState = kT2Order;
            sSince = 0;
            order();
        }
        return;
    case kT2Order: {
        dEvt_control_c* ctl = g_dComIfG_gameInfo.play.getEvent();
        if (dComIfGp_event_runCheck() && ctl->mEventId == sT2Idx) {
            line("T2: %s started after %u frame(s)", sT2Key->event, sSince);
            resetStuckCuts();
            sState = kT2Run;
            sSince = 0;
            return;
        }
        if (sSince >= kStartFrames) {
            randoSmokeCheck(kName, "T2: the event started", false);
            finish();
            return;
        }
        if (!dComIfGp_event_runCheck()) {
            order();
        } else {
            setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        }
        return;
    }
    case kT2Run: {
        dEvt_control_c* ctl = g_dComIfG_gameInfo.play.getEvent();
        const bool ended = !dComIfGp_event_runCheck() || ctl->mEventId != sT2Idx;
        if (ended || dComIfGp_evmng_endCheck(sT2Idx)) {
            line("T2: %s %s after %u frame(s), %d cut(s) ended by the harness", sT2Key->event,
                 ended ? "ended" : "finished", sSince, sForcedCuts);
            if (!ended) {
                dComIfGp_event_reset();
            }
            t2Check();
            finish();
            return;
        }
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        driveStuckCuts();
        if (sSince >= kEventFrames) {
            randoSmokeCheck(kName, "T2: the event finished", false);
            finish();
        }
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc
