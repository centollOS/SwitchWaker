// COS_SMOKE=rando COS_RANDO_SMOKE=enemies (docs/RANDOMIZER_PLAN.md step R7.4a): the enemy randomizer's
// actor rewrites (features/rando/pc_rando_enemies.h), run as
//   run.sh rando --stage <stage:room:point[:layer]> --env COS_RANDO_SMOKE=enemies
//                [--env COS_RANDO_PLACEMENT=<placement with "enemies">] [--env COS_ENEMY_DUMP=<file>]
//                [--env COS_RANDO_ENEMY_HOPS=<stage:room:point:layer>[,...]]
// (the lines are in native/check/regress_rando/R7.4a.txt; native/tools/rando/enemy_check.py runs it).
//
// 1. Static (always), once the play scene has run 90 frames: every room.dzr of the enemy location
//    table (213 room archives) is read from the disc into a private test heap (as
//    smokes/pc_rando_smoke_warppots.cpp does); under pc_rando() the patcher chain runs on a copy with
//    this step's patcher bypassed (the file wwrando's _save sees after its tweaks: R2.10 copies
//    kenroom's Layer 5 Darknuts, R3.15 clears kaze/Room3's Wizzrobe switch), then the real patcher
//    (pc_rando_enemies_patch) on that result, with the active seed. Checks: every file reads and
//    parses; with a seed holding "enemies", every placed location's record is written (no D9
//    mismatch, no refusal) and every other byte of the file is the chain's (same size); without one
//    (no placement, or an enemies-off placement), the patcher changes no file. Per enemy location
//        base <path> <the 0x20-byte ACTR record before this patcher, hex>
//        enemy <path> <the record after it, hex>          (zeros: no such entry)
//    go to COS_ENEMY_DUMP when set, for enemy_check.py (wwrando's own _save on the disc).
// 2. Live, in the boot room and in each COS_RANDO_ENEMY_HOPS stop (dComIfGp_setNextStage): every
//    table location of the stay room on the room's layer (default chunk or ACT<layer>): the stored
//    room.dzr record the game parsed (dComIfG_getStageRes) equals the static result, and, for a
//    written record whose spawn does not wait for a switch, a live actor with the record's procname
//    (dStage_searchName) stands at its home position.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_enemies.h"
#include "features/rando/pc_rando_enemy_data.h"
#include "features/rando/pc_rando_gen_enemies.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_seedfile.h"
#include "pc/pc_rando.h"

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

constexpr const char* kName = "enemies";
constexpr unsigned kSettle = 90;
constexpr unsigned kArriveFrames = 3000;

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

uint32_t fnv(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// --- static ---

struct Rec {
    bool have = false;
    uint8_t vanilla[0x20];
    uint8_t patched[0x20];
    int result = -1;
};
std::vector<Rec> sRecs; // per enemy location
JKRExpHeap* sHeap = nullptr;

bool readDisc(const char* stage, int room, std::vector<uint8_t>* out) {
    char path[128];
    snprintf(path, sizeof path, "/res/Stage/%s/Room%d.arc", stage, room);
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        logf("[cos] rando-smoke %s: %s: cannot mount a private copy\n", kName, path);
        sHeap->freeAll();
        return false;
    }
    for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst(); l != nullptr; l = l->getNext()) {
        if (l->getObject() == arc) {
            JKRFileLoader::getVolumeList().remove(l);
            break;
        }
    }
    void* res = arc->getResource(0, "room.dzr");
    bool ok = false;
    if (res != nullptr) {
        const uint32_t size = arc->getResSize(res);
        out->assign((const uint8_t*)res, (const uint8_t*)res + size);
        ok = true;
    }
    arc->unmountFixed();
    sHeap->freeAll();
    return ok;
}

void* mallocAlloc(size_t size, void*) {
    return calloc(1, size);
}

// A file as the archive hook hands it to the chain: names, a malloc allocator for grown copies.
struct ChainFile {
    PcRandoResFile file = {};
    PcRandoResNames names = {};
};

