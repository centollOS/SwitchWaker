// COS_SMOKE=rando COS_RANDO_SMOKE=rmenu (docs/RANDOMIZER_PLAN.md step R8.2): the options menu's
// Randomizer tab in two pages (features/rando/pc_rando_menu.h): first the game being played, then
// "Next game options..." with everything that only acts on the next New Game. Driven by an input
// script the menu reads (randoMenuSetScripted), checked at fixed frames. Two cases:
//
//   COS_RANDO_RMENU_CASE=title (no --stage; input native/check/input/rando-rmenu-title.txt): at the
//     title screen (the opening scene, from about frame 289) the tab's first page says "No game
//     loaded" and offers "Next game options..."; B closes the menu.
//
//   COS_RANDO_RMENU_CASE=play (--stage sea:44:206, COS_RANDO_SEED=TestSeed1; input
//     native/check/input/rando-rmenu.txt):
//     1. frame 425: the first page of a booted (vanilla) game: "Current game" = "Normal game", then
//        "Next game options..."; no seed, hash or spoiler row;
//     2. DOWN A opens the second page (450): its title and first line (the settings apply to the next
//        new Randomizer file; no pending seed), the cursor on Seed..., the generation rows on it and
//        none of the first page's;
//     3. UP x4 (wrapping past "< Back", Clean unused seed files, Ask on New Game) to Generate (476), A
//        generates TestSeed1 from the page (checked at 600: pending, the header names it and its hash);
//     4. B goes back to the first page, on "Next game options..." (615); the smoke then activates the
//        pending seed as loading its file would: the first page shows "Randomizer", the seed name and
//        hash, and "Spoiler log..." (625);
//     5. DOWN DOWN A on "Spoiler log...": the viewer opens on the active seed's log (660); B closes it
//        (680), B closes the menu (700).
//   The logs go to <run dir>/user/rando (COS_RANDO_DIR, set here) and the run has its own memory card
//   (pc_rando_smoke_wants_run_card), as in the gen smoke.
// Shots (COS_SHOT_IMGUI=1, run with COS_LANG=es and en): play 430 the first page (normal game), 455 the
// second page, 605 the second page with the pending seed, 630 the first page of the randomized file;
// title: 30 frames after its check.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_menu.h"
#include "features/rando/pc_rando_settings.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_spoiler_ui.h"
#include "pc/pc_settings.h"

#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "rmenu";

unsigned int sNext = 0;
bool sStarted = false;

// The labels and values (English) of a page's rows, as the menu shows them (the "< Back" row of a
// sub page is the menu's own and not listed).
struct PageRow {
    std::string label, value;
    RandoRow row;
};

std::vector<PageRow> pageRows(int page) {
    const std::vector<RandoRow> rows = randoMenuRows();
    std::vector<PageRow> out;
    for (int i : randoMenuPageRows(page)) {
        out.push_back({randoRowLabel(rows[i], false), randoRowValue(rows[i], false), rows[i]});
    }
    return out;
}

int find(const std::vector<PageRow>& rows, const char* label) {
    for (int i = 0; i < (int)rows.size(); i++) {
        if (rows[i].label == label) {
            return i;
        }
    }
    return -1;
}

void logPage(int page) {
    for (const PageRow& r : pageRows(page)) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: page %d: %s = %s\n", kName, page, r.label.c_str(), r.value.c_str());
    }
}

void checkMenu(int page, int row) {
    int tab = -1, sel = -1, capture = -1;
    menuSmokeState(&tab, &sel, &capture);
    char what[128];
    snprintf(what, sizeof what, "menu open on the Randomizer tab, page %d row %d (tab %d page %d row %d)", page, row,
             tab, menuPage(PC_SETTING_TAB_RANDO), sel);
    randoSmokeCheck(kName, what,
                    menuOpen() && tab == PC_SETTING_TAB_RANDO && menuPage(PC_SETTING_TAB_RANDO) == page && sel == row);
}

void* findScene(void* proc, void* name) {
    return fpcM_GetName(proc) == *(s16*)name ? proc : nullptr;
}

bool sceneRuns(s16 name) {
    return fpcM_Search(findScene, &name) != nullptr;
}

// The first page of a game without a randomized file: Current game = `value`, Next game options...
void checkPlainPage(const char* value) {
    logPage(kRandoPageGame);
    const std::vector<PageRow> rows = pageRows(kRandoPageGame);
    randoSmokeCheck(kName, "the first page: Current game, Next game options...",
                    rows.size() == 2 && rows[0].label == "Current game" && rows[1].label == "Next game options...");
    randoSmokeCheck(kName, (std::string("Current game shows \"") + (rows.empty() ? "" : rows[0].value) + "\"").c_str(),
                    !rows.empty() && rows[0].value == value);
    randoSmokeCheck(kName, "the game kind", randoCurrentGame() == (strcmp(value, "Normal game") == 0
                                                                       ? RandoGameKind::Normal
                                                                       : RandoGameKind::None));
}

