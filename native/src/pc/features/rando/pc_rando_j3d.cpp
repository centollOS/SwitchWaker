// J3D and BTI recolours of the randomizer (pc_rando_j3d.h, docs/RANDOMIZER_PLAN.md steps R7.2c, R7.2d).
// Pure: no game headers, no game calls.
//
// No floating-point contraction (the colour shift of values above 255 below repeats gclib's float
// math, as pc_rando_color.cpp does).
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#include "pc_rando_j3d.h"

#include "pc_rando_color.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

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

constexpr uint32_t kTex1 = ('T' << 24) | ('E' << 16) | ('X' << 8) | '1';
constexpr uint32_t kMat3 = ('M' << 24) | ('A' << 16) | ('T' << 8) | '3';
constexpr uint32_t kMdl3 = ('M' << 24) | ('D' << 16) | ('L' << 8) | '3';
constexpr size_t kBtiHeader = 0x20;

// The TEX1 chunk's layout, checked against the file.
struct Tex1 {
    size_t chunk = 0;      // chunk offset in the file
    int count = 0;         // texture headers
    size_t headers = 0;    // file offset of header 0
    std::vector<std::string> names;
};

// gclib TEX1.read_chunk_specific_data + JChunk.read_string_table. 0: no TEX1; 1: ok; -1: unreadable.
int readTex1(const uint8_t* data, size_t size, Tex1* t) {
    PcRandoJ3dChunk c;
    if (!pc_rando_j3d_find(data, size, kTex1, &c)) {
        return 0;
    }
    if (c.size < 0x14) {
        return -1;
    }
    const uint8_t* ch = data + c.offset;
    t->chunk = c.offset;
    t->count = rd16(ch + 0x08);
    const uint32_t listOff = rd32(ch + 0x0C);
    const uint32_t strOff = rd32(ch + 0x10);
    if (listOff > c.size || (c.size - listOff) / kBtiHeader < (size_t)t->count) {
        return -1;
    }
    t->headers = c.offset + listOff;
    if (strOff == 0 || strOff > c.size || c.size - strOff < 4) {
        return -1;
    }
    const uint8_t* tab = ch + strOff;
    const int numStrings = rd16(tab);
    if ((c.size - strOff - 4) / 4 < (size_t)numStrings || numStrings < t->count) {
        return -1;
    }
    t->names.clear();
    for (int i = 0; i < t->count; i++) {
        const size_t so = strOff + rd16(tab + 4 + i * 4 + 2);
        if (so >= c.size) {
            return -1;
        }
        const char* s = (const char*)(ch + so);
        const size_t n = strnlen(s, c.size - so);
        if (n == c.size - so) {
            return -1;
        }
        t->names.emplace_back(s, n);
    }
    return 1;
}

void shiftFn(uint8_t rgba[4], int hShift, int vShift) {
    pcr::hsvShiftColor(&rgba[0], &rgba[1], &rgba[2], hShift, vShift);
}

bool sameShape(const PcRandoTexture& a, const PcRandoTexture& b) {
    return a.format == b.format && a.palFormat == b.palFormat && a.width == b.width && a.height == b.height &&
           a.mipmapCount == b.mipmapCount && a.numColors == b.numColors && a.imageSize == b.imageSize;
}

} // namespace

void pc_rando_j3d_shift_rgba(uint8_t rgba[4], int hShift, int vShift) {
    shiftFn(rgba, hShift, vShift);
}

int pc_rando_j3d_is_j3d(const uint8_t* data, size_t size) {
    return data != nullptr && size >= 0x20 && memcmp(data, "J3D", 3) == 0;
}

int pc_rando_j3d_chunks(const uint8_t* data, size_t size, PcRandoJ3dChunk* out, int max) {
    if (!pc_rando_j3d_is_j3d(data, size)) {
        return 0;
    }
    const uint32_t numChunks = rd32(data + 0x0C);
    size_t off = 0x20;
    int n = 0;
    for (uint32_t i = 0; i < numChunks; i++) {
        if (off == size || size - off < 8) {
            break; // gclib: the chunk count can be larger than the file (Bk.arc bk_boko.bmt)
        }
        const uint32_t csize = rd32(data + off + 4);
        if (csize < 8) {
            break;
        }
        if (out != nullptr && n < max) {
            out[n].magic = rd32(data + off);
            out[n].offset = off;
            out[n].size = csize <= size - off ? csize : size - off;
        }
        n++;
        if (csize > size - off) {
            break;
        }
        off += csize;
    }
    return n;
}

int pc_rando_j3d_find(const uint8_t* data, size_t size, uint32_t magic, PcRandoJ3dChunk* chunk) {
    PcRandoJ3dChunk cs[32];
    const int n = pc_rando_j3d_chunks(data, size, cs, 32);
    int found = 0;
    for (int i = 0; i < n && i < 32; i++) {
        if (cs[i].magic == magic) {
            *chunk = cs[i];
            found = 1;
        }
    }
    return found;
}

