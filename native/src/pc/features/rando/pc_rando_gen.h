// pc_rando_gen.h - the seed generator's entry point (docs/RANDOMIZER_PLAN.md step R1.6): a port of
// wwrando 1.10.0_9775811's WWRandomizer.__init__ + randomize() in dry mode, which runs the
// randomizers in randomizer.py's order and returns the placement (R0.6's PcRandoSeed, schema
// "format 1") plus the logs of decision D5.
//
//   charts, boss_reqs, entrances, starting_island, pigs, extra_start_items, enemies, palettes,
//   items, hints
//
// Each randomizer that is enabled gets a fresh RNG from get_new_rng() (BaseRandomizer.reset_rng),
// so a disabled one draws nothing and changes nothing. At wwrando's default options the enabled
// ones are pigs (one choice, the prologue pig colour), items and hints.
//
// What is ported: pigs, extra starting items (R6.4) and items (R1.4) completely; the spoiler,
// non-spoiler and error logs (R1.5); charts (R6.6, pc_rando_gen_charts.h);
// starting_island (R6.7: one choice of sea room 2-49, written as "starting_island_room");
// required bosses (R6.8, pc_rando_gen_bosses.h).
// What is not ported yet, refused with a "not supported yet" error (GenResult::unsupported) instead
// of a seed that would differ from wwrando's:
//   - any randomize_*_entrances (R6.10-R6.11),
//     randomize_enemies (R7.3), randomize_enemy_palettes (R7.2);
// Hints (R5.1, pc_rando_gen_hints.h) always run, as wwrando's HintsRandomizer is always enabled (the
// Big Octo Great Fairy hint is generated even with every hint placement off); the placement gets a
// "hints" value (hintsToJson). With GenRequest::allowMissingHints the seed is made without them:
// hints run after the items with their own RNG, so the placement is still wwrando's; the hint texts
// stay vanilla in game and the spoiler log's Hints section lists only the two Savage Labyrinth rows
// (GenResult::hintsMissing).
//
// The pigs bitfield is not in the oracle's schema; it is written as the extra top-level key
// "captured_pigs_bitfield" (an integer: 1, 2 or 4), which R0.6 keeps and pc_rando_slot reads.
//
// Pure C++ (no game headers, no file I/O): cos_rando_core, the unit tests, the Mac CLI
// (native/check/rando/cli.cpp, target cos_rando_cli) and the menu (R4.3) link it.
// COS_RANDO_TRACE (pc_rando_gen_items.h) logs every draw of every randomizer in ww_trace.py's format.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "pc_rando_options.h"
#include "pc_rando_seedfile.h"

namespace pcr {

// Progress: text (wwrando's progress_randomize_text), done, total (wwrando's duration weights of
// the enabled randomizers, dry run). Called from the generating thread.
typedef void (*GenProgressFn)(void* user, const char* text, int done, int total);

struct GenRequest {
    std::string seed; // as typed; sanitized by the generator (WWRandomizer.sanitize_seed)
    Options options;  // validated by the generator (Options.validate), not ensureValidCombination
    // Generate without hints (faster; the placement is the same, the hint texts stay vanilla).
    bool allowMissingHints = false;
    // SEED_KEY for do_not_generate_spoiler_log (decision D2).
    const char* seedKey = kSwitchWakerSeedKey;
    // SwitchWaker version: the placement's "version" ("SwitchWaker <version>") and the D5 trailer.
    std::string version = "dev";
    GenProgressFn progress = nullptr;
    void* progressUser = nullptr;
};

struct GenResult {
    bool ok = false;
    // A refusal for an option whose randomizer is not ported yet (error says which and the step).
    bool unsupported = false;
    // The error text: the exception wwrando raises (e.g. TooFewProgressionLocationsError's text),
    // or the refusal.
    std::string error;

    std::string sanitizedSeed;
    std::string permalink;
    std::string seedHash;  // "Name1 Name2"
    int pigsBitfield = 0x04; // PigsRandomizer.captured_pigs_bitfield
    int drcFailsafeLocation = -1;
    bool hintsMissing = false; // made with allowMissingHints

    // On success: the placement (hash set) and its JSON text (pc_rando_seed_serialize).
    PcRandoSeed seed;
    std::string placementJson;

    // The logs (D5) without the trailer line; logTrailer(version) is appended by whoever writes
    // them. spoilerLog is empty with do_not_generate_spoiler_log; errorLog only on failure (once the
    // permalink is known, as wwrando's UI writes it).
    std::string spoilerLog;
    std::string nonSpoilerLog;
    std::string errorLog;
};

// The first option of `o` whose randomizer is not ported yet, as the refusal text, or "" when all
// enabled randomizers are ported. allowMissingHints as in GenRequest.
std::string unsupportedOptionsError(const Options& o, bool allowMissingHints);

// Every option as the placement's "options" object (Options.all() order, enums as their strings,
// lists as arrays, custom_colors as its JSON), as ww_export.py writes it.
Json optionsToJson(const Options& o);

// Runs the generator. Returns out.ok.
bool generate(const GenRequest& req, GenResult& out);

} // namespace pcr

// The plan's name for the entry point (same as pcr::generate).
inline bool pc_rando_generate(const pcr::GenRequest& req, pcr::GenResult& out) { return pcr::generate(req, out); }
