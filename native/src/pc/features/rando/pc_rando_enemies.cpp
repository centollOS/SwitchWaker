// The enemy randomizer's actor rewrites (pc_rando_enemies.h, docs/RANDOMIZER_PLAN.md step R7.4a):
// wwrando 1.10.0_9775811 randomizers/enemies.py save_changed_enemies_and_randomize_their_params and
// adjust_enemy on the raw room files. Pure: no game headers, no game calls.
#include "pc_rando_enemies.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_enemy_data.h"
#include "pc_rando_gen_enemies.h"
#include "pc_rando_seedfile.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using pcr::EnemyLocation;
using pcr::EnemyRandoResult;
using pcr::EnemyType;

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

void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void putf(uint8_t* p, double d) {
    const float f = (float)d; // struct.pack(">f"): round to nearest
    uint32_t v;
    memcpy(&v, &f, 4);
    put32(p, v);
}

// ACTR layout (wwlib/dzx.py ACTR): name[8], params u32 0x08, x/y/z f32 0x0C/0x10/0x14,
// x/y/z_rot u16 0x18/0x1A/0x1C, enemy_number u16 0x1E.
constexpr uint32_t kActrSize = 0x20;

// An ACTR entry as Python sees it while _save edits it.
struct Actor {
    char name[9];
    uint32_t params;
    double x, y, z;
    uint16_t x_rot, y_rot, z_rot;
};

void readActor(const uint8_t* e, Actor* a) {
    memcpy(a->name, e, 8);
    a->name[8] = '\0';
    a->params = pc_rando_dzx_be32(e + 0x08);
    a->x = pc_rando_dzx_bef32(e + 0x0C);
    a->y = pc_rando_dzx_bef32(e + 0x10);
    a->z = pc_rando_dzx_bef32(e + 0x14);
    a->x_rot = pc_rando_dzx_be16(e + 0x18);
    a->y_rot = pc_rando_dzx_be16(e + 0x1A);
    a->z_rot = pc_rando_dzx_be16(e + 0x1C);
}

void writeActor(uint8_t* e, const Actor& a) {
    memset(e, 0, 8);
    memcpy(e, a.name, strnlen(a.name, 7)); // fs.write_str(..., 8): at most 7 bytes and a NUL
    put32(e + 0x08, a.params);
    putf(e + 0x0C, a.x);
    putf(e + 0x10, a.y);
    putf(e + 0x14, a.z);
    put16(e + 0x18, a.x_rot);
    put16(e + 0x1A, a.y_rot);
    put16(e + 0x1C, a.z_rot);
    // enemy_number (0x1E) is kept
}

uint32_t* fieldOf(Actor& a, pcr::ActorBitfield b, uint32_t* tmp) {
    switch (b) {
    case pcr::ActorBitfield::params:
        return &a.params;
    case pcr::ActorBitfield::x_rot:
        *tmp = a.x_rot;
        return tmp;
    default:
        *tmp = a.z_rot;
        return tmp;
    }
}

// getattr(enemy, param) with the bitfields of class cls. False when the class has no such param.
bool getParam(const Actor& a, const char* cls, const char* param, uint32_t* out) {
    const pcr::ActorParamField* f = cls != nullptr && param != nullptr ? pcr::actor_param(cls, param) : nullptr;
    if (f == nullptr) {
        return false;
    }
    uint32_t tmp;
    const uint32_t* v = fieldOf(const_cast<Actor&>(a), f->bitfield, &tmp);
    *out = pcr::actor_param_get(*f, *v);
    return true;
}

// setattr(enemy, param, value). False when the class has no such param.
bool setParam(Actor& a, const char* cls, const char* param, uint32_t value) {
    const pcr::ActorParamField* f = cls != nullptr && param != nullptr ? pcr::actor_param(cls, param) : nullptr;
    if (f == nullptr) {
        return false;
    }
    uint32_t tmp;
    uint32_t* v = fieldOf(a, f->bitfield, &tmp);
    *v = pcr::actor_param_set(*f, *v, value);
    if (f->bitfield == pcr::ActorBitfield::x_rot) {
        a.x_rot = (uint16_t)tmp;
    } else if (f->bitfield == pcr::ActorBitfield::z_rot) {
        a.z_rot = (uint16_t)tmp;
    }
    return true;
}

const char* classOf(const char* name) {
    return pcr::actor_class_name(name);
}

int8_t* sResults = nullptr; // per enemy location, -1 = not seen
size_t sResultsSize = 0;
PcRandoEnemyStats sStats;
bool sInit = false;
bool sBypass = false;