int pc_rando_j3d_shift_tex1(uint8_t* data, size_t size, int hShift, int vShift, PcRandoRecolorStats* st) {
    Tex1 t;
    const int r = readTex1(data, size, &t);
    if (r <= 0) {
        if (r < 0 && st != nullptr) {
            st->errors++;
        }
        return r;
    }
    PcRandoRecolorStats local{};
    if (st == nullptr) {
        st = &local;
    }
    st->textures += t.count;
    // The disc's bytes: every name is shifted from them, never from another name's result.
    const std::vector<uint8_t> snap(data, data + size);

    // Groups by name, in first-seen order (gclib textures_by_name).
    struct Group {
        std::vector<int> members;
        bool shifted = false;
        bool usesPalette = false;
        std::vector<uint8_t> image, palette; // the shifted bytes
    };
    std::vector<std::string> order;
    std::vector<Group> groups;
    std::vector<int> groupOf((size_t)t.count, -1);
    for (int i = 0; i < t.count; i++) {
        int g = -1;
        for (size_t k = 0; k < order.size(); k++) {
            if (order[k] == t.names[(size_t)i]) {
                g = (int)k;
                break;
            }
        }
        if (g < 0) {
            g = (int)order.size();
            order.push_back(t.names[(size_t)i]);
            groups.emplace_back();
        }
        groups[(size_t)g].members.push_back(i);
        groupOf[(size_t)i] = g;
    }

    std::vector<uint8_t> shiftedMember((size_t)t.count, 0);
    int rewritten = 0;
    for (size_t g = 0; g < groups.size(); g++) {
        Group& gr = groups[g];
        const int nm = (int)gr.members.size();
        if (order[g].find("toon") != std::string::npos) {
            st->skipped += nm; // "Special texture related to lighting"
            continue;
        }
        PcRandoTexture t0;
        int res = pc_rando_tex_from_bti(&t0, const_cast<uint8_t*>(snap.data()), size,
                                        t.headers + (size_t)gr.members[0] * kBtiHeader);
        if (res != PCR_TEX_OK) {
            logf("[cos] rando: tex1 %s: %s\n", order[g].c_str(), pc_rando_tex_result_name(res));
            st->errors += nm;
            continue;
        }
        gr.image.assign(t0.image, t0.image + t0.imageSize);
        if (t0.palette != nullptr) {
            gr.palette.assign(t0.palette, t0.palette + (size_t)t0.numColors * 2);
        }
        PcRandoTexture ts = t0;
        ts.image = gr.image.data();
        ts.palette = gr.palette.empty() ? nullptr : gr.palette.data();
        res = pc_rando_tex_shift(&ts, hShift, vShift, shiftFn);
        if (res == PCR_TEX_SKIPPED) {
            st->skipped += nm;
            continue;
        }
        if (res != PCR_TEX_OK) {
            logf("[cos] rando: tex1 %s: shift %s\n", order[g].c_str(), pc_rando_tex_result_name(res));
            st->errors += nm;
            continue;
        }
        gr.shifted = true;
        gr.usesPalette = pc_rando_tex_uses_palette(t0.format) != 0;
        for (int m : gr.members) {
            const size_t hdr = t.headers + (size_t)m * kBtiHeader;
            PcRandoTexture tm;
            // The header as on the disc (a previous name never touches another header).
            if (pc_rando_tex_from_bti(&tm, const_cast<uint8_t*>(snap.data()), size, hdr) != PCR_TEX_OK ||
                !sameShape(tm, t0)) {
                logf("[cos] rando: tex1 %s #%d: not the shape of the name's first texture; left alone\n",
                     order[g].c_str(), m);
                st->mismatch++;
                continue;
            }
            const size_t imgOff = (size_t)(tm.image - snap.data());
            if (gr.usesPalette) {
                if (!gr.palette.empty()) {
                    memcpy(data + (size_t)(tm.palette - snap.data()), gr.palette.data(), gr.palette.size());
                }
            } else {
                memcpy(data + imgOff, gr.image.data(), gr.image.size());
                wr16(data + hdr + 0xA, 0); // replace_image: num_colors = len(encoded_colors) = 0
            }
            shiftedMember[(size_t)m] = 1;
            st->shifted++;
            rewritten++;
        }
    }

    // gclib gives every header its own bytes; the in-place layout gives them only while no data is
    // shared between headers with different results. Check every header against what gclib writes.
    for (int i = 0; i < t.count; i++) {
        const size_t hdr = t.headers + (size_t)i * kBtiHeader;
        PcRandoTexture disc;
        if (pc_rando_tex_from_bti(&disc, const_cast<uint8_t*>(snap.data()), size, hdr) != PCR_TEX_OK) {
            continue; // counted above
        }
        const Group& gr = groups[(size_t)groupOf[(size_t)i]];
        const bool mine = shiftedMember[(size_t)i] != 0;
        const size_t imgOff = (size_t)(disc.image - snap.data());
        const uint8_t* wantImage = (mine && !gr.usesPalette) ? gr.image.data() : disc.image;
        bool ok = memcmp(data + imgOff, wantImage, disc.imageSize) == 0;
        if (ok && pc_rando_tex_uses_palette(disc.format) && disc.palette != nullptr) {
            const size_t palOff = (size_t)(disc.palette - snap.data());
            const uint8_t* wantPal = (mine && gr.usesPalette) ? gr.palette.data() : disc.palette;
            ok = memcmp(data + palOff, wantPal, (size_t)disc.numColors * 2) == 0;
        }
        if (!ok) {
            logf("[cos] rando: tex1 %s #%d: its data is shared with a texture shifted differently\n",
                 t.names[(size_t)i].c_str(), i);
            st->conflicts++;
        }
    }
    return rewritten;
}

int pc_rando_bti_shift(uint8_t* data, size_t size, int hShift, int vShift, PcRandoRecolorStats* st) {
    PcRandoRecolorStats local{};
    if (st == nullptr) {
        st = &local;
    }
    st->textures++;
    PcRandoTexture tex;
    int res = pc_rando_tex_from_bti(&tex, data, size, 0);
    if (res == PCR_TEX_OK) {
        res = pc_rando_tex_shift(&tex, hShift, vShift, shiftFn);
    }
    if (res == PCR_TEX_SKIPPED) {
        st->skipped++;
        return 0;
    }
    if (res != PCR_TEX_OK) {
        logf("[cos] rando: bti: %s\n", pc_rando_tex_result_name(res));
        st->errors++;
        return 0;
    }
    if (!pc_rando_tex_uses_palette(tex.format)) {
        wr16(data + 0xA, 0); // replace_image
    }
    st->shifted++;
    return 1;
}

int pc_rando_recolor_kind(const char* fileName) {
    if (fileName == nullptr) {
        return PCR_RECOLOR_NONE;
    }
    const char* dot = strrchr(fileName, '.');
    if (dot == nullptr || dot == fileName) {
        return PCR_RECOLOR_NONE; // os.path.splitext: a leading dot is not an extension
    }
    if (strcmp(dot, ".bti") == 0) {
        return PCR_RECOLOR_BTI;
    }
    if (strcmp(dot, ".bdl") == 0 || strcmp(dot, ".bmd") == 0 || strcmp(dot, ".bmt") == 0 ||
        strcmp(dot, ".brk") == 0) {
        return PCR_RECOLOR_J3D;
    }
    return PCR_RECOLOR_NONE;
}

