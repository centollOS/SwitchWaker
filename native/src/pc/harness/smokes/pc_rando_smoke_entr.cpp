// COS_SMOKE=rando COS_RANDO_SMOKE=entr (docs/RANDOMIZER_PLAN.md step R6.12): the entrance randomizer's
// stage data (features/rando/pc_rando_entr_stage.h), run as
//   run.sh rando --stage <stage>:<room>:<point> --env COS_RANDO_SMOKE=entr
//                [--env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R6_12_seed_<k>.json]
// (the lines are in native/check/regress_rando/R6.12.txt).
//
// 1. The sweep (always). Once the play scene has run 10 frames, every stage file of
//    fixtures/R6_12_wwrando_entr.txt ("file" lines: the stage.dzs and room.dzr files any entrance
//    edit can touch) is read from the disc into a private test heap (JKRMemArchive::mountFixed, out of
//    the volume list, as smokes/pc_rando_smoke_evser.cpp does), a few archives a frame, copied, and
//    patched with pcr::entrStagePatch for each map of the fixture (A, B, C: wwrando seeds; D: the
//    Fire Mountain / Ice Ring Isle swap; V: no entrance option, which must give no map). The records
//    that changed (SCLS dest/room/spawn, PLYR params, 2DMA sector byte, ACTR name and params, per
//    chunk tag and index) must be exactly the fixture's for that map and file (what wwrando's
//    EntranceRandomizer._save changes on the disc, R6_12_make_fixture.py), no other byte may change,
//    and no edit may be refused (D9). Each dungeon start of the patcher (kDungeonStarts) must be the
//    SCLS[0] of its stage.dzs.
// 2. In play:
//    - without a placement (pc_rando() 0): the patcher saw no file;
//    - with R0_6_identity.json (entrance rando off): the patcher found no map and wrote nothing;
//    - with an R6_12_seed_<k>.json: the patcher refused nothing and found every entrance spawn
//      (R3.16's spawns 27 included), and the live stage data of the boot stage and room hold the
//      fixture's records for map k: the stage's and the room's SCLS and PLYR params
//      (dComIfGp_getStage / the room's dStage_roomDt_c), the stage's 2DMA (mOceanXZ), and for a
//      VolTag record the VolTag actor (gone when its name was cleared, else with the new params).
//    Then the game runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_entr_stage.h"
#include "features/rando/pc_rando_event_ser.h" // EvsVec, EvsString (malloc memory)
#include "features/rando/pc_rando_seedfile.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

using pcr::EvsString;
using pcr::EvsVec;

constexpr const char* kName = "entr";
constexpr const char* kFixture = "R6_12_wwrando_entr.txt";
constexpr int kFilesPerFrame = 6;

struct FileLine {
    char stage[16];
    int room;
};

struct MapLine {
    char key[4];
    char file[64];
    PcRandoSeed* seed; // malloc'd placement (new on the game heap is fine for the life of the run)
    bool valid;
    pcr::EntrStageMap map;
};

bool sStarted = false, sSweepDone = false, sPlayChecked = false;
unsigned int sEndFrame = 0;
EvsString sDir;
EvsVec<FileLine> sFiles;
EvsVec<MapLine> sMaps;
EvsVec<EvsString> sRecords; // the fixture's record lines
size_t sNext = 0;
JKRExpHeap* sHeap = nullptr;
int sChecked = 0, sMatched = 0, sClean = 0, sRefused = 0, sNoMount = 0, sStartOk = 0, sStartSeen = 0;
int sNoSpawn = 0, sNoSpawnElsewhere = 0;

// The files where R3.16 (add_spawns_outside_boss_doors) adds the spawn 27 of a boss entrance: the raw
// disc file lacks it, so b finds nothing there (kindan/Room16's boss entrance uses its own spawn 1).
bool isBossDoorSpawnFile(const char* stage, int room) {
    static const struct {
        const char* stage;
        int room;
    } kFiles[] = {{"M_NewD2", 10}, {"Siren", 18}, {"sea", 1}, {"M_Dai", -1}, {"kaze", -1}};
    for (const auto& f : kFiles) {
        if (f.room == room && strcmp(f.stage, stage) == 0) {
            return true;
        }
    }
    return false;
}
size_t sRecordsSeen = 0;
// The PLYR count of each swept stage's stage.dzs (the "file" lines list a stage's stage.dzs first).
EvsVec<EvsString> sPlyrStage;
EvsVec<int> sPlyrCount;

