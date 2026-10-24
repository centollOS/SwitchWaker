// The randomizer's settings model (docs/RANDOMIZER_PLAN.md step R4.1; pc_rando_settings.h, namespace
// pcr): the COS_RANDO_* keys, their stored text, the presets and the starting gear editor. Pure C++:
// no game headers or calls (cos_rando_core; unit test native/check/rando/test_R4_1.cpp).
#include "pc_rando_settings.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace pcr {

const char* const kStartingGearKey = "COS_RANDO_STARTING_GEAR";

namespace {

const char* const kSwordTokens[] = {"sword", "no_sword", "swordless"};
const char* const kMixTokens[] = {"separate", "mix"};
const char* const kTrickTokens[] = {"none", "normal", "hard", "very_hard"};

bool parseInt(const char* s, int* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    char* end = nullptr;
    const long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < -0x7FFFFFFFL || v > 0x7FFFFFFFL) {
        return false;
    }
    *out = (int)v;
    return true;
}

} // namespace

bool settingExposed(int id) {
    if (id < 0 || id >= OPT_COUNT) {
        return false;
    }
    switch (id) {
    case OPT_dry_run:
    case OPT_custom_player_model:
    case OPT_player_in_casual_clothes:
    case OPT_disable_custom_player_voice:
    case OPT_disable_custom_player_items:
    case OPT_custom_color_preset:
    case OPT_custom_colors:
    case OPT_invert_camera_x_axis:
    case OPT_remove_title_and_ending_videos:
    case OPT_randomized_gear:
    case OPT_starting_gear: // its own key, kStartingGearKey
        return false;
    default:
        break;
    }
    const OptKind k = kOptionInfo[id].kind;
    return k == OptKind::Bool || k == OptKind::Enum || k == OptKind::Int;
}

std::string settingKey(int id) {
    if (id < 0 || id >= OPT_COUNT) {
        return std::string();
    }
    std::string key = "COS_RANDO_";
    for (const char* p = kOptionInfo[id].name; *p != '\0'; p++) {
        key += (char)std::toupper((unsigned char)*p);
    }
    return key;
}

const char* const* enumTokens(int id, int* count) {
    *count = 0;
    switch (id) {
    case OPT_sword_mode:
        *count = 3;
        return kSwordTokens;
    case OPT_mix_entrances:
        *count = 2;
        return kMixTokens;
    case OPT_logic_obscurity:
    case OPT_logic_precision:
        *count = 4;
        return kTrickTokens;
    default:
        return nullptr;
    }
}

std::string settingValue(const Options& o, int id) {
    if (id < 0 || id >= OPT_COUNT) {
        return std::string();
    }
    const OptionInfo& info = kOptionInfo[id];
    const int v = optionGetInt(o, id);
    switch (info.kind) {
    case OptKind::Bool:
        return v ? "1" : "0";
    case OptKind::Enum: {
        int n = 0;
        const char* const* t = enumTokens(id, &n);
        return t != nullptr && v >= 0 && v < n ? t[v] : std::string();
    }
    case OptKind::Int:
        return std::to_string(v);
    default:
        return std::string();
    }
}

bool settingParse(Options& o, int id, const char* text) {
    if (id < 0 || id >= OPT_COUNT || text == nullptr) {
        return false;
    }
    const OptionInfo& info = kOptionInfo[id];
    switch (info.kind) {
    case OptKind::Bool:
        if (std::strcmp(text, "0") == 0 || std::strcmp(text, "1") == 0) {
            return optionSetInt(o, id, text[0] == '1');
        }
        return false;
    case OptKind::Enum: {
        int n = 0;
        const char* const* t = enumTokens(id, &n);
        for (int i = 0; i < n; i++) {
            if (std::strcmp(text, t[i]) == 0) {
                return optionSetInt(o, id, i);
            }
        }
        return false;
    }
    case OptKind::Int: {
        int v = 0;
        if (!parseInt(text, &v) || v < info.minimum || v > info.maximum) {
            return false;
        }
        return optionSetInt(o, id, v);
    }
    default:
        return false;
    }
}

std::string gearValue(const Options& o) {
    std::vector<std::string> v = o.starting_gear;
    std::sort(v.begin(), v.end());
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        if (i != 0) {
            s += '|';
        }
        s += v[i];
    }
    return s;
}

std::string gearStored(const Options& o) {
    const std::string v = gearValue(o);
    return v.empty() ? "-" : v;
}

