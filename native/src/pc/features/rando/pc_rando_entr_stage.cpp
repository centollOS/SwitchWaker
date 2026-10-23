// pc_rando_entr_stage.cpp - the entrance randomizer's stage data at run time (docs/RANDOMIZER_PLAN.md
// step R6.12; pc_rando_entr_stage.h). wwrando: randomizers/entrances.py update_entrance_to_lead_to_exit,
// wwlib/dzx.py (SCLS, PLYR, 2DMA, STAG, ACTR layouts), data/actor_parameters.txt (PLYR spawn_type
// params 0x0000F000, spawn_id z_rot 0x00FF; d_a_tag_volcano type params 0x000000C0).
// Pure: no game headers (cos_rando_core).
#include "pc_rando_entr_stage.h"

#include "pc/pc_rando.h"
#include "pc_rando_data.h"
#include "pc_rando_dzx.h"
#include "pc_rando_options.h"
#include "pc_rando_seedfile.h"
#include "pc_rando_trap.h" // optionsFromJson

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace pcr {

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

// SCLS[0] of the stage.dzs of each stage that holds a MINIBOSS_ENTRANCES / BOSS_ENTRANCES entry
// (GZLE01; checked against the file when it loads, and by the smoke against wwrando's result).
const EntrSclsDest kDungeonStarts[] = {
    {"kindan", 0, 1}, {"Siren", 0, 0}, {"M_Dai", 255, 0}, {"kaze", 255, 15},
    {"Hyroom", 0, 10}, {"M_NewD2", 0, 0}, {"sea", 1, 100},
};

struct StageRoom {
    const char* stage;
    int room;
};
// update_entrance_to_lead_to_exit's extra Savage Labyrinth rooms.
constexpr StageRoom kSavageRooms[] = {{"Cave10", 0}, {"Cave10", 20}, {"Cave11", 0}};

bool isMinibossOrBoss(EntranceGroup g) {
    return g == EntranceGroup::Miniboss || g == EntranceGroup::Boss;
}

// The 8-byte zero-padded stage name of a SCLS entry equals s.
bool sclsDestIs(const uint8_t* e, const char* s) {
    char name[9];
    memcpy(name, e, 8);
    name[8] = '\0';
    return strcmp(name, s) == 0;
}

bool sclsEquals(const uint8_t* e, const char* stage, uint8_t room, uint8_t spawn) {
    return sclsDestIs(e, stage) && e[8] == spawn && e[9] == room;
}

} // namespace

bool entrSclsEquals(const uint8_t* e, const char* stage, uint8_t room, uint8_t spawn) {
    return sclsEquals(e, stage, room, spawn);
}

