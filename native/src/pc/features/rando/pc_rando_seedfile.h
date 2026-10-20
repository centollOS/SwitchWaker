// The randomizer's placement file (docs/RANDOMIZER_PLAN.md step R0.6, decisions D1-D3).
//
// A placement is the JSON schema "format 1" of the oracle (native/tools/rando/README.md):
//
//   {format, seed, permalink, options, extra_starting_items, charts, required_boss_locations,
//    entrances, starting_island_room, locations{location: item}}
//
// plus, written by our generator only, "hints" (any JSON; R5 defines its contents) and "version"
// (the SwitchWaker version that generated it). wwrando ignores both; a file from ww_export.py has
// neither. Other top-level keys are kept and written back after these, in their order.
//
// pc_rando_seed_parse reads one into a PcRandoSeed; pc_rando_seed_serialize writes it exactly as the
// oracle does (Python json.dumps(indent=1) plus a newline), so a ww_export.py file loads, saves and
// loads again byte-identical. The 16-byte content hash is the MD5 of that serialization: it names the
// sidecar file next to the save (D3: <card dir>/switchwaker-rando-<hash32>.json) and is stored in the
// slot's reserve record (R0.7), which checks it against the loaded file.
//
// Locations are indexed by their position in "locations", which is wwrando's item_locations.txt order
// (the PCR_LOC_* order of R0.5); a placement must hold exactly PC_RANDO_SEED_LOCATIONS of them. Item
// names map to item ids with wwrando's table: data/item_names.txt plus the names tweaks.py registers
// (Progressive Sword 0x38, DRC Small Key 0x13, Rainbow Rupee 0xB8, ...), so both the logic names of a
// generated seed and the original names of the `identity` test placement resolve.
//
// Pure C++ (no game code): part of cos_rando_core, tested by native/check/rando/test_R0_6.cpp.
#pragma once

#include "pc_rando_json.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Locations in wwrando 1.10.0_9775811's logic/item_locations.txt.
#define PC_RANDO_SEED_LOCATIONS 320
// The placement schema this code reads and writes.
#define PC_RANDO_SEED_FORMAT 1
// The item id stored for a location whose item is unknown (never in a parsed seed).
#define PC_RANDO_NO_ITEM 0xFF

struct PcRandoSeed {
    int format = PC_RANDO_SEED_FORMAT;
    std::string seed;      // wwrando's sanitized seed name
    std::string permalink; // wwrando's base64 permalink
    // Every wwrando option by name (enums as their strings), in file order. Typed access comes with
    // the options step (R1.x); until then use options.find("keylunacy")->asBool() and so on.
    pcr::Json options = pcr::Json::object();

    std::vector<std::string> extra_starting_items; // item names, in order
    std::vector<uint8_t> extra_starting_item_ids;  // parallel item ids
    // Island number (1-49) -> the chart item name found by salvaging there, in file order.
    std::vector<std::pair<int, std::string>> charts;
    std::vector<std::string> required_boss_locations; // location names
    // Entrance name -> zone name, in file order (wwrando writes the vanilla connections when
    // entrances are not randomized).
    std::vector<std::pair<std::string, std::string>> entrances;
    int starting_island_room = 44; // sea room of the starting island (44: Outset)

    // Locations in file order: names, item names, and the item ids pc_rando_set_loc_items reads.
    std::vector<std::string> location_names;
    std::vector<std::string> location_item_names;
    uint8_t locations[PC_RANDO_SEED_LOCATIONS];

    bool has_hints = false;
    pcr::Json hints; // our generator's hints (R5); null when absent
    bool has_version = false;
    std::string version; // "SwitchWaker <version>" of the generator; "" when absent

    pcr::Json extra = pcr::Json::object(); // unknown top-level keys, kept in order

    // MD5 of pc_rando_seed_serialize(*this), set by parse and by pc_rando_seed_update_hash.
    uint8_t hash[16];

    PcRandoSeed();
};

// Parses a placement file. On failure returns false with *err (when not NULL) naming the problem
// (bad JSON, wrong format, missing or mistyped field, unknown item name, wrong location count);
// *out is then reset. On success out->hash is set.
bool pc_rando_seed_parse(std::string_view text, PcRandoSeed* out, std::string* err);

// The placement as JSON text: Python's json.dumps(indent=1) of the fields above in schema order
// (hints, version and the unknown keys last, each only when present), plus a newline.
std::string pc_rando_seed_serialize(const PcRandoSeed& seed);

// Recomputes seed->hash after fields were changed (the generator calls it when it is done).
void pc_rando_seed_update_hash(PcRandoSeed* seed);
// The first 4 hash bytes as a big-endian u32 (the sidecar name), and the hash as 32 lowercase hex.
uint32_t pc_rando_seed_hash32(const PcRandoSeed& seed);
std::string pc_rando_seed_hash_hex(const PcRandoSeed& seed);
// "switchwaker-rando-<8 hex>.json" (D3), without a directory.
std::string pc_rando_seed_sidecar_name(const PcRandoSeed& seed);

// File helpers. Load reads and parses `path`. Save writes the serialization to `path` through a
// temporary file in the same directory ("<path>.tmp") renamed over it, so a crash never leaves a
// half-written sidecar. Both return false with *err on failure.
bool pc_rando_seed_load_file(const char* path, PcRandoSeed* out, std::string* err);
bool pc_rando_seed_save_file(const char* path, const PcRandoSeed& seed, std::string* err);

// wwrando's item table: the id of an item name (original or registered name), or -1.
int pc_rando_item_id(std::string_view name);
// The display name of an item id (the registered name for a renamed id, as wwrando's item_names);
// NULL for an id with no name.
const char* pc_rando_item_name(uint8_t id);
