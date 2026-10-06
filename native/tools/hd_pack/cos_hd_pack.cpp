// cos_hd_pack: converts a Dolphin-format HD texture pack (loose tex1_*.dds files) into the compact
// pack that the game streams from disk or the Switch's SD card (native/src/pc/features/pc_hd_textures.cpp,
// docs/HD_TEXTURES.md). Written for this port (CC0, like the rest of native/); BC codecs from
// bc7enc_rdo (MIT / public domain).
//
//   cos_hd_pack [options] --out DIR INPUT_DIR [INPUT_DIR...]
//
// INPUT_DIRs are searched recursively for tex1_*.dds; a later directory overrides a file of the
// same name in an earlier one (a pack's GZL folder, then optional texture folders). Per texture:
//   - the output keeps the source's BC7/BC3/BC1 format (uncompressed RGBA/BGRA becomes BC7);
//   - --max-size N (default 1024; 0 = keep): the top level is the first source mip whose larger
//     side is <= N, or the source's top box-filtered down by a power of two (in linear light,
//     alpha-weighted colour) until it is; a top that is not a multiple of 4 is resampled to one;
//   - mips: the source's own mips are kept (verbatim when the format and size match: authored and
//     "_arb" arbitrary mipmaps survive), the rest of the chain down to 1x1 is generated (2x2 box in
//     linear light, alpha-weighted colour, alpha scaled to keep the top level's alpha-test
//     coverage at 50%) and encoded;
//   - names are kept (tex1_<w>x<h>[_m]_<hash>[_<tlut>|_$]_<fmt>[_arb].dds); names that do not
//     follow Dolphin's grammar are skipped, as Dolphin skips them.
// Output: DIR/index.bin (layout in pc_hd_textures.cpp) and DIR/data00.bin, data01.bin, ... (each
// at most --chunk-mb MiB, default 1024; FAT32 and MTP friendly), every texture a complete DDS file
// (DX10 header) at a 512-byte aligned offset.
#include "bc7decomp.h"
#include "bc7enc.h"
#include "rgbcx.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

enum class Fmt { BC1, BC3, BC7, RGBA, BGRA, RGBX, BGRX };

bool isBC(Fmt f) { return f == Fmt::BC1 || f == Fmt::BC3 || f == Fmt::BC7; }
uint32_t blockBytes(Fmt f) { return f == Fmt::BC1 ? 8 : 16; }
uint32_t dxgiOf(Fmt f) { return f == Fmt::BC1 ? 71 : f == Fmt::BC3 ? 77 : 98; }
const char* fmtName(Fmt f) {
    switch (f) {
    case Fmt::BC1: return "BC1";
    case Fmt::BC3: return "BC3";
    case Fmt::BC7: return "BC7";
    case Fmt::RGBA: return "RGBA8";
    case Fmt::BGRA: return "BGRA8";
    case Fmt::RGBX: return "RGBX8";
    case Fmt::BGRX: return "BGRX8";
    }
    return "?";
}

uint64_t levelBytes(Fmt f, uint32_t w, uint32_t h) {
    if (isBC(f)) {
        return (uint64_t)std::max(1u, (w + 3) / 4) * std::max(1u, (h + 3) / 4) * blockBytes(f);
    }
    return (uint64_t)w * h * 4;
}

struct Options {
    uint32_t maxSize = 1024;
    uint32_t jobs = 0;
    uint64_t chunkBytes = 1024ull << 20;
    bool fast = false;
    size_t limit = 0;
    std::string only;
    fs::path out;
    std::vector<fs::path> inputs;
};

struct Source {
    Fmt fmt = Fmt::RGBA;
    uint32_t w = 0, h = 0, mips = 1;
    std::vector<uint8_t> bytes;
    std::vector<uint64_t> offsets; // per level, into bytes
    uint32_t lw(uint32_t i) const { return std::max(1u, w >> i); }
    uint32_t lh(uint32_t i) const { return std::max(1u, h >> i); }
    const uint8_t* level(uint32_t i) const { return bytes.data() + offsets[i]; }
};

uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

std::optional<Source> parseDDS(const fs::path& path, std::string& err) {
    Source s;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open"; return std::nullopt; }
    fseeko(f, 0, SEEK_END);
    const off_t size = ftello(f);
    fseeko(f, 0, SEEK_SET);
    s.bytes.resize((size_t)size);
    const bool ok = fread(s.bytes.data(), 1, s.bytes.size(), f) == s.bytes.size();
    fclose(f);
    if (!ok || s.bytes.size() < 128 || rd32(s.bytes.data()) != 0x20534444) { err = "not a DDS file"; return std::nullopt; }
    const uint8_t* hd = s.bytes.data() + 4;
    s.h = rd32(hd + 8);
    s.w = rd32(hd + 12);
    const uint32_t mipCount = rd32(hd + 24);
    const uint8_t* pf = hd + 72;
    const uint32_t pfFlags = rd32(pf + 4), fourCC = rd32(pf + 8), bits = rd32(pf + 12);
    const uint32_t rMask = rd32(pf + 16), aMask = rd32(pf + 28);
    size_t dataOffset = 128;
    if (pfFlags & 0x4) {
        if (fourCC == 0x31545844) s.fmt = Fmt::BC1;
        else if (fourCC == 0x35545844) s.fmt = Fmt::BC3;
        else if (fourCC == 0x30315844) {
            if (s.bytes.size() < 148) { err = "truncated DX10 header"; return std::nullopt; }
            const uint32_t dxgi = rd32(s.bytes.data() + 128);
            dataOffset = 148;
            switch (dxgi) {
            case 71: case 72: s.fmt = Fmt::BC1; break;
            case 77: case 78: s.fmt = Fmt::BC3; break;
            case 98: case 99: s.fmt = Fmt::BC7; break;
            case 28: case 29: s.fmt = Fmt::RGBA; break;
            case 87: case 91: s.fmt = Fmt::BGRA; break;
            case 88: case 93: s.fmt = Fmt::BGRX; break;
            default: err = "unsupported DXGI format " + std::to_string(dxgi); return std::nullopt;
            }
        } else { err = "unsupported FourCC"; return std::nullopt; }
    } else if ((pfFlags & 0x40) && bits == 32) {
        const bool alpha = (pfFlags & 0x1) && aMask == 0xFF000000u;
        if (rMask == 0x000000FF) s.fmt = alpha ? Fmt::RGBA : Fmt::RGBX;
        else if (rMask == 0x00FF0000) s.fmt = alpha ? Fmt::BGRA : Fmt::BGRX;
        else { err = "unsupported 32-bit masks"; return std::nullopt; }
    } else { err = "unsupported pixel format"; return std::nullopt; }
    if (s.w == 0 || s.h == 0) { err = "zero size"; return std::nullopt; }
    const uint32_t wanted = std::max(1u, mipCount);
    uint64_t off = dataOffset;
    for (uint32_t i = 0; i < wanted; ++i) {
        const uint64_t n = levelBytes(s.fmt, s.lw(i), s.lh(i));
        if (off + n > s.bytes.size()) break;
        s.offsets.push_back(off);
        off += n;
        if (s.lw(i) == 1 && s.lh(i) == 1) break;
    }
    if (s.offsets.empty()) { err = "truncated data"; return std::nullopt; }
    s.mips = (uint32_t)s.offsets.size();
    return s;
}

// ---- pixels -----------------------------------------------------------------------------------

struct Image {
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> px; // RGBA8
};

