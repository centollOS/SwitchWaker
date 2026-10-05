// COS_SMOKE=event-sweep: every event of the stage's event list (Stage.arc event_list.dat, the
// dEvent_manager_c list) started and run to its end, one after the other. With the debug stage boot
// (COS_BOOT_STAGE), once the player is in the start room (the M12 probe, pc_outset.cpp) and no event
// runs (A pressed through the stage's start event), for each event index in turn:
//   1. the event is ordered as an actor orders its own (fopAcM_orderOtherEventId, the player as the
//      ordering actor, the event's own priority), again every frame until the event manager starts
//      it (kStartFrames at most, else "not-started"); the order is made inside the play scene's
//      layer, as everything the harness asks of the process tree;
//   2. while it runs, A every kTalkFrames (messages, choices take the first answer); the harness is
//      the ordering actor, so when the manager closes the event (endCheck, every finish flag set)
//      it resets it (dComIfGp_event_reset) as the actor would: "end". An event ended by the game
//      itself (an actor of the event reset it) is "end" too; one replaced by another event
//      "end-other";
//   3. an event that does not end within kEventSeconds of wall time / kEventFrames frames, or that
//      makes no progress (no staff advances its cut, no event flag, message status or demo frame
//      changes) for kIdleFrames frames and kIdleSeconds, is "stuck": the staff still waiting (their
//      current cut's flag unset, with that cut's name) are listed, and the event is reset. A staff of
//      type NORMAL whose actor is not in the stage when the event starts is listed as missing on the
//      begin line: an event waiting on one of those is a harness artifact (the debug boot does not
//      provide the actors, flags or story state the event was made for). The waiting staff are
//      written as <name>:<cut>:<staff type> (dEvDtStaff_c::StaffType_e: 0 an actor, 2 the camera,
//      7 the message, 11 a demo package, ...; <name>:><cut>:<type> when its cut ended and the next
//      one waits for its start flags);
//   4. an event that asks for a stage change (dComIfGp_isEnableNextStage) ends the run
//      ("stage-change <stage>"), as does a lost PLAY scene or player ("scene-lost"); the driver
//      (native/tools/event_sweep.py) boots again from the next index. Between events kGapFrames
//      without any event (A through an event the stage starts by itself, reset after kGapMaxFrames).
// COS_EVENT_SWEEP=<first>[-[<last>]][,...] limits the event indices (a list of indices and ranges;
// "<n>-" runs to the end). COS_EVENT_SWEEP_SECONDS overrides
// kEventSeconds. <COS_RUN_DIR>/event_sweep.txt gets a "begin" line before each order (written
// straight to the file, so it names the event when a fault ends the process), a result line after
// it and "done <count>" at the end (exit 0). Without an event list: "done 0".
#include "pc_internal.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"
#include "m_Do/m_Do_audio.h"

#include <dolphin/pad.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;     // after the player is in the room
constexpr unsigned int kStartEventFrames = 3600; // the stage's own start event, before the first order
constexpr unsigned int kStartFrames = 90;      // order -> the manager starts the event
constexpr unsigned int kTalkFrames = 20;       // A every kTalkFrames (held 2 frames)
constexpr double kEventSecondsDefault = 45.0;  // wall time per event (fanfares play in real time)
constexpr unsigned int kEventFrames = 9000;
constexpr unsigned int kIdleFrames = 900;      // no progress for this many frames ...
constexpr double kIdleSeconds = 8.0;           // ... and this much wall time: stuck
constexpr unsigned int kIdleFramesMissing = 240; // the same when every waiting staff's actor is
constexpr double kIdleSecondsMissing = 3.0;      // missing (it cannot advance)
constexpr float kPutBackDistance = 300.0f;       // the player moved by an event: put back
constexpr unsigned int kResetFrames = 120;     // reset -> no event running
constexpr unsigned int kGapFrames = 20;        // without any event between two events
constexpr unsigned int kGapMaxFrames = 1800;   // an event the stage started by itself: reset after

enum State { kOff, kWaitLink, kNext, kOrder, kRun, kReset, kGap, kDone };

