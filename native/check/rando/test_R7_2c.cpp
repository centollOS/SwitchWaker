// cos_rando_test_R7_2c (docs/RANDOMIZER_PLAN.md step R7.2c): the raw J3D patcher list
// (pc_rando_resources.h), the in-place TEX1 / BTI shifts and the rat-hole patch (pc_rando_j3d.h) and
// the palette patchers with a seed's "palettes" key (pc_rando_palettes.h), on synthetic files only.
// The disc's files are checked against wwrando by the palettes smoke and palette_check.py.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_gen_palettes.h"
#include "pc_rando_j3d.h"
#include "pc_rando_palettes.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

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
uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

struct TexSpec {
    const char* name;
    uint8_t format;
    uint8_t palFormat;
    int dataIndex; // which data block the header points at
    uint16_t numColors;
};

// A J3D file with one TEX1 chunk: 4x4 textures, each pointing at one of `blocks` data blocks of
// 0x40 bytes (image) and, for palette formats, a palette block of 0x20 bytes after the images.
struct Synth {
    Bytes file;
    size_t chunk = 0x20;
    size_t headers = 0;
    std::vector<size_t> dataAbs; // image block offsets
    size_t paletteAbs = 0;
};

Synth makeJ3d(const std::vector<TexSpec>& specs, int blocks, uint32_t seed) {
    Synth s;
    const int n = (int)specs.size();
    const size_t hdrOff = 0x20;                 // in the chunk
    const size_t dataOff = hdrOff + n * 0x20;   // image blocks
    const size_t palOff = dataOff + blocks * 0x40;
    const size_t strOff = palOff + 0x20;
    size_t strSize = 4 + n * 4;
    for (const TexSpec& t : specs) {
        strSize += strlen(t.name) + 1;
    }
    size_t chunkSize = (strOff + strSize + 0x1F) & ~(size_t)0x1F;
    s.file.assign(0x20 + chunkSize, 0);
    uint8_t* f = s.file.data();
    memcpy(f, "J3D2bdl4", 8);
    wr32(f + 8, (uint32_t)s.file.size());
    wr32(f + 0xC, 1);
    uint8_t* c = f + 0x20;
    memcpy(c, "TEX1", 4);
    wr32(c + 4, (uint32_t)chunkSize);
    wr16(c + 8, (uint16_t)n);
    wr32(c + 0xC, (uint32_t)hdrOff);
    wr32(c + 0x10, (uint32_t)strOff);
    s.headers = 0x20 + hdrOff;
    for (int b = 0; b < blocks; b++) {
        s.dataAbs.push_back(0x20 + dataOff + b * 0x40);
    }
    s.paletteAbs = 0x20 + palOff;
    uint32_t x = seed;
    for (size_t i = 0x20 + dataOff; i < 0x20 + strOff; i++) {
        x = x * 1103515245u + 12345u;
        f[i] = (uint8_t)(x >> 16);
    }
    // strings
    uint8_t* tab = c + strOff;
    wr16(tab, (uint16_t)n);
    wr16(tab + 2, 0xFFFF);
    size_t at = 4 + n * 4;
    for (int i = 0; i < n; i++) {
        wr16(tab + 4 + i * 4 + 2, (uint16_t)at);
        strcpy((char*)tab + at, specs[i].name);
        at += strlen(specs[i].name) + 1;
    }
    for (int i = 0; i < n; i++) {
        uint8_t* h = f + s.headers + i * 0x20;
        const size_t hAbs = s.headers + i * 0x20;
        h[0] = specs[i].format;
        wr16(h + 2, 4);
        wr16(h + 4, 4);
        h[9] = specs[i].palFormat;
        wr16(h + 0xA, specs[i].numColors);
        wr32(h + 0xC, (uint32_t)(s.paletteAbs - hAbs));
        h[0x18] = 1;
        wr32(h + 0x1C, (uint32_t)(s.dataAbs[(size_t)specs[i].dataIndex] - hAbs));
    }
    return s;
}