float sLinear[256];
void initTables() {
    for (int i = 0; i < 256; ++i) {
        const float c = i / 255.f;
        sLinear[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
}
uint8_t toSrgb8(float l) {
    l = std::clamp(l, 0.f, 1.f);
    const float c = l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow(l, 1.f / 2.4f) - 0.055f;
    return (uint8_t)std::lround(c * 255.f);
}

// Decodes rows [y0, y0+4) of a level (a block row for BC) into rows of RGBA8 (width w).
void decodeBlockRow(const Source& s, uint32_t level, uint32_t y0, std::vector<uint8_t>& rows) {
    const uint32_t w = s.lw(level), h = s.lh(level);
    rows.assign((size_t)w * 4 * 4, 0);
    const uint8_t* data = s.level(level);
    if (isBC(s.fmt)) {
        const uint32_t bw = std::max(1u, (w + 3) / 4);
        const uint32_t by = y0 / 4;
        uint8_t block[16 * 4];
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const uint8_t* b = data + ((uint64_t)by * bw + bx) * blockBytes(s.fmt);
            if (s.fmt == Fmt::BC7) bc7decomp::unpack_bc7(b, (bc7decomp::color_rgba*)block);
            else if (s.fmt == Fmt::BC3) rgbcx::unpack_bc3(b, block);
            else rgbcx::unpack_bc1(b, block, true);
            for (uint32_t yy = 0; yy < 4 && y0 + yy < h; ++yy)
                for (uint32_t xx = 0; xx < 4 && bx * 4 + xx < w; ++xx)
                    memcpy(&rows[((size_t)yy * w + bx * 4 + xx) * 4], &block[(yy * 4 + xx) * 4], 4);
        }
        return;
    }
    for (uint32_t yy = 0; yy < 4 && y0 + yy < h; ++yy) {
        const uint8_t* src = data + (uint64_t)(y0 + yy) * w * 4;
        uint8_t* dst = &rows[(size_t)yy * w * 4];
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t* p = src + x * 4;
            const bool bgr = s.fmt == Fmt::BGRA || s.fmt == Fmt::BGRX;
            const bool opaque = s.fmt == Fmt::RGBX || s.fmt == Fmt::BGRX;
            dst[x * 4 + 0] = bgr ? p[2] : p[0];
            dst[x * 4 + 1] = p[1];
            dst[x * 4 + 2] = bgr ? p[0] : p[2];
            dst[x * 4 + 3] = opaque ? 255 : p[3];
        }
    }
}

Image decodeLevel(const Source& s, uint32_t level) {
    Image img{s.lw(level), s.lh(level), {}};
    img.px.resize((size_t)img.w * img.h * 4);
    std::vector<uint8_t> rows;
    for (uint32_t y = 0; y < img.h; y += 4) {
        decodeBlockRow(s, level, y, rows);
        for (uint32_t yy = 0; yy < 4 && y + yy < img.h; ++yy)
            memcpy(&img.px[(size_t)(y + yy) * img.w * 4], &rows[(size_t)yy * img.w * 4], (size_t)img.w * 4);
    }
    return img;
}

// Box reduction by an integer factor: linear-light, alpha-weighted colour. The remainder rows and
// columns fold into the last output row and column.
struct Reducer {
    uint32_t w, h, f, ow, oh;
    std::vector<double> acc; // per output pixel: r*a, g*a, b*a, a, r, g, b, n
    Reducer(uint32_t w_, uint32_t h_, uint32_t f_)
        : w(w_), h(h_), f(f_), ow(std::max(1u, w_ / f_)), oh(std::max(1u, h_ / f_)), acc((size_t)ow * oh * 8, 0.0) {}
    void addRow(uint32_t y, const uint8_t* row) {
        const uint32_t oy = std::min(y / f, oh - 1);
        double* line = &acc[(size_t)oy * ow * 8];
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t ox = std::min(x / f, ow - 1);
            double* a = line + (size_t)ox * 8;
            const uint8_t* p = row + (size_t)x * 4;
            const double al = p[3] / 255.0;
            const double r = sLinear[p[0]], g = sLinear[p[1]], b = sLinear[p[2]];
            a[0] += r * al; a[1] += g * al; a[2] += b * al; a[3] += al;
            a[4] += r; a[5] += g; a[6] += b; a[7] += 1;
        }
    }
    Image finish() const {
        Image img{ow, oh, {}};
        img.px.resize((size_t)ow * oh * 4);
        for (size_t i = 0; i < (size_t)ow * oh; ++i) {
            const double* a = &acc[i * 8];
            uint8_t* p = &img.px[i * 4];
            if (a[3] > 1e-6) {
                for (int c = 0; c < 3; ++c) p[c] = toSrgb8((float)(a[c] / a[3]));
            } else {
                for (int c = 0; c < 3; ++c) p[c] = toSrgb8((float)(a[4 + c] / a[7]));
            }
            p[3] = (uint8_t)std::lround(std::clamp(a[3] / a[7], 0.0, 1.0) * 255.0);
        }
        return img;
    }
};

