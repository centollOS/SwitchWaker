// cos_rando_test_R3_16 (docs/RANDOMIZER_PLAN.md step R3.16): stage tweaks B
// (features/rando/pc_rando_tweak_stages_b.h) on synthetic stage files, without the game:
//   - M_NewD2 room 2: the skull drops (#8, entries_by_type(ACTR) across layers), the hint sign (#12,
//     default-layer BFlower[1]), the copied spawn 0 (#19) and the panning cutscene (#21: TagEv with
//     evnt 4 removed, the spawn's evnt cleared, other events kept), then pc_rando_tweak_stages_b_verify;
//   - TF_01: the spawn in front of a stage door (#19), computed from stage.dzs and appended to a room
//     without a PLYR chunk; the expected floats are Python's (tweaks.py's math on the same door);
//   - a door spawn whose stage.dzs was never seen, and a room without the sign's BFlower (results
//     no chunk / mismatch, the rest of the file still patched);
//   - a stage without sites is left alone, and message 847.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_msg.h"
#include "pc_rando_resources.h"
#include "pc_rando_tweak_stages_b.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void putf(uint8_t* p, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    put32(p, u);
}

struct Chunk {
    const char* tag;
    uint32_t esize;
    std::vector<Bytes> entries;
};

Bytes entry(uint32_t size, const char* name, uint32_t params, float x, float y, float z, uint16_t xr,
            uint16_t yr, uint16_t zr) {
    Bytes e(size, 0);
    strncpy((char*)e.data(), name, 8);
    put32(&e[8], params);
    putf(&e[0x0C], x);
    putf(&e[0x10], y);
    putf(&e[0x14], z);
    put16(&e[0x18], xr);
    put16(&e[0x1A], yr);
    put16(&e[0x1C], zr);
    put16(&e[0x1E], 0xFFFF);
    return e;
}

Bytes build(const std::vector<Chunk>& chunks) {
    Bytes b(4 + 12 * chunks.size(), 0);
    put32(&b[0], (uint32_t)chunks.size());
    for (size_t i = 0; i < chunks.size(); i++) {
        while (b.size() % 4) {
            b.push_back(0);
        }
        uint8_t* row = &b[4 + 12 * i];
        memcpy(row, chunks[i].tag, 4);
        put32(row + 4, (uint32_t)chunks[i].entries.size());
        put32(row + 8, (uint32_t)b.size());
        for (const Bytes& e : chunks[i].entries) {
            b.insert(b.end(), e.begin(), e.end());
        }
    }
    while (b.size() % 0x20) {
        b.push_back(0xFF);
    }
    return b;
}

std::vector<void*> sAllocs;
void* testAlloc(size_t size, void*) {
    void* p = calloc(1, size);
    sAllocs.push_back(p);
    return p;
}

PcRandoResFile fileFor(const char* stage, int room, Bytes& data) {
    PcRandoResFile f = {};
    f.dir = "";
    f.stage = stage;
    f.arc = room < 0 ? "Stage" : "Room";
    f.room = room;
    f.name = room < 0 ? "stage.dzs" : "room.dzr";
    f.type = room < 0 ? PCR_RES_DZS : PCR_RES_DZR;
    f.data = data.data();
    f.size = data.size();
    f.alloc = testAlloc;
    return f;
}

const PcRandoStagesBSite* site(const char* stage, int room, int tweak, int nth = 0) {
    for (int i = 0; i < pc_rando_tweak_stages_b_site_count(); i++) {
        const PcRandoStagesBSite* s = pc_rando_tweak_stages_b_site(i);
        if (strcmp(s->stage, stage) == 0 && s->room == room && s->tweak == tweak && nth-- == 0) {
            return s;
        }
    }
    return nullptr;
}

int siteResult(const char* stage, int room, int tweak, int nth = 0) {
    const PcRandoStagesBSite* s = site(stage, room, tweak, nth);
    return s == nullptr ? -1 : (s->runs > 0 ? s->result : -2);
}

uint32_t be32At(const PcRandoDzx& d, const char* tag, uint32_t i, uint32_t off) {
    const uint8_t* e = pc_rando_dzx_entry(&d, tag, i);
    return e == nullptr ? 0xDEADBEEF : pc_rando_dzx_be32(e + off);
}

std::string nameAt(const PcRandoDzx& d, const char* tag, uint32_t i) {
    const uint8_t* e = pc_rando_dzx_entry(&d, tag, i);
    return e == nullptr ? std::string("?") : std::string((const char*)e, strnlen((const char*)e, 8));
}

