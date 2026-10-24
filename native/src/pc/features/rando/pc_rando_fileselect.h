// The randomizer on the file select and the "Clean unused seed files" row (docs/RANDOMIZER_PLAN.md
// step R4.4, decisions D3 and D15). Defined in native/src/pc/game_hooks/pc_rando_hooks_fileselect.cpp
// (game side: it reads the card folder and the game's file-select data); the pure parts are
// pc_rando_sidecars.h. Game thread only.
//
// The file select: dFile_select_c::_draw calls pc_rando_file_select_draw (pc_rando_hooks.h) every
// frame it draws. For the selected slot, while the slot list or its info panel is up, it works out
// the line to show (pcr::FileInfoKind): a randomized slot shows its seed name and hash words, a
// randomized slot whose sidecar is missing says it plays vanilla, and a blank slot shows the pending
// seed the next new file uses. The line is drawn with ImGui at the bottom of the screen by
// randoFileSelectOverlay (called from randoMenuFrame, every frame in pc_frame_end's ImGui frame); a
// vanilla slot shows nothing. Screenshots (COS_SHOT) leave ImGui out, so smokes read
// randoFileSelectInfo instead.
#pragma once

#include "pc_rando_sidecars.h"

#include <string>

namespace pc {

struct RandoFileSelectInfo {
    unsigned int frame = 0; // pc_frame_count() of the last pc_rando_file_select_draw (0: never)
    int slot = -1;          // selectNum then
    int proc = -1;          // the file select's DataSelProc index then
    pcr::FileInfoKind kind = pcr::FileInfoKind::None;
    std::string text;       // the line shown ("" none), in randoFileSelectSpanish()'s language
    std::string seed;       // the seed name (Rando, Pending)
    std::string words;      // its hash words (Rando, Pending)
    unsigned int shownFrames = 0; // frames the overlay drew a line (any slot)
};
const RandoFileSelectInfo& randoFileSelectInfo();

// The line's language (R7.6a, game-screen text rule): English on the USA game; on the European game
// the game's language (dComIfGs_getPalLanguage() 3, Spanish: Spanish; the other four: English), as the
// file select's question (pc_rando_hooks_fileselect_newgame.cpp). The menu keeps pc_ui_spanish().
bool randoFileSelectSpanish();

// Draws the line of the file select's current frame, if any (an ImGui frame must be open).
void randoFileSelectOverlay();

// Registers the "Clean unused seed files" row (called by randoMenuInit).
void randoFileSelectMenuInit();

// The row's action: removes the orphan sidecars of the card folder (pcr::sidecarClean), keeping the
// pending seed's and, while pc_rando() is 1, the active seed's. Refused while a seed is generated
// (its sidecar is written before it becomes the pending seed). Returns the toast; *out (may be NULL)
// gets the result.
std::string randoCleanSidecars(bool spanish, pcr::SidecarCleanResult* out);

} // namespace pc
