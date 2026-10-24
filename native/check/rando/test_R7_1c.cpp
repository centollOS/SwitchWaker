// Unit test of step R7.1c (docs/RANDOMIZER_PLAN.md): the pure part of the inter-dungeon warp pots
// (features/rando/pc_rando_tweak_warp_pots.h) on synthetic stage files (no disc bytes): the table
// (INTER_DUNGEON_WARP_DATA), the Warpts params, the entries a room.dzr / stage.dzs gains, the D9
// refusal of a file that already has a spawn 69, and the patcher's option gate. The real rooms are
// checked in game by the warppots smoke.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_tweak_warp_pots.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

struct Chunk {
    const char* tag;
    uint32_t count;
    uint32_t esize;
    uint8_t fill;
};

// A dzx file: the chunk table, then each chunk's entries (filled with `fill`, a PLYR's spawn id at
// +0x1D set to `spawnId`).
std::vector<uint8_t> makeDzx(const std::vector<Chunk>& chunks, uint8_t spawnId) {
    uint32_t off = 4 + 0xC * (uint32_t)chunks.size();
    off = (off + 0x1F) & ~0x1Fu;
    std::vector<uint8_t> f(off, 0);
    put32(f.data(), (uint32_t)chunks.size());
    for (size_t c = 0; c < chunks.size(); c++) {
        uint8_t* row = f.data() + 4 + 0xC * c;
        memcpy(row, chunks[c].tag, 4);
        put32(row + 4, chunks[c].count);
        put32(row + 8, (uint32_t)f.size());
        for (uint32_t i = 0; i < chunks[c].count; i++) {
            std::vector<uint8_t> e(chunks[c].esize, chunks[c].fill);
            if (strcmp(chunks[c].tag, "PLYR") == 0) {
                e[0x1D] = spawnId;
            }
            f.insert(f.end(), e.begin(), e.end());
        }
    }
    while (f.size() % 0x20 != 0) {
        f.push_back(0xFF);
    }
    return f;
}

void* testAlloc(size_t size, void*) {
    return calloc(1, size);
}

} // namespace

