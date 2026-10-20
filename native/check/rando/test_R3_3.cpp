// cos_rando_test_R3_3 (docs/RANDOMIZER_PLAN.md step R3.3): the progressive item tables of
// features/rando/pc_rando_progressive.h without a game:
//   - the 20 registered ids and their families (tweaks.py make_items_progressive);
//   - convert_progressive_item_id over every owned state of each family, and the base-id fallback
//     for a full or unreachable state (custom_funcs.asm);
//   - the tier the item functions give, walked from an empty save to a full one;
//   - non-progressive ids pass through;
//   - message 101+0xB1 in the overlay with tweaks.py's fields.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_msg.h"
#include "pc_rando_progressive.h"

#include <cstring>

namespace {

PcRandoProgState empty() {
    PcRandoProgState st;
    memset(&st, 0, sizeof st);
    st.bomb_max = 30; // the rando new game's 30/30 (custom_funcs.asm init_save_with_tweaks)
    st.arrow_max = 30;
    return st;
}

// Applies what the family's item function would give for tier `t` to the state (the save effect of
// the vanilla item function it calls).
void give(int family, int t, PcRandoProgState* st) {
    switch (family) {
    case PCR_PROG_SWORD: st->swords |= (uint8_t)(1 << t); break;
    case PCR_PROG_SHIELD: st->shields |= (uint8_t)(1 << t); break;
    case PCR_PROG_BOW: st->bow |= (uint8_t)(1 << t); break;
    case PCR_PROG_WALLET: st->wallet = (uint8_t)(t + 1); break;
    case PCR_PROG_BOMB_BAG: st->bomb_max = t == 0 ? 60 : 99; break;
    case PCR_PROG_QUIVER: st->arrow_max = t == 0 ? 60 : 99; break;
    case PCR_PROG_PICTO_BOX: st->picto |= (uint8_t)(1 << t); break;
    case PCR_PROG_MAGIC_METER: st->max_magic = t == 0 ? 16 : 32; break;
    default: break;
    }
}

} // namespace

