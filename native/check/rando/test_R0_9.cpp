// cos_rando_test_R0_9 (docs/RANDOMIZER_PLAN.md step R0.9): the dzs/dzr patch library
// (features/rando/pc_rando_dzx.h) on synthetic files built here (no disc bytes):
//   - header parsing, entry sizes, layer tags, lookups;
//   - set_field / scalar setters, big-endian, with fingerprints (name and vanilla bytes) that refuse a
//     mismatch without writing;
//   - suppress (empty name, indexes unchanged; refused on non-actor chunks);
//   - grow: appends at the end of an existing chunk (vanilla indexes and every other byte unchanged),
//     a new chunk (data shifted, chunk rows and RTBL pointers fixed), several adds, bad input;
//   - the fixture R0_9_wwrando_grow.txt, written by wwrando's own DZx (fixtures/R0_9_make_fixture.py):
//     the same adds through wwrando and through this library give the same entries chunk by chunk.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_dzx.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void put32(Bytes& b, size_t at, uint32_t v) {
    b[at] = (uint8_t)(v >> 24);
    b[at + 1] = (uint8_t)(v >> 16);
    b[at + 2] = (uint8_t)(v >> 8);
    b[at + 3] = (uint8_t)v;
}

struct Chunk {
    const char* tag;
    Bytes data; // entries (and, for RTBL, the sub-tables after them: offsets patched by build)
    uint32_t count;
};

// Writes {count, rows, data aligned to 4 per chunk}, like wwrando's DZx.save_changes.
Bytes build(std::vector<Chunk>& chunks, std::vector<size_t>* offs = nullptr) {
    Bytes b(4 + chunks.size() * 12, 0);
    put32(b, 0, (uint32_t)chunks.size());
    for (size_t i = 0; i < chunks.size(); i++) {
        while (b.size() % 4) {
            b.push_back(0xFF);
        }
        const size_t off = b.size();
        if (offs) {
            offs->push_back(off);
        }
        memcpy(&b[4 + i * 12], chunks[i].tag, 4);
        put32(b, 4 + i * 12 + 4, chunks[i].count);
        put32(b, 4 + i * 12 + 8, chunks[i].count ? (uint32_t)off : 0);
        b.insert(b.end(), chunks[i].data.begin(), chunks[i].data.end());
    }
    while (b.size() % 0x20) {
        b.push_back(0xFF);
    }
    return b;
}

Bytes actor(const char* name, uint32_t params, uint8_t fill) {
    Bytes e(0x20, fill);
    memset(e.data(), 0, 8);
    memcpy(e.data(), name, strnlen(name, 8));
    e[8] = (uint8_t)(params >> 24);
    e[9] = (uint8_t)(params >> 16);
    e[10] = (uint8_t)(params >> 8);
    e[11] = (uint8_t)params;
    return e;
}

Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}

// A room-like file: ACTR x2, TRES x1, an RTBL whose 2 entries share one sub-table, LBNK (1-byte
// entries, so the next chunk is padded), SCLS x1, ACT3 x1.
struct Sample {
    Bytes file;
    size_t rtblOff = 0;
};

Sample makeSample() {
    std::vector<Chunk> c;
    c.push_back({"ACTR", cat({actor("Kanban", 0x11223344, 0xA0), actor("Bitem", 0x55667788, 0xA1)}), 2});
    Bytes tres = actor("takara", 0x00FF0000, 0xB0);
    tres[0x1C] = 0x05; // z_rot high byte: the chest item (locations.md)
    c.push_back({"TRES", tres, 1});
    c.push_back({"RTBL", Bytes(8 + 8 + 2, 0), 2}); // 2 entries, 1 shared sub-table, 2 rooms
    c.push_back({"LBNK", Bytes{0x07}, 1});
    Bytes scls(0xC, 0x5C);
    c.push_back({"SCLS", scls, 1});
    c.push_back({"ACT3", actor("Pirates", 0xCAFEBABE, 0xC3), 1});
    std::vector<size_t> offs;
    Sample s;
    s.file = build(c, &offs);
    // RTBL: entries -> sub at +8; sub = {num 2, reverb, time, unk, rooms -> +16}; rooms {0x81, 0x02}
    const size_t r = offs[2];
    s.rtblOff = r;
    put32(s.file, r, (uint32_t)(r + 8));
    put32(s.file, r + 4, (uint32_t)(r + 8));
    s.file[r + 8] = 2;
    s.file[r + 9] = 0x10;
    s.file[r + 10] = 1;
    s.file[r + 11] = 0;
    put32(s.file, r + 12, (uint32_t)(r + 16));
    s.file[r + 16] = 0x81;
    s.file[r + 17] = 0x02;
    return s;
}