// --- the fixture folder (as smokes/pc_rando_smoke_evser.cpp) ---

bool readFile(const char* path, EvsString& text) {
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        text.append(buf, n);
    }
    std::fclose(f);
    return true;
}

bool findFixtureDir(EvsString& text) {
    char dirs[2][1024];
    int nDirs = 0;
    const char* env = std::getenv("COS_RANDO_FIXTURES");
    if (env != nullptr && env[0] != '\0') {
        snprintf(dirs[nDirs++], sizeof dirs[0], "%s", env);
    } else {
        char file[1024];
        snprintf(file, sizeof file, "%s", __FILE__);
        for (int up = 0; up < 5; up++) {
            char* slash = strrchr(file, '/');
            if (slash == nullptr) {
                return false;
            }
            *slash = '\0';
        }
        snprintf(dirs[nDirs++], sizeof dirs[0], "%s/check/rando/fixtures", file);
        const char* run = std::getenv("COS_RUN_DIR");
        if (file[0] != '/' && run != nullptr && run[0] != '\0') {
            snprintf(dirs[nDirs++], sizeof dirs[0], "%s/../../%s/check/rando/fixtures", run, file);
        }
    }
    for (int i = 0; i < nDirs; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", dirs[i], kFixture);
        if (readFile(path, text)) {
            sDir = dirs[i];
            return true;
        }
        writef(STDERR_FILENO, "[cos] rando-smoke entr: cannot read %s\n", path);
    }
    return false;
}

bool parseFixture() {
    EvsString text;
    if (!findFixtureDir(text)) {
        return false;
    }
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == EvsString::npos) {
            eol = text.size();
        }
        const EvsString line = text.substr(pos, eol - pos);
        pos = eol + 1;
        FileLine fl = {};
        MapLine ml = {};
        if (sscanf(line.c_str(), "file %15s %d", fl.stage, &fl.room) == 2) {
            sFiles.push_back(fl);
        } else if (sscanf(line.c_str(), "map %3s %63s", ml.key, ml.file) == 2) {
            sMaps.push_back(ml);
        } else if (!line.empty()) {
            sRecords.push_back(line);
        }
    }
    return !sFiles.empty() && !sMaps.empty() && !sRecords.empty();
}

bool loadMaps() {
    bool ok = true;
    for (MapLine& m : sMaps) {
        EvsString path = sDir + "/" + m.file;
        m.seed = new PcRandoSeed();
        std::string err;
        if (!pc_rando_seed_load_file(path.c_str(), m.seed, &err)) {
            writef(STDERR_FILENO, "[cos] rando-smoke entr: %s: %s\n", path.c_str(), err.c_str());
            ok = false;
            continue;
        }
        m.valid = pcr::entrStageMapFromSeed(*m.seed, m.map, &err);
        const bool want = strcmp(m.key, "V") != 0;
        if (m.valid != want || (!want && !err.empty())) {
            writef(STDERR_FILENO, "[cos] rando-smoke entr: map %s (%s): %s (%s)\n", m.key, m.file,
                   m.valid ? "a map" : "no map", err.c_str());
            ok = false;
        }
    }
    return ok;
}

// --- the records of a patched file ---

void fmtStage(char* out, size_t n, const uint8_t* e) {
    char s[9];
    memcpy(s, e, 8);
    s[8] = '\0';
    snprintf(out, n, "%s", s);
}

