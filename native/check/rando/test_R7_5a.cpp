// Unit test of step R7.5a (docs/RANDOMIZER_PLAN.md): the pure part of the cosmetic tweaks wwrando
// always applies (features/rando/pc_rando_tweak_cosmetic.h). The vectors are synthetic, computed with
// Python's colorsys (give_fairy_fountains_distinct_colors) and numpy float32 (the f32 arithmetic of
// misc_rando_features.asm check_animate_rainbow_rupee_color); no disc bytes. The real Pale records are
// compared with wwrando on the disc by native/tools/rando/cosmetic_check.py.
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_tweak_cosmetic.h"

#include <cstring>

namespace {

struct HueCase {
    uint8_t in[3];
    uint8_t out[6][3]; // hue 0/6 .. 5/6, as wwrando sums it
};

// colorsys.rgb_to_hsv(c/255) -> hsv_to_rgb(hue, s, v) -> int(x*255), random.seed(75) for the last 30.
const HueCase kHue[] = {
    {{255, 255, 255}, {{255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}}},
    {{0, 0, 0}, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}},
    {{128, 128, 128}, {{128, 128, 128}, {128, 128, 128}, {128, 128, 128}, {128, 128, 128}, {128, 128, 128}, {128, 128, 128}}},
    {{255, 0, 0}, {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {255, 0, 255}}},
    {{0, 255, 0}, {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {255, 0, 255}}},
    {{0, 0, 255}, {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {255, 0, 255}}},
    {{200, 100, 50}, {{200, 50, 50}, {200, 200, 50}, {50, 200, 50}, {50, 200, 200}, {50, 50, 200}, {200, 50, 200}}},
    {{1, 2, 3}, {{3, 1, 1}, {3, 3, 1}, {1, 3, 1}, {1, 3, 3}, {1, 1, 3}, {3, 1, 3}}},
    {{255, 254, 0}, {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {255, 0, 255}}},
    {{10, 200, 180}, {{200, 9, 9}, {200, 200, 9}, {9, 200, 9}, {9, 200, 200}, {9, 9, 200}, {200, 9, 200}}},
    {{230, 223, 232}, {{232, 223, 223}, {232, 232, 223}, {223, 232, 223}, {223, 232, 232}, {223, 223, 232}, {232, 223, 232}}},
    {{21, 242, 176}, {{242, 21, 21}, {242, 242, 21}, {21, 242, 21}, {21, 242, 242}, {21, 21, 242}, {242, 21, 242}}},
    {{51, 162, 42}, {{162, 41, 41}, {162, 162, 41}, {41, 162, 41}, {41, 162, 162}, {41, 41, 162}, {162, 41, 162}}},
    {{128, 140, 254}, {{254, 128, 128}, {254, 254, 128}, {128, 254, 128}, {128, 254, 254}, {128, 128, 254}, {254, 128, 254}}},
    {{178, 248, 137}, {{248, 137, 137}, {248, 248, 137}, {137, 248, 137}, {137, 248, 248}, {137, 137, 248}, {248, 137, 248}}},
    {{209, 14, 193}, {{209, 14, 14}, {209, 209, 14}, {14, 209, 14}, {14, 209, 209}, {14, 14, 209}, {209, 14, 209}}},
    {{236, 101, 67}, {{236, 66, 66}, {236, 236, 66}, {66, 236, 66}, {66, 236, 236}, {66, 66, 236}, {236, 66, 236}}},
    {{100, 73, 154}, {{154, 73, 73}, {154, 154, 73}, {73, 154, 73}, {73, 154, 154}, {73, 73, 154}, {154, 73, 154}}},
    {{252, 86, 237}, {{252, 86, 86}, {252, 252, 86}, {86, 252, 86}, {86, 252, 252}, {86, 86, 252}, {252, 86, 252}}},
    {{9, 157, 30}, {{157, 8, 8}, {157, 157, 8}, {8, 157, 8}, {8, 157, 157}, {8, 8, 157}, {157, 8, 157}}},
    {{35, 28, 138}, {{138, 28, 28}, {138, 138, 28}, {28, 138, 28}, {28, 138, 138}, {28, 28, 138}, {138, 28, 138}}},
    {{165, 99, 29}, {{165, 29, 29}, {165, 165, 29}, {29, 165, 29}, {29, 165, 165}, {29, 29, 165}, {165, 29, 165}}},
    {{46, 211, 61}, {{211, 46, 46}, {211, 211, 46}, {46, 211, 46}, {46, 211, 211}, {46, 46, 211}, {211, 46, 211}}},
    {{157, 185, 58}, {{185, 58, 58}, {185, 185, 58}, {58, 185, 58}, {58, 185, 185}, {58, 58, 185}, {185, 58, 185}}},
    {{238, 149, 149}, {{238, 149, 149}, {238, 238, 149}, {149, 238, 149}, {149, 238, 238}, {149, 149, 238}, {238, 149, 238}}},
    {{108, 125, 192}, {{192, 108, 108}, {192, 192, 108}, {108, 192, 108}, {108, 192, 192}, {108, 108, 192}, {192, 108, 192}}},
    {{2, 13, 155}, {{155, 1, 1}, {155, 155, 1}, {1, 155, 1}, {1, 155, 155}, {1, 1, 155}, {155, 1, 155}}},
    {{165, 227, 151}, {{227, 151, 151}, {227, 227, 151}, {151, 227, 151}, {151, 227, 227}, {151, 151, 227}, {227, 151, 227}}},
    {{9, 80, 225}, {{225, 9, 9}, {225, 225, 9}, {9, 225, 9}, {9, 225, 225}, {9, 9, 225}, {225, 9, 225}}},
    {{4, 138, 162}, {{162, 3, 3}, {162, 162, 3}, {3, 162, 3}, {3, 162, 162}, {3, 3, 162}, {162, 3, 162}}},
    {{95, 181, 132}, {{181, 95, 95}, {181, 181, 95}, {95, 181, 95}, {95, 181, 181}, {95, 95, 181}, {181, 95, 181}}},
    {{219, 255, 251}, {{255, 219, 219}, {255, 255, 219}, {219, 255, 219}, {219, 255, 255}, {219, 219, 255}, {255, 219, 255}}},
    {{62, 203, 133}, {{203, 62, 62}, {203, 203, 62}, {62, 203, 62}, {62, 203, 203}, {62, 62, 203}, {203, 62, 203}}},
    {{187, 236, 178}, {{236, 178, 178}, {236, 236, 178}, {178, 236, 178}, {178, 236, 236}, {178, 178, 236}, {236, 178, 236}}},
    {{49, 144, 197}, {{197, 48, 48}, {197, 197, 48}, {48, 197, 48}, {48, 197, 197}, {48, 48, 197}, {197, 48, 197}}},
    {{201, 170, 9}, {{201, 9, 9}, {201, 201, 9}, {9, 201, 9}, {9, 201, 201}, {9, 9, 201}, {201, 9, 201}}},
    {{248, 167, 188}, {{248, 166, 166}, {248, 248, 166}, {166, 248, 166}, {166, 248, 248}, {166, 166, 248}, {248, 166, 248}}},
    {{210, 39, 189}, {{210, 39, 39}, {210, 210, 39}, {39, 210, 39}, {39, 210, 210}, {39, 39, 210}, {210, 39, 210}}},
    {{67, 14, 204}, {{204, 13, 13}, {204, 204, 13}, {13, 204, 13}, {13, 204, 204}, {13, 13, 204}, {204, 13, 204}}},
    {{147, 99, 181}, {{181, 99, 99}, {181, 181, 99}, {99, 181, 99}, {99, 181, 181}, {99, 99, 181}, {181, 99, 181}}},
};

