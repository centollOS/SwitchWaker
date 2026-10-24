// COS_SMOKE=rando COS_RANDO_SMOKE=enemysweep (docs/RANDOMIZER_PLAN.md step R7.4d): the enemy randomizer's
// room sweep, one run over a list of stops, as native/tools/rando/enemy_sweep.py runs it:
//   run.sh rando --stage <stage:room:point[:layer]> --env COS_RANDO_SMOKE=enemysweep
//                [--env COS_RANDO_PLACEMENT=<placement with "enemies">]
//                [--env COS_RANDO_SWEEP_HOPS=<stage:room:point:layer>[,...]]   (layer -1: the default)
//                [--env COS_RANDO_SWEEP_FRAMES=<frames per stop, default 300>]
// Stop 0 is the boot room; every hop is a stage change (dComIfGp_setNextStage), as the game's exits make.
// In each stop, once the player has been in the room for 90 frames:
//   - arrival: the stay stage and room are the stop's;
//   - every enemy location of the enemy table in the stay room, on its layer (the default chunk or
//     ACT<layer> of the game's layer), is checked in the room.dzr the game parsed: a record the
//     patcher wrote (pc_rando_enemies_last_result) or, without a seed, the vanilla record, whose spawn
//     does not wait for a switch (ChecksConditionSwitch), has a live actor with the record's procname
//     at its home (or with its params): the actor was created;
//   - "heap" line: the stay room's memory block (dStage_roomControl_c::getMemoryBlock, stages with
//     MEMA) and the main/game/archive heaps, size, free total and largest free block;
// then the stop runs on to COS_RANDO_SWEEP_FRAMES frames in the room (the player kept alive: max life
// every frame, as the combat sweep does). Must-defeat groups (enemy_locations.txt "Must defeat
// enemies: True") of the stay room and layer: the smoke deletes the room's enemies (group
// fopAc_ENEMY_e) every frame, as R7.4b's ALLdie check does, and requires the room to complete: every
// live ALLdie of the room sets its switch, or (no ALLdie) none of the must-defeat groups' own enemies is
// live for 70 frames in a row (none comes back; the room's other groups, e.g. Gyorg or Octorok spawners
// on the sea, may go on spawning), within 1200 frames. Actors still creating are not deleted (their
// delete would free an archive they do not hold yet). An enemy counts as created when its actor was
// seen live in any frame from the arrival to the check (some leave on their own right away).
// Last, the stop's count of heap allocation failures (pc_heap.cpp's "[cos] heap: cannot alloc" lines;
// heapAllocFailures) must be 0. Every check is a "[cos] rando-smoke enemysweep: ..." line, with
// "stop <n> (<stage> room <r>)" so the sweep can tell stops apart.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_enemies.h"
#include "features/rando/pc_rando_enemy_data.h"
#include "pc/pc_rando.h"

#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_ext.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "enemysweep";
constexpr unsigned kSettle = 90;
constexpr unsigned kArriveFrames = 3000;
constexpr unsigned kKillFrames = 1200;
constexpr unsigned kClearFrames = 70;
constexpr u16 kMaxLife = 20 * 4; // quarters: 20 hearts

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

bool check(const char* fmt, bool ok, ...) __attribute__((format(printf, 1, 3)));
bool check(const char* fmt, bool ok, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, ok);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return randoSmokeCheck(kName, buf, ok);
}

struct Stop {
    char stage[9];
    int room, point, layer;
};
std::vector<Stop> sStops; // [0] = the boot stop
size_t sStop = 0;
unsigned sFrames = 300;

bool parseStop(const char* text, Stop* out) {
    Stop s = {};
    s.layer = -1;
    char st[16] = {};
    if (sscanf(text, "%8[^:]:%d:%d:%d", st, &s.room, &s.point, &s.layer) < 3) {
        return false;
    }
    memcpy(s.stage, st, 8);
    *out = s;
    return true;
}