std::string tagStr(const uint8_t* p) {
    return std::string((const char*)p, 4);
}

// Entries of every chunk as (tag, bytes), for comparing files with different layouts.
bool chunkEntries(const Bytes& f, const char* tag, Bytes& out) {
    PcRandoDzx d;
    Bytes copy = f;
    if (pc_rando_dzx_open(&d, copy.data(), copy.size()) != PCR_DZX_OK) {
        return false;
    }
    const uint32_t n = pc_rando_dzx_count(&d, tag);
    const uint32_t es = pc_rando_dzx_entry_size(tag);
    out.clear();
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(&d, tag, i);
        if (e == nullptr) {
            return false;
        }
        out.insert(out.end(), e, e + es);
    }
    return true;
}

Bytes hexBytes(const std::string& s) {
    Bytes b;
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        b.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return b;
}

void basics() {
    char t[5];
    RT_CHECK(pc_rando_dzx_tag("ACTR", -1, t) && strcmp(t, "ACTR") == 0);
    RT_CHECK(pc_rando_dzx_tag("ACTR", 3, t) && strcmp(t, "ACT3") == 0);
    RT_CHECK(pc_rando_dzx_tag("TRES", 0, t) && strcmp(t, "TRE0") == 0);
    RT_CHECK(pc_rando_dzx_tag("SCOB", 11, t) && strcmp(t, "SCOb") == 0);
    RT_CHECK_EQ(pc_rando_dzx_tag("SCOB", 12, t), 0);
    RT_CHECK_EQ(pc_rando_dzx_tag("PLYR", 1, t), 0);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("ACTR"), 0x20u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("ACTa"), 0x20u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("TRE5"), 0x20u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("SCO1"), 0x24u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("SCOB"), 0x24u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("PLYR"), 0x20u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("SCLS"), 0xCu);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("2DMA"), 0x38u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("EVNT"), 0x18u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("TGDR"), 0x24u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("LBNK"), 1u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("ZZZZ"), 0u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size("ACT"), 0u);
    RT_CHECK_EQ(pc_rando_dzx_entry_size(nullptr), 0u);

    uint8_t b[] = {0x12, 0x34, 0x56, 0x78, 0x3F, 0x80, 0x00, 0x00};
    RT_CHECK_EQ(pc_rando_dzx_be16(b), 0x1234);
    RT_CHECK_EQ(pc_rando_dzx_be32(b), 0x12345678u);
    RT_CHECK_EQ(pc_rando_dzx_bef32(b + 4), 1.0f);
    RT_CHECK(strcmp(pc_rando_dzx_result_name(PCR_DZX_MISMATCH), "fingerprint mismatch") == 0);
}

