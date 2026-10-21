// cos_rando_test_R6_5 (docs/RANDOMIZER_PLAN.md step R6.5): trap chests and chest type matches
// contents (features/rando/pc_rando_trap.h), without the game:
//   - R6_5_wwrando_trap.txt (fixtures/R6_5_make_fixture.py): tweaks.py add_trap_chest_event_to_stage
//     on a synthetic event_list.dat, byte-identical through pcr::addTrapChestEvent (and a second
//     call changes nothing), also through the event patcher;
//   - three wwrando seeds (trap_chests and chest_type_matches_contents on; B required bosses, C
//     key-lunacy, Swordless, other progress categories): for every chest location the plan's trap
//     flag and chest type are wwrando's get_ctmc_chest_type_for_item after randomize_all, and a trap
//     location's item id is its original item;
//   - the dzx patcher on synthetic files of every chest stage/archive after R2.1's placement: the
//     trap bit and the type nibble land in each chest entry's params, nothing else changes; an entry
//     whose fingerprint failed (D9) is left alone; the added Medli chest (R2.10) gets them too;
//   - nothing happens with pc_rando() 0, with ctmc and traps off, or for a stage without traps.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_data.h"
#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_event_ser.h"
#include "pc_rando_lockeys.h"
#include "pc_rando_place_dzx.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"
#include "pc_rando_trap.h"
#include "pc_rando_tweak_added_chests.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
using pcr::DzxField;
using pcr::KeyKind;
using pcr::LocKey;

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

bool unhex(const std::string& s, Bytes& out) {
    if (s.size() % 2) {
        return false;
    }
    out.clear();
    for (size_t i = 0; i < s.size(); i += 2) {
        out.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return true;
}

std::vector<void*> sAllocs;
void* testAlloc(size_t size, void*) {
    void* p = calloc(1, size);
    sAllocs.push_back(p);
    return p;
}

bool isDzxChest(const LocKey& k) {
    return k.kind == KeyKind::Dzx && k.field == DzxField::TboxItem;
}

// --- the fixture ---

struct Expect {
    int type;
    int trap;
};
struct Fixture {
    Bytes trapSrc, trapOut;
    std::vector<std::pair<std::string, std::string>> seeds;           // k, file
    std::map<std::string, std::map<std::string, Expect>> ctmc;       // k -> location -> expect
};

bool readFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R6_5_wwrando_trap.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kind;
        ls >> kind;
        if (kind == "trap_src" || kind == "trap_out") {
            std::string hex;
            ls >> hex;
            RT_CHECK(unhex(hex, kind == "trap_src" ? fx.trapSrc : fx.trapOut));
        } else if (kind == "seed") {
            std::string k, file;
            ls >> k >> file;
            fx.seeds.emplace_back(k, file);
        } else if (kind == "ctmc") {
            std::string k;
            Expect e;
            ls >> k >> e.type >> e.trap;
            std::string loc;
            std::getline(ls, loc);
            loc.erase(0, loc.find_first_not_of(' '));
            fx.ctmc[k][loc] = e;
        }
    }
    return !fx.trapSrc.empty() && !fx.trapOut.empty() && fx.seeds.size() == 3;
}

// --- synthetic dzx files (as test_R2_1.cpp) ---

uint8_t filler(const std::string& tag, int index, int off) {
    return (uint8_t)(0x40 + tag[3] * 3 + index * 7 + off);
}

struct File {
    std::string stage, arc;
    Bytes bytes;
    std::map<std::string, int> chunkOff;
};

File makeFile(const std::string& stage, const std::string& arc) {
    std::map<std::string, int> counts;
    for (const LocKey& k : pcr::location_keys()) {
        if (isDzxChest(k) && stage == k.stage && arc == k.arc) {
            int& n = counts[k.tag];
            n = n > k.index + 1 ? n : k.index + 1;
        }
    }
    File f{stage, arc, {}, {}};
    size_t size = 4 + 12 * counts.size();
    for (const auto& c : counts) {
        size += 0x20 * (size_t)c.second;
    }
    f.bytes.assign(size, 0);
    put32(&f.bytes[0], (uint32_t)counts.size());
    size_t hdr = 4, data = 4 + 12 * counts.size();
    for (const auto& c : counts) {
        memcpy(&f.bytes[hdr], c.first.data(), 4);
        put32(&f.bytes[hdr + 4], (uint32_t)c.second);
        put32(&f.bytes[hdr + 8], (uint32_t)data);
        f.chunkOff[c.first] = (int)data;
        for (int i = 0; i < c.second; i++) {
            for (int b = 0; b < 0x20; b++) {
                f.bytes[data + i * 0x20 + b] = filler(c.first, i, b);
            }
        }
        hdr += 12;
        data += 0x20 * (size_t)c.second;
    }
    for (const LocKey& k : pcr::location_keys()) {
        if (isDzxChest(k) && stage == k.stage && arc == k.arc) {
            uint8_t* e = &f.bytes[f.chunkOff[k.tag] + k.index * 0x20];
            memcpy(e, k.fp_name, 8);
            pcr::dzx_key_write(k, e, k.disc_byte);
        }
    }
    return f;
}

