// cos_rando_test_R6_13 (docs/RANDOMIZER_PLAN.md step R6.13): boss warp-out destinations under
// entrance rando (features/rando/pc_rando_warpout.h) without the game, against wwrando's own
// EntranceRandomizer.update_all_boss_warp_out_destinations (fixture R6_13_warpout.txt,
// native/tools/rando/gen_warpout_vectors.py):
//   - per case (every entrance option alone, all separate and mixed, required bosses, dungeons-only
//     starts, and one with every entrance option off): a seed with the case's options and
//     entrance_connections gives, for each boss arena stage, wwrando's WARP_WIND_AFTER Stage/RoomNo/
//     StartCode and the Helmaroc King SCLS[1] destination; with entrances off, nothing;
//   - the M2tower/Room0 room.dzr patcher on a synthetic room: SCLS[1] rewritten (from R3.15's "sea"
//     and from the disc's "M2ganon"), the other bytes unchanged; D9: another destination, a missing
//     SCLS[1], another stage or room, a seed without entrance rando: left byte-identical;
//   - a seed with an unknown entrance name, NULL arguments, a stage that is not a boss arena: nothing.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_entrances.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"
#include "pc_rando_warpout.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* const kBossStages[] = {"M_DragB", "kinBOSS", "SirenB", "M2tower", "M_DaiB", "kazeB"};

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

struct Expect {
    std::string bossStage, stage;
    int room, spawn;
};

struct Case {
    std::string name;
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<std::string> conn;
    std::vector<Expect> expect; // warp lines, then the scls line as bossStage "M2tower"
    bool disabled = false;
    bool error = false;
};