void parsing() {
    Sample s = makeSample();
    PcRandoDzx d;
    RT_CHECK_EQ(pc_rando_dzx_open(&d, s.file.data(), s.file.size()), PCR_DZX_OK);
    RT_CHECK_EQ(d.chunkCount, 6u);
    RT_CHECK_EQ(pc_rando_dzx_find(&d, "ACTR"), 0);
    RT_CHECK_EQ(pc_rando_dzx_find(&d, "ACT3"), 5);
    RT_CHECK_EQ(pc_rando_dzx_find(&d, "SCOB"), -1);
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "ACTR"), 2u);
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "SCOB"), 0u);
    const uint8_t* e = pc_rando_dzx_entry(&d, "ACTR", 1);
    RT_CHECK(e != nullptr && memcmp(e, "Bitem\0\0\0", 8) == 0);
    RT_CHECK_EQ(pc_rando_dzx_be32(e + 8), 0x55667788u);
    RT_CHECK(pc_rando_dzx_entry(&d, "ACTR", 2) == nullptr);
    RT_CHECK(pc_rando_dzx_entry(&d, "SCOB", 0) == nullptr);

    // Bad files: too short, a count beyond the buffer, a chunk past the end.
    RT_CHECK_EQ(pc_rando_dzx_open(&d, s.file.data(), 3), PCR_DZX_BAD_FILE);
    RT_CHECK_EQ(pc_rando_dzx_open(&d, nullptr, 64), PCR_DZX_BAD_FILE);
    Bytes bad = s.file;
    put32(bad, 0, 1000);
    RT_CHECK_EQ(pc_rando_dzx_open(&d, bad.data(), bad.size()), PCR_DZX_BAD_FILE);
    bad = s.file;
    put32(bad, 4 + 4, 100); // ACTR count 100 * 0x20 > file
    RT_CHECK_EQ(pc_rando_dzx_open(&d, bad.data(), bad.size()), PCR_DZX_BAD_FILE);
    bad = s.file;
    put32(bad, 4 + 8, (uint32_t)bad.size() + 4);
    RT_CHECK_EQ(pc_rando_dzx_open(&d, bad.data(), bad.size()), PCR_DZX_BAD_FILE);
    // An unknown chunk is fine (its size is not checked).
    bad = s.file;
    memcpy(&bad[4], "ZZZZ", 4);
    RT_CHECK_EQ(pc_rando_dzx_open(&d, bad.data(), bad.size()), PCR_DZX_OK);
    RT_CHECK(pc_rando_dzx_entry(&d, "ZZZZ", 0) == nullptr);
}

