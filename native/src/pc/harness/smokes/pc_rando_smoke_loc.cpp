// COS_SMOKE=rando COS_RANDO_SMOKE=loc (docs/RANDOMIZER_PLAN.md R2.1, "The location sweep"): the item
// locations of the stages the run visits, checked against the placement. Initial version: the chests
// (features/rando/pc_rando_place_dzx.h). R2.12 adds its driver; R2.2-R2.10 add their location kinds.
//
//   run.sh rando --stage <stage>:<room>[:<point>[:<layer>]] --env COS_RANDO_SMOKE=loc
//                [--env COS_RANDO_PLACEMENT=<placement.json>]
//                [--env COS_RANDO_LOC_HOPS=<stage>:<room>:<point>[:<layer>],...]
//                [--env COS_RANDO_LOC_OPEN=<location>,...] [--env COS_RANDO_LOC_ALL=1]
//
// The run stops at the boot stage, then at every COS_RANDO_LOC_HOPS entry in order (a scene change as
// the doors make it, dComIfGp_setNextStage). At each stop, kSettle frames after the new play scene's
// player appeared:
//   T1 (static), for every chest key (pcr::location_keys(), DzxField::TboxItem) of the stop's stage
//   whose archive is loaded:
//     - the record: the stored stage.dzs / room.dzr (dComIfG_getStageRes, the bytes the game parsed)
//       holds the key's entry (its 8-byte name) with the placed item (pc_rando_loc_item) in its field;
//       with pc_rando() 1 the patcher's status for the key is "written" or "unchanged"; with
//       pc_rando() 0 nothing was patched and the field holds the vanilla item;
//     - the actor: when the entry's actors exist at this stop (stage.dzs: the base TRES chunk, or the
//       TRE<n> of the stage's layer; room.dzr: the stay room, its layer's chunk), a live TBOX stands at
//       the entry's position and holds the placed item (daTbox_c::getItemNo, home.angle.z >> 8).
//   T2 (given), for each COS_RANDO_LOC_OPEN location (a PCR_LOC_* name, with or without the prefix,
//   or its number) whose chest is live at this stop: a chest that waits for a switch (enemies
//   defeated, a switch hit) gets its switch set; once no event runs, the player is put in front of
//   it on the floor (daPy_lk_c::debugPlace; a put that does not hold, e.g. on a ladder, is retried
//   after walking at the chest), facing it, walks at it until the A button says "Open" and presses
//   A; the chest's event must start and end, its item-get (dComIfGp_event_getGtItm) must be the placed item, and
//   execItemGet must run for it (the progressive tier pc_rando_convert_progressive_item_id gives, as
//   the get-item demo does; observed through a pass-through item override, pc_rando_items.h).
// A chest of a room other than the stay room is put in reach only when that room's floor is loaded,
// so T2 locations should be opened from a stop in their own room. A relative COS_RANDO_PLACEMENT is
// also looked up from the repo's root (regress lines run from anywhere).
// After the last stop it prints the coverage (chest keys whose record and actor were checked) and,
// with COS_RANDO_LOC_ALL=1, requires every chest key's record and actor to have been checked.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_place_dzx.h"

#include "d/actor/d_a_player_main.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "d/d_stage.h"
#include "d/actor/d_a_tbox.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

// d_item.cpp's dispatch table (no header declares it), for the pass-through override.
extern void (*item_func_ptr[0x100])();

