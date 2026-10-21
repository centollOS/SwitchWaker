// cos_rando_test_R1_3: the logic state and item pool (docs/RANDOMIZER_PLAN.md step R1.3).
//
// 1. Vectors from wwrando (fixtures/R1_3_pools.txt, native/tools/rando/gen_pool_vectors.py): for 7
//    presets the Logic built by WWRandomizer.__init__ matches Python list for list, in order: the
//    starting items, the all_* and unplaced_* item lists, the owned items, the cleaned item names,
//    the progress location filters and counts, the macros rewritten after macros.txt, the nested
//    entrance macros, dungeons_only_start / dungeons_and_caves_only_start, the
//    TooFewProgressionLocationsError text, check_item_valid_in_location for every item and location,
//    and the usefulness probes (including how they reorder unplaced_progress_items).
// 2. The plan's default counts and synthetic cases: the rock spire shop rule, remove_owned_item's
//    append rule, set_location_to_item, simulated playthrough snapshots, the required bosses macro.
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_data.h"
#include "pc_rando_logic.h"
#include "pc_rando_options.h"
#include "rando_test.h"

namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t b = 0;
    for (;;) {
        size_t e = s.find(sep, b);
        if (e == std::string::npos) {
            out.push_back(s.substr(b));
            return out;
        }
        out.push_back(s.substr(b, e - b));
        b = e + 1;
    }
}

std::vector<std::string> splitList(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    return split(s, '|');
}

struct Preset {
    std::string label;
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<std::pair<std::string, std::string>> lists; // name -> a|b|...
    std::vector<std::pair<std::string, std::string>> locs;
    std::vector<std::pair<std::string, int>> nums;
    std::vector<std::pair<std::string, int>> flags;
    std::vector<std::pair<std::string, std::string>> macros;
    std::vector<std::pair<std::string, std::string>> nested;
    std::vector<std::pair<std::string, std::string>> valid;
    bool hasProbes = false;
    std::string useless, frac, first;
    std::string error;
};

bool loadFixture(std::vector<Preset>& out) {
    std::string text;
    if (!rt::readFixture("R1_3_pools.txt", text)) {
        return false;
    }
    bool ended = false;
    for (const std::string& line : split(text, '\n')) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = split(line, '\t');
        const std::string& tag = f[0];
        auto arg = [&](size_t i) { return i < f.size() ? f[i] : std::string(); };
        if (tag == "version") {
            RT_CHECK_EQ(arg(1), std::string(pcr::kWwrandoVersion));
        } else if (tag == "preset") {
            out.push_back(Preset());
            out.back().label = arg(1);
        } else if (tag == "opt") {
            out.back().opts.emplace_back(arg(1), arg(2));
        } else if (tag == "list") {
            out.back().lists.emplace_back(arg(1), arg(2));
        } else if (tag == "locs") {
            out.back().locs.emplace_back(arg(1), arg(2));
        } else if (tag == "num") {
            out.back().nums.emplace_back(arg(1), std::stoi(arg(2)));
        } else if (tag == "flag") {
            out.back().flags.emplace_back(arg(1), std::stoi(arg(2)));
        } else if (tag == "macro") {
            out.back().macros.emplace_back(arg(1), arg(2));
        } else if (tag == "nested") {
            out.back().nested.emplace_back(arg(1), arg(2));
        } else if (tag == "valid") {
            out.back().valid.emplace_back(arg(1), arg(2));
        } else if (tag == "useless") {
            out.back().hasProbes = true;
            out.back().useless = arg(1);
        } else if (tag == "frac") {
            out.back().frac = arg(1);
        } else if (tag == "first") {
            out.back().first = arg(1);
        } else if (tag == "error") {
            std::string e = arg(1), u;
            for (size_t i = 0; i < e.size(); i++) {
                if (e[i] == '\\' && i + 1 < e.size() && e[i + 1] == 'n') {
                    u.push_back('\n');
                    i++;
                } else {
                    u.push_back(e[i]);
                }
            }
            out.back().error = u;
        } else if (tag == "endpreset") {
        } else if (tag == "end") {
            ended = true;
        } else {
            std::fprintf(stderr, "FAIL unknown fixture line: %s\n", line.c_str());
            return false;
        }
    }
    return RT_CHECK(ended);
}