void parseStops() {
    Stop boot = {};
    const char* b = getenv("COS_BOOT_STAGE");
    if (b == nullptr || !parseStop(b, &boot)) {
        snprintf(boot.stage, sizeof boot.stage, "%s", dComIfGp_getStartStageName());
        boot.room = dComIfGp_roomControl_getStayNo();
        boot.layer = -1;
    }
    sStops.push_back(boot);
    if (const char* f = getenv("COS_RANDO_SWEEP_FRAMES")) {
        const int v = atoi(f);
        if (v >= (int)kSettle) {
            sFrames = (unsigned)v;
        }
    }
    const char* s = getenv("COS_RANDO_SWEEP_HOPS");
    if (s == nullptr) {
        return;
    }
    std::string all(s);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t end = all.find(',', pos);
        if (end == std::string::npos) {
            end = all.size();
        }
        const std::string item = all.substr(pos, end - pos);
        Stop h;
        if (parseStop(item.c_str(), &h)) {
            sStops.push_back(h);
        } else {
            logf("[cos] rando-smoke %s: bad hop \"%s\" (stage:room:point[:layer])\n", kName, item.c_str());
        }
        pos = end + 1;
    }
}

// --- the live checks ---

uint8_t* storedChunk(void* file, const char* tag, int* count) {
    dStage_fileHeader* hdr = (dStage_fileHeader*)file;
    const int n = hdr->m_chunkCount;
    if (n <= 0 || n > 256) {
        return nullptr;
    }
    for (int i = 0; i < n; i++) {
        dStage_nodeHeader& node = hdr->m_nodes[i];
        uint32_t t = node.m_tag;
        if (memcmp(&t, tag, 4) != 0) {
            continue;
        }
        *count = node.m_entryNum;
        if (node.m_offset.isRelocated()) {
            return (uint8_t*)node.m_offset;
        }
        const s32 off = node.m_offset.value;
        return off != 0 ? (uint8_t*)file + off : nullptr;
    }
    return nullptr;
}

struct ActorFind {
    s16 proc;
    cXyz pos;
    uint32_t params;
    bool found;
    int same;       // live actors with the procname
    float nearest;  // their nearest home distance
};

void* findActor(void* proc, void* data) {
    ActorFind* f = static_cast<ActorFind*>(data);
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(proc);
    if (fpcM_GetName(proc) != f->proc) {
        return nullptr;
    }
    const cXyz& h = ac->home.pos;
    const float dx = h.x - f->pos.x, dy = h.y - f->pos.y, dz = h.z - f->pos.z;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    f->nearest = f->same == 0 || d < f->nearest ? d : f->nearest;
    f->same++;
    if (d < 1.0f || fopAcM_GetParam(ac) == f->params) {
        f->found = true;
        return proc;
    }
    return nullptr;
}

void logHeap(int stop, const char* stage, int stay) {
    char room[160] = "no memory block";
    const int id = dStage_roomControl_c::getMemoryBlockID(stay);
    JKRExpHeap* block = id >= 0 ? dStage_roomControl_c::getMemoryBlock(stay) : nullptr;
    if (block != nullptr) {
        snprintf(room, sizeof room, "block %d size 0x%x free 0x%x largest 0x%x", id, (unsigned)block->getHeapSize(),
                 (unsigned)block->getTotalFreeSize(), (unsigned)block->getFreeSize());
    }
    JKRExpHeap* heaps[3] = {mDoExt_getZeldaHeap(), mDoExt_getGameHeap(), mDoExt_getArchiveHeap()};
    unsigned v[3][2] = {};
    for (int i = 0; i < 3; i++) {
        if (heaps[i] != nullptr) {
            v[i][0] = (unsigned)heaps[i]->getTotalFreeSize();
            v[i][1] = (unsigned)heaps[i]->getFreeSize();
        }
    }
    logf("[cos] rando-smoke %s: heap stop %d (%s room %d): room %s; main free 0x%x largest 0x%x; game free 0x%x "
         "largest 0x%x; archive free 0x%x largest 0x%x\n",
         kName, stop, stage, stay, room, v[0][0], v[0][1], v[1][0], v[1][1], v[2][0], v[2][1]);
}

