// The randomizer's settings and its options-menu tab (docs/RANDOMIZER_PLAN.md step R4.1).
//
// Every wwrando option the port uses is a setting of pc_settings.h named COS_RANDO_<NAME> (the
// option's name in capitals: COS_RANDO_PROGRESSION_DUNGEONS, COS_RANDO_SWORD_MODE, ...), saved in
// settings.ini like any other row and shown on the menu's "Randomizer" tab (PC_SETTING_TAB_RANDO).
// The starting gear is one setting, COS_RANDO_STARTING_GEAR: the starting items joined by '|', in
// Python's sorted() order, or "-" for none (randomized_gear is always the rest of wwrando's
// INVENTORY_ITEMS, so it has no key of its own). Stored values:
//   bool   "0" / "1"
//   enum   a token (enumTokens: sword_mode "sword" / "no_sword" / "swordless", ...)
//   int    decimal, inside the option's [min, max]
// A missing or bad value is the option's wwrando default.
//
// randomize_enemies has a key since R7.4d (D18: experimental, "can be unbeatable"; the menu validates
// with ensureValidCombination(o, allowUnbeatable = true)).
// Options with no key (they keep their default; none of them is in the permalink):
//   dry_run, the custom player model options
//   (custom_player_model, player_in_casual_clothes, disable_custom_player_voice,
//   disable_custom_player_items, custom_color_preset, custom_colors: no custom models in the port),
//   invert_camera_x_axis (the port's own Gráficos row COS_CAMERA_INVERT_X does it),
//   remove_title_and_ending_videos (the port plays no THP video).
//
// Two layers:
//   - namespace pcr, pc_rando_presets.cpp: pure (cos_rando_core, unit test test_R4_1.cpp): keys,
//     value text, presets, the starting gear editor's model;
//   - namespace pc, pc_rando_settings.cpp: the pc_settings registration and the tab's rows, which
//     pc_menu.cpp draws and drives (RowKind::Rando). Not callable from the unit tests (it needs
//     pc_settings.cpp).
#pragma once

#include "pc_rando_options.h"

#include <string>
#include <vector>

namespace pcr {

// The option has a COS_RANDO_ key (see above).
bool settingExposed(int id);
// "COS_RANDO_" + the option's name in capitals.
std::string settingKey(int id);
extern const char* const kStartingGearKey; // "COS_RANDO_STARTING_GEAR"

// An enum option's stored tokens, in the enum's order; nullptr (and *count 0) for other options.
const char* const* enumTokens(int id, int* count);
// The stored text of a Bool/Enum/Int option.
std::string settingValue(const Options& o, int id);
// Parses settingValue's text (an int outside [min, max] is refused); false (o unchanged) on a bad
// value or an option of another kind.
bool settingParse(Options& o, int id, const char* text);

// Sorted starting_gear joined by '|' ("" for none; stored as "-": gearStored).
std::string gearValue(const Options& o);
std::string gearStored(const Options& o);
// Sets starting_gear from gearValue's text and randomized_gear to the rest of INVENTORY_ITEMS.
// Unknown names and copies beyond an item's count in INVENTORY_ITEMS are dropped. "" is no
// starting gear. False if anything was dropped (o still gets the rest).
bool gearParse(Options& o, const char* text);

// The settings as Options: get(key) returns the stored text, or "" when there is none. Missing and
// bad values keep wwrando's defaults. No ensureValidCombination (the caller applies it).
using SettingGetter = std::string (*)(const char* key, void* user);
Options optionsFromSettings(SettingGetter get, void* user);
// Every exposed option and the starting gear are equal.
bool settingsEqual(const Options& a, const Options& b);

// The 23 progression_* categories.
bool isCategory(int id);

// Presets of the tab's first row.
enum Preset : int {
    kPresetDefaults = 0, // wwrando's defaults (Options())
    kPresetAllChecks,    // the defaults with every category on
    kPresetShort,        // the defaults with only dungeons and free gifts as categories
    kPresetCount
};
const char* presetName(int preset, bool spanish);
Options presetOptions(int preset);
// The preset the options equal (settingsEqual), or -1.
int presetMatch(const Options& o);

// The starting gear editor: one row per item of wwrando's inventory (the 56 regular items, then
// the 8 progressive ones), each with a count 0..max (1 for a regular item).
struct GearItem {
    const char* name;
    int max;
};
int gearItemCount(); // 64
GearItem gearItem(int index);
// Copies of the item in starting_gear.
int gearCount(const Options& o, const char* name);
// Puts `count` copies (clamped to 0..max) in starting_gear and the rest in randomized_gear; both
// stay sorted.
void gearSetCount(Options& o, const char* name, int count);
// Whether the item can start in the inventory with this sword mode (ensureValidCombination's
// filter: no Progressive Sword without "Start with Hero's Sword", no Hurricane Spin when
// Swordless).
bool gearEnabled(const Options& o, const char* name);

} // namespace pcr

