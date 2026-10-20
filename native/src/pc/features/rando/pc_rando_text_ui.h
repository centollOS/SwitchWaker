// The randomizer's text input on screen (docs/RANDOMIZER_PLAN.md step R4.2), hosted by the options
// menu (pc_menu.cpp): one session at a time, opened by the randomizer's menu rows (R4.3: "Seed...",
// "Paste permalink"). While a session runs the menu shows it instead of its rows and hands it its
// keys; it ends with the `done` callback, then the menu shows its rows again.
//
// How the player types:
//   - Switch, title mode: the system keyboard applet (cos_switch_text_input_start), on its own thread
//     so the frames go on; refused (applet mode) or failed: the grid;
//   - everywhere: the D-pad character grid (pc_rando_text.h TextGrid): D-pad / stick move, A types
//     the cell, B deletes (with no text: cancel), L / R switch the case; the OK and Cancel cells end
//     it. The mouse can click its cells;
//   - Mac / Linux: a text field (ImGui::InputText) with the keyboard focus: Enter = OK, Esc =
//     cancel. While it has the focus the menu's own keyboard keys (arrows, Enter, Backspace, Q/E)
//     are not read (pc_menu.cpp readKeyboard), so typing does not move the grid.
// Game thread only. Code in game_hooks/pc_rando_hooks_text.cpp (not in cos_rando_core: it uses ImGui
// and the harness).
#pragma once

#include "pc_rando_text.h"

#include <stdint.h>
#include <string>

namespace pc {

// ok: OK (text: the sanitized text), else cancelled (text: what had been typed).
using RandoTextDoneFn = void (*)(bool ok, const char* text, void* user);

struct RandoTextRequest {
    rando::TextKind kind = rando::TextKind::Seed;
    const char* titleEs = "Semilla";
    const char* titleEn = "Seed";
    const char* initial = "";
    RandoTextDoneFn done = nullptr;
    void* user = nullptr;
    // Smokes: the menu reads the COS_INPUT script while this session runs, and the Switch keyboard
    // applet is not used.
    bool script = false;
};

// Starts a session (and opens the menu if it is closed). False if one is already running.
bool randoTextOpen(const RandoTextRequest& request);
bool randoTextActive();
// The running session asked for the COS_INPUT script (RandoTextRequest::script).
bool randoTextScripted();
// The desktop text field has the keyboard focus (the menu then reads no keyboard keys).
bool randoTextKeyboardCaptured();
// The menu's keys pressed this frame (pc_menu.cpp Key bits = rando::TextKey bits).
void randoTextKeys(uint32_t pressed);
// Draws the session inside the menu's rows area; `help` gets the help line. Polls the Switch keyboard.
void randoTextDraw(std::string* help);
// Ends a running session as cancelled (the menu closed).
void randoTextCancel();
// The running session's grid (smokes); nullptr when none runs.
const rando::TextGrid* randoTextGrid();

// <userPath>/rando (plan D5): the randomizer's folder for logs and permalink.txt. COS_RANDO_DIR
// overrides it (tests). Not created here.
std::string randoUserDir();
// Imports <randoUserDir()>/permalink.txt (rando::permalinkReadFile) and logs the result; *path gets
// the file's path when not null.
rando::PermalinkInfo randoPermalinkImport(std::string* path = nullptr);

} // namespace pc
