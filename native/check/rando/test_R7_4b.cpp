// Step R7.4b (docs/RANDOMIZER_PLAN.md): the enemy randomizer's added, removed and converted actors
// (features/rando/pc_rando_enemy_actors.h) on synthetic stage files built here (no disc bytes): the
// table of edits, the ALLdie conversion (switch_to_set moved between the classes' bitfields), D9
// mismatches left alone, a modified TGDR, a suppressed SCOB, the stone heads' enemy_to_spawn over
// entries_by_type(ACTR), the appended actors (the file grows), the memory block slack in stage.dzs,
// and the removed-entry table check. The comparison with wwrando's own _save on the disc is
// native/tools/rando/enemy_check.py (regress_rando/R7.4b.txt).
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_enemy_actors.h"
#include "pc_rando_resources.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Entry {
    const char* name;
    uint32_t params;
    float x, y, z;
    uint16_t xr, yr, zr;
};

struct Chunk {
    const char* tag;
    uint32_t size; // entry size
    std::vector<Entry> entries;
    std::vector<uint8_t> raw; // instead of entries (MEMA, MECO)
};

void be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void bef(uint8_t* p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    be32(p, v);
}

std::vector<uint8_t> makeDzx(const std::vector<Chunk>& chunks) {
    std::vector<uint8_t> f(4 + chunks.size() * 12, 0);
    be32(f.data(), (uint32_t)chunks.size());
    for (size_t c = 0; c < chunks.size(); c++) {
        while (f.size() % 4 != 0) {
            f.push_back(0xFF);
        }
        const uint32_t off = (uint32_t)f.size();
        uint32_t count = 0;
        if (!chunks[c].raw.empty()) {
            f.insert(f.end(), chunks[c].raw.begin(), chunks[c].raw.end());
            count = (uint32_t)(chunks[c].raw.size() / chunks[c].size);
        } else {
            for (const Entry& e : chunks[c].entries) {
                std::vector<uint8_t> r(chunks[c].size, 0xFF);
                memset(r.data(), 0, 8);
                memcpy(r.data(), e.name, strlen(e.name));
                be32(&r[8], e.params);
                bef(&r[0x0C], e.x);
                bef(&r[0x10], e.y);
                bef(&r[0x14], e.z);
                be16(&r[0x18], e.xr);
                be16(&r[0x1A], e.yr);
                be16(&r[0x1C], e.zr);
                f.insert(f.end(), r.begin(), r.end());
                count++;
            }
        }
        uint8_t* row = f.data() + 4 + c * 12;
        memcpy(row, chunks[c].tag, 4);
        be32(row + 4, count);
        be32(row + 8, off);
    }
    while (f.size() % 0x20 != 0) {
        f.push_back(0xFF);
    }
    return f;
}

std::vector<Entry> filler(int n, const char* name = "kotubo") {
    std::vector<Entry> v;
    for (int i = 0; i < n; i++) {
        v.push_back({name, 0x01000000u + (uint32_t)i, (float)i, 1, 2, 0, 0, 0});
    }
    return v;
}

void* mallocAlloc(size_t size, void*) {
    return calloc(1, size);
}

struct File {
    std::vector<uint8_t> bytes;
    PcRandoResFile f = {};
    PcRandoResNames names = {};
    File(const char* stage, int room, std::vector<uint8_t> b) : bytes(std::move(b)) {
        char dir[PCR_RES_DIR_MAX], arc[16];
        snprintf(dir, sizeof dir, "/res/Stage/%s/", stage);
        if (room >= 0) {
            snprintf(arc, sizeof arc, "Room%d", room);
        } else {
            snprintf(arc, sizeof arc, "Stage");
        }
        pc_rando_res_describe(&f, &names, arc, dir);
        f.name = room >= 0 ? "room.dzr" : "stage.dzs";
        f.type = room >= 0 ? PCR_RES_DZR : PCR_RES_DZS;
        f.data = bytes.data();
        f.size = bytes.size();
        f.alloc = mallocAlloc;
    }
    ~File() {
        if (f.data != bytes.data()) {
            free(f.data);
        }
    }
    const uint8_t* entry(const char* tag, uint32_t i) {
        PcRandoDzx d;
        if (pc_rando_dzx_open(&d, f.data, f.size) != PCR_DZX_OK) {
            return nullptr;
        }
        return pc_rando_dzx_entry(&d, tag, i);
    }
    uint32_t count(const char* tag) {
        PcRandoDzx d;
        return pc_rando_dzx_open(&d, f.data, f.size) == PCR_DZX_OK ? pc_rando_dzx_count(&d, tag) : 0;
    }
};