PcRandoResFile resFile(const std::string& stage, const std::string& arc, const char* name, uint32_t type,
                       Bytes& data) {
    PcRandoResFile rf = {};
    rf.dir = "";
    rf.stage = stage.c_str();
    rf.arc = arc.c_str();
    rf.room = arc.rfind("Room", 0) == 0 ? atoi(arc.c_str() + 4) : -1;
    rf.name = name;
    rf.type = type;
    rf.data = data.data();
    rf.size = data.size();
    rf.alloc = testAlloc;
    return rf;
}

// R2.1's patcher, then R6.5's (the dispatcher's order).
void patchDzx(File& f) {
    const bool isStage = f.arc == "Stage";
    PcRandoResFile rf = resFile(f.stage, f.arc, isStage ? "stage.dzs" : "room.dzr",
                                isStage ? PCR_RES_DZS : PCR_RES_DZR, f.bytes);
    pc_rando_place_dzx_patch(&rf);
    pc_rando_trap_patch_dzx(&rf);
    RT_CHECK(rf.data == f.bytes.data()); // never grown
}

std::vector<std::pair<std::string, std::string>> chestFiles() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const LocKey& k : pcr::location_keys()) {
        if (!isDzxChest(k)) {
            continue;
        }
        std::pair<std::string, std::string> p(k.stage, k.arc);
        bool seen = false;
        for (const auto& q : out) {
            seen = seen || q == p;
        }
        if (!seen) {
            out.push_back(p);
        }
    }
    return out;
}

int keyIndex(const LocKey& k) {
    return (int)(&k - pcr::location_keys().data);
}

// --- tests ---

void testEventBuilder(const Fixture& fx) {
    pcr::EvsList el;
    RT_CHECK_EQ(el.read(fx.trapSrc.data(), fx.trapSrc.size()), (int)PCR_EVL_OK);
    RT_CHECK_EQ(pcr::addTrapChestEvent(el), 1);
    RT_CHECK_EQ(pcr::addTrapChestEvent(el), 0);
    pcr::EvsBytes out;
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_OK);
    RT_CHECK(Bytes(out.begin(), out.end()) == fx.trapOut);
}

void testCtmcFunction() {
    const std::vector<std::string> prog = {"Progressive Sword", "DRC Small Key", "WT Big Key", "Ice Trap Chest"};
    RT_CHECK_EQ(pcr::ctmcChestType("Green Rupee", prog, false, {}), 0);
    RT_CHECK_EQ(pcr::ctmcChestType("Progressive Sword", prog, false, {}), 2);
    RT_CHECK_EQ(pcr::ctmcChestType("Ice Trap Chest", prog, false, {}), 2);
    RT_CHECK_EQ(pcr::ctmcChestType("DRC Small Key", prog, false, {}), 1);
    RT_CHECK_EQ(pcr::ctmcChestType("FW Small Key", prog, false, {}), 0); // not progress
    const std::vector<std::string> req = {"Wind Temple"};
    RT_CHECK_EQ(pcr::ctmcChestType("DRC Small Key", prog, true, req), 0);
    RT_CHECK_EQ(pcr::ctmcChestType("WT Big Key", prog, true, req), 1);
    const std::vector<std::string> dungeons =
        pcr::requiredDungeonsOf({"Wind Temple - Molgera Heart Container", "Tower of the Gods - Gohdan Heart Container"});
    RT_CHECK(dungeons.size() == 2 && dungeons[0] == "Wind Temple" && dungeons[1] == "Tower of the Gods");
    RT_CHECK(pcr::isTrapChestItem("Ice Trap Chest"));
    RT_CHECK(!pcr::isTrapChestItem("Trap Chest"));
}

