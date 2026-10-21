// cos_rando_test_R6_9 (docs/RANDOMIZER_PLAN.md step R6.9): required bosses at runtime
// (features/rando/pc_rando_bossreq.h) without the game, against wwrando's own
// RequiredBossesRandomizer._save (fixture R6_9_wwrando_bossreq.txt, R6_9_make_fixture.py):
//   - per case (two real placements, one with randomized dungeon entrances; all six bosses with
//     vanilla entrances; a synthetic entrance map with two bosses on one island and nested exits):
//     the required bosses in wwrando's order, the sea chart quest markers (index, visible,
//     position, rotation of all 8 PIC1 panes) and the patched GanonL/Room0 room.dzr (logical dump);
//   - message 851's fields and unwrapped string;
//   - D9: a room without ALLdie, with ALLdie on another switch, without KGBdor, or already patched
//     stays byte-identical; pc_rando() 0, or a seed without required bosses: nothing is patched, no
//     marker.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_bossreq.h"
#include "pc_rando_dzx.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
using Lines = std::vector<std::string>;

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

Bytes unhex(const std::string& s) {
    Bytes out;
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        out.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return out;
}

std::string unhexStr(const std::string& s) {
    const Bytes b = unhex(s);
    return std::string(b.begin(), b.end());
}

std::string hex(const uint8_t* p, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += d[p[i] >> 4];
        s += d[p[i] & 15];
    }
    return s;
}

// R3_17_make_fixture.py dump_dzx: chunks by tag, non-removed entries as hex.
bool isActorTag(const std::string& t) {
    static const char* kTags[] = {"ACTR", "TRES", "SCOB", "PLYR", "TGOB", "TGSC", "DOOR", "TGDR"};
    for (const char* k : kTags) {
        if (t == k) {
            return true;
        }
    }
    const std::string p = t.substr(0, 3);
    return p == "ACT" || p == "TRE" || p == "SCO";
}

uint32_t entrySize(const std::string& t) {
    if (t == "FILI") {
        return 8;
    }
    if (t == "SCLS") {
        return 0xC;
    }
    if (t == "EVNT") {
        return 0x18;
    }
    if (t == "SCOB" || t == "TGSC" || t == "DOOR" || t == "TGDR" || t.substr(0, 3) == "SCO") {
        return 0x24;
    }
    return 0x20;
}

Lines dumpDzx(const Bytes& d) {
    std::map<std::string, Lines> chunks;
    const uint32_t n = rd32(d.data());
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* row = d.data() + 4 + 12 * i;
        const std::string tag((const char*)row, 4);
        const uint32_t count = rd32(row + 4), off = rd32(row + 8), size = entrySize(tag);
        Lines entries;
        for (uint32_t j = 0; j < count; j++) {
            const uint8_t* e = d.data() + off + size * j;
            if (isActorTag(tag) && e[0] == 0) {
                continue;
            }
            entries.push_back(hex(e, size));
        }
        if (!entries.empty()) {
            chunks[tag] = entries;
        }
    }
    Lines out;
    for (auto& [tag, entries] : chunks) {
        out.push_back("chunk " + tag);
        for (const std::string& e : entries) {
            out.push_back(" e " + e);
        }
    }
    return out;
}

std::vector<void*> sAllocs;
void* testAlloc(size_t size, void*) {
    void* p = calloc(1, size);
    sAllocs.push_back(p);
    return p;
}

Bytes patchRoom(const Bytes& src, const char* stage = "GanonL", const char* arc = "Room0") {
    Bytes data = src;
    PcRandoResFile f = {};
    f.dir = "";
    f.stage = stage;
    f.arc = arc;
    f.room = 0;
    f.name = "room.dzr";
    f.type = PCR_RES_DZR;
    f.data = data.data();
    f.size = data.size();
    f.alloc = testAlloc;
    pc_rando_bossreq_patch_dzx(&f);
    return Bytes(f.data, f.data + f.size);
}

struct Case {
    std::string name, seed;
    Lines bosses;
    std::vector<std::pair<std::string, std::string>> entrances;
    Lines markers, after;
};

const char* dungeonOf(const std::string& boss) {
    static const char* kMap[][2] = {
        {"Gohma", "Dragon Roost Cavern"}, {"Kalle Demos", "Forbidden Woods"}, {"Gohdan", "Tower of the Gods"},
        {"Helmaroc King", "Forsaken Fortress"}, {"Jalhalla", "Earth Temple"}, {"Molgera", "Wind Temple"},
    };
    for (auto& m : kMap) {
        if (boss == m[0]) {
            return m[1];
        }
    }
    return "?";
}

bool loadSeed(const std::string& file, PcRandoSeed* out) {
    std::string text, err;
    if (!rt::readFixture(file.c_str(), text)) {
        return false;
    }
    const bool ok = pc_rando_seed_parse(text, out, &err);
    if (!ok) {
        std::fprintf(stderr, "FAIL %s: %s\n", file.c_str(), err.c_str());
        rt::failures()++;
    }
    return ok;
}

