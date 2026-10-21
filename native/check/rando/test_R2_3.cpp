// cos_rando_test_R2_3 (docs/RANDOMIZER_PLAN.md step R2.3): the SCOB placement patcher
// (features/rando/pc_rando_place_scob.h) without the game, on synthetic room.dzr files built from the
// key table (names and disc values of R0.5's fingerprints; no disc bytes):
//   - the key set: 204 SCOB keys over 57 locations, all of class SCOB, every Salvage/SwSlvg key with
//     mask 0x00000FF0 and the dig spot with 0x000000FF;
//   - pc_rando() 0: nothing is written;
//   - pc_rando() 1 with a placement: every key of every one of the 49 sea rooms holds the placed item,
//     only the field bits change, the room's other entries and other rooms' keys stay as they were;
//   - D9: a changed name or a changed vanilla item leaves that entry vanilla (MISMATCH), a missing
//     entry gives MISSING, and the other keys of the room are still placed;
//   - fix_needle_rock_island_salvage_flags: the first two d_a_salvage SCOBs of type 2-4 with flag 0/1
//     (across SCOB chunks, in file order) get flags 8 and 9; type-0 salvages, other actors and a third
//     candidate do not; fewer than two candidates leave the room vanilla;
//   - registration through pc_rando_place_scob_init and pc_rando_res_dispatch.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_dzx.h"
#include "pc_rando_lockeys.h"
#include "pc_rando_place_scob.h"
#include "pc_rando_resources.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
constexpr uint32_t kScobSize = 0x24;

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
uint32_t get32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

struct Chunk {
    std::string tag;
    std::vector<Bytes> entries;
};

Bytes build(const std::vector<Chunk>& chunks) {
    size_t off = 4 + 12 * chunks.size();
    Bytes b(off, 0);
    put32(&b[0], (uint32_t)chunks.size());
    for (size_t c = 0; c < chunks.size(); c++) {
        memcpy(&b[4 + c * 12], chunks[c].tag.data(), 4);
        put32(&b[4 + c * 12 + 4], (uint32_t)chunks[c].entries.size());
        put32(&b[4 + c * 12 + 8], (uint32_t)b.size());
        for (const Bytes& e : chunks[c].entries) {
            b.insert(b.end(), e.begin(), e.end());
        }
    }
    while (b.size() % 0x20 != 0) {
        b.push_back(0xFF);
    }
    return b;
}

// A filler SCOB entry: a name no key uses, params with every bit set.
Bytes filler(const char* name = "Filler") {
    Bytes e(kScobSize, 0);
    strncpy((char*)e.data(), name, 8);
    put32(&e[0x08], 0xFFFFFFFFu);
    return e;
}

// An entry as the key's fingerprint expects it: its name, other params bits set (so the test sees
// that only the field changes), the field holding the disc value.
Bytes keyEntry(const pcr::LocKey& k) {
    Bytes e(kScobSize, 0);
    memcpy(e.data(), k.fp_name, 8);
    put32(&e[k.word_off], 0xFFFFFFFFu);
    pcr::dzx_key_write(k, e.data(), k.disc_byte);
    return e;
}

Bytes salvage(const char* name, uint32_t type, uint32_t flag, uint32_t item) {
    Bytes e(kScobSize, 0);
    strncpy((char*)e.data(), name, 8);
    put32(&e[0x08], type << 28 | flag << 20 | 0xFF000u | item << 4 | 1u);
    return e;
}

// The room.dzr of a sea room with every SCOB key of that room, fillers elsewhere.
Bytes makeRoom(const std::string& arc) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    std::map<uint32_t, const pcr::LocKey*> at;
    uint32_t n = 0;
    for (const pcr::LocKey& k : keys) {
        if (k.kind == pcr::KeyKind::Dzx && strcmp(k.tag, "SCOB") == 0 && arc == k.arc) {
            at[k.index] = &k;
            n = n > k.index + 1u ? n : k.index + 1u;
        }
    }
    Chunk c{"SCOB", {}};
    for (uint32_t i = 0; i < n + 2; i++) {
        c.entries.push_back(at.count(i) ? keyEntry(*at[i]) : filler());
    }
    return build({Chunk{"ACTR", {}}, c});
}

