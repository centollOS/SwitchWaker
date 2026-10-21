// pc_rando_gen_bosses.cpp - required bosses mode, generator half (docs/RANDOMIZER_PLAN.md step R6.8);
// see pc_rando_gen_bosses.h.
#include "pc_rando_gen_bosses.h"

#include <algorithm>
#include <string.h>

#include "pc_rando_data.h"
#include "pc_rando_spoiler.h"

namespace pcr {

namespace {

bool has(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

} // namespace

bool randomizeRequiredBosses(Logic& logic, pc::rando::PyRandom& rng, RequiredBosses& out, std::string* err) {
    out = RequiredBosses();
    auto failWith = [&](const std::string& msg) {
        if (err != nullptr) {
            *err = msg;
        }
        return false;
    };
    const Options& o = logic.options();
    if (!o.progression_dungeons) {
        return failWith("Cannot make bosses required when progression dungeons are disabled.");
    }
    const int numRequired = o.num_required_bosses;

    std::vector<std::string> possible;
    for (int i = 0; i < logic.locationCount(); i++) {
        if (logic.locationTypes(i) & LOC_BOSS) {
            possible.push_back(logic.locationName(i));
        }
    }
    if (possible.size() != 6) {
        std::string msg = "Number of boss item locations is incorrect: ";
        for (size_t i = 0; i < possible.size(); i++) {
            msg += (i ? ", " : "") + possible[i];
        }
        return failWith(msg);
    }
    if (numRequired > 6 || numRequired < 1) {
        return failWith("Number of required bosses is invalid: " + std::to_string(numRequired));
    }

    out.requiredBossItemLocations = rng.sample(possible, (size_t)numRequired);

    for (const std::string& loc : possible) {
        std::string zone, specific;
        splitLocationName(loc, zone, specific);
        if (!endsWith(specific, " Heart Container")) {
            return failWith("AssertionError: " + loc + " is not a Heart Container location");
        }
        const std::string boss = specific.substr(0, specific.size() - strlen(" Heart Container"));
        if (has(out.requiredBossItemLocations, loc)) {
            out.requiredDungeons.push_back(zone);
            out.requiredBosses.push_back(boss);
        } else {
            out.bannedDungeons.push_back(zone);
            out.bannedBosses.push_back(boss);
        }
    }

    std::vector<int> bannedIdx;
    for (int i = 0; i < logic.locationCount(); i++) {
        const std::string name = logic.locationName(i);
        const std::string& zone = logic.locationZone(i);
        bool banned = false;
        if (logic.isDungeonLocation(i) && has(out.bannedDungeons, zone)) {
            banned = true;
        } else if (name == "Mailbox - Letter from Orca") {
            banned = has(out.bannedDungeons, "Forbidden Woods");
        } else if (name == "Mailbox - Letter from Baito") {
            banned = has(out.bannedDungeons, "Earth Temple");
        } else if (name == "Mailbox - Letter from Aryll" || name == "Mailbox - Letter from Tingle") {
            banned = has(out.bannedDungeons, "Forsaken Fortress");
        }
        if (banned) {
            out.bannedLocations.push_back(name);
            bannedIdx.push_back(i);
        }
    }

    LogicState& st = logic.state();
    st.requiredBossItemLocations = out.requiredBossItemLocations;
    st.bannedLocations = bannedIdx;
    logic.updateRequiredBossesMacro();
    if (logic.failed()) {
        return failWith(logic.error());
    }
    return true;
}

} // namespace pcr
