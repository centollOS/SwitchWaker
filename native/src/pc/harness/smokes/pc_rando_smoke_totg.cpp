// COS_SMOKE=rando COS_RANDO_SMOKE=totg (docs/RANDOMIZER_PLAN.md step R3.18): the Tower of the Gods
// servants tweak (features/rando/pc_rando_tweak_totg.h) in the hub room,
//   run.sh rando --stage Siren:7:0 --env COS_RANDO_SMOKE=totg [--env COS_RANDO_FORCE=1]
//                [--env COS_RANDO_TOTG_ORDER=2A,28,29]
//
// Once the play scene ran 30 frames:
//   1. files: with pc_rando() 1 each of the three files went through the patcher once and was
//      patched, and the results equal wwrando's (fixtures/R3_18_wwrando_totg.txt, written by
//      R3_18_make_fixture.py from wwrando's own tweak on the disc: event_list.dat byte for byte by
//      size and FNV-1a 64, stage.dzs TGDR+EVNT and room.dzr ACTR+SCOB by content hash); with
//      pc_rando() 0 the patcher never saw them;
//   2. what the game loaded: the event manager's view of the six events (staff cut counts, the
//      staffs removed and added, hsehi1_appear present or not), the two SwOp actors in the room and
//      the tablet's (Hsh) switch 0x2B (vanilla: no SwOp, 0x29);
//   3. rando only, the behaviour: with the servant switches 0x28-0x2A, 0x2B, 0x7E and 0x7F clear,
//      the smoke sets the three servant switches in the order of COS_RANDO_TOTG_ORDER (default
//      28,29,2A), 60 frames apart. The first one starts hsehi1_appear (the OR SwOp's event), which
//      sets 0x7E, reveals the tablet (its Appear cut sets event bit 0x2B10) and ends; the other two
//      start nothing more and 0x7F stays clear. Then the smoke sets 0x2B (the tablet's item taken)
//      and the AND SwOp sets 0x7F.
// The servants' own events (Os_Finish, Os1_Finish, Os2_Finish) need a servant carried in from its
// room and are not played here; their edited shape is checked through the event manager (2) and the
// file bytes (1).
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_tweak_totg.h"

#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "totg";

// fixtures/R3_18_wwrando_totg.txt (wwrando's output on the USA disc).
constexpr uint32_t kEvlSize = 199864;
constexpr uint64_t kEvlHash = 0x3217af53aa70b55eull;
constexpr uint64_t kDzsHash = 0x86a71b38dcc23d6dull;
constexpr uint64_t kDzrHash = 0x727d9995208c82c8ull;

constexpr unsigned int kStepFrames = 60;    // between two switches
constexpr unsigned int kEventFrames = 900;  // hsehi1_appear must start and end by then
constexpr unsigned int kWarpFrames = 60;    // 0x7F after 0x2B

enum State { kStatic, kFirst, kRest, kTablet, kDone };
State sState = kStatic;
unsigned int sSince = 0;
int sOrder[3] = {PCR_TOTG_SW_NORTH, PCR_TOTG_SW_EAST, PCR_TOTG_SW_WEST};
int sNext = 0;
s16 sAppearIdx = -1;
bool sAppearRan = false, sAppearEnded = false;
unsigned int sAppearRuns = 0;
bool sWasRunning = false;
int sRoom = 7;

bool isSw(int sw) {
    return dComIfGs_isSwitch(sw, sRoom) != 0;
}

dEvDtEvent_c* eventData(const char* name) {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    const s16 idx = mgr->getEventIdx(name, 0xFF);
    return idx >= 0 ? mgr->getEventData(idx) : nullptr;
}

// The number of staffs named `staff` in an event, and the first one.
int staffCount(dEvDtEvent_c* ev, const char* staff, dEvDtStaff_c** first) {
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    int n = 0;
    *first = nullptr;
    for (int i = 0; i < ev->getNStaff(); i++) {
        dEvDtStaff_c* s = list.getStaffP(ev->getStaff(i));
        if (strcmp(s->getName(), staff) == 0) {
            if (n++ == 0) {
                *first = s;
            }
        }
    }
    return n;
}

// Follows a staff's cut chain as dEvent_manager_c does: the count (-1 when broken) and the names
// joined by ',' (into names).
int cutChain(dEvDtStaff_c* s, char* names, size_t cap) {
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    s32 cut = (s32)s->getStartCut();
    int n = 0;
    size_t used = 0;
    names[0] = 0;
    while (cut >= 0) {
        if (cut >= list.getCutNum() || n > 64) {
            return -1;
        }
        dEvDtCut_c* c = list.getCutP(cut);
        const int w = snprintf(names + used, cap - used, "%s%.32s", n ? "," : "", c->getName());
        if (w > 0 && (size_t)w < cap - used) {
            used += (size_t)w;
        }
        n++;
        cut = (s32)c->getNext();
    }
    return n;
}