void describe(ChainFile* c, const char* stage, int room, std::vector<uint8_t>& bytes) {
    char dir[PCR_RES_DIR_MAX];
    snprintf(dir, sizeof dir, "/res/Stage/%s/", stage);
    char arcName[16];
    snprintf(arcName, sizeof arcName, "Room%d", room);
    pc_rando_res_describe(&c->file, &c->names, arcName, dir);
    c->file.name = "room.dzr";
    c->file.type = PCR_RES_DZR;
    c->file.data = bytes.data();
    c->file.size = bytes.size();
    c->file.alloc = mallocAlloc;
}

std::vector<uint8_t> take(ChainFile& c, std::vector<uint8_t>& copy) {
    std::vector<uint8_t> out(c.file.data, c.file.data + c.file.size);
    if (c.file.data != copy.data()) {
        free(c.file.data);
    }
    return out;
}

// What the earlier patchers of the chain make of the disc file (this step's patcher bypassed): the
// file wwrando's _save sees after the tweaks (e.g. R2.10 copies kenroom's ACT5 Darknuts to ACTR).
std::vector<uint8_t> runChainBefore(const char* stage, int room, const std::vector<uint8_t>& disc) {
    std::vector<uint8_t> copy(disc);
    ChainFile c;
    describe(&c, stage, room, copy);
    if (pc_rando()) { // the archive hook dispatches only under pc_rando()
        pc_rando_enemies_set_bypass(1);
        pc_rando_res_dispatch(&c.file);
        pc_rando_enemies_set_bypass(0);
    }
    return take(c, copy);
}

// This step's patcher on a copy.
std::vector<uint8_t> runPatcher(const char* stage, int room, const std::vector<uint8_t>& base) {
    std::vector<uint8_t> copy(base);
    ChainFile c;
    describe(&c, stage, room, copy);
    pc_rando_enemies_patch(&c.file);
    return take(c, copy);
}

bool entryOf(uint8_t* data, size_t size, const pcr::EnemyLocation& loc, uint8_t out[0x20]) {
    PcRandoDzx dzx;
    char tag[5];
    if (pc_rando_dzx_open(&dzx, data, size) != PCR_DZX_OK || !pc_rando_dzx_tag("ACTR", loc.path.layer, tag)) {
        return false;
    }
    const uint8_t* e = pc_rando_dzx_entry(&dzx, tag, loc.path.index);
    if (e == nullptr) {
        return false;
    }
    memcpy(out, e, 0x20);
    return true;
}

