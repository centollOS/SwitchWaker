// The randomizer's option table, defaults, validation and UI rules (docs/RANDOMIZER_PLAN.md step R1.2;
// pc_rando_options.h). Pure C++: no game headers or calls (cos_rando_core).
#include "pc_rando_options.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace pcr {

const char* const kWwrandoVersion = "1.10.0_9775811";
const char* const kWwrandoVersionNoCommit = "1.10.0";

// wwr_ui/inventory.py REGULAR_ITEMS (+ DUNGEON_NONPROGRESS_ITEMS), sorted as Python sorts str.
const char* const kRegularItems[] = {
    "Bait Bag",
    "Ballad of Gales",
    "Beedle's Chart",
    "Bombs",
    "Boomerang",
    "Cabana Deed",
    "Command Melody",
    "DRC Compass",
    "DRC Dungeon Map",
    "Deku Leaf",
    "Delivery Bag",
    "Din's Pearl",
    "Dragon Tingle Statue",
    "ET Compass",
    "ET Dungeon Map",
    "Earth God's Lyric",
    "Earth Tingle Statue",
    "Empty Bottle",
    "FF Compass",
    "FF Dungeon Map",
    "FW Compass",
    "FW Dungeon Map",
    "Farore's Pearl",
    "Fill-Up Coupon",
    "Forbidden Tingle Statue",
    "Ghost Ship Chart",
    "Goddess Tingle Statue",
    "Grappling Hook",
    "Great Fairy Chart",
    "Hero's Charm",
    "Hookshot",
    "Hurricane Spin",
    "Iron Boots",
    "Light Ring Chart",
    "Maggie's Letter",
    "Magic Armor",
    "Moblin's Letter",
    "Nayru's Pearl",
    "Note to Mom",
    "Octo Chart",
    "Platform Chart",
    "Power Bracelets",
    "Secret Cave Chart",
    "Skull Hammer",
    "Song of Passing",
    "Spoils Bag",
    "Submarine Chart",
    "Telescope",
    "Tingle Tuner",
    "Tingle's Chart",
    "TotG Compass",
    "TotG Dungeon Map",
    "WT Compass",
    "WT Dungeon Map",
    "Wind God's Aria",
    "Wind Tingle Statue",
};
const int kRegularItemCount = (int)(sizeof kRegularItems / sizeof kRegularItems[0]);
static_assert(sizeof kRegularItems / sizeof kRegularItems[0] == 56, "REGULAR_ITEMS has 56 items");

// sorted(set(PROGRESSIVE_ITEMS)) and each one's count in PROGRESSIVE_ITEMS.
const char* const kProgressiveItems[] = {
    "Progressive Bomb Bag", "Progressive Bow",   "Progressive Magic Meter", "Progressive Picto Box",
    "Progressive Quiver",   "Progressive Shield", "Progressive Sword",      "Progressive Wallet",
};
const int kProgressiveItemCounts[] = {2, 3, 2, 2, 2, 2, 3, 2};
const int kProgressiveItemCount = (int)(sizeof kProgressiveItems / sizeof kProgressiveItems[0]);
static_assert(sizeof kProgressiveItems / sizeof kProgressiveItems[0] == 8, "8 progressive items");

namespace {

// DEFAULT_STARTING_ITEMS
const char* const kDefaultStartingItems[] = {
    "Progressive Shield",
    "Progressive Magic Meter",
    "Ballad of Gales",
    "Song of Passing",
};

const char* const kSwordModeNames[] = {"Start with Hero's Sword", "No Starting Sword", "Swordless"};
const char* const kEntranceMixModeNames[] = {"Separate Dungeons From Caves & Fountains",
                                             "Mix Dungeons & Caves & Fountains"};
const char* const kTrickDifficultyNames[] = {"None", "Normal", "Hard", "Very Hard"};

template <class T> struct EnumNames;
template <> struct EnumNames<SwordMode> {
    static constexpr const char* const* names = kSwordModeNames;
    static constexpr int count = 3;
};
template <> struct EnumNames<EntranceMixMode> {
    static constexpr const char* const* names = kEntranceMixModeNames;
    static constexpr int count = 2;
};
template <> struct EnumNames<TrickDifficulty> {
    static constexpr const char* const* names = kTrickDifficultyNames;
    static constexpr int count = 4;
};

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

std::string joinList(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        if (i != 0) {
            s += '|';
        }
        s += v[i];
    }
    return s;
}