// An event's staff: present `count` times and with `cuts` cuts (cuts < 0: not checked); when names
// is not null, the cut names must equal it.
void checkStaff(const char* event, const char* staff, int count, int cuts, const char* names = nullptr) {
    char what[160];
    dEvDtEvent_c* ev = eventData(event);
    if (ev == nullptr) {
        snprintf(what, sizeof what, "%s is in the event list", event);
        randoSmokeCheck(kName, what, false);
        return;
    }
    dEvDtStaff_c* s = nullptr;
    const int n = staffCount(ev, staff, &s);
    char chain[512] = "";
    const int c = s != nullptr ? cutChain(s, chain, sizeof chain) : 0;
    writef(STDERR_FILENO, "[cos] rando-smoke totg: %s %s x%d, %d cuts: %s\n", event, staff, n, c, chain);
    snprintf(what, sizeof what, "%s: %d staff %s", event, count, staff);
    randoSmokeCheck(kName, what, n == count);
    if (n > 0 && cuts >= 0) {
        snprintf(what, sizeof what, "%s %s: %d cuts%s%s", event, staff, cuts, names ? " " : "", names ? names : "");
        randoSmokeCheck(kName, what, c == cuts && (names == nullptr || strcmp(chain, names) == 0));
    }
}

struct Count {
    s16 proc;
    int n;
    fopAc_ac_c* first;
};

void* countProc(void* proc, void* data) {
    Count* c = static_cast<Count*>(data);
    if (fpcM_GetName(proc) == c->proc) {
        if (c->n++ == 0) {
            c->first = static_cast<fopAc_ac_c*>(proc);
        }
    }
    return nullptr;
}

Count countActors(s16 proc) {
    Count c = {proc, 0, nullptr};
    fopAcM_Search(countProc, &c);
    return c;
}

void checkFiles() {
    PcRandoTotgStatus dzs, dzr, evl;
    pc_rando_tweak_totg_status(PCR_TOTG_DZS, &dzs);
    pc_rando_tweak_totg_status(PCR_TOTG_DZR, &dzr);
    pc_rando_tweak_totg_status(PCR_TOTG_EVL, &evl);
    writef(STDERR_FILENO,
           "[cos] rando-smoke totg: status dzs %d/%d %016llx, dzr %d/%d %016llx, evl %d/%d %u %016llx, EVNT index %d\n",
           dzs.applied, dzs.seen, (unsigned long long)dzs.hash, dzr.applied, dzr.seen, (unsigned long long)dzr.hash,
           evl.applied, evl.seen, evl.size, (unsigned long long)evl.hash, pc_rando_tweak_totg_evnt_index());
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: the patcher never saw the files", dzs.seen == 0 && dzr.seen == 0 && evl.seen == 0);
        return;
    }
    randoSmokeCheck(kName, "stage.dzs patched once", dzs.seen == 1 && dzs.applied == 1);
    randoSmokeCheck(kName, "Room7 room.dzr patched once", dzr.seen == 1 && dzr.applied == 1);
    randoSmokeCheck(kName, "event_list.dat patched once", evl.seen == 1 && evl.applied == 1);
    randoSmokeCheck(kName, "stage.dzs TGDR+EVNT = wwrando's", dzs.hash == kDzsHash);
    randoSmokeCheck(kName, "Room7 room.dzr ACTR+SCOB = wwrando's", dzr.hash == kDzrHash);
    randoSmokeCheck(kName, "event_list.dat = wwrando's byte for byte", evl.size == kEvlSize && evl.hash == kEvlHash);
    randoSmokeCheck(kName, "hsehi1_appear is EVNT 8", pc_rando_tweak_totg_evnt_index() == 8);
}