void edits() {
    Sample s = makeSample();
    const Bytes orig = s.file;
    PcRandoDzx d;
    RT_CHECK_EQ(pc_rando_dzx_open(&d, s.file.data(), s.file.size()), PCR_DZX_OK);

    // Chest item (TRES +0x1C) with the D9 fingerprint {name, vanilla item}.
    const uint8_t vanilla = 0x05;
    PcRandoDzxFingerprint fp = {"takara", 0x1C, &vanilla, 1};
    RT_CHECK_EQ(pc_rando_dzx_check(&d, "TRES", 0, &fp), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_set_u8(&d, "TRES", 0, 0x1C, 0x31, &fp), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_entry(&d, "TRES", 0)[0x1C], 0x31);
    // Applying it again: the vanilla byte is gone, so it is refused and nothing changes.
    RT_CHECK_EQ(pc_rando_dzx_set_u8(&d, "TRES", 0, 0x1C, 0x77, &fp), PCR_DZX_MISMATCH);
    RT_CHECK_EQ(pc_rando_dzx_entry(&d, "TRES", 0)[0x1C], 0x31);
    // Wrong name (replaced content files, HD #42): refused, untouched.
    PcRandoDzxFingerprint wrongName = {"Kanban", -1, nullptr, 0};
    Bytes before = s.file;
    RT_CHECK_EQ(pc_rando_dzx_set_u32(&d, "TRES", 0, 8, 0xDEADBEEF, &wrongName), PCR_DZX_MISMATCH);
    RT_CHECK(s.file == before);
    // Name only; 8-character names compare fully.
    PcRandoDzxFingerprint pirates = {"Pirates", -1, nullptr, 0};
    RT_CHECK_EQ(pc_rando_dzx_set_u32(&d, "ACT3", 0, 8, 0x01020304, &pirates), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_be32(pc_rando_dzx_entry(&d, "ACT3", 0) + 8), 0x01020304u);
    PcRandoDzxFingerprint prefix = {"Pirate", -1, nullptr, 0};
    RT_CHECK_EQ(pc_rando_dzx_check(&d, "ACT3", 0, &prefix), PCR_DZX_MISMATCH);
    // Fingerprint bytes out of the entry.
    PcRandoDzxFingerprint far = {nullptr, 0x1F, &vanilla, 2};
    RT_CHECK_EQ(pc_rando_dzx_check(&d, "TRES", 0, &far), PCR_DZX_BAD_RANGE);

    // Big-endian scalars.
    RT_CHECK_EQ(pc_rando_dzx_set_u16(&d, "ACTR", 1, 0x18, 0xABCD, nullptr), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_set_f32(&d, "ACTR", 1, 0x0C, -500000.0f, nullptr), PCR_DZX_OK);
    const uint8_t* a1 = pc_rando_dzx_entry(&d, "ACTR", 1);
    RT_CHECK_EQ(a1[0x18], 0xAB);
    RT_CHECK_EQ(a1[0x19], 0xCD);
    RT_CHECK_EQ(a1[0x0C], 0xC8); // -500000.0f = 0xC8F42400
    RT_CHECK_EQ(pc_rando_dzx_be32(a1 + 0x0C), 0xC8F42400u);
    RT_CHECK_EQ(pc_rando_dzx_bef32(a1 + 0x0C), -500000.0f);

    // Range and lookup errors write nothing.
    before = s.file;
    RT_CHECK_EQ(pc_rando_dzx_set_u32(&d, "ACTR", 0, 0x1E, 1, nullptr), PCR_DZX_BAD_RANGE);
    RT_CHECK_EQ(pc_rando_dzx_set_u8(&d, "ACTR", 2, 0, 1, nullptr), PCR_DZX_BAD_INDEX);
    RT_CHECK_EQ(pc_rando_dzx_set_u8(&d, "SCOB", 0, 0, 1, nullptr), PCR_DZX_NO_CHUNK);
    RT_CHECK_EQ(pc_rando_dzx_set_u8(&d, "ZZZ", 0, 0, 1, nullptr), PCR_DZX_BAD_FILE);
    RT_CHECK(s.file == before);

    // Suppress: name cleared, the rest and the other entries unchanged, indexes stable.
    PcRandoDzxFingerprint kanban = {"Kanban", -1, nullptr, 0};
    RT_CHECK_EQ(pc_rando_dzx_suppress(&d, "ACTR", 0, &kanban), PCR_DZX_OK);
    const uint8_t* a0 = pc_rando_dzx_entry(&d, "ACTR", 0);
    static const uint8_t kZero[8] = {};
    RT_CHECK(memcmp(a0, kZero, 8) == 0);
    RT_CHECK_EQ(pc_rando_dzx_be32(a0 + 8), 0x11223344u);
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "ACTR"), 2u);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "ACTR", 1), "Bitem", 5) == 0);
    RT_CHECK_EQ(pc_rando_dzx_suppress(&d, "ACTR", 0, &kanban), PCR_DZX_MISMATCH); // already gone
    RT_CHECK_EQ(pc_rando_dzx_suppress(&d, "SCLS", 0, nullptr), PCR_DZX_UNKNOWN_SIZE);
    RT_CHECK_EQ(pc_rando_dzx_suppress(&d, "ACT3", 0, nullptr), PCR_DZX_OK);

    // Only the touched bytes differ from the original.
    size_t diff = 0;
    for (size_t i = 0; i < orig.size(); i++) {
        diff += orig[i] != s.file[i];
    }
    RT_CHECK(diff > 0 && diff <= 1 + 4 + 2 + 4 + 6 + 7);
}