std::string joinNames(const pcr::Logic& L, const std::vector<int>& ids) {
    std::string s;
    for (size_t i = 0; i < ids.size(); i++) {
        if (i) {
            s += '|';
        }
        s += L.name(ids[i]);
    }
    return s;
}

std::string joinInts(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        if (i) {
            s += ',';
        }
        s += std::to_string(v[i]);
    }
    return s;
}

std::string joinStrs(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        if (i) {
            s += '|';
        }
        s += v[i];
    }
    return s;
}

// The 320 validity bits as wwrando's 80 hex digits (bit i = location i, most significant first).
std::string validHex(const pcr::Logic& L, int item) {
    const int n = L.locationCount();
    std::string hex(80, '0');
    for (int d = 0; d < 80; d++) {
        int v = 0;
        for (int b = 0; b < 4; b++) {
            int loc = d * 4 + b;
            if (loc < n && L.checkItemValidInLocation(item, loc)) {
                v |= 1 << b;
            }
        }
        hex[79 - d] = "0123456789abcdef"[v];
    }
    return hex;
}

bool buildOptions(const Preset& p, pcr::Options& o) {
    o = pcr::Options();
    for (const auto& kv : p.opts) {
        int id = pcr::optionFind(kv.first.c_str());
        if (!RT_CHECK(id >= 0) || !RT_CHECK(pcr::optionFromString(o, id, kv.second.c_str()))) {
            std::fprintf(stderr, "  option %s=%s\n", kv.first.c_str(), kv.second.c_str());
            return false;
        }
    }
    return true;
}

