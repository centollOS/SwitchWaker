// pc_rando_spoiler.cpp - progression spheres and the seed's logs (docs/RANDOMIZER_PLAN.md step R1.5);
// see pc_rando_spoiler.h. Each function is the wwrando function of the same name; comments quote the
// Python where the order or the format matters for byte equality.
#include "pc_rando_spoiler.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "pc_rando_data.h"

namespace pcr {

namespace {

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool startsWith(const std::string& s, const char* prefix) { return s.compare(0, strlen(prefix), prefix) == 0; }

bool has(const std::vector<std::string>& v, const std::string& x) { return std::find(v.begin(), v.end(), x) != v.end(); }

bool hasInt(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

// Python's len() of a str: code points (UTF-8 continuation bytes do not count).
size_t pyLen(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) {
        n += (c & 0xC0) != 0x80;
    }
    return n;
}

// "%-<w>s" and "{:>w}".
std::string padRight(const std::string& s, size_t w) {
    const size_t n = pyLen(s);
    return n >= w ? s : s + std::string(w - n, ' ');
}

std::string padLeft(const std::string& s, size_t w) {
    const size_t n = pyLen(s);
    return n >= w ? s : std::string(w - n, ' ') + s;
}

// dict[key] = value: replaces in place, or appends.
void dictSet(ProgressionSphere& d, const std::string& key, const std::string& value) {
    for (SphereEntry& e : d) {
        if (e.location == key) {
            e.item = value;
            return;
        }
    }
    d.push_back(SphereEntry{key, value});
}

const SphereEntry* dictFind(const ProgressionSphere& d, const std::string& key) {
    for (const SphereEntry& e : d) {
        if (e.location == key) {
            return &e;
        }
    }
    return nullptr;
}

// get_zones_and_max_location_name_len: zone -> [(location, specific name)], dict order.
struct Zone {
    std::string name;
    std::vector<std::pair<std::string, std::string>> locations;
};

size_t zonesFor(const std::vector<std::string>& locations, std::vector<Zone>& zones) {
    zones.clear();
    size_t maxLen = 0;
    for (const std::string& loc : locations) {
        std::string zone, specific;
        splitLocationName(loc, zone, specific);
        Zone* z = nullptr;
        for (Zone& it : zones) {
            if (it.name == zone) {
                z = &it;
                break;
            }
        }
        if (z == nullptr) {
            zones.push_back(Zone{zone, {}});
            z = &zones.back();
        }
        z->locations.emplace_back(loc, specific);
        maxLen = std::max(maxLen, pyLen(specific));
    }
    return maxLen;
}

std::string itemName(const Logic& logic, int id) { return id >= 0 ? logic.name(id) : std::string("None"); }

std::vector<std::string> locationNames(const Logic& logic, const std::vector<int>& locs) {
    std::vector<std::string> out;
    out.reserve(locs.size());
    for (int loc : locs) {
        out.emplace_back(logic.locationName(loc));
    }
    return out;
}

// write_list_of_location_names_to_log.
std::string listOfLocationNames(const std::vector<std::string>& locations) {
    std::vector<Zone> zones;
    zonesFor(locations, zones);
    std::string s;
    for (const Zone& z : zones) {
        // Every location of a zone is in `locations` (the zones come from it), so the Python
        // "no locations for this zone" check never skips one.
        s += z.name + ":\n";
        for (const auto& l : z.locations) {
            s += "    " + l.second + "\n";
        }
    }
    return s;
}

// Python's str() of an option value for the header ("%s" % value).
bool optionShown(const Options& o, int id, std::string& value) {
    const OptionInfo& info = kOptionInfo[id];
    // non_disabled_options: value not in [False, [], {}] (0 == False), and not randomized_gear.
    if (id == OPT_randomized_gear) {
        return false;
    }
    switch (info.kind) {
    case OptKind::Bool:
        value.clear();
        return optionGetInt(o, id) != 0;
    case OptKind::Int:
        value = std::to_string(optionGetInt(o, id));
        return optionGetInt(o, id) != 0;
    case OptKind::Enum:
    case OptKind::Str:
        value = optionToString(o, id);
        return true;
    case OptKind::List: {
        const std::vector<std::string>& l = o.starting_gear; // the only shown list option
        if (l.empty()) {
            return false;
        }
        value = "[";
        for (size_t i = 0; i < l.size(); i++) {
            value += (i ? ", " : "") + pyRepr(l[i]);
        }
        value += "]";
        return true;
    }
    case OptKind::Dict:
        // custom_colors: wwrando shows only the colors that differ from the model's defaults (see
        // the header comment); {} is never shown.
        return false;
    }
    return false;
}

std::string shortDungeonName(const std::string& name) {
    for (const DungeonName& d : dungeon_names()) {
        if (name == d.name) {
            return d.short_name;
        }
    }
    return name;
}

// The earliest " on " or " in " (re.split(r" (?:on|in) ", name, 1)); npos when neither.
size_t firstOnIn(const std::string& s, bool onToo, size_t* sepLen) {
    size_t a = onToo ? s.find(" on ") : std::string::npos;
    size_t b = s.find(" in ");
    *sepLen = 4;
    return std::min(a, b);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Helpers

std::string pyRepr(const std::string& s) {
    const bool sq = s.find('\'') != std::string::npos;
    const bool dq = s.find('"') != std::string::npos;
    const char quote = (sq && !dq) ? '"' : '\'';
    std::string out(1, quote);
    for (unsigned char c : s) {
        if (c == '\\' || c == (unsigned char)quote) {
            out += '\\';
            out += (char)c;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20 || c == 0x7F) {
            char b[8];
            snprintf(b, sizeof b, "\\x%02x", c);
            out += b;
        } else {
            out += (char)c; // printable ASCII and UTF-8 bytes of printable code points
        }
    }
    out += quote;
    return out;
}

void splitLocationName(const std::string& name, std::string& zone, std::string& specific) {
    const size_t p = name.find(" - ");
    if (p == std::string::npos) {
        zone = specific = name;
    } else {
        zone = name.substr(0, p);
        specific = name.substr(p + 3);
    }
}

std::string shortenEntrancePathName(const std::string& name) {
    if (name == "Dungeon Entrance on Dragon Roost Island") {
        return "Dragon Roost Island (Main)";
    }
    if (name == "Secret Cave Entrance on Dragon Roost Island") {
        return "Dragon Roost Island (Pit)";
    }
    size_t sep = 0;
    // re.search(r"^(Dungeon|Secret Cave|Inner) Entrance (on|in) ", name)
    for (const char* kind : {"Dungeon", "Secret Cave", "Inner"}) {
        const std::string p = std::string(kind) + " Entrance ";
        if (startsWith(name, p.c_str()) &&
            (name.compare(p.size(), 3, "on ") == 0 || name.compare(p.size(), 3, "in ") == 0)) {
            const size_t at = firstOnIn(name, true, &sep);
            return name.substr(at + sep);
        }
    }
    // re.search(r"^(Miniboss|Boss) Entrance in ", name)
    for (const char* kind : {"Miniboss", "Boss"}) {
        const std::string p = std::string(kind) + " Entrance in ";
        if (startsWith(name, p.c_str())) {
            const size_t at = firstOnIn(name, false, &sep);
            return name.substr(at + sep) + " (" + kind + ")";
        }
    }
    return name;
}

std::string logFileName(const std::string& seed, const char* kind) { return "WW Random " + seed + " - " + kind + ".txt"; }

std::string logTrailer(const char* version) { return std::string("Generated by SwitchWaker ") + version + "\n"; }

// ---------------------------------------------------------------------------------------------
// Progression spheres

bool calculateProgressionSpheres(Logic& main, const SpoilerInput& in, std::vector<ProgressionSphere>& out,
                                 std::string* err) {
    out.clear();
    auto failWith = [&](const std::string& msg) {
        if (err != nullptr) {
            *err = msg;
        }
        return false;
    };
    const Options& opts = main.options();

    // logic = Logic(self.rando)
    Logic logic;
    if (!logic.create(opts, main.state())) {
        return failWith(logic.error());
    }
    const int n = logic.locationCount();
    const std::vector<int>& banned = main.state().bannedLocations;
    std::vector<uint8_t> isBanned(n, 0);
    for (int loc : banned) {
        if (loc >= 0 && loc < n) {
            isBanned[loc] = 1;
        }
    }
    std::vector<std::string> bannedKeys;
    for (const std::string& d : in.bannedDungeons) {
        bannedKeys.push_back(shortDungeonName(d) + " Small Key");
        bannedKeys.push_back(shortDungeonName(d) + " Big Key");
    }

    std::vector<uint8_t> previouslyAccessible(n, 0);
    bool gameBeatable = false;
    std::vector<int> accessible, inThisSphere, filtered;
    for (;;) {
        ProgressionSphere sphere;

        logic.getAccessibleRemainingLocations(true, accessible);
        accessible.erase(std::remove_if(accessible.begin(), accessible.end(), [&](int l) { return isBanned[l] != 0; }),
                         accessible.end());
        inThisSphere.clear();
        for (int loc : accessible) {
            if (!previouslyAccessible[loc]) {
                inThisSphere.push_back(loc);
            }
        }
        if (inThisSphere.empty()) {
            for (int item : logic.unplacedProgressItems) {
                if (!has(bannedKeys, logic.name(item))) {
                    return failWith("Failed to calculate progression spheres");
                }
            }
            std::vector<int> remaining;
            logic.filterLocationsForProgression(logic.remainingItemLocations, remaining, true);
            for (int loc : remaining) {
                if (!previouslyAccessible[loc] && !isBanned[loc]) {
                    return failWith("Failed to calculate progression spheres");
                }
            }
            break;
        }

        if (!opts.keylunacy) {
            // Small keys the player gained access to are given without counting a new sphere.
            std::vector<int> smallKeyLocations;
            for (int loc : inThisSphere) {
                const int pre = main.prerandomizedItem(loc);
                if (pre >= 0 && endsWith(main.name(pre), " Small Key")) {
                    smallKeyLocations.push_back(loc);
                }
            }
            if (!smallKeyLocations.empty()) {
                for (int loc : smallKeyLocations) {
                    logic.addOwnedItem(main.name(main.prerandomizedItem(loc)));
                    previouslyAccessible[loc] = 1;
                }
                if (logic.failed()) {
                    return failWith(logic.error());
                }
                continue;
            }
        }

        // Duplicated progress items (Empty Bottles) in non-progress locations are hidden.
        logic.filterLocationsForProgression(inThisSphere, filtered);

        bool addedAnyReqBossesThisSphere = false;
        for (int loc : filtered) {
            const std::string locName = main.locationName(loc);
            std::string zone, specific;
            splitLocationName(locName, zone, specific);
            if (endsWith(specific, " Heart Container")) {
                const std::string boss = specific.substr(0, specific.size() - strlen(" Heart Container"));
                dictSet(sphere, zone + " - Boss", "Defeat " + boss);
                if (has(in.requiredBosses, boss)) {
                    addedAnyReqBossesThisSphere = true;
                }
            }
            const int placed = main.doneItemLocations[loc];
            if (placed >= 0) {
                const std::string item = main.name(placed);
                if (hasInt(logic.allProgressItems, logic.item(item))) {
                    dictSet(sphere, locName, item);
                }
            }
        }

        if (!gameBeatable && !addedAnyReqBossesThisSphere) {
            gameBeatable = logic.req().checkRequirementMet("Can Reach and Defeat Ganondorf");
            if (gameBeatable) {
                dictSet(sphere, "Ganon's Tower - Rooftop", "Defeat Ganondorf");
            }
        }

        out.push_back(sphere);

        for (const SphereEntry& e : sphere) {
            if (startsWith(e.item, "Defeat ")) {
                continue;
            }
            logic.addOwnedItem(e.item);
        }
        if (logic.failed()) {
            return failWith(logic.error());
        }

        std::fill(previouslyAccessible.begin(), previouslyAccessible.end(), 0);
        for (int loc : accessible) {
            previouslyAccessible[loc] = 1;
        }
    }

    if (!gameBeatable) {
        gameBeatable = logic.req().checkRequirementMet("Can Reach and Defeat Ganondorf");
        if (gameBeatable) {
            out.push_back(ProgressionSphere{SphereEntry{"Ganon's Tower - Rooftop", "Defeat Ganondorf"}});
        }
    }
    if (logic.failed()) {
        return failWith(logic.error());
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Logs

std::string logHeader(const Options& o, const SpoilerInput& in) {
    std::string h;
    h += std::string("Wind Waker Randomizer Version ") + kWwrandoVersion + "\n";
    if (!in.permalink.empty()) {
        h += "Permalink: " + in.permalink + "\n";
    }
    if (!in.seedHash.empty()) {
        h += "Seed Hash: " + in.seedHash + "\n";
    }
    h += "Seed: " + in.seed + "\n";
    h += "Options selected:\n  ";
    bool first = true;
    for (int id = 0; id < OPT_COUNT; id++) {
        std::string value;
        if (!optionShown(o, id, value)) {
            continue;
        }
        h += first ? "" : ", ";
        first = false;
        h += kOptionInfo[id].name;
        if (kOptionInfo[id].kind != OptKind::Bool) {
            h += ": " + value;
        }
    }
    h += "\n\n\n";
    return h;
}

std::string errorLog(const Options& o, const SpoilerInput& in, const std::string& message) {
    return logHeader(o, in) + message;
}

std::string nonSpoilerLog(Logic& logic, const SpoilerInput& in) {
    std::string s = logHeader(logic.options(), in);
    // ItemRandomizer.write_to_non_spoiler_log
    std::vector<int> progress, nonprogress;
    logic.getProgressAndNonProgressLocations(progress, nonprogress);
    s += "### Locations that may or may not have progress items in them on this run:\n";
    s += listOfLocationNames(locationNames(logic, progress));
    s += "\n\n";
    s += "### Locations that cannot have progress items in them on this run:\n";
    s += listOfLocationNames(locationNames(logic, nonprogress));
    return s;
}

void savageLabyrinthHintRows(Logic& logic, std::vector<std::pair<std::string, std::string>>& rows) {
    for (const char* loc : {"Outset Island - Savage Labyrinth - Floor 30", "Outset Island - Savage Labyrinth - Floor 50"}) {
        const int idx = logic.locationIndex(loc);
        const int item = idx >= 0 ? logic.doneItemLocations[idx] : -1;
        // check_item_can_be_hinted_at
        bool valid = item >= 0 && hasInt(logic.allProgressItems, item);
        if (valid && logic.isDungeonItem(item) && !logic.options().keylunacy) {
            valid = false;
        }
        if (valid && endsWith(logic.name(item), " Trap Chest")) {
            valid = false;
        }
        rows.emplace_back(loc, valid ? logic.name(item) : std::string("Nothing"));
    }
}

bool spoilerLog(Logic& logic, const SpoilerInput& in, std::string& out, std::string* err) {
    const Options& o = logic.options();
    const LogicState& st = logic.state();
    std::string s = logHeader(o, in);

    // ExtraStartingItemsRandomizer.write_to_spoiler_log
    if (st.extraStartItemsEnabled && !st.randomStartingItems.empty()) {
        s += "Random starting item: ";
        for (size_t i = 0; i < st.randomStartingItems.size(); i++) {
            s += (i ? ", " : "") + st.randomStartingItems[i];
        }
        s += "\n\n";
    }

    // RequiredBossesRandomizer.write_to_spoiler_log
    if (o.required_bosses) {
        s += "Required dungeons:\n";
        for (const std::string& d : in.requiredDungeons) {
            s += "  " + d + "\n";
        }
        s += "\n";
        s += "Non-required dungeons:\n";
        for (const std::string& d : in.bannedDungeons) {
            s += "  " + d + "\n";
        }
        s += "\n\n\n";
    }

    if (in.randomizeItems) {
        // write_progression_spheres_to_log
        std::vector<ProgressionSphere> spheres;
        if (!calculateProgressionSpheres(logic, in, spheres, err)) {
            return false;
        }
        s += "Playthrough progression spheres:\n";
        std::vector<std::string> all;
        for (const ProgressionSphere& sp : spheres) {
            for (const SphereEntry& e : sp) {
                all.push_back(e.location);
            }
        }
        std::vector<Zone> zones;
        const size_t w = zonesFor(all, zones) + 1;
        for (size_t i = 0; i < spheres.size(); i++) {
            s += std::to_string(i + 1) + ":\n";
            for (const Zone& z : zones) {
                bool any = false;
                for (const auto& l : z.locations) {
                    any = any || dictFind(spheres[i], l.first) != nullptr;
                }
                if (!any) {
                    continue;
                }
                s += "  " + z.name + ":\n";
                for (const auto& l : z.locations) {
                    if (const SphereEntry* e = dictFind(spheres[i], l.first)) {
                        s += "      " + padRight(l.second + ":", w) + " " + e->item + "\n";
                    }
                }
            }
        }
        s += "\n\n\n";

        // write_item_location_spoilers_to_log (done_item_locations: every location, file order)
        s += "All item locations:\n";
        std::vector<std::string> locs;
        for (int i = 0; i < logic.locationCount(); i++) {
            locs.emplace_back(logic.locationName(i));
        }
        const size_t w2 = zonesFor(locs, zones) + 1;
        for (const Zone& z : zones) {
            s += z.name + ":\n";
            for (const auto& l : z.locations) {
                const int idx = logic.locationIndex(l.first);
                s += "    " + padRight(l.second + ":", w2) + " " + itemName(logic, logic.doneItemLocations[idx]) + "\n";
            }
        }
        s += "\n\n\n";
    }

    // StartingIslandRandomizer.write_to_spoiler_log
    const char* island = island_name(in.startingIslandRoom);
    s += std::string("Starting island: ") + (island != nullptr ? island : "") + "\n";
    s += "\n\n\n";

    // EntranceRandomizer.write_to_spoiler_log
    s += "Entrances:\n";
    for (const auto& c : st.entranceConnections) {
        s += "  " + padRight(c.first + ":", 50) + " " + c.second + "\n";
    }
    if (in.nestingEnabled) {
        s += "\n";
        s += "Nested entrance paths:\n";
        for (const std::vector<std::string>& path : in.nestedEntrancePaths) {
            if (path.size() < 3) {
                continue; // non-nested short paths (e.g. DRI -> Molgera)
            }
            s += "  ";
            for (size_t i = 0; i < path.size(); i++) {
                s += (i ? " -> " : "") + (i + 1 < path.size() ? shortenEntrancePathName(path[i]) : path[i]);
            }
            s += "\n";
        }
    }
    s += "\n\n\n";

    // ChartRandomizer.write_to_spoiler_log
    s += "Charts:\n";
    for (int chart = 1; chart <= 49; chart++) {
        char name[32];
        if (chart <= 8) {
            snprintf(name, sizeof name, "Triforce Chart %d", chart);
        } else {
            snprintf(name, sizeof name, "Treasure Chart %d", chart - 8);
        }
        // chart_name_to_island_number: the last island whose macro names the chart.
        int islandNumber = -1;
        for (int i = 1; i <= 49 && i < (int)st.islandCharts.size(); i++) {
            if (st.islandCharts[i] == name) {
                islandNumber = i;
            }
        }
        const char* islandName = islandNumber > 0 ? island_name(islandNumber) : nullptr;
        if (islandName == nullptr) {
            if (err != nullptr) {
                *err = std::string("No island has ") + name;
            }
            return false;
        }
        s += "  " + padRight(std::string(name) + ":", 18) + " " + islandName + "\n";
    }
    s += "\n\n\n";

    // HintsRandomizer.write_to_spoiler_log
    if (in.randomizeItems) {
        size_t w0 = 0;
        for (const auto& r : in.hints) {
            w0 = std::max(w0, pyLen(r.first));
        }
        s += "Hints:\n";
        for (const auto& r : in.hints) {
            s += padLeft(r.first, w0) + ": " + r.second + "\n";
        }
        s += "\n\n\n";
    }

    out = s;
    return true;
}

} // namespace pcr