bool runStatic(bool active) {
    for (u32 size = 32u << 20; sHeap == nullptr && size >= (4u << 20); size >>= 1) {
        sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
    }
    if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr)) {
        return false;
    }
    const pcr::Span<pcr::EnemyLocation> locs = pcr::enemy_locations();
    sRecs.assign(locs.size, Rec());
    // The seed's placements, to know which locations are placed.
    std::vector<bool> placed(locs.size, false);
    pcr::EnemyRandoResult seed;
    if (active) {
        const PcRandoSeed* s = pc_rando_seed();
        const pcr::Json* j = s != nullptr ? s->extra.find("enemies") : nullptr;
        std::string err;
        if (j != nullptr && pcr::enemiesFromJson(*j, seed, &err)) {
            for (const pcr::EnemyPlacement& p : seed.placements) {
                placed[p.location] = true;
            }
        }
    }
    int files = 0, readOk = 0, kept = 0, written = 0, wantWritten = 0, sameSize = 0, othersKept = 0;
    std::vector<std::string> seen;
    for (size_t i = 0; i < locs.size; i++) {
        const pcr::EnemyActorPath& p = locs[i].path;
        if (p.room < 0) {
            continue;
        }
        bool dup = false;
        for (const std::string& f : seen) {
            dup = dup || f == p.file;
        }
        if (dup) {
            continue;
        }
        seen.push_back(p.file);
        files++;
        std::vector<uint8_t> raw;
        if (!readDisc(p.stage, p.room, &raw)) {
            continue;
        }
        std::vector<uint8_t> disc = runChainBefore(p.stage, p.room, raw);
        PcRandoDzx dzx;
        if (pc_rando_dzx_open(&dzx, disc.data(), disc.size()) != PCR_DZX_OK) {
            logf("[cos] rando-smoke %s: %s room.dzr does not parse\n", kName, p.file);
            continue;
        }
        readOk++;
        std::vector<uint8_t> out = runPatcher(p.stage, p.room, disc);
        if (out.size() == disc.size()) {
            sameSize++;
        }
        if (!active) {
            kept += out.size() == disc.size() && fnv(out.data(), out.size()) == fnv(disc.data(), disc.size()) ? 1 : 0;
        }
        // The file's records, and every other byte kept.
        std::vector<uint8_t> masked(out);
        for (size_t k = 0; k < locs.size; k++) {
            if (strcmp(locs[k].path.file, p.file) != 0) {
                continue;
            }
            Rec& r = sRecs[k];
            r.have = entryOf(disc.data(), disc.size(), locs[k], r.vanilla) &&
                     out.size() == disc.size() && entryOf(out.data(), out.size(), locs[k], r.patched);
            r.result = pc_rando_enemies_last_result((int)k);
            if (r.have && active && placed[k]) {
                wantWritten++;
                written += r.result == PCR_ENEMY_WRITTEN ? 1 : 0;
            }
            if (r.have) {
                // put the vanilla record back into the masked copy
                PcRandoDzx dm;
                char tag[5];
                if (pc_rando_dzx_open(&dm, masked.data(), masked.size()) == PCR_DZX_OK &&
                    pc_rando_dzx_tag("ACTR", locs[k].path.layer, tag)) {
                    uint8_t* e = pc_rando_dzx_entry(&dm, tag, locs[k].path.index);
                    if (e != nullptr) {
                        memcpy(e, r.vanilla, 0x20);
                    }
                }
            }
        }
        othersKept += masked.size() == disc.size() && memcmp(masked.data(), disc.data(), disc.size()) == 0 ? 1 : 0;
    }
    sHeap->destroy();
    sHeap = nullptr;
    int haveRecs = 0;
    for (const Rec& r : sRecs) {
        haveRecs += r.have ? 1 : 0;
    }
    check("%d room files of the table read from the disc and parse (%d)", readOk == files, files, readOk);
    if (pc_rando()) {
        check("every enemy location's entry exists (%d of %zu)", haveRecs == (int)locs.size, haveRecs, locs.size);
    } else {
        // without pc_rando() no patcher runs: kenroom's Actor009/00A (copied by R2.10) do not exist
        logf("[cos] rando-smoke %s: %d of %zu enemy location entries exist without the tweaks\n", kName, haveRecs,
             locs.size);
    }
    check("the patcher keeps every file's size (%d of %d)", sameSize == readOk, sameSize, readOk);
    check("outside the enemy records every byte is the earlier patchers' result (%d of %d files)", othersKept == readOk, othersKept,
          readOk);
    if (active) {
        int mism = 0, ref = 0, changed = 0;
        for (size_t k = 0; k < locs.size; k++) {
            mism += sRecs[k].result == PCR_ENEMY_MISMATCH ? 1 : 0;
            ref += sRecs[k].result == PCR_ENEMY_REFUSED ? 1 : 0;
            changed += sRecs[k].have && memcmp(sRecs[k].vanilla, sRecs[k].patched, 0x20) != 0 ? 1 : 0;
        }
        check("the seed places %zu enemies; %d of %d placed records written", written == wantWritten && wantWritten > 0,
              seed.placements.size(), written, wantWritten);
        check("no D9 mismatch (%d), no refusal (%d)", mism == 0 && ref == 0, mism, ref);
        logf("[cos] rando-smoke %s: %d records differ from the disc\n", kName, changed);
    } else {
        check("no enemies in the seed: the patcher changes no file (%d of %d)", kept == readOk, kept, readOk);
    }
    if (const char* dump = getenv("COS_ENEMY_DUMP")) {
        FILE* f = fopen(dump, "w");
        if (check("dump %s opened", f != nullptr, dump)) {
            for (int kind = 0; kind < 2; kind++) {
                for (size_t k = 0; k < locs.size; k++) {
                    fprintf(f, "%s %s ", kind == 0 ? "base" : "enemy", locs[k].path.raw);
                    const uint8_t* rec = kind == 0 ? sRecs[k].vanilla : sRecs[k].patched;
                    for (int b = 0; b < 0x20; b++) {
                        fprintf(f, "%02x", sRecs[k].have ? rec[b] : 0);
                    }
                    fputc('\n', f);
                }
            }
            fclose(f);
        }
    }
    return true;
}

