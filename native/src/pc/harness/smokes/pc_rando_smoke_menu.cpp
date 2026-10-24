// COS_SMOKE=rando COS_RANDO_SMOKE=menu (docs/RANDOMIZER_PLAN.md step R4.1): the options menu's
// Randomizer tab (features/rando/pc_rando_settings.h), driven by an input script as in
// COS_SMOKE=options-menu:
//   native/tools/run.sh rando --stage sea:44:206 --input native/check/input/rando-menu.txt \
//       --env COS_RANDO_SMOKE=menu
//   1. the script opens the menu and goes to the Randomizer tab (frame 420): R8.2: its first page is
//      about the game being played (a booted stage: "Normal game"); DOWN A on "Next game options..."
//      opens the second page, DOWN x3 past Seed..., Random seed and Paste permalink to the preset,
//      which starts at wwrando's defaults (run.sh gives the run its own settings file);
//   2. it cycles the preset row: All checks, Short, the defaults (wrapping), back to Short. After each
//      the smoke checks the COS_RANDO_* settings, the settings file's lines and that the validated
//      settings give wwrando's permalink for TestSeed1 (native/check/rando/fixtures/R4_1_presets.txt);
//   3. it turns "Categoría: mazmorras" off: Required bosses (and its count) is greyed out, refuses a
//      change, and a stored "on" reads as off for a seed (ensure_valid_combination_of_options); with
//      dungeons back on the stored value is in effect again and the row changes;
//   4. the starting gear editor (rows driven by the smoke itself): the header expands the 64 item
//      rows; Swordless greys out Progressive Sword and Hurricane Spin; an item row toggles the item in
//      COS_RANDO_STARTING_GEAR; a sword in the gear is dropped for a Swordless seed;
//   5. the script presses B: back to the first page (R8.2), on "Next game options..."; B again closes
//      the menu.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_settings.h"
#include "features/rando/pc_rando_settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "menu";
// wwrando 1.10.0_9775811's permalinks for TestSeed1 (fixture R4_1_presets.txt).
constexpr const char* kPermaDefaults = "MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAEkQUBgAAPsCyCUAAAAAAAEAQQCAAA==";
constexpr const char* kPermaAll = "MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAP//fxgAAPsCyCUAAAAAAAEAQQCAAA==";
constexpr const char* kPermaShort = "MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAAEQABgAAPsCyCUAAAAAAAEAQQCAAA==";

unsigned int sNext = 0; // index into kSteps

bool fileHasLine(const std::string& want) {
    FILE* f = fopen(pc_settings_path(), "r");
    if (f == nullptr) {
        return false;
    }
    char line[2048];
    bool found = false;
    while (!found && fgets(line, sizeof(line), f) != nullptr) {
        line[strcspn(line, "\r\n")] = '\0';
        found = want == line;
    }
    fclose(f);
    return found;
}

bool setting(const char* key, const char* want) {
    const std::string v = pc_settings_get(key);
    const bool ok = v == want;
    randoSmokeCheck(kName, (std::string(key) + " = " + v + (ok ? "" : std::string(", expected ") + want)).c_str(), ok);
    return ok;
}

// A setting changed behind the menu's back (an older file, the debug server): the rows read the
// settings again (as the menu does every frame).
void store(const char* key, const char* value) {
    pc_settings_set(key, value);
    randoMenuRows();
}

// The index of the first row of that kind (and id) on the tab's "Next game options" page (R8.2: the
// menu's row index there), or -1.
int rowIndex(RandoRowKind kind, int id = -1) {
    const std::vector<RandoRow> rows = randoMenuRows();
    const std::vector<int> page = randoMenuPageRows(kRandoPageNext);
    for (int i = 0; i < (int)page.size(); i++) {
        const RandoRow& r = rows[page[i]];
        if (r.kind == kind && (id < 0 || r.id == id)) {
            return i;
        }
    }
    return -1;
}

// R8.2: the first rows of the "Next game options" page are Seed..., Random seed, Paste permalink.
constexpr int kPresetRow = 3;

RandoRow optionRow(int id) {
    return {RandoRowKind::Option, id};
}