// The active seed's enemies, parsed once per seed.
const PcRandoSeed* sSeed = nullptr;
uint8_t sSeedHash[16];
bool sSeedHas = false;
EnemyRandoResult sEnemies;
std::vector<int> sPlacementOf; // enemy location index -> index into sEnemies.placements, or -1
std::vector<int> placementIndex(const EnemyRandoResult& seed);

void resetResults() {
    const size_t n = pcr::enemy_locations().size;
    if (sResults == nullptr) {
        sResults = new int8_t[n];
        sResultsSize = n;
    }
    memset(sResults, -1, sResultsSize);
    sStats = PcRandoEnemyStats{};
}

bool seedEnemies() {
    const PcRandoSeed* seed = pc_rando() ? pc_rando_seed() : nullptr;
    if (seed == nullptr) {
        if (sSeed != nullptr) {
            sSeed = nullptr;
            sSeedHas = false;
        }
        return false;
    }
    if (seed != sSeed || memcmp(seed->hash, sSeedHash, sizeof sSeedHash) != 0) {
        sSeed = seed;
        memcpy(sSeedHash, seed->hash, sizeof sSeedHash);
        sSeedHas = false;
        sEnemies = EnemyRandoResult();
        resetResults();
        const pcr::Json* j = seed->extra.find("enemies");
        if (j != nullptr) {
            std::string err;
            if (pcr::enemiesFromJson(*j, sEnemies, &err)) {
                sSeedHas = true;
                sPlacementOf = placementIndex(sEnemies);
                logf("[cos] rando: enemies: %zu placed enemies\n", sEnemies.placements.size());
            } else {
                logf("[cos] rando: enemies: the seed's enemies are unusable (%s); enemies stay vanilla\n",
                     err.c_str());
            }
        }
    }
    return sSeedHas;
}

// The nearest d_a_tsubo entry of a chunk to (x, y, z): Python's min() over the chunk order with
// distance_between_entities. False when the chunk has none.
bool nearestPot(const PcRandoDzx* dzx, const char* tag, const Actor& enemy, Actor* pot) {
    const uint32_t n = pc_rando_dzx_count(dzx, tag);
    bool found = false;
    double best = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
        if (e == nullptr) {
            continue;
        }
        Actor a;
        readActor(e, &a);
        const char* cls = classOf(a.name);
        if (cls == nullptr || strcmp(cls, "d_a_tsubo") != 0) {
            continue;
        }
        const double dx = a.x - enemy.x, dy = a.y - enemy.y, dz = a.z - enemy.z;
        const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!found || d < best) {
            found = true;
            best = d;
            *pot = a;
        }
    }
    return found;
}

