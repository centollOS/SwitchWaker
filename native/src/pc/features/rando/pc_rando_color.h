// pc_rando_color.h - colour math of the enemy palette randomizer (docs/RANDOMIZER_PLAN.md step R7.2a):
// a port of gclib's texture_utils.hsv_shift_color / hsv_shift_palette (gclib d9322523, MIT), which
// wwrando's randomizers/palettes.py applies to textures, palettes, material, particle and code
// colours with each group's (h_shift, v_shift).
//
// Bit exact with CPython 3.12: colorsys.rgb_to_hsv / hsv_to_rgb in double (Python's float `%` for the
// hue), gclib's int() truncations of h*360, s*100, v*100 and of the result channels *255, its value
// clamps (0..100, then 30 and 90 kept when the original was on the other side) and its saturation
// rule (relative to the value change; 0..100, then 5 and 80 kept likewise). The .cpp turns off
// floating-point contraction (no fused multiply-add), so the Switch's GCC and the Mac's clang give
// the same bytes as CPython. Checked by cos_rando_test_R7_2a against gclib (fixtures/R7_2a_*).
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and the game link it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace pcr {

// colorsys.rgb_to_hsv(r, g, b) and colorsys.hsv_to_rgb(h, s, v), components in [0, 1].
void rgbToHsv(double r, double g, double b, double* h, double* s, double* v);
void hsvToRgb(double h, double s, double v, double* r, double* g, double* b);

// texture_utils.hsv_shift_color on (r, g, b), each 0..255; the alpha channel, when there is one, is
// passed through by the callers unchanged. hShift is PaletteRandomizer's 20..340 and vShift its
// -40..40, but any int works as in Python (the hue is reduced mod 360, never negative).
void hsvShiftColor(uint8_t* r, uint8_t* g, uint8_t* b, int hShift, int vShift);

// texture_utils.hsv_shift_palette on n colours of `stride` bytes each, (r, g, b) first (stride 3 for
// RGB, 4 for RGBA; any further bytes are left alone).
void hsvShiftPalette(uint8_t* colors, size_t n, size_t stride, int hShift, int vShift);

} // namespace pcr

// The plan's names.
inline void pc_rando_hsv_shift_color(uint8_t* r, uint8_t* g, uint8_t* b, int hShift, int vShift) {
    pcr::hsvShiftColor(r, g, b, hShift, vShift);
}
inline void pc_rando_hsv_shift_palette(uint8_t* colors, size_t n, size_t stride, int hShift, int vShift) {
    pcr::hsvShiftPalette(colors, n, stride, hShift, vShift);
}