void checkPreset(const Preset& p) {
    std::fprintf(stderr, "preset %s\n", p.label.c_str());
    pcr::Options o;
    if (!buildOptions(p, o)) {
        return;
    }
    pcr::LogicState st = pcr::defaultLogicState(o);
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    const bool ok = pcr::initRandomizerLogic(L, o, st, init);
    if (!p.error.empty()) {
        RT_CHECK(!ok);
        RT_CHECK(init.tooFewProgressionLocations);
        RT_CHECK_EQ(init.error, p.error);
    } else {
        if (!RT_CHECK(ok)) {
            std::fprintf(stderr, "  error: %s\n", init.error.c_str());
            return;
        }
    }

    for (const auto& kv : p.lists) {
        std::string got;
        const std::string& n = kv.first;
        if (n == "starting_items") {
            got = joinStrs(L.state().startingItems);
        } else if (n == "all_cleaned_item_names") {
            got = joinNames(L, L.allCleanedItemNames);
        } else if (n == "all_progress_items") {
            got = joinNames(L, L.allProgressItems);
        } else if (n == "all_nonprogress_items") {
            got = joinNames(L, L.allNonprogressItems);
        } else if (n == "all_fixed_consumable_items") {
            got = joinNames(L, L.allFixedConsumableItems);
        } else if (n == "duplicatable_consumable_items") {
            got = joinNames(L, L.duplicatableConsumableItems);
        } else if (n == "unplaced_progress_items") {
            got = joinNames(L, L.unplacedProgressItems);
        } else if (n == "unplaced_nonprogress_items") {
            got = joinNames(L, L.unplacedNonprogressItems);
        } else if (n == "unplaced_fixed_consumable_items") {
            got = joinNames(L, L.unplacedFixedConsumableItems);
        } else if (n == "currently_owned_items") {
            got = joinNames(L, L.currentlyOwnedItems);
        } else if (n == "all_randomized_progress_items") {
            got = joinNames(L, init.allRandomizedProgressItems);
        } else {
            continue; // probe lists are checked below
        }
        if (!RT_CHECK_EQ(got, kv.second)) {
            std::fprintf(stderr, "  list %s\n", n.c_str());
        }
    }

    std::vector<int> all, v, prog, nonprog;
    for (int i = 0; i < L.locationCount(); i++) {
        all.push_back(i);
    }
    for (const auto& kv : p.locs) {
        std::string got;
        if (kv.first == "progress_locations" || kv.first == "nonprogress_locations") {
            L.getProgressAndNonProgressLocations(prog, nonprog);
            got = joinInts(kv.first == "progress_locations" ? prog : nonprog);
        } else if (kv.first == "progression_filtered") {
            L.filterLocationsForProgression(all, v, false);
            got = joinInts(v);
        } else if (kv.first == "progression_filtered_sunken") {
            L.filterLocationsForProgression(all, v, true);
            got = joinInts(v);
        } else {
            RT_CHECK(!"unknown locs line");
        }
        if (!RT_CHECK_EQ(got, kv.second)) {
            std::fprintf(stderr, "  locs %s\n", kv.first.c_str());
        }
    }
    for (const auto& kv : p.nums) {
        if (kv.first == "num_progression_locations") {
            RT_CHECK_EQ(L.getNumProgressionLocations(), kv.second);
            RT_CHECK_EQ(init.numProgressLocations, kv.second);
        } else if (kv.first == "max_required_bosses_banned_locations") {
            RT_CHECK_EQ(L.getMaxRequiredBossesBannedLocations(), kv.second);
        } else {
            RT_CHECK(!"unknown num line");
        }
    }
    for (const auto& kv : p.flags) {
        if (kv.first == "dungeons_only_start") {
            RT_CHECK_EQ((int)init.dungeonsOnlyStart, kv.second);
        } else if (kv.first == "dungeons_and_caves_only_start") {
            RT_CHECK_EQ((int)init.dungeonsAndCavesOnlyStart, kv.second);
        } else {
            RT_CHECK(!"unknown flag line");
        }
    }

    // Macros: the rewritten ones carry Python's last body; every other macro still has its
    // macros.txt body.
    const pcr::ReqLogic& R = L.req();
    for (const auto& kv : p.macros) {
        const std::string* body = R.macroBody(R.find(kv.first));
        if (!RT_CHECK(body != nullptr) || !RT_CHECK_EQ(*body, kv.second)) {
            std::fprintf(stderr, "  macro %s\n", kv.first.c_str());
        }
    }
    for (const pcr::Macro& m : pcr::macros()) {
        bool rewritten = false;
        for (const auto& kv : p.macros) {
            if (kv.first == m.name) {
                rewritten = true;
            }
        }
        if (!rewritten) {
            const std::string* body = R.macroBody(R.find(m.name));
            if (!RT_CHECK(body != nullptr && *body == m.body)) {
                std::fprintf(stderr, "  macro %s changed\n", m.name);
            }
        }
    }
    RT_CHECK_EQ(R.macroOrder().size(), pcr::macros().size);
    // Nested entrance macros (the recursion guard): every one is in place.
    RT_CHECK_EQ(L.state().nestedEntrances.size(), p.nested.size());
    for (size_t i = 0; i < p.nested.size() && i < L.state().nestedEntrances.size(); i++) {
        RT_CHECK_EQ("Can Access " + L.state().nestedEntrances[i].first, p.nested[i].first);
        RT_CHECK_EQ("Can Access " + L.state().nestedEntrances[i].second, p.nested[i].second);
    }

    int badValid = 0;
    for (const auto& kv : p.valid) {
        int id = L.req().find(kv.first);
        if (!RT_CHECK(id >= 0)) {
            continue;
        }
        std::string got = validHex(L, id);
        if (got != kv.second && badValid++ < 5) {
            RT_CHECK_EQ(got, kv.second);
            std::fprintf(stderr, "  valid %s\n", kv.first.c_str());
        }
    }
    RT_CHECK_EQ(badValid, 0);

    if (!p.hasProbes) {
        RT_CHECK(!ok);
        return;
    }
    auto list = [&](const char* name) {
        for (const auto& kv : p.lists) {
            if (kv.first == name) {
                return kv.second;
            }
        }
        RT_CHECK(!"missing probe list");
        return std::string();
    };
    std::vector<int> useless;
    L.getAllUselessItems(std::vector<int>(L.unplacedProgressItems), useless);
    RT_CHECK_EQ(joinNames(L, useless), p.useless);
    RT_CHECK_EQ(joinNames(L, L.unplacedProgressItems), list("probe1"));
    std::vector<std::pair<int, int>> frac;
    L.getItemsByUsefulnessFraction(std::vector<int>(L.unplacedProgressItems), true, frac);
    std::string fracText;
    for (size_t i = 0; i < frac.size(); i++) {
        if (i) {
            fracText += ',';
        }
        fracText += L.name(frac[i].first) + "*" + std::to_string(frac[i].second);
    }
    RT_CHECK_EQ(fracText, p.frac);
    std::vector<int> rev(L.unplacedProgressItems.rbegin(), L.unplacedProgressItems.rend());
    int first = L.getFirstUsefulItem(rev);
    RT_CHECK_EQ(first >= 0 ? L.name(first) : std::string(), p.first);
    RT_CHECK_EQ(joinNames(L, L.unplacedProgressItems), list("probe3"));
    if (!RT_CHECK(!L.failed())) {
        std::fprintf(stderr, "  error: %s\n", L.error().c_str());
    }
}

