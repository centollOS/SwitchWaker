// COS_SMOKE=rando COS_RANDO_SMOKE=zephos (docs/RANDOMIZER_PLAN.md step R3.19): Zephos's Wind Shrine
// event and the auction intro (features/rando/pc_rando_tweak_zephos.h, the d_a_npc_hr.cpp hunk).
//
// On the sea (run.sh rando --stage sea:13:5 --env COS_RANDO_SMOKE=zephos [--env COS_RANDO_FORCE=1]):
//   0. while the stage loads it sets event bit 0x0902 (Dragon Roost's arrival seen), else the
//      island's tag plays ARRIVAL_DRG2 and the wind tutorial first; it waits for any stage event;
//   1. once the play scene settled, the smoke puts the Wind Waker on X and orders the X-button talk
//      event with the Wind Shrine's Zephos (Hr, shape 0, hidden; its ht_hide state offers the item
//      talk while the player faces it) as the partner, as the player's item button does; Zephos's
//      XY callback turns it into TACT_HT (TACT_HT's 044pos_init then places the player);
//   2. the event plays with a driven pad: A for the messages, and at Link's "034tact" cut that wants
//      the Wind's Requiem (prm0 0) the C-stick conducts it (up, left, right in 3/4) until the
//      melody matches;
//   3. the smoke waits for the event to end and checks what it gave.
// With pc_rando() 0: the patcher never ran; TACT_HT is the vanilla one (Hr 28 cuts, Link 17,
// CAMERA 13) and plays to its end (Zephos appears and talks); the Wind's Requiem is learnt.
// With pc_rando() 1: the patcher shortened TACT_HT (Hr and Link 7 cuts, CAMERA 5, ending flags =
// the last cuts' flags); the smoke stands in for the placement of step R2.4 by changing Link's
// 059get_dance (actions[5]) into wwrando's change_event_item with a Piece of Heart (0x07) on the raw
// bytes. The event ends at the shrine's item, before Zephos appears, and gives only that item: one
// more heart piece and no Wind's Requiem (Zephos's hard-coded execItemGet in demoProcTact0 is gone).
//
// In the House of Wealth (run.sh rando --stage Orichh:0 --env COS_RANDO_SMOKE=zephos
// [--env COS_RANDO_FORCE=1]): the smoke follows AUCTION_START's CAMERA chain through
// dEvent_manager_c: 13 cuts with cut 3 UNITRANS (vanilla), or 11 with UNITRANS and the PAUSE after
// it gone (cut 3 is FIXEDFRM) under pc_rando().
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_event.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_zephos.h"

#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_audio.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "zephos";
constexpr uint8_t kSmokeItem = 0x07;          // Piece of Heart (011get_item)
constexpr unsigned int kOrderFrames = 300;    // the talk order must turn into TACT_HT by then
constexpr unsigned int kEventFrames = 9000;   // the whole event, vanilla included

enum State { kSetup, kOrder, kRun, kDone };
State sState = kSetup;
bool sRegistered = false;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
s16 sTactIdx = -1;
fpc_ProcID sHrId = fpcM_ERROR_PROCESS_ID_e;
u16 sLifeBefore = 0;
BOOL sSongBefore = FALSE;
int sSmokeEdit = -1; // the stand-in change_event_item on the raw bytes
int sCurCut[24];
char sHrLast[40] = "";      // the last cut Hr reached
unsigned int sHrCuts = 0;   // Hr's cuts seen
unsigned int sCutChanges = 0;

void smokePatcher(PcRandoResFile* f) {
    if (strcmp(f->stage, "sea") != 0 || strcmp(f->arc, "Stage") != 0 || strcmp(f->name, "event_list.dat") != 0) {
        return;
    }
    PcRandoEvl evl;
    int r = pc_rando_evl_open(&evl, f->data, f->size);
    if (r == PCR_EVL_OK) {
        const int ev = pc_rando_evl_find_event(&evl, "TACT_HT");
        const int slot = ev >= 0 ? pc_rando_evl_find_actor(&evl, ev, "Link") : -1;
        r = slot < 0 ? PCR_EVL_NO_ACTOR
                     : pc_rando_evl_change_item(&evl, "TACT_HT", slot, 5, kSmokeItem, nullptr, 0, nullptr, nullptr);
    }
    sSmokeEdit = r;
    writef(STDERR_FILENO, "[cos] rando-smoke zephos: stand-in placement (Link actions[5] -> 0x%02X): %s\n", kSmokeItem,
           pc_rando_evl_result_name(r));
}

