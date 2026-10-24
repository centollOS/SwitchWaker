// The enemy randomizer's particles (pc_rando_enemy_particles.h, docs/RANDOMIZER_PLAN.md step R7.4c):
// wwrando 1.10.0_9775811 randomizers/enemies.py particles_to_load_for_each_jpc_index (end of
// save_changed_enemies_and_randomize_their_params) and update_loaded_particles, as imports into the
// particle bank registry of R7.1b. Pure: no game headers, no game calls.
#include "pc_rando_enemy_particles.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_enemy_data.h"
#include "pc_rando_gen_enemies.h"
#include "pc_rando_jpc_scene.h"
#include "pc_rando_seedfile.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using pcr::EnemyLocation;
using pcr::EnemyRandoResult;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

bool isSea(const char* stage) {
    return strcmp(stage, "sea") == 0;
}

// wwlib/dzx.py STAG: params_2 u16 at 0x0A, loaded_particle_bank mask 0x07F8.
int stagBank(const uint8_t* e) {
    return (pc_rando_dzx_be16(e + 0x0A) & 0x07F8) >> 3;
}
// wwlib/dzx.py FILI: params u32 at 0x00, loaded_particle_bank mask 0x1FE00000.
int filiBank(const uint8_t* e) {
    return (int)((pc_rando_dzx_be32(e) & 0x1FE00000u) >> 21);
}

// The active seed's enemies, parsed once per seed (as pc_rando_enemies.cpp does).
const PcRandoSeed* sSeed = nullptr;
uint8_t sSeedHash[16];
bool sSeedHas = false;
EnemyRandoResult sEnemies;
PcRandoEnemyParticleStats sStats = {0, 0, 0, 0, -1};
bool sInit = false;

struct Place {
    std::string stage;
    int room;
    int bank;
};
std::vector<Place> sPlaces; // the banks read in this activation

void resetState() {
    sStats = PcRandoEnemyParticleStats{0, 0, 0, 0, -1};
    sPlaces.clear();
}

bool seedEnemies() {
    const PcRandoSeed* seed = pc_rando() ? pc_rando_seed() : nullptr;
    if (seed == nullptr) {
        if (sSeed != nullptr) {
            sSeed = nullptr;
            sSeedHas = false;
            resetState();
        }
        return false;
    }
    if (seed != sSeed || memcmp(seed->hash, sSeedHash, sizeof sSeedHash) != 0) {
        sSeed = seed;
        memcpy(sSeedHash, seed->hash, sizeof sSeedHash);
        sSeedHas = false;
        sEnemies = EnemyRandoResult();
        resetState();
        const pcr::Json* j = seed->extra.find("enemies");
        if (j != nullptr) {
            std::string err;
            sSeedHas = pcr::enemiesFromJson(*j, sEnemies, &err);
            // pc_rando_enemies.cpp logs an unusable key.
        }
    }
    return sSeedHas;
}

// Python's `stage_folder == "sea"` place key of a location: the stage folder, plus the room on the sea.
bool samePlace(const EnemyLocation& loc, const char* stage, int room) {
    if (strcmp(loc.path.stage, stage) != 0) {
        return false;
    }
    return !isSea(stage) || loc.path.room == room;
}

} // namespace

void pc_rando_enemy_particles_ids(const EnemyRandoResult& seed, const char* stage, int room,
                                  std::vector<uint16_t>* ids) {
    ids->clear();
    if (stage == nullptr) {
        return;
    }
    const pcr::Span<EnemyLocation> locs = pcr::enemy_locations();
    const pcr::Span<pcr::EnemyType> types = pcr::enemy_types();
    for (const pcr::EnemyPlacement& pl : seed.placements) {
        if (pl.location < 0 || (size_t)pl.location >= locs.size || pl.type < 0 || (size_t)pl.type >= types.size) {
            continue;
        }
        if (!samePlace(locs[pl.location], stage, room)) {
            continue;
        }
        const pcr::Span<uint16_t> req = pcr::enemy_type_particles(types[pl.type]);
        for (size_t k = 0; k < req.size; k++) {
            bool have = false;
            for (uint16_t id : *ids) {
                have = have || id == req[k];
            }
            if (!have) {
                ids->push_back(req[k]);
            }
        }
    }
}