void growExisting() {
    Sample s = makeSample();
    const Bytes newActor = actor("Ls1", 0x000000FF, 0xD0);
    const Bytes twoTres = cat({actor("takara3", 0x00200011, 0xE0), actor("takara3", 0x00200012, 0xE1)});
    const Bytes moreActor = actor("SwOp", 0x12345678, 0xD1);
    PcRandoDzxAdd adds[] = {{"ACTR", newActor.data(), 1}, {"TRES", twoTres.data(), 2},
                            {"ACTR", moreActor.data(), 1}};
    int res = -1;
    const size_t need = pc_rando_dzx_grown_size(s.file.data(), s.file.size(), adds, 3, &res);
    RT_CHECK_EQ(res, PCR_DZX_OK);
    RT_CHECK(need > s.file.size() && need % 0x20 == 0);
    Bytes out(need + 64, 0xEE);
    size_t outSize = 0;
    uint32_t first[3] = {99, 99, 99};
    RT_CHECK_EQ(pc_rando_dzx_grow(s.file.data(), s.file.size(), adds, 3, out.data(), need - 1, &outSize, first),
                PCR_DZX_NO_SPACE);
    RT_CHECK_EQ(pc_rando_dzx_grow(s.file.data(), s.file.size(), adds, 3, out.data(), out.size(), &outSize, first),
                PCR_DZX_OK);
    RT_CHECK_EQ(outSize, need);
    RT_CHECK_EQ(first[0], 2u);
    RT_CHECK_EQ(first[1], 1u);
    RT_CHECK_EQ(first[2], 3u);
    RT_CHECK_EQ(out[need], 0xEE); // nothing past the size
    out.resize(outSize);

    // Every byte of the old file stays where it was, except the two grown rows' count and offset.
    const size_t actrRow = 4, tresRow = 4 + 12;
    for (size_t i = 0; i < s.file.size(); i++) {
        const bool row = (i >= actrRow + 4 && i < actrRow + 12) || (i >= tresRow + 4 && i < tresRow + 12);
        if (!row && out[i] != s.file[i]) {
            RT_CHECK_EQ((int)i, -1);
            break;
        }
    }
    PcRandoDzx d;
    RT_CHECK_EQ(pc_rando_dzx_open(&d, out.data(), out.size()), PCR_DZX_OK);
    RT_CHECK_EQ(d.chunkCount, 6u);
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "ACTR"), 4u);
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "TRES"), 3u);
    // Vanilla indexes still hold the vanilla entries; new ones follow in add order.
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "ACTR", 0), "Kanban", 6) == 0);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "ACTR", 1), "Bitem", 5) == 0);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "ACTR", 2), newActor.data(), 0x20) == 0);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "ACTR", 3), moreActor.data(), 0x20) == 0);
    RT_CHECK_EQ(pc_rando_dzx_entry(&d, "TRES", 0)[0x1C], 0x05);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "TRES", 1), twoTres.data(), 0x40) == 0);
    for (const char* t : {"ACTR", "TRES"}) {
        RT_CHECK_EQ(pc_rando_dzx_be32(out.data() + 4 + 12 * pc_rando_dzx_find(&d, t) + 8) % 4, 0u);
    }
    // In-place edits work on the grown copy (a later patcher's field edit on an added entity).
    RT_CHECK_EQ(pc_rando_dzx_set_u8(&d, "TRES", 2, 0x1C, 0x22, nullptr), PCR_DZX_OK);
    // The RTBL pointers did not need to move.
    RT_CHECK_EQ(pc_rando_dzx_be32(out.data() + s.rtblOff), (uint32_t)(s.rtblOff + 8));

    // Errors: RTBL never grows, unknown tags, bad input.
    PcRandoDzxAdd rtbl = {"RTBL", newActor.data(), 1};
    RT_CHECK_EQ(pc_rando_dzx_grow(s.file.data(), s.file.size(), &rtbl, 1, out.data(), out.size(), nullptr,
                                  nullptr),
                PCR_DZX_UNKNOWN_SIZE);
    PcRandoDzxAdd unk = {"QQQQ", newActor.data(), 1};
    RT_CHECK_EQ(pc_rando_dzx_grown_size(s.file.data(), s.file.size(), &unk, 1, &res), 0u);
    RT_CHECK_EQ(res, PCR_DZX_UNKNOWN_SIZE);
    PcRandoDzxAdd noData = {"ACTR", nullptr, 1};
    RT_CHECK_EQ(pc_rando_dzx_grown_size(s.file.data(), s.file.size(), &noData, 1, &res), 0u);
    RT_CHECK_EQ(res, PCR_DZX_BAD_RANGE);
    RT_CHECK_EQ(pc_rando_dzx_grown_size(s.file.data(), 2, adds, 1, &res), 0u);
    RT_CHECK_EQ(res, PCR_DZX_BAD_FILE);
    // No adds: a padded copy.
    RT_CHECK_EQ(pc_rando_dzx_grown_size(s.file.data(), s.file.size(), nullptr, 0, &res), s.file.size());
}

