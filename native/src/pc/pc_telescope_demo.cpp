// COS_SMOKE=telescope-demo (bug B6, docs/NATIVE_PORT_PLAN.md "Known bugs"): the telescope demo of the
// prologue (event telescope_demo: Link watches the Helmaroc King chase Tetra's ship through Aryll's
// telescope) keeps the full width of a widescreen picture. Run with
//   --stage sea:44:206 --input native/check/input/telescope-demo.txt [--aspect 16:9]
// The test gives the debug boot's new file event bit 0x2A80 (the grandmother's tunic done: Aryll
// waits on the lookout) and the telescope on X (COS_BOOT_EVENTS=2A80, COS_BOOT_ITEMS=20, unless
// the environment names others). The script plays Aryll's lookout event, raises the telescope,
// aims at Quill by the postbox, zooms in, looks up at Aryll's cry and lets the demo run.
//
// B6 (hardware, 16:9): from demo frame 425 to 1120 dScp_demoProc hides the telescope's wipe panels
// and the camera's cinemascope trim frames the picture, but the 16:9 code's black bars beside the
// telescope's 4:3 frame were still drawn, so the cutscene was a 4:3 window inside the screen.
// While the scope is in fopMsgStts_SCOPE_DEMO_e with the demo between frames kFirstDemoFrame and
// kLastDemoFrame, every kEvery game frames the presented picture is read back (pc_shot.cpp
// shotProbe) and the mean brightness of its left and right kEdge share of columns, over the rows
// between the trim bars, must be above kMinEdge (the sky and the sea, never black). Pass after
// kProbes probes; fail on a black edge or if the demo never got there by kLastFrame. Each probe
// logs a "[cos] telescope-demo:" line. Exit 0 or 1. Every pipeline is compiled before its first draw
// (COS_SYNC_PIPELINES on, pc_harness.cpp), so a probe never sees a draw skipped while compiling.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "f_op/f_op_msg.h"
#include "pc/pc_aspect.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr u32 kFirstDemoFrame = 460; // dScp_demoProc hides the panels at 425
constexpr u32 kLastDemoFrame = 1090; // and shows them again at 1120
constexpr unsigned int kEvery = 30;
constexpr unsigned int kProbes = 8;
constexpr unsigned int kLastFrame = 9000;
constexpr double kEdge = 0.04;
constexpr double kMinEdge = 24.0;

bool sChecked = false;
bool sOn = false;
bool sProbeNext = false;
unsigned int sLastProbe = 0;
unsigned int sProbes = 0;
u32 sDemoFrame = 0;

[[noreturn]] void fail(const char* why) {
    writef(STDERR_FILENO, "[cos] telescope-demo: FAIL: %s\n", why);
    pc_exit(PC_EXIT_CHECK_FAILED);
}

// Mean of (r + g + b) / 3 over columns [x0, x1) and rows [y0, y1).
double meanLuma(const std::vector<uint8_t>& rgb, uint32_t width, uint32_t x0, uint32_t x1, uint32_t y0,
                uint32_t y1) {
    double sum = 0.0;
    size_t n = 0;
    for (uint32_t y = y0; y < y1; y++) {
        const uint8_t* row = rgb.data() + (size_t)y * width * 3;
        for (uint32_t x = x0; x < x1; x++) {
            sum += (row[x * 3] + row[x * 3 + 1] + row[x * 3 + 2]) / 3.0;
            n++;
        }
    }
    return n != 0 ? sum / (double)n : 0.0;
}

} // namespace

void telescopeDemoFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "telescope-demo") != 0) {
            return;
        }
        const PcBootStage* boot = pc_boot_stage();
        if (boot == nullptr || strcmp(boot->stage, "sea") != 0 || boot->room != 44) {
            writef(STDERR_FILENO, "[cos] telescope-demo: needs COS_BOOT_STAGE sea:44:206 (the lookout) "
                                  "and COS_INPUT native/check/input/telescope-demo.txt\n");
            pc_exit(PC_EXIT_USAGE);
        }
        // Read by the debug boot in the logo scene, many frames from now.
        setenv("COS_BOOT_EVENTS", "2A80", 0);
        setenv("COS_BOOT_ITEMS", "20", 0);
        sOn = true;
        writef(STDERR_FILENO, "[cos] telescope-demo: aspect %s; probing the picture's edges every %u "
                              "frames of the telescope demo, frames %u..%u\n",
               pc_aspect_name(), kEvery, (unsigned int)kFirstDemoFrame, (unsigned int)kLastDemoFrame);
    }
    if (!sOn) {
        return;
    }
    if (frames > kLastFrame) {
        fail("the telescope demo's cinemascope part was not reached (or not probed enough)");
    }
    sProbeNext = false;
    if (dComIfGp_getMesgStatus() != fopMsgStts_SCOPE_DEMO_e) {
        return;
    }
    dDemo_manager_c* demo = dComIfGp_demo_get();
    if (demo == nullptr) {
        return;
    }
    const u32 demoFrame = demo->getFrameNoMsg();
    if (demoFrame < kFirstDemoFrame || demoFrame > kLastDemoFrame) {
        return;
    }
    if (sLastProbe != 0 && frames - sLastProbe < kEvery) {
        return;
    }
    sLastProbe = frames;
    sDemoFrame = demoFrame;
    sProbeNext = true;
}

void telescopeDemoFrameEnd(unsigned int frame) {
    if (!sOn || !sProbeNext) {
        return;
    }
    sProbeNext = false;
    double left = -1.0, right = -1.0, middle = -1.0, top = -1.0;
    uint32_t width = 0, height = 0;
    shotProbe(frame, [&](const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h) {
        width = w;
        height = h;
        const uint32_t edge = (uint32_t)(w * kEdge);
        const uint32_t y0 = h * 3 / 10, y1 = h * 7 / 10;
        left = meanLuma(rgb, w, 0, edge, y0, y1);
        right = meanLuma(rgb, w, w - edge, w, y0, y1);
        middle = meanLuma(rgb, w, w / 2 - edge / 2, w / 2 + edge / 2, y0, y1);
        top = meanLuma(rgb, w, w / 4, w * 3 / 4, 0, h / 50 + 1);
    });
    if (width == 0) {
        fail("the presented picture could not be read back");
    }
    sProbes++;
    writef(STDERR_FILENO, "[cos] telescope-demo: probe %u: game frame %u, demo frame %u, %ux%u: left edge "
                          "%.0f, middle %.0f, right edge %.0f, top rows %.0f (cinemascope bar)\n",
           sProbes, frame, (unsigned int)sDemoFrame, (unsigned int)width, (unsigned int)height, left,
           middle, right, top);
    if (left < kMinEdge || right < kMinEdge) {
        fail("black columns at the picture's left or right edge during the cinemascope demo");
    }
    if (sProbes >= kProbes) {
        writef(STDERR_FILENO, "[cos] telescope-demo: pass\n");
        pc_exit(PC_EXIT_REACHED);
    }
}

} // namespace pc
