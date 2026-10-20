// Unit test of plan step R4.1 (docs/RANDOMIZER_PLAN.md): the randomizer's settings model
// (features/rando/pc_rando_settings.h, namespace pcr; pc_rando_presets.cpp):
//   - every exposed option has a unique COS_RANDO_<NAME> key, and every value of it survives
//     settingValue -> settingParse; bad values (out of range, unknown tokens) are refused;
//   - the options the menu does not show are the documented ones;
//   - the presets: the defaults are Options(); "All checks" turns on the 23 categories; "Short" only
//     dungeons and free gifts; presetMatch finds each; their permalinks for TestSeed1 equal wwrando's
//     (fixture R4_1_presets.txt);
//   - settings -> Options (optionsFromSettings, a key map), then ensureValidCombination, gives
//     wwrando's permalink for a custom case (Swordless with a sword in the starting gear, required
//     bosses without dungeons, remote hints without location hints);
//   - the starting gear editor: counts, clamping, the rest goes to randomized_gear, the sword rules,
//     gearParse of bad text, "-" for none.
#include "rando_test.h"

#include "pc_rando_settings.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace {

using Map = std::map<std::string, std::string>;

std::string mapGet(const char* key, void* user) {
    const Map& m = *static_cast<Map*>(user);
    auto it = m.find(key);
    return it != m.end() ? it->second : std::string();
}

std::map<std::string, std::string> fixture() {
    std::string text;
    std::map<std::string, std::string> out;
    RT_CHECK(rt::readFixture("R4_1_presets.txt", text));
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string name, permalink;
        ls >> name >> permalink;
        out[name] = permalink;
    }
    return out;
}

bool sameInventory(const pcr::Options& o) {
    std::vector<std::string> all = o.starting_gear;
    all.insert(all.end(), o.randomized_gear.begin(), o.randomized_gear.end());
    std::sort(all.begin(), all.end());
    std::vector<std::string> inv = pcr::inventoryItems();
    std::sort(inv.begin(), inv.end());
    return all == inv && std::is_sorted(o.starting_gear.begin(), o.starting_gear.end()) &&
           std::is_sorted(o.randomized_gear.begin(), o.randomized_gear.end());
}

} // namespace