void growNewChunk() {
    Sample s = makeSample();
    PcRandoDzx v;
    RT_CHECK_EQ(pc_rando_dzx_open(&v, s.file.data(), s.file.size()), PCR_DZX_OK);
    Bytes scob(0x24, 0x5B);
    memcpy(scob.data(), "SW_C00\0\0", 8);
    const Bytes plyr = actor("Link", 0x0000FF45, 0x9A);
    const Bytes lbnk = {0x08, 0x09};
    PcRandoDzxAdd adds[] = {{"SCOB", scob.data(), 1}, {"LBNK", lbnk.data(), 2},
                            {"PLYR", plyr.data(), 1}, {"ACT3", plyr.data(), 1}};
    int res = -1;
    const size_t need = pc_rando_dzx_grown_size(s.file.data(), s.file.size(), adds, 4, &res);
    RT_CHECK_EQ(res, PCR_DZX_OK);
    Bytes out(need);
    size_t outSize = 0;
    uint32_t first[4] = {};
    RT_CHECK_EQ(pc_rando_dzx_grow(s.file.data(), s.file.size(), adds, 4, out.data(), out.size(), &outSize, first),
                PCR_DZX_OK);
    RT_CHECK_EQ(outSize, need);
    RT_CHECK_EQ(first[0], 0u);
    RT_CHECK_EQ(first[1], 1u);
    RT_CHECK_EQ(first[2], 0u);
    RT_CHECK_EQ(first[3], 1u);

    PcRandoDzx d;
    RT_CHECK_EQ(pc_rando_dzx_open(&d, out.data(), out.size()), PCR_DZX_OK);
    RT_CHECK_EQ(d.chunkCount, 8u);
    RT_CHECK_EQ(pc_rando_dzx_find(&d, "SCOB"), 6);
    RT_CHECK_EQ(pc_rando_dzx_find(&d, "PLYR"), 7);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "SCOB", 0), scob.data(), 0x24) == 0);
    RT_CHECK(memcmp(pc_rando_dzx_entry(&d, "PLYR", 0), plyr.data(), 0x20) == 0);
    const uint8_t* lb = pc_rando_dzx_entry(&d, "LBNK", 0);
    RT_CHECK(lb != nullptr && lb[0] == 0x07 && lb[1] == 0x08 && lb[2] == 0x09);
    // Every old chunk's entries are unchanged (they moved by a 0x20-aligned shift).
    const size_t shift = pc_rando_dzx_be32(out.data() + 4 + 8) - pc_rando_dzx_be32(s.file.data() + 4 + 8);
    RT_CHECK(shift >= 24 && shift % 0x20 == 0);
    for (const char* t : {"ACTR", "TRES", "SCLS", "ACT3"}) {
        Bytes a, b;
        RT_CHECK(chunkEntries(s.file, t, a));
        RT_CHECK(chunkEntries(out, t, b));
        if (strcmp(t, "ACT3") == 0) {
            RT_CHECK(b.size() == a.size() + 0x20 && memcmp(b.data(), a.data(), a.size()) == 0);
        } else {
            RT_CHECK(a == b);
        }
    }
    // RTBL: both entries point at the shifted sub-table, whose room list moved too (fixed once).
    const uint8_t* rt = pc_rando_dzx_entry(&d, "RTBL", 0);
    RT_CHECK(rt != nullptr);
    const uint32_t sub = pc_rando_dzx_be32(rt);
    RT_CHECK_EQ(sub, (uint32_t)(s.rtblOff + 8 + shift));
    RT_CHECK_EQ(pc_rando_dzx_be32(rt + 4), sub);
    RT_CHECK_EQ(out[sub], 2);
    RT_CHECK_EQ(out[sub + 1], 0x10);
    const uint32_t rooms = pc_rando_dzx_be32(out.data() + sub + 4);
    RT_CHECK_EQ(rooms, (uint32_t)(s.rtblOff + 16 + shift));
    RT_CHECK_EQ(out[rooms], 0x81);
    RT_CHECK_EQ(out[rooms + 1], 0x02);
    // Grown chunks are 4-aligned even after the 1-byte LBNK entries.
    for (uint32_t i = 0; i < d.chunkCount; i++) {
        RT_CHECK_EQ(pc_rando_dzx_be32(out.data() + 4 + 12 * i + 8) % 4, 0u);
    }
}

