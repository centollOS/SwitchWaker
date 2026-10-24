// pc_rando_gen_palettes.h - the enemy palette randomizer's generator side (docs/RANDOMIZER_PLAN.md step
// R7.2a): wwrando 1.10.0_9775811's PaletteRandomizer._randomize and its data,
// data/palette_randomizable_files.txt (generated/rando_palettes.inc, from
// native/tools/rando/gen_palette_data.py).
//
// _randomize draws, per group in file order, h_shift = randint(20, 340) then v_shift =
// randint(-40, 40), on the randomizer's own get_new_rng() (pc_rando_gen.cpp runs it after enemies and
// before items). The shifts go into the placement as the top-level key "palettes":
//
//   "palettes": {"Bokoblin": [h, v], "ChuChu": [h, v], ...}   (37 groups, file order)
//
// which ww_export.py writes from file_group_name_to_hv_shift and PcRandoSeed::extra keeps (and
// hashes); the key is absent when randomize_enemy_palettes is off. The game side (textures R7.2c,
// materials R7.2d, particles and code colours R7.2e) reads them back with palettesFromJson.
// PaletteRandomizer writes nothing to the logs.
//
// Pure C++ (no game headers): cos_rando_core, the unit tests, the Mac CLI and the game link it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pc_rando_json.h"
#include "pc_rando_rng.h"

namespace pcr {

// One RARC of a group: /res/Object/<name>.arc, and the files of it that are never recoloured.
struct PaletteRarc {
    const char* name;
    const char* const* excluded;
    int numExcluded;
};

// One entry of palette_randomizable_files.txt.
struct PaletteGroup {
    const char* name;
    const uint16_t* particleIds; // in the file's order
    int numParticleIds;
    const PaletteRarc* rarcs;
    int numRarcs;
};

// The 37 groups, in file order.
int paletteGroupCount();
const PaletteGroup& paletteGroup(int i);
// The index of the group named `name`, or -1.
int paletteGroupIndex(const char* name);

// PaletteRandomizer.progress_randomize_text and progress_randomize_duration_weight.
constexpr const char* kPaletteProgressText = "Randomizing enemy colors...";
constexpr int kWeightPalettes = 0;

struct PaletteShift {
    int h = 0; // h_shift, 20..340
    int v = 0; // v_shift, -40..40
    bool operator==(const PaletteShift& o) const { return h == o.h && v == o.v; }
};

// PaletteRandomizer._randomize: one shift per group (paletteGroupCount(), file order).
void randomizePalettes(pc::rando::PyRandom& rng, std::vector<PaletteShift>& out);

// The placement's "palettes" value: {group name: [h, v]} in group order.
Json palettesToJson(const std::vector<PaletteShift>& shifts);

// Back from that value: out gets one shift per group, in group order. False (with *err) when a group
// is missing, a name is unknown or a value is not two integers in range.
bool palettesFromJson(const Json& j, std::vector<PaletteShift>& out, std::string* err);

} // namespace pcr