// The bytes texture `i` gets from wwrando: its own header's texture shifted on a copy.
Bytes shiftedImage(const Synth& s, int i, int h, int v) {
    Bytes copy = s.file;
    PcRandoTexture t;
    RT_CHECK_EQ(pc_rando_tex_from_bti(&t, copy.data(), copy.size(), s.headers + i * 0x20), (int)PCR_TEX_OK);
    pc_rando_tex_shift(&t, h, v, pc_rando_j3d_shift_rgba);
    return Bytes(t.image, t.image + t.imageSize);
}

void testChunks() {
    Synth s = makeJ3d({{"a", PCR_TEX_RGB565, 0, 0, 0}}, 1, 1);
    PcRandoJ3dChunk c[4];
    RT_CHECK_EQ(pc_rando_j3d_chunks(s.file.data(), s.file.size(), c, 4), 1);
    RT_CHECK_EQ(c[0].offset, (size_t)0x20);
    // A chunk count larger than the file (Bk.arc bk_boko.bmt): stops at the end.
    wr32(s.file.data() + 0xC, 5);
    RT_CHECK_EQ(pc_rando_j3d_chunks(s.file.data(), s.file.size(), c, 4), 1);
    RT_CHECK(!pc_rando_j3d_is_j3d((const uint8_t*)"RARC0000000000000000000000000000", 32));
    RT_CHECK_EQ(pc_rando_recolor_kind("bk.bdl"), (int)PCR_RECOLOR_J3D);
    RT_CHECK_EQ(pc_rando_recolor_kind("cc.brk"), (int)PCR_RECOLOR_J3D);
    RT_CHECK_EQ(pc_rando_recolor_kind("x.bti"), (int)PCR_RECOLOR_BTI);
    RT_CHECK_EQ(pc_rando_recolor_kind("x.BDL"), (int)PCR_RECOLOR_NONE); // splitext is exact
    RT_CHECK_EQ(pc_rando_recolor_kind("x.bck"), (int)PCR_RECOLOR_NONE);
    RT_CHECK_EQ(pc_rando_recolor_kind(".bdl"), (int)PCR_RECOLOR_NONE);
}

