// The cosmetic tweaks wwrando always applies (pc_rando_tweak_cosmetic.h, docs/RANDOMIZER_PLAN.md step
// R7.5a): the Rainbow Rupee's key frame (misc_rando_features.asm check_animate_rainbow_rupee_color) and
// give_fairy_fountains_distinct_colors (tweaks.py) on the raw stage.dzs bytes, through the dzx library
// of step R0.9. Pure: no game headers, no game calls.
#include "pc_rando_tweak_cosmetic.h"

#include "pc_rando_dzx.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

// Python's float arithmetic rounds every operation: no fused multiply-add (clang contracts
// v * (1.0 - s * f) into an fma on arm64 by default, which changes int() of a channel by 1).
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

// rainbow_rupee_keyframe: .float 0.0 (current), 0.15, 6.0, -6.0.
float sKeyframe = 0.0f;
uint32_t sDraws = 0;

bool sInit = false;
PcRandoCosmeticStatus sStatus[PCR_COSMETIC_FAIRY_STAGES];
bool sStatusInit = false;

void resetStatus() {
    for (PcRandoCosmeticStatus& s : sStatus) {
        s = PcRandoCosmeticStatus{0, 0, -2};
    }
    sStatusInit = true;
}

constexpr const char* kFairyStages[PCR_COSMETIC_FAIRY_STAGES] = {"Fairy01", "Fairy02", "Fairy03",
                                                                "Fairy04", "Fairy05", "Fairy06"};
// wwlib/dzx.py Pale: actor_c0, actor_k0, bg0_c0 (+0x06), bg0_k0 (+0x09), ...; DATA_SIZE 0x2C.
constexpr uint32_t kPaleSize = 0x2C;
constexpr uint32_t kPaleColors[2] = {0x06, 0x09};

} // namespace

// --- Rainbow Rupee ---------------------------------------------------------------------------

extern "C" float pc_rando_cosmetic_rainbow_advance(float* keyframe) {
    // lfs f1 (current); lfs f0 (0.15); fadds f1, f1, f0: a single-precision add.
    float f1 = *keyframe + PCR_COSMETIC_RAINBOW_STEP;
    // fcmpo f1, 6.0; blt store: anything not below the maximum restarts at the minimum.
    if (!(f1 < PCR_COSMETIC_RAINBOW_MAX)) {
        f1 = PCR_COSMETIC_RAINBOW_MIN;
    }
    *keyframe = f1;
    return std::fabs(f1);
}

extern "C" float pc_rando_cosmetic_rainbow_step(void) {
    sDraws++;
    return pc_rando_cosmetic_rainbow_advance(&sKeyframe);
}

extern "C" float pc_rando_cosmetic_rainbow_keyframe(void) {
    return sKeyframe;
}

extern "C" void pc_rando_cosmetic_rainbow_set_keyframe(float keyframe) {
    sKeyframe = keyframe;
}

extern "C" uint32_t pc_rando_cosmetic_rainbow_draws(void) {
    return sDraws;
}

// --- Fairy fountain colours ------------------------------------------------------------------

extern "C" int pc_rando_cosmetic_fairy_index(const char* stage) {
    if (stage == nullptr) {
        return -1;
    }
    for (int k = 0; k < PCR_COSMETIC_FAIRY_STAGES; k++) {
        if (strcmp(stage, kFairyStages[k]) == 0) {
            return k;
        }
    }
    return -1;
}

extern "C" double pc_rando_cosmetic_fairy_hue(int k) {
    // hue = 0; per stage: use it, then hue += 1.0 / len(stage_group). Summed, not k / 6.0, so the
    // doubles are the ones wwrando uses.
    double hue = 0.0;
    for (int i = 0; i < k; i++) {
        hue += 1.0 / PCR_COSMETIC_FAIRY_STAGES;
    }
    return hue;
}

