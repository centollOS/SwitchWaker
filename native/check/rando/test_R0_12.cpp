// R0.12 (docs/RANDOMIZER_PLAN.md): the message overlay and its text encoding (pc_rando_msg.h).
//  1. pc_rando_msg_encode gives gclib's Message.write_string bytes and pc_rando_msg_decode its
//     read_string text, for every wwrando string with a control tag (fixture R0_12_bmg_strings.txt,
//     native/tools/rando/make_msg_fixture.py);
//  2. the encoder's refusals (code points, Shift-JIS lead bytes, braces, tag sizes, space);
//  3. tag builders, INF1 entry pack/unpack and gclib's new-message defaults;
//  4. the overlay (add, set_text, set_entry, set_bytes, remove) and the two hooks the game calls;
//  5. the overlay is cleared when pc_rando() turns 0.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_msg.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

std::string unhex(const std::string& h) {
    std::string out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char)std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}

std::string hex(const uint8_t* p, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s.push_back(d[p[i] >> 4]);
        s.push_back(d[p[i] & 15]);
    }
    return s;
}

int encodeRc(const char* s) {
    uint8_t buf[256];
    size_t n = 0;
    return pc_rando_msg_encode(s, buf, sizeof buf, &n);
}

void fixtureStrings() {
    std::string text;
    if (!rt::readFixture("R0_12_bmg_strings.txt", text)) {
        return;
    }
    int lines = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) {
            eol = text.size();
        }
        const std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t t1 = line.find('\t');
        const size_t t2 = line.find('\t', t1 + 1);
        if (!RT_CHECK(t1 != std::string::npos && t2 != std::string::npos)) {
            continue;
        }
        const std::string in = unhex(line.substr(0, t1));
        const std::string want = line.substr(t1 + 1, t2 - t1 - 1);
        const std::string back = unhex(line.substr(t2 + 1));
        std::vector<uint8_t> buf(in.size() * 4 + 16);
        size_t n = 0;
        const int rc = pc_rando_msg_encode(in.c_str(), buf.data(), buf.size(), &n);
        if (!RT_CHECK_EQ(rc, PCR_MSG_OK)) {
            std::fprintf(stderr, "  input: %s\n", in.c_str());
            continue;
        }
        if (!RT_CHECK_EQ(hex(buf.data(), n), want)) {
            std::fprintf(stderr, "  input: %s\n", in.c_str());
        }
        size_t sized = 0;
        RT_CHECK_EQ(pc_rando_msg_encode(in.c_str(), nullptr, 0, &sized), PCR_MSG_ERR_SPACE);
        RT_CHECK_EQ(sized, n);
        std::vector<char> dec(back.size() * 2 + 16);
        const size_t dl = pc_rando_msg_decode(buf.data(), n, dec.data(), dec.size());
        RT_CHECK_EQ(std::string(dec.data(), dl), back);
        lines++;
    }
    RT_CHECK(lines >= 50);
}

void refusals() {
    RT_CHECK_EQ(encodeRc("\xC3"), PCR_MSG_ERR_UTF8);           // truncated sequence
    RT_CHECK_EQ(encodeRc("\xC0\x80"), PCR_MSG_ERR_UTF8);       // overlong
    RT_CHECK_EQ(encodeRc("\xE2\x82\xAC"), PCR_MSG_ERR_CHAR);   // U+20AC, above 0xFF
    RT_CHECK_EQ(encodeRc("\xC2\x85"), PCR_MSG_ERR_CHAR);       // U+0085: a Shift-JIS lead byte
    RT_CHECK_EQ(encodeRc("\xC2\x9F"), PCR_MSG_ERR_CHAR);
    RT_CHECK_EQ(encodeRc("\xC2\xA0ok"), PCR_MSG_OK);           // 0xA0 is a plain byte
    RT_CHECK_EQ(encodeRc("\\{1A 05 00 00 01"), PCR_MSG_ERR_BRACE);
    RT_CHECK_EQ(encodeRc("\\{1A0 05}"), PCR_MSG_ERR_BRACE);    // 0x1A0 > 0xFF
    RT_CHECK_EQ(encodeRc("\\{1A 06 00 00 01}"), PCR_MSG_ERR_TAG_SIZE);
    RT_CHECK_EQ(encodeRc("\\{1A}"), PCR_MSG_ERR_TAG_SIZE);
    RT_CHECK_EQ(encodeRc("\\{41 42}"), PCR_MSG_OK);            // raw bytes, not a tag
    RT_CHECK_EQ(encodeRc("{41} before any backslash"), PCR_MSG_OK);
    uint8_t small[4];
    size_t n = 0;
    RT_CHECK_EQ(pc_rando_msg_encode("abcd", small, sizeof small, &n), PCR_MSG_ERR_SPACE);
    RT_CHECK_EQ(n, (size_t)5);
    // tag bytes may hold NULs; decode follows the tag size, not the NUL
    uint8_t enc[32];
    RT_CHECK_EQ(pc_rando_msg_encode("A\\{1A 05 00 00 00}B", enc, sizeof enc, &n), PCR_MSG_OK);
    RT_CHECK_EQ(hex(enc, n), std::string("411a0500000042") + "00");
    char dec[64];
    pc_rando_msg_decode(enc, n, dec, sizeof dec);
    RT_CHECK_EQ(std::string(dec), std::string("A\\{1A 05 00 00 00}B"));
    // a cut decode still terminates and reports the full length
    RT_CHECK_EQ(pc_rando_msg_decode(enc, n, dec, 4), std::strlen("A\\{1A 05 00 00 00}B"));
    RT_CHECK_EQ(std::string(dec), std::string("A\\{"));
}

