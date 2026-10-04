// Opt-in GPU-side reductions for A/B runs (native/include/pc/pc_gpu_opts.h).
#include "pc/pc_gpu_opts.h"

#include "pc_internal.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <lib/dolphin/vi/vi_internal.hpp>
#include <lib/window.hpp>

namespace {

// -1 until the first call reads the variable.
int sShadowOffscreen = -1;

bool envIs(const char* name, const char* value) {
    const char* v = getenv(name);
    return v != nullptr && strcmp(v, value) == 0;
}

} // namespace

extern "C" {

int pc_shadow_offscreen(void) {
    if (sShadowOffscreen < 0) {
        sShadowOffscreen = envIs("COS_SHADOW_OFFSCREEN", "1")    ? PC_SHADOW_OFFSCREEN_SAME
                           : envIs("COS_SHADOW_OFFSCREEN", "gc") ? PC_SHADOW_OFFSCREEN_GC
                                                                 : PC_SHADOW_OFFSCREEN_OFF;
        if (sShadowOffscreen == PC_SHADOW_OFFSCREEN_SAME) {
            pc::writef(STDERR_FILENO, "[cos] COS_SHADOW_OFFSCREEN=1: real-time shadows drawn offscreen\n");
        } else if (sShadowOffscreen == PC_SHADOW_OFFSCREEN_GC) {
            pc::writef(STDERR_FILENO,
                       "[cos] COS_SHADOW_OFFSCREEN=gc: real-time shadows drawn offscreen at the GameCube's "
                       "256x256 (128x128 textures)\n");
        }
    }
    return sShadowOffscreen;
}

void pc_efb_pixel_size(unsigned int logicalW, unsigned int logicalH, unsigned int* outW,
                       unsigned int* outH) {
    const auto [logicalFbW, logicalFbH] = aurora::vi::configured_fb_size();
    const AuroraWindowSize window = aurora::window::get_window_size();
    unsigned int w = logicalW;
    unsigned int h = logicalH;
    if (logicalFbW != 0 && logicalFbH != 0 && window.fb_width != 0 && window.fb_height != 0) {
        const float sx = static_cast<float>(window.fb_width) / static_cast<float>(logicalFbW);
        const float sy = static_cast<float>(window.fb_height) / static_cast<float>(logicalFbH);
        w = static_cast<unsigned int>(std::lround(static_cast<float>(logicalW) * sx));
        h = static_cast<unsigned int>(std::lround(static_cast<float>(logicalH) * sy));
    }
    *outW = w != 0 ? w : 1;
    *outH = h != 0 ? h : 1;
}

void pc_shadow_offscreen_opened(unsigned int w, unsigned int h, unsigned int copyW, unsigned int copyH) {
    static unsigned int sLastW = 0, sLastH = 0;
    if (w != sLastW || h != sLastH) {
        sLastW = w;
        sLastH = h;
        pc::writef(STDERR_FILENO, "[cos] shadow offscreen target %ux%u, I4 copies %ux%u (frame %u)\n", w, h,
                   copyW, copyH, pc_frame_count());
    }
}

} // extern "C"