void checkLoaded() {
    const Count swops = countActors(fpcNm_SWITCH_OP_e);
    const Count hsh = countActors(fpcNm_Obj_HSH_e);
    const u32 hshSw = hsh.first != nullptr ? (fopAcM_GetParam(hsh.first) & 0xFF) : 0xFFFFFFFFu;
    writef(STDERR_FILENO, "[cos] rando-smoke totg: %d SwOp, %d Hsh (switch 0x%X), stay room %d\n", swops.n, hsh.n,
           (unsigned)hshSw, (int)dComIfGp_roomControl_getStayNo());
    randoSmokeCheck(kName, "booted in Siren room 7",
                    strcmp(dComIfGp_getStartStageName(), "Siren") == 0 && dComIfGp_roomControl_getStayNo() == 7);
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: no SwOp", swops.n == 0);
        randoSmokeCheck(kName, "vanilla: the tablet sets 0x29", hsh.n == 1 && hshSw == PCR_TOTG_SW_EAST);
        randoSmokeCheck(kName, "vanilla: no hsehi1_appear", eventData(PCR_TOTG_APPEAR_EVENT) == nullptr);
        checkStaff("Os_Finish", "Os", 1, 9);
        checkStaff("Os_Finish", "Hsh", 1, 6);
        checkStaff("Os_Finish", "SwOp", 0, -1);
        checkStaff("Os2_Finish", "Os", 1, 3);
        checkStaff("hsehi1_tact", "Hsh", 1, 17);
        return;
    }
    randoSmokeCheck(kName, "two SwOp in the room", swops.n == 2);
    randoSmokeCheck(kName, "the tablet sets 0x2B", hsh.n == 1 && hshSw == PCR_TOTG_SW_TABLET);
    // East servant: SW_ON inserted at 7, no tablet, SwOp DUMMY.
    checkStaff("Os_Finish", "Os", 1, 10, "WAIT,MOVE,MOVEEND,TURN,WAIT,FINISH,WAIT,SW_ON,WAIT,END");
    checkStaff("Os_Finish", "Hsh", 0, -1);
    checkStaff("Os_Finish", "TIMEKEEPER", 1, 8);
    checkStaff("Os_Finish", "CAMERA", 1, 6);
    checkStaff("Os_Finish", "SwOp", 1, 1, "DUMMY");
    // West servant.
    checkStaff("Os1_Finish", "Os1", 1, 9, "WAIT,MOVE,MOVEEND,TURN,WAIT,WAIT,FINISH,SW_ON,END");
    checkStaff("Os1_Finish", "CAMERA", 1, 6);
    checkStaff("Os1_Finish", "SwOp", 1, 1, "DUMMY");
    checkStaff("Os1_Message", "Os1", 1, 1, "WAIT");
    checkStaff("Os1_Message", "CAMERA", 1, 1, "PAUSE");
    // North servant: no east and west servants.
    checkStaff("Os2_Finish", "Os", 0, -1);
    checkStaff("Os2_Finish", "Os1", 0, -1);
    checkStaff("Os2_Finish", "Os2", 1, 9, "WAIT,MOVE,MOVEEND,TURN,WAIT,WAIT,FINISH,SW_ON,END");
    checkStaff("Os2_Finish", "CAMERA", 1, 5);
    checkStaff("Os2_Finish", "SwOp", 1, 1, "DUMMY");
    // The tablet's event: it disappears at the end.
    checkStaff("hsehi1_tact", "Hsh", 1, 19);
    checkStaff("hsehi1_tact", "CAMERA", 1, 8);
    checkStaff("hsehi1_tact", "SwOp", 1, 1, "DUMMY");
    // The new event.
    checkStaff(PCR_TOTG_APPEAR_EVENT, "CAMERA", 1, 2, "FIXEDFRM,PAUSE");
    checkStaff(PCR_TOTG_APPEAR_EVENT, "Hsh", 1, 3, "WAIT,Appear,WAIT");
    checkStaff(PCR_TOTG_APPEAR_EVENT, "Link", 1, 1, "001n_wait");
    checkStaff(PCR_TOTG_APPEAR_EVENT, "TIMEKEEPER", 1, 2, "WAIT,COUNTDOWN");
}

void parseOrder() {
    const char* env = getenv("COS_RANDO_TOTG_ORDER");
    if (env == nullptr || env[0] == 0) {
        return;
    }
    int got[3];
    if (sscanf(env, "%x,%x,%x", &got[0], &got[1], &got[2]) != 3) {
        randoSmokeCheck(kName, "COS_RANDO_TOTG_ORDER is three hex switches", false);
        randoSmokeEnd(kName);
    }
    int mask = 0;
    for (int i = 0; i < 3; i++) {
        if (got[i] < PCR_TOTG_SW_NORTH || got[i] > PCR_TOTG_SW_WEST) {
            randoSmokeCheck(kName, "COS_RANDO_TOTG_ORDER holds 28, 29 and 2A", false);
            randoSmokeEnd(kName);
        }
        mask |= 1 << (got[i] - PCR_TOTG_SW_NORTH);
        sOrder[i] = got[i];
    }
    if (mask != 7) {
        randoSmokeCheck(kName, "COS_RANDO_TOTG_ORDER holds 28, 29 and 2A once each", false);
        randoSmokeEnd(kName);
    }
}