int pc_rando_j3d_visit_textures(uint8_t* data, size_t size, int isBti, PcRandoTexVisitor fn, void* ctx) {
    if (isBti) {
        PcRandoTexture tex{};
        const int r = pc_rando_tex_from_bti(&tex, data, size, 0);
        fn(0, "", &tex, r, ctx);
        return 1;
    }
    Tex1 t;
    const int r = readTex1(data, size, &t);
    if (r <= 0) {
        return r;
    }
    for (int i = 0; i < t.count; i++) {
        PcRandoTexture tex{};
        const int res = pc_rando_tex_from_bti(&tex, data, size, t.headers + (size_t)i * kBtiHeader);
        fn(i, t.names[(size_t)i].c_str(), &tex, res, ctx);
    }
    return t.count;
}

int pc_rando_rat_hole_patch(uint8_t* data, size_t size) {
    // wwrando writes these two offsets blindly; the port checks they are the disc's MAT3 cull mode
    // (GX_CULL_BACK) and MDL3 GEN_MODE byte first, so replaced content files are left alone.
    PcRandoJ3dChunk mat3, mdl3;
    if (data == nullptr || size < 0x1000 || !pc_rando_j3d_find(data, size, kMat3, &mat3) ||
        !pc_rando_j3d_find(data, size, kMdl3, &mdl3) || 0xC80 < mat3.offset || 0xC84 > mat3.offset + mat3.size ||
        0xFCE < mdl3.offset || 0xFCF > mdl3.offset + mdl3.size) {
        return -1;
    }
    const uint32_t cull = rd32(data + 0xC80);
    const uint8_t gen = data[0xFCE];
    if (cull == 0 && gen == 0x04) {
        return 0;
    }
    if (cull != 2 || gen != 0x44) {
        logf("[cos] rando: kana_00.bdl: cull 0x%x / 0x%02x is not the disc's; left alone\n", cull, gen);
        return -1;
    }
    data[0xC80] = data[0xC81] = data[0xC82] = data[0xC83] = 0;
    data[0xFCE] = 0x04;
    return 1;
}

// ---- R7.2d: material (MAT3, MDL3) and animation (TRK1) colours --------------------------------------

namespace {

void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
int16_t rds16(const uint8_t* p) {
    return (int16_t)rd16(p);
}

constexpr uint32_t kTrk1 = ('T' << 24) | ('R' << 16) | ('K' << 8) | '1';
constexpr size_t kMaterialSize = 0x14C;
constexpr size_t kMatKonstIdx = 0x94; // 4 u16 indexes into the konst colour list
constexpr size_t kMatTevIdx = 0xDC;   // 4 u16 indexes into the tev colour list

// gclib hsv_shift_color on ints. The colours of 0..255 go through pcr::hsvShiftColor (checked against
// gclib by R7.2a); MAT3's s16 tev colours can be larger (the shift reads them as r/255 > 1, as Python
// does). Results are always 0..255.
void shiftInt(int* r, int* g, int* b, int hShift, int vShift) {
    if (*r >= 0 && *r <= 255 && *g >= 0 && *g <= 255 && *b >= 0 && *b <= 255) {
        uint8_t r8 = (uint8_t)*r, g8 = (uint8_t)*g, b8 = (uint8_t)*b;
        pcr::hsvShiftColor(&r8, &g8, &b8, hShift, vShift);
        *r = r8;
        *g = g8;
        *b = b8;
        return;
    }
    double hf, sf, vf;
    pcr::rgbToHsv(*r / 255.0, *g / 255.0, *b / 255.0, &hf, &sf, &vf);
    int h = (int)(hf * 360);
    int s = (int)(sf * 100);
    int v = (int)(vf * 100);
    h = (h + hShift) % 360;
    if (h < 0) {
        h += 360;
    }
    const int origV = v;
    v += vShift;
    v = v < 0 ? 0 : (v > 100 ? 100 : v);
    if (v < 30 && origV >= 30) {
        v = 30;
    }
    if (v > 90 && origV <= 90) {
        v = 90;
    }
    const int vDiff = v - origV;
    const int origS = s;
    if (origS < 15 && vShift > 0) {
        s += origV < 30 ? vShift * 2 : vShift;
    } else {
        s -= vDiff;
    }
    s = s < 0 ? 0 : (s > 100 ? 100 : s);
    if (s < 5 && origS >= 5) {
        s = 5;
    }
    if (s > 80 && origS <= 80) {
        s = 80;
    }
    double rr, gg, bb;
    pcr::hsvToRgb(h / 360.0, s / 100.0, v / 100.0, &rr, &gg, &bb);
    *r = (int)(rr * 255);
    *g = (int)(gg * 255);
    *b = (int)(bb * 255);
}

// The MAT3 chunk's material table and colour lists, checked against the file.
struct Mat3 {
    size_t chunk = 0, size = 0;
    int count = 0;
    std::vector<size_t> records; // file offset of each material's record (through the remap table)
    size_t tevList = 0, konstList = 0; // chunk-relative list offsets (0: no list)
};

// gclib MAT3.read_chunk_specific_data, the parts the colours need. 0: no MAT3; 1: ok; -1: unreadable.
int readMat3(const uint8_t* data, size_t size, Mat3* m) {
    PcRandoJ3dChunk c;
    if (!pc_rando_j3d_find(data, size, kMat3, &c)) {
        return 0;
    }
    if (c.size < 0x84) {
        return -1;
    }
    const uint8_t* ch = data + c.offset;
    m->chunk = c.offset;
    m->size = c.size;
    m->count = rd16(ch + 0x08);
    const uint32_t matData = rd32(ch + 0x0C);
    const uint32_t remap = rd32(ch + 0x10);
    m->tevList = rd32(ch + 0x50);
    m->konstList = rd32(ch + 0x54);
    if (remap > c.size || (c.size - remap) / 2 < (size_t)m->count) {
        return -1;
    }
    m->records.clear();
    for (int i = 0; i < m->count; i++) {
        const size_t rec = matData + (size_t)rd16(ch + remap + i * 2) * kMaterialSize;
        if (rec > c.size || c.size - rec < kMaterialSize) {
            return -1;
        }
        m->records.push_back(c.offset + rec);
    }
    return 1;
}

// The file offset of entry `index` of a MAT3 list of `elem`-byte entries; 0 when unset (index 0xFFFF,
// or the list does not exist: gclib's "None") or out of the chunk (*bad set).
size_t listEntry(const Mat3& m, size_t list, uint16_t index, size_t elem, bool* bad) {
    if (index == 0xFFFF || list == 0) {
        return 0;
    }
    const size_t off = list + (size_t)index * elem;
    if (off > m.size || m.size - off < elem) {
        *bad = true;
        return 0;
    }
    return m.chunk + off;
}

// ---- MDL3

struct MdlCmd {
    size_t offset; // file offset of the BP command (0x61 reg payload24)
    uint8_t reg;
    uint32_t payload;
};

// The TEV colour register commands (BP registers 0xE0..0xE7) of every MDL3 entry, in order; gclib
// MDLEntry.read (BP 5 bytes, XF 5 + 4 * count, 0x00 ends). 0: no MDL3; 1: ok; -1: unreadable.
int readMdl3(const uint8_t* data, size_t size, std::vector<std::vector<MdlCmd>>* entries) {
    PcRandoJ3dChunk c;
    if (!pc_rando_j3d_find(data, size, kMdl3, &c)) {
        return 0;
    }
    if (c.size < 0x24) {
        return -1;
    }
    const uint8_t* ch = data + c.offset;
    const int n = rd16(ch + 0x08);
    const uint32_t packets = rd32(ch + 0x0C);
    if (packets > c.size || (c.size - packets) / 8 < (size_t)n) {
        return -1;
    }
    entries->assign((size_t)n, {});
    for (int i = 0; i < n; i++) {
        const size_t po = packets + (size_t)i * 8;
        const size_t eo = po + rd32(ch + po);
        const size_t es = rd32(ch + po + 4);
        if (eo > c.size || c.size - eo < es) {
            return -1;
        }
        size_t o = eo;
        while (o < eo + es) {
            const uint8_t type = ch[o];
            if (type == 0x61) {
                if (eo + es - o < 5) {
                    return -1;
                }
                const uint8_t reg = ch[o + 1];
                if (reg >= 0xE0 && reg <= 0xE7) {
                    const uint32_t pl = ((uint32_t)ch[o + 2] << 16) | ((uint32_t)ch[o + 3] << 8) | ch[o + 4];
                    (*entries)[(size_t)i].push_back({c.offset + o, reg, pl});
                }
                o += 5;
            } else if (type == 0x10) {
                if (eo + es - o < 5) {
                    return -1;
                }
                const size_t len = 5 + 4 * ((size_t)rd16(ch + o + 1) + 1);
                if (eo + es - o < len) {
                    return -1;
                }
                o += len;
            } else if (type == 0x00) {
                break;
            } else {
                return -1; // gclib: "Invalid MDL3 command type"
            }
        }
    }
    return 1;
}

// ---- TRK1 (gclib animation.py / trk1.py)

struct Key {
    int time, value, tin, tout;
};
struct Track {
    int tangent = 1; // 0 IN, 1 IN_OUT
    std::vector<Key> keys;
};
struct Anim {
    std::string name;
    Track t[4]; // r g b a
    uint8_t colorId = 0;
};
struct Trk1 {
    size_t chunk = 0, size = 0;
    uint8_t loopMode = 0;
    uint16_t duration = 0;
    std::vector<Anim> reg, konst; // file order
};

int readStringTable(const uint8_t* ch, size_t csize, size_t off, std::vector<std::string>* out) {
    out->clear();
    if (off == 0 || off > csize || csize - off < 4) {
        return -1;
    }
    const int n = rd16(ch + off);
    if ((csize - off - 4) / 4 < (size_t)n) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        const size_t so = off + rd16(ch + off + 4 + i * 4 + 2);
        if (so >= csize) {
            return -1;
        }
        const char* s = (const char*)(ch + so);
        const size_t len = strnlen(s, csize - so);
        if (len == csize - so) {
            return -1;
        }
        out->emplace_back(s, len);
    }
    return 1;
}