State sState = kOff;
bool sChecked = false;
constexpr int kMaxRanges = 512;
int sRanges[kMaxRanges][2] = {{0, 0x7FFF}}; // COS_EVENT_SWEEP, inclusive
int sRangeCount = 1;
int sFirst = 0;
int sLast = 0x7FFF;
int sIdx = -1;
int sFd = -1;
unsigned int sSince = 0;      // frames in the current state
unsigned int sIdle = 0;       // frames without progress
uint64_t sIdleNs = 0;         // when the last progress was seen
uint64_t sStartNs = 0;        // the current event's order / start
uint64_t sProgress = 0;       // progress signature
double sEventSeconds = kEventSecondsDefault;
const char* sResult = nullptr;
char sDetail[1024];
unsigned int sRunFrames = 0;
unsigned int sCount = 0;
unsigned int sEnded = 0, sNotStarted = 0, sStuck = 0;
char sMissing[512];       // the current event's NORMAL staff without an actor, "|"-separated
cXyz sHomePos;            // the player when the sweep started
s16 sHomeAngle = 0;
unsigned int sPutBacks = 0; // in the current gap

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

dEvent_manager_c* manager() {
    return dComIfGp_getPEvtManager();
}

dEvt_control_c* control() {
    return g_dComIfG_gameInfo.play.getEvent();
}

int eventNum() {
    dEvent_manager_c* mng = manager();
    return mng->mList.getHeaderP() != nullptr ? mng->mList.getEventNum() : 0;
}

const char* eventName(int idx) {
    dEvDtEvent_c* ev = manager()->getEventData((s16)idx);
    return ev != nullptr ? ev->getName() : "?";
}

const char* runningName() {
    dEvt_control_c* ev = control();
    if (!dComIfGp_event_runCheck()) {
        return "(none)";
    }
    if (ev->mEventId == -1) {
        return "(no event data)";
    }
    return eventName(ev->mEventId);
}

// Appends to sDetail.
void detail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void detail(const char* fmt, ...) {
    size_t used = strlen(sDetail);
    if (used + 1 >= sizeof(sDetail)) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sDetail + used, sizeof(sDetail) - used, fmt, ap);
    va_end(ap);
}

struct CastSearch {
    dStage_objectNameInf* inf;
};

void* findCast(void* proc, void* data) {
    CastSearch* s = static_cast<CastSearch*>(data);
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(proc);
    return s->inf->procname == fopAcM_GetProfName(actor) && s->inf->argument == actor->argument ? actor
                                                                                              : nullptr;
}

// The NORMAL staff of event idx with no actor in the stage (as dEvent_manager_c::startProc looks
// for them: specialCast, then the dStage object name's profile and argument), "|"-separated.
void missingCast(int idx, char* out, size_t size) {
    out[0] = '\0';
    dEvent_manager_c* mng = manager();
    dEvDtEvent_c* ev = mng->getEventData((s16)idx);
    size_t used = 0;
    for (int i = 0; ev != nullptr && i < ev->getNStaff() && i < 20; i++) {
        dEvDtStaff_c* staff = mng->mList.getStaffP(ev->getStaff(i));
        if (staff->getType() != dEvDtStaff_c::NORMAL_e) {
            continue;
        }
        const char* name = staff->getName();
        if (strcmp(name, "SHUTTER_DOOR") == 0 && mng->specialCast(name, 0) != nullptr) {
            continue;
        }
        CastSearch s = {dStage_searchName(name)};
        if (s.inf != nullptr && fopAcM_Search((fopAcIt_JudgeFunc)findCast, &s) != nullptr) {
            continue;
        }
        int n = snprintf(out + used, size - used, "%s%s", used ? "|" : "", name);
        if (n < 0 || used + n >= size) {
            break;
        }
        used += n;
    }
}

// Every staff of the running event whose current cut has not ended is one without an actor.
bool onlyMissingWaiting() {
    if (sMissing[0] == '\0') {
        return false;
    }
    dEvent_manager_c* mng = manager();
    dEvDtEvent_c* ev = mng->getEventData(control()->mEventId);
    bool any = false;
    for (int i = 0; ev != nullptr && i < ev->getNStaff() && i < 20; i++) {
        const int staffIdx = ev->getStaff(i);
        dEvDtCut_c* cut = mng->mList.getCutStaffCurrentCutP(staffIdx);
        if (cut == nullptr || mng->getFlags().flagCheck(cut->getFlagId())) {
            continue;
        }
        const char* name = mng->mList.getStaffP(staffIdx)->getName();
        const size_t len = strlen(name);
        bool found = false;
        for (const char* p = sMissing; *p != '\0';) {
            const char* bar = strchr(p, '|');
            const size_t n = bar != nullptr ? (size_t)(bar - p) : strlen(p);
            found = found || (n == len && strncmp(p, name, n) == 0);
            p += n + (bar != nullptr ? 1 : 0);
        }
        if (!found) {
            return false;
        }
        any = true;
    }
    return any;
}

