// cos_rando_test_R7_2b (docs/RANDOMIZER_PLAN.md step R7.2b): the GX texture codec
// (features/rando/pc_rando_texture.h) against gclib and wwrando, on synthetic textures only:
//   - fixtures/R7_2b_textures.txt (native/tools/rando/make_texture_fixture.py, see its docstring for
//     the line formats and generators, repeated here exactly):
//       D  decode of every format / palette format, odd sizes and edge blocks: RGBA equal to gclib's;
//       E  encode (mipmaps through Pillow's NEAREST resize; palette formats from fitting images):
//          image and palette bytes equal to gclib's, "too many colours" where gclib refuses;
//       S  wwrando's shift_all_colors_in_bti on BTIs opened with pc_rando_tex_from_bti: image and
//          palette bytes after the shift equal, greyscale ones skipped;
//       X  color_exchange (no mask) on pixels, T  on textures: equal to gclib's;
//   - format facts, BTI header bounds, results for bad input.
// The colour shift callback is a local port of gclib's hsv_shift_color (R7.2a owns the game's copy).
// Prints "ok" and exits 0 when every check holds.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#include "rando_test.h"

#include "pc_rando_texture.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

struct XS {
    uint32_t x;
    explicit XS(uint32_t seed) : x(seed ? seed : 1) {}
    uint32_t next() {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return x;
    }
    uint8_t byte() {
        return (uint8_t)(next() >> 24);
    }
};

Bytes genBytes(uint32_t seed, size_t n) {
    XS r(seed);
    Bytes b(n);
    for (auto& v : b) {
        v = r.byte();
    }
    return b;
}

Bytes genC14(uint32_t seed, size_t n, int ncolors) {
    XS r(seed);
    Bytes b;
    for (size_t i = 0; i < n; i++) {
        const uint32_t v = r.next();
        const uint16_t u = (uint16_t)(((v >> 16) & 0xC000) | ((v & 0xFFFF) % (uint32_t)ncolors));
        b.push_back((uint8_t)(u >> 8));
        b.push_back((uint8_t)u);
    }
    return b;
}

Bytes genRgba(int kind, uint32_t seed, int w, int h) {
    XS r(seed);
    Bytes px;
    std::vector<std::array<uint8_t, 4>> cols;
    if (kind == 4 || kind == 5) {
        const int k = kind == 4 ? 1 + (int)(seed % 7) : 2 + (int)(seed % 15);
        static const uint8_t alphas[6] = {0, 8, 128, 255, 255, 255};
        for (int i = 0; i < k; i++) {
            std::array<uint8_t, 4> c{};
            c[0] = r.byte();
            c[1] = r.byte();
            c[2] = r.byte();
            c[3] = kind == 4 ? alphas[r.byte() % 6] : 255;
            cols.push_back(c);
        }
    }
    uint8_t base[3] = {0, 0, 0};
    if (kind == 6) {
        base[0] = r.byte();
        base[1] = r.byte();
        base[2] = r.byte();
    }
    auto mx = [](int v) { return v > 1 ? v : 1; };
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t c[4] = {0, 0, 0, 255};
            switch (kind) {
            case 0:
                c[0] = r.byte();
                c[1] = r.byte();
                c[2] = r.byte();
                c[3] = r.byte();
                break;
            case 1:
                c[0] = r.byte();
                c[1] = r.byte();
                c[2] = r.byte();
                break;
            case 2:
            case 3:
                c[0] = (uint8_t)(x * 255 / mx(w - 1));
                c[1] = (uint8_t)(y * 255 / mx(h - 1));
                c[2] = (uint8_t)((x + y) * 255 / mx(w + h - 2));
                c[3] = kind == 2 ? 255 : (uint8_t)((x * 16 + y * 8) & 255);
                break;
            case 4:
            case 5:
                memcpy(c, cols[r.byte() % cols.size()].data(), 4);
                break;
            case 6:
                for (int i = 0; i < 3; i++) {
                    int v = base[i] + r.byte() % 5 - 2;
                    c[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
                }
                break;
            }
            px.insert(px.end(), c, c + 4);
        }
    }
    return px;
}

std::string fnv(const Bytes& a, const Bytes& b = Bytes()) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (const Bytes* p : {&a, &b}) {
        for (uint8_t v : *p) {
            h ^= v;
            h *= 0x100000001B3ull;
        }
    }
    char s[17];
    snprintf(s, sizeof s, "%016llx", (unsigned long long)h);
    return s;
}

