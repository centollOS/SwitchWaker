// pc_rando_data.cpp - wwrando's logic data tables (docs/RANDOMIZER_PLAN.md, step R0.3).
// The data lives in generated/*.inc (native/tools/rando/gen_rando_data.py); this file only wraps it.
// Pure: no game headers, no allocation (pc_rando_data.h).
#include "pc_rando_data.h"

#include <string.h>

namespace pcr {
namespace gen {
#include "generated/rando_items.inc"
#include "generated/rando_locations.inc"
#include "generated/rando_macros.inc"
#include "generated/rando_tables.inc"
} // namespace gen

namespace {

template <class T, size_t N> constexpr Span<T> span(const T (&a)[N]) { return Span<T>{a, N}; }

template <class T, class F> int find(Span<T> s, const char* name, F key) {
    if (name == nullptr) {
        return -1;
    }
    for (size_t i = 0; i < s.size; i++) {
        if (strcmp(key(s[i]), name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int findStr(StrSpan s, const char* name) {
    return find(s, name, [](const char* v) { return v; });
}

static_assert(sizeof(gen::kLocTypeNames) / sizeof(gen::kLocTypeNames[0]) == kNumLocTypes,
              "pc_rando_data.h LocType is out of date with item_locations.txt's Types tags");

} // namespace

Span<Location> locations() { return span(gen::kLocations); }
Span<LocPath> loc_paths() { return span(gen::kLocPaths); }
Span<LocPath> location_paths(const Location& loc) {
    return Span<LocPath>{gen::kLocPaths + loc.first_path, loc.num_paths};
}
StrSpan loc_type_names() { return span(gen::kLocTypeNames); }
int location_index(const char* name) {
    return find(locations(), name, [](const Location& l) { return l.name; });
}
uint32_t loc_type_bit(const char* tag) {
    int i = findStr(loc_type_names(), tag);
    return i < 0 ? 0u : 1u << i;
}

Span<Macro> macros() { return span(gen::kMacros); }
int macro_index(const char* name) {
    return find(macros(), name, [](const Macro& m) { return m.name; });
}

Span<ItemName> item_names() { return span(gen::kItemNames); }
int item_id(const char* name) {
    int i = find(item_names(), name, [](const ItemName& n) { return n.name; });
    return i < 0 ? -1 : gen::kItemNames[i].id;
}
const char* item_name(uint8_t id) {
    for (const ItemName& n : item_names()) {
        if (n.id == id) {
            return n.name;
        }
    }
    return nullptr;
}

StrSpan progress_items() { return span(gen::kProgressItems); }
StrSpan nonprogress_items() { return span(gen::kNonprogressItems); }
StrSpan consumable_items() { return span(gen::kConsumableItems); }
StrSpan duplicatable_consumable_items() { return span(gen::kDuplicatableConsumableItems); }
StrSpan dungeon_progress_items() { return span(gen::kDungeonProgressItems); }
StrSpan dungeon_nonprogress_items() { return span(gen::kDungeonNonprogressItems); }
StrSpan triforce_chart_names() { return span(gen::kTriforceChartNames); }
StrSpan treasure_chart_names() { return span(gen::kTreasureChartNames); }
StrSpan logic_item_names() { return span(gen::kLogicItemNames); }
size_t default_logic_item_count() { return gen::kNumDefaultLogicItemNames; }
int logic_item_index(const char* name) { return findStr(logic_item_names(), name); }

StrSpan inventory_regular_items() { return span(gen::kInventoryRegularItems); }
StrSpan inventory_progressive_items() { return span(gen::kInventoryProgressiveItems); }
StrSpan default_starting_items() { return span(gen::kDefaultStartingItems); }

Span<Island> islands() { return span(gen::kIslands); }
const char* island_name(int number) {
    for (const Island& i : islands()) {
        if (i.number == number) {
            return i.name;
        }
    }
    return nullptr;
}

Span<IslandChart> default_charts() { return span(gen::kDefaultCharts); }
const char* default_chart_for_island(int island) {
    for (const IslandChart& c : default_charts()) {
        if (c.island == island) {
            return c.chart;
        }
    }
    return nullptr;
}

StrSpan rng_changing_options() { return span(gen::kRngChangingOptions); }
Span<Boss> bosses() { return span(gen::kBosses); }
Span<DungeonName> dungeon_names() { return span(gen::kDungeonNames); }
StrSpan seed_adjectives() { return span(gen::kSeedAdjectives); }
StrSpan seed_nouns() { return span(gen::kSeedNouns); }
StrSpan seed_names() { return span(gen::kSeedNames); }
Span<NameText> progress_item_hints() { return span(gen::kProgressItemHints); }
Span<NameText> zone_name_hints() { return span(gen::kZoneNameHints); }
Span<LocationHint> location_hints() { return span(gen::kLocationHints); }
Span<NameText> exit_overrides() { return span(gen::kExitOverrides); }
const char* data_source_commit() { return gen::kSourceCommit; }

} // namespace pcr