// The staff of the running event whose current cut has not ended (its flag unset), into sDetail;
// the same to stderr as pc_event_watch.cpp logs a stuck event.
void waitingStaff(unsigned int frames) {
    dEvent_manager_c* mng = manager();
    dEvt_control_c* ctl = control();
    dEvDtEvent_c* ev = mng->getEventData(ctl->mEventId);
    writef(STDERR_FILENO, "[cos] event-sweep: frame %u: event %d \"%s\" stuck after %u frames, mode %u, "
                          "message status %d, sub BGM 0x%x\n",
           frames, ctl->mEventId, ev != nullptr ? ev->getName() : "(no data)", sRunFrames, ctl->getMode(),
           (int)dComIfGp_getMesgStatus(), (unsigned)mDoAud_checkPlayingSubBgmFlag());
    if (ev == nullptr) {
        detail(" waiting=(no event data)");
        return;
    }
    detail(" waiting=");
    bool any = false;
    for (int i = 0; i < ev->getNStaff() && i < 20; i++) {
        const int staffIdx = ev->getStaff(i);
        dEvDtStaff_c* staff = mng->mList.getStaffP(staffIdx);
        dEvDtCut_c* cut = mng->mList.getCutStaffCurrentCutP(staffIdx);
        const bool done = cut == nullptr || mng->getFlags().flagCheck(cut->getFlagId());
        writef(STDERR_FILENO, "[cos] event-sweep:   staff %d \"%s\" type %d: cut %u \"%s\"%s, start flags %d %d %d, "
                              "flag %d\n", staffIdx, staff->getName(), (int)staff->getType(), staff->getCurrentCut(),
               cut != nullptr ? cut->getName() : "-", done ? " (ended)" : "",
               cut != nullptr ? (int)cut->getStartFlag(0) : -1, cut != nullptr ? (int)cut->getStartFlag(1) : -1,
               cut != nullptr ? (int)cut->getStartFlag(2) : -1, cut != nullptr ? (int)cut->getFlagId() : -1);
        if (!done) {
            detail("%s%s:%s:%d", any ? "|" : "", staff->getName(), cut->getName(), (int)staff->getType());
            any = true;
        } else if (cut != nullptr && (s32)cut->getNext() != -1) {
            // Its cut ended; the next one waits for its start flags (other staff's cuts).
            detail("%s%s:>%s:%d", any ? "|" : "", staff->getName(), mng->mList.getCutP(cut->getNext())->getName(),
                   (int)staff->getType());
            any = true;
        }
    }
    if (!any) {
        detail("(none:finish-flags-unset)");
    }
}

uint64_t progressSignature() {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    dEvent_manager_c* mng = manager();
    dEvt_control_c* ctl = control();
    mix((uint64_t)(uint16_t)ctl->mEventId);
    mix(ctl->getMode());
    dEvDtEvent_c* ev = mng->getEventData(ctl->mEventId);
    for (int i = 0; ev != nullptr && i < ev->getNStaff() && i < 20; i++) {
        mix((uint64_t)mng->mList.getStaffCurrentCut(ev->getStaff(i)));
    }
    const dEvDtFlag_c& flags = mng->getFlags();
    for (u32 w : flags.mFlag) {
        mix(w);
    }
    mix((uint64_t)dComIfGp_getMesgStatus());
    if (dDemo_manager_c* demo = dComIfGp_demo_get()) {
        mix((uint64_t)demo->getFrame());
    }
    return h;
}

// nullptr while the boot stage is still running with the player in it.
const char* sceneLost() {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return "PLAY-scene-gone";
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return "player-gone";
    }
    const PcBootStage* boot = pc_boot_stage();
    const char* stage = dComIfGp_getStartStageName();
    if (boot != nullptr && (stage == nullptr || strcmp(stage, boot->stage) != 0)) {
        return "left-the-stage";
    }
    return nullptr;
}