namespace pc {

namespace {

using pcr::DzxField;
using pcr::KeyKind;
using pcr::LocKey;

constexpr const char* kName = "loc";
constexpr unsigned int kSettle = 30;
constexpr unsigned int kArriveFrames = 1500;
constexpr unsigned int kQuietFrames = 900;  // T2: events to wait out before placing the player
constexpr unsigned int kWalkFrames = 300;   // T2: to the chest and A
constexpr double kEventSeconds = 90.0;      // T2: wall time (the item fanfare plays in real time)
constexpr unsigned int kTalkFrames = 45;
constexpr float kFront = 130.0f;
constexpr float kPosEps = 1.0f;

struct Stop {
    std::string stage;
    int room, point, layer;
};

enum State { kInit, kArrive, kCheck, kOpenPrep, kOpenWalk, kOpenEvent, kHop, kDone };
State sState = kInit;
std::vector<Stop> sStops;
int sStop = -1;
unsigned int sSince = 0;
unsigned int sPresent = 0;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
bool sAll = false;

std::vector<int> sOpen;      // PCR_LOC_* to open (T2)
std::vector<bool> sOpenDone; // per sOpen entry
int sOpenIdx = -1;           // the sOpen entry being opened
int sOpenKey = -1;           // its key
fpc_ProcID sOpenChest = fpcM_ERROR_PROCESS_ID_e;
uint8_t sOpenPlaced = 0;
int sOpenConv = -1;
bool sOpenGot = false;
int sOpenGtItm = -1;
char sOpenEvent[48] = "";
uint64_t sOpenStartNs = 0;
cXyz sOpenTarget;   // where the player was put
int sOpenTries = 0; // puts that did not hold (the player on a ladder, in a climb)
PcRandoItemFunc sPrevOverride = nullptr;

struct PendingOpen {
    int idx, key;
    fpc_ProcID chest;
    uint8_t placed;
};
std::vector<PendingOpen> sPending; // T2 chests live at this stop

std::vector<uint8_t> sRecordOk; // per key: 1 once its record was checked ok
std::vector<uint8_t> sActorOk;  // per key: 1 once its live chest was checked ok

void* findPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(findPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

bool isChestKey(const LocKey& k) {
    return k.kind == KeyKind::Dzx && k.field == DzxField::TboxItem;
}

uint8_t expectedItem(const LocKey& k) {
    return pc_rando() ? pc_rando_loc_item(k.loc, k.vanilla) : k.vanilla;
}

const char* locName(int loc) {
    const pcr::LocInfo* info = pcr::location_info(loc);
    return info != nullptr ? info->enum_name : "?";
}

// The layer of a chunk tag: -1 for a base tag ("TRES"), 0-11 for "TRE0".."TREb".
int tagLayer(const char* tag) {
    const char c = tag[3];
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c == 'a' || c == 'b') {
        return 10 + (c - 'a');
    }
    return -1;
}

int roomOfArc(const char* arc) {
    return strncmp(arc, "Room", 4) == 0 ? atoi(arc + 4) : -1;
}

// The first entry of chunk `tag` in a stored (possibly relocated, dStage_dt_c_offsetToPtr) dzx, and
// its entry count; nullptr when missing.
uint8_t* storedChunk(void* file, const char* tag, int* count) {
    dStage_fileHeader* hdr = (dStage_fileHeader*)file;
    const int n = hdr->m_chunkCount;
    if (n <= 0 || n > 256) {
        return nullptr;
    }
    for (int i = 0; i < n; i++) {
        dStage_nodeHeader& node = hdr->m_nodes[i];
        uint32_t t = node.m_tag;
        const uint8_t* tb = (const uint8_t*)&t;
        if (memcmp(tb, tag, 4) != 0) {
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

struct Chests {
    int n;
    daTbox_c* list[96];
};

void* collectTbox(void* proc, void* data) {
    Chests* c = static_cast<Chests*>(data);
    if (fpcM_GetName(proc) == fpcNm_TBOX_e && c->n < 96) {
        c->list[c->n++] = static_cast<daTbox_c*>(proc);
    }
    return nullptr;
}

daTbox_c* chestAt(const Chests& c, const cXyz& pos) {
    for (int i = 0; i < c.n; i++) {
        const cXyz& h = c.list[i]->home.pos;
        if (std::fabs(h.x - pos.x) < kPosEps && std::fabs(h.y - pos.y) < kPosEps && std::fabs(h.z - pos.z) < kPosEps) {
            return c.list[i];
        }
    }
    return nullptr;
}

// Where T2 puts the player: in front of the chest (a chest opens only from its front), on the floor.
// The first distance whose ground is near the chest's height; else kFront at the chest's height.
cXyz frontOf(const cXyz& c, s16 yaw) {
    static const float kDist[] = {kFront, 100.0f, 170.0f, 220.0f, 80.0f};
    for (float d : kDist) {
        cXyz p(c.x + d * cM_ssin(yaw), c.y, c.z + d * cM_scos(yaw));
        dBgS_ObjGndChk gnd;
        cXyz probe(p.x, c.y + 150.0f, p.z);
        gnd.SetPos(&probe);
        const f32 h = dComIfG_Bgsp()->GroundCross(&gnd);
        if (h > c.y - 100.0f && h < c.y + 100.0f) {
            p.y = h;
            return p;
        }
    }
    return cXyz(c.x + kFront * cM_ssin(yaw), c.y + 10.0f, c.z + kFront * cM_scos(yaw));
}

// The driven main stick toward `c`, in the camera's frame.
void stickToward(fopAc_ac_c* player, const cXyz& c) {
    const s16 a = cM_atan2s(c.x - player->current.pos.x, c.z - player->current.pos.z);
    const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
    setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 70.0f), (int8_t)(-cM_scos(st) * 70.0f));
}

// T2's pass-through override: records that execItemGet ran for the chest's item, then gives it as
// the override that was there (or the vanilla table) would.
void openItemFunc() {
    sOpenGot = true;
    if (sPrevOverride != nullptr) {
        sPrevOverride();
    } else if (sOpenConv >= 0) {
        item_func_ptr[sOpenConv & 0xFF]();
    }
}

void installOverride(int id) {
    sOpenConv = id;
    sPrevOverride = pc_rando_item_override_get((uint8_t)id);
    pc_rando_item_override((uint8_t)id, openItemFunc);
}

void removeOverride() {
    if (sOpenConv >= 0) {
        pc_rando_item_override((uint8_t)sOpenConv, sPrevOverride);
    }
    sPrevOverride = nullptr;
    sOpenConv = -1;
}

const char* eventName() {
    dEvt_control_c* ev = g_dComIfG_gameInfo.play.getEvent();
    if (ev->mEventId != -1) {
        dEvDtEvent_c* data = dComIfGp_getPEvtManager()->getEventData(ev->mEventId);
        if (data != nullptr) {
            return data->getName();
        }
    }
    return "(none)";
}

// T1 at the current stop; also lists the T2 chests live at this stop (sPending).
void checkStop() {
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    const int stageLayer = dComIfG_play_c::getLayerNo(-1);
    Chests chests = {};
    fopAcM_Search(collectTbox, &chests);
    writef(STDERR_FILENO, "[cos] rando-smoke loc: stop %d: stage %s, stay room %d, layer %d, %d chest actors, pc_rando() %d\n",
           sStop, stage, stay, stageLayer, chests.n, pc_rando());
    const pcr::Span<LocKey> keys = pcr::location_keys();
    int nRec = 0, nAct = 0;
    for (size_t i = 0; i < keys.size; i++) {
        const LocKey& k = keys[i];
        if (!isChestKey(k) || strcmp(k.stage, stage) != 0) {
            continue;
        }
        const bool isStage = strcmp(k.arc, "Stage") == 0;
        void* file = dComIfG_getStageRes(k.arc, isStage ? "stage.dzs" : "room.dzr");
        if (file == nullptr) {
            continue; // archive not loaded at this stop
        }
        const uint8_t want = expectedItem(k);
        const PcRandoPlaceStatus* st = pc_rando_place_dzx_status((int)i);
        int count = 0;
        uint8_t* first = storedChunk(file, k.tag, &count);
        uint8_t* e = first != nullptr && k.index < count ? first + k.index * 0x20 : nullptr;
        char what[200];
        snprintf(what, sizeof what, "T1 %s %s/%s %s[%u]: record holds 0x%02X", locName(k.loc), k.stage, k.arc, k.tag,
                 k.index, want);
        bool recOk = e != nullptr && memcmp(e, k.fp_name, 8) == 0 && pcr::dzx_key_read(k, e) == want;
        if (pc_rando()) {
            recOk = recOk && st != nullptr && st->patches > 0 &&
                    (st->result == PCR_PLACE_WRITTEN || st->result == PCR_PLACE_SAME);
        } else {
            recOk = recOk && (st == nullptr || st->patches == 0);
        }
        if (!recOk) {
            writef(STDERR_FILENO, "[cos] rando-smoke loc:   record: entry %s, name \"%.8s\", field 0x%02X, status %d (%u patches)\n",
                   e != nullptr ? "found" : "missing", e != nullptr ? (const char*)e : "", e != nullptr ? pcr::dzx_key_read(k, e) : 0,
                   st != nullptr ? st->result : -1, st != nullptr ? st->patches : 0);
        }
        randoSmokeCheck(kName, what, recOk);
        nRec++;
        if (recOk) {
            sRecordOk[i] = 1;
        }
        if (e == nullptr) {
            continue;
        }
        // Do the entry's actors exist at this stop?
        const int layer = tagLayer(k.tag);
        bool expected;
        if (isStage) {
            expected = layer < 0 || layer == stageLayer;
        } else {
            const int room = roomOfArc(k.arc);
            expected = room == stay && (layer < 0 || layer == dComIfG_play_c::getLayerNo(room));
        }
        if (!expected) {
            continue;
        }
        const cXyz pos(pc_rando_dzx_bef32(e + 0x0C), pc_rando_dzx_bef32(e + 0x10), pc_rando_dzx_bef32(e + 0x14));
        daTbox_c* chest = chestAt(chests, pos);
        snprintf(what, sizeof what, "T1 %s %s/%s %s[%u]: live chest at (%.0f, %.0f, %.0f) holds 0x%02X", locName(k.loc),
                 k.stage, k.arc, k.tag, k.index, pos.x, pos.y, pos.z, want);
        const bool actOk = chest != nullptr && chest->getItemNo() == want;
        if (chest != nullptr && !actOk) {
            writef(STDERR_FILENO, "[cos] rando-smoke loc:   the chest holds 0x%02X\n", chest->getItemNo());
        }
        randoSmokeCheck(kName, what, actOk);
        nAct++;
        if (actOk) {
            sActorOk[i] = 1;
            if (chest != nullptr) {
                writef(STDERR_FILENO, "[cos] rando-smoke loc:   %s: chest room %d, params 0x%08X, func %d, sw 0x%02X, open %d\n",
                       locName(k.loc), (int)chest->mRoomNo, (unsigned)fopAcM_GetParam(chest), (int)chest->getFuncType(), chest->getSwNo(),
                       (int)chest->checkOpen());
            }
        }
        for (size_t j = 0; j < sOpen.size(); j++) {
            bool queued = sOpenDone[j];
            for (const PendingOpen& p : sPending) {
                queued = queued || p.idx == (int)j;
            }
            if (!queued && sOpen[j] == k.loc && actOk) {
                sPending.push_back({(int)j, (int)i, fopAcM_GetID(chest), want});
            }
        }
    }
    writef(STDERR_FILENO, "[cos] rando-smoke loc: stop %d: %d records, %d live chests checked\n", sStop, nRec, nAct);
}

void parseStops() {
    const char* list = getenv("COS_RANDO_LOC_HOPS");
    if (list == nullptr) {
        return;
    }
    std::string s = list;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t end = s.find(',', pos);
        std::string item = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        char stage[16] = {};
        int room = 0, point = 0, layer = -1;
        const int n = sscanf(item.c_str(), "%15[^:]:%d:%d:%d", stage, &room, &point, &layer);
        if (n >= 3) {
            sStops.push_back({stage, room, point, n >= 4 ? layer : -1});
        } else if (!item.empty()) {
            writef(STDERR_FILENO, "[cos] rando-smoke loc: bad hop \"%s\" (stage:room:point[:layer])\n", item.c_str());
            randoSmokeCheck(kName, "COS_RANDO_LOC_HOPS parses", false);
        }
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
}

void parseOpen() {
    const char* list = getenv("COS_RANDO_LOC_OPEN");
    if (list == nullptr) {
        return;
    }
    std::string s = list;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t end = s.find(',', pos);
        std::string item = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (!item.empty()) {
            int loc = -1;
            if (item[0] >= '0' && item[0] <= '9') {
                loc = atoi(item.c_str());
            } else {
                const std::string full = item.rfind("PCR_LOC_", 0) == 0 ? item : "PCR_LOC_" + item;
                for (int l = 0; l < PCR_LOC_COUNT; l++) {
                    if (full == locName(l)) {
                        loc = l;
                    }
                }
            }
            if (loc < 0 || loc >= PCR_LOC_COUNT) {
                writef(STDERR_FILENO, "[cos] rando-smoke loc: unknown location \"%s\"\n", item.c_str());
                randoSmokeCheck(kName, "COS_RANDO_LOC_OPEN names known locations", false);
            } else {
                sOpen.push_back(loc);
                sOpenDone.push_back(false);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
}

void finishOpen(bool ok, const char* why) {
    char what[160];
    snprintf(what, sizeof what, "T2 %s: chest opened and gave 0x%02X%s%s", locName(sOpen[sOpenIdx]), sOpenPlaced,
             why[0] ? ": " : "", why);
    randoSmokeCheck(kName, what, ok);
    removeOverride();
    setDrivenPad(true, 0, 0, 0);
    sOpenDone[sOpenIdx] = true;
    sOpenIdx = -1;
    sOpenKey = -1;
    sOpenChest = fpcM_ERROR_PROCESS_ID_e;
    sState = kCheck; // the next chest to open at this stop, if any
    sSince = 0;
}

void report() {
    const pcr::Span<LocKey> keys = pcr::location_keys();
    int nKeys = 0, nRec = 0, nAct = 0;
    std::vector<uint8_t> locRec(PCR_LOC_COUNT, 0), locAct(PCR_LOC_COUNT, 0), locChest(PCR_LOC_COUNT, 0);
    for (size_t i = 0; i < keys.size; i++) {
        if (!isChestKey(keys[i])) {
            continue;
        }
        nKeys++;
        nRec += sRecordOk[i];
        nAct += sActorOk[i];
        locChest[keys[i].loc] = 1;
        locRec[keys[i].loc] |= sRecordOk[i];
        locAct[keys[i].loc] |= sActorOk[i];
        if (!sRecordOk[i] || !sActorOk[i]) {
            writef(STDERR_FILENO, "[cos] rando-smoke loc: not covered: %s %s/%s %s[%u] (record %d, actor %d)\n",
                   locName(keys[i].loc), keys[i].stage, keys[i].arc, keys[i].tag, keys[i].index, sRecordOk[i], sActorOk[i]);
        }
    }
    int nLoc = 0, nLocRec = 0, nLocAct = 0;
    for (int l = 0; l < PCR_LOC_COUNT; l++) {
        nLoc += locChest[l];
        nLocRec += locRec[l];
        nLocAct += locAct[l];
    }
    writef(STDERR_FILENO, "[cos] rando-smoke loc: coverage: chest keys %d/%d record, %d/%d live; chest locations %d/%d record, %d/%d live\n",
           nRec, nKeys, nAct, nKeys, nLocRec, nLoc, nLocAct, nLoc);
    if (sAll) {
        randoSmokeCheck(kName, "COS_RANDO_LOC_ALL: every chest key's record and live chest checked",
                        nRec == nKeys && nAct == nKeys);
    }
    for (size_t j = 0; j < sOpen.size(); j++) {
        if (!sOpenDone[j]) {
            char what[160];
            snprintf(what, sizeof what, "T2 %s: its chest was live at a stop", locName(sOpen[j]));
            randoSmokeCheck(kName, what, false);
        }
    }
}

void hopOrEnd(unsigned int frames) {
    if (sStop + 1 >= (int)sStops.size()) {
        report();
        randoSmokeEnd(kName);
    }
    sStop++;
    const Stop& s = sStops[sStop];
    writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: to %s room %d point %d layer %d\n", frames, s.stage.c_str(),
           s.room, s.point, s.layer);
    sOldScene = playSceneId();
    dComIfGp_setNextStage(s.stage.c_str(), (s16)s.point, (s8)s.room, (s8)s.layer);
    sState = kArrive;
    sSince = 0;
    sPresent = 0;
}

// A relative COS_RANDO_PLACEMENT that is not there from the working folder (regress.sh lines name
// native/check/rando/fixtures/... from the repo's root) is taken from the repo's root, found from
// this source file's path (relative to the build folder under ninja, so also tried from the build
// folder that holds run.sh's run dir, <build>/runs/<run>). Runs before main, so before R0.7's debug
// boot reads the variable.
struct ResolvePlacement {
    ResolvePlacement() {
        const char* smoke = getenv("COS_SMOKE");
        const char* name = getenv("COS_RANDO_SMOKE");
        const char* path = getenv("COS_RANDO_PLACEMENT");
        if (smoke == nullptr || strcmp(smoke, "rando") != 0 || name == nullptr || strcmp(name, kName) != 0 ||
            path == nullptr || path[0] == '\0' || path[0] == '/' || access(path, R_OK) == 0) {
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
                setenv("COS_RANDO_PLACEMENT", t.c_str(), 1);
                return;
            }
        }
    }
} sResolvePlacement;

} // namespace

void randoSmokeLoc(unsigned int frames) {
    if (sState == kInit) {
        sState = kArrive;
        sStops.push_back({"(boot)", -1, -1, -1});
        sStop = 0;
        parseStops();
        parseOpen();
        const char* all = getenv("COS_RANDO_LOC_ALL");
        sAll = all != nullptr && strcmp(all, "1") == 0;
        sRecordOk.assign(pcr::location_keys().size, 0);
        sActorOk.assign(pcr::location_keys().size, 0);
        writef(STDERR_FILENO, "[cos] rando-smoke loc: %zu stops, %zu chests to open, pc_rando() %d\n", sStops.size(),
               sOpen.size(), pc_rando());
    }
    sSince++;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const fpc_ProcID scene = playSceneId();
    switch (sState) {
    case kInit:
    case kDone:
        return;
    case kArrive:
        if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr) {
            sPresent = 0;
            if (sSince >= kArriveFrames) {
                char what[96];
                snprintf(what, sizeof what, "stop %d (%s) reached", sStop, sStops[sStop].stage.c_str());
                randoSmokeCheck(kName, what, false);
                hopOrEnd(frames);
            }
            return;
        }
        if (++sPresent >= kSettle) {
            sState = kCheck;
            sSince = 0;
            checkStop();
        }
        return;
    case kCheck:
        // T2 at this stop, else on to the next stop.
        if (sPending.empty()) {
            sState = kHop;
            return;
        }
        sOpenIdx = sPending.front().idx;
        sOpenKey = sPending.front().key;
        sOpenChest = sPending.front().chest;
        sOpenPlaced = sPending.front().placed;
        sPending.erase(sPending.begin());
        writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2 %s: chest process %u (%p), player %p, scene %u\n", frames,
               locName(sOpen[sOpenIdx]), (unsigned)sOpenChest, (void*)fopAcM_SearchByID(sOpenChest), (void*)player,
               (unsigned)scene);
        sOpenTries = 0;
        sState = kOpenPrep;
        sSince = 0;
        return;
    case kHop:
        hopOrEnd(frames);
        return;
    case kOpenPrep: {
        daTbox_c* chest = (daTbox_c*)fopAcM_SearchByID(sOpenChest);
        if (chest == nullptr || player == nullptr) {
            writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2: chest process %u %s, player %p, scene %u\n", frames,
                   (unsigned)sOpenChest, chest != nullptr ? "found" : "not found", (void*)player, (unsigned)scene);
            if (sSince < 300) {
                return;
            }
            finishOpen(false, "the chest is gone");
            return;
        }
        if (sSince == 1 && !chest->checkOpen() && !chest->checkNormal()) {
            const int sw = chest->getSwNo();
            writef(STDERR_FILENO, "[cos] rando-smoke loc: T2: chest func %d waits for switch 0x%02X in room %d; set it\n",
                   (int)chest->getFuncType(), sw, (int)chest->mRoomNo);
            if (sw < 0xFF && chest->mRoomNo >= 0) {
                dComIfGs_onSwitch(sw, chest->mRoomNo);
            }
        }
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sSince >= kQuietFrames) {
                finishOpen(false, "an event kept running before the open");
            }
            return;
        }
        if (sSince < 60) {
            // After a put that did not hold, walk at the chest meanwhile (off a ladder, out of a climb).
            if (sOpenTries > 0) {
                stickToward(player, chest->home.pos);
            } else {
                setDrivenPad(true, 0, 0, 0);
            }
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        const s16 yaw = chest->home.angle.y;
        const cXyz pos = frontOf(chest->home.pos, yaw);
        sOpenTarget = pos;
        // As setPlayerPosAndAngle, outside events too (daPy_lk_c::debugPlace, pc_tool_watch.cpp):
        // the player's execute puts him back at its kept position every frame.
        static_cast<daPy_lk_c*>(player)->debugPlace(pos, (s16)(yaw + 0x8000));
        sOpenGot = false;
        sOpenGtItm = -1;
        sOpenEvent[0] = '\0';
        dComIfGp_event_setGtItm(0xFF);
        const int conv = pc_rando() ? pc_rando_convert_progressive_item_id(sOpenPlaced) : sOpenPlaced;
        installOverride(conv);
        writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2 %s: the player put at (%.0f, %.0f, %.0f); item 0x%02X (gives 0x%02X)\n",
               frames, locName(sOpen[sOpenIdx]), pos.x, pos.y, pos.z, sOpenPlaced, conv);
        sState = kOpenWalk;
        sSince = 0;
        return;
    }
    case kOpenWalk: {
        daTbox_c* chest = (daTbox_c*)fopAcM_SearchByID(sOpenChest);
        if (chest == nullptr || player == nullptr) {
            finishOpen(false, "the chest is gone");
            return;
        }
        if (dComIfGp_event_runCheck()) {
            snprintf(sOpenEvent, sizeof sOpenEvent, "%s", eventName());
            writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2: event \"%s\" after %u frames\n", frames, sOpenEvent,
                   sSince);
            sState = kOpenEvent;
            sSince = 0;
            sOpenStartNs = monotonicNs();
            return;
        }
        if (sSince == 3 && player->current.pos.abs(sOpenTarget) > 60.0f && sOpenTries < 4) {
            sOpenTries++;
            writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2: the put did not hold (the player at (%.0f, %.0f, "
                   "%.0f)); walk at the chest and put again (%d)\n", frames, player->current.pos.x, player->current.pos.y,
                   player->current.pos.z, sOpenTries);
            removeOverride();
            sState = kOpenPrep;
            sSince = 2; // past the switch step
            return;
        }
        if (dComIfGp_getDoStatus() == dActStts_OPEN_e) {
            setDrivenPad(true, sSince % 15 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        } else {
            stickToward(player, chest->home.pos);
        }
        if (sSince % 30 == 0) {
            writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2: player at (%.0f, %.0f, %.0f), do status %d\n", frames,
                   player->current.pos.x, player->current.pos.y, player->current.pos.z, (int)dComIfGp_getDoStatus());
        }
        if (sSince >= kWalkFrames) {
            finishOpen(false, "A did not open it");
        }
        return;
    }
    case kOpenEvent: {
        if (dComIfGp_event_getGtItm() != 0xFF) {
            sOpenGtItm = dComIfGp_event_getGtItm(); // the chest sets it (fopAcM_createItemForTrBoxDemo)
        }
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            if ((monotonicNs() - sOpenStartNs) / 1e9 >= kEventSeconds) {
                finishOpen(false, "the event did not end");
            }
            return;
        }
        const int gt = sOpenGtItm;
        writef(STDERR_FILENO, "[cos] rando-smoke loc: frame %u: T2: event \"%s\" ended; item-get 0x%02X, execItemGet(0x%02X) %s\n",
               frames, sOpenEvent, gt, sOpenConv, sOpenGot ? "ran" : "did not run");
        const bool evOk = strncmp(sOpenEvent, "DEFAULT_TREASURE", 16) == 0;
        finishOpen(evOk && gt == sOpenPlaced && sOpenGot,
                   !evOk ? "not the chest's event" : gt != sOpenPlaced ? "wrong item-get" : !sOpenGot ? "no execItemGet" : "");
        return;
    }
    }
}

} // namespace pc