// --- live ---

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
    int room;
    cXyz pos;
    uint32_t params;
    bool found;
};

void* findActor(void* proc, void* data) {
    ActorFind* f = static_cast<ActorFind*>(data);
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(proc);
    if (fpcM_GetName(proc) != f->proc) {
        return nullptr;
    }
    const cXyz& h = ac->home.pos;
    const float dx = h.x - f->pos.x, dy = h.y - f->pos.y, dz = h.z - f->pos.z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 1.0f || fopAcM_GetParam(ac) == f->params) {
        f->found = true;
        return proc;
    }
    return nullptr;
}

void checkStop(int stop) {
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    const int layer = dComIfG_play_c::getLayerNo(stay);
    char arc[16];
    snprintf(arc, sizeof arc, "Room%d", stay);
    void* file = dComIfG_getStageRes(arc, "room.dzr");
    logf("[cos] rando-smoke %s: stop %d: %s room %d layer %d, room.dzr %s\n", kName, stop, stage, stay, layer,
         file != nullptr ? "loaded" : "missing");
    const pcr::Span<pcr::EnemyLocation> locs = pcr::enemy_locations();
    int nRec = 0, recOk = 0, nAct = 0, actOk = 0;
    for (size_t k = 0; k < locs.size; k++) {
        const pcr::EnemyLocation& loc = locs[k];
        if (strcmp(loc.path.stage, stage) != 0 || loc.path.room != stay) {
            continue;
        }
        if (loc.path.layer >= 0 && loc.path.layer != layer) {
            continue;
        }
        nRec++;
        char tag[5];
        pc_rando_dzx_tag("ACTR", loc.path.layer, tag);
        int count = 0;
        uint8_t* first = file != nullptr ? storedChunk(file, tag, &count) : nullptr;
        const uint8_t* e = first != nullptr && (int)loc.path.index < count ? first + loc.path.index * 0x20 : nullptr;
        const Rec& r = sRecs[k];
        const bool ok = e != nullptr && r.have && memcmp(e, r.patched, 0x20) == 0;
        recOk += ok ? 1 : 0;
        if (!ok) {
            check("%s: the live record equals the patched disc record", false, loc.path.raw);
            continue;
        }
        if (r.result != PCR_ENEMY_WRITTEN) {
            continue;
        }
        // live actor (not for spawns that wait for a switch)
        const uint32_t params = pc_rando_dzx_be32(e + 8);
        char name[9] = {};
        memcpy(name, e, 8);
        // the location's own condition: ChecksConditionSwitch (the spawn waits for a switch)
        if ((loc.conds & pcr::ECOND_CHECKS_CONDITION_SWITCH) != 0) {
            continue;
        }
        dStage_objectNameInf* inf = dStage_searchName(name);
        if (inf == nullptr) {
            check("%s: %s is a known actor name", false, loc.path.raw, name);
            continue;
        }
        nAct++;
        ActorFind f = {inf->procname, stay,
                       cXyz(pc_rando_dzx_bef32(e + 0x0C), pc_rando_dzx_bef32(e + 0x10), pc_rando_dzx_bef32(e + 0x14)),
                       params, false};
        fopAcM_Search(findActor, &f);
        actOk += f.found ? 1 : 0;
        if (!f.found) {
            logf("[cos] rando-smoke %s:   %s: no live %s (proc %d) at (%.0f, %.0f, %.0f) params %08X\n", kName,
                 loc.path.raw, name, (int)inf->procname, f.pos.x, f.pos.y, f.pos.z, (unsigned)params);
        }
    }
    check("stop %d (%s room %d): %d of %d live records equal the patched ones", recOk == nRec, stop, stage, stay, recOk,
          nRec);
    check("stop %d (%s room %d): %d of %d placed enemies live at their home", actOk == nAct, stop, stage, stay, actOk,
          nAct);
}

