// Unit test of plan step R7.2a: the enemy palette generator and its colour math.
//
//   - pc_rando_color: hsvShiftColor on 53,024 colours (20,000 pseudo-random, the 256 greys, a 32-level
//     grid) under 30 shifts gives the bytes of gclib's texture_utils.hsv_shift_color
//     (fixtures/R7_2a_colors.txt: an FNV-1a 64 per shift and 16 full samples), and hsvShiftPalette
//     on RGBA colours equals hsv_shift_palette with the alpha kept; colorsys basics;
//   - the group table (generated/rando_palettes.inc): 37 groups in file order;
//   - the generator: TestSeed1 with randomize_enemy_palettes gives wwrando's "palettes"
//     (fixtures/R7_2a_TestSeed1_palettes.json, ww_export.py) and the same locations; the key is
//     absent with the option off; the option is refused unless GenRequest::allowNotRuntimeReady
//     (the menu, until R7.2e); palettesFromJson round-trips and rejects bad values; the seed file
//     keeps the key and hashes it.
// The 50-seed oracle batch is regress_rando/R7.2a.txt (check_seeds.py --golden).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "pc_rando_color.h"
#include "pc_rando_gen.h"
#include "pc_rando_gen_palettes.h"
#include "pc_rando_json.h"
#include "pc_rando_seedfile.h"
#include "rando_test.h"

namespace {

constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ull;
constexpr uint64_t kFnvPrime = 0x100000001B3ull;

struct Rgb {
    uint8_t r, g, b;
};

// The colours of R7_2a_make_fixture.py, in its order.
std::vector<Rgb> fixtureColours() {
    std::vector<Rgb> out;
    uint32_t x = 0x2545F491u;
    for (int i = 0; i < 20000; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        out.push_back({(uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF), (uint8_t)((x >> 16) & 0xFF)});
    }
    for (int g = 0; g < 256; g++) {
        out.push_back({(uint8_t)g, (uint8_t)g, (uint8_t)g});
    }
    uint8_t levels[32];
    for (int i = 0; i < 32; i++) {
        levels[i] = (uint8_t)((i * 255) / 31);
    }
    for (int r = 0; r < 32; r++) {
        for (int g = 0; g < 32; g++) {
            for (int b = 0; b < 32; b++) {
                out.push_back({levels[r], levels[g], levels[b]});
            }
        }
    }
    return out;
}

bool parseHexRgba(const std::string& tok, uint8_t out[4]) {
    if (tok.size() != 8) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        out[i] = (uint8_t)std::strtoul(tok.substr(i * 2, 2).c_str(), nullptr, 16);
    }
    return true;
}

