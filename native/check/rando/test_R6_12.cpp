// cos_rando_test_R6_12 (docs/RANDOMIZER_PLAN.md step R6.12): the entrance randomizer's stage data,
// without the game (the smoke `entr` checks every edited disc file against wwrando):
//   - the map of a seed: the fixture maps A-D (wwrando seeds and the hand-made Fire Mountain / Ice Ring
//     Isle swap) build, R0_6_identity.json (no entrance option) gives none and no error, an exit with
//     two entrances or an unknown name is an error; outermost entrances follow the nesting;
//   - entrStagePatch on synthetic files: the entrance's and the exit's SCLS (other bytes kept; a SCLS
//     that does not lead to its vanilla stage is refused, D9), the Savage Labyrinth sea SCLS, the 2DMA
//     sector (refused unless STAG says 11-13), the VolTag (kept, type changed or removed), the PLYR
//     spawn type 5 -> 1 in the room or in the stage.dzs, the (mini)boss stage.dzs SCLS[0] (the entrance
//     or the dungeon start);
//   - the archive patcher: nothing while pc_rando() is 0 or the seed has no entrance option; the
//     active seed's map otherwise, with its counters.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_entr_stage.h"
#include "pc_rando_entrances.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

struct Chunk {
    const char* tag;
    Bytes entries;
};

// A dzx file: count, 12-byte rows, then each chunk's entries.
Bytes dzx(const std::vector<Chunk>& chunks) {
    Bytes out(4 + chunks.size() * 12);
    put32(out.data(), (uint32_t)chunks.size());
    for (size_t i = 0; i < chunks.size(); i++) {
        uint8_t* row = out.data() + 4 + i * 12;
        memcpy(row, chunks[i].tag, 4);
        put32(row + 4, (uint32_t)(chunks[i].entries.size() / pc_rando_dzx_entry_size(chunks[i].tag)));
        put32(row + 8, (uint32_t)out.size());
        out.insert(out.end(), chunks[i].entries.begin(), chunks[i].entries.end());
    }
    return out;
}

Bytes scls(const char* stage, uint8_t room, uint8_t spawn, uint8_t fade = 3) {
    Bytes e(0x0C, 0);
    memcpy(e.data(), stage, strlen(stage));
    e[8] = spawn;
    e[9] = room;
    e[0x0A] = fade;
    e[0x0B] = 0xFF;
    return e;
}

Bytes actor(const char* name, uint32_t params, uint8_t zrotLo = 0) {
    Bytes e(0x20, 0);
    memcpy(e.data(), name, strlen(name));
    put32(e.data() + 8, params);
    e[0x0C] = 0x45; // some position bytes that must stay
    e[0x1D] = zrotLo;
    return e;
}

Bytes stag(int stageId) {
    Bytes e(0x14, 0);
    e[9] = (uint8_t)(stageId << 1);
    return e;
}

Bytes dma(uint8_t sector) {
    Bytes e(0x38, 0x11);
    e[0x36] = sector;
    return e;
}

Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}

const uint8_t* entry(Bytes& f, const char* tag, uint32_t i) {
    PcRandoDzx d;
    if (pc_rando_dzx_open(&d, f.data(), f.size()) != PCR_DZX_OK) {
        return nullptr;
    }
    return pc_rando_dzx_entry(&d, tag, i);
}

bool sclsIs(Bytes& f, uint32_t i, const char* stage, uint8_t room, uint8_t spawn) {
    const uint8_t* e = entry(f, "SCLS", i);
    return e != nullptr && pcr::entrSclsEquals(e, stage, room, spawn) && e[0x0A] == 3 && e[0x0B] == 0xFF;
}

uint32_t params(Bytes& f, const char* tag, uint32_t i) {
    const uint8_t* e = entry(f, tag, i);
    return e != nullptr ? pc_rando_dzx_be32(e + 8) : 0xDEADBEEFu;
}

bool loadSeed(const char* file, PcRandoSeed* seed) {
    std::string text, err;
    if (!RT_CHECK(rt::readFixture(file, text))) {
        return false;
    }
    if (!RT_CHECK(pc_rando_seed_parse(text, seed, &err))) {
        std::fprintf(stderr, "  %s: %s\n", file, err.c_str());
        return false;
    }
    return true;
}

std::string exitOf(const PcRandoSeed& s, const char* entrance) {
    for (const auto& [e, x] : s.entrances) {
        if (e == entrance) {
            return x;
        }
    }
    return "";
}