// fabs(keyframe) after each of the first 170 draws from 0.0 (float32 bit patterns).
const uint32_t kRainbow[170] = {
    0x3e19999a, 0x3e99999a, 0x3ee66667, 0x3f19999a, 0x3f400000, 0x3f666666, 0x3f866666, 0x3f999999,
    0x3faccccc, 0x3fbfffff, 0x3fd33332, 0x3fe66665, 0x3ff99998, 0x40066666, 0x40100000, 0x4019999a,
    0x40233334, 0x402cccce, 0x40366668, 0x40400002, 0x4049999c, 0x40533336, 0x405cccd0, 0x4066666a,
    0x40700004, 0x4079999e, 0x4081999c, 0x40866669, 0x408b3336, 0x40900003, 0x4094ccd0, 0x4099999d,
    0x409e666a, 0x40a33337, 0x40a80004, 0x40acccd1, 0x40b1999e, 0x40b6666b, 0x40bb3338, 0x40c00000,
    0x40bb3333, 0x40b66666, 0x40b19999, 0x40accccc, 0x40a7ffff, 0x40a33332, 0x409e6665, 0x40999998,
    0x4094cccb, 0x408ffffe, 0x408b3331, 0x40866664, 0x40819997, 0x40799994, 0x406ffffa, 0x40666660,
    0x405cccc6, 0x4053332c, 0x40499992, 0x403ffff8, 0x4036665e, 0x402cccc4, 0x4023332a, 0x40199990,
    0x400ffff6, 0x4006665c, 0x3ff99985, 0x3fe66652, 0x3fd3331f, 0x3fbfffec, 0x3facccb9, 0x3f999986,
    0x3f866653, 0x3f666640, 0x3f3fffda, 0x3f199974, 0x3ee6661b, 0x3e99994e, 0x3e199902, 0x36180000,
    0x3e199a32, 0x3e9999e6, 0x3ee666b3, 0x3f1999c0, 0x3f400026, 0x3f66668c, 0x3f866679, 0x3f9999ac,
    0x3facccdf, 0x3fc00012, 0x3fd33345, 0x3fe66678, 0x3ff999ab, 0x4006666f, 0x40100009, 0x401999a3,
    0x4023333d, 0x402cccd7, 0x40366671, 0x4040000b, 0x404999a5, 0x4053333f, 0x405cccd9, 0x40666673,
    0x4070000d, 0x407999a7, 0x408199a0, 0x4086666d, 0x408b333a, 0x40900007, 0x4094ccd4, 0x409999a1,
    0x409e666e, 0x40a3333b, 0x40a80008, 0x40acccd5, 0x40b199a2, 0x40b6666f, 0x40bb333c, 0x40c00000,
    0x40bb3333, 0x40b66666, 0x40b19999, 0x40accccc, 0x40a7ffff, 0x40a33332, 0x409e6665, 0x40999998,
    0x4094cccb, 0x408ffffe, 0x408b3331, 0x40866664, 0x40819997, 0x40799994, 0x406ffffa, 0x40666660,
    0x405cccc6, 0x4053332c, 0x40499992, 0x403ffff8, 0x4036665e, 0x402cccc4, 0x4023332a, 0x40199990,
    0x400ffff6, 0x4006665c, 0x3ff99985, 0x3fe66652, 0x3fd3331f, 0x3fbfffec, 0x3facccb9, 0x3f999986,
    0x3f866653, 0x3f666640, 0x3f3fffda, 0x3f199974, 0x3ee6661b, 0x3e99994e, 0x3e199902, 0x36180000,
    0x3e199a32, 0x3e9999e6, 0x3ee666b3, 0x3f1999c0, 0x3f400026, 0x3f66668c, 0x3f866679, 0x3f9999ac,
    0x3facccdf, 0x3fc00012,
};

