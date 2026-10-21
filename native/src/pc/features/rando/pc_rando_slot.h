// Slot binding of the built-in randomizer (docs/RANDOMIZER_PLAN.md step R0.7, decisions D3, D4, D7).
//
// A randomized save slot is marked by a record in dSv_save_c::mReserve (0x50 bytes, saved, loaded and
// copied with the slot, read by no vanilla code):
//
//   +0x00  "WWR1"            magic
//   +0x04  u8 version        PC_RANDO_SLOT_RECORD_VERSION
//   +0x05  u8 flags          PC_RANDO_SLOT_FLAG_*
//   +0x06  u16 0
//   +0x08  u8 hash[16]       PcRandoSeed::hash of the placement (MD5 of its serialization)
//   +0x18  0 up to +0x50
//
// The placement itself is the sidecar <card dir>/switchwaker-rando-<hash32>.json (D3), named by the
// first 4 hash bytes; on load the full 16-byte hash must match the record.
//
// This file holds the pure part (cos_rando_core: no game code): the record codec, the pending seed
// the next New Game consumes (set by the menu, R4.3/R4.4), the active seed this module owns while
// pc_rando() is 1, the seed -> PcRandoConfig/PcRandoModes mapping, and sidecar lookup by directory.
// The game side (mReserve, the card directory, the hooks in d_s_name.cpp, d_s_title.cpp and
// pcBootStage) is native/src/pc/game_hooks/pc_rando_hooks_slot.cpp. Game thread only.
#pragma once

#include "pc/pc_rando.h"
#include "pc/pc_rando_modes.h"
#include "pc_rando_seedfile.h"

#include <cstdint>
#include <string>

#define PC_RANDO_SLOT_RECORD_SIZE 0x50
#define PC_RANDO_SLOT_RECORD_VERSION 1
// The record was written by a debug boot (COS_RANDO_PLACEMENT), not by a New Game.
#define PC_RANDO_SLOT_FLAG_DEBUG_BOOT 0x01

// 1 when the first 4 bytes are the magic "WWR1" (any version).
bool pc_rando_slot_record_is_rando(const uint8_t* reserve);
// Writes the record into reserve[0..0x50): magic, version, flags, hash, zero padding.
void pc_rando_slot_record_write(uint8_t* reserve, const uint8_t hash[16], uint8_t flags);
// Reads a record: false when the magic or the version is wrong. hash/flags may be NULL.
bool pc_rando_slot_record_read(const uint8_t* reserve, uint8_t hash[16], uint8_t* flags);
// D4: clears reserve[0..0x50) only when it holds a rando record; returns 1 when it did.
bool pc_rando_slot_record_clear_stale(uint8_t* reserve);

// "switchwaker-rando-<8 hex>.json" for a hash (pc_rando_seed_sidecar_name for a seed).
std::string pc_rando_slot_sidecar_name(const uint8_t hash[16]);

// The seed-time config of a seed's options (wwrando tweaks.py: update_sword_mode_game_variable,
// update_starting_gear with options.starting_gear + extra_starting_items, set_starting_health,
// set_num_starting_triforce_shards, should_start_with_heros_clothes, enable_hero_mode, the D14 tweak
// options, option_targeting_mode, skip_rematch_bosses) and the R6.15 modes (remove_music,
// invert_sea_compass_x_axis). Values missing from the options keep the defaults. The
// prologue pigs bitfield comes from the extra key "captured_pigs_bitfield" (R1.6; default 0x04).
// Returns false with *err when a starting item name is unknown or the gear is too long.
bool pc_rando_slot_config_from_seed(const PcRandoSeed& seed, PcRandoConfig* cfg, PcRandoModes* modes,
                                    std::string* err);

// The pending seed (D6): the next New Game consumes it. Set copies it.
void pc_rando_slot_set_pending(const PcRandoSeed& seed);
void pc_rando_slot_clear_pending();
const PcRandoSeed* pc_rando_slot_pending(); // NULL when none

// Activates `seed`: copies it into the module (the placement pc_rando_loc_item reads stays alive
// while active), sets the modes and the config from its options, then pc_rando_set_active. Returns
// false (nothing changed) when its options do not map.
bool pc_rando_slot_activate(const PcRandoSeed& seed, std::string* err);
// Activates the seed a record names from <dir>/<sidecar name>: false with *err when the file is
// missing, does not parse, or its hash differs from the record's.
bool pc_rando_slot_activate_from_dir(const std::string& dir, const uint8_t hash[16], std::string* err);
// The seed the module activated last (still kept after pc_rando_deactivate; check pc_rando()).
const PcRandoSeed* pc_rando_slot_active_seed();

// Writes the sidecar of `seed` into `dir` unless a file of that name already holds the same text.
bool pc_rando_slot_write_sidecar(const std::string& dir, const PcRandoSeed& seed, std::string* err);
