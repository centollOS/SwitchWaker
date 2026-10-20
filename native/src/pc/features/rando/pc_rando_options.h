// The randomizer's options, permalinks and seed names (docs/RANDOMIZER_PLAN.md step R1.2): a port of
// wwrando 1.10.0_9775811's options/wwrando_options.py, WWRandomizer.{sanitize_seed, encode_permalink,
// decode_permalink, get_seed_hash, convert_string_to_integer_md5}, packedbits.py, seedgen/seedgen.py,
// wwr_ui/inventory.py and wwr_ui/randomizer_window.py ensure_valid_combination_of_options.
//
// Pure C++ (no game headers, no game calls): the unit tests and the Mac CLI link it through
// cos_rando_core. Files: pc_rando_options.cpp (the option table, defaults, validation, the UI rules),
// pc_rando_permalink.cpp (base64 + packed bits), pc_rando_seedgen.cpp (md5, seed hash, random seed
// names; the word lists are generated/pc_rando_seedgen_words.inc).
//
// Compatibility (decision D1): a permalink made here is byte-identical to wwrando's for the same seed
// and options, and decodes wwrando's permalinks to the same options. The seed hash is wwrando's for
// the same permalink and SEED_KEY; for do_not_generate_spoiler_log seeds the port uses its own key
// (kSwitchWakerSeedKey, decision D2), so those hashes and seeds differ from official ones.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace pcr {

// wwrando's VERSION at the reference commit; permalinks embed it.
extern const char* const kWwrandoVersion; // "1.10.0_9775811"
// wwrando's VERSION without the commit suffix (decode tells a different commit from a different
// version, as WWRandomizer.decode_permalink does when running from source).
extern const char* const kWwrandoVersionNoCommit; // "1.10.0"

enum class OptKind : uint8_t { Bool, Enum, Int, Str, List, Dict };

// Option flags (base_options.option arguments).
enum : uint8_t {
    kOptPermalink = 1,  // in the permalink
    kOptHidden = 2,     // hidden in wwrando's UI unless set
    kOptUnbeatable = 4, // can make an unbeatable seed (wwrando turns it off unless run from source)
};

// StrEnum options: the value is the index into the enum's declaration order (the permalink index).
enum class SwordMode : uint8_t { StartWithSword, NoStartingSword, Swordless };
enum class EntranceMixMode : uint8_t { SeparateDungeons, MixDungeons };
enum class TrickDifficulty : uint8_t { None, Normal, Hard, VeryHard };

