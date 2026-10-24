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
// R7.4b (features/rando/pc_rando_enemy_actors.h) extends both parts: the static part also reads
// TF_01's stage.dzs and the stone heads' kaze rooms, runs R7.4b's patcher after R7.4a's (both bypassed
// for the "base" file), allows the 3 files that get new actors to grow (every other entry kept, chunk by
// chunk), checks every edit was made and that no table path names a removed entry, and dumps
//        actor <label> <the edited record after both patchers, hex>     (0x20, or 0x24 for SCOB/TGDR)
//        added <file> <tag> <index> <the appended record, hex>
// The live part compares those records with the game's stored file and checks the appended actors
// live. COS_RANDO_ENEMY_ALLDIE=1: in the boot room, its R7.4b ALLdie sets its switch once the smoke
// has deleted every enemy of the room.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_enemies.h"
#include "features/rando/pc_rando_enemy_actors.h"
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

// R7.4b: per pc_rando_enemy_actors_targets() entry (adds: unused, see sAdded).
struct TargetRec {
    bool have = false;
    uint32_t size = 0;
    uint8_t rec[0x24];
    int result = -1;
};
std::vector<TargetRec> sTargetRecs;
// The appended entries of the files that grew: "<file> <tag> <index>" and the record.
struct Added {
    std::string file;
    char tag[5];
    uint32_t index;
    uint8_t rec[0x20];
};
std::vector<Added> sAdded;

// A stage file the static part reads: RoomN.arc's room.dzr, or Stage.arc's stage.dzs (room -1).
struct StaticFile {
    const char* stage;
    int room;
    std::string key; // "M_NewD2/Room12.arc", "TF_01/Stage.arc"
};