void testColours() {
    std::string text;
    if (!RT_CHECK(rt::readFixture("R7_2a_colors.txt", text))) {
        return;
    }
    const std::vector<Rgb> cols = fixtureColours();
    std::istringstream in(text);
    std::string line;
    int shifts = 0, samples = 0, pals = 0, bad = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string kind;
        ls >> kind;
        if (kind == "count") {
            size_t n = 0;
            ls >> n;
            RT_CHECK_EQ(n, cols.size());
        } else if (kind == "shift") {
            int h, v;
            std::string hex;
            ls >> h >> v >> hex;
            const uint64_t want = std::strtoull(hex.c_str(), nullptr, 16);
            uint64_t fnv = kFnvOffset;
            for (const Rgb& c : cols) {
                uint8_t r = c.r, g = c.g, b = c.b;
                pcr::hsvShiftColor(&r, &g, &b, h, v);
                for (uint8_t byte : {r, g, b}) {
                    fnv = (fnv ^ byte) * kFnvPrime;
                }
            }
            if (!RT_CHECK_EQ(fnv, want)) {
                std::fprintf(stderr, "  shift %d %d: colours differ from gclib\n", h, v);
                bad++;
            }
            shifts++;
        } else if (kind == "sample") {
            int h, v, r0, g0, b0, r1, g1, b1;
            ls >> h >> v >> r0 >> g0 >> b0 >> r1 >> g1 >> b1;
            uint8_t r = (uint8_t)r0, g = (uint8_t)g0, b = (uint8_t)b0;
            pcr::hsvShiftColor(&r, &g, &b, h, v);
            if (!RT_CHECK(r == r1 && g == g1 && b == b1)) {
                std::fprintf(stderr, "  sample %d %d (%d %d %d): ours %d %d %d, gclib %d %d %d\n", h, v, r0, g0,
                             b0, r, g, b, r1, g1, b1);
            }
            samples++;
        } else if (kind == "pal") {
            int h, v;
            ls >> h >> v;
            std::vector<uint8_t> src, want;
            std::string tok;
            bool arrow = false;
            while (ls >> tok) {
                if (tok == "->") {
                    arrow = true;
                    continue;
                }
                uint8_t c[4];
                RT_CHECK(parseHexRgba(tok, c));
                (arrow ? want : src).insert((arrow ? want : src).end(), c, c + 4);
            }
            RT_CHECK_EQ(src.size(), (size_t)32);
            RT_CHECK_EQ(want.size(), src.size());
            pcr::hsvShiftPalette(src.data(), src.size() / 4, 4, h, v);
            RT_CHECK(src == want);
            pals++;
        } else {
            RT_CHECK(!"unknown fixture line");
        }
    }
    RT_CHECK_EQ(shifts, 30);
    RT_CHECK_EQ(samples, 30 * 16);
    RT_CHECK_EQ(pals, 30);
    RT_CHECK_EQ(bad, 0);

    // Stride 3 equals one colour at a time.
    uint8_t rgb[6] = {10, 200, 30, 250, 128, 0};
    uint8_t a[3] = {10, 200, 30}, b[3] = {250, 128, 0};
    pcr::hsvShiftPalette(rgb, 2, 3, 100, -20);
    pcr::hsvShiftColor(&a[0], &a[1], &a[2], 100, -20);
    pcr::hsvShiftColor(&b[0], &b[1], &b[2], 100, -20);
    RT_CHECK(std::memcmp(rgb, a, 3) == 0 && std::memcmp(rgb + 3, b, 3) == 0);

    // colorsys basics: pure red, a grey, and hsv_to_rgb at the sector edges.
    double h, s, v;
    pcr::rgbToHsv(1.0, 0.0, 0.0, &h, &s, &v);
    RT_CHECK(h == 0.0 && s == 1.0 && v == 1.0);
    pcr::rgbToHsv(0.5, 0.5, 0.5, &h, &s, &v);
    RT_CHECK(h == 0.0 && s == 0.0 && v == 0.5);
    pcr::rgbToHsv(0.0, 0.0, 1.0, &h, &s, &v);
    RT_CHECK(h == 4.0 / 6.0);
    double r, g, bb;
    pcr::hsvToRgb(1.0 / 3.0, 1.0, 1.0, &r, &g, &bb);
    RT_CHECK(r == 0.0 && g == 1.0 && bb == 0.0);
    pcr::hsvToRgb(1.0, 1.0, 1.0, &r, &g, &bb); // i = 6 wraps to sector 0
    RT_CHECK(r == 1.0 && g == 0.0 && bb == 0.0);
}

void testGroups() {
    RT_CHECK_EQ(pcr::paletteGroupCount(), 37);
    RT_CHECK_EQ(std::string(pcr::paletteGroup(0).name), std::string("Bokoblin"));
    RT_CHECK_EQ(pcr::paletteGroup(0).numRarcs, 2);
    RT_CHECK_EQ(std::string(pcr::paletteGroup(0).rarcs[1].name), std::string("Nata"));
    RT_CHECK_EQ(std::string(pcr::paletteGroup(36).name), std::string("Ganondorf"));
    const int armos = pcr::paletteGroupIndex("Armos");
    if (RT_CHECK(armos >= 0)) {
        const pcr::PaletteGroup& g = pcr::paletteGroup(armos);
        RT_CHECK_EQ(g.numParticleIds, 3);
        RT_CHECK(g.particleIds[0] == 0x81AE && g.particleIds[2] == 0x81B0);
    }
    RT_CHECK_EQ(pcr::paletteGroupIndex("No Such Enemy"), -1);
    int excluded = 0;
    for (int i = 0; i < pcr::paletteGroupCount(); i++) {
        for (int k = 0; k < pcr::paletteGroup(i).numRarcs; k++) {
            excluded += pcr::paletteGroup(i).rarcs[k].numExcluded;
        }
    }
    RT_CHECK_EQ(excluded, 6); // pt.brk, aotsi.bdl, damage_eye_a1/b1.brk, hmos3.bdl, s_tshutter.bdl
}

pcr::GenRequest request(bool palettes) {
    pcr::GenRequest req;
    req.seed = "TestSeed1";
    req.allowMissingHints = true;
    req.version = "test";
    req.options.randomize_enemy_palettes = palettes;
    return req;
}