pcr::EntrStagePatchStats patch(const pcr::EntrStageMap& m, const char* stage, int room, Bytes& f,
                               int stagePlyr = 0) {
    pcr::EntrStagePatchStats st;
    RT_CHECK(pcr::entrStagePatch(m, stage, room, f.data(), f.size(), stagePlyr, &st));
    return st;
}

void checkMaps(PcRandoSeed* seeds, pcr::EntrStageMap* maps) {
    const char* files[] = {"R6_12_seed_A.json", "R6_12_seed_B.json", "R6_12_seed_C.json", "R6_12_seed_D.json"};
    for (int k = 0; k < 4; k++) {
        std::string err;
        if (loadSeed(files[k], &seeds[k])) {
            RT_CHECK(pcr::entrStageMapFromSeed(seeds[k], maps[k], &err));
            if (!err.empty()) {
                std::fprintf(stderr, "  %s: %s\n", files[k], err.c_str());
            }
        }
    }
    // Every exit's entrance is the one the seed names; outermost entrances are island entrances.
    for (int k = 0; k < 4; k++) {
        for (const auto& [e, x] : seeds[k].entrances) {
            RT_CHECK_EQ(maps[k].conn.exitToEntrance[pcr::zoneExitIndex(x)], pcr::zoneEntranceIndex(e));
        }
        for (int x = 0; x < pcr::kNumZoneExits; x++) {
            RT_CHECK(pcr::zoneEntrances()[maps[k].outermost[x]].islandName != nullptr);
        }
    }
    const int fireCave = pcr::zoneExitIndex("Fire Mountain Secret Cave");
    RT_CHECK_EQ(maps[3].outermost[fireCave], pcr::zoneEntranceIndex("Secret Cave Entrance on Ice Ring Isle"));
    // Map C: the Hyrule Castle miniboss entrance leads into Dragon Roost Cavern, whose boss entrance
    // leads into Wind Temple: Wind Temple's outermost entrance is Hyrule's.
    RT_CHECK_EQ(exitOf(seeds[2], "Boss Entrance in Dragon Roost Cavern"), std::string("Wind Temple"));
    RT_CHECK_EQ(maps[2].outermost[pcr::zoneExitIndex("Wind Temple")],
                pcr::zoneEntranceIndex("Miniboss Entrance in Hyrule Castle"));

    PcRandoSeed v;
    pcr::EntrStageMap vm;
    std::string err = "x";
    if (loadSeed("R0_6_identity.json", &v)) {
        RT_CHECK(!pcr::entrStageMapFromSeed(v, vm, &err));
        RT_CHECK(err.empty());
    }
    PcRandoSeed bad = seeds[3];
    for (auto& [e, x] : bad.entrances) {
        if (e == "Secret Cave Entrance on Ice Ring Isle") {
            x = "Ice Ring Isle Secret Cave"; // now Fire Mountain's entrance leads there too
        }
    }
    RT_CHECK(!pcr::entrStageMapFromSeed(bad, vm, &err));
    RT_CHECK(err.find("two entrances") != std::string::npos);
    bad = seeds[3];
    bad.entrances[0].second = "Nowhere";
    RT_CHECK(!pcr::entrStageMapFromSeed(bad, vm, &err));
    RT_CHECK(err.find("unknown") != std::string::npos);
}