// A location's record in the game's room.dzr, or nullptr.
const uint8_t* storedRecord(void* file, const pcr::EnemyLocation& loc) {
    char tag[5];
    pc_rando_dzx_tag("ACTR", loc.path.layer, tag);
    int count = 0;
    uint8_t* first = file != nullptr ? storedChunk(file, tag, &count) : nullptr;
    return first != nullptr && (int)loc.path.index < count ? first + loc.path.index * 0x20 : nullptr;
}

bool liveNow(const uint8_t* e, ActorFind* f) {
    char name[9] = {};
    memcpy(name, e, 8);
    dStage_objectNameInf* inf = dStage_searchName(name);
    if (inf == nullptr) {
        return false;
    }
    *f = ActorFind{inf->procname,
                   cXyz(pc_rando_dzx_bef32(e + 0x0C), pc_rando_dzx_bef32(e + 0x10), pc_rando_dzx_bef32(e + 0x14)),
                   pc_rando_dzx_be32(e + 8), false, 0, 0.0f};
    fopAcM_Search(findActor, f);
    return f->found;
}

// Per enemy location: its actor was seen live since the stop's room was entered. Some enemies leave
// on their own soon after their creation (a rat that falls 1000 below its home deletes itself, an
// Octorok of type 4 that dives does, d_a_nz.cpp / d_a_oq.cpp): "created" is seen in any frame from the
// player's arrival to the check.
std::vector<uint8_t> sSeen;

void sampleRoom() {
    const pcr::Span<pcr::EnemyLocation> locs = pcr::enemy_locations();
    if (sSeen.size() != locs.size) {
        sSeen.assign(locs.size, 0);
    }
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    if (stay < 0) {
        return;
    }
    const int layer = dComIfG_play_c::getLayerNo(stay);
    char arc[16];
    snprintf(arc, sizeof arc, "Room%d", stay);
    void* file = dComIfG_getStageRes(arc, "room.dzr");
    if (file == nullptr) {
        return;
    }
    for (size_t k = 0; k < locs.size; k++) {
        const pcr::EnemyLocation& loc = locs[k];
        if (sSeen[k] || loc.path.room != stay || strcmp(loc.path.stage, stage) != 0 ||
            (loc.path.layer >= 0 && loc.path.layer != layer)) {
            continue;
        }
        const uint8_t* e = storedRecord(file, loc);
        ActorFind f;
        if (e != nullptr && liveNow(e, &f)) {
            sSeen[k] = 1;
        }
    }
}