Image reduceImage(const Image& src, uint32_t f) {
    Reducer r(src.w, src.h, f);
    for (uint32_t y = 0; y < src.h; ++y) r.addRow(y, &src.px[(size_t)y * src.w * 4]);
    return r.finish();
}

// Streams a (possibly huge) level through the reducer without decoding it whole.
Image reduceLevel(const Source& s, uint32_t level, uint32_t f) {
    Reducer r(s.lw(level), s.lh(level), f);
    std::vector<uint8_t> rows;
    for (uint32_t y = 0; y < s.lh(level); y += 4) {
        decodeBlockRow(s, level, y, rows);
        for (uint32_t yy = 0; yy < 4 && y + yy < s.lh(level); ++yy) r.addRow(y + yy, &rows[(size_t)yy * s.lw(level) * 4]);
    }
    return r.finish();
}

Image resampleBilinear(const Image& src, uint32_t w, uint32_t h) {
    Image img{w, h, {}};
    img.px.resize((size_t)w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        const float fy = std::clamp((y + 0.5f) * src.h / h - 0.5f, 0.f, (float)src.h - 1);
        const uint32_t y0 = (uint32_t)fy, y1 = std::min(y0 + 1, src.h - 1);
        const float ty = fy - y0;
        for (uint32_t x = 0; x < w; ++x) {
            const float fx = std::clamp((x + 0.5f) * src.w / w - 0.5f, 0.f, (float)src.w - 1);
            const uint32_t x0 = (uint32_t)fx, x1 = std::min(x0 + 1, src.w - 1);
            const float tx = fx - x0;
            for (int c = 0; c < 4; ++c) {
                auto at = [&](uint32_t xx, uint32_t yy) { return (float)src.px[((size_t)yy * src.w + xx) * 4 + c]; };
                const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) + (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
                img.px[((size_t)y * w + x) * 4 + c] = (uint8_t)std::lround(std::clamp(v, 0.f, 255.f));
            }
        }
    }
    return img;
}

bool hasAlpha(const Image& img) {
    for (size_t i = 3; i < img.px.size(); i += 4) if (img.px[i] != 255) return true;
    return false;
}

double coverage(const Image& img, double scale) {
    size_t n = 0;
    for (size_t i = 3; i < img.px.size(); i += 4) n += img.px[i] * scale >= 127.5;
    return (double)n / (img.px.size() / 4);
}

// Scales the level's alpha so as many texels pass a 50% alpha test as in the reference level
// (alpha-tested foliage would otherwise thin out in the distance).
void preserveCoverage(Image& img, double target) {
    if (target <= 0.0 || target >= 1.0) return;
    double lo = 0.25, hi = 4.0;
    for (int i = 0; i < 12; ++i) {
        const double mid = (lo + hi) / 2;
        (coverage(img, mid) < target ? lo : hi) = mid;
    }
    const double scale = (lo + hi) / 2;
    for (size_t i = 3; i < img.px.size(); i += 4) img.px[i] = (uint8_t)std::min(255.0, std::round(img.px[i] * scale));
}

// ---- encoding ---------------------------------------------------------------------------------

bc7enc_compress_block_params sBc7Params;

