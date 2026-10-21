// COS_SMOKE=rando COS_RANDO_SMOKE=addchest (docs/RANDOMIZER_PLAN.md step R2.10): the 4 chests wwrando
// adds and their room edits (features/rando/pc_rando_tweak_added_chests.h), at the stops of a run:
//
//   run.sh rando --stage <stage>:<room>[:<point>[:<layer>]] --env COS_RANDO_SMOKE=addchest
//                [--env COS_RANDO_PLACEMENT=<placement.json>]
//                [--env COS_RANDO_ADDCHEST_HOPS=<stage>:<room>:<point>[:<layer>],...]
//                [--env COS_RANDO_ADDCHEST_OPEN=1] [--env COS_RANDO_ADDCHEST_ALL=1]
//
// The run stops at the boot stage, then at every hop (dComIfGp_setNextStage), as the loc smoke does.
// At each stop, kSettle frames after the play scene's player appeared, for every site of the stop's
// stage whose file the game holds (Pjavdou/Room0, sea/Room44, sea/Stage event_list.dat, M_Dra09/Stage,
// M_NewD2/Stage, kenroom/Room0, sea/Room9):
//   with pc_rando() 1 (a placement):
//     - the site's patcher ran and applied (result ok);
//     - the final raw bytes of the file (taken by this smoke's own archive patcher, which runs after
//       every step's patcher) pass pc_rando_tweak_added_chests_verify with the placed item;
//     - the game's stored dzx (dComIfG_getStageRes, as dStage_dt_c parsed it) has the content hash of
//       those final bytes (the game uses the grown copy);
//     - actors: the chest's live TBOX (stay room) holds the placed item; at sea/Room44 the stone door
//       (Obj_Ajav) and the whirlpool (Obj_Auzu) are live by day; in kenroom no Master Sword
//       (Obj_Vmsms) and the Mighty Darknuts (Tn) are live;
//     - T2 with COS_RANDO_ADDCHEST_OPEN=1: the stop's live added chest is opened as the loc smoke's
//       T2 opens a chest (its appear switch set, the player put in front of it, A): its event
//       DEFAULT_TREASURE must give the placed item (dComIfGp_event_getGtItm) and run execItemGet for it;
//   with pc_rando() 0 (vanilla):
//     - no site was patched, the stored dzx has the disc's content hash (kVanilla, the hashes of the
//       USA disc's files, so no entity was added or removed) and no TBOX stands at a chest's place.
// With COS_RANDO_ADDCHEST_ALL=1, after the last stop, every site must have been checked.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_added_chests.h"

#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_tbox.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "d/d_stage.h"
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

#define PC_RANDO_PATCHER(fn) extern "C" void fn(void);
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER

