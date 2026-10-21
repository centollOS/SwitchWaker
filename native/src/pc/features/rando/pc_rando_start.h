/*
 * pc_rando_start.h - health, magic, clothes and the start spawn of a randomized file
 * (docs/RANDOMIZER_PLAN.md step R3.2), the pure part. The game side is in
 * game_hooks/pc_rando_hooks_start.cpp and in small hunks in game/ (docs/RANDO_HOOKS.md "R3.2").
 *
 * wwrando references:
 *   - misc_rando_features.asm set_starting_health (dSv_player_status_a_c::init 0x800589A8): max HP is
 *     starting_quarter_hearts, current HP the same rounded down to a whole heart;
 *   - get_current/max_health_for_file_select_screen (dFile_select_c::makeRecInfo 0x80182504 /
 *     0x80182544): a blank slot shows the starting HP instead of the deleted save's;
 *   - fully_refill_magic_meter_and_cap_health_on_load_save (FileSelectMainNormal 0x80231B08);
 *   - gameover_continue_reset_life (dGameover_c::_execute 0x8018E7D0);
 *   - tweaks.py skip_wakeup_intro_and_start_at_dock (#1: new-game spawn 206 -> 0),
 *     start_ship_at_outset / change_ship_starting_island(44) (#2: the sea stage's Ship ACTR takes the
 *     position and y rotation of the island room's SHIP entry with ship_id 0) and, after it,
 *     prevent_fire_mountain_lava_softlock (the Ship ACTR's y becomes -500000).
 *
 * The Ship ACTR is patched in memory when sea/Stage.arc's stage.dzs is loaded (R0.10's dispatcher).
 * The island's SHIP entry is in sea/Room44.arc, which is not loaded then, so the game side reads that
 * room.dzr from the disc once (pc_rando_start_yaz0_expand, pc_rando_start_rarc_find) and keeps the
 * spawn; no disc bytes are in the source.
 *
 * No game headers, no game calls: this links into cos_rando_core (unit test test_R3_2).
 */
#ifndef PC_RANDO_START_H
#define PC_RANDO_START_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_dzx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tweaks.py skip_wakeup_intro_and_start_at_dock: the new game's spawn id (vanilla 206). */
#define PCR_START_SPAWN 0
/* start_ship_at_outset: the island room whose SHIP 0 entry the Ship ACTR copies (sea/Room44, Outset).
   R6.7 (starting island) changes the room. */
#define PCR_START_ROOM 44
/* prevent_fire_mountain_lava_softlock: the Ship ACTR's y on a new game. */
#define PCR_START_SHIP_Y (-500000.0f)
/* dGameover_c::_execute: the continue HP (3 hearts). */
#define PCR_GAMEOVER_LIFE 12

/* --- health and magic --- */
/* set_starting_health: the current HP of a new file (max rounded down to a whole heart). */
uint16_t pc_rando_start_life(uint16_t startingQuarterHearts);
/* get_current_health_for_file_select_screen, blank slot: the shown current HP. */
uint16_t pc_rando_start_blank_slot_life(uint16_t startingQuarterHearts);
/* fully_refill_..._cap_health_on_load_save: the current HP after loading a file (at most the max
   rounded down to a whole heart; never raised). */
uint16_t pc_rando_start_loaded_life(uint16_t maxLife, uint16_t life);
/* gameover_continue_reset_life: min(12, max rounded down to a whole heart). */
uint16_t pc_rando_start_gameover_life(uint16_t maxLife);

/* --- the Ship ACTR --- */
typedef struct PcRandoShipSpawn {
    float x, y, z;
    uint16_t yRot;
    uint8_t shipId;
} PcRandoShipSpawn;

/* The SHIP entry (0x10 bytes: f32 x y z, u16 y_rot, u8 ship_id, u8 unknown) of a room.dzr with this
   ship_id, the first one if several. Returns 1 and fills *out when found. */
int pc_rando_start_find_ship_spawn(const void* dzr, size_t size, int shipId, PcRandoShipSpawn* out);

/* The patch of sea/Stage stage.dzs: the ACTR named "Ship" takes the spawn's x, z and y rotation
   (change_ship_starting_island; spawn NULL leaves them) and then the y PCR_START_SHIP_Y
   (prevent_fire_mountain_lava_softlock). Returns PCR_DZX_OK, or the dzx error (PCR_DZX_NO_CHUNK when
   there is no "Ship" ACTR). *index (may be NULL) gets the ACTR index. */
int pc_rando_start_patch_sea_stage(PcRandoDzx* dzx, const PcRandoShipSpawn* spawn, uint32_t* index);

/* --- reading the island room from the disc (no allocation of its own: the caller passes buffers) --- */
/* The expanded size of a Yaz0 block (0 when src is not Yaz0). */
uint32_t pc_rando_start_yaz0_size(const void* src, size_t srcLen);
/* Expands a Yaz0 block into dst (cap >= pc_rando_start_yaz0_size). Returns 1 on success. */
int pc_rando_start_yaz0_expand(const void* src, size_t srcLen, void* dst, size_t cap);
/* A file of a RARC by its name (any directory, first match, e.g. "room.dzr"): returns 1 and sets
   *data / *size to its bytes inside the archive (as stored: may be Yaz0). */
int pc_rando_start_rarc_find(const void* arc, size_t arcLen, const char* name, const uint8_t** data,
                             uint32_t* size);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_START_H */