bool loadCases(std::vector<Case>& cases) {
    std::string text;
    if (!rt::readFixture("R6_13_warpout.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string ln;
    Case* c = nullptr;
    bool ended = false;
    while (std::getline(in, ln)) {
        const std::vector<std::string> f = split(ln, '\t');
        if (f[0] == "version") {
            RT_CHECK_EQ(f[1], std::string("1.10.0_9775811"));
        } else if (f[0] == "case") {
            cases.push_back(Case{});
            c = &cases.back();
            c->name = f[1];
        } else if (c == nullptr) {
            if (f[0] == "end") {
                ended = true;
            }
        } else if (f[0] == "opt") {
            c->opts.emplace_back(f[1], f[2]);
        } else if (f[0] == "conn") {
            c->conn = split(f[1], '|');
        } else if (f[0] == "warp") {
            c->expect.push_back(Expect{f[1], f[2], atoi(f[3].c_str()), atoi(f[4].c_str())});
        } else if (f[0] == "scls") {
            c->expect.push_back(Expect{"M2tower", f[1], atoi(f[2].c_str()), atoi(f[3].c_str())});
        } else if (f[0] == "disabled") {
            c->disabled = true;
        } else if (f[0] == "error" || f[0] == "initerror") {
            c->error = true;
        } else if (f[0] == "endcase") {
            c = nullptr;
        }
    }
    return ended;
}

// A placement with the case's options (bools as JSON booleans, as a seed file holds them) and its
// entrance map ("entrances": entrance name -> exit name, zoneEntrances() order).
bool seedOf(const Case& c, PcRandoSeed& seed) {
    for (const auto& kv : c.opts) {
        if (kv.second == "1" || kv.second == "0") {
            seed.options.set(kv.first, pcr::Json::boolean(kv.second == "1"));
        } else {
            seed.options.set(kv.first, pcr::Json::string(kv.second));
        }
    }
    if (!RT_CHECK_EQ(c.conn.size(), (size_t)pcr::kNumZoneEntrances)) {
        return false;
    }
    for (int i = 0; i < pcr::kNumZoneEntrances; i++) {
        seed.entrances.emplace_back(pcr::zoneEntrances()[(size_t)i].name, c.conn[(size_t)i]);
    }
    return true;
}

void testCases() {
    std::vector<Case> cases;
    RT_CHECK(loadCases(cases));
    RT_CHECK(cases.size() >= 20);
    int checked = 0, disabled = 0;
    for (const Case& c : cases) {
        if (c.error) {
            continue; // wwrando raised: no placement exists
        }
        PcRandoSeed seed;
        if (!seedOf(c, seed)) {
            continue;
        }
        if (c.disabled) {
            disabled++;
            RT_CHECK_EQ(pc_rando_warpout_enabled(&seed), 0);
            for (const char* st : kBossStages) {
                PcRandoWarpOut w = {};
                if (!RT_CHECK_EQ(pc_rando_warpout_dest(&seed, st, &w), 0)) {
                    fprintf(stderr, "  case %s stage %s\n", c.name.c_str(), st);
                }
            }
            continue;
        }
        RT_CHECK_EQ(pc_rando_warpout_enabled(&seed), 1);
        if (!RT_CHECK_EQ(c.expect.size(), (size_t)6)) {
            fprintf(stderr, "  case %s\n", c.name.c_str());
        }
        for (const Expect& e : c.expect) {
            PcRandoWarpOut w = {};
            const int got = pc_rando_warpout_dest(&seed, e.bossStage.c_str(), &w);
            const bool ok = got == 1 && w.stage != nullptr && e.stage == w.stage && e.room == w.room &&
                            e.spawn == w.spawn;
            if (!RT_CHECK(ok)) {
                fprintf(stderr, "  case %s %s: want %s %d %d, got %d %s %d %d\n", c.name.c_str(),
                        e.bossStage.c_str(), e.stage.c_str(), e.room, e.spawn, got,
                        w.stage != nullptr ? w.stage : "(null)", w.room, w.spawn);
            }
            checked++;
        }
    }
    RT_CHECK_EQ(disabled, 1);
    RT_CHECK(checked >= 20 * 6);
}

// --- the M2tower/Room0 patcher on a synthetic room.dzr ---------------------------------------------

using Bytes = std::vector<uint8_t>;

void put32(Bytes& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)(v >> 24);
    b[off + 1] = (uint8_t)(v >> 16);
    b[off + 2] = (uint8_t)(v >> 8);
    b[off + 3] = (uint8_t)v;
}

// A room.dzr with an SCLS chunk of `n` entries: [0] ma2room 17/0, [1] `door` spawn 0 room `doorRoom`,
// and a trailing FILI-like chunk after it, so writes outside SCLS[1] would show.
Bytes makeRoom(int n, const char* door, uint8_t doorRoom) {
    const uint32_t sclsOff = 4 + 12 * 2;
    Bytes b(sclsOff + 0xC * (size_t)n + 8, 0xAB);
    put32(b, 0, 2);
    memcpy(&b[4], "SCLS", 4);
    put32(b, 8, (uint32_t)n);
    put32(b, 12, sclsOff);
    memcpy(&b[16], "FILI", 4);
    put32(b, 20, 1);
    put32(b, 24, sclsOff + 0xC * (uint32_t)n);
    for (int i = 0; i < n; i++) {
        uint8_t* e = &b[sclsOff + 0xC * (size_t)i];
        memset(e, 0, 0xC);
        if (i == 0) {
            memcpy(e, "ma2room", 7);
            e[8] = 17;
        } else if (i == 1) {
            memcpy(e, door, strlen(door));
            e[9] = doorRoom;
        }
        e[10] = 0xFF; // wipe
        e[11] = 0x77;
    }
    return b;
}

PcRandoResFile fileOf(Bytes& b, const char* stage, int room, const char* name = "room.dzr") {
    PcRandoResFile f = {};
    f.dir = "";
    f.stage = stage;
    f.arc = "Room0";
    f.room = room;
    f.name = name;
    f.type = PCR_RES_DZR;
    f.data = b.data();
    f.size = b.size();
    return f;
}

const Case* findCase(const std::vector<Case>& cases, const char* name) {
    for (const Case& c : cases) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

void testPatcher() {
    std::vector<Case> cases;
    RT_CHECK(loadCases(cases));
    // EntA_boss: Helmaroc King behind Wind Temple's boss door, Wind Temple on Gale Isle (sea 4 232).
    const Case* boss = findCase(cases, "EntA_boss");
    const Case* off = findCase(cases, "EntOff");
    if (!RT_CHECK(boss != nullptr && off != nullptr)) {
        return;
    }
    PcRandoSeed seed, vanilla;
    RT_CHECK(seedOf(*boss, seed));
    RT_CHECK(seedOf(*off, vanilla));
    PcRandoWarpOut w = {};
    RT_CHECK_EQ(pc_rando_warpout_dest(&seed, "M2tower", &w), 1);
    RT_CHECK(w.stage != nullptr && strcmp(w.stage, "sea") == 0 && w.room == 4 && w.spawn == 232);

    for (const char* door : {"sea", "M2ganon"}) {
        Bytes b = makeRoom(2, door, strcmp(door, "sea") == 0 ? 1 : 0);
        Bytes want = b;
        uint8_t* e = &want[28 + 0xC];
        memset(e, 0, 8);
        memcpy(e, "sea", 3);
        e[8] = 232;
        e[9] = 4;
        PcRandoResFile f = fileOf(b, "M2tower", 0);
        RT_CHECK_EQ(pc_rando_warpout_patch_file(&f, &seed), 1);
        RT_CHECK(b == want);
    }
    PcRandoWarpoutStatus st;
    pc_rando_warpout_status(&st);
    RT_CHECK_EQ(st.m2towerPatched, 2);

    // Left byte-identical.
    struct Leave {
        int n;
        const char* door;
        const char* stage;
        int room;
        const char* name;
        const PcRandoSeed* seed;
        int result;
    } leaves[] = {
        {2, "GanonK", "M2tower", 0, "room.dzr", &seed, -1},  // D9: not the Ganondorf door
        {1, "", "M2tower", 0, "room.dzr", &seed, -1},        // no SCLS[1]
        {2, "sea", "M2tower", 1, "room.dzr", &seed, 0},      // another room
        {2, "sea", "M_DragB", 0, "room.dzr", &seed, 0},      // another stage
        {2, "sea", "M2tower", 0, "stage.dzs", &seed, 0},     // another file
        {2, "sea", "M2tower", 0, "room.dzr", &vanilla, 0},   // entrances not randomized
        {2, "sea", "M2tower", 0, "room.dzr", nullptr, 0},    // no seed
    };
    for (const Leave& l : leaves) {
        Bytes b = makeRoom(l.n, l.door, 1);
        const Bytes want = b;
        PcRandoResFile f = fileOf(b, l.stage, l.room, l.name);
        if (!RT_CHECK_EQ(pc_rando_warpout_patch_file(&f, l.seed), l.result)) {
            fprintf(stderr, "  leave %s %s/%d %s\n", l.door, l.stage, l.room, l.name);
        }
        RT_CHECK(b == want);
    }
    RT_CHECK_EQ(pc_rando_warpout_patch_file(nullptr, &seed), 0);
}

void testEdges() {
    std::vector<Case> cases;
    RT_CHECK(loadCases(cases));
    const Case* boss = findCase(cases, "EntA_boss");
    if (!RT_CHECK(boss != nullptr)) {
        return;
    }
    PcRandoSeed seed;
    RT_CHECK(seedOf(*boss, seed));
    PcRandoWarpOut w = {};
    RT_CHECK_EQ(pc_rando_warpout_dest(&seed, "kaze", &w), 0);  // a dungeon, not its boss arena
    RT_CHECK_EQ(pc_rando_warpout_dest(&seed, "sea", &w), 0);
    RT_CHECK_EQ(pc_rando_warpout_dest(&seed, nullptr, &w), 0);
    RT_CHECK_EQ(pc_rando_warpout_dest(&seed, "M_DragB", nullptr), 0);
    RT_CHECK_EQ(pc_rando_warpout_dest(nullptr, "M_DragB", &w), 0);
    RT_CHECK_EQ(pc_rando_warpout_enabled(nullptr), 0);
    PcRandoSeed bad = seed;
    bad.entrances[0].second = "No Such Exit";
    RT_CHECK_EQ(pc_rando_warpout_dest(&bad, "M_DragB", &w), 0);
    // A seed whose boss exit has no entrance (undecided): nothing.
    PcRandoSeed partial = seed;
    partial.entrances.clear();
    RT_CHECK_EQ(pc_rando_warpout_dest(&partial, "M_DragB", &w), 0);
}

} // namespace

int main() {
    testCases();
    testPatcher();
    testEdges();
    return rt::finish();
}