Bytes rawImage(int fmt, uint32_t seed, size_t size, int ncolors) {
    return fmt == PCR_TEX_C14X2 ? genC14(seed, size / 2, ncolors) : genBytes(seed, size);
}

int maxMips(int w, int h) {
    int n = 1;
    while (n < 4 && (w >> n) > 0 && (h >> n) > 0) {
        n++;
    }
    return n;
}

int pyMod(int a, int b) {
    const int m = a % b;
    return (m != 0 && ((m < 0) != (b < 0))) ? m + b : m;
}

// gclib texture_utils.hsv_shift_color
void hsvShiftColor(uint8_t c[4], int hShift, int vShift) {
    double hh, ss, vv;
    pc_rando_tex_rgb_to_hsv(c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, &hh, &ss, &vv);
    int h = (int)(hh * 360), s = (int)(ss * 100), v = (int)(vv * 100);
    h = pyMod(h + hShift, 360);
    const int origV = v;
    v += vShift;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (v < 30 && origV >= 30) v = 30;
    if (v > 90 && origV <= 90) v = 90;
    const int vDiff = v - origV;
    const int origS = s;
    if (origS < 15 && vShift > 0) {
        s += origV < 30 ? vShift * 2 : vShift;
    } else {
        s -= vDiff;
    }
    if (s < 0) s = 0;
    if (s > 100) s = 100;
    if (s < 5 && origS >= 5) s = 5;
    if (s > 80 && origS <= 80) s = 80;
    double r, g, b;
    pc_rando_tex_hsv_to_rgb(h / 360.0, s / 100.0, v / 100.0, &r, &g, &b);
    c[0] = (uint8_t)(int)(r * 255);
    c[1] = (uint8_t)(int)(g * 255);
    c[2] = (uint8_t)(int)(b * 255);
}

// A BTI like the fixture script's make_bti: header, image at 0x20, palette after it.
Bytes makeBti(int fmt, int pal, int nc, int w, int h, int mips, const Bytes& image, const Bytes& palette) {
    Bytes b(0x20, 0);
    b[0] = (uint8_t)fmt;
    b[2] = (uint8_t)(w >> 8);
    b[3] = (uint8_t)w;
    b[4] = (uint8_t)(h >> 8);
    b[5] = (uint8_t)h;
    b[8] = pc_rando_tex_uses_palette(fmt) ? 1 : 0;
    b[9] = (uint8_t)pal;
    b[0xA] = (uint8_t)(nc >> 8);
    b[0xB] = (uint8_t)nc;
    const uint32_t po = 0x20 + (uint32_t)image.size();
    b[0xC] = (uint8_t)(po >> 24);
    b[0xD] = (uint8_t)(po >> 16);
    b[0xE] = (uint8_t)(po >> 8);
    b[0xF] = (uint8_t)po;
    b[0x18] = (uint8_t)mips;
    b[0x1F] = 0x20;
    b.insert(b.end(), image.begin(), image.end());
    b.insert(b.end(), palette.begin(), palette.end());
    return b;
}

void rgbHex(const std::string& s, uint8_t out[3]) {
    const unsigned long v = strtoul(s.c_str(), nullptr, 16);
    out[0] = (uint8_t)(v >> 16);
    out[1] = (uint8_t)(v >> 8);
    out[2] = (uint8_t)v;
}

int g_lineFails = 0;
bool lineCheck(bool ok, const std::string& line, const char* what) {
    if (!ok && g_lineFails++ < 20) {
        fprintf(stderr, "FAIL %s: %s\n", what, line.c_str());
    }
    RT_CHECK(ok);
    return ok;
}

void checkDecode(std::istringstream& in, const std::string& line) {
    int fmt, pal, nc, w, h;
    uint32_t seed;
    std::string hin, hout;
    in >> fmt >> pal >> nc >> w >> h >> seed >> hin >> hout;
    const Bytes img = rawImage(fmt, seed, pc_rando_tex_level_size(fmt, w, h), nc > 0 ? nc : 1);
    const Bytes palette = genBytes(seed ^ 0x5A5A5A5Au, (size_t)nc * 2);
    if (!lineCheck(fnv(img, palette) == hin, line, "D input")) {
        return;
    }
    Bytes rgba((size_t)w * h * 4);
    const int r = pc_rando_tex_decode(fmt, img.data(), img.size(), palette.data(), pal, nc, w, h, rgba.data());
    lineCheck(r == PCR_TEX_OK && fnv(rgba) == hout, line, "D decode");
}