void checkSynthetic() {
    pcr::Options o;
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init));
    // The plan's default counts.
    RT_CHECK_EQ(L.unplacedProgressItems.size(), (size_t)56);
    RT_CHECK_EQ(L.unplacedNonprogressItems.size(), (size_t)141);
    RT_CHECK_EQ(L.unplacedFixedConsumableItems.size(), (size_t)90);
    RT_CHECK_EQ(init.numProgressLocations, 119);
    std::vector<int> prog, nonprog;
    L.getProgressAndNonProgressLocations(prog, nonprog);
    RT_CHECK_EQ(prog.size(), (size_t)119);
    RT_CHECK_EQ(nonprog.size(), (size_t)201);
    RT_CHECK(!init.dungeonsOnlyStart);
    RT_CHECK(!init.dungeonsAndCavesOnlyStart);

    // Starting items order.
    std::vector<std::string> start = pcr::startingItemsFor(o);
    RT_CHECK_EQ(joinStrs(start), std::string("Wind Waker|Wind's Requiem|Boat's Sail|Ballad of Gales|Progressive Magic "
                                             "Meter|Progressive Shield|Song of Passing|Progressive Sword|Heart "
                                             "Container|Heart Container|Heart Container"));

    // remove_owned_item appends to unplaced progress only while all_progress holds more copies.
    const int bow = L.item("Progressive Bow");
    const int nBow = (int)std::count(L.unplacedProgressItems.begin(), L.unplacedProgressItems.end(), bow);
    RT_CHECK_EQ(nBow, 3);
    L.addOwnedItem(bow);
    RT_CHECK_EQ((int)std::count(L.unplacedProgressItems.begin(), L.unplacedProgressItems.end(), bow), 2);
    L.removeOwnedItem(bow);
    RT_CHECK_EQ(L.unplacedProgressItems.back(), bow);
    RT_CHECK_EQ((int)std::count(L.unplacedProgressItems.begin(), L.unplacedProgressItems.end(), bow), 3);
    // Consumables cannot be removed (wwrando raises).
    {
        pcr::Logic L2;
        RT_CHECK(L2.create(o, pcr::defaultLogicState(o)));
        L2.addOwnedItem("Green Rupee");
        L2.removeOwnedItem("Green Rupee");
        RT_CHECK(L2.failed());
        RT_CHECK_EQ(L2.error(), std::string("Cannot remove item from simulated inventory: Green Rupee"));
        pcr::Logic L3;
        RT_CHECK(L3.create(o, pcr::defaultLogicState(o)));
        L3.addOwnedItem("Not An Item");
        RT_CHECK_EQ(L3.error(), std::string("Unknown item name: Not An Item"));
    }

    // The rock spire shop: one item at most once across its slots.
    RT_CHECK(L.rockSpireShopShipLocations.size() >= 2);
    if (L.rockSpireShopShipLocations.size() >= 2) {
        const int a = L.rockSpireShopShipLocations[0], b = L.rockSpireShopShipLocations[1];
        const int hook = L.item("Grappling Hook");
        RT_CHECK(L.checkItemValidInLocation(hook, b));
        L.setLocationToItem(a, hook);
        RT_CHECK(!L.checkItemValidInLocation(hook, b));
        RT_CHECK(L.checkItemValidInLocation(L.item("Hookshot"), b));
        RT_CHECK_EQ(L.doneItemLocations[a], hook);
        RT_CHECK(std::find(L.remainingItemLocations.begin(), L.remainingItemLocations.end(), a) ==
                 L.remainingItemLocations.end());
        L.setLocationToItem(a, hook);
        RT_CHECK(L.failed());
        RT_CHECK_EQ(L.error(), std::string("Location was used twice: ") + L.locationName(a));
    }

    // Snapshots restore the lists and the inventory.
    {
        pcr::Logic L4;
        RT_CHECK(L4.create(o, pcr::defaultLogicState(o)));
        pcr::Logic::PlaythroughState s = L4.saveSimulatedPlaythroughState();
        const std::vector<int> prog0 = L4.unplacedProgressItems;
        const int hook = L4.item("Grappling Hook");
        const int loc = L4.locationIndex("Dragon Roost Cavern - First Room");
        RT_CHECK(loc >= 0);
        L4.addOwnedItem(hook);
        L4.addOwnedItem("Progressive Bow");
        RT_CHECK(L4.unplacedProgressItems != prog0);
        L4.loadSimulatedPlaythroughState(s);
        RT_CHECK(L4.unplacedProgressItems == prog0);
        RT_CHECK_EQ(L4.req().owned(hook), 0);
        RT_CHECK(!L4.failed());
    }

    // Required bosses macros.
    {
        pcr::Logic L5;
        pcr::Options ob;
        ob.required_bosses = true;
        pcr::LogicState sb = pcr::defaultLogicState(ob);
        RT_CHECK(L5.create(ob, sb));
        // At init nothing is sampled yet: the macro is an empty AND (always met).
        const std::string* body = L5.req().macroBody(L5.req().find("Can Defeat All Required Bosses"));
        RT_CHECK(body != nullptr && body->empty());
        RT_CHECK(L5.req().checkRequirementMet(std::string("Can Defeat All Required Bosses")));
        L5.state().requiredBossItemLocations = {"Dragon Roost Cavern - Gohma Heart Container",
                                                "Wind Temple - Molgera Heart Container"};
        L5.updateRequiredBossesMacro();
        body = L5.req().macroBody(L5.req().find("Can Defeat All Required Bosses"));
        RT_CHECK_EQ(*body, std::string("Can Access Item Location \"Dragon Roost Cavern - Gohma Heart Container\" & "
                                       "Can Access Item Location \"Wind Temple - Molgera Heart Container\""));
        RT_CHECK(!L5.req().checkRequirementMet(std::string("Can Defeat All Required Bosses")));
    }

    // Dungeon items stay in their dungeon, off the boss, without key-lunacy.
    {
        const int key = L.item("DRC Small Key");
        RT_CHECK(L.checkItemValidInLocation(key, L.locationIndex("Dragon Roost Cavern - First Room")));
        RT_CHECK(!L.checkItemValidInLocation(key, L.locationIndex("Dragon Roost Cavern - Gohma Heart Container")));
        RT_CHECK(!L.checkItemValidInLocation(key, L.locationIndex("Forbidden Woods - First Room")));
        RT_CHECK(L.isDungeonItem(key));
        RT_CHECK(!L.isDungeonItem(L.item("Grappling Hook")));
    }

    // Entrance sets per mix mode.
    {
        pcr::Options oe;
        oe.randomize_dungeon_entrances = true;
        oe.randomize_fairy_fountain_entrances = true;
        std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> sets;
        RT_CHECK(pcr::entranceSetsFor(oe, sets, nullptr));
        RT_CHECK_EQ(sets.size(), (size_t)2);
        if (sets.size() == 2) {
            RT_CHECK_EQ(sets[0].first.size(), (size_t)5);
            RT_CHECK_EQ(sets[1].second.size(), (size_t)6);
        }
        oe.mix_entrances = pcr::EntranceMixMode::MixDungeons;
        RT_CHECK(pcr::entranceSetsFor(oe, sets, nullptr));
        RT_CHECK_EQ(sets.size(), (size_t)1);
        if (sets.size() == 1) {
            RT_CHECK_EQ(sets[0].first.size(), (size_t)11);
            RT_CHECK_EQ(sets[0].second[5], std::string("Outset Fairy Fountain"));
        }
        int n = 0;
        pcr::vanillaEntrances(&n);
        RT_CHECK_EQ(n, 44);
    }
}

} // namespace

int main() {
    std::vector<Preset> presets;
    if (loadFixture(presets)) {
        RT_CHECK_EQ(presets.size(), (size_t)7);
        for (const Preset& p : presets) {
            checkPreset(p);
        }
    }
    checkSynthetic();
    return rt::finish();
}
