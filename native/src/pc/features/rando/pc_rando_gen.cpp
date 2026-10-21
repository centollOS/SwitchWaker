// pc_rando_gen.cpp - the seed generator's entry point (docs/RANDOMIZER_PLAN.md step R1.6); see
// pc_rando_gen.h. The steps follow WWRandomizer.__init__ and WWRandomizer.randomize (randomizer.py)
// in a dry run.
#include "pc_rando_gen.h"

#include <string.h>

#include "pc_rando_gen_charts.h"
#include "pc_rando_gen_extra.h"
#include "pc_rando_gen_hints.h"
#include "pc_rando_gen_items.h"
#include "pc_rando_json.h"
#include "pc_rando_logic.h"
#include "pc_rando_rng.h"
#include "pc_rando_spoiler.h"

namespace pcr {

namespace {

using pc::rando::PyRandom;

// randomizers/*.py progress_randomize_duration_weight of the randomizers that can run here.
constexpr int kWeightItems = 400;
constexpr int kWeightHints = 500;

struct Progress {
    const GenRequest& req;
    int done = 0;
    int total = 0;
    void report(const char* text) const {
        if (req.progress != nullptr) {
            req.progress(req.progressUser, text, done, total);
        }
    }
};

std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    if (s.empty()) {
        return out;
    }
    size_t b = 0;
    for (;;) {
        const size_t e = s.find('|', b);
        out.push_back(s.substr(b, e == std::string::npos ? std::string::npos : e - b));
        if (e == std::string::npos) {
            return out;
        }
        b = e + 1;
    }
}

// PigsRandomizer._randomize: choice([0x01 pink, 0x02 speckled, 0x04 black]).
int randomizePigs(PyRandom& rng) {
    static const std::vector<int> kPigs = {0x01, 0x02, 0x04};
    return rng.choice(kPigs);
}

} // namespace

std::string unsupportedOptionsError(const Options& o, bool allowMissingHints) {
    auto refuse = [](const char* what, const char* step) {
        return std::string("Not supported yet: ") + what + " (plan step " + step +
               "). Turn the option off to generate a seed.";
    };
    // In randomizer.py's order of the randomizers.
    if (o.required_bosses) {
        return refuse("required_bosses", "R6.8");
    }
    if (o.randomize_dungeon_entrances || o.randomize_secret_cave_entrances || o.randomize_miniboss_entrances ||
        o.randomize_boss_entrances || o.randomize_secret_cave_inner_entrances ||
        o.randomize_fairy_fountain_entrances) {
        return refuse("entrance randomization", "R6.10-R6.11");
    }
    if (o.randomize_starting_island) {
        return refuse("randomize_starting_island", "R6.7");
    }
    if (o.randomize_enemies) {
        return refuse("randomize_enemies", "R7.3");
    }
    if (o.randomize_enemy_palettes) {
        return refuse("randomize_enemy_palettes", "R7.2");
    }
    (void)allowMissingHints; // R5.1: hints are generated (allowMissingHints only skips them)
    return std::string();
}

Json optionsToJson(const Options& o) {
    Json out = Json::object();
    for (int id = 0; id < OPT_COUNT; id++) {
        const OptionInfo& info = kOptionInfo[id];
        switch (info.kind) {
        case OptKind::Bool:
            out.set(info.name, Json::boolean(optionGetInt(o, id) != 0));
            break;
        case OptKind::Int:
            out.set(info.name, Json::integer(optionGetInt(o, id)));
            break;
        case OptKind::Enum: {
            const int idx = optionGetInt(o, id);
            out.set(info.name, Json::string(idx >= 0 && idx < info.numChoices ? info.choices[idx] : ""));
            break;
        }
        case OptKind::Str:
            out.set(info.name, Json::string(optionToString(o, id)));
            break;
        case OptKind::List: {
            Json arr = Json::array();
            for (const std::string& s : splitList(optionToString(o, id))) {
                arr.push(Json::string(s));
            }
            out.set(info.name, arr);
            break;
        }
        case OptKind::Dict: {
            Json d;
            if (!jsonParse(optionToString(o, id), &d, nullptr) || !d.isObject()) {
                d = Json::object();
            }
            out.set(info.name, d);
            break;
        }
        }
    }
    return out;
}