std::vector<uint8_t> encodeLevel(const Image& img, Fmt fmt, bool fast) {
    const uint32_t bw = std::max(1u, (img.w + 3) / 4), bh = std::max(1u, (img.h + 3) / 4);
    std::vector<uint8_t> out((size_t)bw * bh * blockBytes(fmt));
    uint8_t block[64];
    for (uint32_t by = 0; by < bh; ++by)
        for (uint32_t bx = 0; bx < bw; ++bx) {
            for (uint32_t yy = 0; yy < 4; ++yy)
                for (uint32_t xx = 0; xx < 4; ++xx) {
                    const uint32_t x = std::min(bx * 4 + xx, img.w - 1), y = std::min(by * 4 + yy, img.h - 1);
                    memcpy(&block[(yy * 4 + xx) * 4], &img.px[((size_t)y * img.w + x) * 4], 4);
                }
            uint8_t* dst = &out[((size_t)by * bw + bx) * blockBytes(fmt)];
            if (fmt == Fmt::BC7) bc7enc_compress_block(dst, block, &sBc7Params);
            else if (fmt == Fmt::BC3) rgbcx::encode_bc3(fast ? 5 : 10, dst, block);
            else rgbcx::encode_bc1(fast ? 5 : 10, dst, block, false, false);
        }
    return out;
}

std::vector<uint8_t> ddsHeader(Fmt fmt, uint32_t w, uint32_t h, uint32_t mips) {
    std::vector<uint8_t> hdr(148, 0);
    auto put = [&](size_t off, uint32_t v) { memcpy(&hdr[off], &v, 4); };
    put(0, 0x20534444);
    put(4, 124);
    put(8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000 | (mips > 1 ? 0x20000 : 0));
    put(12, h);
    put(16, w);
    put(20, (uint32_t)levelBytes(fmt, w, h));
    put(28, mips);
    put(76, 32);       // pixel format size
    put(80, 0x4);      // DDPF_FOURCC
    put(84, 0x30315844); // "DX10"
    put(108, 0x1000 | (mips > 1 ? 0x8 | 0x400000 : 0));
    put(128, dxgiOf(fmt));
    put(132, 3); // TEXTURE2D
    put(140, 1); // array size
    return hdr;
}

struct Converted {
    std::vector<uint8_t> dds;
    uint32_t w = 0, h = 0, mips = 0;
    Fmt fmt = Fmt::BC7;
    uint32_t keptLevels = 0, generatedLevels = 0;
};

bool aligned4(uint32_t w, uint32_t h) { return w % 4 == 0 && h % 4 == 0; }
uint32_t align4(uint32_t v) { return std::max(4u, (v + 2) / 4 * 4); }