bool readDisc(const char* stage, int room, std::vector<uint8_t>* out) {
    char path[128];
    if (room >= 0) {
        snprintf(path, sizeof path, "/res/Stage/%s/Room%d.arc", stage, room);
    } else {
        snprintf(path, sizeof path, "/res/Stage/%s/Stage.arc", stage);
    }
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
    void* res = arc->getResource(0, room >= 0 ? "room.dzr" : "stage.dzs");
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
    if (room >= 0) {
        snprintf(arcName, sizeof arcName, "Room%d", room);
    } else {
        snprintf(arcName, sizeof arcName, "Stage");
    }
    pc_rando_res_describe(&c->file, &c->names, arcName, dir);
    c->file.name = room >= 0 ? "room.dzr" : "stage.dzs";
    c->file.type = room >= 0 ? PCR_RES_DZR : PCR_RES_DZS;
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

// What the earlier patchers of the chain make of the disc file (R7.4a's and R7.4b's patchers
// bypassed): the file wwrando's _save sees after the tweaks (e.g. R2.10 copies kenroom's ACT5 Darknuts
// to ACTR).
std::vector<uint8_t> runChainBefore(const char* stage, int room, const std::vector<uint8_t>& disc) {
    std::vector<uint8_t> copy(disc);
    ChainFile c;
    describe(&c, stage, room, copy);
    if (pc_rando()) { // the archive hook dispatches only under pc_rando()
        pc_rando_enemies_set_bypass(1);
        pc_rando_enemy_actors_set_bypass(1);
        pc_rando_res_dispatch(&c.file);
        pc_rando_enemy_actors_set_bypass(0);
        pc_rando_enemies_set_bypass(0);
    }
    return take(c, copy);
}

// This step's patchers on a copy, in their registration order (R7.4a, then R7.4b).
std::vector<uint8_t> runPatcher(const char* stage, int room, const std::vector<uint8_t>& base) {
    std::vector<uint8_t> copy(base);
    ChainFile c;
    describe(&c, stage, room, copy);
    pc_rando_enemies_patch(&c.file);
    pc_rando_enemy_actors_patch(&c.file);
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

// entries_by_type(ACTR)[index] (every ACTR-type chunk in chunk order), as R7.4b's stone heads.
uint8_t* actrByType(const PcRandoDzx* dzx, int index) {
    int seen = 0;
    for (uint32_t c = 0; c < dzx->chunkCount; c++) {
        const uint8_t* row = dzx->data + 4 + c * 12;
        if (memcmp(row, "ACT", 3) != 0 || !(row[3] == 'R' || (row[3] >= '0' && row[3] <= '9') || row[3] == 'a' ||
                                            row[3] == 'b')) {
            continue;
        }
        char tag[5];
        memcpy(tag, row, 4);
        tag[4] = '\0';
        const int n = (int)pc_rando_dzx_be32(row + 4);
        if (index < seen + n) {
            return pc_rando_dzx_entry(dzx, tag, (uint32_t)(index - seen));
        }
        seen += n;
    }
    return nullptr;
}

// The record of a (non-add) R7.4b target in a file, or nullptr.
const uint8_t* targetEntry(const PcRandoDzx* dzx, const PcRandoEnemyActorTarget& t) {
    return t.kind == PCR_EACTOR_HOMEN ? actrByType(dzx, t.index) : pc_rando_dzx_entry(dzx, t.tag, (uint32_t)t.index);
}

bool targetInFile(const PcRandoEnemyActorTarget& t, const StaticFile& f) {
    return t.room == f.room && strcmp(t.stage, f.stage) == 0;
}

// Chunk-wise comparison of base and out: every chunk of base is in out with every entry equal, except
// the entries `masked` names (tag, index); a chunk may only grow by `grow` entries for `growTag`.
bool chunksKept(const std::vector<uint8_t>& base, const std::vector<uint8_t>& out,
                const std::vector<std::pair<std::string, uint32_t>>& masked, const char* growTag, uint32_t grow,
                std::string* why) {
    PcRandoDzx db, dout;
    std::vector<uint8_t> b(base), o(out);
    if (pc_rando_dzx_open(&db, b.data(), b.size()) != PCR_DZX_OK ||
        pc_rando_dzx_open(&dout, o.data(), o.size()) != PCR_DZX_OK) {
        *why = "a file does not parse";
        return false;
    }
    if (dout.chunkCount < db.chunkCount) {
        *why = "chunks lost";
        return false;
    }
    for (uint32_t c = 0; c < db.chunkCount; c++) {
        char tag[5];
        memcpy(tag, b.data() + 4 + c * 12, 4);
        tag[4] = '\0';
        const uint32_t nb = pc_rando_dzx_count(&db, tag), no = pc_rando_dzx_count(&dout, tag);
        const uint32_t want = growTag != nullptr && strcmp(tag, growTag) == 0 ? nb + grow : nb;
        if (no != want) {
            *why = std::string(tag) + ": entry count changed";
            return false;
        }
        const uint32_t sz = pc_rando_dzx_entry_size(tag);
        if (sz == 0) {
            continue; // unknown entry size: not compared
        }
        for (uint32_t i = 0; i < nb; i++) {
            bool skip = false;
            for (const auto& m : masked) {
                skip = skip || (m.first == tag && m.second == i);
            }
            if (!skip && memcmp(pc_rando_dzx_entry(&db, tag, i), pc_rando_dzx_entry(&dout, tag, i), sz) != 0) {
                char buf[64];
                snprintf(buf, sizeof buf, "%s[%u] changed", tag, (unsigned)i);
                *why = buf;
                return false;
            }
        }
    }
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
    int nTargets = 0;
    const PcRandoEnemyActorTarget* targets = pc_rando_enemy_actors_targets(&nTargets);
    sRecs.assign(locs.size, Rec());
    sTargetRecs.assign((size_t)nTargets, TargetRec());
    sAdded.clear();
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
    // The files: every enemy location's room, then R7.4b's other files (TF_01's stage.dzs, kaze).
    std::vector<StaticFile> list;
    auto addFile = [&](const char* stage, int room) {
        for (const StaticFile& f : list) {
            if (f.room == room && strcmp(f.stage, stage) == 0) {
                return;
            }
        }
        char key[64];
        if (room >= 0) {
            snprintf(key, sizeof key, "%s/Room%d.arc", stage, room);
        } else {
            snprintf(key, sizeof key, "%s/Stage.arc", stage);
        }
        list.push_back(StaticFile{stage, room, key});
    };
    for (size_t i = 0; i < locs.size; i++) {
        if (locs[i].path.room >= 0) {
            addFile(locs[i].path.stage, locs[i].path.room);
        }
    }
    for (int t = 0; t < nTargets; t++) {
        addFile(targets[t].stage, targets[t].room);
    }
    for (int t = 0; t < nTargets; t++) {
        if (targets[t].kind == PCR_EACTOR_ADD) {
            addFile(targets[t].stage, -1); // the memory block slack (M_NewD2's MEMA)
        }
    }
    int files = 0, readOk = 0, kept = 0, written = 0, wantWritten = 0, sizeOk = 0, othersKept = 0, blockSlack = 0;
    for (const StaticFile& f : list) {
        files++;
        std::vector<uint8_t> raw;
        if (!readDisc(f.stage, f.room, &raw)) {
            continue;
        }
        std::vector<uint8_t> disc = runChainBefore(f.stage, f.room, raw);
        PcRandoDzx dzx;
        if (pc_rando_dzx_open(&dzx, disc.data(), disc.size()) != PCR_DZX_OK) {
            logf("[cos] rando-smoke %s: %s does not parse\n", kName, f.key.c_str());
            continue;
        }
        readOk++;
        std::vector<uint8_t> out = runPatcher(f.stage, f.room, disc);
        // the adds this file gets (one group per file: one tag)
        uint32_t nAdds = 0;
        char addTag[5] = {};
        for (int t = 0; t < nTargets; t++) {
            if (targets[t].kind == PCR_EACTOR_ADD && targetInFile(targets[t], f)) {
                nAdds++;
                memcpy(addTag, targets[t].tag, 5);
            }
        }
        const uint32_t grow = active ? nAdds : 0;
        if (grow == 0 ? out.size() == disc.size() : out.size() > disc.size()) {
            sizeOk++;
        } else {
            logf("[cos] rando-smoke %s: %s: size %zu -> %zu (%u new actors)\n", kName, f.key.c_str(), disc.size(),
                 out.size(), (unsigned)grow);
        }
        if (!active) {
            kept += out.size() == disc.size() && fnv(out.data(), out.size()) == fnv(disc.data(), disc.size()) ? 1 : 0;
        }
        PcRandoDzx dout;
        const bool outOk = pc_rando_dzx_open(&dout, out.data(), out.size()) == PCR_DZX_OK;
        // The enemy records; the entries every edit may change.
        std::vector<std::pair<std::string, uint32_t>> masked;
        for (size_t k = 0; k < locs.size; k++) {
            if (f.room < 0 || strcmp(locs[k].path.file, f.key.c_str()) != 0) {
                continue;
            }
            Rec& r = sRecs[k];
            r.have = entryOf(disc.data(), disc.size(), locs[k], r.vanilla) && outOk &&
                     entryOf(out.data(), out.size(), locs[k], r.patched);
            r.result = pc_rando_enemies_last_result((int)k);
            if (r.have && active && placed[k]) {
                wantWritten++;
                written += r.result == PCR_ENEMY_WRITTEN ? 1 : 0;
            }
            char tag[5];
            if (pc_rando_dzx_tag("ACTR", locs[k].path.layer, tag)) {
                masked.emplace_back(tag, locs[k].path.index);
            }
        }
        for (int t = 0; t < nTargets; t++) {
            const PcRandoEnemyActorTarget& tg = targets[t];
            if (tg.kind == PCR_EACTOR_ADD || !targetInFile(tg, f)) {
                continue;
            }
            TargetRec& r = sTargetRecs[t];
            r.result = pc_rando_enemy_actors_last_result(t);
            const uint8_t* e = outOk ? targetEntry(&dout, tg) : nullptr;
            r.size = pc_rando_dzx_entry_size(tg.tag);
            r.have = e != nullptr && r.size <= sizeof r.rec;
            if (r.have) {
                memcpy(r.rec, e, r.size);
            }
            // the entry it edits (stone heads: find the chunk and index of the by-type entry)
            if (tg.kind == PCR_EACTOR_HOMEN) {
                const uint8_t* be = actrByType(&dzx, tg.index);
                for (uint32_t c = 0; be != nullptr && c < dzx.chunkCount; c++) {
                    char tag[5];
                    memcpy(tag, dzx.data + 4 + c * 12, 4);
                    tag[4] = '\0';
                    const uint32_t n = pc_rando_dzx_count(&dzx, tag), sz = pc_rando_dzx_entry_size(tag);
                    const uint8_t* first = sz != 0 && n != 0 ? pc_rando_dzx_entry(&dzx, tag, 0) : nullptr;
                    if (first != nullptr && be >= first && be < first + n * sz) {
                        masked.emplace_back(tag, (uint32_t)((be - first) / sz));
                    }
                }
            } else {
                masked.emplace_back(tg.tag, (uint32_t)tg.index);
            }
        }
        // R7.4b's memory block slack (stage.dzs MEMA of a stage whose rooms get new actors)
        if (f.room < 0 && outOk) {
            const uint32_t nb = pc_rando_dzx_count(&dzx, "MEMA");
            for (uint32_t i = 0; i < nb && nb == pc_rando_dzx_count(&dout, "MEMA"); i++) {
                const uint32_t a = pc_rando_dzx_be32(pc_rando_dzx_entry(&dzx, "MEMA", i));
                const uint32_t b = pc_rando_dzx_be32(pc_rando_dzx_entry(&dout, "MEMA", i));
                if (a != b) {
                    masked.emplace_back("MEMA", i);
                    blockSlack += b == a + 0x1000 ? 1 : 100;
                    logf("[cos] rando-smoke %s: %s MEMA[%u] 0x%X -> 0x%X\n", kName, f.key.c_str(), (unsigned)i,
                         (unsigned)a, (unsigned)b);
                }
            }
        }
        // The appended entries.
        if (outOk && nAdds != 0) {
            const uint32_t nb = pc_rando_dzx_count(&dzx, addTag), no = pc_rando_dzx_count(&dout, addTag);
            for (uint32_t i = nb; i < no; i++) {
                Added a;
                a.file = f.key;
                memcpy(a.tag, addTag, 5);
                a.index = i;
                memcpy(a.rec, pc_rando_dzx_entry(&dout, addTag, i), 0x20);
                sAdded.push_back(a);
            }
        }
        std::string why;
        if (chunksKept(disc, out, masked, nAdds != 0 ? addTag : nullptr, grow, &why)) {
            othersKept++;
        } else {
            logf("[cos] rando-smoke %s: %s: %s\n", kName, f.key.c_str(), why.c_str());
        }
    }
    sHeap->destroy();
    sHeap = nullptr;
    int haveRecs = 0;
    for (const Rec& r : sRecs) {
        haveRecs += r.have ? 1 : 0;
    }
    check("%d stage files of the table read from the disc and parse (%d)", readOk == files, files, readOk);
    if (pc_rando()) {
        check("every enemy location's entry exists (%d of %zu)", haveRecs == (int)locs.size, haveRecs, locs.size);
    } else {
        // without pc_rando() no patcher runs: kenroom's Actor009/00A (copied by R2.10) do not exist
        logf("[cos] rando-smoke %s: %d of %zu enemy location entries exist without the tweaks\n", kName, haveRecs,
             locs.size);
    }
    check("the patchers keep every file's size, or grow the %s files with new actors (%d of %d)", sizeOk == readOk,
          active ? "3" : "0", sizeOk, readOk);
    check("outside the edited records every entry is the earlier patchers' result (%d of %d files)",
          othersKept == readOk, othersKept, readOk);
    char why[200] = {};
    const int conflicts = pc_rando_enemy_actors_selfcheck(why, sizeof why);
    check("no table path names a removed entry (%d%s%s)", conflicts == 0, conflicts, conflicts ? ": " : "", why);
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
        int done = 0, adds = 0;
        for (int t = 0; t < nTargets; t++) {
            const int r = pc_rando_enemy_actors_last_result(t);
            done += r == 1 ? 1 : 0;
            adds += targets[t].kind == PCR_EACTOR_ADD ? 1 : 0;
            if (r != 1) {
                logf("[cos] rando-smoke %s:   %s: result %d\n", kName, targets[t].label, r);
            }
        }
        check("R7.4b: %d of %d actor edits made (ALLdies, modified, added, removed, stone heads)", done == nTargets,
              done, nTargets);
        check("R7.4b: %zu of %d new actors appended", sAdded.size() == (size_t)adds, sAdded.size(), adds);
        // M_NewD2 Room1 is the only room with new actors in a stage with memory blocks
        check("R7.4b: the memory block of 1 room grown by 0x1000 for its new actors (%d)", blockSlack == 1, blockSlack);
    } else {
        check("no enemies in the seed: the patchers change no file (%d of %d)", kept == readOk, kept, readOk);
        check("no enemies in the seed: no new actors (%zu), no memory block grown (%d)", sAdded.empty() && blockSlack == 0,
              sAdded.size(), blockSlack);
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
            // R7.4b: the edited records (zeros: no such entry), then the appended ones
            for (int t = 0; t < nTargets; t++) {
                if (targets[t].kind == PCR_EACTOR_ADD) {
                    continue;
                }
                const TargetRec& r = sTargetRecs[t];
                const uint32_t sz = pc_rando_dzx_entry_size(targets[t].tag);
                fprintf(f, "actor %s ", targets[t].label);
                for (uint32_t b = 0; b < sz; b++) {
                    fprintf(f, "%02x", r.have ? r.rec[b] : 0);
                }
                fputc('\n', f);
            }
            for (const Added& a : sAdded) {
                fprintf(f, "added %s %s %u ", a.file.c_str(), a.tag, (unsigned)a.index);
                for (int b = 0; b < 0x20; b++) {
                    fprintf(f, "%02x", a.rec[b]);
                }
                fputc('\n', f);
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

// The live file's entries_by_type(ACTR)[index] (stone heads).
uint8_t* storedActrByType(void* file, int index) {
    dStage_fileHeader* hdr = (dStage_fileHeader*)file;
    const int n = hdr->m_chunkCount;
    int seen = 0;
    for (int i = 0; i < n && n <= 256; i++) {
        uint32_t t = hdr->m_nodes[i].m_tag;
        char tag[5];
        memcpy(tag, &t, 4);
        tag[4] = '\0';
        if (memcmp(tag, "ACT", 3) != 0 || !(tag[3] == 'R' || (tag[3] >= '0' && tag[3] <= '9') || tag[3] == 'a' ||
                                            tag[3] == 'b')) {
            continue;
        }
        int count = 0;
        uint8_t* first = storedChunk(file, tag, &count);
        if (index < seen + count) {
            return first != nullptr ? first + (index - seen) * 0x20 : nullptr;
        }
        seen += count;
    }
    return nullptr;
}

// R7.4b in the stay room: the edited and appended records the game parsed equal the static result,
// and every appended actor is live.
void checkStopActors(int stop, const char* stage, int stay, void* file) {
    char key[64];
    snprintf(key, sizeof key, "%s/Room%d.arc", stage, stay);
    int nTargets = 0;
    const PcRandoEnemyActorTarget* targets = pc_rando_enemy_actors_targets(&nTargets);
    int n = 0, ok = 0;
    for (int t = 0; t < nTargets && (size_t)t < sTargetRecs.size(); t++) {
        const PcRandoEnemyActorTarget& tg = targets[t];
        if (tg.kind == PCR_EACTOR_ADD || tg.room != stay || strcmp(tg.stage, stage) != 0) {
            continue;
        }
        n++;
        int count = 0;
        uint8_t* e = nullptr;
        if (file != nullptr && tg.kind == PCR_EACTOR_HOMEN) {
            e = storedActrByType(file, tg.index);
        } else if (file != nullptr) {
            uint8_t* first = storedChunk(file, tg.tag, &count);
            const uint32_t sz = pc_rando_dzx_entry_size(tg.tag);
            e = first != nullptr && tg.index < count ? first + tg.index * sz : nullptr;
        }
        const TargetRec& r = sTargetRecs[t];
        const bool same = e != nullptr && r.have && memcmp(e, r.rec, r.size) == 0;
        ok += same ? 1 : 0;
        if (!same) {
            logf("[cos] rando-smoke %s:   %s: the live record differs from the static one\n", kName, tg.label);
        }
    }
    int nAdd = 0, addOk = 0, live = 0;
    for (const Added& a : sAdded) {
        if (a.file != key) {
            continue;
        }
        nAdd++;
        int count = 0;
        uint8_t* first = file != nullptr ? storedChunk(file, a.tag, &count) : nullptr;
        const uint8_t* e = first != nullptr && (int)a.index < count ? first + a.index * 0x20 : nullptr;
        addOk += e != nullptr && memcmp(e, a.rec, 0x20) == 0 ? 1 : 0;
        char name[9] = {};
        memcpy(name, a.rec, 8);
        dStage_objectNameInf* inf = dStage_searchName(name);
        if (inf != nullptr) {
            ActorFind f = {inf->procname, stay,
                           cXyz(pc_rando_dzx_bef32(a.rec + 0x0C), pc_rando_dzx_bef32(a.rec + 0x10),
                                pc_rando_dzx_bef32(a.rec + 0x14)),
                           pc_rando_dzx_be32(a.rec + 8), false};
            fopAcM_Search(findActor, &f);
            live += f.found ? 1 : 0;
            if (!f.found) {
                logf("[cos] rando-smoke %s:   %s %s[%u]: no live %s\n", kName, key, a.tag, (unsigned)a.index, name);
            }
        }
    }
    if (n != 0) {
        check("stop %d (%s room %d): %d of %d edited R7.4b records live as patched", ok == n, stop, stage, stay, ok, n);
    }
    if (nAdd != 0) {
        check("stop %d (%s room %d): %d of %d appended records live, %d of them as actors", addOk == nAdd && live == nAdd,
              stop, stage, stay, addOk, nAdd, live);
    }
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
    checkStopActors(stop, stage, stay, file);
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

enum State { kBoot, kKill, kArrive, kDone };
State sState = kBoot;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
unsigned sSince = 0, sPresent = 0;

// R7.4b live check (COS_RANDO_ENEMY_ALLDIE=1): in the boot room, the ALLdie the patcher made (a
// converted switch actor or an appended one) sets its switch once every enemy of the room is gone. The
// smoke deletes the room's enemies (group fopAc_ENEMY_e, as fopAcM_myRoomSearchEnemy judges) each
// frame and waits for daAlldie_c's 65-frame timer.
constexpr unsigned kKillFrames = 1200;
int sKillSwitch = -1, sKillRoom = -1;
unsigned sKillFrames = 0, sKilled = 0;
std::vector<fpc_ProcID> sDeleted;

void* deleteEnemy(void* proc, void* data) {
    const int room = *static_cast<int*>(data);
    fopAc_ac_c* ac = static_cast<fopAc_ac_c*>(proc);
    if (!fopAcM_IsActor(proc) || ac->group != fopAc_ENEMY_e || fopAcM_GetRoomNo(ac) != room) {
        return nullptr;
    }
    const fpc_ProcID id = fopAcM_GetID(ac);
    for (fpc_ProcID d : sDeleted) {
        if (d == id) {
            return nullptr;
        }
    }
    sDeleted.push_back(id);
    sKilled++;
    fopAcM_delete(ac);
    return nullptr;
}

// The switch of the boot room's R7.4b ALLdie, or -1.
int bootAlldieSwitch(int* room) {
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    char key[64];
    snprintf(key, sizeof key, "%s/Room%d.arc", stage, stay);
    int nTargets = 0;
    const PcRandoEnemyActorTarget* targets = pc_rando_enemy_actors_targets(&nTargets);
    *room = stay;
    for (int t = 0; t < nTargets && (size_t)t < sTargetRecs.size(); t++) {
        if (targets[t].kind == PCR_EACTOR_ALLDIE && targets[t].room == stay && strcmp(targets[t].stage, stage) == 0 &&
            sTargetRecs[t].have && memcmp(sTargetRecs[t].rec, "ALLdie\0", 7) == 0) {
            return (int)((pc_rando_dzx_be32(sTargetRecs[t].rec + 8) >> 8) & 0xFF);
        }
    }
    for (const Added& a : sAdded) {
        if (a.file == key && memcmp(a.rec, "ALLdie\0", 7) == 0) {
            return (int)((pc_rando_dzx_be32(a.rec + 8) >> 8) & 0xFF);
        }
    }
    return -1;
}

void nextHop();

void killStep() {
    sKillFrames++;
    fopAcM_Search(deleteEnemy, &sKillRoom);
    if (dComIfGs_isSwitch(sKillSwitch, sKillRoom)) {
        check("ALLdie: switch %02X of room %d set after the room's %u enemies were removed (frame %u)", sKilled > 0,
              (unsigned)sKillSwitch, sKillRoom, sKilled, sKillFrames);
        parseHops();
        nextHop();
        return;
    }
    if (sKillFrames >= kKillFrames) {
        check("ALLdie: switch %02X of room %d set after the room's %u enemies were removed", false,
              (unsigned)sKillSwitch, sKillRoom, sKilled);
        sState = kDone;
        randoSmokeEnd(kName);
    }
}

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
        if (getenv("COS_RANDO_ENEMY_ALLDIE") != nullptr) {
            sKillSwitch = bootAlldieSwitch(&sKillRoom);
            if (check("ALLdie: the boot room has an R7.4b ALLdie (switch %d)", sKillSwitch >= 0, sKillSwitch) &&
                check("ALLdie: its switch is off before (room %d)", !dComIfGs_isSwitch(sKillSwitch, sKillRoom),
                      sKillRoom)) {
                sState = kKill;
                return;
            }
        }
        parseHops();
        nextHop();
        return;
    }
    if (sState == kKill) {
        killStep();
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
