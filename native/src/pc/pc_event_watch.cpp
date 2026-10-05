// Stuck-event watch (every build, the console included): an event that runs for more than
// kStuckFrames (20 s at 30 fps) is logged, then every kRepeatFrames while it lasts, at most
// kMaxReports times a run: its name and mode, and each staff member's name, current cut and that
// cut's start flags (a cut waits for its start flags, set when other staff finish their cuts). The
// chest at the end of Niko's rope lesson froze half open on the console with the game still running
// at 30 fps (an event waiting forever); the Mac does not reproduce it, so the next time it happens
// this tells which staff member and cut it waits on.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"

#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kStuckFrames = 600;
constexpr unsigned int kRepeatFrames = 300;
constexpr unsigned int kMaxReports = 12;

unsigned int sRunning = 0;
s16 sEventId = -1;
unsigned int sReports = 0;

void report(unsigned int frames) {
    dEvt_control_c* ev = g_dComIfG_gameInfo.play.getEvent();
    dEvent_manager_c* mng = dComIfGp_getPEvtManager();
    dEvDtEvent_c* data = mng->getEventData(ev->mEventId);
    fopAc_ac_c* pt1 = ev->getPt1();
    fopAc_ac_c* pt2 = ev->getPt2();
    writef(STDERR_FILENO, "[cos] event-watch: frame %u: event %d \"%s\" running for %u frames, mode %u, actors %d / %d\n",
           frames, ev->mEventId, data != nullptr ? data->getName() : "(no data)", sRunning, ev->getMode(),
           pt1 != nullptr ? fopAcM_GetName(pt1) : -1, pt2 != nullptr ? fopAcM_GetName(pt2) : -1);
    // The item get's message cannot be closed while the item fanfare (a sub BGM) still plays
    // (d_msg.cpp: checkMesgBgm and mDoAud_checkPlayingSubBgmFlag): a fanfare that never ends
    // leaves the event waiting with the game running.
    writef(STDERR_FILENO, "[cos] event-watch:   audio: sub BGM playing 0x%x, message waits for music %d, "
                          "message status %d\n", (unsigned)mDoAud_checkPlayingSubBgmFlag(),
           (int)dComIfGp_checkMesgBgm(), (int)dComIfGp_getMesgStatus());
    if (data == nullptr) {
        return;
    }
    for (int i = 0; i < data->getNStaff() && i < 20; i++) {
        const int staffIdx = data->getStaff(i);
        dEvDtStaff_c* staff = mng->mList.getStaffP(staffIdx);
        dEvDtCut_c* cut = mng->mList.getCutStaffCurrentCutP(staffIdx);
        writef(STDERR_FILENO, "[cos] event-watch:   staff %d \"%s\" type %d: cut %u \"%s\", start flags %d %d %d, "
                              "flag %d\n", staffIdx, staff->getName(), (int)staff->getType(), staff->getCurrentCut(),
               cut != nullptr ? cut->getName() : "-", cut != nullptr ? (int)cut->getStartFlag(0) : -1,
               cut != nullptr ? (int)cut->getStartFlag(1) : -1, cut != nullptr ? (int)cut->getStartFlag(2) : -1,
               cut != nullptr ? (int)cut->getFlagId() : -1);
    }
}

} // namespace

void eventWatchFrame(unsigned int frames) {
    if (!dComIfGp_event_runCheck()) {
        sRunning = 0;
        sEventId = -1;
        return;
    }
    const s16 id = g_dComIfG_gameInfo.play.getEvent()->mEventId;
    if (id != sEventId) {
        sEventId = id;
        sRunning = 0;
    }
    ++sRunning;
    if (sReports < kMaxReports && sRunning >= kStuckFrames && (sRunning - kStuckFrames) % kRepeatFrames == 0) {
        sReports++;
        report(frames);
    }
}

} // namespace pc