void checkEncode(std::istringstream& in, const std::string& line) {
    int fmt, pal, kind, w, h, mips, nc;
    uint32_t seed;
    std::string hin, status, himg, hpal;
    in >> fmt >> pal >> kind >> seed >> w >> h >> mips >> hin >> status >> nc >> himg >> hpal;
    const Bytes rgba = genRgba(kind, seed, w, h);
    if (!lineCheck(fnv(rgba) == hin, line, "E input")) {
        return;
    }
    Bytes out(pc_rando_tex_image_size(fmt, w, h, mips));
    Bytes palette;
    int r;
    if (pc_rando_tex_uses_palette(fmt)) {
        Bytes palBuf((size_t)pc_rando_tex_max_colors(fmt) * 2);
        int got = -1;
        r = pc_rando_tex_encode_indexed(fmt, pal, rgba.data(), w, h, mips, out.data(), out.size(), palBuf.data(),
                                        &got);
        if (status == "toomany") {
            lineCheck(r == PCR_TEX_TOO_MANY_COLORS, line, "E too many colours");
            return;
        }
        if (r == PCR_TEX_OK) {
            palette.assign(palBuf.begin(), palBuf.begin() + got * 2);
            lineCheck(got == nc, line, "E colour count");
        }
    } else {
        r = pc_rando_tex_encode_mips(fmt, rgba.data(), w, h, mips, out.data(), out.size());
    }
    lineCheck(r == PCR_TEX_OK && fnv(out) == himg && fnv(palette) == hpal, line, "E encode");
}

void checkShift(std::istringstream& in, const std::string& line) {
    int fmt, pal, nc, w, h, mips, hs, vs;
    uint32_t seed;
    std::string src, status, himg, hpal;
    in >> fmt >> pal >> nc >> w >> h >> mips >> src >> seed >> hs >> vs >> status >> himg >> hpal;
    Bytes img;
    if (src == "r") {
        img = rawImage(fmt, seed, pc_rando_tex_image_size(fmt, w, h, mips), nc > 0 ? nc : 1);
    } else {
        const Bytes rgba = genRgba(atoi(src.c_str()), seed, w, h);
        img.resize(pc_rando_tex_image_size(fmt, w, h, mips));
        RT_CHECK_EQ(pc_rando_tex_encode_mips(fmt, rgba.data(), w, h, mips, img.data(), img.size()), (int)PCR_TEX_OK);
    }
    const Bytes palette = genBytes(seed ^ 0x5A5A5A5Au, (size_t)nc * 2);
    Bytes bti = makeBti(fmt, pal, nc, w, h, mips, img, palette);
    PcRandoTexture tex;
    if (!lineCheck(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0) == PCR_TEX_OK, line, "S open")) {
        return;
    }
    lineCheck(tex.image == bti.data() + 0x20 && tex.imageSize == img.size() && tex.numColors == nc &&
                  tex.mipmapCount == mips,
              line, "S header");
    const int r = pc_rando_tex_shift(&tex, hs, vs, hsvShiftColor);
    lineCheck(r == (status == "skipped" ? PCR_TEX_SKIPPED : PCR_TEX_OK), line, "S result");
    const Bytes after(bti.begin() + 0x20, bti.begin() + 0x20 + img.size());
    const Bytes afterPal(bti.begin() + 0x20 + img.size(), bti.end());
    lineCheck(fnv(after) == himg && fnv(afterPal) == hpal, line, "S bytes");
}

void checkExchange(std::istringstream& in, const std::string& line) {
    int kind, w, h, ib;
    uint32_t seed;
    std::string bs, rs, hin, hout;
    in >> kind >> seed >> w >> h >> bs >> rs >> ib >> hin >> hout;
    uint8_t base[3], repl[3];
    rgbHex(bs, base);
    rgbHex(rs, repl);
    Bytes rgba = genRgba(kind, seed, w, h);
    if (!lineCheck(fnv(rgba) == hin, line, "X input")) {
        return;
    }
    pc_rando_tex_color_exchange_rgba(rgba.data(), (size_t)w * h, base, repl, ib);
    lineCheck(fnv(rgba) == hout, line, "X exchange");
}

