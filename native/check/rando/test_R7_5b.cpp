// cos_rando_test_R7_5b (docs/RANDOMIZER_PLAN.md step R7.5b): the player's custom colours
// (features/rando/pc_rando_player_colors.h).
//   - custom_colors parsing: wwrando's [r, g, b] lists, hex strings and integers, the five trail keys
//     with and without alpha, colours equal to link_metadata.txt's base (skipped), colours that need
//     masks (counted, not applied), unknown keys and bad values (ignored), {} and NULL (nothing);
//   - the BRK key frame edit against gclib on the synthetic files of fixtures/R7_5b_player_colors.txt
//     (native/tools/rando/make_player_colors_fixture.py): every register track's values after
//     customizer.py's replace_sword_tip_brk_color_at_frame_index equal what gclib reads back from its
//     own saved file; a key frame slot shared with another track and a key count other than 3 are
//     refused with the file unchanged;
//   - the pupil recolour (render, color_exchange with ignore_bright, replace_image) on synthetic C8 /
//     C4 palette textures: image, palette and colour count byte-identical to gclib (imagequant
//     included), in place;
//   - the arrow trail edit on a synthetic common.jpc-like file: only particle 0x48's BSP1 prm colour
//     changes (alpha kept when none is given).
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_jpc.h"
#include "pc_rando_player_colors.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

Bytes unhex(const std::string& s) {
    Bytes b;
    if (s == "-") {
        return b;
    }
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        b.push_back((uint8_t)std::strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return b;
}

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string w;
    while (in >> w) {
        out.push_back(w);
    }
    return out;
}

void rgbOf(const std::string& hex, uint8_t out[3]) {
    const unsigned long v = std::strtoul(hex.c_str(), nullptr, 16);
    out[0] = (uint8_t)(v >> 16);
    out[1] = (uint8_t)(v >> 8);
    out[2] = (uint8_t)v;
}