void testGenerator() {
    std::string text, err;
    PcRandoSeed want;
    if (!RT_CHECK(rt::readFixture("R7_2a_TestSeed1_palettes.json", text)) ||
        !RT_CHECK(pc_rando_seed_parse(text, &want, &err))) {
        std::fprintf(stderr, "  fixture: %s\n", err.c_str());
        return;
    }
    const pcr::Json* wantPal = want.extra.find("palettes");
    if (!RT_CHECK(wantPal != nullptr)) {
        return;
    }

    // Refused by default (the menu), with the step that makes it live.
    {
        pcr::GenResult res;
        RT_CHECK(!pcr::generate(request(true), res));
        RT_CHECK(res.unsupported);
        RT_CHECK(res.error.find("Not supported yet: randomize_enemy_palettes") != std::string::npos);
        RT_CHECK(res.error.find("R7.2e") != std::string::npos);
        RT_CHECK(!pcr::unsupportedOptionsError(request(true).options, true).empty());
        RT_CHECK(pcr::unsupportedOptionsError(request(true).options, true, true).empty());
    }

    pcr::GenRequest req = request(true);
    req.allowNotRuntimeReady = true;
    pcr::GenResult res;
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  TestSeed1 palettes: %s\n", res.error.c_str());
        return;
    }
    const pcr::Json* ours = res.seed.extra.find("palettes");
    if (RT_CHECK(ours != nullptr) && !RT_CHECK(*ours == *wantPal)) {
        std::fprintf(stderr, "  ours %s\n  want %s\n", pcr::jsonDump(*ours, -1).c_str(),
                     pcr::jsonDump(*wantPal, -1).c_str());
    }
    RT_CHECK_EQ(res.seed.permalink, want.permalink);
    RT_CHECK(res.seed.location_item_names == want.location_item_names);
    RT_CHECK(std::memcmp(res.seed.locations, want.locations, sizeof want.locations) == 0);
    RT_CHECK_EQ(res.pigsBitfield, 0x01); // other randomizers' draws are untouched (fresh RNG each)

    std::vector<pcr::PaletteShift> shifts;
    if (RT_CHECK(pcr::palettesFromJson(*ours, shifts, &err))) {
        RT_CHECK_EQ(shifts.size(), (size_t)37);
        bool inRange = true;
        for (const pcr::PaletteShift& s : shifts) {
            inRange = inRange && s.h >= 20 && s.h <= 340 && s.v >= -40 && s.v <= 40;
        }
        RT_CHECK(inRange);
        RT_CHECK(pcr::palettesToJson(shifts) == *ours);
    }

    // The seed file keeps the key through a round trip, and it is part of the hash.
    PcRandoSeed back;
    RT_CHECK(pc_rando_seed_parse(pc_rando_seed_serialize(res.seed), &back, &err));
    RT_CHECK(back.extra.find("palettes") != nullptr && *back.extra.find("palettes") == *ours);
    RT_CHECK(std::memcmp(back.hash, res.seed.hash, 16) == 0);
    PcRandoSeed changed = res.seed;
    pcr::Json* p = changed.extra.find("palettes");
    p->at(0).at(0) = pcr::Json::integer(p->at(0).at(0).asInt() == 20 ? 21 : 20);
    pc_rando_seed_update_hash(&changed);
    RT_CHECK(std::memcmp(changed.hash, res.seed.hash, 16) != 0);

    // Off: no key, same locations.
    pcr::GenResult off;
    if (RT_CHECK(pcr::generate(request(false), off))) {
        RT_CHECK(off.seed.extra.find("palettes") == nullptr);
        RT_CHECK(off.seed.location_item_names == res.seed.location_item_names);
    }
}

void testFromJsonErrors() {
    std::vector<pcr::PaletteShift> v(37);
    for (int i = 0; i < 37; i++) {
        v[i].h = 20 + i;
        v[i].v = -40 + i;
    }
    const pcr::Json good = pcr::palettesToJson(v);
    std::vector<pcr::PaletteShift> out;
    std::string err;
    RT_CHECK(pcr::palettesFromJson(good, out, &err) && out == v);

    pcr::Json missing = good;
    missing.erase("Bokoblin");
    RT_CHECK(!pcr::palettesFromJson(missing, out, &err) && err.find("missing group Bokoblin") != std::string::npos);
    RT_CHECK(out.empty());

    pcr::Json unknown = good;
    unknown.set("Nobody", good.at(0));
    RT_CHECK(!pcr::palettesFromJson(unknown, out, &err) && err.find("unknown group") != std::string::npos);

    pcr::Json range = good;
    range.at(1).at(0) = pcr::Json::integer(341);
    RT_CHECK(!pcr::palettesFromJson(range, out, &err) && err.find("out of range") != std::string::npos);

    pcr::Json shape = good;
    shape.at(2) = pcr::Json::integer(5);
    RT_CHECK(!pcr::palettesFromJson(shape, out, &err) && err.find("not [h_shift") != std::string::npos);

    RT_CHECK(!pcr::palettesFromJson(pcr::Json::array(), out, &err));
}

} // namespace

int main() {
    testColours();
    testGroups();
    testGenerator();
    testFromJsonErrors();
    return rt::finish();
}