bool generate(const GenRequest& req, GenResult& out) {
    out = GenResult();
    Options o = req.options;
    validateOptions(o); // WWRandomizer.__init__: options.validate()

    out.sanitizedSeed = sanitizeSeed(req.seed);
    if (out.sanitizedSeed.empty()) {
        out.error = "The seed is empty after removing the characters wwrando does not allow.";
        return false;
    }
    out.permalink = encodePermalink(out.sanitizedSeed, o);
    out.seedHash = seedHash(out.permalink, o, req.seedKey);

    SpoilerInput in;
    in.seed = out.sanitizedSeed;
    in.permalink = out.permalink;
    in.seedHash = out.seedHash;

    auto fail = [&](const std::string& msg) {
        out.ok = false;
        if (out.error.empty()) {
            out.error = msg;
        }
        // wwrando's UI: write_error_log("Error on seed <seed>:\n<exception>\n\n<traceback>"); the port
        // has no traceback.
        out.errorLog = errorLog(o, in, "Error on seed " + out.sanitizedSeed + ":\n" + out.error + "\n");
        traceEnd();
        return false;
    };

    const std::string refusal = unsupportedOptionsError(o, req.allowMissingHints);
    if (!refusal.empty()) {
        out.unsupported = true;
        return fail(refusal);
    }
    out.hintsMissing = req.allowMissingHints; // R5.1: hints are skipped only when asked

    Progress prog{req};
    prog.total = kWeightItems + kWeightHints; // pigs weigh 0
    if (o.num_extra_starting_items > 0) {
        prog.total += kWeightExtraStartItems;
    }
    prog.report("Randomizing...");

    // WWRandomizer.__init__: the Logic, the progress location count check, the start probes.
    const LogicState state = defaultLogicState(o);
    if (!state.error.empty()) {
        return fail(state.error);
    }
    Logic logic;
    RandomizerLogicInit init;
    if (!initRandomizerLogic(logic, o, state, init)) {
        return fail(init.error);
    }

    traceBegin(out.sanitizedSeed);

    // randomize(): the randomizers in order. boss_reqs, entrances and starting_island are disabled
    // here (refused above when on).
    if (o.randomize_charts) {
        // ChartRandomizer (R6.6): a fresh RNG, _randomize, then logic.update_chart_macros().
        PyRandom rng;
        newRandomizerRng(rng, out.sanitizedSeed, o, req.seedKey);
        traceAttach(rng, "ChartRandomizer");
        if (!randomizeCharts(rng, logic.state().islandCharts)) {
            return fail("ChartRandomizer: the island chart table does not hold 49 islands");
        }
        logic.updateChartMacros();
    }
    {
        // PigsRandomizer (always enabled).
        PyRandom rng;
        newRandomizerRng(rng, out.sanitizedSeed, o, req.seedKey);
        traceAttach(rng, "PigsRandomizer");
        out.pigsBitfield = randomizePigs(rng);
    }
    if (o.num_extra_starting_items > 0) {
        // ExtraStartingItemsRandomizer (R6.4): on the main Logic, into logic.state().randomStartingItems.
        prog.report("Randomizing...");
        std::string err;
        if (!runExtraStartingItemsRandomizer(logic, out.sanitizedSeed, req.seedKey, &err)) {
            return fail(err);
        }
        prog.done += kWeightExtraStartItems;
    }
    // enemies, palettes: disabled (refused above when on).
    {
        prog.report("Randomizing items...");
        ItemFillResult res;
        if (!runItemRandomizer(logic, init, out.sanitizedSeed, res, req.seedKey)) {
            return fail(res.error.empty() ? logic.error() : res.error);
        }
        out.drcFailsafeLocation = res.drcFailsafeLocation;
        prog.done += kWeightItems;
    }
    HintsResult hints;
    {
        prog.report("Generating hints...");
        if (req.allowMissingHints) {
            // Without hints: only the spoiler rows that need no RNG (the Savage Labyrinth floors).
            savageLabyrinthHintRows(logic, in.hints);
        } else {
            // HintsRandomizer (R5.1, always enabled).
            HintsInput hin;
            hin.requiredDungeons = in.requiredDungeons;
            hin.allRandomizedProgressItems = init.allRandomizedProgressItems;
            if (!runHintsRandomizer(logic, hin, out.sanitizedSeed, hints, req.seedKey)) {
                return fail(hints.error);
            }
            std::string err;
            if (!hintSpoilerRows(logic, hints, in.hints, &err)) {
                return fail(err);
            }
        }
        prog.done += kWeightHints;
    }
    traceEnd();

    prog.report("Writing logs...");
    if (!o.do_not_generate_spoiler_log) {
        std::string err;
        if (!spoilerLog(logic, in, out.spoilerLog, &err)) {
            return fail(err);
        }
    }
    out.nonSpoilerLog = nonSpoilerLog(logic, in);

    // The placement (ww_export.py's placement_from_randomizer, plus our keys).
    Json j = Json::object();
    j.set("format", Json::integer(PC_RANDO_SEED_FORMAT));
    j.set("seed", Json::string(out.sanitizedSeed));
    j.set("permalink", Json::string(out.permalink));
    j.set("options", optionsToJson(o));
    Json extra = Json::array();
    for (const std::string& s : logic.state().randomStartingItems) {
        extra.push(Json::string(s));
    }
    j.set("extra_starting_items", extra);
    Json charts = Json::object();
    for (int island = 1; island <= 49 && island < (int)logic.state().islandCharts.size(); island++) {
        charts.set(std::to_string(island), Json::string(logic.state().islandCharts[island]));
    }
    j.set("charts", charts);
    Json bosses = Json::array();
    for (const std::string& s : logic.state().requiredBossItemLocations) {
        bosses.push(Json::string(s));
    }
    j.set("required_boss_locations", bosses);
    Json entrances = Json::object();
    for (const auto& c : logic.state().entranceConnections) {
        entrances.set(c.first, Json::string(c.second));
    }
    j.set("entrances", entrances);
    j.set("starting_island_room", Json::integer(in.startingIslandRoom));
    Json locations = Json::object();
    for (int i = 0; i < logic.locationCount(); i++) {
        const int item = logic.doneItemLocations[i];
        if (item < 0) {
            return fail(std::string("Location left empty by the item fill: ") + logic.locationName(i));
        }
        locations.set(logic.locationName(i), Json::string(logic.name(item)));
    }
    j.set("locations", locations);
    if (!req.allowMissingHints) {
        j.set("hints", hintsToJson(hints));
    }
    j.set("version", Json::string("SwitchWaker " + req.version));
    j.set("captured_pigs_bitfield", Json::integer(out.pigsBitfield));

    // Through R0.6's parser, so the ids, the hash and the serialization are the seed file's own.
    std::string err;
    if (!pc_rando_seed_parse(jsonDump(j, 1) + "\n", &out.seed, &err)) {
        return fail("The generated placement does not parse: " + err);
    }
    out.placementJson = pc_rando_seed_serialize(out.seed);
    prog.done = prog.total;
    prog.report("Done");
    out.ok = true;
    return true;
}

} // namespace pcr
