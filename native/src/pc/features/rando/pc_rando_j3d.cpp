// J3D and BTI recolours of the randomizer (pc_rando_j3d.h, docs/RANDOMIZER_PLAN.md step R7.2c).
// Pure: no game headers, no game calls.
#include "pc_rando_j3d.h"

#include "pc_rando_color.h"

#include <cstdarg>
#include <cstdio>
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