void testParse() {
    PcRandoPlayerColors c;
    std::string log;
    RT_CHECK_EQ(pc_rando_player_colors_parse(nullptr, 0, &c, &log), 0);
    RT_CHECK_EQ(pc_rando_player_colors_any(&c), 0);
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("{}", 0, &c, &log), 0);
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("", 0, &c, &log), 0);
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("{not json", 0, &c, &log), -1);
    RT_CHECK_EQ(pc_rando_player_colors_any(&c), 0);

    // Base colours of link_metadata.txt.
    RT_CHECK_EQ(pc_rando_link_color_count(0), 12);
    RT_CHECK_EQ(pc_rando_link_color_count(1), 11);
    uint8_t rgb[3];
    const int eyes = pc_rando_link_color_find(0, "Eyes");
    RT_CHECK_EQ(eyes, 3);
    pc_rando_link_color_base(0, eyes, rgb);
    RT_CHECK(rgb[0] == 0x10 && rgb[1] == 0x51 && rgb[2] == 0x4A);
    pc_rando_link_color_base(1, pc_rando_link_color_find(1, "Shoe Soles"), rgb);
    RT_CHECK(rgb[0] == 0xD6 && rgb[1] == 0xBA && rgb[2] == 0x39);
    RT_CHECK_EQ(pc_rando_link_color_find(1, "Hat"), -1);

    // Eyes only.
    log.clear();
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("{\"Eyes\": [255, 0, 16]}", 0, &c, &log), 1);
    RT_CHECK_EQ((int)c.hasEyes, 1);
    RT_CHECK(c.eyes[0] == 255 && c.eyes[1] == 0 && c.eyes[2] == 16);
    RT_CHECK(c.eyesBase[0] == 0x10 && c.eyesBase[1] == 0x51 && c.eyesBase[2] == 0x4A);
    RT_CHECK(log.empty());
    for (int t = 0; t < PCR_TRAIL_COUNT; t++) {
        RT_CHECK_EQ((int)c.hasTrail[t], 0);
    }

    // Equal to the base: skipped (customizer.py `custom_color == base_color`), as in casual.
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("{\"Eyes\": \"10514A\"}", 1, &c, &log), 0);
    RT_CHECK_EQ(c.unchanged, 1);
    RT_CHECK_EQ(c.casual, 1);

    // Every form, masks, unknown keys, bad values.
    log.clear();
    const char* text =
        "{\"Hair\": [1, 2, 3], \"Eyes\": \"#00FF00\", \"Hat\": \"0x5AB24A\", \"Tunic\": 1193046,"
        " \"Bogus\": [1, 2, 3], \"Skin\": [1, 2], \"Pants\": \"12345\","
        " \"sword_slash_trail_color\": \"FF000080\", \"elixir_soup_sword_trail_color\": [1, 2, 3],"
        " \"parrying_sword_trail_color\": 287454020, \"boomerang_trail_color\": \"0x0A0B0C\","
        " \"arrow_trail_color\": [9, 8, 7, 6]}";
    RT_CHECK_EQ(pc_rando_player_colors_parse_text(text, 0, &c, &log), 1);
    RT_CHECK_EQ((int)c.hasEyes, 1);
    RT_CHECK(c.eyes[0] == 0 && c.eyes[1] == 255 && c.eyes[2] == 0);
    RT_CHECK_EQ(c.skippedMasked, 2); // Hair, Tunic (0x123456)
    RT_CHECK_EQ(c.unchanged, 1);     // Hat
    RT_CHECK_EQ(c.unknown, 1);       // Bogus
    RT_CHECK_EQ(c.invalid, 2);       // Skin, Pants
    RT_CHECK(!log.empty());
    const uint8_t want[PCR_TRAIL_COUNT][4] = {
        {0xFF, 0x00, 0x00, 0x80}, {1, 2, 3, 0x96}, {0x11, 0x22, 0x33, 0x44}, {0x0A, 0x0B, 0x0C, 0x96}, {9, 8, 7, 6}};
    const uint8_t hasAlpha[PCR_TRAIL_COUNT] = {1, 0, 1, 0, 1};
    for (int t = 0; t < PCR_TRAIL_COUNT; t++) {
        RT_CHECK_EQ((int)c.hasTrail[t], 1);
        RT_CHECK_EQ((int)c.trailHasAlpha[t], (int)hasAlpha[t]);
        RT_CHECK(memcmp(c.trail[t], want[t], 4) == 0);
    }
    // The drawn colour: a colour without alpha keeps the vanilla value's alpha.
    uint8_t v[4] = {1, 1, 1, 0x42};
    pc_rando_player_trail(&c, PCR_TRAIL_ELIXIR_SOUP, v);
    RT_CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 0x42);
    uint8_t s[4] = {0xFF, 0xFF, 0xFF, 0x96};
    pc_rando_player_trail(&c, PCR_TRAIL_SWORD_SLASH, s);
    RT_CHECK(s[0] == 0xFF && s[1] == 0 && s[2] == 0 && s[3] == 0x80);
    PcRandoPlayerColors none;
    pc_rando_player_colors_clear(&none);
    uint8_t n[4] = {5, 6, 7, 8};
    pc_rando_player_trail(&none, PCR_TRAIL_BOOMERANG, n);
    RT_CHECK(n[0] == 5 && n[1] == 6 && n[2] == 7 && n[3] == 8);
    // A model colour does not take an alpha; a trail key takes no 5-digit hex.
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("{\"Eyes\": \"00FF0080\"}", 0, &c, &log), 0);
    RT_CHECK_EQ(c.invalid, 1);
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("{\"arrow_trail_color\": [1, 2, 300]}", 0, &c, &log), 0);
    RT_CHECK_EQ(pc_rando_player_colors_parse_text("[1, 2]", 0, &c, &log), 0);
    RT_CHECK_EQ(c.invalid, 1);
}

struct Fixture {
    std::map<std::string, Bytes> brk, tex;
    std::vector<std::vector<std::string>> brkset, brkkeys, hx;
};

bool loadFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R7_5b_player_colors.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<std::string> w = words(line);
        if (w[0] == "brk") {
            fx.brk[w[1]] = unhex(w[2]);
        } else if (w[0] == "tex") {
            fx.tex[w[1]] = unhex(w[2]);
        } else if (w[0] == "brkset") {
            fx.brkset.push_back(w);
        } else if (w[0] == "brkkeys") {
            fx.brkkeys.push_back(w);
        } else if (w[0] == "hx") {
            fx.hx.push_back(w);
        }
    }
    return !fx.brk.empty() && !fx.tex.empty();
}