void testTex1() {
    // 0, 1: "a" twice on different data (the second gets the first's result);
    // 2: "b" I8 (greyscale, skipped); 3: "ZBtoonEX" (skipped); 4: "c" C8 RGB5A3 palette;
    // 5: "d" RGB565 on its own data.
    const std::vector<TexSpec> specs = {
        {"a", PCR_TEX_RGB565, 0, 0, 7}, {"a", PCR_TEX_RGB565, 0, 1, 7}, {"b", PCR_TEX_I8, 0, 2, 0},
        {"ZBtoonEX", PCR_TEX_RGB565, 0, 3, 0}, {"c", PCR_TEX_C8, PCR_PAL_RGB5A3, 4, 16},
        {"d", PCR_TEX_RGB565, 0, 5, 0},
    };
    Synth s = makeJ3d(specs, 6, 7);
    const Bytes disc = s.file;
    const Bytes a0 = shiftedImage(s, 0, 100, 20);
    const Bytes d5 = shiftedImage(s, 5, 100, 20);
    PcRandoRecolorStats st{};
    RT_CHECK_EQ(pc_rando_j3d_shift_tex1(s.file.data(), s.file.size(), 100, 20, &st), 4);
    RT_CHECK_EQ(st.textures, 6);
    RT_CHECK_EQ(st.shifted, 4);
    RT_CHECK_EQ(st.skipped, 2);
    RT_CHECK_EQ(st.conflicts, 0);
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[0], a0.data(), a0.size()) == 0);
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[1], a0.data(), a0.size()) == 0);
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[5], d5.data(), d5.size()) == 0);
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[2], disc.data() + s.dataAbs[2], 0x40) == 0); // I8
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[3], disc.data() + s.dataAbs[3], 0x40) == 0); // toon
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[4], disc.data() + s.dataAbs[4], 0x40) == 0); // C8 image
    RT_CHECK(memcmp(s.file.data() + s.paletteAbs, disc.data() + s.paletteAbs, 0x20) != 0);  // its palette
    // replace_image: num_colors 0 on the rewritten non-palette headers only.
    RT_CHECK_EQ(rd16(s.file.data() + s.headers + 0 * 0x20 + 0xA), 0);
    RT_CHECK_EQ(rd16(s.file.data() + s.headers + 1 * 0x20 + 0xA), 0);
    RT_CHECK_EQ(rd16(s.file.data() + s.headers + 4 * 0x20 + 0xA), 16);

    // The same palette shifted once, not once per name: two names on one palette.
    Synth p = makeJ3d({{"p", PCR_TEX_C4, PCR_PAL_RGB565, 0, 16}, {"q", PCR_TEX_C4, PCR_PAL_RGB565, 1, 16}}, 2, 9);
    Bytes once = p.file;
    PcRandoTexture t;
    pc_rando_tex_from_bti(&t, once.data(), once.size(), p.headers);
    pc_rando_tex_shift(&t, 200, -10, pc_rando_j3d_shift_rgba);
    st = PcRandoRecolorStats{};
    pc_rando_j3d_shift_tex1(p.file.data(), p.file.size(), 200, -10, &st);
    RT_CHECK(memcmp(p.file.data() + p.paletteAbs, once.data() + p.paletteAbs, 0x20) == 0);
    RT_CHECK_EQ(st.conflicts, 0);

    // Data shared by a shifted name and a skipped one: reported as a conflict.
    Synth c = makeJ3d({{"x", PCR_TEX_RGB565, 0, 0, 0}, {"y", PCR_TEX_I4, 0, 0, 0}}, 1, 3);
    st = PcRandoRecolorStats{};
    pc_rando_j3d_shift_tex1(c.file.data(), c.file.size(), 50, 0, &st);
    RT_CHECK_EQ(st.conflicts, 1);

    // Same name, another format: the second is left alone and counted.
    Synth m = makeJ3d({{"m", PCR_TEX_RGB565, 0, 0, 0}, {"m", PCR_TEX_RGB5A3, 0, 1, 0}}, 2, 5);
    const Bytes mDisc = m.file;
    st = PcRandoRecolorStats{};
    pc_rando_j3d_shift_tex1(m.file.data(), m.file.size(), 50, 0, &st);
    RT_CHECK_EQ(st.mismatch, 1);
    RT_CHECK(memcmp(m.file.data() + m.dataAbs[1], mDisc.data() + m.dataAbs[1], 0x40) == 0);

    // No TEX1 (a BRK): nothing to do.
    Bytes brk(0x40, 0);
    memcpy(brk.data(), "J3D1brk1", 8);
    wr32(brk.data() + 0xC, 1);
    memcpy(brk.data() + 0x20, "TRK1", 4);
    wr32(brk.data() + 0x24, 0x20);
    RT_CHECK_EQ(pc_rando_j3d_shift_tex1(brk.data(), brk.size(), 50, 0, nullptr), 0);
}