// 0: no TRK1; 1: ok; -1: unreadable (or something wwrando itself fails on).
int readTrk1(const uint8_t* data, size_t size, Trk1* t) {
    PcRandoJ3dChunk c;
    if (!pc_rando_j3d_find(data, size, kTrk1, &c)) {
        return 0;
    }
    if (c.size < 0x58) {
        return -1;
    }
    const uint8_t* ch = data + c.offset;
    t->chunk = c.offset;
    t->size = c.size;
    if (ch[0x09] != 0xFF) {
        return -1;
    }
    t->loopMode = ch[0x08];
    t->duration = rd16(ch + 0x0A);
    for (int kind = 0; kind < 2; kind++) {
        std::vector<Anim>& anims = kind == 0 ? t->reg : t->konst;
        anims.clear();
        const int count = rd16(ch + 0x0C + kind * 2);
        const uint32_t animOff = rd32(ch + 0x20 + kind * 4);
        const uint32_t remapOff = rd32(ch + 0x28 + kind * 4);
        const uint32_t namesOff = rd32(ch + 0x30 + kind * 4);
        std::vector<int> tab[4];
        for (int k = 0; k < 4; k++) {
            const int n = rd16(ch + 0x10 + kind * 8 + k * 2);
            const uint32_t to = rd32(ch + 0x38 + kind * 16 + k * 4);
            if (n > 0 && (to > c.size || (c.size - to) / 2 < (size_t)n)) {
                return -1;
            }
            for (int i = 0; i < n; i++) {
                tab[k].push_back(rds16(ch + to + i * 2));
            }
        }
        if (count == 0) {
            continue;
        }
        if (remapOff > c.size || (c.size - remapOff) / 2 < (size_t)count || animOff > c.size ||
            (c.size - animOff) / 0x1C < (size_t)count) {
            return -1;
        }
        for (int i = 0; i < count; i++) {
            if (rd16(ch + remapOff + i * 2) != i) {
                return -1; // gclib asserts identity remap tables
            }
        }
        std::vector<std::string> names;
        if (readStringTable(ch, c.size, namesOff, &names) < 0 || (int)names.size() < count) {
            return -1;
        }
        for (int i = 0; i < count; i++) {
            Anim a;
            a.name = names[(size_t)i];
            const uint8_t* ap = ch + animOff + (size_t)i * 0x1C;
            for (int k = 0; k < 4; k++) {
                const int n = rd16(ap + k * 6);
                const int idx = rd16(ap + k * 6 + 2);
                const int tt = rd16(ap + k * 6 + 4);
                if (tt != 0 && tt != 1) {
                    return -1;
                }
                Track& tr = a.t[k];
                tr.tangent = tt;
                const std::vector<int>& d = tab[k];
                if (n == 1) {
                    if ((size_t)idx >= d.size()) {
                        return -1;
                    }
                    tr.keys.push_back({0, d[(size_t)idx], 0, 0});
                } else {
                    const int stride = tt == 0 ? 3 : 4;
                    for (int j = 0; j < n; j++) {
                        const size_t b = (size_t)idx + (size_t)j * stride;
                        if (b + (size_t)stride > d.size()) {
                            return -1;
                        }
                        tr.keys.push_back({d[b], d[b + 1], d[b + 2], d[b + (size_t)stride - 1]});
                    }
                }
            }
            a.colorId = ap[0x18];
            anims.push_back(a);
        }
    }
    return 1;
}