void testBrk(const Fixture& fx) {
    std::map<std::string, Bytes> edited = fx.brk;
    for (const auto& w : fx.brkset) {
        Bytes& b = edited[w[1]];
        const uint8_t rgb[3] = {(uint8_t)std::atoi(w[4].c_str()), (uint8_t)std::atoi(w[5].c_str()),
                                (uint8_t)std::atoi(w[6].c_str())};
        RT_CHECK_EQ(pc_rando_brk_set_key_rgb(b.data(), b.size(), std::atoi(w[2].c_str()), std::atoi(w[3].c_str()), 3,
                                             rgb),
                    1);
    }
    int checked = 0;
    for (const auto& w : fx.brkkeys) {
        const Bytes& b = edited[w[1]];
        const int anim = std::atoi(w[2].c_str()), ch = std::atoi(w[3].c_str());
        RT_CHECK_EQ(pc_rando_brk_key_count(b.data(), b.size(), anim, ch), (int)(w.size() - 4));
        for (size_t k = 4; k < w.size(); k++) {
            int16_t v = 0;
            RT_CHECK(pc_rando_brk_key_value(b.data(), b.size(), anim, ch, (int)(k - 4), &v));
            RT_CHECK_EQ((int)v, std::atoi(w[k].c_str()));
            checked++;
        }
    }
    RT_CHECK(checked >= 30);

    // The cutf form of the edit: key frame i = trail i, only the chosen trails.
    PcRandoPlayerColors c;
    pc_rando_player_colors_clear(&c);
    Bytes a = fx.brk.at("A");
    RT_CHECK_EQ(pc_rando_player_colors_brk(a.data(), a.size(), &c), 0);
    RT_CHECK(a == fx.brk.at("A"));
    c.hasTrail[PCR_TRAIL_PARRYING] = 1;
    c.trail[PCR_TRAIL_PARRYING][0] = 1;
    c.trail[PCR_TRAIL_PARRYING][1] = 2;
    c.trail[PCR_TRAIL_PARRYING][2] = 3;
    c.trail[PCR_TRAIL_PARRYING][3] = 4;
    RT_CHECK_EQ(pc_rando_player_colors_brk(a.data(), a.size(), &c), 1);
    int16_t v = 0;
    RT_CHECK(pc_rando_brk_key_value(a.data(), a.size(), 0, 0, 2, &v) && v == 1);
    RT_CHECK(pc_rando_brk_key_value(a.data(), a.size(), 0, 1, 2, &v) && v == 2);
    RT_CHECK(pc_rando_brk_key_value(a.data(), a.size(), 0, 2, 2, &v) && v == 3);
    RT_CHECK(pc_rando_brk_key_value(a.data(), a.size(), 0, 3, 0, &v) && v == 255); // alpha untouched
    int diff = 0;
    for (size_t i = 0; i < a.size(); i++) {
        diff += a[i] != fx.brk.at("A")[i] ? 1 : 0;
    }
    RT_CHECK(diff >= 1 && diff <= 6);

    // Refusals leave the file alone: anim 1 of A has key counts of 3 but asking 4 keys fails; a slot
    // read by another track; out of range; not a J3D file.
    Bytes r = fx.brk.at("A");
    const uint8_t rgb[3] = {7, 7, 7};
    RT_CHECK_EQ(pc_rando_brk_set_key_rgb(r.data(), r.size(), 0, 0, 4, rgb), 0);
    RT_CHECK_EQ(pc_rando_brk_set_key_rgb(r.data(), r.size(), 5, 0, 3, rgb), 0);
    RT_CHECK_EQ(pc_rando_brk_set_key_rgb(r.data(), r.size(), 0, 3, 3, rgb), 0);
    RT_CHECK(r == fx.brk.at("A"));
    Bytes junk(64, 0);
    RT_CHECK_EQ(pc_rando_brk_set_key_rgb(junk.data(), junk.size(), 0, 0, 3, rgb), -1);
    RT_CHECK_EQ(pc_rando_brk_reg_anim_count(junk.data(), junk.size()), -1);
    RT_CHECK_EQ(pc_rando_brk_reg_anim_count(r.data(), r.size()), 2);
    // Make anim 1's r track point at anim 0's r data (a shared slot): the edit of anim 0 is refused.
    Bytes sh = fx.brk.at("A");
    const size_t trk = 0x20;
    const uint32_t anims = ((uint32_t)sh[trk + 0x20] << 24) | ((uint32_t)sh[trk + 0x21] << 16) |
                           ((uint32_t)sh[trk + 0x22] << 8) | sh[trk + 0x23];
    // anim 1 r track: {count 3, index 0, tangent 0} = anim 0's r track.
    memcpy(&sh[trk + anims + 28], &sh[trk + anims], 6);
    const Bytes before = sh;
    RT_CHECK_EQ(pc_rando_brk_set_key_rgb(sh.data(), sh.size(), 0, 1, 3, rgb), 0);
    RT_CHECK(sh == before);
}