void testBti() {
    Bytes f(0x20 + 0x40, 0);
    f[0] = PCR_TEX_CMPR;
    wr16(f.data() + 2, 8);
    wr16(f.data() + 4, 4);
    wr16(f.data() + 0xA, 3);
    f[0x18] = 1;
    wr32(f.data() + 0x1C, 0x20);
    for (size_t i = 0x20; i < f.size(); i++) {
        f[i] = (uint8_t)(i * 37);
    }
    Bytes want = f;
    PcRandoTexture t;
    pc_rando_tex_from_bti(&t, want.data(), want.size(), 0);
    pc_rando_tex_shift(&t, 77, 5, pc_rando_j3d_shift_rgba);
    PcRandoRecolorStats st{};
    RT_CHECK_EQ(pc_rando_bti_shift(f.data(), f.size(), 77, 5, &st), 1);
    RT_CHECK(memcmp(f.data() + 0x20, want.data() + 0x20, 0x20) == 0);
    RT_CHECK_EQ(rd16(f.data() + 0xA), 0);
    f[0] = PCR_TEX_IA8;
    RT_CHECK_EQ(pc_rando_bti_shift(f.data(), f.size(), 77, 5, &st), 0);
    RT_CHECK_EQ(st.skipped, 1);
}

Bytes makeRatHole() {
    Bytes f(0x1100, 0);
    memcpy(f.data(), "J3D2bdl4", 8);
    wr32(f.data() + 0xC, 2);
    memcpy(f.data() + 0x20, "MAT3", 4);
    wr32(f.data() + 0x24, 0xF00 - 0x20);
    memcpy(f.data() + 0xF00, "MDL3", 4);
    wr32(f.data() + 0xF04, 0x200);
    wr32(f.data() + 0xC80, 2);
    f[0xFCE] = 0x44;
    return f;
}

void testRatHole() {
    Bytes f = makeRatHole();
    RT_CHECK_EQ(pc_rando_rat_hole_patch(f.data(), f.size()), 1);
    RT_CHECK_EQ(f[0xC83], 0);
    RT_CHECK_EQ(f[0xFCE], 0x04);
    RT_CHECK_EQ(pc_rando_rat_hole_patch(f.data(), f.size()), 0);
    Bytes g = makeRatHole();
    g[0xFCE] = 0x45; // not the disc's file
    RT_CHECK_EQ(pc_rando_rat_hole_patch(g.data(), g.size()), -1);
    RT_CHECK_EQ(g[0xC83], 2);
}

int sRawCalls = 0, sAnyCalls = 0;
void rawCounter(PcRandoResFile*) {
    sRawCalls++;
}
void anyCounter(PcRandoResFile*) {
    sAnyCalls++;
}

void testRawList() {
    RT_CHECK(pc_rando_res_is_j3d_type(PCR_RES_TYPE('B', 'D', 'L', 'M')));
    RT_CHECK(pc_rando_res_is_j3d_type(PCR_RES_TYPE('B', 'M', 'D', ' ')));
    RT_CHECK(pc_rando_res_is_j3d_type(PCR_RES_TYPE('B', 'M', 'T', 'M')));
    RT_CHECK(pc_rando_res_is_j3d_type(PCR_RES_TYPE('B', 'R', 'K', ' ')));
    RT_CHECK(pc_rando_res_is_j3d_type(PCR_RES_TYPE('B', 'S', 'M', 'D')));
    RT_CHECK(!pc_rando_res_is_j3d_type(PCR_RES_TYPE('B', 'C', 'K', ' ')));
    RT_CHECK(!pc_rando_res_is_j3d_type(PCR_RES_DZS));
    const int before = pc_rando_raw_patcher_count();
    RT_CHECK(pc_rando_register_raw_patcher(rawCounter, PCR_RES_J3D));
    RT_CHECK(pc_rando_register_raw_patcher(rawCounter, PCR_RES_ANY)); // once
    RT_CHECK(pc_rando_register_raw_patcher(anyCounter, PCR_RES_ANY));
    RT_CHECK_EQ(pc_rando_raw_patcher_count(), before + 2);
    const int resBefore = pc_rando_res_patcher_count();
    uint8_t buf[4] = {};
    PcRandoResFile f = {};
    f.dir = f.stage = f.arc = f.name = "";
    f.data = buf;
    f.size = 4;
    f.type = PCR_RES_TYPE('B', 'D', 'L', ' ');
    pc_rando_res_raw_dispatch(&f);
    f.type = PCR_RES_DZS;
    pc_rando_res_raw_dispatch(&f);
    RT_CHECK_EQ(sRawCalls, 1);
    RT_CHECK_EQ(sAnyCalls, 2);
    pc_rando_res_dispatch(&f); // the post-load list is another list
    RT_CHECK_EQ(sRawCalls, 1);
    RT_CHECK_EQ(pc_rando_res_patcher_count(), resBefore);
    RT_CHECK(pc_rando_unregister_raw_patcher(rawCounter));
    RT_CHECK(pc_rando_unregister_raw_patcher(anyCounter));
    RT_CHECK(!pc_rando_unregister_raw_patcher(anyCounter));
    RT_CHECK_EQ(pc_rando_raw_patcher_count(), before);
}