Converted convert(const Source& s, const Options& opt) {
    Converted c;
    c.fmt = isBC(s.fmt) ? s.fmt : Fmt::BC7;
    const uint32_t cap = opt.maxSize;
    // The top level: the first source level that fits, else the reduced source top.
    uint32_t k = 0;
    while (cap != 0 && k < s.mips && std::max(s.lw(k), s.lh(k)) > cap) ++k;
    std::optional<Image> top; // pixels of output level 0 when they had to be computed
    uint32_t tw, th;
    bool useSrcChain = true;  // output level j comes from source level k + j while there is one
    if (k < s.mips) {
        tw = s.lw(k);
        th = s.lh(k);
    } else {
        uint32_t f = 1;
        while (std::max(s.w / f, s.h / f) > cap) f *= 2;
        top = reduceLevel(s, 0, f);
        tw = top->w;
        th = top->h;
        useSrcChain = false;
    }
    if (!aligned4(tw, th)) {
        if (!top) top = decodeLevel(s, k);
        top = resampleBilinear(*top, align4(tw), align4(th));
        tw = top->w;
        th = top->h;
        useSrcChain = false;
    }
    c.w = tw;
    c.h = th;
    uint32_t mips = 1;
    while ((tw >> (mips - 1)) > 1 || (th >> (mips - 1)) > 1) ++mips;
    c.mips = mips;

    std::vector<uint8_t> data;
    std::optional<Image> prev; // pixels of the previous output level, decoded lazily
    int prevSrcLevel = -1;     // its source level when it came verbatim from the source
    double coverageTarget = -1;
    for (uint32_t j = 0; j < mips; ++j) {
        const uint32_t lw = std::max(1u, tw >> j), lh = std::max(1u, th >> j);
        const uint32_t sl = k + j;
        std::vector<uint8_t> level;
        if (j == 0 && top) {
            level = encodeLevel(*top, c.fmt, opt.fast);
            prev = std::move(top);
            prevSrcLevel = -1;
            ++c.generatedLevels;
        } else if (useSrcChain && sl < s.mips && s.lw(sl) == lw && s.lh(sl) == lh) {
            if (s.fmt == c.fmt) {
                const uint8_t* p = s.level(sl);
                level.assign(p, p + levelBytes(c.fmt, lw, lh));
                prev.reset();
                prevSrcLevel = (int)sl;
            } else {
                prev = decodeLevel(s, sl);
                level = encodeLevel(*prev, c.fmt, opt.fast);
                prevSrcLevel = -1;
            }
            ++c.keptLevels;
        } else {
            if (!prev) prev = decodeLevel(s, (uint32_t)prevSrcLevel);
            if (coverageTarget < 0) coverageTarget = hasAlpha(*prev) ? coverage(*prev, 1.0) : 2.0;
            Image next = reduceImage(*prev, 2);
            if (next.w != lw || next.h != lh) next = resampleBilinear(next, lw, lh);
            if (coverageTarget <= 1.0) preserveCoverage(next, coverageTarget);
            level = encodeLevel(next, c.fmt, opt.fast);
            prev = std::move(next);
            prevSrcLevel = -1;
            ++c.generatedLevels;
        }
        data.insert(data.end(), level.begin(), level.end());
    }
    c.dds = ddsHeader(c.fmt, tw, th, mips);
    c.dds.insert(c.dds.end(), data.begin(), data.end());
    return c;
}

// ---- pack -------------------------------------------------------------------------------------

const std::regex kName(R"(^tex1_(\d+)x(\d+)(_m)?_([0-9a-fA-F]{16}|\$)(_([0-9a-fA-F]{16}|\$))?_(\d+)(_arb)?\.dds$)",
                       std::regex::icase);

struct Entry {
    std::string name;
    uint16_t file = 0;
    uint64_t offset = 0;
    uint32_t size = 0;
    uint32_t w = 0, h = 0;
    uint8_t mips = 0, dxgi = 0, flags = 0;
};

struct Writer {
    std::mutex lock;
    fs::path dir;
    uint64_t chunkBytes;
    std::vector<FILE*> files;
    uint64_t pos = 0;
    uint64_t total = 0;
    std::vector<Entry> entries;

    bool append(Entry e, const std::vector<uint8_t>& bytes) {
        std::lock_guard g{lock};
        if (files.empty() || (pos > 0 && pos + bytes.size() > chunkBytes)) {
            if (!files.empty()) fclose(files.back()), files.back() = nullptr;
            char name[32];
            snprintf(name, sizeof(name), "data%02zu.bin", files.size());
            FILE* f = fopen((dir / name).c_str(), "wb");
            if (!f) return false;
            files.push_back(f);
            pos = 0;
        }
        FILE* f = files.back();
        e.file = (uint16_t)(files.size() - 1);
        e.offset = pos;
        e.size = (uint32_t)bytes.size();
        if (fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size()) return false;
        pos += bytes.size();
        total += bytes.size();
        static const uint8_t zeros[512] = {};
        const uint64_t pad = (512 - pos % 512) % 512;
        if (pad != 0 && fwrite(zeros, 1, pad, f) != pad) return false;
        pos += pad;
        entries.push_back(std::move(e));
        return true;
    }