float bitsToFloat(uint32_t u) {
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}
uint32_t floatToBits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

void putBe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// A synthetic stage.dzs: chunk "Pale" with `n` entries (byte i*0x2C+j = 0x10*i + j), then "Virt".
size_t makeDzs(uint8_t* buf, size_t cap, uint32_t n) {
    const uint32_t chunks = 2;
    const uint32_t paleOff = 4 + 12 * chunks;
    const uint32_t virtOff = paleOff + n * 0x2C;
    const size_t size = virtOff + 0x24;
    RT_CHECK(size <= cap);
    memset(buf, 0, size);
    putBe32(buf, chunks);
    memcpy(buf + 4, "Pale", 4);
    putBe32(buf + 8, n);
    putBe32(buf + 12, paleOff);
    memcpy(buf + 16, "Virt", 4);
    putBe32(buf + 20, 1);
    putBe32(buf + 24, virtOff);
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < 0x2C; j++) {
            buf[paleOff + i * 0x2C + j] = (uint8_t)(0x10 * i + j * 7 + 3);
        }
    }
    for (uint32_t j = 0; j < 0x24; j++) {
        buf[virtOff + j] = (uint8_t)(0xA0 + j);
    }
    return size;
}

void testHue() {
    const double kWant[6] = {0, 0.16666666666666666, 0.3333333333333333, 0.5, 0.6666666666666666, 0.8333333333333333};
    for (int k = 0; k < 6; k++) {
        RT_CHECK(pc_rando_cosmetic_fairy_hue(k) == kWant[k]);
    }
    for (const HueCase& c : kHue) {
        for (int k = 0; k < 6; k++) {
            uint8_t rgb[3] = {c.in[0], c.in[1], c.in[2]};
            pc_rando_cosmetic_set_hue(rgb, pc_rando_cosmetic_fairy_hue(k));
            const bool ok = memcmp(rgb, c.out[k], 3) == 0;
            if (!ok) {
                std::fprintf(stderr, "hue %d/6 of (%d,%d,%d): got (%d,%d,%d), want (%d,%d,%d)\n", k, c.in[0],
                             c.in[1], c.in[2], rgb[0], rgb[1], rgb[2], c.out[k][0], c.out[k][1], c.out[k][2]);
            }
            RT_CHECK(ok);
        }
    }
}

void testFairyIndex() {
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_index("Fairy01"), 0);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_index("Fairy06"), 5);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_index("Fairy07"), -1);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_index("fairy01"), -1);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_index(nullptr), -1);
}