// ---- title ------------------------------------------------------------------------------------------

void titleCheck() {
    // The logo hands over to the opening scene, which shows the title screen (d_s_logo.cpp).
    randoSmokeCheck(kName, "the title screen runs (the opening scene, no PLAY scene)",
                    sceneRuns(fpcNm_OPENING_SCENE_e) && !sceneRuns(fpcNm_PLAY_SCENE_e));
    checkMenu(kRandoPageGame, 0);
    checkPlainPage("No game loaded");
}

void titleClosed() {
    randoSmokeCheck(kName, "B closed the menu", !menuOpen());
}

// ---- play -------------------------------------------------------------------------------------------

void playGamePage() {
    checkMenu(kRandoPageGame, 0);
    checkPlainPage("Normal game");
    std::string sub = "x";
    randoSmokeCheck(kName, "the first page has no header", randoMenuPageHeader(kRandoPageGame, false, &sub).empty() &&
                                                                sub.empty());
}

void playNextPage() {
    checkMenu(kRandoPageNext, 0);
    logPage(kRandoPageNext);
    std::string sub;
    const std::string title = randoMenuPageHeader(kRandoPageNext, false, &sub);
    writef(STDERR_FILENO, "[cos] rando-smoke %s: header \"%s\" / \"%s\"\n", kName, title.c_str(), sub.c_str());
    randoSmokeCheck(kName, "the title: Next game options", title == "Next game options");
    randoSmokeCheck(kName, "its first line: the next new Randomizer file, no pending seed",
                    sub.rfind("These settings apply to the next new Randomizer file", 0) == 0 &&
                        sub.find("No pending seed.") != std::string::npos);
    const std::vector<PageRow> rows = pageRows(kRandoPageNext);
    const int seed = find(rows, "Seed...");
    const int gen = find(rows, "Generate");
    const int ask = find(rows, "Ask on New Game");
    const int clean = find(rows, "Clean unused seed files");
    randoSmokeCheck(kName, "Seed..., Random seed, Paste permalink first, then the preset",
                    seed == 0 && find(rows, "Random seed") == 1 && find(rows, "Paste permalink") == 2 &&
                        rows.size() > 3 && rows[3].row.kind == RandoRowKind::Preset);
    randoSmokeCheck(kName, "Generate, Ask on New Game, Clean unused seed files last",
                    gen > 3 && ask == gen + 1 && clean == ask + 1 && clean == (int)rows.size() - 1);
    randoSmokeCheck(kName, "Seed... shows TestSeed1", seed == 0 && rows[0].value == "TestSeed1");
    randoSmokeCheck(kName, "none of the first page's rows",
                    find(rows, "Current game") < 0 && find(rows, "Next game options...") < 0 &&
                        find(rows, "Spoiler log...") < 0);
}

void playOnGenerate() {
    const std::vector<PageRow> rows = pageRows(kRandoPageNext);
    // UP x4 from the first row: "< Back" (the menu's), Clean, Ask, Generate.
    checkMenu(kRandoPageNext, find(rows, "Generate"));
}

void playGenStarted() {
    randoSmokeCheck(kName, "A on Generate started it (or it already ended)",
                    randoGenBusy() || (randoGenLast().have && randoGenLast().seed == "TestSeed1"));
}

void playGenerated() {
    const RandoGenLast& g = randoGenLast();
    const PcRandoSeed* p = pc_rando_slot_pending();
    randoSmokeCheck(kName, ("TestSeed1 generated (" + g.error + ")").c_str(),
                    g.have && g.ok && p != nullptr && p->seed == "TestSeed1" && !g.hash.empty());
    std::string sub;
    randoMenuPageHeader(kRandoPageNext, false, &sub);
    writef(STDERR_FILENO, "[cos] rando-smoke %s: header \"%s\"\n", kName, sub.c_str());
    randoSmokeCheck(kName, "the header names the pending seed and its hash",
                    sub.find("Pending seed: TestSeed1 (" + g.hash + ")") != std::string::npos);
    randoSmokeCheck(kName, "the first page is unchanged by a pending seed (still a normal game)",
                    pageRows(kRandoPageGame).size() == 2 && randoCurrentGame() == RandoGameKind::Normal);
}

