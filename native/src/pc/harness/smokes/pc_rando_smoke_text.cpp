// COS_SMOKE=rando COS_RANDO_SMOKE=text (docs/RANDOMIZER_PLAN.md step R4.2): the randomizer's text
// input in a booted game, driven through the options menu by an input script:
//   native/tools/run.sh rando --stage sea:44:206 --input native/check/input/rando-text.txt \
//       --env COS_RANDO_SMOKE=text
//   1. at frame 360 (in play) it opens the seed input (pc_rando_text_ui.h) with the script flag: the
//      menu opens to host it;
//   2. the script types "TestSeed1" on the D-pad grid (a typo deleted with B) and presses OK: the
//      callback gets OK and "TestSeed1"; the menu stayed open meanwhile and shows its rows again;
//   3. a second input, empty, is cancelled by B (frame 640): the callback gets "cancelled";
//   4. the permalink import: <COS_RANDO_DIR>/permalink.txt (here <run dir>/rando, written with a BOM
//      and CRLF) gives wwrando's TestSeed1 permalink; without the file: "no file"; the default folder
//      is <userPath>/rando.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_text_ui.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "text";
constexpr unsigned int kOpenFrame = 360;  // the script's first key is at 400
constexpr unsigned int kSecondFrame = 600; // the first input ends at 576
constexpr unsigned int kLastFrame = 720;   // the second ends at 640
// wwrando 1.10.0_9775811's permalink for TestSeed1 with its default options
// (native/check/rando/fixtures/R4_2_permalinks.txt).
constexpr const char* kPermalink = "MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAEkQUBgAAPsCyCUAAAAAAAEAQQCAAA==";

int sPhase = 0;
int sCalls = 0;
bool sOk = false;
std::string sText;
bool sClosedWhileTyping = false;

void done(bool ok, const char* text, void* user) {
    sCalls++;
    sOk = ok;
    sText = text;
    *static_cast<unsigned int*>(user) = pc_frame_count();
}

unsigned int sDoneFrame = 0;

void checkPermalinkImport() {
    const char* runDir = getenv("COS_RUN_DIR");
    const std::string dir = std::string(runDir != nullptr ? runDir : ".") + "/rando";
    unsetenv("COS_RANDO_DIR");
    const std::string def = randoUserDir();
    randoSmokeCheck(kName, ("default folder <userPath>/rando: " + def).c_str(),
                    def.size() > 11 && def.compare(def.size() - 11, 11, "/user/rando") == 0);
    setenv("COS_RANDO_DIR", dir.c_str(), 1);
    mkdir(dir.c_str(), 0755);
    const std::string file = dir + "/permalink.txt";
    unlink(file.c_str());
    std::string path;
    randoSmokeCheck(kName, "no permalink.txt: no file",
                    randoPermalinkImport(&path).status == rando::PermalinkStatus::NoFile && path == file);
    FILE* f = fopen(file.c_str(), "wb");
    if (f != nullptr) {
        fprintf(f, "\xEF\xBB\xBF%s\r\n", kPermalink);
        fclose(f);
    }
    const rando::PermalinkInfo info = randoPermalinkImport();
    randoSmokeCheck(kName, "permalink.txt imported", info.status == rando::PermalinkStatus::Ok);
    randoSmokeCheck(kName, "its seed is TestSeed1", info.seed == "TestSeed1");
    randoSmokeCheck(kName, "its text is the permalink", info.text == kPermalink);
}

} // namespace

void randoSmokeText(unsigned int frames) {
    (void)frames;
    const unsigned int frame = pc_frame_count();
    if (sPhase == 0) {
        if (!randoSmokeInPlay(10) || frame < kOpenFrame) {
            if (frame >= kOpenFrame + 30) {
                randoSmokeCheck(kName, "in play by frame 390 (the script starts at 400)", false);
                randoSmokeEnd(kName);
            }
            return;
        }
        RandoTextRequest req;
        req.kind = rando::TextKind::Seed;
        req.done = done;
        req.user = &sDoneFrame;
        req.script = true;
        randoSmokeCheck(kName, "seed input opened", randoTextOpen(req));
        randoSmokeCheck(kName, "a second session is refused", !randoTextOpen(req));
        sPhase = 1;
        return;
    }
    if (sPhase == 1) {
        if (randoTextActive()) {
            sClosedWhileTyping = sClosedWhileTyping || (frame > kOpenFrame + 2 && !menuOpen());
            if (frame > kSecondFrame) {
                const rando::TextGrid* g = randoTextGrid();
                randoSmokeCheck(kName, ("the first input ended by frame 600 (text \"" + (g ? g->text() : "") + "\")").c_str(),
                                false);
                randoSmokeEnd(kName);
            }
            return;
        }
        randoSmokeCheck(kName, "the menu hosted the input while it ran", !sClosedWhileTyping);
        randoSmokeCheck(kName, "done called once", sCalls == 1);
        randoSmokeCheck(kName, "ended with OK", sOk);
        randoSmokeCheck(kName, ("typed \"TestSeed1\" (got \"" + sText + "\")").c_str(), sText == "TestSeed1");
        randoSmokeCheck(kName, "ended by the script's last A (frame 576)", sDoneFrame >= 576 && sDoneFrame <= 580);
        sPhase = 2;
        return;
    }
    if (sPhase == 2) {
        if (frame < kSecondFrame) {
            return;
        }
        randoSmokeCheck(kName, "the menu is still open after OK", menuOpen());
        RandoTextRequest req;
        req.kind = rando::TextKind::Permalink;
        req.titleEs = "Permalink";
        req.titleEn = "Permalink";
        req.done = done;
        req.user = &sDoneFrame;
        req.script = true;
        randoSmokeCheck(kName, "permalink input opened", randoTextOpen(req));
        sPhase = 3;
        return;
    }
    if (sPhase == 3) {
        if (randoTextActive() && frame < kLastFrame) {
            return;
        }
        randoSmokeCheck(kName, "B with no text cancelled the second input",
                        !randoTextActive() && sCalls == 2 && !sOk && sText.empty());
        randoSmokeCheck(kName, "cancelled at the script's B (frame 640)", sDoneFrame >= 640 && sDoneFrame <= 642);
        checkPermalinkImport();
        randoSmokeEnd(kName);
    }
}

} // namespace pc