int main() {
    using namespace pcr;

    // ---- keys and values
    std::set<std::string> keys;
    int exposed = 0;
    for (int id = 0; id < OPT_COUNT; id++) {
        if (!settingExposed(id)) {
            continue;
        }
        exposed++;
        const std::string key = settingKey(id);
        RT_CHECK(key.rfind("COS_RANDO_", 0) == 0);
        RT_CHECK(keys.insert(key).second);
        const OptionInfo& info = kOptionInfo[id];
        int lo = 0, hi = 1;
        if (info.kind == OptKind::Enum) {
            hi = info.numChoices - 1;
            int n = 0;
            RT_CHECK(enumTokens(id, &n) != nullptr);
            RT_CHECK_EQ(n, info.numChoices);
        } else if (info.kind == OptKind::Int) {
            lo = info.minimum;
            hi = info.maximum;
        }
        for (int v = lo; v <= hi; v++) {
            Options o;
            RT_CHECK(optionSetInt(o, id, v));
            const std::string text = settingValue(o, id);
            Options p;
            RT_CHECK(settingParse(p, id, text.c_str()));
            RT_CHECK_EQ(optionGetInt(p, id), v);
        }
        Options o;
        const int before = optionGetInt(o, id);
        RT_CHECK(!settingParse(o, id, "bogus"));
        RT_CHECK(!settingParse(o, id, ""));
        if (info.kind == OptKind::Int) {
            RT_CHECK(!settingParse(o, id, std::to_string(info.maximum + 1).c_str()));
            RT_CHECK(!settingParse(o, id, std::to_string(info.minimum - 1).c_str()));
        }
        RT_CHECK_EQ(optionGetInt(o, id), before);
    }
    RT_CHECK(keys.count(kStartingGearKey) == 0);
    RT_CHECK(keys.count("COS_RANDO_PROGRESSION_DUNGEONS") == 1);
    RT_CHECK(keys.count("COS_RANDO_SWORD_MODE") == 1);
    RT_CHECK(keys.count("COS_RANDO_NUM_REQUIRED_BOSSES") == 1);
    // The keys the earlier steps read from the environment are not option keys.
    for (const char* k : {"COS_RANDO_FORCE", "COS_RANDO_SMOKE", "COS_RANDO_PLACEMENT", "COS_RANDO_SEED",
                          "COS_RANDO_DIR", "COS_RANDO_TRACE"}) {
        RT_CHECK(keys.count(k) == 0);
    }
    // 77 options minus the 12 without a key of their own.
    RT_CHECK_EQ(exposed, 65);
    for (int id : {OPT_randomize_enemies, OPT_dry_run, OPT_custom_player_model, OPT_player_in_casual_clothes,
                   OPT_disable_custom_player_voice, OPT_disable_custom_player_items, OPT_custom_color_preset,
                   OPT_custom_colors, OPT_invert_camera_x_axis, OPT_remove_title_and_ending_videos,
                   OPT_randomized_gear, OPT_starting_gear}) {
        RT_CHECK(!settingExposed(id));
    }
    {
        Options o;
        RT_CHECK_EQ(settingValue(o, OPT_sword_mode), std::string("sword"));
        RT_CHECK(settingParse(o, OPT_sword_mode, "swordless"));
        RT_CHECK(o.sword_mode == SwordMode::Swordless);
        RT_CHECK(settingParse(o, OPT_logic_precision, "very_hard"));
        RT_CHECK(o.logic_precision == TrickDifficulty::VeryHard);
        RT_CHECK(!settingParse(o, OPT_hero_mode, "true")); // stored bools are 0/1 only
    }

    // ---- presets
    int categories = 0;
    for (int id = 0; id < OPT_COUNT; id++) {
        categories += isCategory(id) ? 1 : 0;
    }
    RT_CHECK_EQ(categories, 23);
    RT_CHECK(settingsEqual(presetOptions(kPresetDefaults), Options()));
    RT_CHECK(optionsEqual(presetOptions(kPresetDefaults), Options()));
    {
        const Options all = presetOptions(kPresetAllChecks);
        const Options shrt = presetOptions(kPresetShort);
        for (int id = 0; id < OPT_COUNT; id++) {
            if (isCategory(id)) {
                RT_CHECK_EQ(optionGetInt(all, id), 1);
                RT_CHECK_EQ(optionGetInt(shrt, id),
                            id == OPT_progression_dungeons || id == OPT_progression_free_gifts ? 1 : 0);
            } else {
                RT_CHECK_EQ(optionToString(all, id), optionToString(Options(), id));
                RT_CHECK_EQ(optionToString(shrt, id), optionToString(Options(), id));
            }
        }
    }
    for (int p = 0; p < kPresetCount; p++) {
        RT_CHECK_EQ(presetMatch(presetOptions(p)), p);
        Options v = presetOptions(p);
        ensureValidCombination(v);
        RT_CHECK(settingsEqual(v, presetOptions(p))); // the presets are valid as they are
        RT_CHECK(presetName(p, true) != nullptr && presetName(p, false) != nullptr);
    }
    {
        Options o;
        o.hero_mode = true;
        RT_CHECK_EQ(presetMatch(o), -1);
        o = Options();
        gearSetCount(o, "Bombs", 1);
        RT_CHECK_EQ(presetMatch(o), -1);
    }
    const auto perma = fixture();
    RT_CHECK_EQ(perma.size(), (size_t)4);
    if (perma.size() == 4) {
        RT_CHECK_EQ(encodePermalink("TestSeed1", presetOptions(kPresetDefaults)), perma.at("defaults"));
        RT_CHECK_EQ(encodePermalink("TestSeed1", presetOptions(kPresetAllChecks)), perma.at("all"));
        RT_CHECK_EQ(encodePermalink("TestSeed1", presetOptions(kPresetShort)), perma.at("short"));
    }

    // ---- settings -> Options
    {
        Map m;
        RT_CHECK(settingsEqual(optionsFromSettings(mapGet, &m), Options())); // nothing stored: defaults
        // A stored preset reads back as that preset.
        const Options all = presetOptions(kPresetAllChecks);
        for (int id = 0; id < OPT_COUNT; id++) {
            if (settingExposed(id)) {
                m[settingKey(id)] = settingValue(all, id);
            }
        }
        m[kStartingGearKey] = gearStored(all);
        RT_CHECK_EQ(presetMatch(optionsFromSettings(mapGet, &m)), (int)kPresetAllChecks);
    }
    {
        Map m;
        m["COS_RANDO_SWORD_MODE"] = "swordless";
        m[kStartingGearKey] = "Bombs|Progressive Bow|Progressive Sword|Progressive Bow";
        m["COS_RANDO_PROGRESSION_DUNGEONS"] = "0";
        m["COS_RANDO_REQUIRED_BOSSES"] = "1";
        m["COS_RANDO_NUM_LOCATION_HINTS"] = "0";
        m["COS_RANDO_PRIORITIZE_REMOTE_HINTS"] = "1";
        m["COS_RANDO_STARTING_HCS"] = "5";
        m["COS_RANDO_STARTING_POHS"] = "99"; // out of range: the default
        m["COS_RANDO_HERO_MODE"] = "maybe";  // bad: the default
        Options o = optionsFromSettings(mapGet, &m);
        RT_CHECK(o.required_bosses);
        RT_CHECK_EQ(o.starting_pohs, 0);
        RT_CHECK(!o.hero_mode);
        RT_CHECK_EQ(gearCount(o, "Progressive Sword"), 1);
        RT_CHECK(!optionEnabled(o, OPT_required_bosses));
        RT_CHECK(!optionEnabled(o, OPT_prioritize_remote_hints));
        ensureValidCombination(o);
        RT_CHECK(!o.required_bosses);
        RT_CHECK(!o.prioritize_remote_hints);
        RT_CHECK_EQ(gearCount(o, "Progressive Sword"), 0);
        RT_CHECK(sameInventory(o));
        if (perma.count("custom") != 0) {
            RT_CHECK_EQ(encodePermalink("TestSeed1", o), perma.at("custom"));
        }
    }

    // ---- starting gear editor
    RT_CHECK_EQ(gearItemCount(), 64);
    RT_CHECK_EQ(std::string(gearItem(0).name), std::string(kRegularItems[0]));
    RT_CHECK_EQ(gearItem(0).max, 1);
    RT_CHECK_EQ(std::string(gearItem(kRegularItemCount + 6).name), std::string("Progressive Sword"));
    RT_CHECK_EQ(gearItem(kRegularItemCount + 6).max, 3);
    {
        int total = 0;
        for (int i = 0; i < gearItemCount(); i++) {
            total += gearItem(i).max;
        }
        RT_CHECK_EQ(total, (int)inventoryItems().size()); // 74
    }
    {
        Options o;
        RT_CHECK_EQ(gearCount(o, "Progressive Shield"), 1);
        RT_CHECK_EQ(gearCount(o, "Ballad of Gales"), 1);
        gearSetCount(o, "Progressive Bow", 5); // clamped to 3
        RT_CHECK_EQ(gearCount(o, "Progressive Bow"), 3);
        RT_CHECK(std::count(o.randomized_gear.begin(), o.randomized_gear.end(), "Progressive Bow") == 0);
        RT_CHECK(sameInventory(o));
        gearSetCount(o, "Progressive Bow", 1);
        RT_CHECK_EQ(gearCount(o, "Progressive Bow"), 1);
        RT_CHECK(std::count(o.randomized_gear.begin(), o.randomized_gear.end(), "Progressive Bow") == 2);
        gearSetCount(o, "Ballad of Gales", 0);
        RT_CHECK_EQ(gearCount(o, "Ballad of Gales"), 0);
        RT_CHECK(sameInventory(o));
        // stored text round trip
        Options p;
        RT_CHECK(gearParse(p, gearValue(o).c_str()));
        RT_CHECK_EQ(gearValue(p), gearValue(o));
        RT_CHECK(sameInventory(p));
        // none: "-"
        Options none;
        RT_CHECK(gearParse(none, ""));
        RT_CHECK(none.starting_gear.empty());
        RT_CHECK_EQ(gearStored(none), std::string("-"));
        RT_CHECK(sameInventory(none));
        Map m;
        m[kStartingGearKey] = "-";
        RT_CHECK(optionsFromSettings(mapGet, &m).starting_gear.empty());
        // bad text: unknown names and extra copies dropped
        Options bad;
        RT_CHECK(!gearParse(bad, "Bombs|Nonsense|Bombs|Hookshot"));
        RT_CHECK_EQ(gearValue(bad), std::string("Bombs|Hookshot"));
        RT_CHECK(sameInventory(bad));
    }
    {
        Options o;
        RT_CHECK(gearEnabled(o, "Progressive Sword"));
        RT_CHECK(gearEnabled(o, "Hurricane Spin"));
        RT_CHECK(gearEnabled(o, "Bombs"));
        o.sword_mode = SwordMode::NoStartingSword;
        RT_CHECK(!gearEnabled(o, "Progressive Sword"));
        RT_CHECK(gearEnabled(o, "Hurricane Spin"));
        o.sword_mode = SwordMode::Swordless;
        RT_CHECK(!gearEnabled(o, "Progressive Sword"));
        RT_CHECK(!gearEnabled(o, "Hurricane Spin"));
    }
    return rt::finish();
}