dEvDtEvent_c* eventData(const char* name) {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    const s16 idx = mgr->getEventIdx(name, 0xFF);
    return idx >= 0 ? mgr->getEventData(idx) : nullptr;
}

// The staff named `staff` of an event (nullptr when absent).
dEvDtStaff_c* staffOf(dEvDtEvent_c* ev, const char* staff) {
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    for (int i = 0; i < ev->getNStaff(); i++) {
        dEvDtStaff_c* s = list.getStaffP(ev->getStaff(i));
        if (strcmp(s->getName(), staff) == 0) {
            return s;
        }
    }
    return nullptr;
}

// Follows a staff's cut chain as dEvent_manager_c does; the count (-1 when broken) and the name of
// cut `nth` (into name).
int cutChain(dEvDtStaff_c* s, int nth, char* name, size_t cap) {
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    s32 cut = (s32)s->getStartCut();
    int n = 0;
    name[0] = 0;
    while (cut >= 0) {
        if (cut >= list.getCutNum() || n > 64) {
            return -1;
        }
        dEvDtCut_c* c = list.getCutP(cut);
        if (n == nth) {
            snprintf(name, cap, "%.32s", c->getName());
        }
        n++;
        cut = (s32)c->getNext();
    }
    return n;
}

// The flag the last cut of a staff's chain sets.
s32 lastCutFlag(dEvDtStaff_c* s) {
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    s32 cut = (s32)s->getStartCut();
    s32 flag = -1;
    for (int n = 0; cut >= 0 && cut < list.getCutNum() && n <= 64; n++) {
        dEvDtCut_c* c = list.getCutP(cut);
        flag = (s32)c->getFlagId();
        cut = (s32)c->getNext();
    }
    return flag;
}

void checkAuction() {
    dEvDtEvent_c* ev = eventData("AUCTION_START");
    if (!randoSmokeCheck(kName, "AUCTION_START is in Orichh's event list", ev != nullptr)) {
        return;
    }
    dEvDtStaff_c* cam = staffOf(ev, "CAMERA");
    if (!randoSmokeCheck(kName, "AUCTION_START has a CAMERA staff", cam != nullptr)) {
        return;
    }
    char cut3[40];
    const int n = cutChain(cam, 3, cut3, sizeof cut3);
    char cut2[40];
    cutChain(cam, 2, cut2, sizeof cut2);
    writef(STDERR_FILENO, "[cos] rando-smoke zephos: AUCTION_START CAMERA %d cuts, cut 2 '%s', cut 3 '%s'\n", n, cut2,
           cut3);
    PcRandoZephosStatus st;
    pc_rando_tweak_zephos_status(PCR_ZEPHOS_AUCTION, &st);
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: the patcher never saw the file", st.seen == 0);
        randoSmokeCheck(kName, "vanilla: CAMERA 13 cuts, cut 3 UNITRANS (the pan)", n == 13 && strcmp(cut3, "UNITRANS") == 0);
        return;
    }
    randoSmokeCheck(kName, "shorten_auction_intro_event applied once", st.seen == 1 && st.applied == 1);
    randoSmokeCheck(kName, "CAMERA 11 cuts: PAUSE 30 then FIXEDFRM (the pan and the pause after it gone)",
                    n == 11 && strcmp(cut2, "PAUSE") == 0 && strcmp(cut3, "FIXEDFRM") == 0);
}