namespace pc {

namespace {

constexpr const char* kName = "addchest";
constexpr unsigned int kSettle = 30;
constexpr unsigned int kArriveFrames = 1500;
constexpr unsigned int kQuietFrames = 900;
constexpr unsigned int kWalkFrames = 300;
constexpr double kEventSeconds = 90.0;
constexpr unsigned int kTalkFrames = 45;
constexpr float kFront = 130.0f;
constexpr float kPosEps = 1.0f;

// The content hashes (pc_rando_tweak_added_chests_content_hash) of the USA disc's files, per site
// (computed from GZLE01 with the R2.10 test's disc mode; 0: not a dzx).
constexpr uint64_t kVanilla[PCR_AC_SITE_COUNT] = {
    0x43c6e535591181eaull, // Pjavdou/Room0
    0xa669f268bef93317ull, // sea/Room44
    0,                     // sea/Stage event_list.dat
    0x2a6d672855819d98ull, // M_Dra09/Stage
    0xe6832876ea22d905ull, // M_NewD2/Stage
    0x22565c17f8a207e9ull, // kenroom/Room0
    0xf3dead31be366797ull, // sea/Room9
};

struct Stop {
    std::string stage;
    int room, point, layer;
};

enum State { kInit, kArrive, kCheck, kOpenPrep, kOpenWalk, kOpenEvent, kHop };
State sState = kInit;
std::vector<Stop> sStops;
int sStop = -1;
unsigned int sSince = 0;
unsigned int sPresent = 0;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
bool sOpenWanted = false;
bool sAll = false;
bool sChecked[PCR_AC_SITE_COUNT] = {};
bool sOpened[PCR_AC_SITE_COUNT] = {};
// What this smoke's own patcher saw of each site's file at its last load (its final raw bytes),
// checked right there: global operator new allocates on the current JKRHeap, which during a load is
// the archive's heap and is freed with it, so no copy of the bytes is kept.
struct Seen {
    bool seen;
    bool ok;          // pc_rando_tweak_added_chests_verify held
    char why[160];    // its first failed check
    uint64_t hash;    // dzx: pc_rando_tweak_added_chests_content_hash
};
Seen sSeen[PCR_AC_SITE_COUNT] = {};

// T2
// Sites whose chest is live at this stop (a fixed array: see Seen).
int sPending[PCR_AC_SITE_COUNT];
int sPendingN = 0;
int sOpenSite = -1;
fpc_ProcID sOpenChest = fpcM_ERROR_PROCESS_ID_e;
uint8_t sOpenPlaced = 0;
int sOpenConv = -1;
bool sOpenGot = false;
int sOpenGtItm = -1;
char sOpenEvent[48] = "";
uint64_t sOpenStartNs = 0;
cXyz sOpenTarget;
int sOpenTries = 0;
PcRandoItemFunc sPrevOverride = nullptr;

int siteOfFile(const PcRandoResFile* f) {
    for (int i = 0; i < PCR_AC_SITE_COUNT; i++) {
        const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(i);
        if (strcmp(s->stage, f->stage) == 0 && strcmp(s->arc, f->arc) == 0 && strcmp(s->name, f->name) == 0) {
            return i;
        }
    }
    return -1;
}

uint8_t placedItem(int site);

// Runs after every patcher: checks the file's final raw bytes.
void observe(PcRandoResFile* f) {
    if (f == nullptr || f->stage == nullptr || f->arc == nullptr || f->name == nullptr) {
        return;
    }
    const int site = siteOfFile(f);
    if (site < 0) {
        return;
    }
    Seen& o = sSeen[site];
    o.seen = true;
    o.why[0] = '\0';
    const uint8_t item = site == PCR_AC_MEDLI_DUMMY ? 0xFF : placedItem(site);
    o.ok = pc_rando_tweak_added_chests_verify(site, f->data, f->size, item, o.why, sizeof o.why) != 0;
    o.hash = pc_rando_tweak_added_chests_site(site)->is_event ? 0 : pc_rando_tweak_added_chests_content_hash(f->data, f->size);
}

void* findPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(findPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

uint8_t placedItem(int site) {
    const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(site);
    if (s->loc < 0) {
        return 0xFF;
    }
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys(s->loc);
    return keys.size > 0 ? pc_rando_loc_item(s->loc, keys[0].vanilla) : 0xFF;
}

// The content hash of a stored (possibly relocated) dzx, chunk by chunk as the raw one.
uint64_t storedHash(void* file) {
    dStage_fileHeader* hdr = (dStage_fileHeader*)file;
    const int n = hdr->m_chunkCount;
    if (n <= 0 || n > 256) {
        return 0;
    }
    uint64_t h = PCR_AC_HASH_SEED;
    for (int i = 0; i < n; i++) {
        dStage_nodeHeader& node = hdr->m_nodes[i];
        uint32_t t = node.m_tag;
        const char* tag = (const char*)&t;
        const uint8_t* data;
        if (node.m_offset.isRelocated()) {
            data = (const uint8_t*)node.m_offset;
        } else {
            const s32 off = node.m_offset.value;
            data = off != 0 ? (const uint8_t*)file + off : nullptr;
        }
        h = pc_rando_tweak_added_chests_hash_chunk(h, tag, (uint32_t)(int)node.m_entryNum, data);
    }
    return h;
}

struct Procs {
    s16 name;
    int n;
    fopAc_ac_c* list[64];
};

void* collect(void* proc, void* data) {
    Procs* c = static_cast<Procs*>(data);
    if (fpcM_GetName(proc) == c->name && c->n < 64) {
        c->list[c->n++] = static_cast<fopAc_ac_c*>(proc);
    }
    return nullptr;
}

Procs procsNamed(s16 name) {
    Procs p = {};
    p.name = name;
    fopAcM_Search(collect, &p);
    return p;
}

daTbox_c* chestAt(const cXyz& pos) {
    Procs c = procsNamed(fpcNm_TBOX_e);
    for (int i = 0; i < c.n; i++) {
        const cXyz& h = c.list[i]->home.pos;
        if (std::fabs(h.x - pos.x) < kPosEps && std::fabs(h.y - pos.y) < kPosEps && std::fabs(h.z - pos.z) < kPosEps) {
            return static_cast<daTbox_c*>(c.list[i]);
        }
    }
    return nullptr;
}

// The chest of a site (the dummy for M_NewD2): its built entry's position and room.
bool chestOf(int site, cXyz* pos, int* room) {
    const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(site);
    const int added = site == PCR_AC_MEDLI_DUMMY ? PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST : s->added;
    uint8_t e[0x20];
    if (!pc_rando_added_chest_entry(added, 0xFF, e)) {
        return false;
    }
    *pos = cXyz(pc_rando_dzx_bef32(e + 0x0C), pc_rando_dzx_bef32(e + 0x10), pc_rando_dzx_bef32(e + 0x14));
    *room = pc_rando_dzx_be16(e + 0x18) & 0x3F;
    return true;
}

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

void stickToward(fopAc_ac_c* player, const cXyz& c) {
    const s16 a = cM_atan2s(c.x - player->current.pos.x, c.z - player->current.pos.z);
    const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
    setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 70.0f), (int8_t)(-cM_scos(st) * 70.0f));
}

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

const char* siteName(int site) {
    static char buf[64];
    const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(site);
    snprintf(buf, sizeof buf, "%s/%s/%s", s->stage, s->arc, s->name);
    return buf;
}

void checkActors(int site, int stay) {
    char what[200];
    switch (site) {
    case PCR_AC_JABUN_DOOR:
        if (stay == 44) {
            randoSmokeCheck(kName, "sea/Room44: the stone door (Obj_Ajav) is live", procsNamed(fpcNm_Obj_Ajav_e).n > 0);
            randoSmokeCheck(kName, "sea/Room44: the whirlpool (Obj_Auzu) is live", procsNamed(fpcNm_Obj_Auzu_e).n > 0);
        }
        break;
    case PCR_AC_JABUN_ROOM:
        randoSmokeCheck(kName, "Pjavdou: the raft (Obj_Ikada) is live", procsNamed(fpcNm_OBJ_IKADA_e).n > 0);
        break;
    case PCR_AC_MASTER_SWORD: {
        const int nMs = procsNamed(fpcNm_Obj_Vmsms_e).n, nTn = procsNamed(fpcNm_TN_e).n;
        snprintf(what, sizeof what, "kenroom: no Master Sword (%d Obj_Vmsms), Mighty Darknuts live (%d Tn)", nMs, nTn);
        randoSmokeCheck(kName, what, nMs == 0 && nTn >= 2);
        break;
    }
    default:
        break;
    }
}

void checkStop() {
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    writef(STDERR_FILENO, "[cos] rando-smoke addchest: stop %d: stage %s, stay room %d, layer %d, pc_rando() %d\n", sStop,
           stage, stay, dComIfG_play_c::getLayerNo(-1), pc_rando());
    char what[220];
    for (int site = 0; site < PCR_AC_SITE_COUNT; site++) {
        const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(site);
        if (strcmp(s->stage, stage) != 0) {
            continue;
        }
        if (s->is_event) {
            if (!pc_rando()) {
                snprintf(what, sizeof what, "%s: not patched", siteName(site));
                randoSmokeCheck(kName, what, s->runs == 0);
            } else {
                const Seen& o = sSeen[site];
                snprintf(what, sizeof what, "%s: patched (%d runs, %s) and the final bytes hold the edits%s%s", siteName(site),
                         s->runs, pc_rando_tweak_added_chests_result_name(site), o.why[0] ? ": " : "", o.why);
                randoSmokeCheck(kName, what, s->runs > 0 && s->result == 0 && o.seen && o.ok);
            }
            sChecked[site] = true;
            continue;
        }
        void* file = dComIfG_getStageRes(s->arc, s->name);
        if (file == nullptr) {
            continue; // not loaded at this stop
        }
        const uint64_t stored = storedHash(file);
        cXyz pos;
        int room = -1;
        const bool hasChest = chestOf(site, &pos, &room);
        const bool roomLive = strcmp(s->arc, "Stage") == 0 ? room == stay : atoi(s->arc + 4) == stay;
        if (!pc_rando()) {
            snprintf(what, sizeof what, "%s: not patched; stored content hash %016llx is the disc's", siteName(site),
                     (unsigned long long)stored);
            randoSmokeCheck(kName, what, s->runs == 0 && stored == kVanilla[site]);
            if (hasChest && roomLive && site != PCR_AC_MEDLI_DUMMY) {
                snprintf(what, sizeof what, "%s: no chest at (%.0f, %.0f, %.0f)", siteName(site), pos.x, pos.y, pos.z);
                randoSmokeCheck(kName, what, chestAt(pos) == nullptr);
            }
            sChecked[site] = true;
            continue;
        }
        const uint8_t item = site == PCR_AC_MEDLI_DUMMY ? 0xFF : placedItem(site);
        const Seen& o = sSeen[site];
        snprintf(what, sizeof what, "%s: patched (%d runs, %s), the final bytes hold the edits (item 0x%02X)%s%s", siteName(site),
                 s->runs, pc_rando_tweak_added_chests_result_name(site), item, o.why[0] ? ": " : "", o.why);
        randoSmokeCheck(kName, what, s->runs > 0 && s->result == 0 && o.seen && o.ok);
        snprintf(what, sizeof what, "%s: the game holds the patched file (stored hash %016llx, patched %016llx, disc's %016llx)",
                 siteName(site), (unsigned long long)stored, (unsigned long long)o.hash, (unsigned long long)kVanilla[site]);
        randoSmokeCheck(kName, what, o.seen && stored == o.hash && stored != kVanilla[site]);
        sChecked[site] = true;
        checkActors(site, stay);
        if (!hasChest || !roomLive || site == PCR_AC_MEDLI_DUMMY) {
            continue;
        }
        daTbox_c* chest = chestAt(pos);
        snprintf(what, sizeof what, "%s: live chest at (%.0f, %.0f, %.0f) holds 0x%02X (holds 0x%02X)", siteName(site), pos.x,
                 pos.y, pos.z, item, chest != nullptr ? chest->getItemNo() : -1);
        const bool actOk = chest != nullptr && chest->getItemNo() == item;
        randoSmokeCheck(kName, what, actOk);
        if (actOk) {
            writef(STDERR_FILENO, "[cos] rando-smoke addchest:   chest room %d, params 0x%08X, func %d, sw 0x%02X, open %d\n",
                   (int)chest->mRoomNo, (unsigned)fopAcM_GetParam(chest), (int)chest->getFuncType(), chest->getSwNo(),
                   (int)chest->checkOpen());
        }
        if (actOk && sOpenWanted && !sOpened[site]) {
            sPending[sPendingN++] = site;
        }
    }
}

void parseStops() {
    const char* list = getenv("COS_RANDO_ADDCHEST_HOPS");
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
            randoSmokeCheck(kName, "COS_RANDO_ADDCHEST_HOPS parses (stage:room:point[:layer])", false);
        }
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
}

