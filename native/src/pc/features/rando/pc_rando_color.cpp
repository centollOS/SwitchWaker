// pc_rando_color.cpp - colour math of the enemy palette randomizer (docs/RANDOMIZER_PLAN.md step R7.2a);
// see pc_rando_color.h.
//
// No floating-point contraction here: CPython evaluates every * and - separately, and a fused
// multiply-add (clang's default "on" within an expression, GCC's default "fast" for GNU C++) would
// round differently and change a channel by one in rare cases.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#include "pc_rando_color.h"

#include <cmath>

namespace pcr {

namespace {

// Python's float `x % y` for y > 0 (floatobject.c float_rem): fmod, then the divisor's sign.
double pyFloatMod(double x, double y) {
    double mod = std::fmod(x, y);
    if (mod != 0.0) {
        if ((y < 0.0) != (mod < 0.0)) {
            mod += y;
        }
    } else {
        mod = std::copysign(0.0, y);
    }
    return mod;
}

// Python's int `x % y` for y > 0.
int pyIntMod(int x, int y) {
    const int m = x % y;
    return m < 0 ? m + y : m;
}

// int(x) of a Python float: truncation toward zero (the values here are never out of int range).
int pyInt(double x) { return (int)x; }

} // namespace

// colorsys.rgb_to_hsv (CPython 3.12).
void rgbToHsv(double r, double g, double b, double* h, double* s, double* v) {
    const double maxc = std::fmax(std::fmax(r, g), b);
    const double minc = std::fmin(std::fmin(r, g), b);
    const double rangec = maxc - minc;
    *v = maxc;
    if (minc == maxc) {
        *h = 0.0;
        *s = 0.0;
        return;
    }
    *s = rangec / maxc;
    const double rc = (maxc - r) / rangec;
    const double gc = (maxc - g) / rangec;
    const double bc = (maxc - b) / rangec;
    double hh;
    if (r == maxc) {
        hh = bc - gc;
    } else if (g == maxc) {
        hh = 2.0 + rc - bc;
    } else {
        hh = 4.0 + gc - rc;
    }
    *h = pyFloatMod(hh / 6.0, 1.0);
}

// colorsys.hsv_to_rgb (CPython 3.12).
void hsvToRgb(double h, double s, double v, double* r, double* g, double* b) {
    if (s == 0.0) {
        *r = *g = *b = v;
        return;
    }
    int i = pyInt(h * 6.0);
    const double f = (h * 6.0) - i;
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - s * f);
    const double t = v * (1.0 - s * (1.0 - f));
    i = pyIntMod(i, 6);
    switch (i) {
    case 0: *r = v; *g = t; *b = p; return;
    case 1: *r = q; *g = v; *b = p; return;
    case 2: *r = p; *g = v; *b = t; return;
    case 3: *r = p; *g = q; *b = v; return;
    case 4: *r = t; *g = p; *b = v; return;
    default: *r = v; *g = p; *b = q; return;
    }
}

// gclib texture_utils.hsv_shift_color.
void hsvShiftColor(uint8_t* r8, uint8_t* g8, uint8_t* b8, int hShift, int vShift) {
    double hf, sf, vf;
    rgbToHsv(*r8 / 255.0, *g8 / 255.0, *b8 / 255.0, &hf, &sf, &vf);
    int h = pyInt(hf * 360);
    int s = pyInt(sf * 100);
    int v = pyInt(vf * 100);

    h += hShift;
    h = pyIntMod(h, 360);

    const int origV = v;
    v += vShift;
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    if (v < 30 && origV >= 30) {
        v = 30;
    }
    if (v > 90 && origV <= 90) {
        v = 90;
    }
    const int vDiff = v - origV;

    // Saturation follows the value change (brighter: less saturated), except for colours that were
    // nearly grey, which get more saturated when the value goes up.
    const int origS = s;
    if (origS < 15 && vShift > 0) {
        if (origV < 30) {
            s += vShift * 2;
        } else {
            s += vShift;
        }
    } else {
        s -= vDiff;
    }
    if (s < 0) {
        s = 0;
    }
    if (s > 100) {
        s = 100;
    }
    if (s < 5 && origS >= 5) {
        s = 5;
    }
    if (s > 80 && origS <= 80) {
        s = 80;
    }

    double r, g, b;
    hsvToRgb(h / 360.0, s / 100.0, v / 100.0, &r, &g, &b);
    *r8 = (uint8_t)pyInt(r * 255);
    *g8 = (uint8_t)pyInt(g * 255);
    *b8 = (uint8_t)pyInt(b * 255);
}

// gclib texture_utils.hsv_shift_palette.
void hsvShiftPalette(uint8_t* colors, size_t n, size_t stride, int hShift, int vShift) {
    for (size_t i = 0; i < n; i++) {
        uint8_t* c = colors + i * stride;
        hsvShiftColor(&c[0], &c[1], &c[2], hShift, vShift);
    }
}

} // namespace pcr