int main() {
    // The table.
    const char* stages[6] = {"M_NewD2", "kindan", "Siren", "ma2room", "M_Dai", "kaze"};
    const int rooms[6] = {2, 1, 6, 2, 1, 3};
    for (int i = 0; i < PCR_WARP_POT_COUNT; i++) {
        const PcRandoWarpPot* p = pc_rando_warp_pot(i);
        RT_CHECK(p != nullptr);
        RT_CHECK_EQ(std::string(p->stage), std::string(stages[i]));
        RT_CHECK_EQ(p->room, rooms[i]);
        RT_CHECK_EQ((int)p->eventReg, i < 3 ? 2 : 5);
        RT_CHECK_EQ((int)p->spawnInStage, i >= 4 ? 1 : 0);
        RT_CHECK_EQ(pc_rando_warp_pot_find(stages[i], rooms[i]), i);
        RT_CHECK_EQ(pc_rando_warp_pot_next_dest(i), (i % 3 + 1) % 3);
    }
    RT_CHECK(pc_rando_warp_pot(6) == nullptr);
    RT_CHECK_EQ(pc_rando_warp_pot_find("M_NewD2", 3), -1);
    RT_CHECK_EQ(pc_rando_warp_pot(1)->y, (float)3956.43);
    RT_CHECK_EQ((int)pc_rando_warp_pot(1)->yRot, 0xB929);
    // Warpts params: type k+2, register << 4, exits << 8/16/24.
    RT_CHECK_EQ(pc_rando_warp_pot_params(0, 3), 0x05040322u);
    RT_CHECK_EQ(pc_rando_warp_pot_params(5, 1), 0x03020154u);
    char name[8];
    pc_rando_warp_pot_name(2, name);
    RT_CHECK(memcmp(name, "Warpts3\0", 8) == 0);
    RT_CHECK_EQ(pc_rando_warp_pot_particle(0), (uint16_t)0x8161);
    RT_CHECK_EQ(pc_rando_warp_pot_particle(4), (uint16_t)0x8112);

    // A room.dzr of DRC with 3 SCLS, 2 ACTR, 1 PLYR: +1 PLYR, +3 SCLS, +1 ACTR at the chunk ends.
    std::vector<uint8_t> drc = makeDzx({{"SCLS", 3, 0xC, 0x11}, {"ACTR", 2, 0x20, 0x22}, {"PLYR", 1, 0x20, 0x33}}, 0);
    size_t cap = pc_rando_warp_pots_grown_size("M_NewD2", 2, drc.data(), drc.size());
    RT_CHECK(cap > drc.size());
    std::vector<uint8_t> out(cap);
    size_t outSize = 0;
    RT_CHECK_EQ(pc_rando_warp_pots_apply("M_NewD2", 2, drc.data(), drc.size(), out.data(), cap, &outSize),
                (int)PCR_WARP_POTS_PATCHED);
    PcRandoDzx dzx;
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, out.data(), outSize), (int)PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "SCLS"), 6u);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "ACTR"), 3u);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "PLYR"), 2u);
    const uint8_t* s3 = pc_rando_dzx_entry(&dzx, "SCLS", 3);
    const uint8_t* s4 = pc_rando_dzx_entry(&dzx, "SCLS", 4);
    const uint8_t* s5 = pc_rando_dzx_entry(&dzx, "SCLS", 5);
    RT_CHECK(memcmp(s3, "M_NewD2\0", 8) == 0 && s3[8] == 69 && s3[9] == 2 && s3[10] == 4 && s3[11] == 0xFF);
    RT_CHECK(memcmp(s4, "kindan\0\0", 8) == 0 && s4[9] == 1);
    RT_CHECK(memcmp(s5, "Siren\0\0\0", 8) == 0 && s5[9] == 6);
    RT_CHECK(pc_rando_dzx_entry(&dzx, "SCLS", 2)[0] == 0x11); // the vanilla entries keep their bytes
    const uint8_t* a = pc_rando_dzx_entry(&dzx, "ACTR", 2);
    RT_CHECK(memcmp(a, "Warpts1\0", 8) == 0);
    RT_CHECK_EQ(pc_rando_dzx_be32(a + 8), 0x05040322u);
    RT_CHECK_EQ(pc_rando_dzx_be16(a + 0x18), (uint16_t)0xFFFF);
    RT_CHECK_EQ(pc_rando_dzx_be16(a + 0x1A), (uint16_t)0xA000);
    RT_CHECK_EQ(pc_rando_dzx_be16(a + 0x1C), (uint16_t)0xFFFF);
    RT_CHECK_EQ(pc_rando_dzx_be16(a + 0x1E), (uint16_t)0xFFFF);
    RT_CHECK_EQ(pc_rando_dzx_bef32(a + 0x0C), 2185.0f);
    const uint8_t* pl = pc_rando_dzx_entry(&dzx, "PLYR", 1);
    RT_CHECK(memcmp(pl, "Link\0\0\0\0", 8) == 0);
    RT_CHECK_EQ(pc_rando_dzx_be32(pl + 8), 0xFFFF7002u); // spawn type 7, room 2
    RT_CHECK_EQ(pc_rando_dzx_be16(pl + 0x1C), (uint16_t)0xFF45); // ship_id 0xFF, spawn 69
    RT_CHECK_EQ(pc_rando_dzx_be16(pl + 0x1E), (uint16_t)0xFFFF);
    // DRC's stage.dzs: nothing to add.
    RT_CHECK_EQ(pc_rando_warp_pots_apply("M_NewD2", -1, drc.data(), drc.size(), out.data(), cap, &outSize),
                (int)PCR_WARP_POTS_NONE);

    // WT: the room has no PLYR chunk and gets none; the stage.dzs gets the spawn 69 (room 3).
    std::vector<uint8_t> wtRoom = makeDzx({{"SCLS", 1, 0xC, 0x11}, {"ACTR", 1, 0x20, 0x22}}, 0);
    cap = pc_rando_warp_pots_grown_size("kaze", 3, wtRoom.data(), wtRoom.size());
    out.assign(cap, 0);
    RT_CHECK_EQ(pc_rando_warp_pots_apply("kaze", 3, wtRoom.data(), wtRoom.size(), out.data(), cap, &outSize),
                (int)PCR_WARP_POTS_PATCHED);
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, out.data(), outSize), (int)PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "SCLS"), 4u);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "PLYR"), 0u);
    RT_CHECK_EQ(pc_rando_dzx_be32(pc_rando_dzx_entry(&dzx, "ACTR", 1) + 8), 0x03020154u);
    std::vector<uint8_t> wtStage = makeDzx({{"PLYR", 2, 0x20, 0x33}}, 5);
    cap = pc_rando_warp_pots_grown_size("kaze", -1, wtStage.data(), wtStage.size());
    out.assign(cap, 0);
    RT_CHECK_EQ(pc_rando_warp_pots_apply("kaze", -1, wtStage.data(), wtStage.size(), out.data(), cap, &outSize),
                (int)PCR_WARP_POTS_PATCHED);
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, out.data(), outSize), (int)PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "PLYR"), 3u);
    RT_CHECK_EQ(pc_rando_dzx_be32(pc_rando_dzx_entry(&dzx, "PLYR", 2) + 8), 0xFFFF7003u);

    // D9: a file that already has a spawn 69 is refused.
    std::vector<uint8_t> bad = makeDzx({{"SCLS", 1, 0xC, 0x11}, {"PLYR", 1, 0x20, 0x33}}, 69);
    out.assign(bad.size() + 0x200, 0);
    RT_CHECK_EQ(pc_rando_warp_pots_grown_size("Siren", 6, bad.data(), bad.size()), (size_t)0);
    RT_CHECK_EQ(pc_rando_warp_pots_apply("Siren", 6, bad.data(), bad.size(), out.data(), out.size(), &outSize),
                (int)PCR_WARP_POTS_REFUSED);

    // The patcher's gate: nothing without pc_rando(), nothing with the option off, the edit with it on.
    PcRandoConfig cfg;
    pc_rando_config_defaults(&cfg);
    RT_CHECK_EQ((int)cfg.inter_dungeon_warps, 0);
    auto runPatcher = [&](std::vector<uint8_t>& bytes) {
        uint8_t* copy = (uint8_t*)malloc(bytes.size());
        memcpy(copy, bytes.data(), bytes.size());
        PcRandoResFile f = {};
        PcRandoResNames names = {};
        pc_rando_res_describe(&f, &names, "Room2", "/res/Stage/M_NewD2/");
        f.name = "room.dzr";
        f.type = PCR_RES_DZR;
        f.data = copy;
        f.size = bytes.size();
        f.alloc = testAlloc;
        pc_rando_tweak_warp_pots_patch(&f);
        std::vector<uint8_t> r(f.data, f.data + f.size);
        if (f.data != copy) {
            free(f.data);
        }
        free(copy);
        return r;
    };
    pc_rando_warp_pots_reset_status();
    RT_CHECK(runPatcher(drc) == drc);
    pc_rando_set_active(nullptr, &cfg);
    RT_CHECK(runPatcher(drc) == drc);
    RT_CHECK_EQ(pc_rando_warp_pots_skipped(), 2u);
    cfg.inter_dungeon_warps = 1;
    pc_rando_set_active(nullptr, &cfg);
    std::vector<uint8_t> patched = runPatcher(drc);
    cap = pc_rando_warp_pots_grown_size("M_NewD2", 2, drc.data(), drc.size());
    out.assign(cap, 0);
    pc_rando_warp_pots_apply("M_NewD2", 2, drc.data(), drc.size(), out.data(), cap, &outSize);
    RT_CHECK(patched.size() == outSize && memcmp(patched.data(), out.data(), outSize) == 0);
    PcRandoWarpPotStatus st;
    pc_rando_warp_pots_status(0, &st);
    RT_CHECK_EQ(st.roomPatched, 1u);
    RT_CHECK_EQ(st.firstScls, 3u);
    pc_rando_deactivate();
    return rt::finish();
}
