// Health, magic, clothes and the start spawn, game side (docs/RANDOMIZER_PLAN.md step R3.2; pure part in
// features/rando/pc_rando_start.h, hunks listed in docs/RANDO_HOOKS.md "R3.2").
//
// - The HP helpers of the hunks in d_save.cpp, d_gameover.cpp and d_file_select.cpp, and the load
//   hook of d_s_name.cpp (magic refill, HP cap).
// - The sea/Stage stage.dzs patcher (registered through pc_rando_patchers.def): the "Ship" ACTR takes
//   the position and y rotation of sea/Room44's SHIP entry with ship_id 0 (tweaks.py
//   start_ship_at_outset -> change_ship_starting_island(44)) and then y = -500000
//   (prevent_fire_mountain_lava_softlock). sea/Room44.arc is not loaded when the stage is, so its
//   room.dzr is read from the disc once (DVDOpen, host memory) and the spawn is kept for the session.
//   If that read fails, the position stays vanilla (logged) and only the y edit is made.
// - R6.7 (starting island): the island is the active seed's starting_island_room (pc_rando_start_room,
//   44 without the option), for both the new game's return place (the d_save.cpp hunk) and the Ship
//   ACTR (starting_island.py _save -> change_ship_starting_island(room)). The spawn is kept per room,
//   so a later seed with another island reads its room again.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_start.h"
#include "pc/pc_rando_modes.h"

#include "d/d_com_inf_game.h"

#include "cos_sdk/host_alloc.h"

#include <dolphin/dvd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

void logLine(const char* line) {
    (void)!write(STDERR_FILENO, line, strlen(line));
}

// The island spawn of sSpawnRoom: 0 not read yet, 1 read, -1 the read failed (not tried again for
// that room).
int sSpawnState = 0;
int sSpawnRoom = 0;
PcRandoShipSpawn sSpawn;
int sPatched = 0;

bool readIslandSpawn(int room, PcRandoShipSpawn* out) {
    char path[48];
    snprintf(path, sizeof path, "/res/Stage/sea/Room%d.arc", room);
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        return false;
    }
    cos_sdk::HostVector<uint8_t> file(info.length);
    const s32 got = DVDReadPrio(&info, file.data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    if (got != (s32)info.length) {
        return false;
    }
    cos_sdk::HostVector<uint8_t> arc;
    if (const uint32_t n = pc_rando_start_yaz0_size(file.data(), file.size()); n != 0) {
        arc.resize(n);
        if (!pc_rando_start_yaz0_expand(file.data(), file.size(), arc.data(), arc.size())) {
            return false;
        }
    } else {
        arc.swap(file);
    }
    const uint8_t* dzr = nullptr;
    uint32_t size = 0;
    if (!pc_rando_start_rarc_find(arc.data(), arc.size(), "room.dzr", &dzr, &size)) {
        return false;
    }
    cos_sdk::HostVector<uint8_t> plain;
    if (const uint32_t n = pc_rando_start_yaz0_size(dzr, size); n != 0) {
        plain.resize(n);
        if (!pc_rando_start_yaz0_expand(dzr, size, plain.data(), plain.size())) {
            return false;
        }
        dzr = plain.data();
        size = n;
    }
    return pc_rando_start_find_ship_spawn(dzr, size, 0, out) != 0;
}

const PcRandoShipSpawn* islandSpawn() {
    const int room = pc_rando_start_room();
    if (room != sSpawnRoom) {
        sSpawnRoom = room;
        sSpawnState = 0;
    }
    if (sSpawnState == 0) {
        sSpawnState = readIslandSpawn(room, &sSpawn) ? 1 : -1;
        char buf[200];
        if (sSpawnState == 1) {
            snprintf(buf, sizeof buf, "[cos] rando: start: sea/Room%d SHIP 0 at (%.1f, %.1f, %.1f) y_rot 0x%04X\n",
                     room, sSpawn.x, sSpawn.y, sSpawn.z, sSpawn.yRot);
        } else {
            snprintf(buf, sizeof buf,
                     "[cos] rando: start: sea/Room%d room.dzr has no SHIP 0 or cannot be read (the Ship keeps its "
                     "vanilla position)\n",
                     room);
        }
        logLine(buf);
    }
    return sSpawnState == 1 ? &sSpawn : nullptr;
}

