// Hint placement at runtime, pure part (docs/RANDOMIZER_PLAN.md step R5.3): what wwrando 1.10.0_9775811's
// HintsRandomizer._save (randomizers/hints.py) changes besides the message texts of R5.2
// (pc_rando_hint_text.h):
//   - rotate_hoho_to_face_hint: each "Ah" actor of the Ho Ho rooms (sea rooms 34, 14, 44, 1, 5, 33,
//     3, 43, 31, 46; HOHO_INDEX_TO_ISLAND_NUM) gets the y_rot that faces the sea stage MULT entry of
//     the island his hints name first (HintsResult::hohoLookIsland, R5.1). The 49 MULT positions are
//     an embedded table (the sea stage.dzs's, read from the USA disc by
//     native/check/rando/fixtures/R5_3_make_fixture.py's checks; a 100000-unit grid);
//   - the seed options the texts depend on (cryptic_hints, hint_importance, keylunacy).
// The game side (game_hooks/pc_rando_hooks_hints.cpp) runs the room patcher at archive load and
// puts the texts in the overlay at activation; the d_a_bigelf hunk (flexible_hint_locations.asm)
// asks pc_rando_hints_flexible().
//
// Pure: no game headers and no game calls (cos_rando_core and the unit tests link it).
#pragma once

#include "pc_rando_dzx.h"
#include "pc_rando_gen_hints.h"
#include "pc_rando_json.h"

#include <stdint.h>

namespace pcr {

// The sea stage's MULT entry of island 1..49: x_pos, z_pos. False for another number.
bool seaIslandMultPos(int island, float* x, float* z);

// rotate_hoho_to_face_hint's angle: int(atan2(dx, dz) * (0x8000 / pi)) % 0x10000, from an actor at
// (x, z) towards the MULT entry of `island` (Python's int truncates towards zero, % is positive).
// False for a bad island.
bool hohoYRot(float x, float z, int island, uint16_t* yRot);

// The Ho Ho index of sea room `room` (kHohoIndexToIsland), or -1.
int hohoIndexOfRoom(int room);

struct HohoPatchResult {
    int found = 0;    // "Ah" actors in the room's ACTR chunks (all layers)
    int turned = 0;   // of them, given a new y_rot
    int mismatch = 0; // "Ah" actors whose which_hoho (params & 0xFF) is not the room's Ho Ho (left alone)
    uint16_t yRot = 0; // the y_rot written (when turned > 0)
};

// rotate_hoho_to_face_hint on one sea room's room.dzr: when `room` is a Ho Ho room and his look
// island (h.hohoLookIsland) is set, every "Ah" actor of ACTR and ACT0..ACTb whose which_hoho is
// that Ho Ho gets the y_rot facing that island (wwrando asserts which_hoho; the port logs and skips
// an actor that differs, plan D9). Returns false when the room is not a Ho Ho room, the hints have no
// Ho Ho hints, or he keeps his vanilla rotation (nothing written).
bool patchHohoRoom(PcRandoDzx* dzx, int room, const HintsResult& h, HohoPatchResult* out);

// The text options of a seed (its "options" object; wwrando's defaults when a key is missing).
struct HintSeedOptions {
    bool cryptic = true;     // cryptic_hints
    bool importance = false; // hint_importance
    bool keylunacy = false;  // keylunacy
};
HintSeedOptions hintSeedOptions(const Json& options);

} // namespace pcr
