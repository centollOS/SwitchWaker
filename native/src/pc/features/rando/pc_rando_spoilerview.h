// The spoiler viewer's model (docs/RANDOMIZER_PLAN.md step R4.5): the options menu's "Spoiler log"
// page shows a seed's progression spheres and every location by zone, behind a confirmation.
//
// Sources:
//   - the spheres come from the seed's spoiler log (D5: <userPath>/rando/WW Random <seed> - Spoiler
//     Log.txt, written by R4.3 at generation), section "Playthrough progression spheres:", as
//     pcr::spoilerLog (pc_rando_spoiler.cpp, wwrando's write_spoiler_log) writes it;
//   - the locations by zone come from the placement itself (PcRandoSeed::location_names and
//     location_item_names, split by split_location_name_by_zone), so they are what the game uses even
//     when the log is gone. When the log is there its "All item locations:" section must agree.
// A seed made with do_not_generate_spoiler_log has no spoiler: the viewer refuses it (seedHidesSpoiler).
//
// Pure C++ (cos_rando_core, unit test test_R4_5.cpp). The page on screen and its row are
// game_hooks/pc_rando_hooks_spoiler.cpp (features/rando/pc_rando_spoiler_ui.h).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcr {

// One line of the page. depth 0: a heading ("Sphere 1", a zone), 1: a zone inside a sphere,
// 2: a location (text: its name in the zone, item: what it holds).
struct SpoilerLine {
    int depth = 0;
    std::string text;
    std::string item;
};

struct SpoilerLog {
    // The header lines ("" when absent).
    std::string seed;      // "Seed: "
    std::string seedHash;  // "Seed Hash: "
    std::string permalink; // "Permalink: "
    bool haveSpheres = false;   // the section was found
    std::vector<SpoilerLine> spheres; // depth 0 "1", "2", ... (the number only), 1 zone, 2 location
    bool haveLocations = false; // "All item locations:" found
    std::vector<SpoilerLine> locations; // depth 0 zone, 2 location
};

// Parses the sections of a spoiler log (wwrando's format; the D5 trailer line is ignored). False
// (and *err) when the text is not a spoiler log (no "Seed: " line or no spheres section).
bool spoilerLogParse(const std::string& text, SpoilerLog* out, std::string* err);

// The locations by zone of a placement (names and items in file order): one depth-0 line per zone,
// in the order its first location appears (wwrando's log order), then its locations (depth 2).
std::vector<SpoilerLine> spoilerZonesFromPlacement(const std::vector<std::string>& names,
                                                   const std::vector<std::string>& items);

// The page's sections.
enum SpoilerSection : int { kSpoilerSpheres = 0, kSpoilerZones, kSpoilerSectionCount };

// The page's keys: the menu's bits (pc_menu.cpp Key, as rando::TextKey).
enum SpoilerKey : uint32_t {
    kSpoilerUp = 1u << 0,
    kSpoilerDown = 1u << 1,
    kSpoilerLeft = 1u << 2,    // previous heading (depth 0)
    kSpoilerRight = 1u << 3,   // next heading
    kSpoilerConfirm = 1u << 4, // A: on the confirmation, show
    kSpoilerBack = 1u << 5,    // B: on the confirmation, cancel; on the page, close
    kSpoilerTabL = 1u << 6,    // L / R: the other section
    kSpoilerTabR = 1u << 7,
};

enum class SpoilerState { Closed, Confirm, View };

// The page's navigation: a confirmation first (the spoilers stay hidden until A), then the two
// sections, each with its own cursor line.
class SpoilerPager {
public:
    // Opens on the confirmation with these sections (either may be empty).
    void open(std::vector<SpoilerLine> spheres, std::vector<SpoilerLine> zones);
    void close();
    // Keys pressed this frame. Returns true when the cursor or the section moved (scroll to it).
    bool press(uint32_t keys);

    SpoilerState state() const { return mState; }
    int section() const { return mSection; }
    const std::vector<SpoilerLine>& lines(int section) const { return mLines[section]; }
    const std::vector<SpoilerLine>& lines() const { return mLines[mSection]; }
    int cursor() const { return mCursor[mSection]; }
    // The depth-0 line the cursor is under (-1: none).
    int heading() const;
    // The number of depth-0 / depth-2 lines of a section.
    int headings(int section) const;
    int entries(int section) const;

private:
    SpoilerState mState = SpoilerState::Closed;
    int mSection = kSpoilerSpheres;
    std::vector<SpoilerLine> mLines[kSpoilerSectionCount];
    int mCursor[kSpoilerSectionCount] = {0, 0};
};

} // namespace pcr
