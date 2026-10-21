// R3.1 (docs/RANDOMIZER_PLAN.md): the pure part of the new-game state (pc_rando_newgame.h):
//  1. the switch table gives exactly the u32 words custom_funcs.asm init_save_with_tweaks stores in
//     the eight stage infos (and nothing in the words it leaves alone);
//  2. the story event bits are the asm's 38 (distinct, none in the D17 range), the rematch bits its 4;
//  3. the plan of the default config, and of swordless / no starting sword / 8 shards / no rematch
//     skip / no chart / more magic and gear configs.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_newgame.h"

#include <cstring>
#include <set>

namespace {

struct Word {
    int stage;
    int word; // asm offset (4 + 4 * word) of the stage info
    uint32_t value;
};

// The stw's of custom_funcs.asm init_save_with_tweaks (lis/addi values as stored).
constexpr Word kAsmWords[] = {
    {11, 0, 0x4008038B}, {11, 1, 0xC0000000},                      // Outset 0x803C5114 +4, +8
    {0, 0, 0x02000000},  {0, 1, 0x80000000}, {0, 2, 0x41010080},   // sea 0x803C4F88 +4, +8, +0xC
    {3, 0, 0x00000200},  {3, 1, 0x00000002}, {3, 2, 0x00000040},   // DRC 0x803C4FF4
    {4, 1, 0x00400000},                                            // FW 0x803C5018 +8
    {5, 1, 0x00002000},  {5, 3, 0x00000008},                       // TotG 0x803C503C +8, +0x10
    {6, 1, 0x00000400},                                            // ET 0x803C5060 +8
    {9, 0, 0x00040040},                                            // Hyrule 0x803C50CC +4
    {8, 0, 0xF0042000},                                            // GT 0x803C50A8 +4
};

uint32_t asmWord(int stage, int word) {
    for (const Word& w : kAsmWords) {
        if (w.stage == stage && w.word == word) {
            return w.value;
        }
    }
    return 0;
}

} // namespace