void testPatchDzs() {
    static uint8_t raw[512], work[512];
    for (uint32_t n = 1; n <= 2; n++) {
        const size_t size = makeDzs(raw, sizeof raw, n);
        for (int k = 0; k < 6; k++) {
            memcpy(work, raw, size);
            RT_CHECK_EQ(pc_rando_cosmetic_fairy_patch_dzs(work, size, k), (int)n);
            const uint32_t paleOff = 4 + 24;
            for (size_t b = 0; b < size; b++) {
                const bool inPale = b >= paleOff && b < paleOff + n * 0x2C;
                const uint32_t field = inPale ? (uint32_t)((b - paleOff) % 0x2C) : 0;
                const bool edited = inPale && field >= 0x06 && field < 0x0C;
                if (!edited) {
                    RT_CHECK(work[b] == raw[b]);
                }
            }
            for (uint32_t i = 0; i < n; i++) {
                for (uint32_t off : {0x06u, 0x09u}) {
                    uint8_t want[3];
                    memcpy(want, raw + paleOff + i * 0x2C + off, 3);
                    pc_rando_cosmetic_set_hue(want, pc_rando_cosmetic_fairy_hue(k));
                    RT_CHECK(memcmp(work + paleOff + i * 0x2C + off, want, 3) == 0);
                }
            }
        }
    }
    // D9: no Pale chunk, a bad file, a bad index: refused, nothing written.
    size_t size = makeDzs(raw, sizeof raw, 1);
    memcpy(raw + 4, "Palx", 4);
    memcpy(work, raw, size);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_patch_dzs(work, size, 0), -1);
    RT_CHECK(memcmp(work, raw, size) == 0);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_patch_dzs(work, 3, 0), -1);
    size = makeDzs(raw, sizeof raw, 1);
    memcpy(work, raw, size);
    RT_CHECK_EQ(pc_rando_cosmetic_fairy_patch_dzs(work, size, 6), -1);
    RT_CHECK(memcmp(work, raw, size) == 0);
}

void testPatcher() {
    static uint8_t raw[512], work[512];
    const size_t size = makeDzs(raw, sizeof raw, 1);
    pc_rando_tweak_cosmetic_reset_status();
    struct Case {
        const char* stage;
        const char* arc;
        const char* name;
        int k; // -1: not touched
    } cases[] = {
        {"Fairy03", "Stage", "stage.dzs", 2},
        {"Fairy03", "Room0", "room.dzr", -1},
        {"Fairy03", "Stage", "event_list.dat", -1},
        {"sea", "Stage", "stage.dzs", -1},
        {"Fairy06", "Stage", "stage.dzs", 5},
    };
    for (const Case& c : cases) {
        memcpy(work, raw, size);
        PcRandoResFile f;
        memset(&f, 0, sizeof f);
        f.dir = "/res/Stage/x/";
        f.stage = c.stage;
        f.arc = c.arc;
        f.room = -1;
        f.name = c.name;
        f.data = work;
        f.size = size;
        pc_rando_tweak_cosmetic_patch(&f);
        RT_CHECK_EQ(memcmp(work, raw, size) == 0, c.k < 0);
    }
    PcRandoCosmeticStatus st;
    pc_rando_tweak_cosmetic_status(2, &st);
    RT_CHECK_EQ(st.seen, 1u);
    RT_CHECK_EQ(st.patched, 1u);
    RT_CHECK_EQ(st.last, 1);
    pc_rando_tweak_cosmetic_status(0, &st);
    RT_CHECK_EQ(st.seen, 0u);
    RT_CHECK_EQ(st.last, -2);
}

void testRainbow() {
    RT_CHECK_EQ(pc_rando_cosmetic_rainbow_keyframe(), 0.0f); // wwrando's .float 0.0 at boot
    for (int i = 0; i < 170; i++) {
        const float got = pc_rando_cosmetic_rainbow_step();
        if (floatToBits(got) != kRainbow[i]) {
            std::fprintf(stderr, "draw %d: got %.9g (0x%08x), want %.9g\n", i, got, floatToBits(got),
                         bitsToFloat(kRainbow[i]));
        }
        RT_CHECK_EQ(floatToBits(got), kRainbow[i]);
    }
    RT_CHECK_EQ(pc_rando_cosmetic_rainbow_draws(), 170u);
    // The shape: up to 6.0 at draw 39 (the wrap to -6.0), down to ~0 at 79, up to 6.0 at 119.
    RT_CHECK_EQ(bitsToFloat(kRainbow[39]), 6.0f);
    RT_CHECK(bitsToFloat(kRainbow[79]) < 0.001f);
    RT_CHECK_EQ(bitsToFloat(kRainbow[119]), 6.0f);
    float kf = 5.95f;
    RT_CHECK_EQ(pc_rando_cosmetic_rainbow_advance(&kf), 6.0f);
    RT_CHECK_EQ(kf, -6.0f);
    kf = -0.1f;
    RT_CHECK(pc_rando_cosmetic_rainbow_advance(&kf) > 0.049f);
    RT_CHECK(kf > 0.0f);
}

} // namespace

int main() {
    testHue();
    testFairyIndex();
    testPatchDzs();
    testPatcher();
    testRainbow();
    return rt::finish();
}