void testDrcHub() {
    const uint32_t skull = 0x757FFF3Fu;
    Chunk actr{"ACTR", 0x20, {}};
    Chunk act3{"ACT3", 0x20, {}};
    // Odokuro 0, 1 and 4, 5 in ACTR, 2 and 3 in ACT3 (chunk order: ACTR first).
    for (int i = 0; i < 2; i++) {
        actr.entries.push_back(entry(0x20, "Odokuro", skull, 0, 0, 0, 0, 0, 0));
    }
    actr.entries.push_back(entry(0x20, "BFlower", 0xFF00, 1, 2, 3, 0, 0xC000, 0));
    actr.entries.push_back(entry(0x20, "BFlower", 0xFF00, 4, 5, 6, 0, 0xC000, 0));
    for (int i = 0; i < 2; i++) {
        actr.entries.push_back(entry(0x20, "Odokuro", skull, 0, 0, 0, 0, 0, 0));
    }
    for (int i = 0; i < 2; i++) {
        act3.entries.push_back(entry(0x20, "Odokuro", skull, 9, 9, 9, 0, 0, 0));
    }
    act3.entries.push_back(entry(0x20, "BFlower", 0xFF00, 7, 8, 9, 0, 0, 0)); // not the default layer
    Chunk plyr{"PLYR", 0x20,
               {entry(0x20, "Link", 0x04FF2002, 10.5f, 20.25f, -30.0f, 0, 0x8000, 0xFF01),
                entry(0x20, "Link", 0x05FF0002, 1, 1, 1, 0, 0, 0xFF03)}};
    Chunk scob{"SCOB", 0x24,
               {entry(0x24, "TagEv", 0x04FF33FF, 0, 0, 0, 0, 0, 0), entry(0x24, "TagEv", 0x05FF33FF, 0, 0, 0, 0, 0, 0)}};
    Bytes raw = build({actr, plyr, scob, act3});
    Bytes data = raw;
    PcRandoResFile f = fileFor("M_NewD2", 2, data);
    pc_rando_tweak_stages_b_patch(&f);
    RT_CHECK(f.replaced == 1);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_MAGIC_JAR, 0), (int)PCR_DZX_OK);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_MAGIC_JAR, 1), (int)PCR_DZX_OK);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_HINT_SIGN), (int)PCR_DZX_OK);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_FAILSAFE_SPAWN), (int)PCR_DZX_OK);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_PANNING), (int)PCR_DZX_OK);

    PcRandoDzx d;
    RT_CHECK_EQ(pc_rando_dzx_open(&d, f.data, f.size), (int)PCR_DZX_OK);
    // The skulls in entries_by_type order: ACTR 0, 1, 4, 5, then ACT3 0, 1. So skulls[2] is ACTR[4]
    // (small magic) and skulls[5] is ACT3[1] (large magic).
    RT_CHECK_EQ(be32At(d, "ACTR", 4, 8), (skull & ~0x3Fu) | 0x09);
    RT_CHECK_EQ(be32At(d, "ACT3", 1, 8), (skull & ~0x3Fu) | 0x0A);
    RT_CHECK_EQ(be32At(d, "ACTR", 0, 8), skull);
    RT_CHECK_EQ(be32At(d, "ACTR", 5, 8), skull);
    RT_CHECK_EQ(be32At(d, "ACT3", 0, 8), skull);
    // the sign: default-layer BFlower[1] = ACTR[3]
    RT_CHECK_EQ(nameAt(d, "ACTR", 3), std::string("Kanban"));
    RT_CHECK_EQ(be32At(d, "ACTR", 3, 8), 847u);
    RT_CHECK_EQ(be32At(d, "ACTR", 3, 0x18) >> 16 & 0xFFFF, 0x0000u);
    RT_CHECK_EQ(be32At(d, "ACTR", 3, 0x18) & 0xFFFF, 0x2000u);
    RT_CHECK_EQ(nameAt(d, "ACTR", 2), std::string("BFlower"));
    RT_CHECK_EQ(nameAt(d, "ACT3", 2), std::string("BFlower"));
    // spawn 0: a copy of spawn 1 (spawn_type 2, room 2, position, y_rot) with PLYR() defaults
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "PLYR"), 3u);
    const uint8_t* s0 = pc_rando_dzx_entry(&d, "PLYR", 2);
    RT_CHECK(s0 != nullptr && memcmp(s0, "Link\0\0\0\0", 8) == 0);
    RT_CHECK_EQ(be32At(d, "PLYR", 2, 8), 0xFFFF2002u);
    RT_CHECK_EQ(pc_rando_dzx_bef32(s0 + 0x0C), 10.5f);
    RT_CHECK_EQ(pc_rando_dzx_bef32(s0 + 0x10), 20.25f);
    RT_CHECK_EQ(pc_rando_dzx_bef32(s0 + 0x14), -30.0f);
    RT_CHECK_EQ(be32At(d, "PLYR", 2, 0x18), 0x00008000u);
    RT_CHECK_EQ(be32At(d, "PLYR", 2, 0x1C), 0xFF00FFFFu);
    // panning: spawn 1's evnt 4 cleared, spawn 3's evnt 5 kept; TagEv 4 removed, TagEv 5 kept
    RT_CHECK_EQ(be32At(d, "PLYR", 0, 8), 0xFFFF2002u);
    RT_CHECK_EQ(be32At(d, "PLYR", 1, 8), 0x05FF0002u);
    RT_CHECK_EQ(nameAt(d, "SCOB", 0), std::string(""));
    RT_CHECK_EQ(nameAt(d, "SCOB", 1), std::string("TagEv"));
    char why[160] = {};
    RT_CHECK_EQ(pc_rando_tweak_stages_b_verify("M_NewD2", 2, f.data, (uint32_t)f.size, why, sizeof why), 1);
    RT_CHECK_EQ(pc_rando_tweak_stages_b_verify("M_NewD2", 2, raw.data(), (uint32_t)raw.size(), why, sizeof why), 0);
    RT_CHECK(why[0] != 0);
}