bool inRange(int idx) {
    for (int i = 0; i < sRangeCount; i++) {
        if (idx >= sRanges[i][0] && idx <= sRanges[i][1]) {
            return true;
        }
    }
    return false;
}

void parseRange() {
    const char* v = getenv("COS_EVENT_SWEEP");
    if (v != nullptr && *v != '\0') {
        sRangeCount = 0;
        const char* p = v;
        for (;;) {
            char* end = nullptr;
            long first = strtol(p, &end, 10);
            long last = first;
            bool ok = end != p;
            if (ok && *end == '-') {
                const char* q = end + 1;
                last = *q == '\0' || *q == ',' ? 0x7FFF : strtol(q, &end, 10);
                if (*q == '\0' || *q == ',') {
                    end = const_cast<char*>(q);
                }
            }
            ok = ok && (*end == '\0' || *end == ',') && first >= 0 && last >= first && last <= 0x7FFF &&
                 sRangeCount < kMaxRanges;
            if (!ok) {
                writef(STDERR_FILENO, "[cos] event-sweep: COS_EVENT_SWEEP=\"%s\" is not a list of "
                                      "<first>[-[<last>]]\n", v);
                pc_exit(PC_EXIT_USAGE);
            }
            sRanges[sRangeCount][0] = (int)first;
            sRanges[sRangeCount][1] = (int)last;
            sRangeCount++;
            if (*end == '\0') {
                break;
            }
            p = end + 1;
        }
        sFirst = 0x7FFF;
        sLast = 0;
        for (int i = 0; i < sRangeCount; i++) {
            sFirst = sRanges[i][0] < sFirst ? sRanges[i][0] : sFirst;
            sLast = sRanges[i][1] > sLast ? sRanges[i][1] : sLast;
        }
    }
    const char* s = getenv("COS_EVENT_SWEEP_SECONDS");
    if (s != nullptr && *s != '\0') {
        double secs = strtod(s, nullptr);
        if (secs <= 0) {
            writef(STDERR_FILENO, "[cos] event-sweep: COS_EVENT_SWEEP_SECONDS=\"%s\" is not > 0\n", s);
            pc_exit(PC_EXIT_USAGE);
        }
        sEventSeconds = secs;
    }
}

// Every harness call into the process tree from the play scene's layer (a process created from the
// root layer is drawn twice a frame; see pc_ky_procs.cpp and pc_chest.cpp).
struct PlayLayerScope {
    layer_class* saved;
    PlayLayerScope() : saved(fpcLy_CurrentLayer()) {
        if (void* play = fpcM_Search(isPlayScene, nullptr)) {
            fpcLy_SetCurrentLayer(&static_cast<process_node_class*>(play)->mLayer);
        }
    }
    ~PlayLayerScope() { fpcLy_SetCurrentLayer(saved); }
};

void order() {
    PlayLayerScope layer;
    fopAcM_orderOtherEventId(dComIfGp_getPlayer(0), (s16)sIdx);
}

double secondsSince(uint64_t ns) {
    return (monotonicNs() - ns) / 1e9;
}

void result(const char* what) {
    sResult = what;
    if (sFd >= 0) {
        writef(sFd, "%d %s %s %u %.1f%s\n", sIdx, eventName(sIdx), what, sRunFrames, secondsSince(sStartNs),
               sDetail);
    }
    writef(STDERR_FILENO, "[cos] event-sweep: event %d \"%s\": %s after %u frames, %.1f s%s\n", sIdx,
           eventName(sIdx), what, sRunFrames, secondsSince(sStartNs), sDetail);
    sDetail[0] = '\0';
    sCount++;
}

// An event moved the player (some put him in another room or at sea): back to where the sweep
// started, so the next event starts where the others did. daPy_lk_c::setPlayerPosAndAngle works only
// while an event runs; between events the position is written directly.
bool putBack() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr || player->current.pos.abs(sHomePos) <= kPutBackDistance) {
        return false;
    }
    writef(STDERR_FILENO, "[cos] event-sweep: the player at (%.0f, %.0f, %.0f) after event %d; put back%s\n",
           player->current.pos.x, player->current.pos.y, player->current.pos.z, sIdx,
           dComIfGp_event_runCheck() ? "" : " (no event)");
    if (dComIfGp_event_runCheck()) {
        static_cast<daPy_py_c*>(player)->setPlayerPosAndAngle(&sHomePos, sHomeAngle);
    } else {
        player->current.pos = sHomePos;
        player->old.pos = sHomePos;
        player->shape_angle.y = sHomeAngle;
        player->current.angle.y = sHomeAngle;
    }
    return true;
}