// The patchers on a "Bk" group file, with and without a seed's palettes.
void testPatchers() {
    pc_rando_palettes_init();
    const int bk = pcr::paletteGroupIndex("Bokoblin");
    RT_CHECK(bk >= 0);
    int h = 0, v = 0;
    RT_CHECK(!pc_rando_palette_shift(bk, &h, &v)); // pc_rando() is 0

    std::vector<pcr::PaletteShift> shifts((size_t)pcr::paletteGroupCount());
    for (size_t i = 0; i < shifts.size(); i++) {
        shifts[i].h = 20 + (int)i * 7;
        shifts[i].v = -40 + (int)i * 2;
    }
    PcRandoSeed seed;
    seed.extra.set("palettes", pcr::palettesToJson(shifts));
    pc_rando_set_active(&seed, nullptr);
    RT_CHECK(pc_rando_palette_shift(bk, &h, &v));
    RT_CHECK_EQ(h, shifts[(size_t)bk].h);
    RT_CHECK_EQ(v, shifts[(size_t)bk].v);

    Synth s = makeJ3d({{"skin", PCR_TEX_RGB565, 0, 0, 0}}, 1, 11);
    const Bytes want = shiftedImage(s, 0, h, v);
    PcRandoResFile f = {};
    f.dir = "/res/Object/";
    f.stage = "";
    f.arc = "Bk";
    f.room = -1;
    f.name = "bk.bdl";
    f.type = PCR_RES_TYPE('B', 'D', 'L', 'M');
    f.data = s.file.data();
    f.size = s.file.size();
    pc_rando_palettes_reset_stats();
    pc_rando_res_raw_dispatch(&f);
    RT_CHECK(memcmp(s.file.data() + s.dataAbs[0], want.data(), want.size()) == 0);
    PcRandoPaletteStats st;
    pc_rando_palettes_stats(&st);
    RT_CHECK_EQ(st.j3dFiles, 1u);

    // Another directory (a stage archive), or an archive in no group: untouched.
    Synth o = makeJ3d({{"skin", PCR_TEX_RGB565, 0, 0, 0}}, 1, 11);
    const Bytes oDisc = o.file;
    f.data = o.file.data();
    f.dir = "/res/Stage/sea/";
    pc_rando_res_raw_dispatch(&f);
    f.dir = "/res/Object/";
    f.arc = "Link";
    pc_rando_res_raw_dispatch(&f);
    RT_CHECK(o.file == oDisc);

    // The rat hole: under pc_rando() even without palettes.
    PcRandoSeed plain;
    pc_rando_set_active(&plain, nullptr);
    RT_CHECK(!pc_rando_palette_shift(bk, &h, &v));
    Bytes rat = makeRatHole();
    f.arc = "Nzg";
    f.name = "kana_00.bdl";
    f.data = rat.data();
    f.size = rat.size();
    pc_rando_res_raw_dispatch(&f);
    RT_CHECK_EQ(rat[0xFCE], 0x04);
    pc_rando_deactivate();
}

} // namespace

int main() {
    testChunks();
    testTex1();
    testBti();
    testRatHole();
    testRawList();
    testPatchers();
    return rt::finish();
}