namespace {

void sclsWrite(uint8_t* e, const char* stage, uint8_t room, uint8_t spawn) {
    // wwlib fs.write_str(..., 8): the name, NUL-padded to 8 bytes (at most 7 characters).
    memset(e, 0, 8);
    memcpy(e, stage, strnlen(stage, 7));
    e[8] = spawn;
    e[9] = room;
}

struct FileCtx {
    const char* stage;
    int room;
    PcRandoDzx dzx;
    int stagePlyr;
    EntrStagePatchStats* st;
};

bool isFile(const FileCtx& f, const char* stage, int room) {
    return f.room == room && strcmp(f.stage, stage) == 0;
}

void refuse(FileCtx& f, const char* what) {
    f.st->refused++;
    logf("[cos] rando: entrances %s/%s%d: %s; left vanilla\n", f.stage, f.room < 0 ? "Stage" : "Room",
         f.room < 0 ? 0 : f.room, what);
}

// Writes SCLS[index] -> (stage, room, spawn). vanillaStage: the stage the vanilla record leads to
// (D9), nullptr for no check.
void setScls(FileCtx& f, uint32_t index, const char* vanillaStage, const char* stage, uint8_t room, uint8_t spawn) {
    uint8_t* e = pc_rando_dzx_entry(&f.dzx, "SCLS", index);
    if (e == nullptr) {
        char b[64];
        snprintf(b, sizeof b, "no SCLS %u", (unsigned)index);
        refuse(f, b);
        return;
    }
    if (vanillaStage != nullptr && !sclsDestIs(e, vanillaStage) && !sclsEquals(e, stage, room, spawn)) {
        char b[96];
        snprintf(b, sizeof b, "SCLS %u does not lead to %s", (unsigned)index, vanillaStage);
        refuse(f, b);
        return;
    }
    sclsWrite(e, stage, room, spawn);
    f.st->written++;
}

// b: the first PLYR whose spawn id (z_rot & 0xFF) is `spawn`: spawn type 5 -> 1.
void fixSpawnType(FileCtx& f, uint8_t spawn) {
    const uint32_t n = pc_rando_dzx_count(&f.dzx, "PLYR");
    for (uint32_t i = 0; i < n; i++) {
        uint8_t* e = pc_rando_dzx_entry(&f.dzx, "PLYR", i);
        if (e == nullptr || e[0x1D] != spawn) {
            continue;
        }
        const uint32_t params = pc_rando_dzx_be32(e + 8);
        if (((params >> 12) & 0xF) == 5) {
            const uint32_t v = (params & ~0x0000F000u) | 0x00001000u;
            pc_rando_dzx_set_u32(&f.dzx, "PLYR", i, 8, v, nullptr);
            f.st->written++;
        }
        return;
    }
    // wwrando's next() would raise; R3.16's spawns 27 are missing (not applied, or a changed file).
    char b[64];
    snprintf(b, sizeof b, "no PLYR with spawn id %u", (unsigned)spawn);
    refuse(f, b);
}

int islandNumber(const char* name) {
    if (name == nullptr) {
        return -1;
    }
    for (const Island& i : islands()) {
        if (i.number >= 1 && strcmp(i.name, name) == 0) {
            return i.number;
        }
    }
    return -1;
}

// f: 2DMA[0] sector -> the island's.
void setSector(FileCtx& f, const char* islandName) {
    const uint8_t* stag = pc_rando_dzx_entry(&f.dzx, "STAG", 0);
    const int stageId = stag != nullptr ? (stag[9] & 0xFE) >> 1 : -1;
    const int n = islandNumber(islandName);
    if (stageId < 11 || stageId > 13 || n < 1) {
        char b[96];
        snprintf(b, sizeof b, "STAG stage id %d (11-13 expected), island \"%s\"", stageId,
                 islandName != nullptr ? islandName : "?");
        refuse(f, b);
        return;
    }
    if (pc_rando_dzx_entry(&f.dzx, "2DMA", 0) == nullptr) {
        refuse(f, "no 2DMA");
        return;
    }
    const int sx = (n - 1) % 7 - 3;
    const int sy = (n - 1) / 7 - 3;
    const uint8_t v = (uint8_t)((sx & 0xF) | ((sy & 0xF) << 4));
    pc_rando_dzx_set_u8(&f.dzx, "2DMA", 0, 0x36, v, nullptr);
    f.st->written++;
}

// g: the first VolTag of the ACTR chunks (ACTR, ACT0-ACTb) in file order.
void setVolTag(FileCtx& f, bool fireMountainCave, int entrance, int fireEntrance, int iceEntrance) {
    for (uint32_t c = 0; c < f.dzx.chunkCount; c++) {
        char tag[5];
        memcpy(tag, f.dzx.data + 4 + c * 12, 4);
        tag[4] = '\0';
        if (strncmp(tag, "ACT", 3) != 0) {
            continue;
        }
        const uint32_t n = pc_rando_dzx_count(&f.dzx, tag);
        for (uint32_t i = 0; i < n; i++) {
            uint8_t* e = pc_rando_dzx_entry(&f.dzx, tag, i);
            if (e == nullptr || memcmp(e, "VolTag\0\0", 8) != 0) {
                continue;
            }
            const int own = fireMountainCave ? fireEntrance : iceEntrance;
            const int other = fireMountainCave ? iceEntrance : fireEntrance;
            if (entrance == own) {
                return; // unchanged from vanilla
            }
            if (entrance == other) {
                // The other cave's timer: Ice Ring's kill trigger is type 2, Fire Mountain's type 1.
                const uint32_t type = fireMountainCave ? 2u : 1u;
                const uint32_t params = pc_rando_dzx_be32(e + 8);
                pc_rando_dzx_set_u32(&f.dzx, tag, i, 8, (params & ~0xC0u) | (type << 6), nullptr);
            } else {
                pc_rando_dzx_suppress(&f.dzx, tag, i, nullptr);
            }
            f.st->written++;
            return;
        }
    }
    // A file already patched (the name cleared) also lands here; wwrando's next() would raise.
    refuse(f, "no VolTag");
}

// The stage of the entrance an exit has by default (EntranceRandomizer.entrance_connections).
const char* vanillaEntranceStage(int exit) {
    for (int v = 0; v < kNumZoneEntrances; v++) {
        if (defaultConnectionExit(v) == exit) {
            return zoneEntrances()[v].stage;
        }
    }
    return nullptr;
}

} // namespace