[[noreturn]] void stopRun(const char* why) {
    setDrivenPad(true, 0, 0, 0);
    if (sFd >= 0) {
        close(sFd);
        sFd = -1;
    }
    writef(STDERR_FILENO, "[cos] event-sweep: stopping after event %d: %s (%u events: %u ended, %u not "
                          "started, %u stuck)\n", sIdx, why, sCount, sEnded, sNotStarted, sStuck);
    pc_exit(PC_EXIT_REACHED);
}

// A stage change asked for by the running event (or anything else): the run ends here.
bool checkStageChange() {
    if (!dComIfGp_isEnableNextStage()) {
        return false;
    }
    detail(" next=%s", dComIfGp_getNextStageName());
    result("stage-change");
    stopRun("a stage change");
}

} // namespace

void eventSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "event-sweep") != 0) {
            return;
        }
        if (pc_boot_stage() == nullptr) {
            writef(STDERR_FILENO, "[cos] event-sweep: needs COS_BOOT_STAGE (e.g. --stage sea:44:206)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        parseRange();
        sFd = openRunFile("event_sweep.txt");
        setDrivenPad(true, 0, 0, 0);
        sState = kWaitLink;
    }
    if (sState == kOff || sState == kDone) {
        return;
    }
    sSince++;

    if (sState == kWaitLink) {
        if (!outsetLinkReady()) {
            sSince = 0;
            return;
        }
        // A through the stage's own start event first.
        static unsigned int sStartEvent = 0;
        if (dComIfGp_event_runCheck() && sStartEvent < kStartEventFrames) {
            sStartEvent++;
            setDrivenPad(true, sStartEvent % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince < kSettleFrames) {
            return;
        }
        if (dComIfGp_event_runCheck()) {
            writef(STDERR_FILENO, "[cos] event-sweep: the stage's event \"%s\" still runs; reset\n", runningName());
            dComIfGp_event_reset();
        }
        const int num = eventNum();
        if (sFd >= 0) {
            writef(sFd, "# event-sweep (COS_SMOKE=event-sweep) stage %s: %d events, indices %d-%d; <index> <name> "
                        "<result> <frames> <seconds> [details]; \"begin\" is written before each order\n",
                   pc_boot_stage()->stage, num, sFirst, sLast < num ? sLast : num - 1);
        }
        writef(STDERR_FILENO, "[cos] event-sweep: %d events in the stage's list; sweeping %d-%d from frame %u\n",
               num, sFirst, sLast < num ? sLast : num - 1, frames);
        if (sLast >= num) {
            sLast = num - 1;
        }
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        sHomePos = player->current.pos;
        sHomeAngle = player->shape_angle.y;
        sIdx = sFirst - 1;
        sState = kNext;
    }

    if (sState == kGap) {
        if (checkStageChange()) {
            return;
        }
        if (const char* lost = sceneLost()) {
            if (sFd >= 0) {
                writef(sFd, "%d %s scene-lost %u 0.0 why=%s\n", sIdx, eventName(sIdx), 0u, lost);
            }
            stopRun(lost);
        }
        if (dComIfGp_event_runCheck()) {
            // An event the stage started by itself (or our event's follow-up).
            setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sSince == kGapMaxFrames) {
                writef(STDERR_FILENO, "[cos] event-sweep: event \"%s\" runs between two events; reset\n",
                       runningName());
                dComIfGp_event_reset();
            } else if (sSince > kGapMaxFrames + kResetFrames) {
                if (sFd >= 0) {
                    writef(sFd, "%d %s scene-lost %u 0.0 why=event-%s-not-reset\n", sIdx, eventName(sIdx), 0u,
                           runningName());
                }
                stopRun("an event between two events cannot be reset");
            }
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sPutBacks < 3 && putBack()) {
            sPutBacks++;
            sSince = 0;
            return;
        }
        if (sSince < kGapFrames) {
            return;
        }
        sState = kNext;
    }

    if (sState == kNext) {
        do {
            sIdx++;
        } while (sIdx <= sLast && !inRange(sIdx));
        if (sIdx > sLast) {
            sState = kDone;
            setDrivenPad(true, 0, 0, 0);
            writef(STDERR_FILENO, "[cos] event-sweep: %u events: %u ended, %u not started, %u stuck\n", sCount,
                   sEnded, sNotStarted, sStuck);
            if (sFd >= 0) {
                writef(sFd, "done %u\n", sCount);
                close(sFd);
                sFd = -1;
            }
            pc_exit(PC_EXIT_REACHED);
        }
        sPutBacks = 0;
        char* missing = sMissing;
        missingCast(sIdx, sMissing, sizeof(sMissing));
        dEvDtEvent_c* ev = manager()->getEventData((s16)sIdx);
        if (sFd >= 0) {
            writef(sFd, "%d %s begin staff=%d missing=%s\n", sIdx, eventName(sIdx), ev != nullptr ? ev->getNStaff() : 0,
                   missing[0] ? missing : "-");
        }
        writef(STDERR_FILENO, "[cos] event-sweep: frame %u: event %d \"%s\" (missing cast: %s)\n", frames, sIdx,
               eventName(sIdx), missing[0] ? missing : "-");
        sDetail[0] = '\0';
        if (missing[0]) {
            detail(" missing=%s", missing);
        }
        sStartNs = monotonicNs();
        sRunFrames = 0;
        sSince = 0;
        sState = kOrder;
        order();
        return;
    }

    sRunFrames++;
    if (checkStageChange()) {
        return;
    }
    if (sState == kOrder) {
        dEvt_control_c* ctl = control();
        if (dComIfGp_event_runCheck() && ctl->mEventId == sIdx) {
            sState = kRun;
            sSince = 0;
            sIdle = 0;
            sIdleNs = monotonicNs();
            sProgress = progressSignature();
            return;
        }
        if (sSince >= kStartFrames) {
            if (dComIfGp_event_runCheck()) {
                detail(" running=%s", runningName());
            }
            sNotStarted++;
            result("not-started");
            sState = kGap;
            sSince = 0;
            return;
        }
        if (!dComIfGp_event_runCheck()) {
            order();
        } else {
            setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        }
        return;
    }

    if (sState == kRun) {
        dEvt_control_c* ctl = control();
        if (!dComIfGp_event_runCheck()) {
            sEnded++;
            result("end");
            sState = kGap;
            sSince = 0;
            return;
        }
        if (ctl->mEventId != sIdx) {
            detail(" next=%s", runningName());
            sEnded++;
            result("end-other");
            sState = kGap;
            sSince = 0;
            return;
        }
        if (dComIfGp_evmng_endCheck((s16)sIdx)) {
            // The harness ordered the event, so it ends it, as the ordering actor would.
            putBack();
            dComIfGp_event_reset();
            sEnded++;
            result("end");
            sState = kGap;
            sSince = 0;
            return;
        }
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        const uint64_t progress = progressSignature();
        if (progress != sProgress) {
            sProgress = progress;
            sIdle = 0;
            sIdleNs = monotonicNs();
        } else {
            sIdle++;
        }
        const bool cannot = onlyMissingWaiting();
        const bool idle = sIdle >= (cannot ? kIdleFramesMissing : kIdleFrames) &&
                          secondsSince(sIdleNs) >= (cannot ? kIdleSecondsMissing : kIdleSeconds);
        const bool late = secondsSince(sStartNs) >= sEventSeconds || sRunFrames >= kEventFrames;
        if (idle || late) {
            detail(" why=%s", idle ? "no-progress" : "timeout");
            waitingStaff(frames);
            sStuck++;
            result("stuck");
            putBack();
            dComIfGp_event_reset();
            setDrivenPad(true, 0, 0, 0);
            sState = kReset;
            sSince = 0;
        }
        return;
    }

    if (sState == kReset) {
        if (!dComIfGp_event_runCheck() || control()->mEventId != sIdx) {
            sState = kGap;
            sSince = 0;
            return;
        }
        if (sSince >= kResetFrames) {
            if (sFd >= 0) {
                writef(sFd, "%d %s scene-lost %u 0.0 why=reset-refused\n", sIdx, eventName(sIdx), 0u);
            }
            stopRun("the stuck event does not end after its reset");
        }
    }
}

} // namespace pc