void checkPatch(const PcRandoSeed* seeds, const pcr::EntrStageMap* maps) {
    const pcr::EntrStageMap& A = maps[0];
    const pcr::EntrStageMap& B = maps[1];
    const pcr::EntrStageMap& D = maps[3];

    // Savage Labyrinth's exit (Cave09/Room0 SCLS[1]); A: Bomb Island's entrance (sea 34 spawn 1).
    RT_CHECK_EQ(exitOf(seeds[0], "Secret Cave Entrance on Bomb Island"), std::string("Savage Labyrinth"));
    Bytes cave = dzx({{"SCLS", cat({scls("Cave10", 0, 0), scls("sea", 44, 10)})}, {"ACTR", actor("kanban", 7)}});
    const Bytes caveRaw = cave;
    pcr::EntrStagePatchStats st = patch(A, "Cave09", 0, cave);
    RT_CHECK_EQ(st.written, 1);
    RT_CHECK_EQ(st.refused, 0);
    RT_CHECK(sclsIs(cave, 1, "sea", 34, 1));
    RT_CHECK(memcmp(entry(cave, "SCLS", 0), scls("Cave10", 0, 0).data(), 0x0C) == 0);
    RT_CHECK(memcmp(entry(cave, "ACTR", 0), actor("kanban", 7).data(), 0x20) == 0);
    // D9: a SCLS that leads elsewhere is left alone.
    Bytes odd = dzx({{"SCLS", cat({scls("Cave10", 0, 0), scls("Abesso", 0, 1)})}});
    const Bytes oddRaw = odd;
    st = patch(A, "Cave09", 0, odd);
    RT_CHECK_EQ(st.refused, 1);
    RT_CHECK(odd == oddRaw);
    // A second patch of the patched buffer writes the same (already the target).
    st = patch(A, "Cave09", 0, cave);
    RT_CHECK_EQ(st.refused, 0);
    RT_CHECK(sclsIs(cave, 1, "sea", 34, 1));
    // Other stages and rooms are not touched.
    Bytes other = caveRaw;
    st = patch(A, "Cave09", 1, other);
    RT_CHECK_EQ(st.written + st.refused, 0);
    RT_CHECK(other == caveRaw);

    // Savage's other sea exits (Cave10/Room0: SCLS[1] only).
    Bytes c10 = dzx({{"SCLS", cat({scls("Cave10", 1, 0), scls("sea", 44, 10)})}});
    st = patch(A, "Cave10", 0, c10);
    RT_CHECK_EQ(st.written, 1);
    RT_CHECK(sclsIs(c10, 0, "Cave10", 1, 0));
    RT_CHECK(sclsIs(c10, 1, "sea", 34, 1));

    // The sector (Cave09 stage.dzs); A: Bomb Island is island 34: (33%7-3, 33/7-3) = (2, 1) -> 0x12.
    Bytes cs = dzx({{"STAG", stag(13)}, {"2DMA", dma(0xD3)}});
    st = patch(A, "Cave09", -1, cs);
    RT_CHECK_EQ(st.written, 1);
    RT_CHECK_EQ(entry(cs, "2DMA", 0)[0x36], 0x12);
    RT_CHECK_EQ(entry(cs, "2DMA", 0)[0x35], 0x11);
    Bytes cs3 = dzx({{"STAG", stag(3)}, {"2DMA", dma(0xD3)}});
    st = patch(A, "Cave09", -1, cs3);
    RT_CHECK_EQ(st.refused, 1);
    RT_CHECK_EQ(entry(cs3, "2DMA", 0)[0x36], 0xD3);

    // MiniKaz/Room0: VolTag removed (A: Private Oasis' entrance), type 2 (D: Ice Ring Isle's).
    const Bytes kazRaw = dzx({{"SCLS", scls("sea", 20, 1)},
                              {"ACTR", cat({actor("kytag00", 1), actor("VolTag", 0x001EFF40)})}});
    Bytes kaz = kazRaw;
    st = patch(A, "MiniKaz", 0, kaz);
    RT_CHECK(sclsIs(kaz, 0, "Abesso", 0, 1));
    RT_CHECK(entry(kaz, "ACTR", 1)[0] == 0);
    RT_CHECK_EQ(params(kaz, "ACTR", 1), 0x001EFF40u);
    kaz = kazRaw;
    st = patch(D, "MiniKaz", 0, kaz);
    RT_CHECK(sclsIs(kaz, 0, "sea", 40, 0));
    RT_CHECK(memcmp(entry(kaz, "ACTR", 1), "VolTag", 6) == 0);
    RT_CHECK_EQ(params(kaz, "ACTR", 1), 0x001EFF80u);
    // Its own entrance: VolTag kept; the SCLS still gets the entrance's spawn (sea 20 spawn 0).
    PcRandoSeed own = seeds[3];
    for (auto& [e, x] : own.entrances) {
        if (e == "Secret Cave Entrance on Fire Mountain") {
            x = "Fire Mountain Secret Cave";
        } else if (e == "Secret Cave Entrance on Ice Ring Isle") {
            x = "Ice Ring Isle Secret Cave";
        }
    }
    pcr::EntrStageMap ownMap;
    RT_CHECK(pcr::entrStageMapFromSeed(own, ownMap, nullptr));
    kaz = kazRaw;
    st = patch(ownMap, "MiniKaz", 0, kaz);
    RT_CHECK_EQ(st.written, 1);
    RT_CHECK(sclsIs(kaz, 0, "sea", 20, 0));
    RT_CHECK_EQ(params(kaz, "ACTR", 1), 0x001EFF40u);
    RT_CHECK(memcmp(entry(kaz, "ACTR", 1), "VolTag", 6) == 0);
    // MiniHyo with D: Fire Mountain's entrance -> type 1 (0x1EFF81 -> 0x1EFF41), on a layer chunk.
    Bytes hyo = dzx({{"SCLS", cat({scls("sea", 40, 1), scls("ITest62", 0, 0)})},
                     {"ACTR", actor("kytag00", 1)}, {"ACT0", actor("VolTag", 0x001EFF81)}});
    st = patch(D, "MiniHyo", 0, hyo);
    RT_CHECK_EQ(st.refused, 0);
    RT_CHECK(sclsIs(hyo, 0, "sea", 20, 0));
    RT_CHECK(sclsIs(hyo, 1, "ITest62", 0, 0));
    RT_CHECK_EQ(params(hyo, "ACT0", 0), 0x001EFF41u);

    // The DRI spawn (Adanmae/Room0, spawn 2, type 5 -> 1) in the room when the stage has no PLYR.
    const Bytes adRaw = dzx({{"SCLS", cat({scls("sea", 13, 0), scls("sea", 13, 1), scls("M_NewD2", 0, 0)})},
                             {"PLYR", cat({actor("Link", 0xFFFF2000, 1), actor("Link", 0xFFFF5000, 2),
                                           actor("Link", 0xFFFF5000, 2)})}});
    Bytes ad = adRaw;
    st = patch(A, "Adanmae", 0, ad, 0);
    RT_CHECK_EQ(st.refused, 0);
    RT_CHECK_EQ(params(ad, "PLYR", 1), 0xFFFF1000u);
    RT_CHECK_EQ(params(ad, "PLYR", 2), 0xFFFF5000u); // only the first with the spawn id
    RT_CHECK_EQ(params(ad, "PLYR", 0), 0xFFFF2000u);
    RT_CHECK(sclsIs(ad, 2, "kenroom", 0, 0)); // A: DRI's dungeon entrance -> Master Sword Chamber
    ad = adRaw;
    st = patch(A, "Adanmae", 0, ad, 3); // the stage.dzs holds the spawns: the room's are left
    RT_CHECK_EQ(params(ad, "PLYR", 1), 0xFFFF5000u);
    Bytes ads = dzx({{"PLYR", cat({actor("Link", 0xFFFF5000, 2)})}});
    st = patch(A, "Adanmae", -1, ads);
    RT_CHECK_EQ(params(ads, "PLYR", 0), 0xFFFF1000u);
    // A missing spawn is counted, not refused.
    Bytes nos = dzx({{"SCLS", cat({scls("sea", 13, 0), scls("sea", 13, 1), scls("M_NewD2", 0, 0)})}});
    st = patch(A, "Adanmae", 0, nos, 0);
    RT_CHECK_EQ(st.noSpawn, 1);
    RT_CHECK_EQ(st.refused, 0);

    // (Mini)boss save-and-reload, kenroom stage.dzs SCLS[0]: B: Cliff Plateau's inner entrance (an
    // island-side entrance: itself); D: the Hyrule miniboss entrance (the dungeon start, Hyroom's
    // SCLS[0]).
    RT_CHECK_EQ(exitOf(seeds[1], "Inner Entrance in Cliff Plateau Isles Secret Cave"),
                std::string("Master Sword Chamber"));
    const Bytes kenRaw = dzx({{"SCLS", cat({scls("kenroom", 0, 10), scls("Hyroom", 0, 2)})}});
    Bytes ken = kenRaw;
    st = patch(B, "kenroom", -1, ken);
    RT_CHECK(sclsIs(ken, 0, "Cave03", 0, 1));
    RT_CHECK(sclsIs(ken, 1, "Hyroom", 0, 2));
    ken = kenRaw;
    st = patch(D, "kenroom", -1, ken);
    RT_CHECK(sclsIs(ken, 0, "Hyroom", 0, 10));
    const pcr::EntrSclsDest* ds = pcr::entrStageDungeonStart("kaze");
    RT_CHECK(ds != nullptr && strcmp(ds->stage, "kaze") == 0 && ds->room == 255 && ds->spawn == 15);
    RT_CHECK(pcr::entrStageDungeonStart("Cave09") == nullptr);

    // A file that does not parse.
    Bytes junk(8, 0xFF);
    RT_CHECK(!pcr::entrStagePatch(A, "Cave09", 0, junk.data(), junk.size(), 0, nullptr));
}

