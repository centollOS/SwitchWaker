// COS_SMOKE=rando COS_RANDO_SMOKE=entrsweep (docs/RANDOMIZER_PLAN.md step R6.14): the entrance sweep's
// in-game driver. Every entrance of a plan is used in both directions, and a save-and-quit inside its
// exit reloads where wwrando says it does.
//   native/tools/run.sh rando --stage <first entrance's stage:room:spawn> \
//       --env COS_RANDO_SMOKE=entrsweep --env COS_RANDO_PLACEMENT=<placement json> \
//       --env COS_ENTRSWEEP_PLAN=<plan file> [--env COS_ENTRSWEEP_ONLY=<i>,<j>,...]
// native/tools/rando/entrance_sweep.py writes the plan from wwrando's own tables (ZoneEntrance /
// ZoneExit, island numbers) and the disc (the dungeons' stage.dzs SCLS[0]) and runs this smoke over
// it; the expectations are never computed by the port's code under test. One line per entrance:
//   <entrance>|<exit>|E <stage> <room> <scls> <spawn>|X <stage> <room> <scls> <spawn>|
//   B <scls|warp|helmaroc> <stage> <room> <spawn>|S <stage> <room> <spawn>   (S - : no save check)
// E is the entrance (its room's SCLS[scls] is the door, spawn is where leaving the exit puts the
// player), X the exit it leads to (room's SCLS[scls] the way out; boss arenas: -1), B where leaving X
// goes (an SCLS exit; a boss arena's WARP_WIND_AFTER warp; Helmaroc King's M2tower room 0 SCLS[1])
// and S where a save inside X reloads. Lines starting with '#' are comments.
//
// Per entrance i, in plan order (scene changes as the doors make them, dComIfGp_setNextStage):
//  0. the world (prepareWorld): saved switches a player who can use these doors has (Stone Watcher
//     Island's stone head lifted);
//  1. to E (the first one is the boot stage when the run boots there): the play scene runs on E's
//     stage and room;
//  2. forward: E's live room SCLS[scls] leads to X (stage, room, spawn); the harness takes that exit
//     as the door does (dLib_setNextStageBySclsNum) and the next stage requested is X; the play scene
//     then runs on X's stage and room, at X's spawn, and no stage change comes for kStayFrames
//     (nothing throws the player out: a Fire Mountain / Ice Ring Isle VolTag, a bad spawn);
//  3. save-and-quit (S given): with RODE_KORL set (dComIfGs_setGameStartStage's first case; the
//     debug boot's file may lack it), the save menu's steps (dComIfGs_putSave, then
//     dComIfGs_setGameStartStage) put the return place at S; then the save goes through the card
//     buffer (dComIfGs_setMemoryToCard), the title turns the randomizer off, the slot is loaded back
//     (dComIfGs_setCardToMemory, R0.7's load hook) with the same seed, and dComIfGs_gameStart (the
//     file select's start) reaches S's stage and room; then back to X (a hop) for 4;
//  4. back: kind scls: X's live room SCLS[scls] leads to B and taking it reaches B; warp: the arena's
//     WARP_WIND_AFTER (ordered from the player, stuck cuts ended as in the R6.13 smoke) requests B
//     and reaches it; helmaroc: M2tower room 0's live SCLS[1] leads to B and taking it reaches B;
//     then the player stays there kStayFrames as in 2.
// Every check line names the entrance ("[cos] rando-smoke entrsweep: <i> <entrance>: <what>: ok|FAIL"),
// which the sweep parses. A stage that is not reached fails that check and skips the rest of the
// entrance. Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_seedfile.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "d/d_lib.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "entrsweep";
constexpr unsigned int kSettle = 30;          // frames on the arrival stage before checks
constexpr unsigned int kArriveFrames = 1800;  // to reach a requested stage
constexpr unsigned int kWarpStartFrames = 900; // WARP_WIND_AFTER to start (an arena intro may run first)
constexpr unsigned int kWarpFrames = 3000;    // WARP_WIND_AFTER to request the next stage
constexpr unsigned int kStuckFrames = 150;
constexpr unsigned int kStayFrames = 180;     // after the forward and back arrivals: no stage change
constexpr int kMaxStaff = 32;

