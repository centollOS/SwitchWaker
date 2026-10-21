// COS_SMOKE=rando COS_RANDO_SMOKE=spoiler (docs/RANDOMIZER_PLAN.md step R4.5): the options menu's
// spoiler viewer (features/rando/pc_rando_spoiler_ui.h) on a seed generated in a booted game:
//   native/tools/run.sh rando --stage sea:44:206 --input native/check/input/rando-spoiler.txt \
//       --env COS_RANDO_SMOKE=spoiler --env COS_RANDO_SEED=TestSeed1 --env COS_SHOT_IMGUI=1 --shot 530,580,630
// The run has its own memory card (pc_rando_smoke_wants_run_card) for the sidecar, and the logs go to
// <run dir>/user/rando (COS_RANDO_DIR, set here), as in the gen smoke.
//   1. in play, before any seed: the Randomizer tab's last row is "Spoiler log..." (after Generate),
//      it shows "(no seed)" and A on it is refused;
//   2. Generate (R4.3) makes TestSeed1 the pending seed and writes its spoiler log;
//   3. frame 520: A on the row opens the page on its confirmation, the menu opens with it, and the
//      page holds the pending seed: its spoiler log found and matching, its hash, spheres 1..n from
//      the log (the Rooftop in one of them), every location of the placement by zone, equal to the
//      log's "All item locations";
//   4. the input script (read by the menu) presses A (the spheres), DOWN DOWN RIGHT (cursor on
//      "Sphere 2", checked at 570), R (the locations by zone) RIGHT RIGHT (the third zone, at 620),
//      then B (closed at 660; the menu shows its rows again).
// Shots (COS_SHOT_IMGUI=1 draws the menu in them): 530 the confirmation, 580 the spheres, 630 the
// locations by zone. Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_menu.h"
#include "features/rando/pc_rando_settings.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_spoiler.h"
#include "features/rando/pc_rando_spoiler_ui.h"
#include "pc/pc_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "spoiler";
constexpr unsigned int kOpenFrame = 520;
constexpr unsigned int kSphereFrame = 570;
constexpr unsigned int kZoneFrame = 620;
constexpr unsigned int kClosedFrame = 660;

int sPhase = 0;
bool sMenuChecked = false;

int actionRow(const std::vector<RandoRow>& rows, const char* label) {
    for (int i = 0; i < (int)rows.size(); i++) {
        if (rows[i].kind == RandoRowKind::Action && randoRowLabel(rows[i], false) == label) {
            return i;
        }
    }
    return -1;
}

RandoRow spoilerRow() {
    const std::vector<RandoRow> rows = randoMenuRows();
    const int r = actionRow(rows, "Spoiler log...");
    return r >= 0 ? rows[r] : RandoRow{RandoRowKind::Notice, -1};
}

void start() {
    const char* run = getenv("COS_RUN_DIR");
    const std::string dir = std::string(run != nullptr ? run : ".") + "/user/rando";
    setenv("COS_RANDO_DIR", dir.c_str(), 1);

    const std::vector<RandoRow> rows = randoMenuRows();
    const int genRow = actionRow(rows, "Generate");
    const int row = actionRow(rows, "Spoiler log...");
    randoSmokeCheck(kName, "the \"Spoiler log...\" row is after Generate, the tab's last",
                    genRow > 0 && row > genRow && row == (int)rows.size() - 1);
    if (row < 0) {
        randoSmokeEnd(kName);
    }
    randoSmokeCheck(kName, "no pending seed yet", pc_rando_slot_pending() == nullptr);
    const std::string value = randoRowValue(rows[row], false);
    randoSmokeCheck(kName, ("it shows no seed (\"" + value + "\")").c_str(), value == "(no seed)");
    const std::string toast = randoRowActivate(rows[row], 0, false);
    randoSmokeCheck(kName, ("A is refused (\"" + toast + "\")").c_str(),
                    toast == "No seed: generate one first" && !randoSpoilerActive());

    std::string msg;
    randoSmokeCheck(kName, ("Generate started (" + msg + ")").c_str(), randoGenerate(false, &msg));
}