// mat_name_to_*_anims: the animations grouped by name, names in first-seen order.
std::vector<Anim> grouped(const std::vector<Anim>& in) {
    std::vector<std::string> order;
    for (const Anim& a : in) {
        bool seen = false;
        for (const std::string& n : order) {
            seen = seen || n == a.name;
        }
        if (!seen) {
            order.push_back(a.name);
        }
    }
    std::vector<Anim> out;
    for (const std::string& n : order) {
        for (const Anim& a : in) {
            if (a.name == n) {
                out.push_back(a);
            }
        }
    }
    return out;
}

// shift_all_colors_in_trk1's loop body for one animation.
void shiftAnim(Anim* a, int hShift, int vShift) {
    std::vector<int> times;
    for (int k = 0; k < 3; k++) {
        for (const Key& kf : a->t[k].keys) {
            bool seen = false;
            for (int t : times) {
                seen = seen || t == kf.time;
            }
            if (!seen) {
                times.push_back(kf.time);
            }
        }
    }
    std::sort(times.begin(), times.end());
    auto closest = [](const std::vector<Key>& keys, int time) -> const Key& {
        size_t best = 0;
        for (size_t i = 1; i < keys.size(); i++) {
            if (std::abs(keys[i].time - time) < std::abs(keys[best].time - time)) {
                best = i; // min(): the first of equal distances
            }
        }
        return keys[best];
    };
    std::vector<int> rs, gs, bs;
    for (int time : times) {
        int r = closest(a->t[0].keys, time).value & 0xFF;
        int g = closest(a->t[1].keys, time).value & 0xFF;
        int b = closest(a->t[2].keys, time).value & 0xFF;
        shiftInt(&r, &g, &b, hShift, vShift);
        rs.push_back(r);
        gs.push_back(g);
        bs.push_back(b);
    }
    for (size_t i = 0; i < times.size(); i++) {
        const int vals[3] = {rs[i], gs[i], bs[i]};
        for (int k = 0; k < 3; k++) {
            for (Key& kf : a->t[k].keys) {
                if (kf.time == times[i]) {
                    kf.value = vals[k]; // the first key at exactly that time
                    break;
                }
            }
        }
    }
}

struct Out {
    std::vector<uint8_t> b;
    void pad(size_t n) {
        static const char kPad[] = "This is padding data to alignme";
        size_t need = (n - b.size() % n) % n;
        for (size_t i = 0; i < need; i++) {
            b.push_back((uint8_t)kPad[i % 31]);
        }
    }
    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) {
        b.push_back((uint8_t)(v >> 8));
        b.push_back((uint8_t)v);
    }
    void at16(size_t o, uint16_t v) { wr16(b.data() + o, v); }
    void at32(size_t o, uint32_t v) { wr32(b.data() + o, v); }
};

// AnimationTrack.save: the track's data, found in the table so far or appended.
void saveTrack(const Track& tr, Out* o, std::vector<int>* table) {
    std::vector<int> mine;
    if (tr.keys.size() == 1) {
        mine.push_back(tr.keys[0].value);
    } else {
        for (const Key& k : tr.keys) {
            mine.push_back(k.time);
            mine.push_back(k.value);
            mine.push_back(k.tin);
            if (tr.tangent == 1) {
                mine.push_back(k.tout);
            }
        }
    }
    int index = -1; // gclib's search: the first position where the whole sequence already is
    for (size_t i = 0; i + mine.size() <= table->size(); i++) {
        if (std::equal(mine.begin(), mine.end(), table->begin() + (ptrdiff_t)i)) {
            index = (int)i;
            break;
        }
    }
    if (index < 0) {
        index = (int)table->size();
        table->insert(table->end(), mine.begin(), mine.end());
    }
    o->u16((uint16_t)tr.keys.size());
    o->u16((uint16_t)index);
    o->u16((uint16_t)tr.tangent);
}

// JChunk.write_string_table (JUTNameTab: u16 count, 0xFFFF, {u16 hash, u16 offset}, the strings).
void writeStringTable(Out* o, const std::vector<std::string>& names) {
    o->u16((uint16_t)names.size());
    o->u16(0xFFFF);
    size_t next = 4 + names.size() * 4;
    for (const std::string& s : names) {
        uint16_t h = 0;
        for (unsigned char c : s) {
            h = (uint16_t)(h * 3 + c);
        }
        o->u16(h);
        o->u16((uint16_t)next);
        next += s.size() + 1;
    }
    for (const std::string& s : names) {
        o->b.insert(o->b.end(), s.begin(), s.end());
        o->b.push_back(0);
    }
}