// One seed: the plan against wwrando, the item ids of the traps, then the patchers.
void testSeed(const Fixture& fx, const std::string& k, const std::string& file) {
    std::string text;
    RT_CHECK(rt::readFixture(file.c_str(), text));
    PcRandoSeed seed;
    std::string err;
    if (!RT_CHECK(pc_rando_seed_parse(text, &seed, &err))) {
        std::fprintf(stderr, "seed %s: %s\n", k.c_str(), err.c_str());
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    pcr::ChestPlan plan;
    RT_CHECK(pcr::buildChestPlan(seed, &plan));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "seed %s: plan in %.1f ms, %d traps in %zu stages (%s)\n", k.c_str(), ms, plan.numTraps,
                 plan.trapStages.size(), plan.error.c_str());
    RT_CHECK(plan.ok && plan.ctmc);
    RT_CHECK_EQ(plan.numTraps, 5);
    const auto& want = fx.ctmc.at(k);
    RT_CHECK_EQ(want.size(), (size_t)170);
    int nCmp = 0;
    for (const auto& w : want) {
        const int loc = pcr::location_index(w.first.c_str());
        if (!RT_CHECK(loc >= 0)) {
            continue;
        }
        nCmp++;
        if (!RT_CHECK_EQ((int)plan.type[(size_t)loc], w.second.type) ||
            !RT_CHECK_EQ((int)plan.trap[(size_t)loc], w.second.trap)) {
            std::fprintf(stderr, "  seed %s: %s (%s)\n", k.c_str(), w.first.c_str(),
                         seed.location_item_names[(size_t)loc].c_str());
        }
        if (w.second.trap) {
            const int orig = pc_rando_item_id(pcr::locations()[(size_t)loc].original_item);
            RT_CHECK_EQ((int)seed.locations[loc], orig);
        }
    }
    RT_CHECK_EQ(nCmp, 170);
    for (int loc = 0; loc < PCR_LOC_COUNT; loc++) {
        if (!want.count(pcr::locations()[(size_t)loc].name)) {
            RT_CHECK_EQ((int)plan.type[(size_t)loc], (int)PC_RANDO_CHEST_TYPE_KEEP);
            RT_CHECK_EQ((int)plan.trap[(size_t)loc], 0);
        }
    }

    // The patchers, with the seed active.
    pc_rando_place_dzx_reset();
    pc_rando_trap_reset();
    pc_rando_set_loc_items(seed.locations, PCR_LOC_COUNT);
    pc_rando_set_active(&seed, nullptr);
    RT_CHECK(pc_rando_trap_plan() != nullptr);
    int nTrapKeys = 0;
    for (const auto& sa : chestFiles()) {
        File f = makeFile(sa.first, sa.second);
        const Bytes before = f.bytes;
        patchDzx(f);
        for (const LocKey& kk : pcr::location_keys()) {
            if (!isDzxChest(kk) || sa.first != kk.stage || sa.second != kk.arc) {
                continue;
            }
            const size_t off = (size_t)f.chunkOff[kk.tag] + kk.index * 0x20;
            const uint32_t p0 = rd32(&before[off + 8]);
            const uint32_t p1 = rd32(&f.bytes[off + 8]);
            uint32_t want1 = (p0 & ~PC_RANDO_TBOX_TYPE_MASK) | ((uint32_t)plan.type[kk.loc] << 20);
            if (plan.trap[kk.loc]) {
                want1 |= PC_RANDO_TBOX_TRAP_BIT;
                nTrapKeys++;
                RT_CHECK_EQ((int)f.bytes[off + 0x1C], (int)kk.disc_byte); // the item stays
            } else {
                RT_CHECK_EQ((int)f.bytes[off + 0x1C], (int)seed.locations[kk.loc]);
            }
            if (!RT_CHECK_EQ(p1, want1)) {
                std::fprintf(stderr, "  %s/%s %s[%u]\n", kk.stage, kk.arc, kk.tag, kk.index);
            }
            const PcRandoTrapKeyStatus* st = pc_rando_trap_key_status(keyIndex(kk));
            RT_CHECK(st != nullptr && st->result == PCR_TRAP_KEY_DONE && st->params == p1);
            // Only the params word and the item byte may change.
            for (int b = 0; b < 0x20; b++) {
                if ((b < 8 || b > 11) && b != 0x1C) {
                    RT_CHECK_EQ(f.bytes[off + b], before[off + b]);
                }
            }
        }
    }
    RT_CHECK(nTrapKeys >= 5);

    // The event patcher: a trap stage gets the event (here on the synthetic file: wwrando's bytes),
    // another stage nothing.
    if (RT_CHECK(!plan.trapStages.empty())) {
        Bytes ev(fx.trapSrc);
        PcRandoResFile rf = resFile(plan.trapStages[0], "Stage", "event_list.dat", PCR_RES_DAT, ev);
        pc_rando_trap_patch_event(&rf);
        RT_CHECK(Bytes(rf.data, rf.data + rf.size) == fx.trapOut);
        Bytes again(fx.trapOut);
        PcRandoResFile rf2 = resFile(plan.trapStages[0], "Stage", "event_list.dat", PCR_RES_DAT, again);
        pc_rando_trap_patch_event(&rf2);
        RT_CHECK(Bytes(rf2.data, rf2.data + rf2.size) == fx.trapOut);
        PcRandoTrapEventStats es;
        pc_rando_trap_event_stats(&es);
        RT_CHECK_EQ(es.added, 1u);
        RT_CHECK_EQ(es.present, 1u);
        RT_CHECK_EQ(es.failed, 0u);
        std::string other = "Asoko";
        for (const char* cand : {"Asoko", "Orichh", "LinkUG", "sea"}) {
            bool used = false;
            for (const std::string& s : plan.trapStages) {
                used = used || s == cand;
            }
            if (!used) {
                other = cand;
                break;
            }
        }
        Bytes ev3(fx.trapSrc);
        PcRandoResFile rf3 = resFile(other, "Stage", "event_list.dat", PCR_RES_DAT, ev3);
        pc_rando_trap_patch_event(&rf3);
        RT_CHECK(Bytes(rf3.data, rf3.data + rf3.size) == fx.trapSrc);
        // A room archive's event list is not the stage's.
        Bytes ev4(fx.trapSrc);
        PcRandoResFile rf4 = resFile(plan.trapStages[0], "Room0", "event_list.dat", PCR_RES_DAT, ev4);
        pc_rando_trap_patch_event(&rf4);
        RT_CHECK(Bytes(rf4.data, rf4.data + rf4.size) == fx.trapSrc);
    }
    pc_rando_deactivate();
    RT_CHECK(pc_rando_trap_plan() == nullptr);
}