// The record lines (fixture format) of the fields that differ between raw and patched, and whether
// every other byte is the same.
bool diffRecords(const char* key, const char* stage, int room, const uint8_t* raw, const uint8_t* out, size_t size,
                 EvsVec<EvsString>& recs) {
    uint8_t* undo = (uint8_t*)malloc(size);
    memcpy(undo, out, size);
    PcRandoDzx a, b;
    pc_rando_dzx_open(&a, (void*)raw, size);
    pc_rando_dzx_open(&b, undo, size);
    for (uint32_t c = 0; c < a.chunkCount; c++) {
        char tag[5];
        memcpy(tag, a.data + 4 + c * 12, 4);
        tag[4] = '\0';
        const bool scls = strcmp(tag, "SCLS") == 0, plyr = strcmp(tag, "PLYR") == 0, dma = strcmp(tag, "2DMA") == 0;
        const bool actr = strncmp(tag, "ACT", 3) == 0;
        if (!scls && !plyr && !dma && !actr) {
            continue;
        }
        const uint32_t n = pc_rando_dzx_count(&a, tag);
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t* r = pc_rando_dzx_entry(&a, tag, i);
            uint8_t* p = pc_rando_dzx_entry(&b, tag, i);
            if (r == nullptr || p == nullptr) {
                continue;
            }
            char line[160];
            line[0] = '\0';
            if (scls && memcmp(r, p, 10) != 0) {
                char s[16];
                fmtStage(s, sizeof s, p);
                snprintf(line, sizeof line, "scls %s %s %d %s %u %s %u %u", key, stage, room, tag, (unsigned)i, s,
                         (unsigned)p[9], (unsigned)p[8]);
                memcpy(p, r, 10);
            } else if (plyr && memcmp(r + 8, p + 8, 4) != 0) {
                snprintf(line, sizeof line, "plyr %s %s %d %s %u %08x", key, stage, room, tag, (unsigned)i,
                         (unsigned)pc_rando_dzx_be32(p + 8));
                memcpy(p + 8, r + 8, 4);
            } else if (dma && r[0x36] != p[0x36]) {
                snprintf(line, sizeof line, "dma %s %s %d %s %u %02x", key, stage, room, tag, (unsigned)i,
                         (unsigned)p[0x36]);
                p[0x36] = r[0x36];
            } else if (actr && memcmp(r, p, 12) != 0) {
                char s[16];
                fmtStage(s, sizeof s, p);
                snprintf(line, sizeof line, "actr %s %s %d %s %u %s %08x", key, stage, room, tag, (unsigned)i,
                         s[0] != '\0' ? s : "-", (unsigned)pc_rando_dzx_be32(p + 8));
                memcpy(p, r, 12);
            }
            if (line[0] != '\0') {
                recs.push_back(EvsString(line));
            }
        }
    }
    const bool clean = memcmp(undo, raw, size) == 0;
    free(undo);
    return clean;
}

// The fixture's records for (map, stage, room).
void fixtureRecords(const char* key, const char* stage, int room, EvsVec<EvsString>& out) {
    char prefix[64];
    for (const EvsString& rec : sRecords) {
        const size_t sp = rec.find(' ');
        if (sp == EvsString::npos) {
            continue;
        }
        snprintf(prefix, sizeof prefix, " %s %s %d ", key, stage, room);
        if (rec.compare(sp, strlen(prefix), prefix) == 0) {
            out.push_back(rec);
        }
    }
}