void checkCase(const Case& c, const Bytes& src) {
    std::fprintf(stderr, "case %s\n", c.name.c_str());
    PcRandoSeed seed;
    if (c.seed == "-") {
        if (!loadSeed("R0_6_TestSeed1.json", &seed)) {
            return;
        }
        seed.entrances = c.entrances;
        seed.required_boss_locations.clear();
        // Reversed, so the order comes from the boss table, not from the list.
        for (auto it = c.bosses.rbegin(); it != c.bosses.rend(); ++it) {
            seed.required_boss_locations.push_back(std::string(dungeonOf(*it)) + " - " + *it + " Heart Container");
        }
    } else if (!loadSeed(c.seed, &seed)) {
        return;
    }
    const char* names[PCR_BR_MAX_BOSSES] = {};
    const int n = pc_rando_bossreq_bosses_of(&seed, names);
    RT_CHECK_EQ(n, (int)c.bosses.size());
    for (int i = 0; i < n && i < (int)c.bosses.size(); i++) {
        RT_CHECK_EQ(std::string(names[i]), c.bosses[(size_t)i]);
    }
    // Markers: all 8 panes, the shown ones from pc_rando_bossreq_markers, the rest zero (the fixture's
    // blo was zeros).
    PcRandoBossMarker ms[PCR_BR_MAX_BOSSES];
    const int nm = pc_rando_bossreq_markers(&seed, ms);
    RT_CHECK_EQ(nm, n);
    Lines got;
    for (int i = 0; i < PCR_BR_MARKERS; i++) {
        int vis = 0, x = 0, y = 0, rot = 0;
        for (int k = 0; k < nm; k++) {
            if (ms[k].index == i) {
                vis = 1;
                x = ms[k].x;
                y = ms[k].y;
                rot = ms[k].rotation;
            }
        }
        char line[80];
        snprintf(line, sizeof line, "marker %d %d %d %d %d", i, vis, x, y, rot);
        got.push_back(line);
    }
    RT_CHECK_EQ(got.size(), c.markers.size());
    for (size_t i = 0; i < got.size() && i < c.markers.size(); i++) {
        RT_CHECK_EQ(got[i], c.markers[i]);
    }
    // The room.
    pc_rando_bossreq_set_seed_for_test(&seed);
    pc_rando_bossreq_reset();
    const Bytes out = patchRoom(src);
    PcRandoBossreqStatus st;
    pc_rando_bossreq_status(&st);
    RT_CHECK_EQ(st.seen, 1);
    RT_CHECK_EQ(st.applied, 1);
    RT_CHECK_EQ(st.result, (int)PCR_DZX_OK);
    RT_CHECK_EQ(st.mask, pc_rando_bossreq_stage_mask(&seed));
    const Lines dump = dumpDzx(out);
    RT_CHECK_EQ(dump.size(), c.after.size());
    for (size_t i = 0; i < dump.size() && i < c.after.size(); i++) {
        RT_CHECK_EQ(dump[i], c.after[i]);
    }
    // A second pass on the patched file is refused (DngSw already there) and changes nothing.
    const Bytes again = patchRoom(out);
    RT_CHECK(again == out);
    pc_rando_bossreq_status(&st);
    RT_CHECK_EQ(st.result, (int)PCR_DZX_MISMATCH);
    // Another room is never touched.
    RT_CHECK(patchRoom(src, "GanonL", "Room1") == src);
    RT_CHECK(patchRoom(src, "GanonK", "Room0") == src);
    pc_rando_bossreq_set_seed_for_test(nullptr);
}

// A copy of `src` with entry `index` of `tag` changed by fn.
template <class F> Bytes edited(const Bytes& src, const char* tag, int index, F fn) {
    Bytes b = src;
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, b.data(), b.size()) == PCR_DZX_OK) {
        uint8_t* e = pc_rando_dzx_entry(&dzx, tag, (uint32_t)index);
        if (e != nullptr) {
            fn(e);
        }
    }
    return b;
}