// Every option of wwrando's Options dataclass, in Options.all() order (77; 66 have permalink=True,
// randomized_gear among them although it takes no bits). The column macros:
//   B(name, default, flags)                bool
//   E(name, EnumType, default, flags)      StrEnum
//   I(name, default, min, max, flags)      int (min/max inclusive)
//   S(name, default, flags)                str
//   L(name, flags)                         list[str] (gear lists; defaults from inventory.py)
//   D(name, flags)                         dict (custom_colors; kept as JSON text, cosmetic only)
#define PCR_P ::pcr::kOptPermalink
#define PCR_OPTIONS(B, E, I, S, L, D)                                                                    \
    B(progression_dungeons, true, PCR_P)                                                                 \
    B(progression_tingle_chests, false, PCR_P)                                                           \
    B(progression_dungeon_secrets, false, PCR_P)                                                         \
    B(progression_puzzle_secret_caves, true, PCR_P)                                                      \
    B(progression_combat_secret_caves, false, PCR_P)                                                     \
    B(progression_savage_labyrinth, false, PCR_P)                                                        \
    B(progression_great_fairies, true, PCR_P)                                                            \
    B(progression_short_sidequests, false, PCR_P)                                                        \
    B(progression_long_sidequests, false, PCR_P)                                                         \
    B(progression_spoils_trading, false, PCR_P)                                                          \
    B(progression_minigames, false, PCR_P)                                                               \
    B(progression_battlesquid, false, PCR_P)                                                             \
    B(progression_free_gifts, true, PCR_P)                                                               \
    B(progression_mail, false, PCR_P)                                                                    \
    B(progression_platforms_rafts, false, PCR_P)                                                         \
    B(progression_submarines, false, PCR_P)                                                              \
    B(progression_eye_reef_chests, false, PCR_P)                                                         \
    B(progression_big_octos_gunboats, false, PCR_P)                                                      \
    B(progression_triforce_charts, false, PCR_P)                                                         \
    B(progression_treasure_charts, false, PCR_P)                                                         \
    B(progression_expensive_purchases, true, PCR_P)                                                      \
    B(progression_island_puzzles, false, PCR_P)                                                          \
    B(progression_misc, true, PCR_P)                                                                     \
    B(keylunacy, false, PCR_P)                                                                           \
    E(sword_mode, SwordMode, SwordMode::StartWithSword, PCR_P)                                           \
    B(required_bosses, false, PCR_P)                                                                     \
    I(num_required_bosses, 4, 1, 6, PCR_P)                                                               \
    B(chest_type_matches_contents, false, PCR_P)                                                         \
    B(trap_chests, false, PCR_P)                                                                         \
    B(hero_mode, false, PCR_P)                                                                           \
    E(logic_obscurity, TrickDifficulty, TrickDifficulty::None, PCR_P)                                    \
    E(logic_precision, TrickDifficulty, TrickDifficulty::None, PCR_P)                                    \
    B(randomize_dungeon_entrances, false, PCR_P)                                                         \
    B(randomize_secret_cave_entrances, false, PCR_P)                                                     \
    B(randomize_miniboss_entrances, false, PCR_P)                                                        \
    B(randomize_boss_entrances, false, PCR_P)                                                            \
    B(randomize_secret_cave_inner_entrances, false, PCR_P)                                               \
    B(randomize_fairy_fountain_entrances, false, PCR_P)                                                  \
    E(mix_entrances, EntranceMixMode, EntranceMixMode::SeparateDungeons, PCR_P)                          \
    B(randomize_enemies, false, PCR_P | ::pcr::kOptHidden | ::pcr::kOptUnbeatable)                       \
    B(randomize_enemy_palettes, false, PCR_P)                                                            \
    B(randomize_starting_island, false, PCR_P)                                                           \
    B(randomize_charts, false, PCR_P)                                                                    \
    B(hoho_hints, true, PCR_P)                                                                           \
    B(fishmen_hints, true, PCR_P)                                                                        \
    B(korl_hints, false, PCR_P)                                                                          \
    I(num_item_hints, 15, 0, 15, PCR_P)                                                                  \
    I(num_location_hints, 5, 0, 15, PCR_P)                                                               \
    I(num_barren_hints, 0, 0, 15, PCR_P)                                                                 \
    I(num_path_hints, 0, 0, 15, PCR_P)                                                                   \
    B(cryptic_hints, true, PCR_P)                                                                        \
    B(prioritize_remote_hints, false, PCR_P)                                                             \
    B(hint_importance, false, PCR_P)                                                                     \
    B(swift_sail, true, PCR_P)                                                                           \
    B(instant_text_boxes, true, PCR_P)                                                                   \
    B(reveal_full_sea_chart, true, PCR_P)                                                                \
    B(add_shortcut_warps_between_dungeons, false, PCR_P)                                                 \
    B(skip_rematch_bosses, true, PCR_P)                                                                  \
    B(invert_camera_x_axis, false, 0)                                                                    \
    B(invert_sea_compass_x_axis, false, 0)                                                               \
    B(remove_title_and_ending_videos, true, 0)                                                           \
    B(remove_music, false, PCR_P)                                                                        \
    B(switch_targeting_mode, false, 0)                                                                   \
    L(randomized_gear, PCR_P)                                                                            \
    L(starting_gear, PCR_P)                                                                              \
    I(num_starting_triforce_shards, 0, 0, 8, PCR_P)                                                      \
    I(starting_pohs, 0, 0, 44, PCR_P)                                                                    \
    I(starting_hcs, 3, 1, 9, PCR_P)                                                                      \
    I(num_extra_starting_items, 0, 0, 3, PCR_P)                                                          \
    S(custom_player_model, "Link", 0)                                                                    \
    B(player_in_casual_clothes, false, 0)                                                                \
    B(disable_custom_player_voice, false, 0)                                                             \
    B(disable_custom_player_items, false, 0)                                                             \
    S(custom_color_preset, "Default", 0)                                                                 \
    D(custom_colors, 0)                                                                                  \
    B(do_not_generate_spoiler_log, false, PCR_P)                                                         \
    B(dry_run, false, 0)