struct Hop {
    char stage[9];
    int room, point, layer;
};
std::vector<Hop> sHops;
size_t sHop = 0;

void parseHops() {
    const char* s = getenv("COS_RANDO_ENEMY_HOPS");
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
        std::string item = all.substr(pos, end - pos);
        Hop h = {};
        h.layer = -1;
        char st[16] = {};
        if (sscanf(item.c_str(), "%8[^:]:%d:%d:%d", st, &h.room, &h.point, &h.layer) >= 3) {
            memcpy(h.stage, st, 8);
            sHops.push_back(h);
        } else {
            logf("[cos] rando-smoke %s: bad hop \"%s\" (stage:room:point[:layer])\n", kName, item.c_str());
        }
        pos = end + 1;
    }
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(isPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

enum State { kBoot, kArrive, kDone };
State sState = kBoot;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
unsigned sSince = 0, sPresent = 0;

void nextHop() {
    if (sHop >= sHops.size()) {
        sState = kDone;
        randoSmokeEnd(kName);
    }
    const Hop& h = sHops[sHop];
    logf("[cos] rando-smoke %s: stage change to %s room %d point %d layer %d\n", kName, h.stage, h.room, h.point,
         h.layer);
    sOldScene = playSceneId();
    sSince = 0;
    sPresent = 0;
    sState = kArrive;
    dComIfGp_setNextStage(h.stage, (s16)h.point, (s8)h.room, (s8)h.layer);
}

} // namespace

void randoSmokeEnemies(unsigned int frames) {
    (void)frames;
    if (sState == kDone) {
        return;
    }
    if (sState == kBoot) {
        if (!randoSmokeInPlay(kSettle)) {
            return;
        }
        const bool active = pc_rando_enemies_active() != 0;
        logf("[cos] rando-smoke %s: pc_rando %d, seed enemies %d\n", kName, pc_rando(), active ? 1 : 0);
        PcRandoEnemyStats st;
        pc_rando_enemies_stats(&st);
        logf("[cos] rando-smoke %s: game loads so far: %u files, %u written, %u mismatched, %u refused, %u skipped\n",
             kName, st.files, st.written, st.mismatched, st.refused, st.skipped);
        if (!runStatic(active)) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        checkStop(0);
        parseHops();
        nextHop();
        return;
    }
    sSince++;
    const fpc_ProcID scene = playSceneId();
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr || dComIfGp_isEnableNextStage()) {
        sPresent = 0;
        if (sSince >= kArriveFrames) {
            check("hop %zu: %s room %d reached", false, sHop, sHops[sHop].stage, sHops[sHop].room);
            sState = kDone;
            randoSmokeEnd(kName);
        }
        return;
    }
    if (++sPresent < kSettle) {
        return;
    }
    check("hop %zu: arrived in %s room %d (is %s room %d)",
          strcmp(dComIfGp_getStartStageName(), sHops[sHop].stage) == 0 &&
              dComIfGp_roomControl_getStayNo() == sHops[sHop].room,
          sHop, sHops[sHop].stage, sHops[sHop].room, dComIfGp_getStartStageName(), dComIfGp_roomControl_getStayNo());
    checkStop((int)sHop + 1);
    sHop++;
    nextHop();
}

} // namespace pc
