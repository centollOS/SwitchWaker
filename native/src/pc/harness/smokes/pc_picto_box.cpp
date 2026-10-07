// COS_SMOKE=picto-box (bug B36): photos with the picto box. Run with
//   --stage Asoko:0:0:2 --input native/check/input/picto-box.txt [--env COS_BOOT_ITEMS=26]
// The test gives the debug boot's new file the picto box on X (COS_BOOT_ITEMS=23, unless the
// environment names another: 26 is the deluxe picto box). The script raises it with X and takes a
// photo with A, twice. Each photo's GXCopyTex must be read back into the game's buffer
// (pc_capture.cpp) with a picture in it (a luma range of at least kMinSpread, not the grey of a
// failed readback) in the box's format (I8, the deluxe box RGB565), and the capture thread's
// encode_s3tc must not panic (its JUT_ASSERT on the I8 range ended the game before the fix). Pass
// kSettleFrames after the second photo; fail if a readback failed, a photo is flat, or the photos
// were not taken by kLastFrame. Exit 0 or 1.
#include "pc_internal.h"

#include <dolphin/gx.h>

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kPhotos = 2;
constexpr unsigned int kSettleFrames = 60;
constexpr unsigned int kLastFrame = 1800;
constexpr unsigned int kMinSpread = 32;

bool sChecked = false;
bool sOn = false;
int sFormat = GX_TF_I8;
unsigned int sSeen = 0;
unsigned int sDoneFrame = 0;

[[noreturn]] void fail(const char* why) {
    writef(STDERR_FILENO, "[cos] picto-box: FAIL: %s\n", why);
    pc_exit(PC_EXIT_CHECK_FAILED);
}

} // namespace

void pictoBoxFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "picto-box") != 0) {
            return;
        }
        // Read by the debug boot in the logo scene, many frames from now.
        setenv("COS_BOOT_ITEMS", "23", 0);
        sFormat = strtoul(getenv("COS_BOOT_ITEMS"), nullptr, 16) == 0x26 ? GX_TF_RGB565 : GX_TF_I8;
        sOn = true;
        writef(STDERR_FILENO, "[cos] picto-box: item %s, %u photos expected as %s\n", getenv("COS_BOOT_ITEMS"),
               kPhotos, sFormat == GX_TF_I8 ? "I8" : "RGB565");
    }
    if (!sOn) {
        return;
    }
    const CaptureStats& stats = captureStats();
    if (stats.failed != 0) {
        fail("a photo's copy was not read back");
    }
    if (stats.readBack != sSeen) {
        sSeen = stats.readBack;
        if (stats.lastFormat != sFormat) {
            fail("the photo was copied in another format than the box's");
        }
        if (stats.maxLuma < stats.minLuma + kMinSpread) {
            fail("the photo is flat (no picture in the read-back copy)");
        }
        if (sSeen == kPhotos) {
            sDoneFrame = frames;
        }
    }
    if (sDoneFrame != 0 && frames >= sDoneFrame + kSettleFrames) {
        writef(STDERR_FILENO, "[cos] picto-box: pass (%u photos)\n", sSeen);
        pc_exit(PC_EXIT_REACHED);
    }
    if (frames > kLastFrame) {
        fail("the photos were not taken");
    }
}

} // namespace pc