int main() {
    // The 20 ids, in tweaks.py order, and their families.
    const uint8_t ids[PCR_PROG_ID_COUNT] = {0x38, 0x39, 0x3A, 0x3D, 0x3E, 0x3B, 0x3C, 0x27, 0x35, 0x36,
                                            0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0x23, 0x26, 0xB1, 0xB2};
    const int fams[PCR_PROG_ID_COUNT] = {0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7};
    for (int i = 0; i < PCR_PROG_ID_COUNT; i++) {
        RT_CHECK_EQ((int)pc_rando_progressive_id(i), (int)ids[i]);
        RT_CHECK_EQ(pc_rando_progressive_family(ids[i]), fams[i]);
    }
    RT_CHECK_EQ((int)pc_rando_progressive_id(PCR_PROG_ID_COUNT), 0xFF);
    RT_CHECK_EQ((int)pc_rando_progressive_id(-1), 0xFF);
    int nprog = 0;
    for (int id = 0; id < 0x100; id++) {
        nprog += pc_rando_progressive_family((uint8_t)id) >= 0;
    }
    RT_CHECK_EQ(nprog, PCR_PROG_ID_COUNT);

    // Tier sequences from an empty save to a full one: each give of any id of the family converts
    // to the next tier, and the item function gives that tier.
    const uint8_t seq[PCR_PROG_COUNT][4] = {
        {0x38, 0x39, 0x3A, 0x3E}, {0x3B, 0x3C}, {0x27, 0x35, 0x36}, {0xAB, 0xAC},
        {0xAD, 0xAE},             {0xAF, 0xB0}, {0x23, 0x26},       {0xB1, 0xB2},
    };
    const int nseq[PCR_PROG_COUNT] = {4, 2, 3, 2, 2, 2, 2, 2};
    for (int f = 0; f < PCR_PROG_COUNT; f++) {
        RT_CHECK_EQ(pc_rando_progressive_tier_count(f), nseq[f]);
        PcRandoProgState st = empty();
        for (int t = 0; t < nseq[f]; t++) {
            RT_CHECK_EQ((int)pc_rando_progressive_tier_id(f, t), (int)seq[f][t]);
            for (int i = 0; i < PCR_PROG_ID_COUNT; i++) {
                if (fams[i] == f) {
                    RT_CHECK_EQ((int)pc_rando_progressive_convert(ids[i], &st), (int)seq[f][t]);
                }
            }
            RT_CHECK_EQ(pc_rando_progressive_tier(f, &st), t);
            give(f, t, &st);
        }
        // Full: the conversion falls back to the base id, the item function gives nothing.
        RT_CHECK_EQ((int)pc_rando_progressive_convert(seq[f][0], &st), (int)seq[f][0]);
        RT_CHECK_EQ(pc_rando_progressive_tier(f, &st), -1);
    }
    RT_CHECK_EQ(pc_rando_progressive_tier_count(PCR_PROG_COUNT), 0);
    RT_CHECK_EQ((int)pc_rando_progressive_tier_id(0, 4), 0xFF);
    RT_CHECK_EQ(pc_rando_progressive_tier(-1, nullptr), -1);

    // Unreachable states (asm "invalid state"): base id, no tier.
    PcRandoProgState st = empty();
    st.swords = 2; // Master Sword without the Hero's Sword
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0x3E, &st), 0x38);
    RT_CHECK_EQ(pc_rando_progressive_tier(PCR_PROG_SWORD, &st), -1);
    st = empty();
    st.bomb_max = 0; // no 30-bomb start (a vanilla-started save)
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0xAE, &st), 0xAD);
    RT_CHECK_EQ(pc_rando_progressive_tier(PCR_PROG_BOMB_BAG, &st), -1);
    st = empty();
    st.max_magic = 8;
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0xB2, &st), 0xB1);

    // Families read only their own state.
    st = empty();
    st.swords = 7;
    st.max_magic = 16;
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0x38, &st), 0x3E);
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0x3B, &st), 0x3B);
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0xB1, &st), 0xB2);
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0xAF, &st), 0xAF);

    // Other ids pass through, whatever the state.
    const uint8_t others[] = {0x00, 0x01, 0x22, 0x24, 0x31, 0x3F, 0x50, 0xAA, 0xB3, 0xFF};
    for (uint8_t id : others) {
        RT_CHECK_EQ((int)pc_rando_progressive_convert(id, &st), (int)id);
    }
    RT_CHECK_EQ((int)pc_rando_progressive_convert(0x38, nullptr), 0x38);

    // Message 101+0xB1.
    RT_CHECK_EQ(PCR_PROG_MAGIC_MSG_NO, 278);
    RT_CHECK_EQ(pc_rando_progressive_add_messages(), PCR_MSG_OK);
    const PcRandoMsg* m = pc_rando_msg_find(PCR_PROG_MAGIC_MSG_NO);
    RT_CHECK(m != nullptr);
    if (m != nullptr) {
        RT_CHECK(m->has_entry);
        RT_CHECK_EQ((int)m->entry.text_box_type, 9);
        RT_CHECK_EQ((int)m->entry.initial_draw_type, 2);
        RT_CHECK_EQ((int)m->entry.display_item_id, 0xB1);
        char text[256];
        pc_rando_msg_decode(m->bytes, m->len, text, sizeof text);
        RT_CHECK_EQ(std::strcmp(text, PCR_PROG_MAGIC_MSG_TEXT), 0);
        // Encoded as wwrando's write_string: tag bytes, then the plain text.
        static const uint8_t head[] = {0x1A, 0x05, 0x00, 0x00, 0x01, 'Y', 'o', 'u'};
        RT_CHECK(m->len > sizeof head && memcmp(m->bytes, head, sizeof head) == 0);
    }
    return rt::finish();
}