void* allocNone(size_t, void*) {
    return nullptr;
}

PcRandoResFile resFile(const char* stage, int room, Bytes& data, PcRandoResNames& names) {
    PcRandoResFile f = {};
    char arc[16];
    if (room < 0) {
        snprintf(arc, sizeof arc, "Stage");
    } else {
        snprintf(arc, sizeof arc, "Room%d", room);
    }
    char dir[40];
    snprintf(dir, sizeof dir, "/res/Stage/%s/", stage);
    pc_rando_res_describe(&f, &names, arc, dir);
    f.name = room < 0 ? "stage.dzs" : "room.dzr";
    f.type = room < 0 ? PCR_RES_DZS : PCR_RES_DZR;
    f.data = data.data();
    f.size = data.size();
    f.alloc = allocNone;
    return f;
}

void checkRuntime(const PcRandoSeed* seeds) {
    const Bytes stageRaw = dzx({{"STAG", stag(13)}, {"2DMA", dma(0xD3)}});
    const Bytes roomRaw = dzx({{"SCLS", cat({scls("Cave10", 0, 0), scls("sea", 44, 10)})}});
    PcRandoResNames n1, n2;
    pcr::entrStageRuntimeReset();
    // pc_rando() 0: nothing.
    Bytes s = stageRaw, r = roomRaw;
    PcRandoResFile fs = resFile("Cave09", -1, s, n1), fr = resFile("Cave09", 0, r, n2);
    pc_rando_entr_stage_patch(&fs);
    pc_rando_entr_stage_patch(&fr);
    RT_CHECK(s == stageRaw && r == roomRaw);
    RT_CHECK_EQ(pcr::entrStageRuntimeStats().files + pcr::entrStageRuntimeStats().noMap, 0);
    RT_CHECK(pcr::entrStageActiveMap() == nullptr);
    // A seed without entrance rando: no map.
    PcRandoSeed v;
    if (loadSeed("R0_6_identity.json", &v)) {
        pc_rando_set_active(&v, nullptr);
        pc_rando_entr_stage_patch(&fs);
        pc_rando_entr_stage_patch(&fr);
        RT_CHECK(s == stageRaw && r == roomRaw);
        RT_CHECK_EQ(pcr::entrStageRuntimeStats().noMap, 2);
        RT_CHECK_EQ(pcr::entrStageRuntimeStats().files, 0);
        pc_rando_deactivate();
    }
    // Seed A: its map, stage.dzs then room.dzr.
    pc_rando_set_active(&seeds[0], nullptr);
    RT_CHECK(pcr::entrStageActiveMap() != nullptr);
    pc_rando_entr_stage_patch(&fs);
    pc_rando_entr_stage_patch(&fr);
    RT_CHECK_EQ(s[s.size() - 0x38 + 0x36], 0x12);
    RT_CHECK(sclsIs(r, 1, "sea", 34, 1));
    const pcr::EntrStageRuntimeStats& rs = pcr::entrStageRuntimeStats();
    RT_CHECK_EQ(rs.files, 2);
    RT_CHECK_EQ(rs.patched, 2);
    RT_CHECK_EQ(rs.written, 2);
    RT_CHECK_EQ(rs.refused + rs.noSpawn + rs.badFiles + rs.startMismatches, 0);
    // Another type of file is not touched.
    Bytes ev = roomRaw;
    PcRandoResNames n3;
    PcRandoResFile fe = resFile("Cave09", 0, ev, n3);
    fe.name = "event_list.dat";
    fe.type = PCR_RES_DAT;
    pc_rando_entr_stage_patch(&fe);
    RT_CHECK(ev == roomRaw);
    // A dungeon start stage.dzs that is not the constant is counted.
    Bytes kz = dzx({{"SCLS", scls("kaze", 2, 15)}, {"PLYR", actor("Link", 0xFFFF0000, 0)}});
    PcRandoResNames n4;
    PcRandoResFile fk = resFile("kaze", -1, kz, n4);
    pc_rando_entr_stage_patch(&fk);
    RT_CHECK_EQ(pcr::entrStageRuntimeStats().startMismatches, 1);
    pc_rando_deactivate();
    RT_CHECK(pcr::entrStageActiveMap() == nullptr);
}

} // namespace

int main() {
    static PcRandoSeed seeds[4];
    static pcr::EntrStageMap maps[4];
    checkMaps(seeds, maps);
    checkPatch(seeds, maps);
    checkRuntime(seeds);
    return rt::finish();
}