void checkTexExchange(std::istringstream& in, const std::string& line) {
    int fmt, kind, w, h, mips, ib;
    uint32_t seed;
    std::string bs, rs, himg;
    in >> fmt >> kind >> seed >> w >> h >> mips >> bs >> rs >> ib >> himg;
    uint8_t base[3], repl[3];
    rgbHex(bs, base);
    rgbHex(rs, repl);
    const Bytes rgba = genRgba(kind, seed, w, h);
    Bytes img(pc_rando_tex_image_size(fmt, w, h, mips));
    RT_CHECK_EQ(pc_rando_tex_encode_mips(fmt, rgba.data(), w, h, mips, img.data(), img.size()), (int)PCR_TEX_OK);
    Bytes bti = makeBti(fmt, PCR_PAL_IA8, 0, w, h, mips, img, Bytes());
    PcRandoTexture tex;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_OK);
    const int r = pc_rando_tex_color_exchange(&tex, base, repl, ib);
    const Bytes after(bti.begin() + 0x20, bti.end());
    lineCheck(r == PCR_TEX_OK && fnv(after) == himg, line, "T exchange");
}

void checkFixture() {
    std::string text;
    RT_CHECK(rt::readFixture("R7_2b_textures.txt", text));
    std::istringstream lines(text);
    std::string line;
    int counts[5] = {0, 0, 0, 0, 0};
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream in(line.substr(2));
        switch (line[0]) {
        case 'D': checkDecode(in, line); counts[0]++; break;
        case 'E': checkEncode(in, line); counts[1]++; break;
        case 'S': checkShift(in, line); counts[2]++; break;
        case 'X': checkExchange(in, line); counts[3]++; break;
        case 'T': checkTexExchange(in, line); counts[4]++; break;
        default: lineCheck(false, line, "unknown line"); break;
        }
    }
    // Every kind of line is there (the script writes 272 D, 1425 E, 539 S, 56 X, 72 T).
    RT_CHECK(counts[0] >= 200);
    RT_CHECK(counts[1] >= 1000);
    RT_CHECK(counts[2] >= 400);
    RT_CHECK(counts[3] >= 40);
    RT_CHECK(counts[4] >= 50);
}