std::vector<std::string> splitList(const char* text) {
    std::vector<std::string> v;
    if (*text == '\0') {
        return v;
    }
    const char* p = text;
    for (;;) {
        const char* bar = std::strchr(p, '|');
        if (bar == nullptr) {
            v.emplace_back(p);
            break;
        }
        v.emplace_back(p, (size_t)(bar - p));
        p = bar + 1;
    }
    return v;
}

void sortList(std::vector<std::string>& v) { std::sort(v.begin(), v.end()); }

// list.remove(x): erases the first occurrence; false if absent.
bool removeFirst(std::vector<std::string>& v, const std::string& x) {
    auto it = std::find(v.begin(), v.end(), x);
    if (it == v.end()) {
        return false;
    }
    v.erase(it);
    return true;
}

} // namespace

std::vector<std::string> inventoryItems() {
    std::vector<std::string> v(kRegularItems, kRegularItems + kRegularItemCount);
    for (int i = 0; i < kProgressiveItemCount; i++) {
        for (int n = 0; n < kProgressiveItemCounts[i]; n++) {
            v.emplace_back(kProgressiveItems[i]);
        }
    }
    return v;
}

std::vector<std::string> defaultStartingGear() {
    std::vector<std::string> v(kDefaultStartingItems, kDefaultStartingItems + 4);
    sortList(v);
    return v;
}

std::vector<std::string> defaultRandomizedGear() {
    std::vector<std::string> v = inventoryItems();
    for (const char* item : kDefaultStartingItems) {
        removeFirst(v, item);
    }
    sortList(v);
    return v;
}

Options::Options() : randomized_gear(defaultRandomizedGear()), starting_gear(defaultStartingGear()) {}