int gearIndex(const char* name) {
    for (int i = 0; i < pcr::gearItemCount(); i++) {
        if (strcmp(pcr::gearItem(i).name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void checkMenu(int row, int page = kRandoPageNext) {
    int tab = -1, sel = -1, capture = -1;
    menuSmokeState(&tab, &sel, &capture);
    char what[128];
    snprintf(what, sizeof what, "menu open on the Randomizer tab, page %d row %d (tab %d page %d row %d)", page, row,
             tab, menuPage(PC_SETTING_TAB_RANDO), sel);
    randoSmokeCheck(kName, what,
                    menuOpen() && tab == PC_SETTING_TAB_RANDO && menuPage(PC_SETTING_TAB_RANDO) == page && sel == row);
}

// R8.2: frame 425, the tab's first page in a booted (vanilla) game.
void stepGamePage() {
    checkMenu(0, kRandoPageGame);
    const std::vector<RandoRow> rows = randoMenuRows();
    const std::vector<int> page = randoMenuPageRows(kRandoPageGame);
    const bool two = page.size() == 2;
    randoSmokeCheck(kName, "the first page: Current game, Next game options...",
                    two && randoRowLabel(rows[page[0]], false) == "Current game" &&
                        randoRowLabel(rows[page[1]], false) == "Next game options...");
    const std::string v = two ? randoRowValue(rows[page[0]], false) : "";
    randoSmokeCheck(kName, ("a booted stage is a normal game (\"" + v + "\")").c_str(), v == "Normal game");
}

void checkPreset(int preset, const char* permalink) {
    const std::string name = pcr::presetName(preset, false);
    const pcr::Options raw = randoSettingsOptions(false);
    randoSmokeCheck(kName, ("the settings are the preset " + name).c_str(), pcr::presetMatch(raw) == preset);
    randoSmokeCheck(kName, ("the preset row shows " + name).c_str(),
                    randoRowValue({RandoRowKind::Preset}, false) == name);
    const std::string got = pcr::encodePermalink("TestSeed1", randoSettingsOptions(true));
    randoSmokeCheck(kName, ("TestSeed1's permalink is wwrando's (" + got + ")").c_str(), got == permalink);
}

void stepTab() {
    checkMenu(kPresetRow);
    randoSmokeCheck(kName, "the preset row follows the three seed rows", rowIndex(RandoRowKind::Preset) == kPresetRow);
    randoSmokeCheck(kName, "then Category: dungeons",
                    rowIndex(RandoRowKind::Option, pcr::OPT_progression_dungeons) == kPresetRow + 1);
    randoSmokeCheck(kName, "the starting gear list starts collapsed", rowIndex(RandoRowKind::Gear) < 0);
    checkPreset(pcr::kPresetDefaults, kPermaDefaults);
}

void stepAll() {
    checkMenu(kPresetRow);
    checkPreset(pcr::kPresetAllChecks, kPermaAll);
    setting("COS_RANDO_PROGRESSION_MAIL", "1");
    setting("COS_RANDO_PROGRESSION_SAVAGE_LABYRINTH", "1");
    setting("COS_RANDO_HOHO_HINTS", "1"); // not a category: left at its default
    randoSmokeCheck(kName, "settings file: COS_RANDO_PROGRESSION_MAIL=1", fileHasLine("COS_RANDO_PROGRESSION_MAIL=1"));
}

void stepShort() {
    checkMenu(kPresetRow);
    checkPreset(pcr::kPresetShort, kPermaShort);
    setting("COS_RANDO_PROGRESSION_DUNGEONS", "1");
    setting("COS_RANDO_PROGRESSION_FREE_GIFTS", "1");
    setting("COS_RANDO_PROGRESSION_MAIL", "0");
    setting("COS_RANDO_PROGRESSION_GREAT_FAIRIES", "0");
    randoSmokeCheck(kName, "settings file: COS_RANDO_PROGRESSION_GREAT_FAIRIES=0",
                    fileHasLine("COS_RANDO_PROGRESSION_GREAT_FAIRIES=0"));
}

void stepDefaults() {
    checkMenu(kPresetRow);
    checkPreset(pcr::kPresetDefaults, kPermaDefaults);
    setting("COS_RANDO_PROGRESSION_GREAT_FAIRIES", "1");
    setting("COS_RANDO_PROGRESSION_MAIL", "0");
}

void stepShortAgain() {
    checkMenu(kPresetRow);
    checkPreset(pcr::kPresetShort, kPermaShort);
}

void stepDungeonsOff() {
    checkMenu(kPresetRow + 1);
    setting("COS_RANDO_PROGRESSION_DUNGEONS", "0");
    randoSmokeCheck(kName, "no preset matches (Custom)", pcr::presetMatch(randoSettingsOptions(false)) < 0);
    const RandoRow req = optionRow(pcr::OPT_required_bosses);
    const RandoRow num = optionRow(pcr::OPT_num_required_bosses);
    const std::string why = randoRowDisabled(req, false);
    randoSmokeCheck(kName, ("Required bosses greyed out (\"" + why + "\")").c_str(), !why.empty());
    randoSmokeCheck(kName, "Number of required bosses greyed out", !randoRowDisabled(num, false).empty());
    randoSmokeCheck(kName, "Category: dungeons itself is not greyed out",
                    randoRowDisabled(optionRow(pcr::OPT_progression_dungeons), false).empty());
    const std::string toast = randoRowActivate(req, 1, false);
    randoSmokeCheck(kName, ("a change of Required bosses is refused (\"" + toast + "\")").c_str(), !toast.empty());
    setting("COS_RANDO_REQUIRED_BOSSES", "0");
    // A stored "on" (an older file, the environment) is off for a seed while dungeons are off.
    store("COS_RANDO_REQUIRED_BOSSES", "1");
    randoSmokeCheck(kName, "stored on, the row shows Off", randoRowValue(req, false) == "Off");
    randoSmokeCheck(kName, "stored on, a seed gets it off", !randoSettingsOptions(true).required_bosses);
}

void stepDungeonsOn() {
    checkMenu(kPresetRow + 1);
    setting("COS_RANDO_PROGRESSION_DUNGEONS", "1");
    const RandoRow req = optionRow(pcr::OPT_required_bosses);
    const RandoRow num = optionRow(pcr::OPT_num_required_bosses);
    randoSmokeCheck(kName, "Required bosses enabled again", randoRowDisabled(req, false).empty());
    randoSmokeCheck(kName, "the stored on is in effect again", randoRowValue(req, false) == "On" &&
                                                                    randoSettingsOptions(true).required_bosses);
    randoSmokeCheck(kName, "Number of required bosses enabled with it", randoRowDisabled(num, false).empty());
    randoRowActivate(req, 1, false);
    setting("COS_RANDO_REQUIRED_BOSSES", "0");
    randoSmokeCheck(kName, "Number of required bosses greyed out again", !randoRowDisabled(num, false).empty());
    // num_required_bosses keeps its value while greyed out (a spin box in wwrando).
    store("COS_RANDO_REQUIRED_BOSSES", "1");
    randoRowActivate(num, 1, false);
    setting("COS_RANDO_NUM_REQUIRED_BOSSES", "5");
    randoRowActivate(num, -1, false);
    randoRowActivate(num, -1, false);
    randoRowActivate(num, -1, false);
    randoRowActivate(num, -1, false);
    randoRowActivate(num, -1, false); // 4 3 2 1, then wraps to 6
    setting("COS_RANDO_NUM_REQUIRED_BOSSES", "6");
    store("COS_RANDO_REQUIRED_BOSSES", "0");

    // The starting gear editor.
    const int header = rowIndex(RandoRowKind::GearHeader);
    randoSmokeCheck(kName, "the starting gear header row", header > kPresetRow);
    const size_t before = randoMenuRows().size();
    randoRowActivate({RandoRowKind::GearHeader}, 0, false);
    const size_t after = randoMenuRows().size();
    randoSmokeCheck(kName, "A on the header shows the 64 item rows", after == before + 64 &&
                                                                    rowIndex(RandoRowKind::Gear) == header + 1);
    const RandoRow sword{RandoRowKind::Gear, gearIndex("Progressive Sword")};
    const RandoRow spin{RandoRowKind::Gear, gearIndex("Hurricane Spin")};
    const RandoRow bombs{RandoRowKind::Gear, gearIndex("Bombs")};
    randoSmokeCheck(kName, "the gear rows are found", sword.id >= 0 && spin.id >= 0 && bombs.id >= 0);
    randoSmokeCheck(kName, "with a sword: Progressive Sword enabled", randoRowDisabled(sword, false).empty());
    randoRowActivate(bombs, 1, false);
    randoSmokeCheck(kName, "Bombs in the starting gear",
                    pcr::gearCount(randoSettingsOptions(false), "Bombs") == 1 && randoRowValue(bombs, false) == "Yes");
    randoRowActivate(sword, 1, false);
    randoRowActivate(sword, 1, false);
    randoSmokeCheck(kName, "Progressive Sword 2 / 3", randoRowValue(sword, false) == "2 / 3");
    setting(pcr::kStartingGearKey,
            "Ballad of Gales|Bombs|Progressive Magic Meter|Progressive Shield|Progressive Sword|Progressive Sword|"
            "Song of Passing");
    randoSmokeCheck(kName, "settings file: the starting gear line",
                    fileHasLine(std::string(pcr::kStartingGearKey) +
                                "=Ballad of Gales|Bombs|Progressive Magic Meter|Progressive Shield|Progressive "
                                "Sword|Progressive Sword|Song of Passing"));
    const RandoRow mode = optionRow(pcr::OPT_sword_mode);
    randoRowActivate(mode, -1, false); // Start with sword -> Swordless (wraps)
    setting("COS_RANDO_SWORD_MODE", "swordless");
    randoSmokeCheck(kName, "Swordless: Progressive Sword greyed out", !randoRowDisabled(sword, false).empty());
    randoSmokeCheck(kName, "Swordless: Hurricane Spin greyed out", !randoRowDisabled(spin, false).empty());
    randoSmokeCheck(kName, "Swordless: a change of Progressive Sword is refused",
                    !randoRowActivate(sword, 1, false).empty());
    randoSmokeCheck(kName, "Swordless: the stored swords are dropped for a seed",
                    pcr::gearCount(randoSettingsOptions(true), "Progressive Sword") == 0 &&
                        pcr::gearCount(randoSettingsOptions(false), "Progressive Sword") == 2 &&
                        randoRowValue(sword, false) == "0 / 3");
    randoRowActivate(mode, -1, false);
    setting("COS_RANDO_SWORD_MODE", "no_sword");
    randoSmokeCheck(kName, "No starting sword: Hurricane Spin enabled, Progressive Sword not",
                    randoRowDisabled(spin, false).empty() && !randoRowDisabled(sword, false).empty());
    randoRowActivate({RandoRowKind::GearHeader}, 0, false);
    randoSmokeCheck(kName, "A on the header hides the item rows", randoMenuRows().size() == before);
}

void stepBack() {
    checkMenu(1, kRandoPageGame); // B: the first page, on the row that opened the second
}

void stepClosed() {
    randoSmokeCheck(kName, "B closed the menu", !menuOpen());
}

struct Step {
    unsigned int frame;
    void (*fn)();
};
// R8.2: the frames from 445 on are 20 later than before (the second page is opened first).
const Step kSteps[] = {
    {421, stepGamePage},   {445, stepTab},          {460, stepAll},        {480, stepShort},
    {500, stepDefaults},   {520, stepShortAgain},   {540, stepDungeonsOff}, {560, stepDungeonsOn},
    {595, stepBack},       {610, stepClosed},
};

} // namespace

void randoSmokeMenu(unsigned int frames) {
    (void)frames;
    randoMenuSetScripted(true);
    const unsigned int frame = pc_frame_count();
    if (frame >= 395 && frame < 400 && !randoSmokeInPlay(10)) {
        randoSmokeCheck(kName, "in play by frame 395 (the script starts at 400)", false);
        randoSmokeEnd(kName);
    }
    randoSmokeInPlay(10);
    while (sNext < sizeof kSteps / sizeof kSteps[0] && frame >= kSteps[sNext].frame) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u\n", kName, frame);
        kSteps[sNext++].fn();
    }
    if (sNext == sizeof kSteps / sizeof kSteps[0]) {
        randoSmokeEnd(kName);
    }
}

} // namespace pc