void playBack() {
    checkMenu(kRandoPageGame, 1); // B: the first page, on "Next game options..."
    const PcRandoSeed* p = pc_rando_slot_pending();
    std::string err;
    randoSmokeCheck(kName, ("the pending seed activated, as its file would be (" + err + ")").c_str(),
                    p != nullptr && pc_rando_slot_activate(*p, &err) && pc_rando());
}

void playRandoPage() {
    logPage(kRandoPageGame);
    const RandoGenLast& g = randoGenLast();
    const std::vector<PageRow> rows = pageRows(kRandoPageGame);
    randoSmokeCheck(kName, "a randomized file: Current game, Seed, Hash, Spoiler log..., Next game options...",
                    rows.size() == 5 && rows[0].label == "Current game" && rows[1].label == "Seed" &&
                        rows[2].label == "Hash" && rows[3].label == "Spoiler log..." &&
                        rows[4].label == "Next game options...");
    if (rows.size() == 5) {
        randoSmokeCheck(kName, ("Current game: " + rows[0].value).c_str(), rows[0].value == "Randomizer");
        randoSmokeCheck(kName, ("Seed: " + rows[1].value).c_str(), rows[1].value == "TestSeed1");
        randoSmokeCheck(kName, ("Hash: " + rows[2].value).c_str(), rows[2].value == g.hash);
        randoSmokeCheck(kName, ("Spoiler log...: " + rows[3].value).c_str(), rows[3].value == "A: view TestSeed1");
    }
}

void playOnSpoiler() {
    checkMenu(kRandoPageGame, 3);
}

void playSpoilerOpen() {
    const RandoSpoilerInfo& info = randoSpoilerInfo();
    randoSmokeCheck(kName, "A opened the spoiler viewer", randoSpoilerActive());
    randoSmokeCheck(kName, ("on the active seed's log " + info.logPath).c_str(),
                    info.active && info.seed == "TestSeed1" && info.logFound && info.logMatches &&
                        info.logPath == randoGenLast().spoilerPath && info.hash == randoGenLast().hash);
}

void playSpoilerClosed() {
    randoSmokeCheck(kName, "B closed the viewer; the menu shows the first page",
                    !randoSpoilerActive() && menuOpen() && menuPage(PC_SETTING_TAB_RANDO) == kRandoPageGame);
}

void playClosed() {
    randoSmokeCheck(kName, "B closed the menu", !menuOpen());
}

struct Step {
    unsigned int frame;
    void (*fn)();
};
const Step kPlay[] = {
    {425, playGamePage}, {450, playNextPage},  {476, playOnGenerate},  {484, playGenStarted},
    {600, playGenerated}, {615, playBack},     {625, playRandoPage},   {648, playOnSpoiler},
    {660, playSpoilerOpen}, {680, playSpoilerClosed}, {700, playClosed},
};
// The title screen's frames (rando-rmenu-title.txt).
const Step kTitle[] = {
    {825, titleCheck},
    {870, titleClosed},
};

} // namespace

void randoSmokeRmenu(unsigned int frames) {
    (void)frames;
    const char* c = getenv("COS_RANDO_RMENU_CASE");
    const bool title = c != nullptr && strcmp(c, "title") == 0;
    const Step* steps = title ? kTitle : kPlay;
    const unsigned int count = title ? sizeof kTitle / sizeof kTitle[0] : sizeof kPlay / sizeof kPlay[0];
    const unsigned int frame = pc_frame_count();
    if (!sStarted) {
        sStarted = true;
        const char* run = getenv("COS_RUN_DIR");
        const std::string dir = std::string(run != nullptr ? run : ".") + "/user/rando";
        setenv("COS_RANDO_DIR", dir.c_str(), 1);
        randoMenuSetScripted(true); // the menu reads the COS_INPUT script
        writef(STDERR_FILENO, "[cos] rando-smoke %s: case %s\n", kName, title ? "title" : "play");
    }
    if (!title) {
        if (frame >= 395 && frame < 400 && !randoSmokeInPlay(10)) {
            randoSmokeCheck(kName, "in play by frame 395 (the script starts at 400)", false);
            randoSmokeEnd(kName);
        }
        randoSmokeInPlay(10);
    } else {
        static bool sTitleSeen = false;
        if (!sTitleSeen && sceneRuns(fpcNm_OPENING_SCENE_e)) {
            sTitleSeen = true;
            writef(STDERR_FILENO, "[cos] rando-smoke %s: title scene from frame %u\n", kName, frame);
        }
    }
    while (sNext < count && frame >= steps[sNext].frame) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u\n", kName, frame);
        steps[sNext++].fn();
    }
    if (sNext == count) {
        randoMenuSetScripted(false);
        randoSmokeEnd(kName);
    }
}

} // namespace pc