int pc_rando_enemy_particles_for_file(const EnemyRandoResult& seed, const char* stage, int room, const char* name,
                                      uint8_t* data, size_t size, int* bank, std::vector<uint16_t>* ids) {
    ids->clear();
    if (stage == nullptr || stage[0] == '\0' || name == nullptr || data == nullptr) {
        return 0;
    }
    const bool sea = isSea(stage);
    if (sea ? (room < 0 || strcmp(name, "room.dzr") != 0) : (strcmp(name, "stage.dzs") != 0)) {
        return 0;
    }
    pc_rando_enemy_particles_ids(seed, stage, room, ids);
    if (ids->empty()) {
        return 0;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, data, size) != PCR_DZX_OK) {
        return -1;
    }
    const uint8_t* e = pc_rando_dzx_entry(&dzx, sea ? "FILI" : "STAG", 0);
    if (e == nullptr) {
        return -1;
    }
    *bank = sea ? filiBank(e) : stagBank(e);
    return 1;
}

extern "C" {

void pc_rando_enemy_particles_stats(PcRandoEnemyParticleStats* out) {
    if (out != nullptr) {
        *out = sStats;
    }
}

size_t pc_rando_enemy_particles_required(const char* stage, int room, uint16_t* out, size_t cap) {
    if (!seedEnemies()) {
        return 0;
    }
    std::vector<uint16_t> ids;
    pc_rando_enemy_particles_ids(sEnemies, stage, room, &ids);
    for (size_t i = 0; out != nullptr && i < ids.size() && i < cap; i++) {
        out[i] = ids[i];
    }
    return ids.size();
}

int pc_rando_enemy_particles_bank_of(const char* stage, int room) {
    if (stage == nullptr) {
        return -1;
    }
    for (const Place& p : sPlaces) {
        if (p.stage == stage && (!isSea(stage) || p.room == room)) {
            return p.bank;
        }
    }
    return -1;
}

void pc_rando_enemy_particles_patch(PcRandoResFile* file) {
    if (file == nullptr || file->stage == nullptr || file->stage[0] == '\0' || file->name == nullptr) {
        return;
    }
    const bool sea = isSea(file->stage);
    if (sea ? (file->room < 0 || strcmp(file->name, "room.dzr") != 0) : strcmp(file->name, "stage.dzs") != 0) {
        return;
    }
    if (!seedEnemies()) {
        return;
    }
    int bank = -1;
    std::vector<uint16_t> ids;
    const int r = pc_rando_enemy_particles_for_file(sEnemies, file->stage, file->room, file->name, file->data,
                                                    file->size, &bank, &ids);
    if (r == 0) {
        return;
    }
    sStats.files++;
    if (r < 0) {
        sStats.failed++;
        logf("[cos] rando: enemy particles: %s%s%s: no %s entry, the %zu required particle id(s) are not imported\n",
             file->stage, sea ? "/Room" : " ", sea ? std::to_string(file->room).c_str() : file->name,
             sea ? "FILI" : "STAG", ids.size());
        return;
    }
    sStats.last_bank = bank;
    sStats.requested += (uint32_t)ids.size();
    bool known = false;
    for (Place& p : sPlaces) {
        if (p.stage == file->stage && (!sea || p.room == file->room)) {
            p.bank = bank;
            known = true;
        }
    }
    if (!known) {
        sPlaces.push_back(Place{file->stage, sea ? file->room : -1, bank});
    }
    if (bank > 254) {
        // wwrando would open files/res/Particle/Pscene255.jpc, which does not exist (it raises).
        sStats.failed++;
        logf("[cos] rando: enemy particles: %s: particle bank %d is not a Pscene file, %zu id(s) not imported\n",
             file->stage, bank, ids.size());
        return;
    }
    int ok = 0;
    for (uint16_t id : ids) {
        if (pc_rando_jpc_scene_add_import(bank, id, PCR_JPC_SCENE_ANY)) {
            ok++;
        } else {
            sStats.failed++;
            logf("[cos] rando: enemy particles: the particle import table is full (0x%04X into Pscene%03d lost)\n", id,
                 bank);
        }
    }
    sStats.registered += (uint32_t)ok;
    if (sea) {
        logf("[cos] rando: enemy particles: sea/Room%d: %d of %zu required particle id(s) registered for Pscene%03d\n",
             file->room, ok, ids.size(), bank);
    } else {
        logf("[cos] rando: enemy particles: %s: %d of %zu required particle id(s) registered for Pscene%03d\n",
             file->stage, ok, ids.size(), bank);
    }
}

void pc_rando_enemy_particles_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_dzx_patcher(pc_rando_enemy_particles_patch);
}

} // extern "C"