// The table's locations of the stay room on its layer: live actors, and whether a must-defeat group
// is there.
bool checkRoom(int stop, const char* stage, int stay, int layer) {
    char arc[16];
    snprintf(arc, sizeof arc, "Room%d", stay);
    void* file = dComIfG_getStageRes(arc, "room.dzr");
    const bool rando = pc_rando_enemies_active() != 0;
    const pcr::Span<pcr::EnemyLocation> locs = pcr::enemy_locations();
    const pcr::Span<pcr::EnemyGroup> groups = pcr::enemy_groups();
    int nLoc = 0, nAct = 0, actOk = 0, notWritten = 0;
    bool mustDefeat = false;
    for (size_t k = 0; k < locs.size; k++) {
        const pcr::EnemyLocation& loc = locs[k];
        if (strcmp(loc.path.stage, stage) != 0 || loc.path.room != stay) {
            continue;
        }
        if (loc.path.layer >= 0 && loc.path.layer != layer) {
            continue;
        }
        nLoc++;
        mustDefeat = mustDefeat || groups[loc.group].must_defeat;
        if (rando && pc_rando_enemies_last_result((int)k) != PCR_ENEMY_WRITTEN) {
            notWritten++;
            logf("[cos] rando-smoke %s:   %s: not written (result %d)\n", kName, loc.path.raw,
                 pc_rando_enemies_last_result((int)k));
            continue;
        }
        if ((loc.conds & pcr::ECOND_CHECKS_CONDITION_SWITCH) != 0) {
            continue; // the spawn waits for a switch
        }
        const uint8_t* e = storedRecord(file, loc);
        nAct++;
        if (e == nullptr) {
            logf("[cos] rando-smoke %s:   %s: no record %u in the game's room.dzr\n", kName, loc.path.raw,
                 (unsigned)loc.path.index);
            continue;
        }
        char name[9] = {};
        memcpy(name, e, 8);
        dStage_objectNameInf* inf = dStage_searchName(name);
        if (inf == nullptr) {
            logf("[cos] rando-smoke %s:   %s: %s is not an actor name\n", kName, loc.path.raw, name);
            continue;
        }
        const uint32_t params = pc_rando_dzx_be32(e + 8);
        ActorFind f = {inf->procname,
                       cXyz(pc_rando_dzx_bef32(e + 0x0C), pc_rando_dzx_bef32(e + 0x10), pc_rando_dzx_bef32(e + 0x14)),
                       params, false, 0, 0.0f};
        fopAcM_Search(findActor, &f);
        const bool seen = k < sSeen.size() && sSeen[k] != 0;
        actOk += f.found || seen ? 1 : 0;
        if (!f.found && seen) {
            logf("[cos] rando-smoke %s:   %s: %s was live after the arrival, gone by the check\n", kName,
                 loc.path.raw, name);
        } else if (!f.found) {
            logf("[cos] rando-smoke %s:   %s: no live %s (proc %d) at (%.0f, %.0f, %.0f) params %08X (%d live, "
                 "nearest home %.0f)\n",
                 kName, loc.path.raw, name, (int)inf->procname, f.pos.x, f.pos.y, f.pos.z, (unsigned)params, f.same,
                 f.nearest);
        }
    }
    logf("[cos] rando-smoke %s: stop %d (%s room %d layer %d): %d enemy locations, seed enemies %d%s\n", kName, stop,
         stage, stay, layer, nLoc, rando ? 1 : 0, mustDefeat ? ", must defeat" : "");
    if (rando) {
        check("stop %d (%s room %d): every enemy location of the room written (%d not)", notWritten == 0, stop, stage,
              stay, notWritten);
    }
    check("stop %d (%s room %d): %d of %d enemies created (live at their home)", actOk == nAct, stop, stage, stay, actOk,
          nAct);
    return mustDefeat;
}

// --- must defeat ---

std::vector<fpc_ProcID> sDeleted;
std::vector<s16> sDeletedNames; // the deleted actors' process names, for the log
unsigned sKilled = 0;

void* deleteEnemy(void* proc, void* data) {
    const int room = *static_cast<int*>(data);
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(proc);
    if (!fopAcM_IsActor(proc) || ac->group != fopAc_ENEMY_e || fopAcM_GetRoomNo(ac) != room) {
        return nullptr;
    }
    const fpc_ProcID id = fopAcM_GetID(ac);
    if (fpcM_IsCreating(id)) {
        return nullptr; // still loading its archive: its delete would free what it has not got yet
    }
    for (fpc_ProcID d : sDeleted) {
        if (d == id) {
            return nullptr;
        }
    }
    sDeleted.push_back(id);
    sDeletedNames.push_back(fpcM_GetName(proc));
    sKilled++;
    fopAcM_delete(ac);
    return nullptr;
}

struct Alldie {
    int room;
    std::vector<int> switches;
};

void* findAlldie(void* proc, void* data) {
    Alldie* a = static_cast<Alldie*>(data);
    static s16 sProc = -1;
    if (sProc < 0) {
        dStage_objectNameInf* inf = dStage_searchName("ALLdie");
        sProc = inf != nullptr ? inf->procname : 0x7FFF;
    }
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(proc);
    if (fpcM_GetName(proc) == sProc && fopAcM_GetRoomNo(ac) == a->room) {
        a->switches.push_back((int)((fopAcM_GetParam(ac) >> 8) & 0xFF));
    }
    return nullptr;
}