    bool finish(uint32_t maxDim) {
        if (!files.empty() && files.back()) fclose(files.back()), files.back() = nullptr;
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
        std::vector<uint8_t> names;
        std::vector<uint8_t> idx(40 + entries.size() * 32, 0);
        auto put32 = [&](size_t off, uint32_t v) { memcpy(&idx[off], &v, 4); };
        auto put16 = [&](size_t off, uint16_t v) { memcpy(&idx[off], &v, 2); };
        auto put64 = [&](size_t off, uint64_t v) { memcpy(&idx[off], &v, 8); };
        memcpy(idx.data(), "COSHDIX1", 8);
        put32(8, 1);
        put32(12, (uint32_t)entries.size());
        put32(16, (uint32_t)files.size());
        put32(20, maxDim);
        put64(24, total);
        for (size_t i = 0; i < entries.size(); ++i) {
            const Entry& e = entries[i];
            const size_t o = 40 + i * 32;
            put32(o, (uint32_t)names.size());
            put16(o + 4, (uint16_t)e.name.size());
            put16(o + 6, e.file);
            put64(o + 8, e.offset);
            put32(o + 16, e.size);
            put16(o + 20, (uint16_t)e.w);
            put16(o + 22, (uint16_t)e.h);
            idx[o + 24] = e.mips;
            idx[o + 25] = e.dxgi;
            idx[o + 26] = e.flags;
            names.insert(names.end(), e.name.begin(), e.name.end());
        }
        put32(32, (uint32_t)names.size());
        idx.insert(idx.end(), names.begin(), names.end());
        FILE* f = fopen((dir / "index.bin").c_str(), "wb");
        if (!f) return false;
        const bool ok = fwrite(idx.data(), 1, idx.size(), f) == idx.size();
        return fclose(f) == 0 && ok;
    }
};