void patchSeaStage(PcRandoResFile* f) {
    if (f == nullptr || strcmp(f->stage, "sea") != 0 || strcmp(f->arc, "Stage") != 0 ||
        strcmp(f->name, "stage.dzs") != 0) {
        return;
    }
    PcRandoDzx dzx;
    int r = pc_rando_dzx_open(&dzx, f->data, f->size);
    uint32_t index = 0;
    const PcRandoShipSpawn* spawn = islandSpawn();
    if (r == PCR_DZX_OK) {
        r = pc_rando_start_patch_sea_stage(&dzx, spawn, &index);
    }
    if (r == PCR_DZX_OK) {
        sPatched++;
    }
    char buf[200];
    if (spawn != nullptr) {
        snprintf(buf, sizeof buf, "[cos] rando: start: sea/Stage stage.dzs ACTR[%u] Ship: x/z/y_rot of sea/Room%d SHIP 0, y %.0f: %s\n",
                 index, sSpawnRoom, (double)PCR_START_SHIP_Y, pc_rando_dzx_result_name(r));
    } else {
        snprintf(buf, sizeof buf, "[cos] rando: start: sea/Stage stage.dzs ACTR[%u] Ship: vanilla x/z, y %.0f: %s\n", index,
                 (double)PCR_START_SHIP_Y, pc_rando_dzx_result_name(r));
    }
    logLine(buf);
}

} // namespace

extern "C" {

int pc_rando_start_room(void) {
    // StartingIslandRandomizer.room_number; the seed file loader already refuses rooms outside 1-49.
    const PcRandoSeed* seed = pc_rando() ? pc_rando_seed() : nullptr;
    if (seed == nullptr || seed->starting_island_room < 1 || seed->starting_island_room > 49) {
        return PCR_START_ROOM;
    }
    return seed->starting_island_room;
}

uint16_t pc_rando_start_new_life(void) {
    return pc_rando_start_life(pc_rando_cfg()->starting_quarter_hearts);
}

// pc_rando_start_gameover_life (pc_rando_hooks.h) is the pure function of features/rando/pc_rando_start.cpp.

uint16_t pc_rando_start_file_select_life(char firstNameChar, uint16_t saved, int isMax) {
    if (firstNameChar != 0) {
        return saved;
    }
    uint16_t quarters = 0;
    if (pc_rando()) {
        quarters = pc_rando_cfg()->starting_quarter_hearts;
    } else if (const PcRandoSeed* pending = pc_rando_slot_pending(); pending != nullptr) {
        PcRandoConfig cfg;
        PcRandoModes modes;
        pc_rando_config_defaults(&cfg);
        std::string err;
        if (!pc_rando_slot_config_from_seed(*pending, &cfg, &modes, &err)) {
            return saved;
        }
        quarters = cfg.starting_quarter_hearts;
    } else {
        return saved;
    }
    return isMax ? quarters : pc_rando_start_blank_slot_life(quarters);
}

void pc_rando_start_on_load(void) {
    if (!pc_rando()) {
        return;
    }
    dComIfGs_setMagic(dComIfGs_getMaxMagic());
    dComIfGs_setLife(pc_rando_start_loaded_life(dComIfGs_getMaxLife(), dComIfGs_getLife()));
}

void pc_rando_start_init(void) {
    pc_rando_register_dzx_patcher(patchSeaStage);
}

int pc_rando_start_ship_spawn(float* x, float* z, uint16_t* yRot, int* patched) {
    if (patched != nullptr) {
        *patched = sPatched;
    }
    const PcRandoShipSpawn* s = islandSpawn();
    if (s == nullptr) {
        return 0;
    }
    if (x != nullptr) *x = s->x;
    if (z != nullptr) *z = s->z;
    if (yRot != nullptr) *yRot = s->yRot;
    return 1;
}

} // extern "C"
