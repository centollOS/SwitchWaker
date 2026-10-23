// Boss warp-out destinations under entrance rando (pc_rando_warpout.h, docs/RANDOMIZER_PLAN.md step
// R6.13): wwrando 1.10.0_9775811 randomizers/entrances.py update_all_boss_warp_out_destinations,
// update_boss_warp_out_destination and update_helmaroc_king_arena_ganon_door. Pure: no game headers.
#include "pc_rando_warpout.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_entrances.h"
#include "pc_rando_seedfile.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

// EntranceRandomizer.is_enabled.
const char* const kEntranceOptions[] = {
    "randomize_dungeon_entrances",          "randomize_secret_cave_entrances",
    "randomize_miniboss_entrances",         "randomize_boss_entrances",
    "randomize_secret_cave_inner_entrances", "randomize_fairy_fountain_entrances",
};

PcRandoWarpoutStatus sStatus = {};
bool sInit = false;

// The BOSS_EXITS entry whose stage is `stage`, or -1.
int bossExitForStage(const char* stage) {
    const pcr::IndexRange r = pcr::exitGroup(pcr::EntranceGroup::Boss);
    for (int i = r.first; i < r.first + r.count; i++) {
        if (strcmp(pcr::zoneExits()[(size_t)i].stage, stage) == 0) {
            return i;
        }
    }
    return -1;
}

// done_exits_to_entrances from the seed's "entrances" (entrance name -> exit unique name, as
// entrance_connections). False (logged) on an unknown name.
bool connectionsOf(const PcRandoSeed& seed, pcr::EntranceConnections& c) {
    for (int i = 0; i < pcr::kNumZoneEntrances; i++) {
        c.entranceToExit[i] = -1;
    }
    for (int i = 0; i < pcr::kNumZoneExits; i++) {
        c.exitToEntrance[i] = -1;
    }
    for (const auto& kv : seed.entrances) {
        const int en = pcr::zoneEntranceIndex(kv.first);
        const int ex = pcr::zoneExitIndex(kv.second);
        if (en < 0 || ex < 0) {
            logf("[cos] rando: warpout: unknown entrance connection '%s' -> '%s'\n", kv.first.c_str(),
                 kv.second.c_str());
            return false;
        }
        c.connect(en, ex);
    }
    return true;
}

void onActivation(int active) {
    (void)active;
    sStatus = PcRandoWarpoutStatus{};
}

void patcher(PcRandoResFile* f) {
    if (!pc_rando()) {
        return;
    }
    (void)pc_rando_warpout_patch_file(f, pc_rando_seed());
}

} // namespace

extern "C" {

int pc_rando_warpout_enabled(const PcRandoSeed* seed) {
    if (seed == nullptr) {
        return 0;
    }
    for (const char* name : kEntranceOptions) {
        const pcr::Json* v = seed->options.find(name);
        if (v != nullptr && v->asBool(false)) {
            return 1;
        }
    }
    return 0;
}

int pc_rando_warpout_dest(const PcRandoSeed* seed, const char* bossStage, PcRandoWarpOut* out) {
    if (bossStage == nullptr || out == nullptr || !pc_rando_warpout_enabled(seed)) {
        return 0;
    }
    const int bossExit = bossExitForStage(bossStage);
    if (bossExit < 0) {
        return 0;
    }
    pcr::EntranceConnections c;
    if (!connectionsOf(*seed, c)) {
        return 0;
    }
    std::string err;
    const int outer = pcr::outermostEntranceForExit(c, bossExit, &err);
    if (outer < 0) {
        logf("[cos] rando: warpout: %s: no outermost entrance (%s); the warp out stays vanilla\n",
             bossStage, err.empty() ? "undecided" : err.c_str());
        return 0;
    }
    const pcr::ZoneEntranceDef& e = pcr::zoneEntrances()[(size_t)outer];
    if (e.warpOutStage == nullptr || e.warpOutSpawn < 0) {
        // update_all_boss_warp_out_destinations asserts warp_out_spawn_id is not None.
        logf("[cos] rando: warpout: %s: '%s' has no warp-out spawn\n", bossStage, e.name);
        return 0;
    }
    out->stage = e.warpOutStage;
    out->room = e.warpOutRoom;
    out->spawn = e.warpOutSpawn;
    return 1;
}

int pc_rando_warpout_patch_file(PcRandoResFile* f, const PcRandoSeed* seed) {
    if (f == nullptr || f->type != PCR_RES_DZR || f->room != 0 || strcmp(f->stage, "M2tower") != 0 ||
        strcmp(f->name, "room.dzr") != 0) {
        return 0;
    }
    PcRandoWarpOut w;
    if (!pc_rando_warpout_dest(seed, "M2tower", &w)) {
        sStatus.m2towerSkipped++;
        return 0;
    }
    PcRandoDzx dzx;
    const uint8_t* e = nullptr;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) == PCR_DZX_OK) {
        e = pc_rando_dzx_entry(&dzx, "SCLS", 1);
    }
    // D9: SCLS[1] is the door to Ganondorf's room; R3.15 (remove_forsaken_fortress_2_cutscenes) has
    // already pointed it at sea room 1 when it ran first.
    char dest[9] = {};
    if (e != nullptr) {
        memcpy(dest, e, 8);
    }
    if (e == nullptr || (strcmp(dest, "sea") != 0 && strcmp(dest, "M2ganon") != 0)) {
        logf("[cos] rando: warpout: M2tower/Room0 SCLS[1] %s; the Helmaroc King exit stays vanilla\n",
             e == nullptr ? "missing" : "is not the Ganondorf door (fingerprint)");
        sStatus.m2towerSkipped++;
        return -1;
    }
    // SCLS: char dest_stage_name[8], u8 spawn_id, u8 room_index (wwlib/dzx.py SCLS).
    uint8_t bytes[10] = {};
    strncpy((char*)bytes, w.stage, 8);
    bytes[8] = (uint8_t)w.spawn;
    bytes[9] = (uint8_t)w.room;
    if (pc_rando_dzx_set_field(&dzx, "SCLS", 1, 0, bytes, sizeof bytes, nullptr) != PCR_DZX_OK) {
        sStatus.m2towerSkipped++;
        return -1;
    }
    logf("[cos] rando: warpout: M2tower/Room0 SCLS[1] %s -> %s room %d spawn %d\n", dest, w.stage, w.room,
         w.spawn);
    sStatus.m2towerPatched++;
    return 1;
}

void pc_rando_warpout_status(PcRandoWarpoutStatus* out) {
    if (out != nullptr) {
        *out = sStatus;
    }
}

void pc_rando_warpout_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_dzx_patcher(patcher);
    pc_rando_add_listener(onActivation);
}

} // extern "C"