int indexOf(const Bytes& src, const char* tag, const char* name) {
    Bytes b = src;
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, b.data(), b.size()) != PCR_DZX_OK) {
        return -1;
    }
    for (uint32_t i = 0; i < pc_rando_dzx_count(&dzx, tag); i++) {
        if (strncmp((const char*)pc_rando_dzx_entry(&dzx, tag, i), name, 8) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void checkRefusals(const Bytes& src) {
    PcRandoSeed seed;
    if (!loadSeed("R0_6_abc_full.json", &seed)) {
        return;
    }
    pc_rando_bossreq_set_seed_for_test(&seed);
    const int alldie = indexOf(src, "ACTR", "ALLdie");
    const int door = indexOf(src, "TGOB", "KGBdor");
    RT_CHECK(alldie >= 0 && door >= 0);
    const Bytes variants[3] = {
        edited(src, "ACTR", alldie, [](uint8_t* e) { memcpy(e, "ALLdi2\0\0", 8); }),
        edited(src, "ACTR", alldie, [](uint8_t* e) { e[0x0A] = 0x1A; }),
        edited(src, "TGOB", door, [](uint8_t* e) { memcpy(e, "KGBdo2\0\0", 8); }),
    };
    for (const Bytes& v : variants) {
        pc_rando_bossreq_reset();
        RT_CHECK(patchRoom(v) == v);
        PcRandoBossreqStatus st;
        pc_rando_bossreq_status(&st);
        RT_CHECK_EQ(st.seen, 1);
        RT_CHECK_EQ(st.applied, 0);
        RT_CHECK_EQ(st.result, (int)PCR_DZX_MISMATCH);
    }
    // pc_rando() 0: nothing, not even seen.
    pc_rando_deactivate();
    pc_rando_bossreq_reset();
    RT_CHECK(patchRoom(src) == src);
    PcRandoBossreqStatus st;
    pc_rando_bossreq_status(&st);
    RT_CHECK_EQ(st.seen, 0);
    pc_rando_set_active(nullptr, nullptr);
    // A seed without required bosses: nothing, no marker.
    PcRandoSeed plain;
    if (loadSeed("R0_6_TestSeed1.json", &plain)) {
        pc_rando_bossreq_set_seed_for_test(&plain);
        RT_CHECK(patchRoom(src) == src);
        pc_rando_bossreq_status(&st);
        RT_CHECK_EQ(st.seen, 0);
        PcRandoBossMarker ms[PCR_BR_MAX_BOSSES];
        RT_CHECK_EQ(pc_rando_bossreq_markers(&plain, ms), 0);
        RT_CHECK_EQ(pc_rando_bossreq_stage_mask(&plain), 0u);
    }
    // No active seed at all (COS_RANDO_FORCE): nothing.
    pc_rando_bossreq_set_seed_for_test(nullptr);
    RT_CHECK(pc_rando_seed() == nullptr);
    RT_CHECK(patchRoom(src) == src);
    RT_CHECK_EQ(pc_rando_bossreq_markers(nullptr, nullptr), 0);
}

} // namespace

int main() {
    std::string text;
    if (!rt::readFixture("R6_9_wwrando_bossreq.txt", text)) {
        return rt::finish();
    }
    Bytes src;
    std::vector<Case> cases;
    std::string msgLine;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("src ", 0) == 0) {
            src = unhex(line.substr(4));
        } else if (line.rfind("case ", 0) == 0) {
            cases.push_back(Case());
            cases.back().name = line.substr(5);
        } else if (line.rfind("seed ", 0) == 0) {
            cases.back().seed = line.substr(5);
        } else if (line.rfind("boss ", 0) == 0) {
            cases.back().bosses.push_back(line.substr(5));
        } else if (line.rfind("entrance ", 0) == 0) {
            std::istringstream ls(line.substr(9));
            std::string a, b;
            ls >> a >> b;
            cases.back().entrances.emplace_back(unhexStr(a), unhexStr(b));
        } else if (line.rfind("marker ", 0) == 0) {
            cases.back().markers.push_back(line);
        } else if (line.rfind("after ", 0) == 0) {
            cases.back().after.push_back(line.substr(6));
        } else if (line.rfind("msg ", 0) == 0) {
            msgLine = line;
        }
    }
    RT_CHECK_EQ(cases.size(), (size_t)4);
    RT_CHECK(!src.empty());

    pc_rando_set_active(nullptr, nullptr);
    for (const Case& c : cases) {
        checkCase(c, src);
    }
    checkRefusals(src);

    // Message 851: bmg.add_new_message's fields and msg.string before the wrap.
    {
        std::istringstream ls(msgLine);
        std::string tag, str;
        int id = 0, box = 0, draw = 0, align = 0;
        ls >> tag >> id >> box >> draw >> align >> str;
        RT_CHECK_EQ(id, PCR_BR_MSG);
        RT_CHECK_EQ(box, PCR_BR_MSG_BOX);
        RT_CHECK_EQ(draw, 0);
        RT_CHECK_EQ(align, PCR_BR_MSG_ALIGN);
        RT_CHECK_EQ(std::string(pc_rando_bossreq_message_text()), unhexStr(str));
    }
    // Marker pane tags (f_map.blo PIC1 order on the USA disc).
    RT_CHECK_EQ(pc_rando_bossreq_marker_tag(0), 0x434B3332u);
    RT_CHECK_EQ(pc_rando_bossreq_marker_tag(7), 0x434B3131u);
    RT_CHECK_EQ(pc_rando_bossreq_marker_tag(8), 0u);

    pc_rando_deactivate();
    for (void* p : sAllocs) {
        free(p);
    }
    return rt::finish();
}