void checkBasics() {
    RT_CHECK_EQ(pc_rando_tex_block_width(PCR_TEX_CMPR), 8);
    RT_CHECK_EQ(pc_rando_tex_block_height(PCR_TEX_I8), 4);
    RT_CHECK_EQ(pc_rando_tex_block_size(PCR_TEX_RGBA8), 64);
    RT_CHECK_EQ(pc_rando_tex_block_size(7), 0);
    RT_CHECK_EQ(pc_rando_tex_known_format(7), 0);
    RT_CHECK_EQ(pc_rando_tex_max_colors(PCR_TEX_C14X2), 16384);
    // 37x29 RGB565: 10x8 blocks of 32; mip 1 18x14: 5x4 blocks.
    RT_CHECK_EQ(pc_rando_tex_level_size(PCR_TEX_RGB565, 37, 29), (size_t)2560);
    int mw = 0, mh = 0;
    size_t ms = 0;
    RT_CHECK_EQ(pc_rando_tex_mip(PCR_TEX_RGB565, 37, 29, 1, &mw, &mh, &ms), (size_t)2560);
    RT_CHECK(mw == 18 && mh == 14 && ms == 640);
    RT_CHECK_EQ(pc_rando_tex_image_size(PCR_TEX_RGB565, 37, 29, 2), (size_t)3200);
    RT_CHECK_EQ(std::string(pc_rando_tex_result_name(PCR_TEX_SKIPPED)), std::string("skipped"));

    // BTI bounds: a header outside the file, image or palette past its end, a bad format.
    Bytes img(pc_rando_tex_image_size(PCR_TEX_C8, 8, 4, 1), 0);
    Bytes pal(16 * 2, 0);
    Bytes bti = makeBti(PCR_TEX_C8, PCR_PAL_RGB5A3, 16, 8, 4, 1, img, pal);
    PcRandoTexture tex;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_OK);
    RT_CHECK(tex.palette == bti.data() + 0x20 + img.size());
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size() - 1, 0), (int)PCR_TEX_BAD_SIZE);
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), 0x1F, 0), (int)PCR_TEX_BAD_SIZE);
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), bti.size()), (int)PCR_TEX_BAD_SIZE);
    // The header's mip count 0 reads as 1.
    bti[0x18] = 0;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_OK);
    RT_CHECK_EQ((int)tex.mipmapCount, 1);
    // A header inside a bigger buffer (TEX1 style): offsets relative to it.
    Bytes outer(0x40, 0xAA);
    outer.insert(outer.end(), bti.begin(), bti.end());
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, outer.data(), outer.size(), 0x40), (int)PCR_TEX_OK);
    RT_CHECK(tex.image == outer.data() + 0x60);
    bti[0] = 7;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_BAD_FORMAT);
    bti[0] = PCR_TEX_C8;
    bti[9] = 3;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_BAD_FORMAT);

    // Palette formats: colour exchange and plain encode are refused; indexed encode refuses too many.
    bti[9] = PCR_PAL_RGB5A3;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_OK);
    const uint8_t base[3] = {10, 20, 30}, repl[3] = {200, 0, 0};
    RT_CHECK_EQ(pc_rando_tex_color_exchange(&tex, base, repl, 0), (int)PCR_TEX_UNSUPPORTED);
    const Bytes rgba = genRgba(1, 99, 8, 4);
    Bytes out(64);
    RT_CHECK_EQ(pc_rando_tex_encode(PCR_TEX_C4, rgba.data(), 8, 4, out.data(), out.size()), (int)PCR_TEX_UNSUPPORTED);
    Bytes palOut(32);
    int nc = 0;
    RT_CHECK_EQ(pc_rando_tex_encode_indexed(PCR_TEX_C4, PCR_PAL_RGB565, rgba.data(), 8, 4, 1, out.data(), out.size(),
                                            palOut.data(), &nc),
                (int)PCR_TEX_TOO_MANY_COLORS);
    // Too small an output, too many mipmaps for the size.
    RT_CHECK_EQ(pc_rando_tex_encode(PCR_TEX_RGB565, rgba.data(), 8, 4, out.data(), 63), (int)PCR_TEX_BAD_SIZE);
    Bytes big(4096);
    RT_CHECK_EQ(pc_rando_tex_encode_mips(PCR_TEX_RGB565, rgba.data(), 8, 4, 4, big.data(), big.size()),
                (int)PCR_TEX_BAD_SIZE);

    // A shift of an IA8-palette C8 texture is skipped and leaves every byte as it was.
    bti[9] = PCR_PAL_IA8;
    for (size_t i = 0x20; i < bti.size(); i++) {
        bti[i] = (uint8_t)(i * 7);
    }
    const Bytes before = bti;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&tex, bti.data(), bti.size(), 0), (int)PCR_TEX_OK);
    RT_CHECK_EQ(pc_rando_tex_shift(&tex, 100, 20, hsvShiftColor), (int)PCR_TEX_SKIPPED);
    RT_CHECK(bti == before);

    // The resize gclib uses for mipmaps: even halves pick odd source pixels (x*2+1).
    Bytes src(4 * 2 * 4);
    for (size_t i = 0; i < src.size(); i++) {
        src[i] = (uint8_t)i;
    }
    Bytes dst(2 * 1 * 4);
    pc_rando_tex_resize_nearest(src.data(), 4, 2, dst.data(), 2, 1);
    RT_CHECK(memcmp(&dst[0], &src[(1 * 4 + 1) * 4], 4) == 0);
    RT_CHECK(memcmp(&dst[4], &src[(1 * 4 + 3) * 4], 4) == 0);

    // colorsys spot checks (Python: rgb_to_hsv(1, 0, 0.5) = (0.9166..., 1, 1)).
    double h, s, v;
    pc_rando_tex_rgb_to_hsv(1.0, 0.0, 0.5, &h, &s, &v);
    RT_CHECK(h > 0.9166 && h < 0.9167 && s == 1.0 && v == 1.0);
    double r, g, b;
    pc_rando_tex_hsv_to_rgb(1.0, 1.0, 1.0, &r, &g, &b); // i = 6 wraps to 0
    RT_CHECK(r == 1.0 && g == 0.0 && b == 0.0);
}

} // namespace

int main() {
    checkBasics();
    checkFixture();
    return rt::finish();
}