void testHitomi(const Fixture& fx) {
    int checked = 0;
    for (const auto& w : fx.hx) {
        Bytes t = fx.tex.at(w[1]);
        PcRandoTexture tex;
        RT_CHECK_EQ(pc_rando_player_tex_open(&tex, t.data()), (int)PCR_TEX_OK);
        PcRandoPlayerColors c;
        pc_rando_player_colors_clear(&c);
        c.hasEyes = 1;
        rgbOf(w[2], c.eyesBase);
        rgbOf(w[3], c.eyes);
        const int r = pc_rando_player_colors_hitomi(&tex, &c);
        if (w[4] != "ok") {
            RT_CHECK(r != PCR_TEX_OK);
            continue;
        }
        RT_CHECK_EQ(r, (int)PCR_TEX_OK);
        RT_CHECK_EQ((int)tex.numColors, std::atoi(w[5].c_str()));
        const Bytes img = unhex(w[6]), pal = unhex(w[7]);
        RT_CHECK_EQ(tex.imageSize, img.size());
        RT_CHECK(memcmp(tex.image, img.data(), img.size()) == 0);
        RT_CHECK_EQ((size_t)tex.numColors * 2, pal.size());
        RT_CHECK(memcmp(tex.palette, pal.data(), pal.size()) == 0);
        // The bytes past the new palette are the old ones (the in-place file keeps its layout).
        checked++;
    }
    RT_CHECK(checked >= 15);

    // Not chosen: skipped, nothing changed.
    Bytes t = fx.tex.at("C8_565");
    PcRandoTexture tex;
    RT_CHECK_EQ(pc_rando_player_tex_open(&tex, t.data()), (int)PCR_TEX_OK);
    PcRandoPlayerColors none;
    pc_rando_player_colors_clear(&none);
    RT_CHECK_EQ(pc_rando_player_colors_hitomi(&tex, &none), (int)PCR_TEX_SKIPPED);
    RT_CHECK(t == fx.tex.at("C8_565"));
    // A palette with no room for more colours is refused, unchanged: one entry of room, many colours.
    Bytes small = fx.tex.at("C8_565");
    small[0x0A] = 0;
    small[0x0B] = 1;
    PcRandoTexture st;
    RT_CHECK_EQ(pc_rando_player_tex_open(&st, small.data()), (int)PCR_TEX_OK);
    std::vector<uint8_t> rgba((size_t)st.width * st.height * 4, 0);
    for (size_t i = 0; i < rgba.size(); i++) {
        rgba[i] = (uint8_t)(i * 7);
    }
    const Bytes keep = small;
    RT_CHECK(pc_rando_player_tex_replace(&st, rgba.data()) != PCR_TEX_OK);
    RT_CHECK(small == keep);
}

// A synthetic JPAC1-00 file with particles 0x0010 and 0x0048, each a BSP1 and a TDB1, no textures.
Bytes makeJpc() {
    auto u16 = [](Bytes& b, uint16_t v) {
        b.push_back((uint8_t)(v >> 8));
        b.push_back((uint8_t)v);
    };
    auto u32 = [](Bytes& b, uint32_t v) {
        for (int s = 24; s >= 0; s -= 8) {
            b.push_back((uint8_t)(v >> s));
        }
    };
    Bytes f = {'J', 'P', 'A', 'C', '1', '-', '0', '0'};
    u16(f, 2); // particles
    u16(f, 0); // textures
    while (f.size() < 0x20) {
        f.push_back(0);
    }
    for (uint16_t id : {(uint16_t)0x0010, (uint16_t)0x0048}) {
        Bytes bsp(0x60, 0);
        memcpy(bsp.data(), "BSP1", 4);
        bsp[7] = 0x60;
        // prm colour at 0x2C, env at 0x30 (no key tables).
        const uint8_t prm[4] = {0xFF, 0xFF, 0xFF, 0x64}, env[4] = {0x10, 0x20, 0x30, 0x40};
        memcpy(&bsp[0x2C], prm, 4);
        memcpy(&bsp[0x30], env, 4);
        Bytes tdb(0x0C, 0);
        memcpy(tdb.data(), "TDB1", 4);
        tdb[7] = 0x0C;
        Bytes p = {'J', 'E', 'F', 'F', 'j', 'p', 'a', '1'};
        u32(p, 0x12345678);
        u32(p, 2); // blocks
        u32(p, 0x20 + (uint32_t)bsp.size() + (uint32_t)tdb.size());
        p.push_back(0); // kfa
        p.push_back(0); // fld
        p.push_back(0); // textures
        p.push_back(0);
        u16(p, id);
        u16(p, 0);
        u16(p, 0);
        u16(p, 0);
        p.insert(p.end(), bsp.begin(), bsp.end());
        p.insert(p.end(), tdb.begin(), tdb.end());
        f.insert(f.end(), p.begin(), p.end());
    }
    return f;
}