// --- the state machine ---

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(isPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

enum State { kBoot, kArrive, kHold, kKill, kDone };
State sState = kBoot;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
unsigned sSince = 0, sPresent = 0, sKillFrames = 0, sClear = 0;
unsigned sAllocBase = 0;
bool sMustDefeat = false;
int sRoom = -1;
char sStage[9] = "";
Alldie sAlldie;

void keepAlive() {
    if (dComIfGs_getMaxLife() < kMaxLife) {
        dComIfGs_setMaxLife((u8)kMaxLife);
    }
    dComIfGs_setLife(kMaxLife);
}

void endStop();

void nextStop() {
    sStop++;
    if (sStop >= sStops.size()) {
        sState = kDone;
        randoSmokeEnd(kName);
    }
    const Stop& h = sStops[sStop];
    logf("[cos] rando-smoke %s: stage change to %s room %d point %d layer %d\n", kName, h.stage, h.room, h.point,
         h.layer);
    sAllocBase = heapAllocFailures(); // the stop's count: the stage change, its loads and its frames
    sSeen.assign(pcr::enemy_locations().size, 0);
    sOldScene = playSceneId();
    sSince = 0;
    sPresent = 0;
    sState = kArrive;
    dComIfGp_setNextStage(h.stage, (s16)h.point, (s8)h.room, (s8)h.layer);
}

// The player has been in the stop's room for kSettle frames.
void arrived() {
    const Stop& h = sStops[sStop];
    snprintf(sStage, sizeof sStage, "%s", dComIfGp_getStartStageName());
    sRoom = dComIfGp_roomControl_getStayNo();
    const int layer = dComIfG_play_c::getLayerNo(sRoom);
    check("stop %zu (%s room %d): arrived (is %s room %d layer %d)",
          strcmp(sStage, h.stage) == 0 && sRoom == h.room, sStop, h.stage, h.room, sStage, sRoom, layer);
    logHeap((int)sStop, sStage, sRoom);
    sMustDefeat = checkRoom((int)sStop, sStage, sRoom, layer);
    sState = kHold;
}

// The must-defeat groups' records of the room (no ALLdie: what "complete" watches).
std::vector<std::vector<uint8_t>> sMustRecs;

void collectMustDefeat() {
    sMustRecs.clear();
    const pcr::Span<pcr::EnemyLocation> locs = pcr::enemy_locations();
    const pcr::Span<pcr::EnemyGroup> groups = pcr::enemy_groups();
    const int layer = dComIfG_play_c::getLayerNo(sRoom);
    char arc[16];
    snprintf(arc, sizeof arc, "Room%d", sRoom);
    void* file = dComIfG_getStageRes(arc, "room.dzr");
    for (size_t k = 0; k < locs.size && file != nullptr; k++) {
        const pcr::EnemyLocation& loc = locs[k];
        if (!groups[loc.group].must_defeat || loc.path.room != sRoom || strcmp(loc.path.stage, sStage) != 0 ||
            (loc.path.layer >= 0 && loc.path.layer != layer)) {
            continue;
        }
        if (const uint8_t* e = storedRecord(file, loc)) {
            sMustRecs.emplace_back(e, e + 0x20);
        }
    }
}

// Some must-defeat enemy of the room is live (at its home, or with its params).
bool mustDefeatLive() {
    for (const std::vector<uint8_t>& r : sMustRecs) {
        ActorFind f;
        if (liveNow(r.data(), &f)) {
            return true;
        }
    }
    return false;
}

void startKill() {
    sDeleted.clear();
    sDeletedNames.clear();
    sKilled = 0;
    sKillFrames = 0;
    sClear = 0;
    sAlldie = Alldie{sRoom, {}};
    fopAcM_Search(findAlldie, &sAlldie);
    collectMustDefeat();
    logf("[cos] rando-smoke %s: stop %zu (%s room %d): must defeat: %zu live ALLdie, deleting the room's enemies\n",
         kName, sStop, sStage, sRoom, sAlldie.switches.size());
    sState = kKill;
}

void killStep() {
    sKillFrames++;
    fopAcM_Search(deleteEnemy, &sRoom);
    bool done;
    if (!sAlldie.switches.empty()) {
        done = true;
        for (int sw : sAlldie.switches) {
            done = done && dComIfGs_isSwitch(sw, sRoom);
        }
    } else {
        // No ALLdie: the must-defeat groups' own enemies (their death switches, or the room's door or
        // chest, are what the game then waits for) stay gone; the room's other groups (spawners of
        // Gyorgs or Octoroks on the sea) may go on making enemies.
        sClear = !mustDefeatLive() ? sClear + 1 : 0;
        done = sClear >= kClearFrames;
    }
    if (done || sKillFrames >= kKillFrames) {
        char list[256] = "";
        size_t used = 0;
        std::vector<s16> names = sDeletedNames;
        for (size_t i = 0; i < names.size() && used + 16 < sizeof list; i++) {
            int n = 0;
            for (size_t j = i; j < names.size(); j++) {
                n += names[j] == names[i] ? 1 : 0;
            }
            bool earlier = false;
            for (size_t j = 0; j < i; j++) {
                earlier = earlier || names[j] == names[i];
            }
            if (!earlier) {
                used += (size_t)snprintf(list + used, sizeof list - used, "%s%d x%d", used ? ", " : "", (int)names[i], n);
            }
        }
        logf("[cos] rando-smoke %s: stop %zu: removed by process name: %s\n", kName, sStop, list);
        check("stop %zu (%s room %d): must-defeat room complete after %u enemies removed (%s, frame %u)", done, sStop,
              sStage, sRoom, sKilled, sAlldie.switches.empty() ? "its must-defeat enemies stay gone" : "every ALLdie switch set",
              sKillFrames);
        endStop();
    }
}

void endStop() {
    const unsigned failures = heapAllocFailures() - sAllocBase;
    check("stop %zu (%s room %d): %u heap allocation failure(s)", failures == 0, sStop, sStage, sRoom, failures);
    nextStop();
}

} // namespace