struct Place {
    std::string stage;
    int room = -1;
    int scls = -1;
    int spawn = -1;
};

enum class Back { Scls, Warp, Helmaroc };

struct Entry {
    std::string entrance, exit;
    Place e, x, b, s;
    Back back = Back::Scls;
    bool save = false;
};

enum State { kInit, kArrive, kStay, kWarpOrder, kWarpRun, kRecover, kDone };
enum Step { kToE, kForward, kSaveQuit, kToX, kBack };

State sState = kInit;
Step sStep = kToE;
std::vector<Entry> sPlan;
std::vector<int> sOnly;
size_t sIdx = 0;
unsigned int sSince = 0;
unsigned int sPresent = 0;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
Place sWant; // the stage being travelled to
bool sWantSpawn = false;
uint8_t sHash[16];
bool sHaveHash = false;
int sEntriesDone = 0;
int sEntriesFailed = 0;
bool sCurFailed = false;
u8 sCard[3 * sizeof(card_gamedata)];

// WARP_WIND_AFTER (as the R6.13 smoke drives it)
s16 sEventIdx = -1;
int sDirector = -1;
int sStaffCut[kMaxStaff];
unsigned int sStaffSince[kMaxStaff];

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    writef(STDERR_FILENO, "[cos] rando-smoke entrsweep: %s\n", buf);
}

void abandon();
bool sStayed = false;

const Entry& cur() {
    return sPlan[sIdx];
}

// A check of the current entrance: "<i> <entrance>: <what>".
bool check(const char* what, bool ok) {
    char buf[400];
    snprintf(buf, sizeof buf, "%zu %s: %s", sIdx, cur().entrance.c_str(), what);
    randoSmokeCheck(kName, buf, ok);
    if (!ok) {
        sCurFailed = true;
    }
    return ok;
}

void* findPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(findPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

struct PlayLayerScope {
    layer_class* saved;
    PlayLayerScope() : saved(fpcLy_CurrentLayer()) {
        if (void* play = fpcM_Search(findPlayScene, nullptr)) {
            fpcLy_SetCurrentLayer(&static_cast<process_node_class*>(play)->mLayer);
        }
    }
    ~PlayLayerScope() { fpcLy_SetCurrentLayer(saved); }
};

std::string placeStr(const Place& p) {
    char buf[64];
    snprintf(buf, sizeof buf, "%s room %d spawn %d", p.stage.c_str(), p.room, p.spawn);
    return buf;
}

// --- the plan ---

bool parsePlace(const std::string& field, char tag, Place& p, bool withScls) {
    char t = 0;
    char stage[16] = {};
    int a = 0, b = 0, c = 0;
    if (withScls) {
        if (sscanf(field.c_str(), " %c %15s %d %d %d", &t, stage, &a, &b, &c) != 5 || t != tag) {
            return false;
        }
        p.stage = stage;
        p.room = a;
        p.scls = b;
        p.spawn = c;
        return true;
    }
    if (sscanf(field.c_str(), " %c %15s %d %d", &t, stage, &a, &b) != 4 || t != tag) {
        return false;
    }
    p.stage = stage;
    p.room = a;
    p.spawn = b;
    return true;
}

bool parseLine(const std::string& text, Entry& out) {
    std::vector<std::string> f;
    size_t start = 0;
    for (;;) {
        const size_t bar = text.find('|', start);
        f.push_back(text.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (bar == std::string::npos) {
            break;
        }
        start = bar + 1;
    }
    if (f.size() != 6) {
        return false;
    }
    out.entrance = f[0];
    out.exit = f[1];
    if (!parsePlace(f[2], 'E', out.e, true) || !parsePlace(f[3], 'X', out.x, true)) {
        return false;
    }
    char t = 0;
    char kind[16] = {}, stage[16] = {};
    int room = 0, spawn = 0;
    if (sscanf(f[4].c_str(), " %c %15s %15s %d %d", &t, kind, stage, &room, &spawn) != 5 || t != 'B') {
        return false;
    }
    if (strcmp(kind, "scls") == 0) {
        out.back = Back::Scls;
    } else if (strcmp(kind, "warp") == 0) {
        out.back = Back::Warp;
    } else if (strcmp(kind, "helmaroc") == 0) {
        out.back = Back::Helmaroc;
    } else {
        return false;
    }
    out.b.stage = stage;
    out.b.room = room;
    out.b.spawn = spawn;
    char dash[4] = {};
    if (sscanf(f[5].c_str(), " S %3s", dash) == 1 && strcmp(dash, "-") == 0) {
        out.save = false;
        return true;
    }
    out.save = parsePlace(f[5], 'S', out.s, false);
    return out.save;
}

bool loadPlan() {
    const char* path = getenv("COS_ENTRSWEEP_PLAN");
    if (path == nullptr || path[0] == '\0') {
        line("COS_ENTRSWEEP_PLAN is not set");
        return false;
    }
    FILE* fp = fopen(path, "r");
    if (fp == nullptr) {
        line("cannot open the plan %s", path);
        return false;
    }
    std::vector<Entry> all;
    char buf[512];
    bool ok = true;
    while (fgets(buf, sizeof buf, fp) != nullptr) {
        std::string s = buf;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
            s.pop_back();
        }
        if (s.empty() || s[0] == '#') {
            continue;
        }
        Entry e;
        if (!parseLine(s, e)) {
            line("bad plan line: %s", s.c_str());
            ok = false;
            continue;
        }
        all.push_back(e);
    }
    fclose(fp);
    const char* only = getenv("COS_ENTRSWEEP_ONLY");
    if (only != nullptr && only[0] != '\0') {
        std::string list = only;
        size_t start = 0;
        while (start <= list.size()) {
            const size_t comma = list.find(',', start);
            const std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            const int i = atoi(item.c_str());
            if (item.empty() || i < 0 || i >= (int)all.size()) {
                line("bad COS_ENTRSWEEP_ONLY entry \"%s\" (%zu plan lines)", item.c_str(), all.size());
                ok = false;
            } else {
                sOnly.push_back(i);
                sPlan.push_back(all[(size_t)i]);
            }
            if (comma == std::string::npos) {
                break;
            }
            start = comma + 1;
        }
    } else {
        for (size_t i = 0; i < all.size(); i++) {
            sOnly.push_back((int)i);
        }
        sPlan = all;
    }
    return ok && !sPlan.empty();
}

// --- travel ---

void travel(const Place& to, bool checkSpawn) {
    sOldScene = playSceneId();
    sWant = to;
    sWantSpawn = checkSpawn;
    sState = kArrive;
    sSince = 0;
    sPresent = 0;
}

void hop(const Place& to) {
    line("frame %u: entrance %zu: hop to %s", pc_frame_count(), sIdx, placeStr(to).c_str());
    travel(to, true);
    dComIfGp_setNextStage(to.stage.c_str(), (s16)to.spawn, (s8)to.room, -1);
}

// The live SCLS entry `idx` of room `room` (-1: the stage's).
const stage_scls_info_class* liveScls(int room, int idx) {
    stage_scls_info_dummy_class* info = nullptr;
    if (room < 0) {
        info = dComIfGp_getStageSclsInfo();
    } else if (dComIfGp_roomControl_getStatusRoomDt(room) != nullptr) {
        info = dComIfGp_roomControl_getStatusRoomDt(room)->getSclsInfo();
    }
    if (info == nullptr || idx < 0 || idx >= info->num) {
        return nullptr;
    }
    return &info->m_entries[idx];
}

bool sclsLeadsTo(const stage_scls_info_class* e, const Place& p) {
    if (e == nullptr) {
        return false;
    }
    char dest[9] = {};
    memcpy(dest, e->mStage, 8);
    return p.stage == dest && (s8)e->mRoom == p.room && e->mStart == p.spawn;
}

std::string sclsStr(const stage_scls_info_class* e) {
    if (e == nullptr) {
        return "(none)";
    }
    char dest[9] = {};
    memcpy(dest, e->mStage, 8);
    char buf[64];
    snprintf(buf, sizeof buf, "%s room %d spawn %d", dest, (s8)e->mRoom, e->mStart);
    return buf;
}

std::string nextStr() {
    char buf[64];
    snprintf(buf, sizeof buf, "%s room %d spawn %d", dComIfGp_getNextStageName(), dComIfGp_getNextStageRoomNo(),
             dComIfGp_getNextStagePoint());
    return buf;
}

bool nextIs(const Place& p) {
    return dComIfGp_isEnableNextStage() && p.stage == dComIfGp_getNextStageName() &&
           dComIfGp_getNextStageRoomNo() == p.room && dComIfGp_getNextStagePoint() == p.spawn;
}

// Takes SCLS[idx] of `room` (the door's own call) and checks the request.
void takeScls(int room, int idx, const Place& to, const char* what) {
    const stage_scls_info_class* e = liveScls(room, idx);
    char buf[200];
    snprintf(buf, sizeof buf, "%s: live room %d SCLS[%d] leads to %s (is %s)", what, room, idx, placeStr(to).c_str(),
             sclsStr(e).c_str());
    check(buf, sclsLeadsTo(e, to));
    if (e == nullptr) {
        abandon();
        return;
    }
    {
        PlayLayerScope layer;
        dLib_setNextStageBySclsNum((u8)idx, (s8)room);
    }
    snprintf(buf, sizeof buf, "%s: the next stage requested is %s (is %s)", what, placeStr(to).c_str(), nextStr().c_str());
    check(buf, nextIs(to));
    travel(to, true);
}

void nextEntry();

// The current entrance gave up (a stage not reached, an exit missing).
// A stage change still under way (a throw-out the stay check caught) settles first, so the next hop
// is not raced by it (kRecover).
void abandon() {
    sCurFailed = true;
    sStayed = false;
    sState = kRecover;
    sSince = 0;
    sPresent = 0;
}

// --- save and quit ---

bool sameHash(const PcRandoSeed* s) {
    return sHaveHash && s != nullptr && memcmp(s->hash, sHash, 16) == 0;
}

void saveAndQuit() {
    const Entry& en = cur();
    if (!dComIfGs_isEventBit(dSv_event_flag_c::RODE_KORL)) {
        dComIfGs_onEventBit(dSv_event_flag_c::RODE_KORL);
    }
    // dMenu_save_c::memCardDataSave
    dComIfGs_putSave(dStage_stagInfo_GetSaveTbl(dComIfGp_getStage().getStagInfo()));
    dComIfGs_setGameStartStage();
    dSv_player_return_place_c& rp = g_dComIfG_gameInfo.save.getPlayer().getPlayerReturnPlace();
    char name[9] = {};
    memcpy(name, rp.getName(), 8);
    char buf[200];
    snprintf(buf, sizeof buf, "save: the return place is %s (is %s room %d spawn %d)", placeStr(en.s).c_str(), name,
             rp.getRoomNo(), rp.getPoint());
    check(buf, en.s.stage == name && rp.getRoomNo() == en.s.room && rp.getPoint() == en.s.spawn);
    // the card, the title, the file select's load
    memset(sCard, 0, sizeof sCard);
    dComIfGs_setMemoryToCard(sCard, 0);
    mDoMemCdRWm_SetCheckSumGameData(sCard, 0);
    pc_rando_slot_scene_entry();
    const bool off = pc_rando() == 0;
    dComIfGs_setCardToMemory(sCard, 0);
    pc_rando_slot_card_loaded();
    check("quit: the title turned the randomizer off and the reloaded slot is randomized with the same seed",
          off && pc_rando() == 1 && sameHash(pc_rando_seed()));
    line("frame %u: entrance %zu: reload at %s", pc_frame_count(), sIdx, placeStr(en.s).c_str());
    travel(en.s, true);
    dComIfGs_gameStart();
}

// --- WARP_WIND_AFTER (the R6.13 smoke's driver) ---

bool warpInit() {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtBase_c& list = mgr->mList;
    sEventIdx = mgr->getEventIdx("WARP_WIND_AFTER", 0xFF);
    dEvDtEvent_c* ev = mgr->getEventData(sEventIdx);
    if (!check("back: the arena has WARP_WIND_AFTER", ev != nullptr)) {
        return false;
    }
    sDirector = -1;
    for (int i = 0; i < ev->getNStaff(); i++) {
        if (strcmp(list.getStaffP(ev->getStaff(i))->getName(), "DIRECTOR") == 0) {
            sDirector = ev->getStaff(i);
        }
    }
    for (int i = 0; i < kMaxStaff; i++) {
        sStaffCut[i] = -1;
        sStaffSince[i] = 0;
    }
    return true;
}

void warpOrder() {
    PlayLayerScope layer;
    fopAcM_orderOtherEventId(dComIfGp_getPlayer(0), sEventIdx, 0xFF, 0xFFFF, 1);
}

void driveStuckCuts() {
    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtEvent_c* ev = mgr->getEventData(sEventIdx);
    if (ev == nullptr) {
        return;
    }
    dEvDtBase_c& list = mgr->mList;
    for (int i = 0; i < ev->getNStaff() && i < kMaxStaff; i++) {
        const int staffIdx = ev->getStaff(i);
        if (staffIdx == sDirector) {
            continue;
        }
        dEvDtStaff_c* staff = list.getStaffP(staffIdx);
        const int c = (int)staff->getCurrentCut();
        if (c != sStaffCut[i]) {
            sStaffCut[i] = c;
            sStaffSince[i] = 0;
            continue;
        }
        if (++sStaffSince[i] < kStuckFrames) {
            continue;
        }
        sStaffSince[i] = 0;
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

// --- the world a player who used these doors has ---

// Saved switches a real file has once the player can use an entrance, which the debug boot's new file
// lacks. Without them the harness's arrival is not where a player arrives:
//   - sea switch 0x2B: Stone Watcher Island's stone head (Ekao, d_a_stone2 swSave) lifted. Spawn 1
//     (the cave's way out) is inside the head; with the head down the arrival bounces to spawn 0
//     (vanilla does the same: R0_6_identity's sweep).
struct SaveSwitch {
    int stageNo;
    int sw;
};
constexpr SaveSwitch kSaveSwitches[] = {
    {dSv_save_c::STAGE_SEA, 0x2B},
};

void prepareWorld() {
    const int cur = dStage_stagInfo_GetSaveTbl(dComIfGp_getStage().getStagInfo());
    for (const SaveSwitch& s : kSaveSwitches) {
        dComIfGs_onSaveSwitch(s.stageNo, s.sw);
        if (cur == s.stageNo) {
            dComIfGs_onSwitch(s.sw, dComIfGp_roomControl_getStayNo());
        }
        line("saved switch 0x%02X of stage table %d on", s.sw, s.stageNo);
    }
}

// --- the steps ---

void startEntry() {
    sCurFailed = false;
    sStep = kToE;
    const Entry& en = cur();
    line("entrance %zu (plan line %d): %s -> %s", sIdx, sOnly[sIdx], en.entrance.c_str(), en.exit.c_str());
    if (sIdx == 0 && en.e.stage == dComIfGp_getStartStageName() && dComIfGp_roomControl_getStayNo() == en.e.room) {
        // booted on E: arrived already
        sState = kArrive;
        sWant = en.e;
        sWantSpawn = false;
        sOldScene = fpcM_ERROR_PROCESS_ID_e;
        sPresent = kSettle;
        sSince = 0;
        return;
    }
    hop(en.e);
}

[[noreturn]] void finishAll() {
    line("%d entrance(s) done, %d with a failure", sEntriesDone, sEntriesFailed);
    sState = kDone;
    randoSmokeEnd(kName);
}

void nextEntry() {
    sEntriesDone++;
    if (sCurFailed) {
        sEntriesFailed++;
    }
    line("entrance %zu %s: %s", sIdx, cur().entrance.c_str(), sCurFailed ? "FAIL" : "ok");
    sIdx++;
    if (sIdx >= sPlan.size()) {
        finishAll();
    }
    startEntry();
}

void startBack() {
    const Entry& en = cur();
    sStep = kBack;
    switch (en.back) {
    case Back::Scls:
        takeScls(en.x.room, en.x.scls, en.b, "back");
        return;
    case Back::Helmaroc:
        takeScls(0, 1, en.b, "back (Helmaroc King's door)");
        return;
    case Back::Warp:
        if (!warpInit()) {
            abandon();
            return;
        }
        sState = kWarpOrder;
        sSince = 0;
        warpOrder();
        return;
    }
}

// After the forward and back arrivals the player stays kStayFrames with no stage change: nothing
// throws them out (e.g. a Fire Mountain / Ice Ring Isle VolTag left in a cave reached from an
// entrance without a timer, or a spawn the player cannot stand on). sStayed: declared at the top.

void stayed();

// The play scene runs on sWant: the step's next action.
void arrived() {
    const Entry& en = cur();
    if ((sStep == kForward || sStep == kBack) && !sStayed) {
        sState = kStay;
        sSince = 0;
        return;
    }
    sStayed = false;
    switch (sStep) {
    case kToE:
        sStep = kForward;
        takeScls(en.e.room, en.e.scls, en.x, "forward");
        return;
    case kForward:
        if (en.save) {
            sStep = kSaveQuit;
            saveAndQuit();
            return;
        }
        startBack();
        return;
    case kSaveQuit:
        sStep = kToX;
        hop(en.x);
        return;
    case kToX:
        startBack();
        return;
    case kBack:
        nextEntry();
        return;
    }
}

void stayed() {
    sStayed = true;
    arrived();
}

const char* stepName() {
    switch (sStep) {
    case kToE: return "to the entrance";
    case kForward: return "forward";
    case kSaveQuit: return "quit: reload";
    case kToX: return "to the exit";
    case kBack: return "back";
    }
    return "?";
}

// A relative COS_RANDO_PLACEMENT / COS_ENTRSWEEP_PLAN that is not there from the working folder is
// taken from the repo's root (as the loc smoke does). Runs before main.
struct ResolvePaths {
    static void resolve(const char* var) {
        const char* path = getenv(var);
        if (path == nullptr || path[0] == '\0' || path[0] == '/' || access(path, R_OK) == 0) {
            return;
        }
        std::string root = __FILE__; // <root>/native/src/pc/harness/smokes/<this file>
        for (int up = 0; up < 6; up++) {
            const size_t slash = root.find_last_of('/');
            if (slash == std::string::npos) {
                return;
            }
            root.resize(slash);
        }
        std::vector<std::string> tries = {root + "/" + path};
        const char* run = getenv("COS_RUN_DIR");
        if (root[0] != '/' && run != nullptr && run[0] != '\0') {
            tries.push_back(std::string(run) + "/../../" + root + "/" + path);
        }
        for (const std::string& t : tries) {
            if (access(t.c_str(), R_OK) == 0) {
                setenv(var, t.c_str(), 1);
                return;
            }
        }
    }
    ResolvePaths() {
        const char* smoke = getenv("COS_SMOKE");
        const char* name = getenv("COS_RANDO_SMOKE");
        if (smoke == nullptr || strcmp(smoke, "rando") != 0 || name == nullptr || strcmp(name, kName) != 0) {
            return;
        }
        resolve("COS_RANDO_PLACEMENT");
        resolve("COS_ENTRSWEEP_PLAN");
    }
} sResolvePaths;

} // namespace

void randoSmokeEntrsweep(unsigned int frames) {
    (void)frames;
    if (sState == kDone) {
        return;
    }
    if (sState == kInit) {
        if (!randoSmokeInPlay(kSettle) || dComIfGp_getPlayer(0) == nullptr) {
            return;
        }
        const bool planOk = loadPlan();
        randoSmokeCheck(kName, "the plan loads (COS_ENTRSWEEP_PLAN, COS_ENTRSWEEP_ONLY)", planOk);
        const PcRandoSeed* seed = pc_rando_seed();
        randoSmokeCheck(kName, "a randomized seed is active (COS_RANDO_PLACEMENT)", pc_rando() == 1 && seed != nullptr);
        if (!planOk || seed == nullptr) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        memcpy(sHash, seed->hash, 16);
        sHaveHash = true;
        prepareWorld();
        line("%zu entrance(s); boot %s room %d", sPlan.size(), dComIfGp_getStartStageName(),
             dComIfGp_roomControl_getStayNo());
        sIdx = 0;
        startEntry();
        return;
    }
    sSince++;
    switch (sState) {
    case kInit:
    case kDone:
        return;
    case kArrive: {
        const fpc_ProcID scene = playSceneId();
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr || dComIfGp_isEnableNextStage()) {
            sPresent = 0;
            if (sSince >= kArriveFrames) {
                char buf[200];
                snprintf(buf, sizeof buf, "%s: %s reached (still on %s room %d)", stepName(), placeStr(sWant).c_str(),
                         dComIfGp_getStartStageName(), dComIfGp_roomControl_getStayNo());
                check(buf, false);
                abandon();
            }
            return;
        }
        if (++sPresent < kSettle) {
            return;
        }
        const char* stage = dComIfGp_getStartStageName();
        const int room = dComIfGp_roomControl_getStayNo();
        const int point = dComIfGp_getStartStagePoint();
        // room -1 (a dungeon start's SCLS[0]): the room of the spawn, whichever it is
        const bool there = sWant.stage == stage && (sWant.room < 0 || room == sWant.room) &&
                           (!sWantSpawn || point == sWant.spawn);
        char buf[200];
        snprintf(buf, sizeof buf, "%s: the play scene runs on %s (is %s room %d spawn %d, after %u frames)", stepName(),
                 placeStr(sWant).c_str(), stage, room, point, sSince);
        if (!check(buf, there)) {
            abandon();
            return;
        }
        arrived();
        return;
    }
    case kStay: {
        const char* stage = dComIfGp_getStartStageName();
        const bool same = sWant.stage == stage && (sWant.room < 0 || dComIfGp_roomControl_getStayNo() == sWant.room);
        if (dComIfGp_isEnableNextStage() || !same || dComIfGp_getPlayer(0) == nullptr) {
            char buf[200];
            snprintf(buf, sizeof buf, "%s: the player stays on %s for %u frames (after %u: %s %s)", stepName(),
                     placeStr(sWant).c_str(), kStayFrames, sSince,
                     dComIfGp_isEnableNextStage() ? "a stage change to" : "now on",
                     dComIfGp_isEnableNextStage() ? nextStr().c_str() : stage);
            check(buf, false);
            abandon();
            return;
        }
        if (sSince >= kStayFrames) {
            char buf[200];
            snprintf(buf, sizeof buf, "%s: the player stays on %s for %u frames", stepName(), placeStr(sWant).c_str(),
                     kStayFrames);
            check(buf, true);
            sState = kArrive; // (not travelling: stayed() goes on with the step)
            stayed();
        }
        return;
    }
    case kRecover: {
        const bool calm = !dComIfGp_isEnableNextStage() && dComIfGp_getPlayer(0) != nullptr &&
                          playSceneId() != fpcM_ERROR_PROCESS_ID_e;
        sPresent = calm ? sPresent + 1 : 0;
        if (sPresent >= kSettle || sSince >= kArriveFrames) {
            nextEntry();
        }
        return;
    }
    case kWarpOrder: {
        dEvt_control_c* ctl = g_dComIfG_gameInfo.play.getEvent();
        if (dComIfGp_event_runCheck() && ctl->mEventId == sEventIdx) {
            line("frame %u: entrance %zu: WARP_WIND_AFTER started after %u frame(s)", pc_frame_count(), sIdx, sSince);
            sState = kWarpRun;
            sSince = 0;
            return;
        }
        if (sSince >= kWarpStartFrames) {
            check("back: WARP_WIND_AFTER started", false);
            abandon();
            return;
        }
        if (!dComIfGp_event_runCheck()) {
            warpOrder();
        }
        return;
    }
    case kWarpRun: {
        const Entry& en = cur();
        if (dComIfGp_isEnableNextStage()) {
            char buf[200];
            snprintf(buf, sizeof buf, "back: the warp out requests %s (is %s)", placeStr(en.b).c_str(), nextStr().c_str());
            check(buf, nextIs(en.b));
            travel(en.b, true);
            return;
        }
        driveStuckCuts();
        if (sSince >= kWarpFrames) {
            check("back: WARP_WIND_AFTER requested the next stage", false);
            abandon();
        }
        return;
    }
    }
}

} // namespace pc