void report() {
    for (int site = 0; site < PCR_AC_SITE_COUNT; site++) {
        writef(STDERR_FILENO, "[cos] rando-smoke addchest: %s: %s%s\n", siteName(site), sChecked[site] ? "checked" : "not checked",
               sOpened[site] ? ", opened" : "");
    }
    if (sAll) {
        bool all = true;
        for (bool c : sChecked) {
            all = all && c;
        }
        randoSmokeCheck(kName, "COS_RANDO_ADDCHEST_ALL: every site checked", all);
        if (sOpenWanted && pc_rando()) {
            const int chests[] = {PCR_AC_JABUN_ROOM, PCR_AC_MEDLI_CHEST, PCR_AC_MASTER_SWORD, PCR_AC_QUEEN_FAIRY};
            bool opened = true;
            for (int c : chests) {
                opened = opened && sOpened[c];
            }
            randoSmokeCheck(kName, "COS_RANDO_ADDCHEST_ALL: every added chest opened", opened);
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
    writef(STDERR_FILENO, "[cos] rando-smoke addchest: frame %u: to %s room %d point %d layer %d\n", frames, s.stage.c_str(),
           s.room, s.point, s.layer);
    sOldScene = playSceneId();
    dComIfGp_setNextStage(s.stage.c_str(), (s16)s.point, (s8)s.room, (s8)s.layer);
    sState = kArrive;
    sSince = 0;
    sPresent = 0;
}

void finishOpen(bool ok, const char* why) {
    char what[160];
    snprintf(what, sizeof what, "T2 %s: chest opened and gave 0x%02X%s%s", siteName(sOpenSite), sOpenPlaced, why[0] ? ": " : "",
             why);
    randoSmokeCheck(kName, what, ok);
    removeOverride();
    setDrivenPad(true, 0, 0, 0);
    sOpened[sOpenSite] = ok;
    sOpenSite = -1;
    sOpenChest = fpcM_ERROR_PROCESS_ID_e;
    sState = kCheck;
    sSince = 0;
}

// A relative COS_RANDO_PLACEMENT is taken from the repo's root (as the loc smoke does), before main.
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

void randoSmokeAddchest(unsigned int frames) {
    if (sState == kInit) {
        // Every step's patcher first (idempotent; the game side would register them before its first
        // dispatch), then the observer, so it sees each file's final bytes.
#define PC_RANDO_PATCHER(fn) fn();
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER
        pc_rando_register_res_patcher(observe, PCR_RES_ANY);
        sStops.push_back({"(boot)", -1, -1, -1});
        sStop = 0;
        parseStops();
        const char* open = getenv("COS_RANDO_ADDCHEST_OPEN");
        sOpenWanted = open != nullptr && strcmp(open, "1") == 0;
        const char* all = getenv("COS_RANDO_ADDCHEST_ALL");
        sAll = all != nullptr && strcmp(all, "1") == 0;
        PcRandoResStats st;
        pc_rando_res_stats(&st);
        writef(STDERR_FILENO, "[cos] rando-smoke addchest: %zu stops, open %d, pc_rando() %d, %u files dispatched before\n",
               sStops.size(), sOpenWanted, pc_rando(), st.dispatched);
        sState = kArrive;
    }
    sSince++;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const fpc_ProcID scene = playSceneId();
    switch (sState) {
    case kInit:
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
    case kCheck: {
        if (sPendingN == 0) {
            sState = kHop;
            return;
        }
        sOpenSite = sPending[0];
        for (int i = 1; i < sPendingN; i++) {
            sPending[i - 1] = sPending[i];
        }
        sPendingN--;
        cXyz pos;
        int room;
        chestOf(sOpenSite, &pos, &room);
        daTbox_c* chest = chestAt(pos);
        sOpenChest = chest != nullptr ? fopAcM_GetID(chest) : fpcM_ERROR_PROCESS_ID_e;
        sOpenPlaced = placedItem(sOpenSite);
        sOpenTries = 0;
        sState = kOpenPrep;
        sSince = 0;
        return;
    }
    case kHop:
        hopOrEnd(frames);
        return;
    case kOpenPrep: {
        daTbox_c* chest = (daTbox_c*)fopAcM_SearchByID(sOpenChest);
        if (chest == nullptr || player == nullptr) {
            if (sSince < 300) {
                return;
            }
            finishOpen(false, "the chest is gone");
            return;
        }
        if (sSince == 1 && !chest->checkOpen() && !chest->checkNormal()) {
            const int sw = chest->getSwNo();
            writef(STDERR_FILENO, "[cos] rando-smoke addchest: T2: chest func %d waits for switch 0x%02X in room %d; set it\n",
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
        static_cast<daPy_lk_c*>(player)->debugPlace(pos, (s16)(yaw + 0x8000));
        sOpenGot = false;
        sOpenGtItm = -1;
        sOpenEvent[0] = '\0';
        dComIfGp_event_setGtItm(0xFF);
        const int conv = pc_rando_convert_progressive_item_id(sOpenPlaced);
        installOverride(conv);
        writef(STDERR_FILENO, "[cos] rando-smoke addchest: frame %u: T2 %s: the player put at (%.0f, %.0f, %.0f); item 0x%02X (gives 0x%02X)\n",
               frames, siteName(sOpenSite), pos.x, pos.y, pos.z, sOpenPlaced, conv);
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
            writef(STDERR_FILENO, "[cos] rando-smoke addchest: frame %u: T2: event \"%s\" after %u frames\n", frames, sOpenEvent,
                   sSince);
            sState = kOpenEvent;
            sSince = 0;
            sOpenStartNs = monotonicNs();
            return;
        }
        if (sSince == 3 && player->current.pos.abs(sOpenTarget) > 60.0f && sOpenTries < 4) {
            sOpenTries++;
            writef(STDERR_FILENO, "[cos] rando-smoke addchest: frame %u: T2: the put did not hold (the player at (%.0f, %.0f, "
                   "%.0f)); walk at the chest and put again (%d)\n", frames, player->current.pos.x, player->current.pos.y,
                   player->current.pos.z, sOpenTries);
            removeOverride();
            sState = kOpenPrep;
            sSince = 2;
            return;
        }
        if (dComIfGp_getDoStatus() == dActStts_OPEN_e) {
            setDrivenPad(true, sSince % 15 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        } else {
            stickToward(player, chest->home.pos);
        }
        if (sSince % 30 == 0) {
            writef(STDERR_FILENO, "[cos] rando-smoke addchest: frame %u: T2: player at (%.0f, %.0f, %.0f), do status %d\n", frames,
                   player->current.pos.x, player->current.pos.y, player->current.pos.z, (int)dComIfGp_getDoStatus());
        }
        if (sSince >= kWalkFrames) {
            finishOpen(false, "A did not open it");
        }
        return;
    }
    case kOpenEvent: {
        if (dComIfGp_event_getGtItm() != 0xFF) {
            sOpenGtItm = dComIfGp_event_getGtItm();
        }
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            if ((monotonicNs() - sOpenStartNs) / 1e9 >= kEventSeconds) {
                finishOpen(false, "the event did not end");
            }
            return;
        }
        const int gt = sOpenGtItm;
        writef(STDERR_FILENO, "[cos] rando-smoke addchest: frame %u: T2: event \"%s\" ended; item-get 0x%02X, execItemGet(0x%02X) %s\n",
               frames, sOpenEvent, gt, sOpenConv, sOpenGot ? "ran" : "did not run");
        const bool evOk = strncmp(sOpenEvent, "DEFAULT_TREASURE", 16) == 0;
        finishOpen(evOk && gt == sOpenPlaced && sOpenGot,
                   !evOk ? "not the chest's event" : gt != sOpenPlaced ? "wrong item-get" : !sOpenGot ? "no execItemGet" : "");
        return;
    }
    }
}

} // namespace pc