// D9 and the off cases on the first trap location's file of seed A.
void testRefusals(const Fixture& fx) {
    std::string text;
    RT_CHECK(rt::readFixture(fx.seeds[0].second.c_str(), text));
    PcRandoSeed seed;
    RT_CHECK(pc_rando_seed_parse(text, &seed, nullptr));
    const LocKey* trapKey = nullptr;
    pcr::ChestPlan plan;
    RT_CHECK(pcr::buildChestPlan(seed, &plan));
    for (const LocKey& k : pcr::location_keys()) {
        if (isDzxChest(k) && plan.trap[k.loc]) {
            trapKey = &k;
            break;
        }
    }
    if (!RT_CHECK(trapKey != nullptr)) {
        return;
    }
    // pc_rando() 0: nothing.
    File f0 = makeFile(trapKey->stage, trapKey->arc);
    const Bytes b0 = f0.bytes;
    patchDzx(f0);
    RT_CHECK(f0.bytes == b0);

    pc_rando_place_dzx_reset();
    pc_rando_trap_reset();
    pc_rando_set_loc_items(seed.locations, PCR_LOC_COUNT);
    pc_rando_set_active(&seed, nullptr);
    // D9: another actor name in the trap chest's entry: R2.1 leaves it, so does R6.5.
    File f1 = makeFile(trapKey->stage, trapKey->arc);
    uint8_t* e = &f1.bytes[(size_t)f1.chunkOff[trapKey->tag] + trapKey->index * 0x20];
    e[0] ^= 0x20;
    const Bytes eBefore(e, e + 0x20);
    patchDzx(f1);
    RT_CHECK(Bytes(e, e + 0x20) == eBefore);
    const PcRandoTrapKeyStatus* st = pc_rando_trap_key_status(keyIndex(*trapKey));
    RT_CHECK(st != nullptr && st->result == PCR_TRAP_KEY_SKIPPED);
    pc_rando_deactivate();

    // Both options off: a placement without traps and ctmc changes no chest.
    PcRandoSeed plain = seed;
    pcr::Json* o = plain.options.find("chest_type_matches_contents");
    RT_CHECK(o != nullptr);
    if (o != nullptr) {
        *o = pcr::Json::boolean(false);
    }
    for (int loc = 0; loc < PCR_LOC_COUNT; loc++) {
        if (pcr::isTrapChestItem(plain.location_item_names[(size_t)loc])) {
            plain.location_item_names[(size_t)loc] = pcr::locations()[(size_t)loc].original_item;
        }
    }
    pc_rando_seed_update_hash(&plain);
    pcr::ChestPlan plainPlan;
    RT_CHECK(pcr::buildChestPlan(plain, &plainPlan));
    RT_CHECK(!plainPlan.ctmc && plainPlan.numTraps == 0 && plainPlan.trapStages.empty());
    pc_rando_place_dzx_reset();
    pc_rando_trap_reset();
    pc_rando_set_loc_items(plain.locations, PCR_LOC_COUNT);
    pc_rando_set_active(&plain, nullptr);
    for (const auto& sa : chestFiles()) {
        File f = makeFile(sa.first, sa.second);
        const Bytes before = f.bytes;
        patchDzx(f);
        for (const LocKey& kk : pcr::location_keys()) {
            if (isDzxChest(kk) && sa.first == kk.stage && sa.second == kk.arc) {
                const size_t off = (size_t)f.chunkOff[kk.tag] + kk.index * 0x20;
                RT_CHECK_EQ(rd32(&f.bytes[off + 8]), rd32(&before[off + 8]));
            }
        }
    }
    Bytes ev(fx.trapSrc);
    PcRandoResFile rf = resFile(trapKey->stage, "Stage", "event_list.dat", PCR_RES_DAT, ev);
    pc_rando_trap_patch_event(&rf);
    RT_CHECK(Bytes(rf.data, rf.data + rf.size) == fx.trapSrc);
    pc_rando_deactivate();
}

