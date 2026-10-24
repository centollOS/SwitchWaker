// The file select's "Normal / Randomizer" question (docs/RANDOMIZER_PLAN.md step R8.1, Phase R8,
// which revises D6). Defined in native/src/pc/game_hooks/pc_rando_hooks_fileselect_newgame.cpp (game
// side); the hooks the decompiled file select calls are in pc/pc_rando_hooks.h. Game thread only.
//
// With the setting COS_RANDO_ASK_NEWGAME on (the default, row "Ask on New Game" of the Randomizer tab),
// A on a blank slot of the file select asks "Start a Randomizer file?" with the panes of the
// corrupted-slot prompt (message pane + Yes/No, d_file_select.cpp), in place of the info panel and the
// Start/Copy/Erase/Back menu. The message pane shows the seed line: the pending seed's name and hash
// words, or the seed the menu's settings will generate. The same number of presses as before reaches
// the name entry (A on the slot, A on the preselected No):
//   - No: the vanilla new file; a pending seed stays pending (R0.7's New Game hook skips it);
//   - Yes: when there is a seed (a pending one, or a name in COS_RANDO_SEED), a second question
//     (R8.3) replaces the texts in the same panes: "Make a new seed?" with the current seed below
//     (the pending seed's name and hash words, else the setting's name), Yes preselected:
//       - Yes: a new random seed name (as the menu's "Random seed" row, stored in COS_RANDO_SEED so
//         the menu shows it; kept as it is when the environment sets it) is generated;
//       - No: the pending seed if there is one, else the seed of the settings as they are;
//       - B: back to the slot list.
//     With no seed at all the second question is skipped and a new seed is generated. A seed is
//     generated on the worker job (pc_rando_menu.h randoGenerate, as the Generate row does) while
//     the panel shows "Generating the seed..." and its progress, then the name entry follows and the
//     New Game hook consumes the new pending seed (R4.4); a generation error shows a one-line message
//     (B/A or a few seconds) and returns to the slot list; the error log is written as in R4.3;
//   - B: back to the slot list (also while a seed is generated: it stays the pending seed).
// A slot that is not blank never asks. With the setting off the file select is the R4.4 one exactly.
//
// The texts are messages of the R3.20 overlay (pc_rando_msg.h, numbers PCR_FSNG_MSG_*): English on
// the USA game; on the European game they follow the game's language (Spanish for Spanish, English for
// the other three). D16: the PAL file select has the same hunks and asks too since R7.6a.
#pragma once

#include <string>

// The overlay message numbers of the question's texts (not in the BMG; read by the file select only).
#define PCR_FSNG_MSG_QUESTION 0xFF00 // title: the question
#define PCR_FSNG_MSG_SEED 0xFF01     // message pane: the pending seed (%s name, %s hash words)
#define PCR_FSNG_MSG_NEWSEED 0xFF02  // message pane: no pending seed (%s the seed name or "random")
#define PCR_FSNG_MSG_WAIT 0xFF03     // title while generating (%d percent)
#define PCR_FSNG_MSG_ERROR 0xFF04    // title after a generation error
#define PCR_FSNG_MSG_RANDOM 0xFF05   // the seed name shown for an empty seed setting
#define PCR_FSNG_MSG_NEWQ 0xFF06     // R8.3 title: the second question ("Make a new seed?")
#define PCR_FSNG_MSG_CURPEND 0xFF07  // R8.3 message pane: the pending seed (%s name, %s hash words)
#define PCR_FSNG_MSG_CURNAME 0xFF08  // R8.3 message pane: the setting's seed name (%s)

namespace pc {

extern const char* const kRandoAskNewGameKey; // "COS_RANDO_ASK_NEWGAME"

// Registers the setting and the "Ask on New Game" row (called by randoMenuInit). Again is a no-op.
void randoFsNewgameMenuInit();
// The setting is on and the region has the randomizer (D16).
bool randoFsNewgameEnabled();

// What the question did in this process, for the smoke and the logs.
struct RandoFsNewgameInfo {
    int asked = 0;       // questions asked
    int notAsked = 0;    // A on a slot that did not ask (not blank, or the setting off)
    int answeredNo = 0;
    int answeredYes = 0;
    int backs = 0;       // B, or back after an error
    int usedPending = 0; // Yes with a pending seed
    int generated = 0;   // Yes that started a generation (or waited for the menu's)
    int errors = 0;      // generations that failed
    int asked2 = 0;      // R8.3: second questions asked ("Make a new seed?")
    int answered2Yes = 0;
    int answered2No = 0;
    int newSeeds = 0;    // R8.3: new random seed names made (second question Yes, or no seed at all)
    std::string lastNewSeed; // the last new random seed name
    std::string question2Message; // the message pane text of the last second question
    int waitFrames = 0;  // frames of the last wait state
    double seconds = 0.0; // the last generation's time (genjob, files excluded)
    std::string lastError;  // first line of the last error
    std::string lastTitle;  // the last title text written
    std::string lastMessage; // the last message pane text written
};
const RandoFsNewgameInfo& randoFsNewgameInfo();

} // namespace pc