namespace {

// Sets starting_gear to `start` (each name at most its INVENTORY_ITEMS count) and randomized_gear to
// the rest; both sorted. False if a name was dropped.
bool setGear(Options& o, const std::vector<std::string>& start) {
    std::vector<std::string> rest = inventoryItems();
    std::vector<std::string> kept;
    bool ok = true;
    for (const std::string& item : start) {
        auto it = std::find(rest.begin(), rest.end(), item);
        if (it == rest.end()) {
            ok = false;
            continue;
        }
        rest.erase(it);
        kept.push_back(item);
    }
    std::sort(kept.begin(), kept.end());
    std::sort(rest.begin(), rest.end());
    o.starting_gear = kept;
    o.randomized_gear = rest;
    return ok;
}

} // namespace

bool gearParse(Options& o, const char* text) {
    std::vector<std::string> start;
    if (text != nullptr && *text != '\0') {
        const char* p = text;
        for (;;) {
            const char* bar = std::strchr(p, '|');
            std::string item = bar != nullptr ? std::string(p, (size_t)(bar - p)) : std::string(p);
            if (!item.empty()) {
                start.push_back(item);
            }
            if (bar == nullptr) {
                break;
            }
            p = bar + 1;
        }
    }
    return setGear(o, start);
}

Options optionsFromSettings(SettingGetter get, void* user) {
    Options o;
    for (int id = 0; id < OPT_COUNT; id++) {
        if (!settingExposed(id)) {
            continue;
        }
        const std::string key = settingKey(id);
        const std::string v = get(key.c_str(), user);
        if (!v.empty()) {
            settingParse(o, id, v.c_str()); // a bad value keeps the default
        }
    }
    // An unset key is the default gear; a stored empty value cannot be told from unset, so "no
    // starting gear" is stored as "-".
    const std::string gear = get(kStartingGearKey, user);
    if (!gear.empty()) {
        gearParse(o, gear == "-" ? "" : gear.c_str());
    }
    return o;
}

bool settingsEqual(const Options& a, const Options& b) {
    for (int id = 0; id < OPT_COUNT; id++) {
        if (settingExposed(id) && optionGetInt(a, id) != optionGetInt(b, id)) {
            return false;
        }
    }
    return gearValue(a) == gearValue(b);
}

bool isCategory(int id) {
    return id >= 0 && id < OPT_COUNT && std::strncmp(kOptionInfo[id].name, "progression_", 12) == 0;
}

const char* presetName(int preset, bool spanish) {
    switch (preset) {
    case kPresetDefaults:
        return spanish ? "Valores de wwrando" : "wwrando defaults";
    case kPresetAllChecks:
        return spanish ? "Todas las ubicaciones" : "All checks";
    case kPresetShort:
        return spanish ? "Corta (mazmorras y regalos)" : "Short (dungeons and gifts)";
    default:
        return spanish ? "Personalizado" : "Custom";
    }
}

Options presetOptions(int preset) {
    Options o;
    if (preset == kPresetAllChecks || preset == kPresetShort) {
        for (int id = 0; id < OPT_COUNT; id++) {
            if (isCategory(id)) {
                const bool on = preset == kPresetAllChecks || id == OPT_progression_dungeons ||
                                id == OPT_progression_free_gifts;
                optionSetInt(o, id, on ? 1 : 0);
            }
        }
    }
    return o;
}

int presetMatch(const Options& o) {
    for (int p = 0; p < kPresetCount; p++) {
        if (settingsEqual(o, presetOptions(p))) {
            return p;
        }
    }
    return -1;
}

int gearItemCount() {
    return kRegularItemCount + kProgressiveItemCount;
}

GearItem gearItem(int index) {
    if (index >= 0 && index < kRegularItemCount) {
        return {kRegularItems[index], 1};
    }
    index -= kRegularItemCount;
    if (index >= 0 && index < kProgressiveItemCount) {
        return {kProgressiveItems[index], kProgressiveItemCounts[index]};
    }
    return {"", 0};
}

int gearCount(const Options& o, const char* name) {
    return (int)std::count(o.starting_gear.begin(), o.starting_gear.end(), std::string(name));
}

void gearSetCount(Options& o, const char* name, int count) {
    int max = 0;
    for (int i = 0; i < gearItemCount(); i++) {
        if (std::strcmp(gearItem(i).name, name) == 0) {
            max = gearItem(i).max;
        }
    }
    count = std::max(0, std::min(count, max));
    std::vector<std::string> start;
    for (const std::string& item : o.starting_gear) {
        if (item != name) {
            start.push_back(item);
        }
    }
    for (int i = 0; i < count; i++) {
        start.emplace_back(name);
    }
    setGear(o, start);
}

bool gearEnabled(const Options& o, const char* name) {
    if (std::strcmp(name, "Progressive Sword") == 0) {
        return o.sword_mode == SwordMode::StartWithSword;
    }
    if (std::strcmp(name, "Hurricane Spin") == 0) {
        return o.sword_mode != SwordMode::Swordless;
    }
    return true;
}

} // namespace pcr