const OptionInfo kOptionInfo[OPT_COUNT] = {
#define PCR_IB(n, d, f) {#n, OptKind::Bool, (uint8_t)(f), 0, 0, (d) ? 1 : 0, nullptr, 0},
#define PCR_IE(n, T, d, f)                                                                              \
    {#n, OptKind::Enum, (uint8_t)(f), 0, 0, (int)(d), EnumNames<T>::names, EnumNames<T>::count},
#define PCR_II(n, d, lo, hi, f) {#n, OptKind::Int, (uint8_t)(f), lo, hi, d, nullptr, 0},
#define PCR_IS(n, d, f) {#n, OptKind::Str, (uint8_t)(f), 0, 0, 0, nullptr, 0},
#define PCR_IL(n, f) {#n, OptKind::List, (uint8_t)(f), 0, 0, 0, nullptr, 0},
#define PCR_ID(n, f) {#n, OptKind::Dict, (uint8_t)(f), 0, 0, 0, nullptr, 0},
    PCR_OPTIONS(PCR_IB, PCR_IE, PCR_II, PCR_IS, PCR_IL, PCR_ID)
#undef PCR_IB
#undef PCR_IE
#undef PCR_II
#undef PCR_IS
#undef PCR_IL
#undef PCR_ID
};

int optionFind(const char* name) {
    if (name == nullptr) {
        return -1;
    }
    for (int i = 0; i < OPT_COUNT; i++) {
        if (std::strcmp(kOptionInfo[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int optionGetInt(const Options& o, int id) {
    switch (id) {
#define PCR_GB(n, d, f)                                                                                 \
    case OPT_##n:                                                                                       \
        return o.n ? 1 : 0;
#define PCR_GE(n, T, d, f)                                                                              \
    case OPT_##n:                                                                                       \
        return (int)o.n;
#define PCR_GI(n, d, lo, hi, f)                                                                         \
    case OPT_##n:                                                                                       \
        return o.n;
#define PCR_GX(n, ...)
        PCR_OPTIONS(PCR_GB, PCR_GE, PCR_GI, PCR_GX, PCR_GX, PCR_GX)
#undef PCR_GB
#undef PCR_GE
#undef PCR_GI
    default:
        return 0;
    }
}

bool optionSetInt(Options& o, int id, int value) {
    switch (id) {
#define PCR_SB(n, d, f)                                                                                 \
    case OPT_##n:                                                                                       \
        o.n = value != 0;                                                                               \
        return true;
#define PCR_SE(n, T, d, f)                                                                              \
    case OPT_##n:                                                                                       \
        if (value < 0 || value >= EnumNames<T>::count) {                                                \
            return false;                                                                               \
        }                                                                                               \
        o.n = (T)value;                                                                                 \
        return true;
#define PCR_SI(n, d, lo, hi, f)                                                                         \
    case OPT_##n:                                                                                       \
        o.n = value;                                                                                    \
        return true;
        PCR_OPTIONS(PCR_SB, PCR_SE, PCR_SI, PCR_GX, PCR_GX, PCR_GX)
#undef PCR_SB
#undef PCR_SE
#undef PCR_SI
    default:
        return false;
    }
}

namespace {

std::string* strField(Options& o, int id) {
    switch (id) {
#define PCR_FS(n, d, f)                                                                                 \
    case OPT_##n:                                                                                       \
        return &o.n;
#define PCR_FD(n, f)                                                                                    \
    case OPT_##n:                                                                                       \
        return &o.n;
        PCR_OPTIONS(PCR_GX, PCR_GX, PCR_GX, PCR_FS, PCR_GX, PCR_FD)
#undef PCR_FS
#undef PCR_FD
    default:
        return nullptr;
    }
}

std::vector<std::string>* listField(Options& o, int id) {
    switch (id) {
#define PCR_FL(n, f)                                                                                    \
    case OPT_##n:                                                                                       \
        return &o.n;
        PCR_OPTIONS(PCR_GX, PCR_GX, PCR_GX, PCR_GX, PCR_FL, PCR_GX)
#undef PCR_FL
    default:
        return nullptr;
    }
}
#undef PCR_GX

} // namespace

std::string optionToString(const Options& o, int id) {
    if (id < 0 || id >= OPT_COUNT) {
        return std::string();
    }
    const OptionInfo& info = kOptionInfo[id];
    Options& m = const_cast<Options&>(o); // the field getters are shared; nothing is written
    switch (info.kind) {
    case OptKind::Bool:
        return optionGetInt(o, id) ? "1" : "0";
    case OptKind::Enum:
        return info.choices[optionGetInt(o, id)];
    case OptKind::Int:
        return std::to_string(optionGetInt(o, id));
    case OptKind::Str:
    case OptKind::Dict:
        return *strField(m, id);
    case OptKind::List:
        return joinList(*listField(m, id));
    }
    return std::string();
}

bool optionFromString(Options& o, int id, const char* text) {
    if (id < 0 || id >= OPT_COUNT || text == nullptr) {
        return false;
    }
    const OptionInfo& info = kOptionInfo[id];
    switch (info.kind) {
    case OptKind::Bool:
        if (std::strcmp(text, "1") == 0 || std::strcmp(text, "true") == 0) {
            return optionSetInt(o, id, 1);
        }
        if (std::strcmp(text, "0") == 0 || std::strcmp(text, "false") == 0) {
            return optionSetInt(o, id, 0);
        }
        return false;
    case OptKind::Enum:
        for (int i = 0; i < info.numChoices; i++) {
            if (std::strcmp(text, info.choices[i]) == 0) {
                return optionSetInt(o, id, i);
            }
        }
        return false;
    case OptKind::Int: {
        int v = 0;
        return parseInt(text, &v) && optionSetInt(o, id, v);
    }
    case OptKind::Str:
    case OptKind::Dict:
        *strField(o, id) = text;
        return true;
    case OptKind::List:
        *listField(o, id) = splitList(text);
        return true;
    }
    return false;
}

bool optionsEqual(const Options& a, const Options& b) {
    for (int i = 0; i < OPT_COUNT; i++) {
        if (optionToString(a, i) != optionToString(b, i)) {
            return false;
        }
    }
    return true;
}

void validateOptions(Options& o) {
    for (int i = 0; i < OPT_COUNT; i++) {
        const OptionInfo& info = kOptionInfo[i];
        if (info.kind != OptKind::Int) {
            continue;
        }
        const int v = optionGetInt(o, i);
        if (v < info.minimum || v > info.maximum) {
            optionSetInt(o, i, info.def);
        }
    }
}

bool optionEnabled(const Options& o, int id) {
    switch (id) {
    case OPT_required_bosses:
        return o.progression_dungeons;
    case OPT_num_required_bosses:
        return o.required_bosses;
    case OPT_prioritize_remote_hints:
        return o.num_location_hints != 0;
    case OPT_mix_entrances: {
        const bool dungeon = o.randomize_dungeon_entrances || o.randomize_miniboss_entrances ||
                             o.randomize_boss_entrances;
        const bool nonDungeon = o.randomize_secret_cave_entrances ||
                                o.randomize_secret_cave_inner_entrances ||
                                o.randomize_fairy_fountain_entrances;
        return dungeon && nonDungeon;
    }
    default:
        return true;
    }
}

void ensureValidCombination(Options& o, bool allowUnbeatable) {
    // The enabled states come from the values before any change, as in wwrando (it reads every
    // widget first).
    bool enabled[OPT_COUNT];
    for (int i = 0; i < OPT_COUNT; i++) {
        enabled[i] = optionEnabled(o, i);
    }

    std::vector<std::string> filterOut;
    if (o.sword_mode == SwordMode::Swordless) {
        filterOut.emplace_back("Hurricane Spin");
    }
    if (o.sword_mode == SwordMode::Swordless || o.sword_mode == SwordMode::NoStartingSword) {
        for (int i = 0; i < 3; i++) {
            filterOut.emplace_back("Progressive Sword");
        }
    }
    // get_option_value returns the list models sorted.
    sortList(o.starting_gear);
    sortList(o.randomized_gear);
    for (const std::string& item : filterOut) {
        if (!removeFirst(o.randomized_gear, item)) {
            removeFirst(o.starting_gear, item);
        }
    }
    o.randomized_gear.insert(o.randomized_gear.end(), filterOut.begin(), filterOut.end());
    sortList(o.starting_gear);
    sortList(o.randomized_gear);

    std::vector<std::string> all = o.starting_gear;
    all.insert(all.end(), o.randomized_gear.begin(), o.randomized_gear.end());
    sortList(all);
    std::vector<std::string> inv = inventoryItems();
    sortList(inv);
    if (all != inv) {
        o.starting_gear = defaultStartingGear();
        o.randomized_gear = defaultRandomizedGear();
    }

    // A disabled check box is unchecked; spin boxes and combo boxes keep their value.
    for (int i = 0; i < OPT_COUNT; i++) {
        if (!enabled[i] && kOptionInfo[i].kind == OptKind::Bool) {
            optionSetInt(o, i, 0);
        }
        if ((kOptionInfo[i].flags & kOptUnbeatable) != 0 && !allowUnbeatable) {
            optionSetInt(o, i, 0);
        }
    }
}

} // namespace pcr