bool sameSet(EvsVec<EvsString> a, EvsVec<EvsString> b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const EvsString& s : a) {
        bool found = false;
        for (size_t i = 0; i < b.size(); i++) {
            if (b[i] == s) {
                b.erase(b.begin() + (long)i);
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

int stagePlyrOf(const char* stage) {
    for (size_t i = 0; i < sPlyrStage.size(); i++) {
        if (sPlyrStage[i] == stage) {
            return sPlyrCount[i];
        }
    }
    return -1;
}

void checkFile(const FileLine& fl, const uint8_t* raw, size_t size) {
    if (fl.room < 0) {
        sPlyrStage.push_back(EvsString(fl.stage));
        sPlyrCount.push_back(pcr::entrStagePlyrCount(raw, size));
        const pcr::EntrSclsDest* d = pcr::entrStageDungeonStart(fl.stage);
        if (d != nullptr) {
            sStartSeen++;
            PcRandoDzx dzx;
            const uint8_t* e = pc_rando_dzx_open(&dzx, (void*)raw, size) == PCR_DZX_OK
                                   ? pc_rando_dzx_entry(&dzx, "SCLS", 0)
                                   : nullptr;
            const bool ok = e != nullptr && pcr::entrSclsEquals(e, d->stage, d->room, d->spawn);
            sStartOk += ok ? 1 : 0;
            if (!ok) {
                writef(STDERR_FILENO, "[cos] rando-smoke entr: %s stage.dzs SCLS[0] is not the dungeon start\n",
                       fl.stage);
            }
        }
    }
    const int stagePlyr = fl.room < 0 ? -1 : stagePlyrOf(fl.stage);
    uint8_t* work = (uint8_t*)malloc(size);
    for (MapLine& m : sMaps) {
        sChecked++;
        memcpy(work, raw, size);
        pcr::EntrStagePatchStats st;
        bool parsed = true;
        if (m.valid) {
            parsed = pcr::entrStagePatch(m.map, fl.stage, fl.room, work, size, stagePlyr, &st);
        }
        EvsVec<EvsString> got, want;
        const bool clean = diffRecords(m.key, fl.stage, fl.room, raw, work, size, got);
        fixtureRecords(m.key, fl.stage, fl.room, want);
        sRecordsSeen += want.size();
        const bool match = parsed && sameSet(got, want);
        sMatched += match ? 1 : 0;
        sClean += clean ? 1 : 0;
        sRefused += st.refused;
        sNoSpawn += st.noSpawn;
        sNoSpawnElsewhere += isBossDoorSpawnFile(fl.stage, fl.room) ? 0 : st.noSpawn;
        if (!match || !clean || st.refused != 0) {
            writef(STDERR_FILENO,
                   "[cos] rando-smoke entr: map %s %s/%d: %zu record(s), fixture %zu, other bytes %s, %d written, "
                   "%d refused\n",
                   m.key, fl.stage, fl.room, got.size(), want.size(), clean ? "same" : "CHANGED", st.written,
                   st.refused);
            for (const EvsString& g : got) {
                writef(STDERR_FILENO, "[cos] rando-smoke entr:   got  %s\n", g.c_str());
            }
            for (const EvsString& w : want) {
                writef(STDERR_FILENO, "[cos] rando-smoke entr:   want %s\n", w.c_str());
            }
        }
    }
    free(work);
}

void sweepOne(const FileLine& fl) {
    char path[128];
    if (fl.room < 0) {
        snprintf(path, sizeof path, "/res/Stage/%s/Stage.arc", fl.stage);
    } else {
        snprintf(path, sizeof path, "/res/Stage/%s/Room%d.arc", fl.stage, fl.room);
    }
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        sNoMount++;
        writef(STDERR_FILENO, "[cos] rando-smoke entr: %s: cannot mount a private copy\n", path);
        sHeap->freeAll();
        return;
    }
    for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst(); l != nullptr; l = l->getNext()) {
        if (l->getObject() == arc) {
            JKRFileLoader::getVolumeList().remove(l);
            break;
        }
    }
    void* res = arc->getResource(0, fl.room < 0 ? "stage.dzs" : "room.dzr");
    if (res != nullptr) {
        const size_t size = arc->getResSize(res);
        uint8_t* raw = (uint8_t*)malloc(size);
        memcpy(raw, res, size);
        arc->unmountFixed();
        sHeap->freeAll();
        checkFile(fl, raw, size);
        free(raw);
        return;
    }
    sNoMount++;
    writef(STDERR_FILENO, "[cos] rando-smoke entr: %s: no %s\n", path, fl.room < 0 ? "stage.dzs" : "room.dzr");
    arc->unmountFixed();
    sHeap->freeAll();
}

// --- in play ---

int countActors(s16 name, u32* params) {
    struct Ctx {
        s16 name;
        int n;
        u32* params;
    } ctx = {name, 0, params};
    fopAcM_Search(
        [](void* proc, void* data) -> void* {
            Ctx* c = static_cast<Ctx*>(data);
            if (fpcM_GetName(proc) == c->name) {
                if (c->n == 0 && c->params != nullptr) {
                    *c->params = fopAcM_GetParam(proc);
                }
                c->n++;
            }
            return nullptr;
        },
        &ctx);
    return ctx.n;
}

const MapLine* activeMap() {
    const PcRandoSeed* seed = pc_rando_seed();
    if (seed == nullptr) {
        return nullptr;
    }
    for (const MapLine& m : sMaps) {
        if (m.seed != nullptr && memcmp(m.seed->hash, seed->hash, sizeof seed->hash) == 0) {
            return &m;
        }
    }
    return nullptr;
}

// One live record of the boot stage / room against the fixture.
bool checkLive(const char* rec, const char* stage, int room) {
    char kind[8], key[4], st[16], tag[8], a[16];
    int rm = 0;
    unsigned idx = 0, b = 0, c = 0;
    if (sscanf(rec, "%7s %3s %15s %d %7s %u", kind, key, st, &rm, tag, &idx) != 6) {
        return false;
    }
    if (strcmp(kind, "scls") == 0 && sscanf(rec, "%*s %*s %*s %*d %*s %*u %15s %u %u", a, &b, &c) == 3) {
        stage_scls_info_dummy_class* scls = rm < 0 ? dComIfGp_getStage().getSclsInfo()
                                                   : dComIfGp_roomControl_getStatusRoomDt(room)->getSclsInfo();
        if (scls == nullptr || (int)idx >= (int)scls->num) {
            return false;
        }
        const stage_scls_info_class* e = &scls->m_entries[idx];
        return strncmp(e->mStage, a, 8) == 0 && e->mRoom == b && e->mStart == c;
    }
    if (strcmp(kind, "dma") == 0 && sscanf(rec, "%*s %*s %*s %*d %*s %*u %x", &b) == 1) {
        stage_map_info_dummy_class* mi = dComIfGp_getStage().getMapInfoBase();
        return mi != nullptr && (int)idx < (int)mi->num && mi->m_entries[idx].mOceanXZ == b;
    }
    if (strcmp(kind, "actr") == 0 && sscanf(rec, "%*s %*s %*s %*d %*s %*u %15s %x", a, &b) == 2) {
        u32 params = 0;
        const int n = countActors(fpcNm_Tag_Volcano_e, &params);
        writef(STDERR_FILENO, "[cos] rando-smoke entr: VolTag actors %d, params %08x\n", n, (unsigned)params);
        return strcmp(a, "-") == 0 ? n == 0 : (n == 1 && params == b);
    }
    if (strcmp(kind, "plyr") == 0 && sscanf(rec, "%*s %*s %*s %*d %*s %*u %x", &b) == 1) {
        stage_actor_class* pl = rm < 0 ? dComIfGp_getStage().getPlayer()
                                       : dComIfGp_roomControl_getStatusRoomDt(room)->getPlayer();
        if (pl == nullptr || (int)idx >= (int)pl->num) {
            return false;
        }
        const uint8_t* e = reinterpret_cast<const uint8_t*>(&pl->m_entries[idx]);
        return pc_rando_dzx_be32(e + 8) == b;
    }
    (void)stage;
    return false;
}

void checkPlay() {
    const pcr::EntrStageRuntimeStats& rs = pcr::entrStageRuntimeStats();
    const char* stage = dComIfGp_getStartStageName();
    const int room = dComIfGp_roomControl_getStayNo();
    writef(STDERR_FILENO,
           "[cos] rando-smoke entr: in play at %s room %d, pc_rando() %d: patcher files %d, patched %d, written %d, "
           "refused %d, missing spawns %d, bad %d, dungeon-start mismatches %d, no map %d\n",
           stage, room, pc_rando(), rs.files, rs.patched, rs.written, rs.refused, rs.noSpawn, rs.badFiles,
           rs.startMismatches, rs.noMap);
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: the patcher saw no file", rs.files == 0 && rs.noMap == 0 && rs.written == 0);
        return;
    }
    const MapLine* m = activeMap();
    if (!randoSmokeCheck(kName, "the active seed is one of the fixture's maps", m != nullptr)) {
        return;
    }
    if (!m->valid) {
        randoSmokeCheck(kName, "entrance rando off: the patcher found no map and wrote nothing",
                        rs.noMap > 0 && rs.files == 0 && rs.written == 0);
        return;
    }
    randoSmokeCheck(kName, "the patcher refused nothing (D9) and every file parsed",
                    rs.files > 0 && rs.refused == 0 && rs.badFiles == 0 && rs.startMismatches == 0);
    randoSmokeCheck(kName, "every entrance spawn found (R3.16's spawns 27 included)", rs.noSpawn == 0);
    EvsVec<EvsString> recs;
    fixtureRecords(m->key, stage, -1, recs);
    fixtureRecords(m->key, stage, room, recs);
    int checked = 0, ok = 0;
    for (const EvsString& r : recs) {
        checked++;
        const bool good = checkLive(r.c_str(), stage, room);
        ok += good ? 1 : 0;
        writef(STDERR_FILENO, "[cos] rando-smoke entr: live %s: %s\n", r.c_str(), good ? "ok" : "FAIL");
    }
    randoSmokeCheck(kName, "the boot stage and room have records for this map", checked > 0);
    randoSmokeCheck(kName, "the live stage data hold every record", checked > 0 && ok == checked);
}

} // namespace

