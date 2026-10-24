// The spoiler viewer on screen (docs/RANDOMIZER_PLAN.md step R4.5): the Randomizer tab's row
// "Spoiler log..." opens a page of the options menu, hosted like the text input of R4.2
// (pc_rando_text_ui.h): while it is open the menu draws it instead of its rows and hands it its keys.
//
// The seed shown is the active one (pc_rando() is 1: a randomized file is played). R8.2: the row is on
// the tab's page about the game being played and only shown then; the pending seed (the next game's)
// is not offered any more. The page opens on a confirmation (the spoilers stay
// hidden until A; B goes back), then shows two sections, L / R switching between them:
//   - Spheres: wwrando's "Playthrough progression spheres" of the seed's spoiler log
//     (<randoUserDir()>/WW Random <seed> - Spoiler Log.txt, D5), sphere by sphere and zone by zone;
//     only when that log is there and is this seed's (its Seed and Permalink lines);
//   - Locations by zone: every location of the placement and its item.
// Up / down move a line, left / right a heading (sphere or zone), B closes the page.
// Refused (a toast) with no seed, and for a do_not_generate_spoiler_log seed.
//
// Model: pc_rando_spoilerview.h (pure). Code: game_hooks/pc_rando_hooks_spoiler.cpp (ImGui, the files).
// Game thread only.
#pragma once

#include "pc_rando_spoilerview.h"

#include <stdint.h>
#include <string>

namespace pc {

// Registers the "Spoiler log..." row (randoMenuAddAction, after Generate). Called by pc_menu.cpp's
// menuInit after randoMenuInit; again is a no-op.
void randoSpoilerInit();
// The row's A: loads the seed and opens the confirmation (the menu opens with it). False (and *msg
// why) when there is nothing to show; *msg gets the toast either way.
bool randoSpoilerOpen(bool spanish, std::string* msg);
bool randoSpoilerActive();
// The menu's keys pressed this frame (pc_menu.cpp Key bits = pcr::SpoilerKey bits).
void randoSpoilerKeys(uint32_t pressed);
// Draws the page inside the menu's rows area; `help` gets the help line.
void randoSpoilerDraw(std::string* help);
// Closes the page (the menu closed).
void randoSpoilerClose();

// What the open page shows (smokes).
struct RandoSpoilerInfo {
    std::string seed;    // the seed's name
    std::string hash;    // "Name1 Name2" ("" unknown: no log)
    bool active = false; // the active seed (R8.2: always, when the page is open)
    std::string logPath;
    bool logFound = false;   // the log was read and parsed
    bool logMatches = false; // ... and is this seed's: its spheres are shown
    bool zonesMatchLog = false; // the placement's locations by zone equal the log's "All item locations"
};
const RandoSpoilerInfo& randoSpoilerInfo();
const pcr::SpoilerPager& randoSpoilerPager();

} // namespace pc