void testTfDoor() {
    pc_rando_tweak_stages_b_reset();
    // Room 1 before its stage.dzs was seen: no spawn, result "no chunk".
    Bytes early = build({{"ACTR", 0x20, {entry(0x20, "kusax1", 0, 0, 0, 0, 0, 0, 0)}}});
    PcRandoResFile fe = fileFor("TF_01", 1, early);
    pc_rando_tweak_stages_b_patch(&fe);
    RT_CHECK_EQ(siteResult("TF_01", 1, PCR_SB_FAILSAFE_SPAWN), (int)PCR_DZX_NO_CHUNK);
    RT_CHECK(fe.replaced == 0);

    // A door10 from room 3 to room 1 at (1234.5, 10, -987.25), y_rot 0x1234.
    Bytes dzs = build({{"TGDR", 0x24, {entry(0x24, "door10", 0, 1234.5f, 10.0f, -987.25f, (uint16_t)(1 << 6 | 3), 0x1234, 0)}}});
    PcRandoResFile fs = fileFor("TF_01", -1, dzs);
    pc_rando_tweak_stages_b_patch(&fs);
    RT_CHECK(fs.replaced == 0);
    for (int room : {3, 1}) {
        Bytes data = build({{"ACTR", 0x20, {entry(0x20, "kusax1", 0, 0, 0, 0, 0, 0, 0)}}});
        PcRandoResFile f = fileFor("TF_01", room, data);
        pc_rando_tweak_stages_b_patch(&f);
        RT_CHECK_EQ(siteResult("TF_01", room, PCR_SB_FAILSAFE_SPAWN), (int)PCR_DZX_OK);
        PcRandoDzx d;
        RT_CHECK_EQ(pc_rando_dzx_open(&d, f.data, f.size), (int)PCR_DZX_OK);
        RT_CHECK_EQ(pc_rando_dzx_count(&d, "PLYR"), 1u);
        // Python: x = f32(1234.5) + sin(radians(y_rot * 90 / 0x4000)) * 200, z likewise with cos; room 1
        // is the door's "to" side, so its y_rot is turned by 0x8000.
        const bool from = room == 3;
        RT_CHECK_EQ(be32At(d, "PLYR", 0, 0x0C), from ? 0x44A51D29u : 0x448F82D7u);
        RT_CHECK_EQ(be32At(d, "PLYR", 0, 0x10), 0x41200000u); // 10.0
        RT_CHECK_EQ(be32At(d, "PLYR", 0, 0x14), from ? 0xC449B85Du : 0xC491F3D1u);
        RT_CHECK_EQ(be32At(d, "PLYR", 0, 8), 0xFFFF0000u | (uint32_t)room);
        RT_CHECK_EQ(be32At(d, "PLYR", 0, 0x18), from ? 0x00001234u : 0x00009234u);
        RT_CHECK_EQ(be32At(d, "PLYR", 0, 0x1C), 0xFF00FFFFu);
        RT_CHECK_EQ(pc_rando_tweak_stages_b_verify("TF_01", room, f.data, (uint32_t)f.size, nullptr, 0), 1);
    }
}