// One enemy location on an open file. Returns a PcRandoEnemyResult.
int applyOne(const pcr::EnemyPlacement& pl, const EnemyLocation& loc, PcRandoDzx* dzx, std::string* why) {
    char tag[5];
    if (!pc_rando_dzx_tag("ACTR", loc.path.layer, tag)) {
        *why = "bad layer";
        return PCR_ENEMY_MISMATCH;
    }
    uint8_t* e = pc_rando_dzx_entry(dzx, tag, loc.path.index);
    if (e == nullptr) {
        *why = std::string("no ") + tag + " entry";
        return PCR_ENEMY_MISMATCH;
    }
    const EnemyType& orig = pcr::enemy_types()[loc.original_type];
    const EnemyType& neu = pcr::enemy_types()[pl.type];
    Actor a;
    readActor(e, &a);
    if (strncmp(a.name, orig.actor_name, 8) != 0) {
        *why = std::string("the entry is \"") + a.name + "\", not the original " + orig.actor_name;
        return PCR_ENEMY_MISMATCH;
    }
    const char* origCls = classOf(orig.actor_name);
    char buf[160];

    // The values _save copies, read with the original type's names.
    bool haveSpawn = false, haveDeath = false, havePath = false;
    uint32_t spawnSw = 0, deathSw = 0, pathIdx = 0;
    if (pl.flags & pcr::EFLAG_SPAWN_SWITCH) {
        if (orig.spawn_switch_param == nullptr || !getParam(a, origCls, orig.spawn_switch_param, &spawnSw)) {
            *why = "the original enemy cannot check a spawn switch";
            return PCR_ENEMY_REFUSED;
        }
        if (spawnSw == 0xFE || spawnSw == 0xFF || spawnSw >= 0xF0) {
            snprintf(buf, sizeof buf, "spawn switch %02X is not valid for all enemy types", (unsigned)spawnSw);
            *why = buf;
            return PCR_ENEMY_REFUSED;
        }
        haveSpawn = true;
    }
    if (pl.flags & pcr::EFLAG_DEATH_SWITCH) {
        if (orig.death_switch_param == nullptr || !getParam(a, origCls, orig.death_switch_param, &deathSw)) {
            *why = "the original enemy cannot set a death switch";
            return PCR_ENEMY_REFUSED;
        }
        if (deathSw == 0x00 || deathSw == 0x80 || deathSw == 0xFF || deathSw >= 0xF0) {
            snprintf(buf, sizeof buf, "death switch %02X is not valid for all enemy types", (unsigned)deathSw);
            *why = buf;
            return PCR_ENEMY_REFUSED;
        }
        haveDeath = true;
    }
    if (orig.path_param != nullptr) {
        if (!getParam(a, origCls, orig.path_param, &pathIdx)) {
            *why = std::string("unknown path param ") + orig.path_param;
            return PCR_ENEMY_REFUSED;
        }
        if (pathIdx != 0xFF) {
            if (pathIdx >= pc_rando_dzx_count(dzx, "RPAT")) {
                snprintf(buf, sizeof buf, "path index %u >= the RPAT count %u", (unsigned)pathIdx,
                         (unsigned)pc_rando_dzx_count(dzx, "RPAT"));
                *why = buf;
                return PCR_ENEMY_REFUSED;
            }
            havePath = true;
        }
    }

    // The new enemy (randomize_enemy_params already ran in the generator).
    memset(a.name, 0, sizeof a.name);
    strncpy(a.name, neu.actor_name, 7);
    a.params = pl.params;
    a.x_rot = pl.x_rot;
    a.z_rot = pl.z_rot;
    if (loc.flags & pcr::ELOC_POSITION) {
        a.x = loc.position[0];
        a.y = loc.position[1];
        a.z = loc.position[2];
    }
    if (loc.flags & pcr::ELOC_POSITION_OFFSET) {
        a.x += loc.position_offset[0];
        a.y += loc.position_offset[1];
        a.z += loc.position_offset[2];
    }
    if (loc.flags & pcr::ELOC_Y_ROTATION) {
        a.y_rot = loc.y_rot;
    }
    if (loc.flags & pcr::ELOC_Y_ROTATION_OFFSET) {
        a.y_rot = (uint16_t)(((int64_t)a.y_rot + loc.y_rot_offset) & 0xFFFF);
    }

    // adjust_enemy
    if (pl.flags & pcr::EFLAG_CEILING_DROP) {
        a.y -= 500.0;
    }
    if (pl.flags & pcr::EFLAG_MAGTAIL_RAISE) {
        a.y += 50.0;
    } else if (pl.flags & pcr::EFLAG_POT_SNAP) {
        Actor pot;
        if (!nearestPot(dzx, tag, a, &pot)) {
            *why = "no pots on same layer as ChuChu in a pot";
            return PCR_ENEMY_REFUSED;
        }
        a.x = pot.x;
        a.y = pot.y;
        a.z = pot.z;
    }

    // The copied values, with the new type's names.
    const char* newCls = classOf(neu.actor_name);
    if (haveSpawn) {
        if (neu.spawn_switch_param == nullptr || !setParam(a, newCls, neu.spawn_switch_param, spawnSw)) {
            *why = std::string("the new enemy ") + neu.actor_name + " cannot check a spawn switch";
            return PCR_ENEMY_REFUSED;
        }
        bool ok = true;
        if (newCls != nullptr && strcmp(newCls, "d_a_rd") == 0) {
            ok = setParam(a, newCls, "dont_check_enable_spawn_switch", 0);
        } else if (newCls != nullptr && strcmp(newCls, "d_a_st") == 0) {
            ok = setParam(a, newCls, "stalfos_type", 1);
        }
        if (!ok) {
            *why = std::string("unknown spawn switch companion param of ") + newCls;
            return PCR_ENEMY_REFUSED;
        }
    }
    if (haveDeath) {
        if (neu.death_switch_param == nullptr || !setParam(a, newCls, neu.death_switch_param, deathSw)) {
            *why = std::string("the new enemy ") + neu.actor_name + " cannot set a death switch";
            return PCR_ENEMY_REFUSED;
        }
    }
    if (havePath && neu.path_param != nullptr) {
        bool ok = setParam(a, newCls, neu.path_param, pathIdx);
        if (ok && newCls != nullptr && strcmp(newCls, "d_a_fm") == 0) {
            ok = setParam(a, newCls, "type", 1);
        }
        if (!ok) {
            *why = std::string("unknown path param of ") + neu.actor_name;
            return PCR_ENEMY_REFUSED;
        }
    }

    writeActor(e, a);
    return PCR_ENEMY_WRITTEN;
}

const char* resultName(int r) {
    switch (r) {
    case PCR_ENEMY_WRITTEN:
        return "written";
    case PCR_ENEMY_MISMATCH:
        return "mismatch";
    case PCR_ENEMY_REFUSED:
        return "refused";
    default:
        return "not placed";
    }
}