void tags() {
    char s[64];
    RT_CHECK(pc_rando_msg_wait_str(s, sizeof s, 30) > 0);
    RT_CHECK_EQ(std::string(s), std::string("\\{1A 07 00 00 07 00 1E}"));
    RT_CHECK(pc_rando_msg_color_str(s, sizeof s, 1) > 0);
    RT_CHECK_EQ(std::string(s), std::string(PCR_MSG_COLOR_RED));
    RT_CHECK_EQ(std::string(PCR_MSG_COLOR(05)), std::string("\\{1A 06 FF 00 00 05}"));
    RT_CHECK_EQ(encodeRc(PCR_MSG_PLAYER_NAME PCR_MSG_INSTANT PCR_MSG_SELECT_TWO PCR_MSG_SELECT_THREE
                         PCR_MSG_COLOR_DEFAULT),
                PCR_MSG_OK);
    RT_CHECK_EQ(pc_rando_msg_wait_str(s, 10, 30), (size_t)0); // does not fit
    uint8_t b[16];
    const uint8_t scale[2] = {0x00, 0x96};
    RT_CHECK_EQ(pc_rando_msg_tag(b, sizeof b, PCR_MSG_GROUP_STYLE, 1, scale, 2), (size_t)7);
    RT_CHECK_EQ(hex(b, 7), std::string("1a07ff00010096"));
    RT_CHECK_EQ(pc_rando_msg_tag(b, 6, PCR_MSG_GROUP_STYLE, 1, scale, 2), (size_t)0);
}

void entries() {
    PcRandoMsgEntry d;
    pc_rando_msg_entry_defaults(&d);
    uint8_t raw[PCR_MSG_ENTRY_SIZE];
    pc_rando_msg_entry_pack(&d, 849, 0, raw);
    // gclib Message(): offset 0, id 849, price 0, next 0, unknown_1 0x60, DIALOG, draw 0, pos 3,
    // item 0xFF, align 0, sound 0, camera 0, anim 0, unknown_3 0, 4 lines, unknown_4 0
    RT_CHECK_EQ(hex(raw, sizeof raw), std::string("0000000003510000000000600000") + "03ff" +
                                          "00000000" + "00000400");
    PcRandoMsgEntry e;
    pc_rando_msg_entry_defaults(&e);
    e.item_price = 0x1234;
    e.next_message_id = 3907;
    e.text_box_type = 9;
    e.initial_draw_type = 2;
    e.display_item_id = 0xB1;
    e.text_alignment = 4;
    e.initial_sound = 0x11;
    e.initial_camera_behavior = 0x22;
    e.initial_speaker_anim = 0x33;
    e.unknown_3 = 0x44;
    e.num_lines_per_box = 0x0102;
    e.unknown_4 = 0x55;
    pc_rando_msg_entry_pack(&e, 0x0F42, 0xA0B0C0D0, raw);
    RT_CHECK_EQ(hex(raw, sizeof raw), std::string("a0b0c0d00f4212340f430060090203b1041122334401") + "0255");
    PcRandoMsgEntry u;
    std::memset(&u, 0xEE, sizeof u);
    pc_rando_msg_entry_unpack(raw, &u);
    uint8_t again[PCR_MSG_ENTRY_SIZE];
    pc_rando_msg_entry_pack(&u, 0x0F42, 0xA0B0C0D0, again);
    RT_CHECK(std::memcmp(raw, again, sizeof raw) == 0);
    RT_CHECK_EQ(u.num_lines_per_box, 0x0102);
    RT_CHECK_EQ(u.unknown_4, 0x55);
}