PcRandoResFile fileFor(const char* stage, const char* arc, Bytes& b) {
    PcRandoResFile f = {};
    f.dir = "/res/Stage/sea/";
    f.stage = stage;
    f.arc = arc;
    f.room = -1;
    f.name = "room.dzr";
    f.type = PCR_RES_DZR;
    f.data = b.data();
    f.size = b.size();
    return f;
}

bool isScob(const pcr::LocKey& k) {
    return k.kind == pcr::KeyKind::Dzx &&
           (k.field == pcr::DzxField::SalvageItem || k.field == pcr::DzxField::TagKbItem);
}

// The placed item of a location in this test: never the vanilla one, and different per location.
uint8_t placedFor(int loc, uint8_t vanilla) {
    uint8_t v = (uint8_t)(0x20 + (loc * 7) % 0xA0);
    return v == vanilla ? (uint8_t)(v + 1) : v;
}

uint8_t sItems[PCR_LOC_COUNT];

} // namespace

int main() {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();

    // --- the key set
    std::set<int> locs;
    std::set<std::string> arcs;
    int nKeys = 0;
    for (const pcr::LocKey& k : keys) {
        if (!isScob(k)) {
            continue;
        }
        nKeys++;
        locs.insert(k.loc);
        arcs.insert(k.arc);
        RT_CHECK(strcmp(k.stage, "sea") == 0);
        RT_CHECK(strcmp(k.tag, "SCOB") == 0);
        RT_CHECK_EQ((int)k.word_off, 8);
        RT_CHECK_EQ((int)k.width, 4);
        RT_CHECK_EQ(k.mask, k.field == pcr::DzxField::SalvageItem ? 0x00000FF0u : 0x000000FFu);
        RT_CHECK((k.flags & pcr::KEY_FP_ITEM) != 0 && k.disc_byte == k.vanilla);
        const pcr::LocInfo* info = pcr::location_info(k.loc);
        RT_CHECK(info != nullptr && info->classes == pcr::PCR_CLASS_SCOB);
    }
    RT_CHECK_EQ(nKeys, 204);
    RT_CHECK_EQ(pc_rando_place_scob_key_count(), 204);
    RT_CHECK_EQ((int)locs.size(), 57);
    RT_CHECK_EQ((int)arcs.size(), 49);
    int nScobLocs = 0;
    for (int l = 0; l < PCR_LOC_COUNT; l++) {
        nScobLocs += pcr::location_info(l)->classes == pcr::PCR_CLASS_SCOB ? 1 : 0;
    }
    RT_CHECK_EQ(nScobLocs, 57);
    const pcr::Span<pcr::LocKey> dig = pcr::location_keys(PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL);
    RT_CHECK(dig.size == 1 && dig[0].field == pcr::DzxField::TagKbItem && dig[0].index == 58 &&
             strcmp(dig[0].arc, "Room44") == 0);

    for (int l = 0; l < PCR_LOC_COUNT; l++) {
        const pcr::Span<pcr::LocKey> ks = pcr::location_keys(l);
        sItems[l] = placedFor(l, ks.size > 0 ? ks[0].vanilla : 0);
    }

    // --- pc_rando() 0: nothing is written
    pc_rando_place_scob_reset();
    {
        Bytes b = makeRoom("Room44");
        const Bytes before = b;
        PcRandoResFile f = fileFor("sea", "Room44", b);
        pc_rando_place_scob_patch(&f);
        pc_rando_place_scob_needle_rock_patch(&f);
        RT_CHECK(b == before);
        RT_CHECK_EQ(pc_rando_place_scob_needle_rock(), -1);
    }

    pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);

    // --- every room: every key placed, only the field bits change
    int nPlaced = 0;
    for (const std::string& arc : arcs) {
        Bytes b = makeRoom(arc);
        const Bytes before = b;
        PcRandoResFile f = fileFor("sea", arc.c_str(), b);
        pc_rando_place_scob_patch(&f);
        PcRandoDzx dzx;
        RT_CHECK_EQ(pc_rando_dzx_open(&dzx, b.data(), b.size()), (int)PCR_DZX_OK);
        size_t changedBytes = 0;
        for (size_t i = 0; i < b.size(); i++) {
            changedBytes += b[i] != before[i] ? 1 : 0;
        }
        size_t expectChanged = 0;
        for (size_t i = 0; i < keys.size; i++) {
            const pcr::LocKey& k = keys[i];
            if (!isScob(k) || arc != k.arc) {
                continue;
            }
            const uint8_t* e = pc_rando_dzx_entry(&dzx, "SCOB", k.index);
            RT_CHECK(e != nullptr);
            if (e == nullptr) {
                continue;
            }
            const uint8_t want = sItems[k.loc];
            RT_CHECK_EQ(pcr::dzx_key_read(k, e), (uint32_t)want);
            RT_CHECK_EQ(pc_rando_place_scob_result((int)i), PCR_SCOB_PLACED);
            RT_CHECK_EQ(pc_rando_place_scob_item((int)i), want);
            // the rest of the params word is untouched
            RT_CHECK_EQ(get32(e + 8) | k.mask, 0xFFFFFFFFu);
            nPlaced++;
            // bytes that hold differing field bits
            const uint32_t diff = (get32(e + 8) ^ get32(&before[e - b.data()] + 8));
            for (int s = 0; s < 4; s++) {
                expectChanged += ((diff >> (24 - 8 * s)) & 0xFF) != 0 ? 1 : 0;
            }
        }
        RT_CHECK_EQ(changedBytes, expectChanged);
        // the same room's file under another stage or archive is not touched
        Bytes other = makeRoom(arc);
        const Bytes otherBefore = other;
        PcRandoResFile g = fileFor("Asoko", arc.c_str(), other);
        pc_rando_place_scob_patch(&g);
        RT_CHECK(other == otherBefore);
        PcRandoResFile h = fileFor("sea", arc.c_str(), other);
        h.type = PCR_RES_DZS;
        pc_rando_place_scob_patch(&h);
        RT_CHECK(other == otherBefore);
    }
    RT_CHECK_EQ(nPlaced, 204);

    // --- D9: name and item mismatches stay vanilla; a missing entry; the rest still placed
    {
        pc_rando_place_scob_reset();
        Bytes b = makeRoom("Room44");
        PcRandoDzx dzx;
        pc_rando_dzx_open(&dzx, b.data(), b.size());
        const pcr::Span<pcr::LocKey> ks = pcr::location_keys(PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE);
        RT_CHECK_EQ((int)ks.size, 4);
        uint8_t* e0 = pc_rando_dzx_entry(&dzx, "SCOB", ks[0].index);
        uint8_t* e1 = pc_rando_dzx_entry(&dzx, "SCOB", ks[1].index);
        e0[0] = 'X';                                  // replaced actor
        pcr::dzx_key_write(ks[1], e1, 0x01);          // replaced item
        const Bytes before = b;
        PcRandoResFile f = fileFor("sea", "Room44", b);
        pc_rando_place_scob_patch(&f);
        const int k0 = (int)(&ks[0] - keys.data), k1 = (int)(&ks[1] - keys.data), k2 = (int)(&ks[2] - keys.data);
        RT_CHECK_EQ(pc_rando_place_scob_result(k0), PCR_SCOB_MISMATCH);
        RT_CHECK_EQ(pc_rando_place_scob_result(k1), PCR_SCOB_MISMATCH);
        RT_CHECK_EQ(pc_rando_place_scob_item(k1), (uint8_t)0xFF);
        RT_CHECK(memcmp(e0, &before[e0 - b.data()], kScobSize) == 0);
        RT_CHECK_EQ(pcr::dzx_key_read(ks[1], e1), 0x01u);
        RT_CHECK_EQ(pc_rando_place_scob_result(k2), PCR_SCOB_PLACED);
        const pcr::LocKey& d = dig[0];
        const int kd = (int)(&d - keys.data);
        RT_CHECK_EQ(pc_rando_place_scob_result(kd), PCR_SCOB_PLACED);
        RT_CHECK_EQ(pcr::dzx_key_read(d, pc_rando_dzx_entry(&dzx, "SCOB", 58)),
                    (uint32_t)sItems[PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL]);

        // a room.dzr that ends before SCOB[58]
        Chunk c{"SCOB", {}};
        for (int i = 0; i < 30; i++) {
            c.entries.push_back(filler());
        }
        Bytes shortRoom = build({c});
        PcRandoResFile s = fileFor("sea", "Room44", shortRoom);
        pc_rando_place_scob_patch(&s);
        RT_CHECK_EQ(pc_rando_place_scob_result(kd), PCR_SCOB_MISSING);
        RT_CHECK_EQ(pc_rando_place_scob_result(k0), PCR_SCOB_MISMATCH); // index 19 exists, a filler
        // not a dzx at all: nothing is written, nothing crashes
        Bytes junk(16, 0xEE);
        PcRandoResFile j = fileFor("sea", "Room44", junk);
        pc_rando_place_scob_patch(&j);
        RT_CHECK(junk == Bytes(16, 0xEE));
    }

    // --- Needle Rock salvage flags
    {
        pc_rando_place_scob_reset();
        // as on the disc: [0] Salvag2 type 3 flag 0, [1] Salvag2 type 3 flag 1, then other actors,
        // type-0 salvages (flag = chart) and the gunboats (type 2, flags 0, 1, 2) after them.
        Chunk c{"SCOB", {}};
        c.entries.push_back(salvage("Salvag2", 3, 0, 0x05));
        c.entries.push_back(salvage("Salvag2", 3, 1, 0x04));
        c.entries.push_back(filler("agbCSW"));
        c.entries.push_back(salvage("Salvage", 0, 1, 0x0F));
        c.entries.push_back(salvage("SwSlvg", 2, 0, 0x05));
        c.entries.push_back(salvage("SwSlvg", 2, 1, 0x06));
        Bytes b = build({Chunk{"ACTR", {}}, c});
        Bytes before = b;
        PcRandoResFile f = fileFor("sea", "Room29", b);
        pc_rando_place_scob_needle_rock_patch(&f);
        RT_CHECK_EQ(pc_rando_place_scob_needle_rock(), 2);
        PcRandoDzx dzx;
        pc_rando_dzx_open(&dzx, b.data(), b.size());
        RT_CHECK_EQ(get32(pc_rando_dzx_entry(&dzx, "SCOB", 0) + 8), (3u << 28 | 8u << 20 | 0xFF000u | 0x05u << 4 | 1u));
        RT_CHECK_EQ(get32(pc_rando_dzx_entry(&dzx, "SCOB", 1) + 8), (3u << 28 | 9u << 20 | 0xFF000u | 0x04u << 4 | 1u));
        for (uint32_t i = 2; i < 6; i++) {
            RT_CHECK(memcmp(pc_rando_dzx_entry(&dzx, "SCOB", i), &before[pc_rando_dzx_entry(&dzx, "SCOB", i) - b.data()],
                            kScobSize) == 0);
        }
        // another room is not touched
        Bytes other = before;
        PcRandoResFile g = fileFor("sea", "Room30", other);
        pc_rando_place_scob_needle_rock_patch(&g);
        RT_CHECK(other == before);

        // across chunks (a layer chunk first in file order), skipping a non-salvage actor
        pc_rando_place_scob_reset();
        Chunk l{"SCO1", {salvage("SalvagN", 4, 1, 0x04), filler("Salvag2")}};
        Chunk d{"SCOB", {salvage("Salvage", 0, 0, 0x0F), salvage("SwSlvg", 2, 0, 0x05), salvage("Salvag2", 3, 0, 0x05)}};
        Bytes b2 = build({l, d});
        PcRandoResFile f2 = fileFor("sea", "Room29", b2);
        pc_rando_place_scob_needle_rock_patch(&f2);
        pc_rando_dzx_open(&dzx, b2.data(), b2.size());
        RT_CHECK_EQ((get32(pc_rando_dzx_entry(&dzx, "SCO1", 0) + 8) >> 20) & 0xFF, 8u);
        RT_CHECK_EQ((get32(pc_rando_dzx_entry(&dzx, "SCOB", 0) + 8) >> 20) & 0xFF, 0u);
        RT_CHECK_EQ((get32(pc_rando_dzx_entry(&dzx, "SCOB", 1) + 8) >> 20) & 0xFF, 9u);
        RT_CHECK_EQ((get32(pc_rando_dzx_entry(&dzx, "SCOB", 2) + 8) >> 20) & 0xFF, 0u);

        // one candidate only: left vanilla
        pc_rando_place_scob_reset();
        Chunk one{"SCOB", {salvage("Salvag2", 3, 0, 0x05), salvage("Salvage", 0, 1, 0x0F)}};
        Bytes b3 = build({one});
        const Bytes b3before = b3;
        PcRandoResFile f3 = fileFor("sea", "Room29", b3);
        pc_rando_place_scob_needle_rock_patch(&f3);
        RT_CHECK(b3 == b3before);
        RT_CHECK_EQ(pc_rando_place_scob_needle_rock(), -2);
    }

    // --- registered through the init, run by the dispatcher
    {
        pc_rando_place_scob_reset();
        const int before = pc_rando_res_patcher_count();
        pc_rando_place_scob_init();
        pc_rando_place_scob_init(); // once only
        RT_CHECK_EQ(pc_rando_res_patcher_count(), before + 2);
        Bytes b = makeRoom("Room29");
        // Room29's SCOB[0]/[1] are fillers here; put the two light rings there
        PcRandoDzx dzx;
        pc_rando_dzx_open(&dzx, b.data(), b.size());
        const Bytes r0 = salvage("Salvag2", 3, 0, 0x05), r1 = salvage("Salvag2", 3, 1, 0x04);
        memcpy(pc_rando_dzx_entry(&dzx, "SCOB", 0), r0.data(), kScobSize);
        memcpy(pc_rando_dzx_entry(&dzx, "SCOB", 1), r1.data(), kScobSize);
        PcRandoResFile f = fileFor("sea", "Room29", b);
        f.room = 29;
        pc_rando_res_dispatch(&f);
        RT_CHECK_EQ(pc_rando_place_scob_needle_rock(), 2);
        const pcr::Span<pcr::LocKey> gb = pcr::location_keys(PCR_LOC_NEEDLE_ROCK_ISLE_GOLDEN_GUNBOAT);
        RT_CHECK_EQ((int)gb.size, 1);
        RT_CHECK_EQ(pcr::dzx_key_read(gb[0], pc_rando_dzx_entry(&dzx, "SCOB", gb[0].index)),
                    (uint32_t)sItems[PCR_LOC_NEEDLE_ROCK_ISLE_GOLDEN_GUNBOAT]);
        RT_CHECK_EQ((get32(pc_rando_dzx_entry(&dzx, "SCOB", 1) + 8) >> 20) & 0xFF, 9u);
    }

    // --- the placement off again: vanilla items through pc_rando_loc_item
    pc_rando_deactivate();
    {
        Bytes b = makeRoom("Room44");
        const Bytes before = b;
        PcRandoResFile f = fileFor("sea", "Room44", b);
        pc_rando_place_scob_patch(&f);
        RT_CHECK(b == before);
    }
    return rt::finish();
}