std::vector<int> placementIndex(const EnemyRandoResult& seed) {
    std::vector<int> of(pcr::enemy_locations().size, -1);
    for (size_t i = 0; i < seed.placements.size(); i++) {
        const int loc = seed.placements[i].location;
        if (loc >= 0 && (size_t)loc < of.size()) {
            of[loc] = (int)i;
        }
    }
    return of;
}

int applyWith(const EnemyRandoResult& seed, const std::vector<int>& of, const char* stage, int room, uint8_t* data,
              size_t size, int8_t* results, int* handled) {
    int n = 0, h = 0;
    if (handled != nullptr) {
        *handled = 0;
    }
    if (stage == nullptr || room < 0) {
        return 0; // every enemy location is in a room.dzr
    }
    const pcr::Span<EnemyLocation> locs = pcr::enemy_locations();
    PcRandoDzx dzx;
    bool opened = false, openFailed = false;
    for (size_t i = 0; i < locs.size; i++) {
        const EnemyLocation& loc = locs[i];
        if (loc.path.room != room || strcmp(loc.path.stage, stage) != 0) {
            continue;
        }
        h++;
        if (!opened && !openFailed) {
            openFailed = pc_rando_dzx_open(&dzx, data, size) != PCR_DZX_OK;
            opened = !openFailed;
        }
        int r;
        std::string why;
        if (openFailed) {
            r = PCR_ENEMY_MISMATCH;
            why = "the file does not parse";
        } else if (of[i] < 0) {
            r = PCR_ENEMY_NOT_PLACED;
        } else {
            r = applyOne(seed.placements[of[i]], loc, &dzx, &why);
        }
        if (results != nullptr) {
            results[i] = (int8_t)r;
        }
        if (r == PCR_ENEMY_WRITTEN) {
            n++;
        } else {
            logf("[cos] rando: enemies: %s: %s, left vanilla (%s)\n", loc.path.raw, resultName(r), why.c_str());
        }
    }
    if (handled != nullptr) {
        *handled = h;
    }
    return n;
}

} // namespace

int pc_rando_enemies_apply(const pcr::EnemyRandoResult& seed, const char* stage, int room, uint8_t* data,
                           size_t size, int8_t* results, int* handled) {
    return applyWith(seed, placementIndex(seed), stage, room, data, size, results, handled);
}

extern "C" {

void pc_rando_enemies_stats(PcRandoEnemyStats* out) {
    if (out != nullptr) {
        *out = sStats;
    }
}

int pc_rando_enemies_active(void) {
    return seedEnemies() ? 1 : 0;
}

int pc_rando_enemies_last_result(int loc) {
    if (sResults == nullptr || loc < 0 || (size_t)loc >= sResultsSize) {
        return -1;
    }
    return sResults[loc];
}

void pc_rando_enemies_set_bypass(int on) {
    sBypass = on != 0;
}

void pc_rando_enemies_patch(PcRandoResFile* file) {
    if (sBypass || file == nullptr || file->stage == nullptr || file->stage[0] == '\0' || file->name == nullptr ||
        file->room < 0 || strcmp(file->name, "room.dzr") != 0) {
        return;
    }
    // Only files that hold enemy locations.
    if (pcr::enemy_stage_index(file->stage) < 0) {
        char key[48];
        snprintf(key, sizeof key, "%s/Room%d.arc", file->stage, file->room);
        if (pcr::enemy_stage_index(key) < 0) {
            return;
        }
    }
    if (!seedEnemies()) {
        sStats.skipped++;
        return;
    }
    int handled = 0;
    const int n = applyWith(sEnemies, sPlacementOf, file->stage, file->room, file->data, file->size, sResults,
                            &handled);
    if (handled == 0) {
        return;
    }
    sStats.files++;
    sStats.written += (uint32_t)n;
    const pcr::Span<EnemyLocation> locs = pcr::enemy_locations();
    int mism = 0, ref = 0;
    for (size_t i = 0; i < locs.size; i++) {
        if (locs[i].path.room != file->room || strcmp(locs[i].path.stage, file->stage) != 0) {
            continue;
        }
        mism += sResults[i] == PCR_ENEMY_MISMATCH ? 1 : 0;
        ref += sResults[i] == PCR_ENEMY_REFUSED ? 1 : 0;
    }
    sStats.mismatched += (uint32_t)mism;
    sStats.refused += (uint32_t)ref;
    logf("[cos] rando: enemies: %s/Room%d room.dzr: %d of %d enemy record(s) written\n", file->stage, file->room, n,
         handled);
}

void pc_rando_enemies_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    resetResults();
    pc_rando_register_dzx_patcher(pc_rando_enemies_patch);
}

} // extern "C"