void usage() {
    fprintf(stderr,
            "usage: cos_hd_pack [--max-size N] [--jobs N] [--chunk-mb N] [--fast] [--limit N] [--only SUBSTR]\n"
            "                   --out DIR INPUT_DIR [INPUT_DIR...]\n");
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { usage(); exit(2); }
            return argv[++i];
        };
        if (a == "--max-size") opt.maxSize = (uint32_t)atoi(next());
        else if (a == "--jobs") opt.jobs = (uint32_t)atoi(next());
        else if (a == "--chunk-mb") opt.chunkBytes = (uint64_t)atoll(next()) << 20;
        else if (a == "--fast") opt.fast = true;
        else if (a == "--limit") opt.limit = (size_t)atoll(next());
        else if (a == "--only") opt.only = next();
        else if (a == "--out") opt.out = next();
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { usage(); return 2; }
        else opt.inputs.push_back(a);
    }
    if (opt.out.empty() || opt.inputs.empty() || opt.chunkBytes < (16u << 20)) { usage(); return 2; }
    if (opt.maxSize != 0 && (opt.maxSize < 4 || (opt.maxSize & (opt.maxSize - 1)) != 0)) {
        fprintf(stderr, "cos_hd_pack: --max-size must be a power of two >= 4 (or 0)\n");
        return 2;
    }

    initTables();
    rgbcx::init();
    bc7enc_compress_block_init();
    bc7enc_compress_block_params_init(&sBc7Params);
    if (opt.fast) {
        sBc7Params.m_max_partitions = 16;
        sBc7Params.m_try_least_squares = false;
    }

    // name -> path, later inputs overriding earlier ones
    std::map<std::string, fs::path> files;
    size_t skipped = 0;
    for (const auto& in : opt.inputs) {
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(in, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file()) continue;
            const std::string name = it->path().filename().string();
            if (name.size() < 5 || strncasecmp(name.c_str(), "tex1_", 5) != 0) continue;
            if (!std::regex_match(name, kName)) { ++skipped; continue; }
            if (!opt.only.empty() && name.find(opt.only) == std::string::npos) continue;
            files[name] = it->path();
        }
        if (ec) { fprintf(stderr, "cos_hd_pack: cannot read %s: %s\n", in.c_str(), ec.message().c_str()); return 1; }
    }
    std::vector<std::pair<std::string, fs::path>> work(files.begin(), files.end());
    if (opt.limit != 0 && work.size() > opt.limit) work.resize(opt.limit);
    // Largest sources first, so the long ones do not end up last on one thread.
    std::stable_sort(work.begin(), work.end(), [](const auto& a, const auto& b) {
        std::error_code ec;
        return fs::file_size(a.second, ec) > fs::file_size(b.second, ec);
    });

    fs::create_directories(opt.out);
    for (auto& old : fs::directory_iterator(opt.out)) {
        const std::string n = old.path().filename().string();
        if (n == "index.bin" || (n.rfind("data", 0) == 0 && old.path().extension() == ".bin")) fs::remove(old.path());
    }
    Writer writer;
    writer.dir = opt.out;
    writer.chunkBytes = opt.chunkBytes;

    const uint32_t jobs = opt.jobs ? opt.jobs : std::max(1u, std::thread::hardware_concurrency());
    std::atomic<size_t> nextIndex{0}, done{0}, failed{0};
    std::atomic<uint64_t> srcBytes{0}, keptLevels{0}, generatedLevels{0}, gpuBytes{0};
    std::mutex logLock;
    std::map<std::string, size_t> srcFormats, outFormats;
    std::atomic<bool> writeError{false};
    const auto start = std::chrono::steady_clock::now();
    auto worker = [&] {
        for (;;) {
            const size_t i = nextIndex.fetch_add(1);
            if (i >= work.size() || writeError) return;
            const auto& [name, path] = work[i];
            std::string err;
            auto src = parseDDS(path, err);
            if (!src) {
                std::lock_guard g{logLock};
                fprintf(stderr, "cos_hd_pack: skipped %s: %s\n", path.c_str(), err.c_str());
                ++failed;
                continue;
            }
            srcBytes += src->bytes.size();
            Converted c = convert(*src, opt);
            Entry e;
            e.name = name;
            e.w = c.w;
            e.h = c.h;
            e.mips = (uint8_t)c.mips;
            e.dxgi = (uint8_t)dxgiOf(c.fmt);
            e.flags = name.find("_arb") != std::string::npos ? 1 : 0;
            gpuBytes += c.dds.size() - 148;
            keptLevels += c.keptLevels;
            generatedLevels += c.generatedLevels;
            if (!writer.append(e, c.dds)) { writeError = true; return; }
            const size_t n = ++done;
            std::lock_guard g{logLock};
            ++srcFormats[fmtName(src->fmt)];
            ++outFormats[fmtName(c.fmt)];
            if (n % 250 == 0 || n == work.size()) {
                const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                fprintf(stderr, "cos_hd_pack: %zu/%zu textures, %.1f s\n", n, work.size(), s);
            }
        }
    };
    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < jobs; ++t) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    if (writeError || !writer.finish(opt.maxSize)) {
        fprintf(stderr, "cos_hd_pack: cannot write the pack in %s\n", opt.out.c_str());
        return 1;
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    printf("cos_hd_pack: %zu textures (%zu failed, %zu names skipped) from %zu input dir(s) in %.1f s, %u jobs\n",
           (size_t)done, (size_t)failed, skipped, opt.inputs.size(), secs, jobs);
    printf("  source %.1f MiB -> pack %.1f MiB in %zu data file(s) (GPU %.1f MiB with mips), max size %u\n",
           srcBytes / 1048576.0, writer.total / 1048576.0, writer.files.size(), gpuBytes / 1048576.0, opt.maxSize);
    printf("  mip levels kept from the source %llu, generated/re-encoded %llu\n", (unsigned long long)keptLevels,
           (unsigned long long)generatedLevels);
    printf("  source formats:");
    for (auto& [k, v] : srcFormats) printf(" %s %zu", k.c_str(), v);
    printf("\n  output formats:");
    for (auto& [k, v] : outFormats) printf(" %s %zu", k.c_str(), v);
    printf("\n");
    return failed ? 1 : 0;
}