int targetIndex(const char* label) {
    int n = 0;
    const PcRandoEnemyActorTarget* t = pc_rando_enemy_actors_targets(&n);
    for (int i = 0; i < n; i++) {
        if (strcmp(t[i].label, label) == 0) {
            return i;
        }
    }
    return -1;
}

const int kHomen[3] = {10, 3, 7};

} // namespace

int main() {
    // The table: 13 ALLdies, 3 modified, 5 added, 2 removed, 3 stone heads, in that order.
    int n = 0;
    const PcRandoEnemyActorTarget* t = pc_rando_enemy_actors_targets(&n);
    RT_CHECK_EQ(n, 26);
    int kinds[5] = {};
    for (int i = 0; i < n; i++) {
        kinds[t[i].kind]++;
        if (i > 0) {
            RT_CHECK(t[i].kind >= t[i - 1].kind);
        }
    }
    RT_CHECK_EQ(kinds[PCR_EACTOR_ALLDIE], 13);
    RT_CHECK_EQ(kinds[PCR_EACTOR_MODIFY], 3);
    RT_CHECK_EQ(kinds[PCR_EACTOR_ADD], 5);
    RT_CHECK_EQ(kinds[PCR_EACTOR_REMOVE], 2);
    RT_CHECK_EQ(kinds[PCR_EACTOR_HOMEN], 3);
    RT_CHECK_EQ(std::string(t[targetIndex("TF_01/Stage.arc/ScalableDoor005")].tag), std::string("TGDR"));
    RT_CHECK_EQ(std::string(t[targetIndex("SirenMB/Room23.arc/ScalableObject000")].tag), std::string("SCOB"));
    RT_CHECK(targetIndex("M_NewD2/Room1.arc/Add1") >= 0);
    RT_CHECK(targetIndex("kinMB/Room10.arc/Add0") >= 0);
    RT_CHECK_EQ(pc_rando_enemy_actors_selfcheck(nullptr, 0), 0);

    int8_t results[64];

    // ALLdie: Siren/Room17 Actor00E is an AND_SW0 (switch_to_set: params 0xFF000000) -> ALLdie
    // (switch_to_set: params 0x0000FF00), params otherwise 0xFFFFFFFF, x/z_rot 0, y_rot kept.
    {
        std::vector<Entry> actr = filler(0x10);
        actr[0x0E] = {"AND_SW0", 0x0FE10003u, 5, 6, 7, 0x1234, 0x4000, 0x00FF};
        File f("Siren", 17, makeDzx({{"ACTR", 0x20, actr, {}}}));
        std::vector<uint8_t> before(f.bytes);
        memset(results, -1, sizeof results);
        int handled = 0;
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, results, &handled), 1);
        RT_CHECK_EQ(handled, 1);
        RT_CHECK_EQ((int)results[targetIndex("Siren/Room17.arc/Actor00E")], 1);
        const uint8_t* e = f.entry("ACTR", 0x0E);
        RT_CHECK(e != nullptr && memcmp(e, "ALLdie\0\0", 8) == 0);
        RT_CHECK_EQ(pc_rando_dzx_be32(e + 8), 0xFFFF0FFFu);
        RT_CHECK_EQ(pc_rando_dzx_be16(e + 0x18), (uint16_t)0);
        RT_CHECK_EQ(pc_rando_dzx_be16(e + 0x1A), (uint16_t)0x4000);
        RT_CHECK_EQ(pc_rando_dzx_be16(e + 0x1C), (uint16_t)0);
        RT_CHECK_EQ(pc_rando_dzx_bef32(e + 0x0C), 5.0f);
        // every other byte kept
        memcpy(before.data() + (e - f.f.data), e, 0x20);
        RT_CHECK(f.f.size == before.size() && memcmp(before.data(), f.f.data, before.size()) == 0);
    }
    // Kbota_B (d_a_obj_swpush switch_to_set 0x0000FF00) and AND_SW2 (0x00FF0000).
    {
        std::vector<Entry> actr = filler(1);
        actr[0] = {"Kbota_B", 0x4100E2FFu, 0, 0, 0, 0, 0, 0};
        File f("Cave01", 0, makeDzx({{"ACTR", 0x20, actr, {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, nullptr), 1);
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("ACTR", 0) + 8), 0xFFFFE2FFu);
        std::vector<Entry> a2 = filler(6);
        a2[5] = {"AND_SW2", 0xE1E00004u, 0, 0, 0, 0, 0, 0};
        File g("Abship", 0, makeDzx({{"ACTR", 0x20, a2, {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &g.f, nullptr, nullptr), 1);
        RT_CHECK_EQ(pc_rando_dzx_be32(g.entry("ACTR", 5) + 8), 0xFFFFE0FFu);
    }
    // D9: another actor at the path stays as it is.
    {
        std::vector<Entry> actr = filler(0x10);
        File f("Siren", 17, makeDzx({{"ACTR", 0x20, actr, {}}}));
        std::vector<uint8_t> before(f.bytes);
        memset(results, -1, sizeof results);
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, results, nullptr), 0);
        RT_CHECK_EQ((int)results[targetIndex("Siren/Room17.arc/Actor00E")], 0);
        RT_CHECK(memcmp(before.data(), f.f.data, before.size()) == 0);
        // a missing entry too
        File g("Siren", 17, makeDzx({{"ACTR", 0x20, filler(3), {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &g.f, results, nullptr), 0);
    }
    // Modified: TF_01's stage.dzs TGDR 005 Zenshut -> door10 0x0FFFF022 (0x24-byte entries).
    {
        std::vector<Entry> tgdr = filler(6, "door10");
        tgdr[5] = {"Zenshut", 0x0FFFF222u, 1, 2, 3, 0, 0x8000, 0};
        File f("TF_01", -1, makeDzx({{"TGDR", 0x24, tgdr, {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, nullptr), 1);
        const uint8_t* e = f.entry("TGDR", 5);
        RT_CHECK(memcmp(e, "door10\0\0", 8) == 0);
        RT_CHECK_EQ(pc_rando_dzx_be32(e + 8), 0x0FFFF022u);
        RT_CHECK_EQ(pc_rando_dzx_be16(e + 0x1A), (uint16_t)0x8000);
        RT_CHECK_EQ(e[0x20], (uint8_t)0xFF); // the scale bytes kept
    }
    // Modified with a position: M_NewD2/Room11 Actor004 (ootubo1).
    {
        std::vector<Entry> actr = filler(5);
        actr[4] = {"ootubo1", 0x017FFF01u, 9, 9, 9, 0, 0, 0};
        File f("M_NewD2", 11, makeDzx({{"ACTR", 0x20, actr, {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, nullptr), 1);
        const uint8_t* e = f.entry("ACTR", 4);
        RT_CHECK_EQ(pc_rando_dzx_be32(e + 8), 0x017FC201u);
        RT_CHECK_EQ(pc_rando_dzx_bef32(e + 0x0C), -3953.0f);
        RT_CHECK_EQ(pc_rando_dzx_bef32(e + 0x10), 500.0f);
        RT_CHECK_EQ(pc_rando_dzx_bef32(e + 0x14), 1426.0f);
    }
    // Removed: SirenMB/Room23 SCOB 000 (TagEv): the name cleared, the index kept.
    {
        std::vector<Entry> scob = filler(2, "TagEv");
        File f("SirenMB", 23, makeDzx({{"ACTR", 0x20, filler(1, "Bb"), {}}, {"SCOB", 0x24, scob, {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, nullptr), 1);
        RT_CHECK(memcmp(f.entry("SCOB", 0), "\0\0\0\0\0\0\0\0", 8) == 0);
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("SCOB", 0) + 8), 0x01000000u);
        RT_CHECK(memcmp(f.entry("SCOB", 1), "TagEv", 5) == 0);
        RT_CHECK_EQ(f.count("SCOB"), 2u);
    }
    // Stone heads: kaze/Room10 entries_by_type(ACTR)[0] and [9], across an ACTR and an ACT1 chunk.
    {
        std::vector<Entry> a = filler(5), b = filler(6);
        a[0] = {"Homen2", 0x980BF105u, 0, 0, 0, 0, 0, 0};
        b[4] = {"Homen1", 0x9B0FF106u, 0, 0, 0, 0, 0, 0}; // by-type index 5 + 4 = 9
        File f("kaze", 10, makeDzx({{"ACTR", 0x20, a, {}}, {"ACT1", 0x20, b, {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, nullptr), 2);
        // enemy_to_spawn: params 0x003C0000
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("ACTR", 0) + 8), (0x980BF105u & ~0x003C0000u) | (3u << 18));
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("ACT1", 4) + 8), (0x9B0FF106u & ~0x003C0000u) | (7u << 18));
    }
    // Added: M_NewD2/Room1 gets ootubo1 and bonbori at the end of its ACTR chunk (the file grows).
    {
        std::vector<Entry> actr = filler(12);
        File f("M_NewD2", 1, makeDzx({{"FILI", 8, {}, std::vector<uint8_t>(8, 0)}, {"ACTR", 0x20, actr, {}},
                                       {"SCOB", 0x24, filler(2), {}}}));
        const size_t size0 = f.f.size;
        memset(results, -1, sizeof results);
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, results, nullptr), 2);
        RT_CHECK(f.f.size > size0 && f.f.data != f.bytes.data());
        RT_CHECK_EQ((int)results[targetIndex("M_NewD2/Room1.arc/Add0")], 1);
        RT_CHECK_EQ(f.count("ACTR"), 14u);
        RT_CHECK_EQ(f.count("SCOB"), 2u);
        const uint8_t* a = f.entry("ACTR", 12);
        RT_CHECK(memcmp(a, "ootubo1\0", 8) == 0);
        RT_CHECK_EQ(pc_rando_dzx_be32(a + 8), 0x717FC23Fu);
        RT_CHECK_EQ(pc_rando_dzx_bef32(a + 0x0C), 3886.0f);
        RT_CHECK_EQ(pc_rando_dzx_be16(a + 0x1C), (uint16_t)0x00FF);
        RT_CHECK_EQ(pc_rando_dzx_be16(a + 0x1E), (uint16_t)0xFFFF); // dzx.py's enemy_number default
        const uint8_t* b = f.entry("ACTR", 13);
        RT_CHECK(memcmp(b, "bonbori\0", 8) == 0);
        RT_CHECK_EQ(pc_rando_dzx_be32(b + 8), 0xFFFFFFFFu);
        // the vanilla entries keep their bytes
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("ACTR", 11) + 8), 0x0100000Bu);
        // TF_01/Room1: ALLdie (no position: 0) and AND_SW2 x_rot 0x00FF
        File g("TF_01", 1, makeDzx({{"ACTR", 0x20, filler(19), {}}}));
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &g.f, nullptr, nullptr), 2);
        const uint8_t* c = g.entry("ACTR", 19);
        RT_CHECK(memcmp(c, "ALLdie\0\0", 8) == 0);
        RT_CHECK_EQ(pc_rando_dzx_be32(c + 8), 0xFFFF99FFu);
        RT_CHECK_EQ(pc_rando_dzx_bef32(c + 0x10), 0.0f);
        RT_CHECK_EQ(pc_rando_dzx_be16(g.entry("ACTR", 20) + 0x18), (uint16_t)0x00FF);
        // no allocator: nothing added, the file unchanged, the adds failed
        File h("kinMB", 10, makeDzx({{"ACTR", 0x20, filler(84), {}}}));
        h.f.alloc = nullptr;
        memset(results, -1, sizeof results);
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &h.f, results, nullptr), 0);
        RT_CHECK_EQ((int)results[targetIndex("kinMB/Room10.arc/Add0")], 2);
        RT_CHECK_EQ(h.count("ACTR"), 84u);
    }
    // The memory block slack: M_NewD2's stage.dzs MEMA block of room 1 (MECO 1 -> block 2) + 0x1000.
    {
        std::vector<uint8_t> mema(5 * 4), meco;
        for (int i = 0; i < 5; i++) {
            be32(&mema[i * 4], 0x10000u * (i + 1));
        }
        const uint8_t pairs[][2] = {{0, 1}, {1, 2}, {2, 0}, {3, 1}};
        for (const auto& p : pairs) {
            meco.push_back(p[0]);
            meco.push_back(p[1]);
        }
        File f("M_NewD2", -1, makeDzx({{"MEMA", 4, {}, mema}, {"MECO", 2, {}, meco}}));
        pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, nullptr);
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("MEMA", 2)), 0x30000u + 0x1000u);
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("MEMA", 1)), 0x20000u);
        RT_CHECK_EQ(pc_rando_dzx_be32(f.entry("MEMA", 0)), 0x10000u);
        // a stage without new actors keeps its blocks
        File g("kaze", -1, makeDzx({{"MEMA", 4, {}, mema}, {"MECO", 2, {}, meco}}));
        pc_rando_enemy_actors_apply(kHomen, &g.f, nullptr, nullptr);
        RT_CHECK(memcmp(g.f.data, g.bytes.data(), g.bytes.size()) == 0);
    }
    // Files without edits are not touched.
    {
        File f("M_NewD2", 2, makeDzx({{"ACTR", 0x20, filler(8), {}}}));
        int handled = -1;
        RT_CHECK_EQ(pc_rando_enemy_actors_apply(kHomen, &f.f, nullptr, &handled), 0);
        RT_CHECK_EQ(handled, 0);
    }
    return rt::finish();
}