int main() {
    // 1. switch words
    for (int stage = 0; stage < 16; stage++) {
        for (int word = 0; word < 4; word++) {
            RT_CHECK_EQ(pc_rando_newgame_switch_word(stage, word), asmWord(stage, word));
        }
    }
    {
        std::set<int> seen;
        for (const PcrNewgameSwitch& s : pcr_newgame_switches) {
            RT_CHECK(s.sw < 128);
            RT_CHECK(seen.insert(s.stage * 256 + s.sw).second);
        }
    }

    // 2. event bits
    const uint16_t asmBits[PCR_NEWGAME_STORY_BITS] = {
        0x3510, 0x2A80, 0x0280, 0x0520, 0x2E01, 0x0F80, 0x0908, 0x2A08, 0x0902, 0x1F40,
        0x0A80, 0x0901, 0x0A20, 0x1801, 0x0A08, 0x0808, 0x1F02, 0x2F20, 0x3840, 0x2D04,
        0x3802, 0x2D01, 0x2D02, 0x3201, 0x3380, 0x1001, 0x2E04, 0x2920, 0x1620, 0x3304,
        0x2910, 0x1610, 0x3440, 0x3A20, 0x2D08, 0x3980, 0x3B02, 0x4002,
    };
    {
        std::set<uint16_t> seen;
        for (int i = 0; i < PCR_NEWGAME_STORY_BITS; i++) {
            RT_CHECK_EQ(pcr_newgame_story_bits[i], asmBits[i]);
            RT_CHECK(seen.insert(pcr_newgame_story_bits[i]).second);
            const uint16_t b = pcr_newgame_story_bits[i];
            RT_CHECK(!(b >= 0x6901 && b <= 0x6980) && !(b >= 0x6A01 && b <= 0x6A40));
        }
    }
    const uint16_t rematch[4] = {0x3904, 0x3902, 0x3901, 0x3A80};
    for (int i = 0; i < 4; i++) {
        RT_CHECK_EQ(pcr_newgame_rematch_bits[i], rematch[i]);
    }

    // 3. plans
    PcRandoNewGamePlan p;
    pc_rando_newgame_plan(nullptr, &p);
    RT_CHECK_EQ(p.give_sword, 1);
    RT_CHECK_EQ(p.n_barrier_bits, 0);
    RT_CHECK_EQ(p.magic, 16);
    RT_CHECK_EQ(p.n_gear, 3); // Ballad of Gales, Progressive Shield, Song of Passing
    RT_CHECK_EQ(p.gear[0], 0x6E);
    RT_CHECK_EQ(p.gear[1], 0x3B);
    RT_CHECK_EQ(p.gear[2], 0x72);
    RT_CHECK_EQ(p.pigs, 0x04);
    RT_CHECK_EQ(p.targeting_mode, 0);
    RT_CHECK_EQ(p.triforce, 0x00);
    RT_CHECK_EQ(p.n_end_bits, 4);
    for (int i = 0; i < 4; i++) {
        RT_CHECK_EQ(p.end_bits[i], rematch[i]);
    }
    RT_CHECK_EQ(p.reveal_chart, 1);

    PcRandoConfig c;
    pc_rando_config_defaults(&c);
    c.sword_mode = PC_RANDO_SWORD_SWORDLESS;
    c.num_triforce_shards = 8;
    c.skip_rematch_bosses = 0;
    c.reveal_full_sea_chart = 0;
    c.starting_magic = 32;
    c.targeting_mode = 1;
    c.pigs_bitfield = 0x01;
    memset(c.starting_gear, PC_RANDO_ITEM_END, sizeof c.starting_gear);
    pc_rando_newgame_plan(&c, &p);
    RT_CHECK_EQ(p.give_sword, 0);
    RT_CHECK_EQ(p.n_barrier_bits, 2);
    RT_CHECK_EQ(p.barrier_bits[0], 0x2C02);
    RT_CHECK_EQ(p.barrier_bits[1], 0x3B08);
    RT_CHECK_EQ(p.magic, 32);
    RT_CHECK_EQ(p.n_gear, 0);
    RT_CHECK_EQ(p.pigs, 0x01);
    RT_CHECK_EQ(p.targeting_mode, 1);
    RT_CHECK_EQ(p.triforce, 0xFF);
    RT_CHECK_EQ(p.n_end_bits, 1);
    RT_CHECK_EQ(p.end_bits[0], 0x3D04);
    RT_CHECK_EQ(p.reveal_chart, 0);

    c.sword_mode = PC_RANDO_SWORD_NO_START;
    c.num_triforce_shards = 5;
    for (int i = 0; i < PC_RANDO_MAX_STARTING_GEAR; i++) {
        c.starting_gear[i] = (uint8_t)(0x20 + (i % 0x40));
    }
    pc_rando_newgame_plan(&c, &p);
    RT_CHECK_EQ(p.give_sword, 0);
    RT_CHECK_EQ(p.n_barrier_bits, 0);
    RT_CHECK_EQ(p.triforce, 0x1F);
    RT_CHECK_EQ(p.n_end_bits, 0);
    RT_CHECK_EQ(p.n_gear, PC_RANDO_MAX_STARTING_GEAR);
    RT_CHECK_EQ(p.gear[85], 0x20 + (85 % 0x40));

    // skip_rematch_bosses is compared with 1 (cmpwi r5, 1; bne).
    c.skip_rematch_bosses = 2;
    pc_rando_newgame_plan(&c, &p);
    RT_CHECK_EQ(p.n_end_bits, 0);
    c.num_triforce_shards = 0;
    c.skip_rematch_bosses = 1;
    pc_rando_newgame_plan(&c, &p);
    RT_CHECK_EQ(p.triforce, 0x00);
    RT_CHECK_EQ(p.n_end_bits, 4);

    return rt::finish();
}