const EntrSclsDest* entrStageDungeonStart(const char* entranceStage) {
    for (const EntrSclsDest& d : kDungeonStarts) {
        if (strcmp(d.stage, entranceStage) == 0) {
            return &d;
        }
    }
    return nullptr;
}

bool entrStageMapFromSeed(const PcRandoSeed& seed, EntrStageMap& out, std::string* err) {
    Options o;
    std::string e;
    if (!optionsFromJson(seed.options, &o, &e)) {
        if (err != nullptr) {
            *err = "options: " + e;
        }
        return false;
    }
    if (!entranceRandoEnabled(o)) {
        if (err != nullptr) {
            err->clear();
        }
        return false;
    }
    out.conn = EntranceConnections::defaults();
    for (const auto& [entranceName, exitName] : seed.entrances) {
        const int en = zoneEntranceIndex(entranceName);
        const int ex = zoneExitIndex(exitName);
        if (en < 0 || ex < 0) {
            if (err != nullptr) {
                *err = "unknown entrance or exit \"" + entranceName + "\" -> \"" + exitName + "\"";
            }
            return false;
        }
        out.conn.entranceToExit[en] = ex;
    }
    // done_exits_to_entrances from the entrance side (an exit listed twice is a bad map).
    for (int x = 0; x < kNumZoneExits; x++) {
        out.conn.exitToEntrance[x] = -1;
    }
    for (int en = 0; en < kNumZoneEntrances; en++) {
        const int ex = out.conn.entranceToExit[en];
        if (out.conn.exitToEntrance[ex] >= 0) {
            if (err != nullptr) {
                *err = std::string("exit \"") + zoneExits()[ex].name + "\" has two entrances";
            }
            return false;
        }
        out.conn.exitToEntrance[ex] = en;
    }
    for (int x = 0; x < kNumZoneExits; x++) {
        std::string why;
        out.outermost[x] = outermostEntranceForExit(out.conn, x, &why);
        if (out.outermost[x] < 0 || zoneEntrances()[out.outermost[x]].islandName == nullptr) {
            if (err != nullptr) {
                *err = std::string("exit \"") + zoneExits()[x].name + "\": " + (why.empty() ? "no island" : why);
            }
            return false;
        }
    }
    return true;
}

int entrStagePlyrCount(const uint8_t* data, size_t size) {
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, (void*)data, size) != PCR_DZX_OK) {
        return 0;
    }
    return (int)pc_rando_dzx_count(&dzx, "PLYR");
}