namespace pc {

// Registers every COS_RANDO_* setting (pc_settings_register, tab PC_SETTING_TAB_RANDO). Called by
// pc_menu.cpp's menuInit; again is a no-op.
void randoSettingsRegister();
// The current settings as Options; validated: ensureValidCombination applied, as a seed must be
// generated with (R4.3).
pcr::Options randoSettingsOptions(bool validated = true);
// Writes every exposed option and the starting gear whose stored value differs (one file write per
// changed key). Returns how many keys were locked by the environment and kept.
int randoSettingsApply(const pcr::Options& o);

// One row of the "Randomizer" tab. id: the option (Option), the gear item (Gear), the action
// (Action); unused otherwise.
enum class RandoRowKind { Notice, Preset, Option, GearHeader, Gear, Action };
struct RandoRow {
    RandoRowKind kind;
    int id = -1;
};
// The rows of both pages in order (the gear rows only while the header is expanded; actions whose
// shown() is false left out); re-reads the settings.
std::vector<RandoRow> randoMenuRows();
// The index-th row of the last randoMenuRows (no re-read: the menu asks per row, many times a frame).
RandoRow randoMenuRowAt(int index);
std::string randoRowLabel(const RandoRow& r, bool spanish);
std::string randoRowValue(const RandoRow& r, bool spanish);
std::string randoRowHelp(const RandoRow& r, bool spanish);
// Empty when the row can be changed, else why not (greyed out): another option it depends on, the
// environment, the region (D16).
std::string randoRowDisabled(const RandoRow& r, bool spanish);
// Left/right change the value (A cycles forward too); false: A only (an action, the gear header).
bool randoRowCycles(const RandoRow& r);
// Shows a value only: a notice, or an action row without activate (R8.2's rows about the game being
// played). The menu draws no "< >" around its value.
bool randoRowReadOnly(const RandoRow& r);
// A (dir 0 on an action row; 1 on a cycling row) or left/right (-1 / 1). Returns a toast ("" none).
std::string randoRowActivate(const RandoRow& r, int dir, bool spanish);
// The COS_RANDO_* key of an Option row ("" for the others).
std::string randoRowKey(const RandoRow& r);

// Action rows for later steps (R4.3: "Seed...", "Random seed", "Generate", ...), in `order`. The
// strings must outlive the program. value/activate/shown may be null (shown null: always shown).
//
// R8.2: the tab has two pages (pc_menu.cpp keeps the page; randoMenuPageRows). The order range
// places an action:
//   order < 0                    page 0 (kRandoPageGame), about the game being played: the current
//                                file's seed, hash and spoiler log, and the "Next game options..."
//                                row that opens page 1;
//   0 <= order < kRandoOrderLate page 1 (kRandoPageNext), before the preset and the options (R4.3's
//                                Seed..., Random seed, Paste permalink);
//   order >= kRandoOrderLate     page 1, after the options (Generate, R8.1's Ask on New Game at 90,
//                                R4.4's Clean unused seed files at 150).
// The preset, option and starting gear rows are on page 1; the region notice (D16) on both.
struct RandoMenuAction {
    const char* labelEs;
    const char* labelEn;
    const char* helpEs;
    const char* helpEn;
    std::string (*value)(bool spanish);
    std::string (*activate)(bool spanish); // A pressed; returns a toast
    int order;
    bool (*shown)() = nullptr; // R8.2: false hides the row (e.g. the spoiler row without a randomized file)
};
void randoMenuAddAction(const RandoMenuAction& action);

// R8.2: the tab's pages (see RandoMenuAction).
constexpr int kRandoPageGame = 0;
constexpr int kRandoPageNext = 1;
constexpr int kRandoOrderLate = 50;
// The page of a row; -1: every page (the region notice).
int randoRowPage(const RandoRow& r);
// The indices into randoMenuRows() of the rows on that page, in order (re-reads the rows).
std::vector<int> randoMenuPageRows(int page);

// Smokes: the menu reads the COS_INPUT script (as in COS_SMOKE=options-menu) while this is set.
void randoMenuSetScripted(bool on);
bool randoMenuScripted();

} // namespace pc