void checkTactShape() {
    dEvDtEvent_c* ev = eventData("TACT_HT");
    if (!randoSmokeCheck(kName, "TACT_HT is in the sea's event list", ev != nullptr)) {
        return;
    }
    dEvDtStaff_c* hr = staffOf(ev, "Hr");
    dEvDtStaff_c* link = staffOf(ev, "Link");
    dEvDtStaff_c* cam = staffOf(ev, "CAMERA");
    if (!randoSmokeCheck(kName, "TACT_HT has Hr, Link and CAMERA", hr != nullptr && link != nullptr && cam != nullptr)) {
        return;
    }
    char name[40], linkCut5[40], camLast[40];
    const int nHr = cutChain(hr, 0, name, sizeof name);
    const int nLink = cutChain(link, 5, linkCut5, sizeof linkCut5);
    const int nCam = cutChain(cam, 4, camLast, sizeof camLast);
    writef(STDERR_FILENO,
           "[cos] rando-smoke zephos: TACT_HT Hr %d cuts, Link %d (cut 5 '%s'), CAMERA %d (cut 4 '%s'); ending flags "
           "%d %d %d\n",
           nHr, nLink, linkCut5, nCam, camLast, (s32)ev->mFlagCheckFinish[0], (s32)ev->mFlagCheckFinish[1], (s32)ev->mFlagCheckFinish[2]);
    PcRandoZephosStatus st;
    pc_rando_tweak_zephos_status(PCR_ZEPHOS_TACT_HT, &st);
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: the patcher never saw the file", st.seen == 0);
        randoSmokeCheck(kName, "vanilla: Hr 28 cuts, Link 17, CAMERA 13", nHr == 28 && nLink == 17 && nCam == 13);
        randoSmokeCheck(kName, "vanilla: Link cut 5 is 059get_dance", strcmp(linkCut5, "059get_dance") == 0);
        return;
    }
    randoSmokeCheck(kName, "shorten_zephos_event applied once", st.seen == 1 && st.applied == 1);
    randoSmokeCheck(kName, "Hr 7 cuts, Link 7, CAMERA 5 (cut 4 GETITEM)",
                    nHr == 7 && nLink == 7 && nCam == 5 && strcmp(camLast, "GETITEM") == 0);
    randoSmokeCheck(kName, "ending flags = the flags of Hr's, Link's and CAMERA's last cuts",
                    (s32)ev->mFlagCheckFinish[0] == lastCutFlag(hr) && (s32)ev->mFlagCheckFinish[1] == lastCutFlag(link) &&
                        (s32)ev->mFlagCheckFinish[2] == lastCutFlag(cam));
    randoSmokeCheck(kName, "the stand-in placement made Link cut 5 011get_item",
                    sSmokeEdit == PCR_EVL_OK && strcmp(linkCut5, "011get_item") == 0);
}

// Logs every staff's cut change of the running event; returns Link's current cut (-1: none).
int logCuts() {
    dEvt_control_c* evc = g_dComIfG_gameInfo.play.getEvent();
    dEvDtEvent_c* ev = dComIfGp_getPEvtManager()->getEventData(evc->mEventId);
    if (ev == nullptr) {
        return -1;
    }
    int linkCut = -1;
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    for (int i = 0; i < ev->getNStaff() && i < 24; i++) {
        dEvDtStaff_c* s = list.getStaffP(ev->getStaff(i));
        const int cut = (int)s->getCurrentCut();
        if (strcmp(s->getName(), "Link") == 0) {
            linkCut = cut;
        }
        if (cut != sCurCut[i]) {
            sCurCut[i] = cut;
            sCutChanges++;
            const char* cutName = cut >= 0 && cut < list.getCutNum() ? list.getCutP(cut)->getName() : "-";
            if (strcmp(s->getName(), "Hr") == 0 && cut >= 0) {
                snprintf(sHrLast, sizeof sHrLast, "%.32s", cutName);
                sHrCuts++;
            }
            writef(STDERR_FILENO, "[cos] rando-smoke zephos: frame %u %s -> cut %d %.32s\n", sEventFrames, s->getName(),
                   cut, cutName);
        }
    }
    return linkCut;
}

// The driven pad during the event: A pulses for the messages, and the Wind's Requiem while Link
// conducts. The song is C-stick up, left, right in 3/4 (JAIZelInst::mMelodyPattern[0]; the control
// stick stays centred, which keeps 3/4). daPy_lk_c::procTactWait judges the C-stick's direction once
// per beat (it raises daPyRFlg0_TACT_INPUT then), and the judge's history restarts at beat 0, which
// is where a new 034tact cut starts. So the smoke counts the judges since Link's cut changed and
// holds the direction of the next one.
unsigned int sJudges = 0;
int sLinkCut = -2;
bool sJudgeSeen = false;