void testJpc() {
    Bytes f = makeJpc();
    std::vector<PcRandoJpcParticle> ps;
    const int scan = pc_rando_jpc_scan(f.data(), f.size(), &ps, nullptr);
    RT_CHECK_EQ(scan, (int)PCR_JPC_OK);
    if (scan != PCR_JPC_OK) {
        return;
    }
    const Bytes orig = f;
    PcRandoPlayerColors c;
    pc_rando_player_colors_clear(&c);
    RT_CHECK_EQ(pc_rando_player_colors_jpc(f.data(), f.size(), &c), 0);
    RT_CHECK(f == orig);
    c.hasTrail[PCR_TRAIL_ARROW] = 1;
    c.trail[PCR_TRAIL_ARROW][0] = 1;
    c.trail[PCR_TRAIL_ARROW][1] = 2;
    c.trail[PCR_TRAIL_ARROW][2] = 3;
    c.trail[PCR_TRAIL_ARROW][3] = 0x96; // the vanilla alpha of a colour given without one: not used
    RT_CHECK_EQ(pc_rando_player_colors_jpc(f.data(), f.size(), &c), 1);
    PcRandoJpcRgba got;
    for (const PcRandoJpcParticle& p : ps) {
        RT_CHECK_EQ(pc_rando_jpc_get_color(f.data(), p, PCR_JPC_BSP1_PRM, &got), (int)PCR_JPC_OK);
        if (p.id == 0x48) {
            RT_CHECK(got.r == 1 && got.g == 2 && got.b == 3 && got.a == 0x64);
        } else {
            RT_CHECK(got.r == 0xFF && got.g == 0xFF && got.b == 0xFF && got.a == 0x64);
        }
        RT_CHECK_EQ(pc_rando_jpc_get_color(f.data(), p, PCR_JPC_BSP1_ENV, &got), (int)PCR_JPC_OK);
        RT_CHECK(got.r == 0x10 && got.a == 0x40);
    }
    int diff = 0;
    for (size_t i = 0; i < f.size(); i++) {
        diff += f[i] != orig[i] ? 1 : 0;
    }
    RT_CHECK_EQ(diff, 3);
    c.trailHasAlpha[PCR_TRAIL_ARROW] = 1;
    c.trail[PCR_TRAIL_ARROW][3] = 0x22;
    RT_CHECK_EQ(pc_rando_player_colors_jpc(f.data(), f.size(), &c), 1);
    for (const PcRandoJpcParticle& p : ps) {
        if (p.id == 0x48) {
            RT_CHECK(pc_rando_jpc_get_color(f.data(), p, PCR_JPC_BSP1_PRM, &got) == PCR_JPC_OK && got.a == 0x22);
        }
    }
    // No particle 0x48: refused.
    Bytes g = makeJpc();
    for (const PcRandoJpcParticle& p : ps) {
        if (p.id == 0x48) {
            g[p.offset + 0x18] = 0x00;
            g[p.offset + 0x19] = 0x49;
        }
    }
    const Bytes gk = g;
    RT_CHECK_EQ(pc_rando_player_colors_jpc(g.data(), g.size(), &c), -1);
    RT_CHECK(g == gk);
}

} // namespace

int main() {
    testParse();
    Fixture fx;
    RT_CHECK(loadFixture(fx));
    if (!fx.brk.empty()) {
        testBrk(fx);
        testHitomi(fx);
    }
    testJpc();
    return rt::finish();
}