extern "C" void pc_rando_cosmetic_set_hue(uint8_t rgb[3], double hue) {
    // colorsys.rgb_to_hsv(r/255, g/255, b/255): only s and v are kept (h is replaced).
    const double r = rgb[0] / 255.0;
    const double g = rgb[1] / 255.0;
    const double b = rgb[2] / 255.0;
    const double maxc = std::fmax(r, std::fmax(g, b));
    const double minc = std::fmin(r, std::fmin(g, b));
    const double v = maxc;
    double s = 0.0;
    if (minc != maxc) {
        s = (maxc - minc) / maxc;
    }
    // colorsys.hsv_to_rgb(h, s, v).
    double outR, outG, outB;
    if (s == 0.0) {
        outR = outG = outB = v;
    } else {
        const int i0 = (int)(hue * 6.0); // int() truncates (h >= 0 here)
        const double f = (hue * 6.0) - i0;
        const double p = v * (1.0 - s);
        const double q = v * (1.0 - s * f);
        const double t = v * (1.0 - s * (1.0 - f));
        const int i = ((i0 % 6) + 6) % 6; // Python's i % 6
        switch (i) {
        case 0: outR = v; outG = t; outB = p; break;
        case 1: outR = q; outG = v; outB = p; break;
        case 2: outR = p; outG = v; outB = t; break;
        case 3: outR = p; outG = q; outB = v; break;
        case 4: outR = t; outG = p; outB = v; break;
        default: outR = v; outG = p; outB = q; break;
        }
    }
    // int(x * 255): truncation toward zero; every channel is in [0, 1].
    rgb[0] = (uint8_t)(int)(outR * 255);
    rgb[1] = (uint8_t)(int)(outG * 255);
    rgb[2] = (uint8_t)(int)(outB * 255);
}

extern "C" int pc_rando_cosmetic_fairy_patch_dzs(uint8_t* data, size_t size, int k) {
    if (k < 0 || k >= PCR_COSMETIC_FAIRY_STAGES) {
        return -1;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, data, size) != PCR_DZX_OK) {
        return -1;
    }
    if (pc_rando_dzx_entry_size("Pale") != kPaleSize) {
        return -1;
    }
    const uint32_t count = pc_rando_dzx_count(&dzx, "Pale");
    if (count == 0) {
        return -1; // D9: every vanilla Fairy0N stage.dzs has a Pale chunk
    }
    // Check every entry first, so a refused file is left whole.
    for (uint32_t i = 0; i < count; i++) {
        if (pc_rando_dzx_entry(&dzx, "Pale", i) == nullptr) {
            return -1;
        }
    }
    const double hue = pc_rando_cosmetic_fairy_hue(k);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t* e = pc_rando_dzx_entry(&dzx, "Pale", i);
        for (uint32_t off : kPaleColors) {
            uint8_t rgb[3] = {e[off], e[off + 1], e[off + 2]};
            pc_rando_cosmetic_set_hue(rgb, hue);
            pc_rando_dzx_set_field(&dzx, "Pale", i, off, rgb, 3, nullptr);
        }
    }
    return (int)count;
}

extern "C" void pc_rando_tweak_cosmetic_patch(PcRandoResFile* file) {
    if (file == nullptr || file->arc == nullptr || file->name == nullptr || strcmp(file->arc, "Stage") != 0 ||
        strcmp(file->name, "stage.dzs") != 0) {
        return;
    }
    const int k = pc_rando_cosmetic_fairy_index(file->stage);
    if (k < 0) {
        return;
    }
    if (!sStatusInit) {
        resetStatus();
    }
    const int r = pc_rando_cosmetic_fairy_patch_dzs(file->data, file->size, k);
    sStatus[k].seen++;
    sStatus[k].last = r;
    if (r > 0) {
        sStatus[k].patched++;
        logf("[cos] rando cosmetic: %s/stage.dzs: %d Pale record(s) at hue %d/6\n", file->stage, r, k);
    } else {
        logf("[cos] rando cosmetic: %s/stage.dzs: not the vanilla Pale chunk, left vanilla\n", file->stage);
    }
}

extern "C" void pc_rando_tweak_cosmetic_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    if (!sStatusInit) {
        resetStatus();
    }
    pc_rando_register_dzx_patcher(pc_rando_tweak_cosmetic_patch);
}

extern "C" void pc_rando_tweak_cosmetic_status(int k, PcRandoCosmeticStatus* out) {
    if (out == nullptr) {
        return;
    }
    if (!sStatusInit) {
        resetStatus();
    }
    if (k < 0 || k >= PCR_COSMETIC_FAIRY_STAGES) {
        *out = PcRandoCosmeticStatus{0, 0, -2};
        return;
    }
    *out = sStatus[k];
}

extern "C" void pc_rando_tweak_cosmetic_reset_status(void) {
    resetStatus();
}