// The added Medli chest (R2.10, M_Dra09 stage.dzs TRES[0]) as a trap.
void testAddedChest(const Fixture& fx) {
    std::string text;
    RT_CHECK(rt::readFixture(fx.seeds[0].second.c_str(), text));
    PcRandoSeed seed;
    RT_CHECK(pc_rando_seed_parse(text, &seed, nullptr));
    const int loc = PCR_LOC_DRAGON_ROOST_CAVERN_MINIBOSS;
    seed.location_item_names[loc] = "Ice Trap Chest";
    pc_rando_seed_update_hash(&seed);
    pc_rando_place_dzx_reset();
    pc_rando_trap_reset();
    pc_rando_tweak_added_chests_reset();
    pc_rando_set_loc_items(seed.locations, PCR_LOC_COUNT);
    pc_rando_set_active(&seed, nullptr);
    const pcr::ChestPlan* plan = pc_rando_trap_plan();
    RT_CHECK(plan != nullptr && plan->trap[loc] == 1 && plan->type[loc] == 2);
    Bytes dzs(4, 0); // a stage.dzs with no chunk: R2.10 appends the chest as TRES[0]
    PcRandoResFile rf = resFile("M_Dra09", "Stage", "stage.dzs", PCR_RES_DZS, dzs);
    pc_rando_place_dzx_patch(&rf);
    pc_rando_tweak_added_chests_patch_dzx(&rf);
    pc_rando_trap_patch_dzx(&rf);
    PcRandoDzx dzx;
    if (RT_CHECK_EQ(pc_rando_dzx_open(&dzx, rf.data, rf.size), (int)PCR_DZX_OK)) {
        const uint8_t* e = pc_rando_dzx_entry(&dzx, "TRES", 0);
        if (RT_CHECK(e != nullptr)) {
            uint8_t want[0x20];
            pc_rando_added_chest_entry(PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST, seed.locations[loc], want);
            const uint32_t p = (rd32(want + 8) & ~PC_RANDO_TBOX_TYPE_MASK) | (2u << 20) | PC_RANDO_TBOX_TRAP_BIT;
            RT_CHECK_EQ(rd32(e + 8), p);
            RT_CHECK_EQ((int)e[0x1C], (int)seed.locations[loc]);
        }
    }
    pc_rando_deactivate();
}

} // namespace

int main() {
    Fixture fx;
    if (!RT_CHECK(readFixture(fx))) {
        return rt::finish();
    }
    testEventBuilder(fx);
    testCtmcFunction();
    for (const auto& s : fx.seeds) {
        testSeed(fx, s.first, s.second);
    }
    testRefusals(fx);
    testAddedChest(fx);
    for (void* p : sAllocs) {
        free(p);
    }
    return rt::finish();
}