// TRK1.save_chunk_specific_data + JChunk.save, from the grouped animations.
std::vector<uint8_t> saveTrk1(const Trk1& t, const std::vector<Anim>& reg, const std::vector<Anim>& konst) {
    Out o;
    o.b.assign(0x58, 0);
    o.pad(0x20);
    size_t animOff[2] = {0, 0};
    std::vector<int> tables[8];
    for (int kind = 0; kind < 2; kind++) {
        const std::vector<Anim>& anims = kind == 0 ? reg : konst;
        if (kind == 1) {
            o.pad(4);
        }
        animOff[kind] = anims.empty() ? 0 : o.b.size();
        for (const Anim& a : anims) {
            for (int k = 0; k < 4; k++) {
                saveTrack(a.t[k], &o, &tables[kind * 4 + k]);
            }
            o.u8(a.colorId);
            o.u8(0xFF);
            o.u8(0xFF);
            o.u8(0xFF);
        }
    }
    size_t tabOff[8];
    for (int i = 0; i < 8; i++) {
        o.pad(4);
        tabOff[i] = tables[i].empty() ? 0 : o.b.size();
        for (int v : tables[i]) {
            o.u16((uint16_t)(int16_t)v);
        }
    }
    o.pad(4);
    const size_t regRemap = reg.empty() ? 0 : o.b.size();
    for (size_t i = 0; i < reg.size(); i++) {
        o.u16((uint16_t)i);
    }
    const size_t konstRemap = konst.empty() ? 0 : o.b.size();
    for (size_t i = 0; i < konst.size(); i++) {
        o.u16((uint16_t)i);
    }
    o.pad(4);
    const size_t regNames = o.b.size();
    std::vector<std::string> names;
    for (const Anim& a : reg) {
        names.push_back(a.name);
    }
    writeStringTable(&o, names);
    o.pad(4);
    const size_t konstNames = o.b.size();
    names.clear();
    for (const Anim& a : konst) {
        names.push_back(a.name);
    }
    writeStringTable(&o, names);
    o.pad(0x20);

    memcpy(o.b.data(), "TRK1", 4);
    o.at32(4, (uint32_t)o.b.size());
    o.b[0x08] = t.loopMode;
    o.b[0x09] = 0xFF;
    o.at16(0x0A, t.duration);
    o.at16(0x0C, (uint16_t)reg.size());
    o.at16(0x0E, (uint16_t)konst.size());
    for (int i = 0; i < 8; i++) {
        o.at16(0x10 + i * 2, (uint16_t)tables[i].size());
    }
    o.at32(0x20, (uint32_t)animOff[0]);
    o.at32(0x24, (uint32_t)animOff[1]);
    o.at32(0x28, (uint32_t)regRemap);
    o.at32(0x2C, (uint32_t)konstRemap);
    o.at32(0x30, (uint32_t)regNames);
    o.at32(0x34, (uint32_t)konstNames);
    for (int i = 0; i < 8; i++) {
        o.at32(0x38 + i * 4, (uint32_t)tabOff[i]);
    }
    return o.b;
}

bool sameAnims(const std::vector<Anim>& a, const std::vector<Anim>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].name != b[i].name || a[i].colorId != b[i].colorId) {
            return false;
        }
        for (int k = 0; k < 4; k++) {
            const Track& x = a[i].t[k];
            const Track& y = b[i].t[k];
            if (x.tangent != y.tangent || x.keys.size() != y.keys.size()) {
                return false;
            }
            for (size_t j = 0; j < x.keys.size(); j++) {
                const Key& p = x.keys[j];
                const Key& q = y.keys[j];
                // a one-key track stores only its value
                if (p.value != q.value ||
                    (x.keys.size() != 1 && (p.time != q.time || p.tin != q.tin || p.tout != q.tout))) {
                    return false;
                }
            }
        }
    }
    return true;
}

// In-place fallback: each key's value into its own slot of the disc's tables.
void writeTrk1InPlace(uint8_t* data, const Trk1& t, const std::vector<Anim>& reg, const std::vector<Anim>& konst) {
    uint8_t* ch = data + t.chunk;
    for (int kind = 0; kind < 2; kind++) {
        const std::vector<Anim>& want = kind == 0 ? reg : konst;
        const std::vector<Anim>& disc = kind == 0 ? t.reg : t.konst;
        const uint32_t animOff = rd32(ch + 0x20 + kind * 4);
        for (size_t i = 0; i < disc.size(); i++) {
            // the animation of the same file index in `want` (want holds them grouped)
            const Anim* w = nullptr;
            size_t seenSame = 0, rank = 0;
            for (size_t j = 0; j < i; j++) {
                rank += disc[j].name == disc[i].name ? 1 : 0;
            }
            for (const Anim& a : want) {
                if (a.name == disc[i].name && seenSame++ == rank) {
                    w = &a;
                    break;
                }
            }
            if (w == nullptr) {
                continue;
            }
            const uint8_t* ap = ch + animOff + i * 0x1C;
            for (int k = 0; k < 3; k++) {
                const uint32_t to = rd32(ch + 0x38 + kind * 16 + k * 4);
                const int n = rd16(ap + k * 6);
                const int idx = rd16(ap + k * 6 + 2);
                const int stride = disc[i].t[k].tangent == 0 ? 3 : 4;
                for (int j = 0; j < n; j++) {
                    const size_t slot = n == 1 ? (size_t)idx : (size_t)idx + (size_t)j * stride + 1;
                    wr16(ch + to + slot * 2, (uint16_t)(int16_t)w->t[k].keys[(size_t)j].value);
                }
            }
        }
    }
}

void lineColor(std::string* s, bool set, int r, int g, int b, int a) {
    char buf[64];
    if (!set) {
        *s += " -";
        return;
    }
    snprintf(buf, sizeof buf, " %d,%d,%d,%d", r, g, b, a);
    *s += buf;
}

} // namespace