// OPT_<name>: the option's index in Options.all() order.
enum OptId : int {
#define PCR_ID1(n, ...) OPT_##n,
    PCR_OPTIONS(PCR_ID1, PCR_ID1, PCR_ID1, PCR_ID1, PCR_ID1, PCR_ID1)
#undef PCR_ID1
        OPT_COUNT
};
static_assert(OPT_COUNT == 77, "wwrando 1.10.0 has 77 options");

// Gear lists (wwr_ui/inventory.py). Item names are wwrando's, sorted like Python's sorted().
extern const char* const kRegularItems[];   // REGULAR_ITEMS: 56, sorted; one permalink bit each
extern const int kRegularItemCount;
extern const char* const kProgressiveItems[]; // sorted(set(PROGRESSIVE_ITEMS)): 8, 2 bits each
extern const int kProgressiveItemCounts[];    // PROGRESSIVE_ITEMS.count(item)
extern const int kProgressiveItemCount;
// INVENTORY_ITEMS (REGULAR_ITEMS + sorted PROGRESSIVE_ITEMS with repeats: 74 entries).
std::vector<std::string> inventoryItems();
// sorted(DEFAULT_STARTING_ITEMS) and sorted(DEFAULT_RANDOMIZED_ITEMS).
std::vector<std::string> defaultStartingGear();
std::vector<std::string> defaultRandomizedGear();

// wwrando's Options with every field at its default (Options()).
struct Options {
#define PCR_FB(n, d, f) bool n = d;
#define PCR_FE(n, T, d, f) T n = d;
#define PCR_FI(n, d, lo, hi, f) int n = d;
#define PCR_FS(n, d, f) std::string n = d;
#define PCR_FL(n, f) std::vector<std::string> n;
#define PCR_FD(n, f) std::string n = "{}"; /* JSON object text */
    PCR_OPTIONS(PCR_FB, PCR_FE, PCR_FI, PCR_FS, PCR_FL, PCR_FD)
#undef PCR_FB
#undef PCR_FE
#undef PCR_FI
#undef PCR_FS
#undef PCR_FL
#undef PCR_FD
    Options();
};

// Static description of one option.
struct OptionInfo {
    const char* name;
    OptKind kind;
    uint8_t flags;   // kOpt*
    int minimum;     // Int only
    int maximum;     // Int only
    int def;         // Bool/Enum/Int default (Enum: index)
    const char* const* choices; // Enum: the StrEnum string values, in declaration order
    int numChoices;
};
extern const OptionInfo kOptionInfo[OPT_COUNT];

// OPT_* for a wwrando option name, or -1.
int optionFind(const char* name);
// Bool/Enum/Int options as an int (bool 0/1, enum index); 0 for the other kinds.
int optionGetInt(const Options& o, int id);
// Sets a Bool/Enum/Int option (no range check: see validateOptions). False for the other kinds or
// an enum index out of range.
bool optionSetInt(Options& o, int id, int value);
// The value as text: bool "0"/"1", enum its string value ("Start with Hero's Sword"), int decimal,
// list items joined by '|', str as is, dict its JSON text (the R1.2 fixture format).
std::string optionToString(const Options& o, int id);
// Parses optionToString's format (bool also takes true/false); false on a bad value (o unchanged).
bool optionFromString(Options& o, int id, const char* text);
// Every option equal (lists element by element).
bool optionsEqual(const Options& a, const Options& b);

// Options.validate(): an int option outside [min, max] goes back to its default.
void validateOptions(Options& o);