void drivePad(int linkCut) {
    daPy_lk_c* link = daPy_getPlayerLinkActorClass();
    if (linkCut != sLinkCut) {
        sLinkCut = linkCut;
        sJudges = 0;
    }
    const bool conducting = link != nullptr && dComIfGp_checkPlayerStatus1(0, daPyStts1_WIND_WAKER_CONDUCT_e);
    if (conducting) {
        const bool judged = link->checkResetFlg0(daPy_py_c::daPyRFlg0_TACT_INPUT) != 0;
        if (judged && !sJudgeSeen) {
            sJudges++;
            writef(STDERR_FILENO, "[cos] rando-smoke zephos: frame %u beat judged (%u), melody %d\n", sEventFrames,
                   sJudges, (int)link->getTactMusic());
        }
        sJudgeSeen = judged;
        static const int8_t kCx[3] = {0, -127, 127}; // up, left, right
        static const int8_t kCy[3] = {127, 0, 0};
        const unsigned int b = sJudges % 3;
        setDrivenPad(true, 0, 0, 0, kCx[b], kCy[b]);
        return;
    }
    sJudgeSeen = false;
    setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
}

void finish() {
    const u16 life = dComIfGs_getMaxLife();
    const BOOL song = dComIfGs_isTact(0);
    writef(STDERR_FILENO,
           "[cos] rando-smoke zephos: TACT_HT ended after %u frames (%u cut changes); max life %u -> %u, Wind's Requiem "
           "%d -> %d\n",
           sEventFrames, sCutChanges, sLifeBefore, life, sSongBefore, song);
    randoSmokeCheck(kName, "the Wind's Requiem was not known before", !sSongBefore);
    // daNpc_Hr_c::demoProcTact1 (Hr's TACT1 cut, kept by the shortened event) sets 0x2708.
    randoSmokeCheck(kName, "Hr's TACT1 ran (event bit 0x2708)", dComIfGs_isEventBit(dSv_event_flag_c::UNK_2708));
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: Hr played all 28 cuts, to WAIT_E (Zephos appeared and talked)",
                        sHrCuts == 28 && strcmp(sHrLast, "WAIT_E") == 0);
        randoSmokeCheck(kName, "vanilla: the Wind's Requiem is learnt", song);
        randoSmokeCheck(kName, "vanilla: no heart piece", life == sLifeBefore);
        return;
    }
    randoSmokeCheck(kName, "rando: the event ended at the shrine's item: Hr's 7 cuts, to WAIT_1 (Zephos never appeared)",
                    sHrCuts == 7 && strcmp(sHrLast, "WAIT_1") == 0);
    randoSmokeCheck(kName, "rando: the placed item only: one heart piece", life == sLifeBefore + 1);
    randoSmokeCheck(kName, "rando: no Wind's Requiem (no hard-coded give in demoProcTact0)", !song);
}

} // namespace