int pc_rando_j3d_shift_mat3(uint8_t* data, size_t size, int hShift, int vShift, PcRandoRecolorStats* st) {
    PcRandoRecolorStats local{};
    if (st == nullptr) {
        st = &local;
    }
    Mat3 m;
    const int r = readMat3(data, size, &m);
    if (r <= 0) {
        if (r < 0) {
            logf("[cos] rando: mat3: unreadable\n");
            st->errors++;
        }
        return r;
    }
    std::vector<size_t> done; // list entries already shifted (each from the disc's value, once)
    bool bad = false;
    int n = 0;
    auto once = [&done](size_t off) {
        for (size_t d : done) {
            if (d == off) {
                return false;
            }
        }
        done.push_back(off);
        return true;
    };
    for (size_t rec : m.records) {
        for (int k = 0; k < 4; k++) {
            const size_t e = listEntry(m, m.tevList, rd16(data + rec + kMatTevIdx + k * 2), 8, &bad);
            if (e == 0 || !once(e)) {
                continue;
            }
            int cr = rds16(data + e), cg = rds16(data + e + 2), cb = rds16(data + e + 4);
            if (cr < 0 || cg < 0 || cb < 0) {
                continue; // "Negative color? Skip it to avoid errors."
            }
            shiftInt(&cr, &cg, &cb, hShift, vShift);
            wr16(data + e, (uint16_t)cr);
            wr16(data + e + 2, (uint16_t)cg);
            wr16(data + e + 4, (uint16_t)cb);
            n++;
        }
        for (int k = 0; k < 4; k++) {
            const size_t e = listEntry(m, m.konstList, rd16(data + rec + kMatKonstIdx + k * 2), 4, &bad);
            if (e == 0 || !once(e)) {
                continue;
            }
            pcr::hsvShiftColor(&data[e], &data[e + 1], &data[e + 2], hShift, vShift);
            n++;
        }
    }
    if (bad) {
        logf("[cos] rando: mat3: a colour index points out of the chunk\n");
        st->errors++;
    }
    st->matColors += n;
    return n;
}

int pc_rando_j3d_sync_mdl3(uint8_t* data, size_t size, PcRandoRecolorStats* st) {
    PcRandoRecolorStats local{};
    if (st == nullptr) {
        st = &local;
    }
    std::vector<std::vector<MdlCmd>> entries;
    const int r = readMdl3(data, size, &entries);
    if (r <= 0) {
        if (r < 0) {
            logf("[cos] rando: mdl3: unreadable\n");
            st->errors++;
        }
        return r;
    }
    Mat3 m;
    if (readMat3(data, size, &m) <= 0) {
        logf("[cos] rando: mdl3 without a readable mat3\n");
        st->errors++;
        return -1;
    }
    std::vector<std::pair<size_t, uint32_t>> written;
    int n = 0;
    const size_t count = entries.size() < m.records.size() ? entries.size() : m.records.size();
    for (size_t i = 0; i < count; i++) {
        const size_t rec = m.records[i];
        bool bad = false;
        // MDLEntry.generate_from_material's TEV colour register commands.
        std::vector<std::pair<uint8_t, uint32_t>> gen;
        for (int k = 0; k < 3; k++) { // tev colours 0..2 -> registers 1..3 ("i == 3: continue")
            const size_t e = listEntry(m, m.tevList, rd16(data + rec + kMatTevIdx + k * 2), 8, &bad);
            if (e == 0) {
                continue;
            }
            const uint32_t cr = (uint32_t)rds16(data + e) & 0x7FF, cg = (uint32_t)rds16(data + e + 2) & 0x7FF;
            const uint32_t cb = (uint32_t)rds16(data + e + 4) & 0x7FF, ca = (uint32_t)rds16(data + e + 6) & 0x7FF;
            const uint8_t reg = (uint8_t)(0xE0 + 2 * (k + 1));
            gen.push_back({reg, cr | (ca << 12)});
            gen.push_back({(uint8_t)(reg + 1), cb | (cg << 12)});
            gen.push_back({(uint8_t)(reg + 1), cb | (cg << 12)});
            gen.push_back({(uint8_t)(reg + 1), cb | (cg << 12)});
        }
        for (int k = 0; k < 4; k++) {
            const size_t e = listEntry(m, m.konstList, rd16(data + rec + kMatKonstIdx + k * 2), 4, &bad);
            if (e == 0) {
                continue;
            }
            const uint8_t reg = (uint8_t)(0xE0 + 2 * k);
            gen.push_back({reg, (uint32_t)data[e] | ((uint32_t)data[e + 3] << 12) | (1u << 23)});
            gen.push_back({(uint8_t)(reg + 1), (uint32_t)data[e + 2] | ((uint32_t)data[e + 1] << 12) | (1u << 23)});
        }
        const std::vector<MdlCmd>& cmds = entries[i];
        bool same = !bad && gen.size() == cmds.size();
        for (size_t j = 0; same && j < cmds.size(); j++) {
            same = cmds[j].reg == gen[j].first && (cmds[j].payload >> 23) == (gen[j].second >> 23);
        }
        if (!same) {
            logf("[cos] rando: mdl3 entry %zu: its TEV colour commands are not its material's; left alone\n", i);
            st->mismatch++;
            continue;
        }
        bool changed = false;
        for (size_t j = 0; j < cmds.size(); j++) {
            const size_t o = cmds[j].offset;
            const uint32_t pl = gen[j].second;
            for (const auto& w : written) {
                if (w.first == o && w.second != pl) {
                    logf("[cos] rando: mdl3 entry %zu: a display list shared with another material\n", i);
                    st->conflicts++;
                }
            }
            written.push_back({o, pl});
            if (cmds[j].payload != pl) {
                data[o + 2] = (uint8_t)(pl >> 16);
                data[o + 3] = (uint8_t)(pl >> 8);
                data[o + 4] = (uint8_t)pl;
                changed = true;
            }
        }
        if (changed) {
            n++;
        }
    }
    st->mdlEntries += n;
    return n;
}