// Follows hsehi1_appear: started, ended, how often.
void watchAppear() {
    dEvt_control_c* evc = g_dComIfG_gameInfo.play.getEvent();
    const bool running = dComIfGp_event_runCheck() && evc->mEventId == sAppearIdx;
    if (running && !sWasRunning) {
        sAppearRuns++;
        sAppearRan = true;
        writef(STDERR_FILENO, "[cos] rando-smoke totg: frame %u hsehi1_appear starts (run %u); 0x7E %d\n", sSince,
               sAppearRuns, (int)isSw(PCR_TOTG_SW_ANY));
    }
    if (!running && sWasRunning) {
        sAppearEnded = true;
        writef(STDERR_FILENO, "[cos] rando-smoke totg: frame %u hsehi1_appear ended; event bit 0x2B10 %d\n", sSince,
               (int)dComIfGs_isEventBit(dSv_event_flag_c::UNK_2B10));
    }
    sWasRunning = running;
}

void setServant() {
    const int sw = sOrder[sNext++];
    writef(STDERR_FILENO, "[cos] rando-smoke totg: frame %u servant switch 0x%X on\n", sSince, sw);
    dComIfGs_onSwitch(sw, sRoom);
}

} // namespace

void randoSmokeTotg(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(30)) {
        return;
    }
    switch (sState) {
    case kStatic: {
        parseOrder();
        sRoom = dComIfGp_roomControl_getStayNo();
        checkFiles();
        checkLoaded();
        if (!pc_rando()) {
            randoSmokeEnd(kName);
        }
        const bool clear = !isSw(0x28) && !isSw(0x29) && !isSw(0x2A) && !isSw(0x2B) && !isSw(PCR_TOTG_SW_ANY) &&
                           !isSw(PCR_TOTG_SW_ALL) && !dComIfGs_isEventBit(dSv_event_flag_c::UNK_2B10);
        randoSmokeCheck(kName, "switches 0x28-0x2B, 0x7E, 0x7F and event bit 0x2B10 start clear", clear);
        sAppearIdx = dComIfGp_evmng_getEventIdx(PCR_TOTG_APPEAR_EVENT);
        if (!randoSmokeCheck(kName, "hsehi1_appear has an event index", sAppearIdx >= 0) || !clear) {
            randoSmokeEnd(kName);
        }
        sSince = 0;
        setServant();
        sState = kFirst;
        return;
    }
    case kFirst:
        sSince++;
        watchAppear();
        if (sAppearEnded) {
            randoSmokeCheck(kName, "the first servant switch starts hsehi1_appear", sAppearRan);
            randoSmokeCheck(kName, "0x7E is set", isSw(PCR_TOTG_SW_ANY));
            randoSmokeCheck(kName, "the tablet appeared (event bit 0x2B10)", dComIfGs_isEventBit(dSv_event_flag_c::UNK_2B10));
            randoSmokeCheck(kName, "0x7F is still clear", !isSw(PCR_TOTG_SW_ALL));
            sState = kRest;
            sSince = 0;
            return;
        }
        if (sSince > kEventFrames) {
            randoSmokeCheck(kName, sAppearRan ? "hsehi1_appear ends" : "the first servant switch starts hsehi1_appear", false);
            randoSmokeEnd(kName);
        }
        return;
    case kRest:
        sSince++;
        watchAppear();
        if (sSince % kStepFrames == 0) {
            if (sNext < 3) {
                setServant();
                return;
            }
            randoSmokeCheck(kName, "the other two servant switches start nothing more", sAppearRuns == 1 && !sWasRunning);
            randoSmokeCheck(kName, "0x7F stays clear while the tablet's item is not taken", !isSw(PCR_TOTG_SW_ALL));
            writef(STDERR_FILENO, "[cos] rando-smoke totg: frame %u tablet switch 0x2B on\n", sSince);
            dComIfGs_onSwitch(PCR_TOTG_SW_TABLET, sRoom);
            sState = kTablet;
            sSince = 0;
        }
        return;
    case kTablet:
        sSince++;
        if (isSw(PCR_TOTG_SW_ALL)) {
            writef(STDERR_FILENO, "[cos] rando-smoke totg: 0x7F set %u frames after 0x2B\n", sSince);
            randoSmokeCheck(kName, "all servants back and the tablet's item taken set 0x7F", true);
            randoSmokeCheck(kName, "hsehi1_appear ran once in all", sAppearRuns == 1);
            randoSmokeEnd(kName);
        }
        if (sSince > kWarpFrames) {
            randoSmokeCheck(kName, "all servants back and the tablet's item taken set 0x7F", false);
            randoSmokeEnd(kName);
        }
        return;
    case kDone:
        return;
    }
}

} // namespace pc