// The fixture holds a synthetic file and the file wwrando's DZx writes after add_entity calls, as
// "<name> <hex>" lines: "src", "add <tag> <hex entry>" (in order), "wwrando".
void wwrandoFixture() {
    std::string text;
    RT_CHECK(rt::readFixture("R0_9_wwrando_grow.txt", text));
    Bytes src, ww;
    std::vector<std::string> addTags;
    std::vector<Bytes> addData;
    size_t at = 0;
    while (at < text.size()) {
        size_t nl = text.find('\n', at);
        if (nl == std::string::npos) {
            nl = text.size();
        }
        const std::string line = text.substr(at, nl - at);
        at = nl + 1;
        if (line.rfind("src ", 0) == 0) {
            src = hexBytes(line.substr(4));
        } else if (line.rfind("wwrando ", 0) == 0) {
            ww = hexBytes(line.substr(8));
        } else if (line.rfind("add ", 0) == 0 && line.size() > 9) {
            addTags.push_back(line.substr(4, 4));
            addData.push_back(hexBytes(line.substr(9)));
        }
    }
    RT_CHECK(!src.empty() && !ww.empty() && !addTags.empty());
    std::vector<PcRandoDzxAdd> adds;
    for (size_t i = 0; i < addTags.size(); i++) {
        adds.push_back({addTags[i].c_str(), addData[i].data(), 1});
    }
    int res = -1;
    const size_t need = pc_rando_dzx_grown_size(src.data(), src.size(), adds.data(), (int)adds.size(), &res);
    RT_CHECK_EQ(res, PCR_DZX_OK);
    Bytes out(need);
    RT_CHECK_EQ(pc_rando_dzx_grow(src.data(), src.size(), adds.data(), (int)adds.size(), out.data(), out.size(),
                                  nullptr, nullptr),
                PCR_DZX_OK);
    // Same chunk tags in the same order, same entries per chunk.
    PcRandoDzx a, b;
    RT_CHECK_EQ(pc_rando_dzx_open(&a, out.data(), out.size()), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_open(&b, ww.data(), ww.size()), PCR_DZX_OK);
    RT_CHECK_EQ(a.chunkCount, b.chunkCount);
    for (uint32_t i = 0; i < a.chunkCount && i < b.chunkCount; i++) {
        const std::string t = tagStr(out.data() + 4 + 12 * i);
        RT_CHECK(t == tagStr(ww.data() + 4 + 12 * i));
        if (t == "RTBL") {
            continue; // pointers differ by layout; checked below by content
        }
        Bytes ea, eb;
        RT_CHECK(chunkEntries(out, t.c_str(), ea));
        RT_CHECK(chunkEntries(ww, t.c_str(), eb));
        if (ea != eb) {
            fprintf(stderr, "chunk %s differs from wwrando\n", t.c_str());
            RT_CHECK(false);
        }
    }
    // RTBL by content: each entry's sub-table header and room list.
    const uint32_t n = pc_rando_dzx_count(&a, "RTBL");
    RT_CHECK_EQ(n, pc_rando_dzx_count(&b, "RTBL"));
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t sa = pc_rando_dzx_be32(pc_rando_dzx_entry(&a, "RTBL", i));
        const uint32_t sb = pc_rando_dzx_be32(pc_rando_dzx_entry(&b, "RTBL", i));
        RT_CHECK(memcmp(out.data() + sa, ww.data() + sb, 4) == 0);
        const uint32_t ra = pc_rando_dzx_be32(out.data() + sa + 4);
        const uint32_t rb = pc_rando_dzx_be32(ww.data() + sb + 4);
        RT_CHECK(memcmp(out.data() + ra, ww.data() + rb, out[sa]) == 0);
    }
}

} // namespace

int main() {
    basics();
    parsing();
    edits();
    growExisting();
    growNewChunk();
    wwrandoFixture();
    return rt::finish();
}