void overlay() {
    pc_rando_msg_clear();
    RT_CHECK_EQ(pc_rando_msg_count(), 0);
    RT_CHECK(pc_rando_msg_hook_text(3906) == nullptr);

    // text only, for a vanilla message: the BMG entry stays
    RT_CHECK_EQ(pc_rando_msg_set_text(3906, "New " PCR_MSG_COLOR_RED "shop" PCR_MSG_COLOR_DEFAULT " text"),
                PCR_MSG_OK);
    const char* t = pc_rando_msg_hook_text(3906);
    RT_CHECK(t != nullptr && std::strncmp(t, "New \x1A\x06\xFF", 7) == 0);
    PcRandoMsgEntry bmg;
    pc_rando_msg_entry_defaults(&bmg);
    bmg.text_box_type = 2;
    uint8_t raw[PCR_MSG_ENTRY_SIZE];
    uint8_t before[PCR_MSG_ENTRY_SIZE];
    pc_rando_msg_entry_pack(&bmg, 3906, 0x1234, raw);
    std::memcpy(before, raw, sizeof raw);
    pc_rando_msg_hook_entry(3906, raw);
    RT_CHECK(std::memcmp(raw, before, sizeof raw) == 0);
    // ...and for a number the BMG lacks (the entry is another message's): the new-message defaults
    pc_rando_msg_entry_pack(&bmg, 9999, 0x1234, raw);
    RT_CHECK_EQ(pc_rando_msg_set_text(9998, "x"), PCR_MSG_OK);
    pc_rando_msg_hook_entry(9998, raw);
    PcRandoMsgEntry got;
    pc_rando_msg_entry_unpack(raw, &got);
    RT_CHECK_EQ(raw[4] * 256 + raw[5], 9998);
    RT_CHECK_EQ(got.text_box_type, 0);
    RT_CHECK_EQ(got.num_lines_per_box, 4);

    // a new message with entry fields (as wwrando's 849, Aryll's text)
    PcRandoMsgEntry e;
    pc_rando_msg_entry_defaults(&e);
    e.text_alignment = 4;
    RT_CHECK_EQ(pc_rando_msg_add(849, &e, "Oh! Did you get stuck in there, Big Brother?"), PCR_MSG_OK);
    RT_CHECK_EQ(std::string(pc_rando_msg_hook_text(849)), std::string("Oh! Did you get stuck in there, Big Brother?"));
    pc_rando_msg_entry_pack(&bmg, 65000, 0x40, raw); // the BMG's last entry
    pc_rando_msg_hook_entry(849, raw);
    pc_rando_msg_entry_unpack(raw, &got);
    RT_CHECK_EQ(raw[4] * 256 + raw[5], 849);
    RT_CHECK_EQ(got.text_alignment, 4);
    RT_CHECK_EQ(got.text_box_type, 0);
    RT_CHECK_EQ(raw[0] * 0x1000000 + raw[1] * 0x10000 + raw[2] * 0x100 + raw[3], 0x40); // offset kept

    // a failed encode changes nothing
    RT_CHECK_EQ(pc_rando_msg_add(849, &e, "\\{1A 09}"), PCR_MSG_ERR_TAG_SIZE);
    RT_CHECK(pc_rando_msg_hook_text(849)[0] == 'O');

    // entry only: the BMG text stays
    PcRandoMsgEntry f;
    pc_rando_msg_entry_defaults(&f);
    f.initial_draw_type = 1;
    RT_CHECK_EQ(pc_rando_msg_set_entry(463, &f), PCR_MSG_OK);
    RT_CHECK(pc_rando_msg_hook_text(463) == nullptr);
    pc_rando_msg_entry_pack(&bmg, 463, 0x80, raw);
    pc_rando_msg_hook_entry(463, raw);
    RT_CHECK_EQ(raw[0x0D], 1);
    RT_CHECK_EQ(raw[0x0C], 0);
    // ...then text too, entry kept
    RT_CHECK_EQ(pc_rando_msg_set_text(463, "Swift Sail"), PCR_MSG_OK);
    RT_CHECK(pc_rando_msg_find(463)->has_entry == 1);
    RT_CHECK_EQ(pc_rando_msg_find(463)->len, (size_t)11);

    // raw bytes, with and without the NUL
    const uint8_t bytes[3] = {'h', 'i', '!'};
    RT_CHECK_EQ(pc_rando_msg_set_bytes(5, bytes, 3), PCR_MSG_OK);
    RT_CHECK_EQ(std::string(pc_rando_msg_hook_text(5)), std::string("hi!"));
    RT_CHECK_EQ(pc_rando_msg_find(5)->len, (size_t)4);

    // many records stay sorted and findable
    for (int i = 0; i < 300; i++) {
        char s[16];
        std::snprintf(s, sizeof s, "m%d", (i * 7919) % 300 + 20000);
        RT_CHECK_EQ(pc_rando_msg_set_text((uint16_t)((i * 7919) % 300 + 20000), s), PCR_MSG_OK);
    }
    RT_CHECK_EQ(pc_rando_msg_count(), 5 + 300);
    RT_CHECK_EQ(std::string(pc_rando_msg_hook_text(20123)), std::string("m20123"));
    RT_CHECK_EQ(pc_rando_msg_remove(20123), 1);
    RT_CHECK_EQ(pc_rando_msg_remove(20123), 0);
    RT_CHECK(pc_rando_msg_hook_text(20123) == nullptr);
    RT_CHECK_EQ(std::string(pc_rando_msg_hook_text(20124)), std::string("m20124"));
}

void clearedOnDeactivate() {
    // Run without COS_RANDO_FORCE: deactivation clears the overlay.
    if (pc_rando_forced()) {
        return;
    }
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK(pc_rando_msg_count() > 0); // activation keeps what was built before it
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando_msg_count(), 0);
    RT_CHECK(pc_rando_msg_hook_text(849) == nullptr);
}

} // namespace

int main() {
    fixtureStrings();
    refusals();
    tags();
    entries();
    overlay();
    clearedOnDeactivate();
    return rt::finish();
}