int pc_rando_j3d_shift_trk1(uint8_t* data, size_t size, const char* fileName, int hShift, int vShift,
                            PcRandoRecolorStats* st) {
    PcRandoRecolorStats local{};
    if (st == nullptr) {
        st = &local;
    }
    Trk1 t;
    const int r = readTrk1(data, size, &t);
    if (r <= 0) {
        if (r < 0) {
            logf("[cos] rando: trk1 %s: unreadable\n", fileName != nullptr ? fileName : "?");
            st->errors++;
        }
        return r;
    }
    std::vector<Anim> reg = grouped(t.reg);
    std::vector<Anim> konst = grouped(t.konst);
    const bool cc = fileName != nullptr && strcmp(fileName, "cc.brk") == 0;
    int n = 0;
    for (size_t i = 0; i < reg.size() + konst.size(); i++) {
        if (cc && i == 1) {
            continue; // ChuChu eyes: wwrando recolours the texture instead
        }
        Anim& a = i < reg.size() ? reg[i] : konst[i - reg.size()];
        if (a.t[0].keys.empty() || a.t[1].keys.empty() || a.t[2].keys.empty()) {
            logf("[cos] rando: trk1 %s: animation %zu has an empty track\n", fileName, i);
            st->errors++;
            return -1;
        }
    }
    for (size_t i = 0; i < reg.size() + konst.size(); i++) {
        if (cc && i == 1) {
            continue;
        }
        shiftAnim(i < reg.size() ? &reg[i] : &konst[i - reg.size()], hShift, vShift);
        n++;
    }

    // gclib's layout when it fits: the chunk's own space, or up to the file's end when TRK1 is last.
    PcRandoJ3dChunk cs[32];
    const int nc = pc_rando_j3d_chunks(data, size, cs, 32);
    const bool last = nc > 0 && nc <= 32 && cs[nc - 1].offset == t.chunk;
    const size_t avail = last ? size - t.chunk : t.size;
    const std::vector<uint8_t> out = saveTrk1(t, reg, konst);
    if (out.size() <= avail) {
        const size_t oldEnd = t.chunk + t.size;
        memcpy(data + t.chunk, out.data(), out.size());
        if (last) {
            const size_t end = t.chunk + out.size();
            if (end < oldEnd) {
                memset(data + end, 0, oldEnd - end);
            }
            wr32(data + 0x08, (uint32_t)end); // J3D.save: length and chunk count
            wr32(data + 0x0C, (uint32_t)nc);
        } else {
            wr32(data + t.chunk + 4, (uint32_t)t.size); // keep the next chunk where it is
            memset(data + t.chunk + out.size(), 0, t.size - out.size());
        }
        st->trkRebuilt++;
    } else {
        writeTrk1InPlace(data, t, reg, konst);
        st->trkInPlace++;
    }
    // Read back: the chunk must now hold exactly the recoloured animations.
    Trk1 back;
    bool ok = readTrk1(data, size, &back) == 1;
    if (ok && out.size() <= avail) {
        ok = sameAnims(back.reg, reg) && sameAnims(back.konst, konst);
    } else if (ok) {
        ok = sameAnims(grouped(back.reg), reg) && sameAnims(grouped(back.konst), konst);
    }
    if (!ok) {
        logf("[cos] rando: trk1 %s: track data shared between keys shifted differently\n", fileName);
        st->conflicts++;
    }
    st->trkAnims += n;
    return n;
}

int pc_rando_j3d_shift_file(uint8_t* data, size_t size, const char* fileName, int hShift, int vShift,
                            PcRandoRecolorStats* st) {
    PcRandoRecolorStats local{};
    if (st == nullptr) {
        st = &local;
    }
    const PcRandoRecolorStats before = *st;
    pc_rando_j3d_shift_tex1(data, size, hShift, vShift, st);
    pc_rando_j3d_shift_mat3(data, size, hShift, vShift, st);
    pc_rando_j3d_sync_mdl3(data, size, st);
    pc_rando_j3d_shift_trk1(data, size, fileName, hShift, vShift, st);
    return st->shifted != before.shifted || st->matColors != before.matColors ||
           st->mdlEntries != before.mdlEntries || st->trkAnims != before.trkAnims;
}

int pc_rando_j3d_color_lines(const uint8_t* data, size_t size, PcRandoLineFn fn, void* ctx) {
    int lines = 0, bad = 0;
    char buf[96];
    Mat3 m;
    const int mr = readMat3(data, size, &m);
    if (mr < 0) {
        bad = 1;
    } else if (mr > 0) {
        for (size_t i = 0; i < m.records.size(); i++) {
            std::string s;
            snprintf(buf, sizeof buf, "mat %zu tev", i);
            s = buf;
            bool oob = false;
            for (int k = 0; k < 4; k++) {
                const size_t e = listEntry(m, m.tevList, rd16(data + m.records[i] + kMatTevIdx + k * 2), 8, &oob);
                lineColor(&s, e != 0, e ? rds16(data + e) : 0, e ? rds16(data + e + 2) : 0,
                          e ? rds16(data + e + 4) : 0, e ? rds16(data + e + 6) : 0);
            }
            s += " konst";
            for (int k = 0; k < 4; k++) {
                const size_t e =
                    listEntry(m, m.konstList, rd16(data + m.records[i] + kMatKonstIdx + k * 2), 4, &oob);
                lineColor(&s, e != 0, e ? data[e] : 0, e ? data[e + 1] : 0, e ? data[e + 2] : 0,
                          e ? data[e + 3] : 0);
            }
            fn(s.c_str(), ctx);
            lines++;
        }
    }
    std::vector<std::vector<MdlCmd>> entries;
    const int dr = readMdl3(data, size, &entries);
    if (dr < 0) {
        bad = 1;
    } else if (dr > 0) {
        for (size_t i = 0; i < entries.size(); i++) {
            snprintf(buf, sizeof buf, "mdl %zu", i);
            std::string s = buf;
            for (const MdlCmd& c : entries[i]) {
                snprintf(buf, sizeof buf, " %02x:%06x", c.reg, c.payload);
                s += buf;
            }
            fn(s.c_str(), ctx);
            lines++;
        }
    }
    Trk1 t;
    const int tr = readTrk1(data, size, &t);
    if (tr < 0) {
        bad = 1;
    } else if (tr > 0) {
        for (int kind = 0; kind < 2; kind++) {
            const std::vector<Anim>& anims = kind == 0 ? t.reg : t.konst;
            for (size_t i = 0; i < anims.size(); i++) {
                const Anim& a = anims[i];
                snprintf(buf, sizeof buf, "trk %s %zu ", kind == 0 ? "reg" : "konst", i);
                std::string s = buf;
                s += a.name;
                snprintf(buf, sizeof buf, " %u", a.colorId);
                s += buf;
                for (int k = 0; k < 4; k++) {
                    snprintf(buf, sizeof buf, " %c %d ", "rgba"[k], a.t[k].tangent);
                    s += buf;
                    for (size_t j = 0; j < a.t[k].keys.size(); j++) {
                        const Key& kf = a.t[k].keys[j];
                        snprintf(buf, sizeof buf, "%s%d:%d:%d:%d", j ? "," : "", kf.time, kf.value, kf.tin, kf.tout);
                        s += buf;
                    }
                }
                fn(s.c_str(), ctx);
                lines++;
            }
        }
    }
    return bad ? -1 : lines;
}