void randoSmokeZephos(unsigned int frames) {
    (void)frames;
    if (!sRegistered) {
        sRegistered = true;
        if (pc_rando()) {
            pc_rando_register_res_patcher(smokePatcher, PCR_RES_DAT);
        }
        for (int& c : sCurCut) {
            c = -2;
        }
    }
    if (!randoSmokeInPlay(30)) {
        // Dragon Roost's arrival seen (d_a_tag_island type 1 plays ARRIVAL_DRG2 and the wind
        // tutorial TACT_TEARCH0 without it), set while the stage loads, before its actors run.
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0902);
        return;
    }
    const char* stage = dComIfGp_getStartStageName();
    if (strcmp(stage, "Orichh") == 0) {
        checkAuction();
        randoSmokeEnd(kName);
    }
    if (strcmp(stage, "sea") != 0) {
        randoSmokeCheck(kName, "boot stage sea:13 or Orichh:0", false);
        randoSmokeEnd(kName);
    }

    daPy_lk_c* link = daPy_getPlayerLinkActorClass();
    switch (sState) {
    case kSetup: {
        if (dComIfGp_event_runCheck()) {
            // An event the stage started by itself (the arrival at the island): let it end.
            dEvt_control_c* evc = g_dComIfG_gameInfo.play.getEvent();
            dEvDtEvent_c* ev = dComIfGp_getPEvtManager()->getEventData(evc->mEventId);
            if (sSince++ % 60 == 0) {
                writef(STDERR_FILENO, "[cos] rando-smoke zephos: waiting for event %d '%s' to end\n", evc->mEventId,
                       ev != nullptr ? ev->getName() : "?");
            }
            setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sSince > 1800) {
                randoSmokeCheck(kName, "no other event runs before TACT_HT", false);
                randoSmokeEnd(kName);
            }
            return;
        }
        checkTactShape();
        fopAc_ac_c* hr = fopAcM_SearchByName(fpcNm_NPC_HR_e);
        if (!randoSmokeCheck(kName, "Zephos (Hr) is in sea room 13", hr != nullptr)) {
            randoSmokeEnd(kName);
        }
        sHrId = fopAcM_GetID(hr);
        sTactIdx = dComIfGp_evmng_getEventIdx("TACT_HT");
        sLifeBefore = dComIfGs_getMaxLife();
        sSongBefore = dComIfGs_isTact(0);
        dComIfGs_setItem(dInvSlot_WIND_WAKER_e, dItemNo_WIND_WAKER_e);
        dComIfGs_setSelectItem(dItemBtn_X_e, dInvSlot_WIND_WAKER_e);
        dComIfGp_setSelectItem(dItemBtn_X_e);
        writef(STDERR_FILENO, "[cos] rando-smoke zephos: Hr at (%.0f %.0f %.0f), TACT_HT index %d; ordering\n",
               hr->current.pos.x, hr->current.pos.y, hr->current.pos.z, sTactIdx);
        sState = kOrder;
        sSince = 0;
        return;
    }
    case kOrder: {
        sSince++;
        setDrivenPad(true, 0, 0, 0);
        dEvt_control_c* evc = g_dComIfG_gameInfo.play.getEvent();
        if (dComIfGp_event_runCheck() && evc->mEventId == sTactIdx) {
            writef(STDERR_FILENO, "[cos] rando-smoke zephos: TACT_HT runs after %u frames\n", sSince);
            sState = kRun;
            sEventFrames = 0;
            return;
        }
        if (sSince > kOrderFrames) {
            randoSmokeCheck(kName, "the X-button talk with Zephos starts TACT_HT", false);
            randoSmokeEnd(kName);
        }
        fopAc_ac_c* hr = fopAcM_SearchByID(sHrId);
        if (sSince % 30 == 1) {
            writef(STDERR_FILENO,
                   "[cos] rando-smoke zephos: order frame %u: run %d event %d, Hr cond 0x%X talk %d, Link cond 0x%X talk %d "
                   "at (%.0f %.0f %.0f) y %d, X item 0x%02X\n",
                   sSince, (int)dComIfGp_event_runCheck(), evc->mEventId, hr ? hr->eventInfo.getCondition() : 0,
                   hr ? hr->eventInfo.checkCommandTalk() : 0, link->eventInfo.getCondition(), link->eventInfo.checkCommandTalk(),
                   link->current.pos.x, link->current.pos.y, link->current.pos.z, link->shape_angle.y,
                   dComIfGp_getSelectItem(dItemBtn_X_e));
        }
        if (hr != nullptr && !dComIfGp_event_runCheck()) {
            fopAcM_orderTalkXBtnEvent(link, hr);
        }
        return;
    }
    case kRun:
        sEventFrames++;
        if (!dComIfGp_event_runCheck()) {
            setDrivenPad(false, 0, 0, 0);
            randoSmokeCheck(kName, "TACT_HT started", true);
            finish();
            sState = kDone;
            sSince = 0;
            return;
        }
        drivePad(logCuts());
        if (sEventFrames > kEventFrames) {
            randoSmokeCheck(kName, "TACT_HT ends", false);
            randoSmokeEnd(kName);
        }
        return;
    case kDone:
        if (++sSince >= 30) {
            randoSmokeEnd(kName);
        }
        return;
    }
}

} // namespace pc