// wwr_ui/randomizer_window.py ensure_valid_combination_of_options, as the menu must apply it before a
// seed is generated (wwrando's UI applies it on every change):
//   - required_bosses is unchecked without progression_dungeons;
//   - prioritize_remote_hints is unchecked when num_location_hints is 0;
//   - num_required_bosses and mix_entrances are only disabled (their value is kept, as Qt does for a
//     spin box and a combo box); see optionEnabled;
//   - Swordless filters out Hurricane Spin, Swordless and No Starting Sword the 3 Progressive Swords:
//     each is removed from randomized_gear, else from starting_gear, then all are appended to
//     randomized_gear; both lists are then sorted (the Qt list models sort);
//   - if starting_gear + randomized_gear is not INVENTORY_ITEMS as a multiset, both lists go back to
//     their defaults;
//   - an unbeatable option (randomize_enemies) is turned off unless allowUnbeatable (wwrando keeps it
//     only when running from source).
void ensureValidCombination(Options& o, bool allowUnbeatable = false);
// Whether ensure_valid_combination_of_options leaves the option's widget enabled.
bool optionEnabled(const Options& o, int id);

// ---------------------------------------------------------------------------------------------
// Permalinks (pc_rando_permalink.cpp)

// WWRandomizer.sanitize_seed: keeps - _ ' % . and ASCII letters and digits, at most 42 characters.
std::string sanitizeSeed(const std::string& seed);
constexpr int kMaxSeedLength = 42;

// WWRandomizer.encode_permalink: base64(VERSION \0 sanitized seed \0 packed option bits).
std::string encodePermalink(const std::string& seed, const Options& o);

enum class PermalinkError : uint8_t {
    None,
    Blank,        // empty after stripping whitespace
    BadBase64,    // not decodable base64 (bad padding)
    Malformed,    // fewer than two \0 separators, or a non-ASCII version/seed
    WrongVersion, // another wwrando version (PermalinkWrongVersionError)
    WrongCommit,  // same version, another commit (PermalinkWrongCommitError)
    Truncated,    // the option bits end early
    BadEnum,      // an enum index out of range
};
const char* permalinkErrorText(PermalinkError e);

// WWRandomizer.decode_permalink. *o holds the starting values (options not in the permalink keep
// them, as wwrando does with the options it is given; pass a default Options for Options()). On
// success *seed is the permalink's seed and *o its options; on an error neither is changed.
PermalinkError decodePermalink(const std::string& permalink, std::string* seed, Options* o,
                               bool allowDifferentCommit = false);

// ---------------------------------------------------------------------------------------------
// Seed names and hashes (pc_rando_seedgen.cpp)

// The port's SEED_KEY for do_not_generate_spoiler_log seeds (decision D2; wwrando's official key is
// private, so such seeds never match official ones).
extern const char* const kSwitchWakerSeedKey;

void md5(const void* data, size_t len, uint8_t out[16]);

// WWRandomizer.get_seed_hash: two names of at most 5 letters from seedgen/names.txt, sampled by a
// Python Random seeded with md5(permalink [+ seedKey when do_not_generate_spoiler_log]),
// "Name1 Name2". Empty for an empty permalink.
std::string seedHash(const std::string& permalink, const Options& o,
                     const char* seedKey = kSwitchWakerSeedKey);

// WWRandomizer.integer_seed: md5(sanitized seed [+ seedKey when do_not_generate_spoiler_log]) as the
// 16 digest bytes (the big-endian 128-bit integer Python seeds its Random with).
void integerSeedMd5(const std::string& sanitizedSeed, const Options& o, uint8_t out[16],
                    const char* seedKey = kSwitchWakerSeedKey);

// seedgen.make_random_seed_name: 2 adjectives + a noun or a name, each with its first letter
// capitalized ("GummyAliveTrust"). wwrando seeds from the OS; here `entropy` seeds the same
// algorithm (callers pass time or an OS random value).
std::string randomSeedName(uint64_t entropy);

} // namespace pcr