void randoSmokeEnemysweep(unsigned int frames) {
    (void)frames;
    // the stop list and the kill step's vectors outlive the game's current heap, which a hop frees
    JKRPcHostAllocScope hostAlloc;
    if (sState == kDone) {
        return;
    }
    if (sState == kBoot) {
        if (dComIfGp_getPlayer(0) != nullptr && playSceneId() != fpcM_ERROR_PROCESS_ID_e) {
            sampleRoom();
        }
        if (!randoSmokeInPlay(kSettle)) {
            return;
        }
        parseStops();
        logf("[cos] rando-smoke %s: pc_rando %d, seed enemies %d, %zu stop(s), %u frames each\n", kName, pc_rando(),
             pc_rando_enemies_active(), sStops.size(), sFrames);
        sAllocBase = 0; // the boot stop counts the boot's loads
        sPresent = kSettle;
        arrived();
        return;
    }
    if (sState == kArrive) {
        sSince++;
        const fpc_ProcID scene = playSceneId();
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr ||
            dComIfGp_isEnableNextStage()) {
            sPresent = 0;
            if (sSince >= kArriveFrames) {
                check("stop %zu: %s room %d reached", false, sStop, sStops[sStop].stage, sStops[sStop].room);
                sState = kDone;
                randoSmokeEnd(kName);
            }
            return;
        }
        keepAlive();
        sampleRoom();
        if (++sPresent < kSettle) {
            return;
        }
        arrived();
        return;
    }
    keepAlive();
    if (sState == kHold) {
        if (++sPresent < sFrames) {
            return;
        }
        if (sMustDefeat) {
            startKill();
        } else {
            endStop();
        }
        return;
    }
    if (sState == kKill) {
        killStep();
    }
}

} // namespace pc