void randoSmokeEntr(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    if (!sStarted) {
        sStarted = true;
        const bool fixture = parseFixture();
        randoSmokeCheck(kName, "fixture R6_12_wwrando_entr.txt read", fixture);
        const bool maps = fixture && loadMaps();
        randoSmokeCheck(kName, "the fixture's maps load (A-D a map, V none)", maps);
        for (u32 size = 16u << 20; sHeap == nullptr && size >= (2u << 20); size >>= 1) {
            sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
        }
        if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr) || !maps) {
            sEndFrame = frames + 1;
            return;
        }
        writef(STDERR_FILENO, "[cos] rando-smoke entr: %zu files, %zu maps, %zu records\n", sFiles.size(),
               sMaps.size(), sRecords.size());
        return;
    }
    if (!sPlayChecked) {
        sPlayChecked = true;
        checkPlay();
    }
    if (!sSweepDone) {
        for (int i = 0; i < kFilesPerFrame && sNext < sFiles.size(); i++) {
            sweepOne(sFiles[sNext++]);
        }
        if (sNext < sFiles.size()) {
            return;
        }
        sSweepDone = true;
        const int want = (int)(sFiles.size() * sMaps.size());
        writef(STDERR_FILENO,
               "[cos] rando-smoke entr: %d file x map checks: %d = wwrando, %d with no other byte changed, %d refused; "
               "%zu of %zu records seen; %d mount failures; dungeon starts %d/%d\n",
               sChecked, sMatched, sClean, sRefused, sRecordsSeen, sRecords.size(), sNoMount, sStartOk, sStartSeen);
        randoSmokeCheck(kName, "every fixture file read from the disc", sNoMount == 0 && sChecked == want);
        randoSmokeCheck(kName, "every map's records on every file = wwrando's", sMatched == want);
        randoSmokeCheck(kName, "no byte outside the records changed", sClean == want);
        randoSmokeCheck(kName, "no edit refused (D9)", sRefused == 0);
        writef(STDERR_FILENO, "[cos] rando-smoke entr: %d missing spawn(s), %d outside the R3.16 boss door files\n",
               sNoSpawn, sNoSpawnElsewhere);
        randoSmokeCheck(kName, "a missing spawn only where R3.16 adds spawn 27", sNoSpawnElsewhere == 0);
        randoSmokeCheck(kName, "every fixture record belongs to a swept file", sRecordsSeen == sRecords.size());
        randoSmokeCheck(kName, "the 7 dungeon starts are their stage.dzs SCLS[0]", sStartSeen == 7 && sStartOk == 7);
        if (sHeap != nullptr) {
            sHeap->destroy();
            sHeap = nullptr;
        }
        sEndFrame = frames + 30; // the game runs on
    }
}

} // namespace pc
