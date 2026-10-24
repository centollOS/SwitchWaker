// pc_rando_texture.cpp - GX texture codec of the randomizer (docs/RANDOMIZER_PLAN.md, step R7.2b).
// A line-by-line port of gclib texture_utils.py / bti.py (MIT); see pc_rando_texture.h. The function
// names follow gclib's so the two can be read side by side.

// The colour maths must round like CPython's: no fused multiply-adds (clang contracts a*b+c within
// an expression by default, GCC across statements).
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#include "pc_rando_texture.h"

#include <math.h>
#include <string.h>

#include <unordered_map>
#include <vector>

namespace {

typedef uint8_t Rgba[4];

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
void wr16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void setRgba(uint8_t* c, int r, int g, int b, int a) {
    c[0] = (uint8_t)r;
    c[1] = (uint8_t)g;
    c[2] = (uint8_t)b;
    c[3] = (uint8_t)a;
}
bool sameRgba(const uint8_t* x, const uint8_t* y) {
    return memcmp(x, y, 4) == 0;
}
uint32_t packRgba(const uint8_t* c) {
    return ((uint32_t)c[0] << 24) | ((uint32_t)c[1] << 16) | ((uint32_t)c[2] << 8) | c[3];
}

// --- colour conversions (texture_utils.py, top) ---

int swz3(int v) {
    return ((v << 5) | (v << 2) | (v >> 1)) & 0xFF;
}
int swz4(int v) {
    return ((v << 4) | v) & 0xFF;
}
int swz5(int v) {
    return ((v << 3) | (v >> 2)) & 0xFF;
}
int swz6(int v) {
    return ((v << 2) | (v >> 4)) & 0xFF;
}

// round(((r*30) + (g*59) + (b*11)) / 100) with Python's round half to even.
int greyscale(int r, int g, int b) {
    const int n = r * 30 + g * 59 + b * 11;
    const int q = n / 100;
    const int rem = n % 100;
    if (rem > 50) {
        return q + 1;
    }
    if (rem == 50) {
        return q + (q & 1);
    }
    return q;
}

void rgb565ToColor(uint16_t v, uint8_t* c) {
    setRgba(c, swz5((v >> 11) & 0x1F), swz6((v >> 5) & 0x3F), swz5(v & 0x1F), 255);
}
uint16_t colorToRgb565(const uint8_t* c) {
    return (uint16_t)((((c[0] >> 3) & 0x1F) << 11) | (((c[1] >> 2) & 0x3F) << 5) | ((c[2] >> 3) & 0x1F));
}
void rgb5a3ToColor(uint16_t v, uint8_t* c) {
    if ((v & 0x8000) == 0) {
        setRgba(c, swz4((v >> 8) & 0xF), swz4((v >> 4) & 0xF), swz4(v & 0xF), swz3((v >> 12) & 0x7));
    } else {
        setRgba(c, swz5((v >> 10) & 0x1F), swz5((v >> 5) & 0x1F), swz5(v & 0x1F), 255);
    }
}
uint16_t colorToRgb5a3(const uint8_t* c) {
    if (c[3] != 255) {
        return (uint16_t)((((c[3] >> 5) & 0x7) << 12) | (((c[0] >> 4) & 0xF) << 8) | (((c[1] >> 4) & 0xF) << 4) |
                          ((c[2] >> 4) & 0xF));
    }
    return (uint16_t)(0x8000 | (((c[0] >> 3) & 0x1F) << 10) | (((c[1] >> 3) & 0x1F) << 5) | ((c[2] >> 3) & 0x1F));
}
void ia4ToColor(uint8_t v, uint8_t* c) {
    const int l = swz4(v & 0xF);
    setRgba(c, l, l, l, swz4((v >> 4) & 0xF));
}
uint8_t colorToIa4(const uint8_t* c) {
    const int l = greyscale(c[0], c[1], c[2]);
    return (uint8_t)(((l >> 4) & 0xF) | (c[3] & 0xF0));
}
void ia8ToColor(uint16_t v, uint8_t* c) {
    setRgba(c, v & 0xFF, v & 0xFF, v & 0xFF, (v >> 8) & 0xFF);
}
uint16_t colorToIa8(const uint8_t* c) {
    const int l = greyscale(c[0], c[1], c[2]);
    return (uint16_t)((l & 0xFF) | ((c[3] << 8) & 0xFF00));
}
uint8_t colorToI4(const uint8_t* c) {
    return (uint8_t)((greyscale(c[0], c[1], c[2]) >> 4) & 0xF);
}
uint8_t colorToI8(const uint8_t* c) {
    return (uint8_t)(greyscale(c[0], c[1], c[2]) & 0xFF);
}

// get_interpolated_cmpr_colors
void cmprColors(uint16_t c0, uint16_t c1, uint8_t out[4][4]) {
    rgb565ToColor(c0, out[0]);
    rgb565ToColor(c1, out[1]);
    const int r0 = out[0][0], g0 = out[0][1], b0 = out[0][2];
    const int r1 = out[1][0], g1 = out[1][1], b1 = out[1][2];
    if (c0 > c1) {
        setRgba(out[2], (2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
        setRgba(out[3], (r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
    } else {
        setRgba(out[2], r0 / 2 + r1 / 2, g0 / 2 + g1 / 2, b0 / 2 + b1 / 2, 255);
        setRgba(out[3], 0, 0, 0, 0);
    }
}

int colorDistance(const uint8_t* x, const uint8_t* y) {
    return abs(x[0] - y[0]) + abs(x[1] - y[1]) + abs(x[2] - y[2]) + abs(x[3] - y[3]);
}

// get_best_cmpr_key_colors (the pure-Python path: pyfastbti is not installed with wwrando's oracle).
void bestCmprKeyColors(const uint8_t (*colors)[4], int count, uint8_t k1[4], uint8_t k2[4]) {
    int maxDist = -1;
    int i1 = -1, i2 = -1;
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            const int d = colorDistance(colors[i], colors[j]);
            if (d > maxDist) {
                maxDist = d;
                i1 = i;
                i2 = j;
            }
        }
    }
    if (maxDist == -1) {
        setRgba(k1, 0, 0, 0, 0xFF);
        setRgba(k2, 0xFF, 0xFF, 0xFF, 0xFF);
        return;
    }
    setRgba(k1, colors[i1][0], colors[i1][1], colors[i1][2], 0xFF);
    setRgba(k2, colors[i2][0], colors[i2][1], colors[i2][2], 0xFF);
    const int r1 = k1[0], g1 = k1[1], b1 = k1[2];
    const int r2 = k2[0], g2 = k2[1], b2 = k2[2];
    if ((r1 >> 3) == (r2 >> 3) && (g1 >> 2) == (g2 >> 2) && (b1 >> 3) == (b2 >> 3)) {
        if ((r1 >> 3) == 0 && (g1 >> 2) == 0 && (b1 >> 3) == 0) {
            setRgba(k2, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
            setRgba(k2, 0, 0, 0, 0xFF);
        }
    }
}

// colors.index(get_nearest_color_fast(color, colors)), or colors.index(color) when it is in colors.
int cmprIndex(const uint8_t* color, uint8_t pal[4][4]) {
    for (int i = 0; i < 4; i++) {
        if (sameRgba(color, pal[i])) {
            return i;
        }
    }
    int best = 0;
    if (color[3] < 16) {
        for (int i = 0; i < 4; i++) {
            if (pal[i][3] == 0) {
                best = i;
                goto found;
            }
        }
    }
    {
        int minDist = 0x7FFFFFFF;
        for (int i = 0; i < 4; i++) {
            const int d = colorDistance(color, pal[i]);
            if (d < minDist) {
                best = i;
                if (d == 0) {
                    break;
                }
                minDist = d;
            }
        }
    }
found:
    // list.index of the value found: the first equal entry.
    for (int i = 0; i < 4; i++) {
        if (sameRgba(pal[i], pal[best])) {
            return i;
        }
    }
    return best;
}

// --- blocks ---

// decode_block: the block's pixels in block order (block_width per row) into px.
// valid[i] = 0 marks a palette index past the palette.
void decodeBlock(int format, const uint8_t* d, const uint8_t (*colors)[4], int numColors, uint8_t (*px)[4]) {
    switch (format) {
    case PCR_TEX_I4:
        for (int i = 0; i < 32; i++) {
            for (int n = 0; n < 2; n++) {
                const int l = swz4((d[i] >> ((1 - n) * 4)) & 0xF);
                setRgba(px[i * 2 + n], l, l, l, l);
            }
        }
        break;
    case PCR_TEX_I8:
        for (int i = 0; i < 32; i++) {
            setRgba(px[i], d[i], d[i], d[i], d[i]);
        }
        break;
    case PCR_TEX_IA4:
        for (int i = 0; i < 32; i++) {
            ia4ToColor(d[i], px[i]);
        }
        break;
    case PCR_TEX_IA8:
        for (int i = 0; i < 16; i++) {
            ia8ToColor(rd16(d + i * 2), px[i]);
        }
        break;
    case PCR_TEX_RGB565:
        for (int i = 0; i < 16; i++) {
            rgb565ToColor(rd16(d + i * 2), px[i]);
        }
        break;
    case PCR_TEX_RGB5A3:
        for (int i = 0; i < 16; i++) {
            rgb5a3ToColor(rd16(d + i * 2), px[i]);
        }
        break;
    case PCR_TEX_RGBA8:
        for (int i = 0; i < 16; i++) {
            setRgba(px[i], d[i * 2 + 1], d[i * 2 + 32], d[i * 2 + 33], d[i * 2]);
        }
        break;
    case PCR_TEX_C4:
        for (int i = 0; i < 32; i++) {
            for (int n = 0; n < 2; n++) {
                const int idx = (d[i] >> ((1 - n) * 4)) & 0xF;
                if (idx < numColors) {
                    memcpy(px[i * 2 + n], colors[idx], 4);
                } else {
                    setRgba(px[i * 2 + n], 0, 0, 0, 0);
                }
            }
        }
        break;
    case PCR_TEX_C8:
        for (int i = 0; i < 32; i++) {
            if (d[i] < numColors) {
                memcpy(px[i], colors[d[i]], 4);
            } else {
                setRgba(px[i], 0, 0, 0, 0);
            }
        }
        break;
    case PCR_TEX_C14X2:
        for (int i = 0; i < 16; i++) {
            const int idx = rd16(d + i * 2) & 0x3FFF;
            if (idx < numColors) {
                memcpy(px[i], colors[idx], 4);
            } else {
                setRgba(px[i], 0, 0, 0, 0);
            }
        }
        break;
    case PCR_TEX_CMPR:
        for (int sb = 0; sb < 4; sb++) {
            const uint8_t* s = d + sb * 8;
            uint8_t pal[4][4];
            cmprColors(rd16(s), rd16(s + 2), pal);
            const uint32_t idx = rd32(s + 4);
            const int sx = (sb % 2) * 4, sy = (sb / 2) * 4;
            for (int i = 0; i < 16; i++) {
                const int ci = (int)((idx >> ((15 - i) * 2)) & 3);
                memcpy(px[sx + sy * 8 + (i / 4) * 8 + (i % 4)], pal[ci], 4);
            }
        }
        break;
    default:
        break;
    }
}

struct Img {
    const uint8_t* rgba;
    int w, h;
    bool in(int x, int y) const {
        return x < w && y < h;
    }
    const uint8_t* at(int x, int y) const {
        return rgba + ((size_t)y * w + x) * 4;
    }
};

// encode_image_to_block; idx maps a packed RGBA to its palette index (palette formats only).
void encodeBlock(int format, const Img& im, const std::unordered_map<uint32_t, int>* idx, int bx, int by,
                 uint8_t* o) {
    const int bw = pc_rando_tex_block_width(format), bh = pc_rando_tex_block_height(format);
    int off = 0;
    switch (format) {
    case PCR_TEX_I4:
    case PCR_TEX_C4:
        for (int y = by; y < by + bh; y++) {
            for (int x = bx; x < bx + bw; x += 2) {
                int c1 = 0xF, c2 = 0xF;
                if (im.in(x, y)) {
                    c1 = format == PCR_TEX_I4 ? colorToI4(im.at(x, y)) : idx->at(packRgba(im.at(x, y)));
                }
                if (im.in(x + 1, y)) {
                    c2 = format == PCR_TEX_I4 ? colorToI4(im.at(x + 1, y)) : idx->at(packRgba(im.at(x + 1, y)));
                }
                o[off++] = (uint8_t)(((c1 & 0xF) << 4) | (c2 & 0xF));
            }
        }
        break;
    case PCR_TEX_I8:
    case PCR_TEX_IA4:
    case PCR_TEX_C8:
        for (int y = by; y < by + bh; y++) {
            for (int x = bx; x < bx + bw; x++) {
                int v = 0xFF;
                if (im.in(x, y)) {
                    const uint8_t* c = im.at(x, y);
                    v = format == PCR_TEX_I8 ? colorToI8(c) : format == PCR_TEX_IA4 ? colorToIa4(c) : idx->at(packRgba(c));
                }
                o[off++] = (uint8_t)v;
            }
        }
        break;
    case PCR_TEX_IA8:
    case PCR_TEX_RGB565:
    case PCR_TEX_RGB5A3:
    case PCR_TEX_C14X2:
        for (int y = by; y < by + bh; y++) {
            for (int x = bx; x < bx + bw; x++) {
                int v = format == PCR_TEX_IA8 ? 0xFF : format == PCR_TEX_C14X2 ? 0x3FFF : 0xFFFF;
                if (im.in(x, y)) {
                    const uint8_t* c = im.at(x, y);
                    switch (format) {
                    case PCR_TEX_IA8: v = colorToIa8(c); break;
                    case PCR_TEX_RGB565: v = colorToRgb565(c); break;
                    case PCR_TEX_RGB5A3: v = colorToRgb5a3(c); break;
                    default: v = idx->at(packRgba(c)); break;
                    }
                }
                wr16(o + off, (uint16_t)v);
                off += 2;
            }
        }
        break;
    case PCR_TEX_RGBA8:
        for (int i = 0; i < 16; i++) {
            const int x = bx + i % bw, y = by + i / bw;
            uint8_t c[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            if (im.in(x, y)) {
                memcpy(c, im.at(x, y), 4);
            }
            o[i * 2] = c[3];
            o[i * 2 + 1] = c[0];
            o[i * 2 + 32] = c[1];
            o[i * 2 + 33] = c[2];
        }
        break;
    case PCR_TEX_CMPR:
        for (int sb = 0; sb < 4; sb++) {
            const int sx = bx + (sb % 2) * 4, sy = by + (sb / 2) * 4;
            uint8_t all[16][4];
            int count = 0;
            bool needsTransparent = false;
            for (int i = 0; i < 16; i++) {
                const int x = sx + i % 4, y = sy + i / 4;
                if (!im.in(x, y)) {
                    continue;
                }
                const uint8_t* c = im.at(x, y);
                if (c[3] < 16) {
                    needsTransparent = true;
                } else {
                    memcpy(all[count++], c, 4);
                }
            }
            uint8_t k0[4], k1[4];
            bestCmprKeyColors(all, count, k0, k1);
            uint16_t c0 = colorToRgb565(k0), c1 = colorToRgb565(k1);
            if ((needsTransparent && c0 > c1) || (!needsTransparent && c0 < c1)) {
                const uint16_t t = c0;
                c0 = c1;
                c1 = t;
                uint8_t tk[4];
                memcpy(tk, k0, 4);
                memcpy(k0, k1, 4);
                memcpy(k1, tk, 4);
            }
            uint8_t pal[4][4];
            cmprColors(c0, c1, pal);
            memcpy(pal[0], k0, 4);
            memcpy(pal[1], k1, 4);
            uint8_t* s = o + sb * 8;
            wr16(s, c0);
            wr16(s + 2, c1);
            uint32_t bits = 0;
            for (int i = 0; i < 16; i++) {
                const int x = sx + i % 4, y = sy + i / 4;
                if (!im.in(x, y)) {
                    continue;
                }
                bits |= (uint32_t)cmprIndex(im.at(x, y), pal) << ((15 - i) * 2);
            }
            wr32(s + 4, bits);
        }
        break;
    default:
        break;
    }
}

// encode_mipmap_image for one level.
void encodeLevel(int format, const Img& im, const std::unordered_map<uint32_t, int>* idx, uint8_t* out) {
    const int bw = pc_rando_tex_block_width(format), bh = pc_rando_tex_block_height(format);
    const int bs = pc_rando_tex_block_size(format);
    size_t off = 0;
    for (int by = 0; by < im.h; by += bh) {
        for (int bx = 0; bx < im.w; bx += bw) {
            encodeBlock(format, im, idx, bx, by, out + off);
            off += bs;
        }
    }
}

// encode_image's mipmap loop: level i is level 0 resized (not the previous level).
void encodeAllLevels(int format, const uint8_t* rgba, int w, int h, int mips,
                     const std::unordered_map<uint32_t, int>* idx, uint8_t* out) {
    std::vector<uint8_t> scaled;
    size_t off = 0;
    int mw = w, mh = h;
    for (int i = 0; i < mips; i++) {
        Img im{rgba, w, h};
        if (i != 0) {
            mw /= 2;
            mh /= 2;
            scaled.assign((size_t)mw * mh * 4, 0);
            pc_rando_tex_resize_nearest(rgba, w, h, scaled.data(), mw, mh);
            im = Img{scaled.data(), mw, mh};
        }
        encodeLevel(format, im, idx, out + off);
        off += pc_rando_tex_level_size(format, mw, mh);
    }
}

// Python's float a % b.
double pyFmod(double a, double b) {
    double m = fmod(a, b);
    if (m != 0.0) {
        if ((b < 0) != (m < 0)) {
            m += b;
        }
    } else {
        m = copysign(0.0, b);
    }
    return m;
}

int pyMod(int a, int b) {
    const int m = a % b;
    return (m != 0 && ((m < 0) != (b < 0))) ? m + b : m;
}

// int(x * k) for the non-negative values colorsys gives.
int truncScale(double x, double k) {
    return (int)(x * k);
}

void exchangeHsv(const uint8_t rgb[3], int* h, int* s, int* v) {
    double hh, ss, vv;
    pc_rando_tex_rgb_to_hsv(rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0, &hh, &ss, &vv);
    *h = truncScale(hh, 360);
    *s = truncScale(ss, 100);
    *v = truncScale(vv, 100);
}

} // namespace

const char* pc_rando_tex_result_name(int result) {
    switch (result) {
    case PCR_TEX_OK: return "ok";
    case PCR_TEX_SKIPPED: return "skipped";
    case PCR_TEX_BAD_FORMAT: return "bad format";
    case PCR_TEX_BAD_SIZE: return "bad size";
    case PCR_TEX_TOO_MANY_COLORS: return "too many colors";
    case PCR_TEX_UNSUPPORTED: return "unsupported";
    default: return "?";
    }
}

int pc_rando_tex_known_format(int format) {
    switch (format) {
    case PCR_TEX_I4: case PCR_TEX_I8: case PCR_TEX_IA4: case PCR_TEX_IA8: case PCR_TEX_RGB565:
    case PCR_TEX_RGB5A3: case PCR_TEX_RGBA8: case PCR_TEX_C4: case PCR_TEX_C8: case PCR_TEX_C14X2:
    case PCR_TEX_CMPR:
        return 1;
    default:
        return 0;
    }
}

int pc_rando_tex_block_width(int format) {
    switch (format) {
    case PCR_TEX_I4: case PCR_TEX_I8: case PCR_TEX_IA4: case PCR_TEX_C4: case PCR_TEX_C8: case PCR_TEX_CMPR:
        return 8;
    case PCR_TEX_IA8: case PCR_TEX_RGB565: case PCR_TEX_RGB5A3: case PCR_TEX_RGBA8: case PCR_TEX_C14X2:
        return 4;
    default:
        return 0;
    }
}

int pc_rando_tex_block_height(int format) {
    switch (format) {
    case PCR_TEX_I4: case PCR_TEX_C4: case PCR_TEX_CMPR:
        return 8;
    case PCR_TEX_I8: case PCR_TEX_IA4: case PCR_TEX_IA8: case PCR_TEX_RGB565: case PCR_TEX_RGB5A3:
    case PCR_TEX_RGBA8: case PCR_TEX_C8: case PCR_TEX_C14X2:
        return 4;
    default:
        return 0;
    }
}

int pc_rando_tex_block_size(int format) {
    if (!pc_rando_tex_known_format(format)) {
        return 0;
    }
    return format == PCR_TEX_RGBA8 ? 64 : 32;
}

int pc_rando_tex_uses_palette(int format) {
    return format == PCR_TEX_C4 || format == PCR_TEX_C8 || format == PCR_TEX_C14X2;
}

int pc_rando_tex_max_colors(int format) {
    switch (format) {
    case PCR_TEX_C4: return 1 << 4;
    case PCR_TEX_C8: return 1 << 8;
    case PCR_TEX_C14X2: return 1 << 14;
    default: return 0;
    }
}

int pc_rando_tex_is_grey_format(int format) {
    return format == PCR_TEX_I4 || format == PCR_TEX_I8 || format == PCR_TEX_IA4 || format == PCR_TEX_IA8;
}

int pc_rando_tex_is_grey_palette(int palFormat) {
    return palFormat == PCR_PAL_IA8;
}

size_t pc_rando_tex_level_size(int format, int w, int h) {
    const int bw = pc_rando_tex_block_width(format), bh = pc_rando_tex_block_height(format);
    if (bw == 0 || w < 0 || h < 0) {
        return 0;
    }
    return (size_t)((w + bw - 1) / bw) * (size_t)((h + bh - 1) / bh) * (size_t)pc_rando_tex_block_size(format);
}

size_t pc_rando_tex_mip(int format, int w, int h, int mip, int* mipW, int* mipH, size_t* mipSize) {
    size_t off = 0;
    size_t size = pc_rando_tex_level_size(format, w, h);
    for (int i = 0; i < mip; i++) {
        off += size;
        w /= 2;
        h /= 2;
        size = pc_rando_tex_level_size(format, w, h);
    }
    if (mipW) {
        *mipW = w;
    }
    if (mipH) {
        *mipH = h;
    }
    if (mipSize) {
        *mipSize = size;
    }
    return off;
}

size_t pc_rando_tex_image_size(int format, int w, int h, int mips) {
    return pc_rando_tex_mip(format, w, h, mips, nullptr, nullptr, nullptr);
}

void pc_rando_tex_decode_color(uint16_t raw, int palFormat, uint8_t rgba[4]) {
    switch (palFormat) {
    case PCR_PAL_IA8: ia8ToColor(raw, rgba); break;
    case PCR_PAL_RGB565: rgb565ToColor(raw, rgba); break;
    case PCR_PAL_RGB5A3: rgb5a3ToColor(raw, rgba); break;
    default: setRgba(rgba, 0, 0, 0, 0); break;
    }
}

uint16_t pc_rando_tex_encode_color(const uint8_t rgba[4], int palFormat) {
    switch (palFormat) {
    case PCR_PAL_IA8: return colorToIa8(rgba);
    case PCR_PAL_RGB565: return colorToRgb565(rgba);
    case PCR_PAL_RGB5A3: return colorToRgb5a3(rgba);
    default: return 0;
    }
}

void pc_rando_tex_decode_palette(const uint8_t* palette, int count, int palFormat, uint8_t* rgba) {
    for (int i = 0; i < count; i++) {
        pc_rando_tex_decode_color(rd16(palette + i * 2), palFormat, rgba + i * 4);
    }
}

void pc_rando_tex_encode_palette(const uint8_t* rgba, int count, int palFormat, uint8_t* palette) {
    for (int i = 0; i < count; i++) {
        wr16(palette + i * 2, pc_rando_tex_encode_color(rgba + i * 4, palFormat));
    }
}

int pc_rando_tex_decode(int format, const uint8_t* image, size_t imageSize, const uint8_t* palette,
                        int palFormat, int numColors, int w, int h, uint8_t* rgba) {
    if (!pc_rando_tex_known_format(format) || palFormat > PCR_PAL_RGB5A3) {
        return PCR_TEX_BAD_FORMAT;
    }
    if (w <= 0 || h <= 0 || imageSize < pc_rando_tex_level_size(format, w, h) ||
        (pc_rando_tex_uses_palette(format) && numColors > 0 && palette == nullptr)) {
        return PCR_TEX_BAD_SIZE;
    }
    std::vector<uint8_t> colors;
    if (pc_rando_tex_uses_palette(format)) {
        colors.resize((size_t)(numColors > 0 ? numColors : 1) * 4);
        pc_rando_tex_decode_palette(palette, numColors, palFormat, colors.data());
    } else {
        numColors = 0;
        colors.resize(4);
    }
    const int bw = pc_rando_tex_block_width(format), bh = pc_rando_tex_block_height(format);
    const int bs = pc_rando_tex_block_size(format);
    memset(rgba, 0, (size_t)w * h * 4);
    uint8_t px[64][4];
    size_t off = 0;
    for (int by = 0; by < h; by += bh) {
        for (int bx = 0; bx < w; bx += bw) {
            decodeBlock(format, image + off, (const uint8_t(*)[4])colors.data(), numColors, px);
            for (int i = 0; i < bw * bh; i++) {
                const int x = bx + i % bw, y = by + i / bw;
                if (x < w && y < h) {
                    memcpy(rgba + ((size_t)y * w + x) * 4, px[i], 4);
                }
            }
            off += bs;
        }
    }
    return PCR_TEX_OK;
}

int pc_rando_tex_encode(int format, const uint8_t* rgba, int w, int h, uint8_t* out, size_t outSize) {
    return pc_rando_tex_encode_mips(format, rgba, w, h, 1, out, outSize);
}

int pc_rando_tex_encode_mips(int format, const uint8_t* rgba, int w, int h, int mips, uint8_t* out,
                             size_t outSize) {
    if (!pc_rando_tex_known_format(format)) {
        return PCR_TEX_BAD_FORMAT;
    }
    if (pc_rando_tex_uses_palette(format)) {
        return PCR_TEX_UNSUPPORTED;
    }
    if (mips < 1) {
        mips = 1;
    }
    if (w <= 0 || h <= 0 || (mips > 1 && ((w >> (mips - 1)) == 0 || (h >> (mips - 1)) == 0)) ||
        outSize < pc_rando_tex_image_size(format, w, h, mips)) {
        return PCR_TEX_BAD_SIZE;
    }
    encodeAllLevels(format, rgba, w, h, mips, nullptr, out);
    return PCR_TEX_OK;
}

int pc_rando_tex_encode_indexed(int format, int palFormat, const uint8_t* rgba, int w, int h, int mips,
                                uint8_t* out, size_t outSize, uint8_t* palette, int* numColors) {
    if (!pc_rando_tex_uses_palette(format) || palFormat > PCR_PAL_RGB5A3) {
        return PCR_TEX_BAD_FORMAT;
    }
    if (mips < 1) {
        mips = 1;
    }
    if (w <= 0 || h <= 0 || (mips > 1 && ((w >> (mips - 1)) == 0 || (h >> (mips - 1)) == 0)) ||
        outSize < pc_rando_tex_image_size(format, w, h, mips)) {
        return PCR_TEX_BAD_SIZE;
    }
    // generate_new_palettes_from_images([image], ...)
    const int maxColors = pc_rando_tex_max_colors(format);
    std::vector<uint16_t> encoded;
    std::unordered_map<uint16_t, int> encodedIndex;
    std::unordered_map<uint32_t, int> colorIndex;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint8_t* c = rgba + ((size_t)y * w + x) * 4;
            const uint16_t e = pc_rando_tex_encode_color(c, palFormat);
            auto it = encodedIndex.find(e);
            int index;
            if (it == encodedIndex.end()) {
                index = (int)encoded.size();
                encodedIndex[e] = index;
                encoded.push_back(e);
            } else {
                index = it->second;
            }
            colorIndex.emplace(packRgba(c), index);
        }
    }
    if ((int)encoded.size() > maxColors) {
        return PCR_TEX_TOO_MANY_COLORS;
    }
    encodeAllLevels(format, rgba, w, h, mips, &colorIndex, out);
    for (size_t i = 0; i < encoded.size(); i++) {
        wr16(palette + i * 2, encoded[i]);
    }
    *numColors = (int)encoded.size();
    return PCR_TEX_OK;
}

void pc_rando_tex_resize_nearest(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
    if (sw == dw && sh == dh) {
        memcpy(dst, src, (size_t)sw * sh * 4);
        return;
    }
    // Pillow _resize (NEAREST): a[0] = box width / xsize, a[4] = box height / ysize, offsets 0;
    // ImagingScaleAffine: COORD(v) = v < 0 ? -1 : (int)v, positions accumulated by +=.
    const double ax = (double)sw / dw;
    const double ay = (double)sh / dh;
    std::vector<int> xin((size_t)dw, -1);
    double xo = 0.0 + ax * 0.5;
    for (int x = 0; x < dw; x++) {
        const int xi = xo < 0.0 ? -1 : (int)xo;
        if (xi >= 0 && xi < sw) {
            xin[x] = xi;
        }
        xo += ax;
    }
    double yo = 0.0 + ay * 0.5;
    for (int y = 0; y < dh; y++) {
        const int yi = yo < 0.0 ? -1 : (int)yo;
        uint8_t* row = dst + (size_t)y * dw * 4;
        memset(row, 0, (size_t)dw * 4);
        if (yi >= 0 && yi < sh) {
            for (int x = 0; x < dw; x++) {
                if (xin[x] >= 0) {
                    memcpy(row + x * 4, src + ((size_t)yi * sw + xin[x]) * 4, 4);
                }
            }
        }
        yo += ay;
    }
}

int pc_rando_tex_from_bti(PcRandoTexture* tex, uint8_t* data, size_t size, size_t headerOffset) {
    if (data == nullptr || headerOffset > size || size - headerOffset < 0x20) {
        return PCR_TEX_BAD_SIZE;
    }
    const uint8_t* hd = data + headerOffset;
    PcRandoTexture t{};
    t.format = hd[0];
    t.width = rd16(hd + 2);
    t.height = rd16(hd + 4);
    t.palFormat = hd[9];
    t.numColors = rd16(hd + 0xA);
    const uint32_t paletteOffset = rd32(hd + 0xC);
    t.mipmapCount = hd[0x18] == 0 ? 1 : hd[0x18];
    const uint32_t imageOffset = rd32(hd + 0x1C);
    if (!pc_rando_tex_known_format(t.format) || t.palFormat > PCR_PAL_RGB5A3) {
        return PCR_TEX_BAD_FORMAT;
    }
    if (t.width == 0 || t.height == 0) {
        return PCR_TEX_BAD_SIZE;
    }
    t.imageSize = pc_rando_tex_image_size(t.format, t.width, t.height, t.mipmapCount);
    const size_t room = size - headerOffset;
    if (imageOffset > room || room - imageOffset < t.imageSize) {
        return PCR_TEX_BAD_SIZE;
    }
    t.image = data + headerOffset + imageOffset;
    if (t.numColors > 0) {
        if (paletteOffset > room || room - paletteOffset < (size_t)t.numColors * 2) {
            return PCR_TEX_BAD_SIZE;
        }
        t.palette = data + headerOffset + paletteOffset;
    }
    *tex = t;
    return PCR_TEX_OK;
}

int pc_rando_tex_recolor(PcRandoTexture* tex, PcRandoTexColorFn fn, void* ctx) {
    if (!pc_rando_tex_known_format(tex->format) || tex->palFormat > PCR_PAL_RGB5A3) {
        return PCR_TEX_BAD_FORMAT;
    }
    if (pc_rando_tex_is_grey_format(tex->format)) {
        return PCR_TEX_SKIPPED;
    }
    if (pc_rando_tex_uses_palette(tex->format)) {
        if (pc_rando_tex_is_grey_palette(tex->palFormat)) {
            return PCR_TEX_SKIPPED;
        }
        if (tex->numColors > 0 && tex->palette == nullptr) {
            return PCR_TEX_BAD_SIZE;
        }
        // render_palette, hsv_shift_palette, replace_palette
        for (int i = 0; i < tex->numColors; i++) {
            uint8_t c[4];
            pc_rando_tex_decode_color(rd16(tex->palette + i * 2), tex->palFormat, c);
            fn(c, ctx);
            wr16(tex->palette + i * 2, pc_rando_tex_encode_color(c, tex->palFormat));
        }
        return PCR_TEX_OK;
    }
    const int mips = tex->mipmapCount < 1 ? 1 : tex->mipmapCount;
    const int w = tex->width, h = tex->height;
    if (w == 0 || h == 0 || tex->imageSize < pc_rando_tex_image_size(tex->format, w, h, mips) ||
        (mips > 1 && ((w >> (mips - 1)) == 0 || (h >> (mips - 1)) == 0))) {
        return PCR_TEX_BAD_SIZE;
    }
    // render (mipmap 0), hsv_shift_image, replace_image (every mipmap from the shifted level 0)
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    int r = pc_rando_tex_decode(tex->format, tex->image, tex->imageSize, nullptr, tex->palFormat, 0, w, h,
                                rgba.data());
    if (r != PCR_TEX_OK) {
        return r;
    }
    for (size_t i = 0; i < (size_t)w * h; i++) {
        fn(&rgba[i * 4], ctx);
    }
    r = pc_rando_tex_encode_mips(tex->format, rgba.data(), w, h, mips, tex->image, tex->imageSize);
    if (r == PCR_TEX_OK) {
        tex->numColors = 0;
    }
    return r;
}

namespace {
struct ShiftCtx {
    PcRandoTexShiftFn fn;
    int h, v;
};
void shiftThunk(uint8_t rgba[4], void* ctx) {
    const ShiftCtx* s = (const ShiftCtx*)ctx;
    s->fn(rgba, s->h, s->v);
}

struct ExchangeCtx {
    int baseS, replH, sChange, vChange;
    int ignoreBright;
};

void exchangeOne(uint8_t c[4], const ExchangeCtx& e) {
    if (e.ignoreBright && c[0] > 128 && c[1] > 128 && c[2] > 128 && c[3] == 0xFF) {
        return;
    }
    int h, s, v;
    exchangeHsv(c, &h, &s, &v);
    (void)h;
    if (s == 0) {
        s = e.baseS;
    }
    int nh = pyMod(e.replH, 360);
    int ns = s + e.sChange;
    int nv = v + e.vChange;
    ns = ns < 0 ? 0 : ns > 100 ? 100 : ns;
    nv = nv < 0 ? 0 : nv > 100 ? 100 : nv;
    double r, g, b;
    pc_rando_tex_hsv_to_rgb(nh / 360.0, ns / 100.0, nv / 100.0, &r, &g, &b);
    c[0] = (uint8_t)truncScale(r, 255);
    c[1] = (uint8_t)truncScale(g, 255);
    c[2] = (uint8_t)truncScale(b, 255);
}

ExchangeCtx makeExchange(const uint8_t base[3], const uint8_t repl[3], int ignoreBright) {
    int bh, bs, bv, rh, rs, rv;
    exchangeHsv(base, &bh, &bs, &bv);
    exchangeHsv(repl, &rh, &rs, &rv);
    (void)bh;
    return ExchangeCtx{bs, rh, rs - bs, rv - bv, ignoreBright};
}

void exchangeThunk(uint8_t rgba[4], void* ctx) {
    exchangeOne(rgba, *(const ExchangeCtx*)ctx);
}
} // namespace

int pc_rando_tex_shift(PcRandoTexture* tex, int hShift, int vShift, PcRandoTexShiftFn fn) {
    ShiftCtx ctx{fn, hShift, vShift};
    return pc_rando_tex_recolor(tex, shiftThunk, &ctx);
}

void pc_rando_tex_color_exchange_rgba(uint8_t* rgba, size_t count, const uint8_t base[3],
                                      const uint8_t replacement[3], int ignoreBright) {
    const ExchangeCtx e = makeExchange(base, replacement, ignoreBright);
    for (size_t i = 0; i < count; i++) {
        exchangeOne(rgba + i * 4, e);
    }
}

int pc_rando_tex_color_exchange(PcRandoTexture* tex, const uint8_t base[3], const uint8_t replacement[3],
                                int ignoreBright) {
    if (!pc_rando_tex_known_format(tex->format)) {
        return PCR_TEX_BAD_FORMAT;
    }
    if (pc_rando_tex_uses_palette(tex->format)) {
        return PCR_TEX_UNSUPPORTED;
    }
    ExchangeCtx e = makeExchange(base, replacement, ignoreBright);
    // Not skipped for greyscale formats: customizer.py recolours whatever texture it is given.
    const int mips = tex->mipmapCount < 1 ? 1 : tex->mipmapCount;
    const int w = tex->width, h = tex->height;
    if (w == 0 || h == 0 || tex->imageSize < pc_rando_tex_image_size(tex->format, w, h, mips) ||
        (mips > 1 && ((w >> (mips - 1)) == 0 || (h >> (mips - 1)) == 0))) {
        return PCR_TEX_BAD_SIZE;
    }
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    int r = pc_rando_tex_decode(tex->format, tex->image, tex->imageSize, nullptr, tex->palFormat, 0, w, h,
                                rgba.data());
    if (r != PCR_TEX_OK) {
        return r;
    }
    for (size_t i = 0; i < (size_t)w * h; i++) {
        exchangeThunk(&rgba[i * 4], &e);
    }
    r = pc_rando_tex_encode_mips(tex->format, rgba.data(), w, h, mips, tex->image, tex->imageSize);
    if (r == PCR_TEX_OK) {
        tex->numColors = 0;
    }
    return r;
}

void pc_rando_tex_rgb_to_hsv(double r, double g, double b, double* h, double* s, double* v) {
    const double maxc = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const double minc = r < g ? (r < b ? r : b) : (g < b ? g : b);
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
    *h = pyFmod(hh / 6.0, 1.0);
}

void pc_rando_tex_hsv_to_rgb(double h, double s, double v, double* r, double* g, double* b) {
    if (s == 0.0) {
        *r = *g = *b = v;
        return;
    }
    int i = (int)(h * 6.0);
    const double f = (h * 6.0) - i;
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - s * f);
    const double t = v * (1.0 - s * (1.0 - f));
    i = pyMod(i, 6);
    switch (i) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
    }
}
