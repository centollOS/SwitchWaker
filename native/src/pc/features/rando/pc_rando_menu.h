// The seed rows of the options menu's "Randomizer" tab (docs/RANDOMIZER_PLAN.md step R4.3,
// decisions D3, D5, D6), after the option rows of R4.1 (pc_rando_settings.h, RandoMenuAction):
//
//   Seed...          the seed name (setting COS_RANDO_SEED, saved in settings.ini; empty: a random
//                    one at Generate); A opens the text input of R4.2 (pc_rando_text_ui.h)
//   Random seed      A puts a new random name in COS_RANDO_SEED (seedgen.make_random_seed_name)
//   Paste permalink  A opens the text input for a wwrando permalink, filled with
//                    <randoUserDir()>/permalink.txt when that file holds one; OK sets the seed and
//                    every option from it (randoApplyPermalink)
//   Generate         A generates the seed of the current settings (randoSettingsOptions, validated)
//                    on a worker thread (pc_rando_genjob.h); the row shows the progress, then the
//                    pending seed's hash ("Name1 Name2") or the error
//
// A generated seed is the pending seed (pc_rando_slot_set_pending) the next New Game consumes
// (R4.4). Its sidecar is written at generation into the card folder (pc_rando_slot_card_dir), its
// logs into randoUserDir() (<userPath>/rando; COS_RANDO_DIR overrides it): "WW Random <seed> -
// Spoiler Log.txt" and "- Non-Spoiler Log.txt", or "- Error Log.txt" on a failure.
//
// Seeds are generated with their hints (R5.1), which R5.3 places in the game at activation.
//
// Game thread only. Compiled into cos_rando_core too (the features/rando glob), but no unit test may
// call into it: it needs pc_settings.cpp and the game side (the pure job is pc_rando_genjob.cpp).
#pragma once

#include <string>

namespace pc {

extern const char* const kRandoSeedKey; // "COS_RANDO_SEED"

// Registers COS_RANDO_SEED and the four rows (and R4.4's "Clean unused seed files",
// pc_rando_fileselect.h); called by pc_menu.cpp's menuInit after randoSettingsRegister. Again is a
// no-op.
void randoMenuInit();
// Every frame (pc_menu.cpp menuFrame, menu open or not): a finished generation becomes the pending
// seed (or the row's error) and is logged; R4.4's file-select line is drawn (randoFileSelectOverlay).
void randoMenuFrame();

// The Generate row's action: starts the generation. False (and *msg why) when it cannot start: one
// is running, the region (D16), or the thread. *msg gets the toast text either way.
bool randoGenerate(bool spanish, std::string* msg);
// A generation runs (started and not yet taken by randoMenuFrame).
bool randoGenBusy();

// The outcome of the last generation (randoMenuFrame), for the rows and the smoke.
struct RandoGenLast {
    bool have = false; // a generation ended since the start
    bool ok = false;
    std::string seed;   // sanitized seed name
    std::string hash;   // "Name1 Name2"
    std::string hash32; // the sidecar's 8 hex digits
    std::string error;  // first line of the error (failure) or of the first file not written
    std::string spoilerPath, nonSpoilerPath, errorPath, sidecarPath;
    double seconds = 0.0;
    unsigned int startFrame = 0, endFrame = 0; // pc_frame_count() at start and when taken
};
const RandoGenLast& randoGenLast();

// The permalink row's OK: decodes a wwrando permalink and stores its seed and options. False with
// *msg (the reason) when it does not decode, or when a setting is locked by the environment (the
// others are still stored).
bool randoApplyPermalink(const char* permalink, bool spanish, std::string* msg);

} // namespace pc