bool entrStagePatch(const EntrStageMap& m, const char* stage, int room, uint8_t* data, size_t size, int stagePlyr,
                    EntrStagePatchStats* stats) {
    EntrStagePatchStats local;
    FileCtx f;
    f.stage = stage;
    f.room = room;
    f.stagePlyr = stagePlyr;
    f.st = stats != nullptr ? stats : &local;
    if (pc_rando_dzx_open(&f.dzx, data, size) != PCR_DZX_OK) {
        return false;
    }
    const Span<ZoneEntranceDef> ens = zoneEntrances();
    const Span<ZoneExitDef> exs = zoneExits();
    const int savage = zoneExitIndex("Savage Labyrinth");
    const int fireCave = zoneExitIndex("Fire Mountain Secret Cave");
    const int iceCave = zoneExitIndex("Ice Ring Isle Secret Cave");
    const int iceInner = zoneExitIndex("Ice Ring Isle Inner Cave");
    const int fireEntrance = zoneEntranceIndex("Secret Cave Entrance on Fire Mountain");
    const int iceEntrance = zoneEntranceIndex("Secret Cave Entrance on Ice Ring Isle");
    const bool plyrInStage = room < 0 ? pc_rando_dzx_count(&f.dzx, "PLYR") > 0 : stagePlyr > 0;
    bool warnedPlyr = false;

    for (int x = 0; x < kNumZoneExits; x++) {
        const ZoneExitDef& X = exs[x];
        const int en = m.conn.exitToEntrance[x];
        if (en < 0) {
            continue;
        }
        const ZoneEntranceDef& E = ens[en];
        const ZoneEntranceDef& O = ens[m.outermost[x]];

        // a. The entrance's SCLS leads into the exit.
        if (isFile(f, E.stage, E.room)) {
            setScls(f, (uint32_t)E.sclsExitIndex, exs[defaultConnectionExit(en)].stage, X.stage, (uint8_t)X.room,
                    X.spawnId);
        }
        // b. The entrance's spawn: type 5 -> 1.
        if (strcmp(f.stage, E.stage) == 0 && (room < 0 ? plyrInStage : (room == E.room && !plyrInStage))) {
            if (room >= 0 && stagePlyr < 0 && !warnedPlyr) {
                warnedPlyr = true;
                logf("[cos] rando: entrances %s/Room%d: the stage.dzs PLYR count is unknown; taken as 0\n", stage,
                     room);
            }
            fixSpawnType(f, E.spawnId);
        }
        // c. Save and reload inside a (mini)boss arena.
        if (isMinibossOrBoss(X.group) && isFile(f, X.stage, -1)) {
            if (isMinibossOrBoss(E.group)) {
                const EntrSclsDest* d = entrStageDungeonStart(E.stage);
                if (d == nullptr) {
                    refuse(f, "no dungeon start for the entrance's stage");
                } else {
                    setScls(f, 0, nullptr, d->stage, d->room, d->spawn);
                }
            } else {
                setScls(f, 0, nullptr, E.stage, (uint8_t)E.room, E.spawnId);
            }
        }
        // d. The exit's SCLS leads back out to the entrance.
        if (X.group != EntranceGroup::Boss && isFile(f, X.stage, X.room)) {
            // The vanilla record leads to the vanilla entrance's stage, except Ice Ring Isle Inner
            // Cave's, which leads to the sea (sea/40 spawn 1) rather than into MiniHyo.
            const char* vanilla = x == iceInner ? "sea" : vanillaEntranceStage(x);
            setScls(f, (uint32_t)X.sclsExitIndex, vanilla, E.stage, (uint8_t)E.room, E.spawnId);
        }
        // e. Savage Labyrinth's other exits to the sea.
        if (x == savage && room >= 0) {
            for (const auto& sr : kSavageRooms) {
                if (!isFile(f, sr.stage, sr.room)) {
                    continue;
                }
                const uint32_t n = pc_rando_dzx_count(&f.dzx, "SCLS");
                for (uint32_t i = 0; i < n; i++) {
                    uint8_t* e = pc_rando_dzx_entry(&f.dzx, "SCLS", i);
                    if (e != nullptr && sclsDestIs(e, "sea")) {
                        sclsWrite(e, E.stage, (uint8_t)E.room, E.spawnId);
                        f.st->written++;
                    }
                }
            }
        }
        // f. The sector save-and-quit puts the player in.
        if ((X.group == EntranceGroup::SecretCave || X.group == EntranceGroup::FairyFountain || x == iceInner) &&
            isFile(f, X.stage, -1)) {
            setSector(f, O.islandName);
        }
        // g. The kill trigger of the Fire Mountain / Ice Ring Isle caves.
        if ((x == fireCave || x == iceCave) && isFile(f, X.stage, X.room)) {
            setVolTag(f, x == fireCave, en, fireEntrance, iceEntrance);
        }
    }
    return true;
}

} // namespace pcr

// --- run time -------------------------------------------------------------------------------------