void checkOpened() {
    const RandoGenLast& g = randoGenLast();
    const PcRandoSeed* pending = pc_rando_slot_pending();
    randoSmokeCheck(kName, ("TestSeed1 generated (" + g.error + ")").c_str(),
                    g.have && g.ok && pending != nullptr && pending->seed == "TestSeed1");
    if (pending == nullptr) {
        randoSmokeEnd(kName);
    }
    const RandoRow row = spoilerRow();
    const std::string value = randoRowValue(row, false);
    randoSmokeCheck(kName, ("the row offers it (\"" + value + "\")").c_str(), value == "A: view TestSeed1");
    const std::string toast = randoRowActivate(row, 0, false);
    const pcr::SpoilerPager& p = randoSpoilerPager();
    randoSmokeCheck(kName, ("A opens the confirmation (\"" + toast + "\")").c_str(),
                    toast.empty() && randoSpoilerActive() && p.state() == pcr::SpoilerState::Confirm);

    const RandoSpoilerInfo& info = randoSpoilerInfo();
    randoSmokeCheck(kName, "the pending seed (no file is played)", !info.active && info.seed == "TestSeed1");
    randoSmokeCheck(kName, ("its spoiler log " + info.logPath).c_str(),
                    info.logFound && info.logMatches && info.logPath == g.spoilerPath);
    randoSmokeCheck(kName, ("its hash (\"" + info.hash + "\")").c_str(), !info.hash.empty() && info.hash == g.hash);

    const std::vector<pcr::SpoilerLine>& spheres = p.lines(pcr::kSpoilerSpheres);
    int n = 0, rooftop = 0;
    bool numbered = true;
    std::string zone;
    for (const pcr::SpoilerLine& l : spheres) {
        if (l.depth == 0) {
            n++;
            numbered = numbered && l.text == "Sphere " + std::to_string(n);
        } else if (l.depth == 1) {
            zone = l.text;
        } else if (zone == "Ganon's Tower" && l.text == "Rooftop" && l.item == "Defeat Ganondorf") {
            rooftop++;
        }
    }
    char line[160];
    snprintf(line, sizeof line, "spheres 1..%d from the log, %d locations, the Rooftop once", n,
             p.entries(pcr::kSpoilerSpheres));
    randoSmokeCheck(kName, line, n >= 3 && numbered && rooftop == 1 && spheres.size() > 1 && spheres[1].depth == 1);
    snprintf(line, sizeof line, "every location (%d of %zu) in %d zones, as the log's", p.entries(pcr::kSpoilerZones),
             pending->location_names.size(), p.headings(pcr::kSpoilerZones));
    randoSmokeCheck(kName, line,
                    p.entries(pcr::kSpoilerZones) == (int)pending->location_names.size() &&
                        p.headings(pcr::kSpoilerZones) > 40 && info.zonesMatchLog);
}

std::string cursorText() {
    const pcr::SpoilerPager& p = randoSpoilerPager();
    return p.lines().empty() ? std::string() : p.lines()[p.cursor()].text;
}

} // namespace

void randoSmokeSpoiler(unsigned int frames) {
    (void)frames;
    const unsigned int frame = pc_frame_count();
    switch (sPhase) {
    case 0:
        if (!randoSmokeInPlay(10)) {
            return;
        }
        if (frame + 60 > kOpenFrame) {
            randoSmokeCheck(kName, "in play 60 frames before frame 520", false);
            randoSmokeEnd(kName);
        }
        randoMenuSetScripted(true); // the menu reads the COS_INPUT script
        start();
        sPhase = 1;
        return;
    case 1:
        if (randoGenBusy() || frame < kOpenFrame) {
            if (frame >= kOpenFrame) {
                randoSmokeCheck(kName, "the generation ended by frame 520", false);
                randoSmokeEnd(kName);
            }
            return;
        }
        checkOpened();
        sPhase = 2;
        return;
    case 2:
        if (!sMenuChecked && frame >= kOpenFrame + 2) {
            sMenuChecked = true;
            randoSmokeCheck(kName, "the menu opened with the page", menuOpen());
        }
        if (frame < kSphereFrame) {
            return;
        }
        {
            const pcr::SpoilerPager& p = randoSpoilerPager();
            const std::string text = cursorText();
            randoSmokeCheck(kName, ("A showed the spheres; DOWN DOWN RIGHT: the cursor on \"" + text + "\"").c_str(),
                            p.state() == pcr::SpoilerState::View && p.section() == pcr::kSpoilerSpheres &&
                                text == "Sphere 2" && p.lines()[p.cursor()].depth == 0);
        }
        sPhase = 3;
        return;
    case 3:
        if (frame < kZoneFrame) {
            return;
        }
        {
            const pcr::SpoilerPager& p = randoSpoilerPager();
            std::string third;
            int h = 0;
            for (const pcr::SpoilerLine& l : p.lines(pcr::kSpoilerZones)) {
                if (l.depth == 0 && ++h == 3) {
                    third = l.text;
                    break;
                }
            }
            const std::string text = cursorText();
            randoSmokeCheck(kName, ("R: the locations by zone; RIGHT RIGHT: the third zone \"" + text + "\"").c_str(),
                            p.state() == pcr::SpoilerState::View && p.section() == pcr::kSpoilerZones &&
                                !third.empty() && text == third);
        }
        sPhase = 4;
        return;
    default:
        if (frame < kClosedFrame) {
            return;
        }
        randoSmokeCheck(kName, "B closed the page; the menu shows its rows",
                        !randoSpoilerActive() && menuOpen() &&
                            randoRowValue(spoilerRow(), false) == "A: view TestSeed1");
        randoMenuSetScripted(false);
        randoSmokeEnd(kName);
    }
}

} // namespace pc