void testMismatchAndOthers() {
    pc_rando_tweak_stages_b_reset();
    // M_NewD2 room 2 without BFlower and without spawn 1: sign and spawn refused, drops still set.
    Chunk actr{"ACTR", 0x20, {}};
    for (int i = 0; i < 6; i++) {
        actr.entries.push_back(entry(0x20, "Odokuro", 0x3F, 0, 0, 0, 0, 0, 0));
    }
    Bytes data = build({actr});
    PcRandoResFile f = fileFor("M_NewD2", 2, data);
    pc_rando_tweak_stages_b_patch(&f);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_HINT_SIGN), (int)PCR_DZX_MISMATCH);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_FAILSAFE_SPAWN), (int)PCR_DZX_MISMATCH);
    RT_CHECK_EQ(siteResult("M_NewD2", 2, PCR_SB_MAGIC_JAR, 0), (int)PCR_DZX_OK);
    RT_CHECK(f.replaced == 0);
    PcRandoDzx d;
    RT_CHECK_EQ(pc_rando_dzx_open(&d, f.data, f.size), (int)PCR_DZX_OK);
    RT_CHECK_EQ(be32At(d, "ACTR", 2, 8), 0x09u);
    RT_CHECK_EQ(be32At(d, "ACTR", 5, 8), 0x0Au);

    // A stage without sites, and a non-dzx file of a site stage: untouched.
    Bytes other = build({actr});
    const Bytes before = other;
    PcRandoResFile fo = fileFor("Omori", 0, other);
    pc_rando_tweak_stages_b_patch(&fo);
    RT_CHECK(fo.replaced == 0 && other == before);
    Bytes brk = before;
    PcRandoResFile fb = fileFor("M_NewD2", 2, brk);
    fb.name = "model.brk";
    pc_rando_tweak_stages_b_patch(&fb);
    RT_CHECK(fb.replaced == 0 && brk == before);

    // Forest Haven: the warp and its switch trigger appended to SCOB.
    pc_rando_tweak_stages_b_reset();
    Bytes fh = build({{"SCOB", 0x24, {entry(0x24, "kytag00", 0, 0, 0, 0, 0, 0, 0)}}});
    PcRandoResFile ff = fileFor("sea", 41, fh);
    pc_rando_tweak_stages_b_patch(&ff);
    RT_CHECK_EQ(siteResult("sea", 41, PCR_SB_FH_WARP), (int)PCR_DZX_OK);
    RT_CHECK(ff.replaced == 1);
    RT_CHECK_EQ(pc_rando_dzx_open(&d, ff.data, ff.size), (int)PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_count(&d, "SCOB"), 3u);
    RT_CHECK_EQ(nameAt(d, "SCOB", 1), std::string("Ysdls00"));
    RT_CHECK_EQ(be32At(d, "SCOB", 1, 8), 0x10FF067Fu);
    RT_CHECK_EQ(be32At(d, "SCOB", 1, 0x20), 0x0A0A0AFFu);
    RT_CHECK_EQ(nameAt(d, "SCOB", 2), std::string("SW_C00"));
    RT_CHECK_EQ(be32At(d, "SCOB", 2, 8), 0x0003FF7Fu);
    RT_CHECK_EQ(be32At(d, "SCOB", 2, 0x20), 0x303030FFu);
    RT_CHECK_EQ(pc_rando_tweak_stages_b_verify("sea", 41, ff.data, (uint32_t)ff.size, nullptr, 0), 1);
}

void testMessage() {
    RT_CHECK_EQ(pc_rando_tweak_stages_b_add_messages(), 1);
    const PcRandoMsg* m = pc_rando_msg_find(847);
    RT_CHECK(m != nullptr);
    if (m != nullptr) {
        RT_CHECK(m->has_entry);
        RT_CHECK_EQ((int)m->entry.text_box_type, 2);
        RT_CHECK_EQ((int)m->entry.initial_draw_type, 1);
        RT_CHECK_EQ((int)m->entry.text_alignment, 3);
        const uint8_t want[] = {0x1A, 0x05, 0x00, 0x00, 0x15, 0x00};
        RT_CHECK(m->len == sizeof want && memcmp(m->bytes, want, sizeof want) == 0);
    }
}

} // namespace

int main() {
    testDrcHub();
    testTfDoor();
    testMismatchAndOthers();
    testMessage();
    for (void* p : sAllocs) {
        free(p);
    }
    return rt::finish();
}