namespace pcr {

namespace {

EntrStageRuntimeStats sStats;

// The active map, built once per seed (by its hash).
const PcRandoSeed* sMapSeed = nullptr;
uint8_t sMapHash[16];
bool sMapValid = false;
EntrStageMap sMap;

// The stage.dzs last dispatched: its stage and PLYR count (the rooms of a stage load after it).
char sLastStage[PCR_RES_DIR_MAX] = "";
int sLastStagePlyr = -1;

} // namespace

const EntrStageMap* entrStageActiveMap() {
    if (!pc_rando()) {
        return nullptr;
    }
    const PcRandoSeed* seed = pc_rando_seed();
    if (seed == nullptr) {
        return nullptr;
    }
    if (seed != sMapSeed || memcmp(seed->hash, sMapHash, sizeof sMapHash) != 0) {
        sMapSeed = seed;
        memcpy(sMapHash, seed->hash, sizeof sMapHash);
        std::string err;
        sMapValid = entrStageMapFromSeed(*seed, sMap, &err);
        if (sMapValid) {
            logf("[cos] rando: entrances: stage data follows seed %s's entrance map\n",
                 pc_rando_seed_hash_hex(*seed).c_str());
        } else if (!err.empty()) {
            logf("[cos] rando: entrances: seed %s: %s; stage data left vanilla\n", pc_rando_seed_hash_hex(*seed).c_str(),
                 err.c_str());
        }
    }
    return sMapValid ? &sMap : nullptr;
}

const EntrStageRuntimeStats& entrStageRuntimeStats() {
    return sStats;
}

void entrStageRuntimeReset() {
    sStats = EntrStageRuntimeStats();
    sLastStage[0] = '\0';
    sLastStagePlyr = -1;
}

} // namespace pcr

extern "C" {

void pc_rando_entr_stage_patch(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->stage[0] == '\0') {
        return;
    }
    const bool dzs = file->type == PCR_RES_DZS && strcmp(file->name, "stage.dzs") == 0;
    const bool dzr = file->type == PCR_RES_DZR && strcmp(file->name, "room.dzr") == 0 && file->room >= 0;
    if (!dzs && !dzr) {
        return;
    }
    if (dzs) {
        snprintf(pcr::sLastStage, sizeof pcr::sLastStage, "%s", file->stage);
        pcr::sLastStagePlyr = pcr::entrStagePlyrCount(file->data, file->size);
    }
    const pcr::EntrStageMap* m = pcr::entrStageActiveMap();
    if (m == nullptr) {
        pcr::sStats.noMap++;
        return;
    }
    pcr::sStats.files++;
    const int room = dzs ? -1 : file->room;
    if (dzs) {
        // D9 for kDungeonStarts: this stage's own SCLS[0], before any edit.
        const pcr::EntrSclsDest* d = pcr::entrStageDungeonStart(file->stage);
        PcRandoDzx dzx;
        if (d != nullptr && pc_rando_dzx_open(&dzx, file->data, file->size) == PCR_DZX_OK) {
            const uint8_t* e = pc_rando_dzx_entry(&dzx, "SCLS", 0);
            if (e == nullptr || !pcr::entrSclsEquals(e, d->stage, d->room, d->spawn)) {
                pcr::sStats.startMismatches++;
                pcr::logf("[cos] rando: entrances %s/Stage: SCLS[0] is not %s room %u spawn %u (the dungeon start "
                          "a save-and-reload in a nested (mini)boss arena uses)\n",
                          file->stage, d->stage, (unsigned)d->room, (unsigned)d->spawn);
            }
        }
    }
    const int stagePlyr = strcmp(pcr::sLastStage, file->stage) == 0 ? pcr::sLastStagePlyr : -1;
    pcr::EntrStagePatchStats st;
    if (!pcr::entrStagePatch(*m, file->stage, room, file->data, file->size, stagePlyr, &st)) {
        pcr::sStats.badFiles++;
        pcr::logf("[cos] rando: entrances %s/%s: does not parse; left vanilla\n", file->stage, file->arc);
        return;
    }
    pcr::sStats.written += st.written;
    pcr::sStats.refused += st.refused;
    if (st.written > 0) {
        pcr::sStats.patched++;
    }
}

void pc_rando_entr_stage_init(void) {
    pc_rando_register_dzx_patcher(pc_rando_entr_stage_patch);
}

} // extern "C"
