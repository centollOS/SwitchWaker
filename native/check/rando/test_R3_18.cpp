// cos_rando_test_R3_18: the Tower of the Gods servants tweak (features/rando/pc_rando_tweak_totg.h,
// docs/RANDOMIZER_PLAN.md step R3.18).
//
// Always (no disc bytes): the content hash's rules (chunk order and offsets do not matter, empty
// actor names are left out, EVNT entries are not), the fixture's format, and the refusals: files
// that are not the vanilla ones are left unchanged (D9), room.dzr is left vanilla when stage.dzs was
// not patched, and the patcher ignores other stages and files.
//
// With COS_R3_18_DISC_DIR=<dir> (dev only, never in regress: it needs the disc) it also patches the
// disc's files that R3_18_make_fixture.py --dump <dir> wrote (vanilla_<file>) and compares them with
// wwrando's output (wwrando_<file>): event_list.dat byte for byte, stage.dzs and room.dzr by content
// hash, and both against fixtures/R3_18_wwrando_totg.txt.
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_tweak_totg.h"

#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

std::vector<void*> sAllocs;
void* testAlloc(size_t size, void*) {
    void* p = calloc(1, size);
    sAllocs.push_back(p);
    return p;
}

PcRandoResFile resFile(const char* arc, const char* name, Bytes& data) {
    PcRandoResFile f = {};
    f.dir = "/res/Stage/Siren/";
    f.stage = "Siren";
    f.arc = arc;
    f.room = strncmp(arc, "Room", 4) == 0 ? atoi(arc + 4) : -1;
    f.name = name;
    f.type = strcmp(name, "event_list.dat") == 0 ? PCR_RES_DAT : (strcmp(name, "stage.dzs") == 0 ? PCR_RES_DZS : PCR_RES_DZR);
    f.data = data.data();
    f.size = data.size();
    f.alloc = testAlloc;
    return f;
}

void put32(Bytes& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)(v >> 24);
    b[off + 1] = (uint8_t)(v >> 16);
    b[off + 2] = (uint8_t)(v >> 8);
    b[off + 3] = (uint8_t)v;
}

// A dzx with the given chunks ({tag, entries}), entries back to back after the rows plus `gap`.
Bytes makeDzx(const std::vector<std::pair<const char*, Bytes>>& chunks, uint32_t gap) {
    Bytes b(4 + 12 * chunks.size() + gap, 0xFF);
    put32(b, 0, (uint32_t)chunks.size());
    for (size_t i = 0; i < chunks.size(); i++) {
        const uint32_t size = pc_rando_dzx_entry_size(chunks[i].first);
        memcpy(&b[4 + 12 * i], chunks[i].first, 4);
        put32(b, 4 + 12 * i + 4, (uint32_t)(chunks[i].second.size() / size));
        put32(b, 4 + 12 * i + 8, (uint32_t)b.size());
        b.insert(b.end(), chunks[i].second.begin(), chunks[i].second.end());
    }
    while (b.size() % 0x20) {
        b.push_back(0xFF);
    }
    return b;
}

Bytes actr(const char* name, uint32_t params) {
    Bytes e(0x20, 0);
    strncpy((char*)e.data(), name, 8);
    put32(e, 8, params);
    e[0x1E] = e[0x1F] = 0xFF;
    return e;
}

void testHash() {
    const char* tags[2] = {"ACTR", "EVNT"};
    Bytes ev(0x18, 0);
    ev[0] = 0; // an EVNT whose first byte is 0 still counts (not an actor chunk)
    memcpy(&ev[1], "x", 1);
    Bytes a1 = actr("Hsh", 0x29);
    Bytes empty = actr("", 0);
    Bytes both = a1;
    both.insert(both.end(), empty.begin(), empty.end());
    const Bytes x = makeDzx({{"ACTR", a1}, {"EVNT", ev}}, 0);
    const Bytes y = makeDzx({{"EVNT", ev}, {"ACTR", both}}, 0x40);
    const uint64_t hx = pc_rando_tweak_totg_content_hash(x.data(), x.size(), tags, 2);
    RT_CHECK(hx != 0);
    RT_CHECK_EQ(hx, pc_rando_tweak_totg_content_hash(y.data(), y.size(), tags, 2));
    const Bytes z = makeDzx({{"ACTR", a1}}, 0);
    RT_CHECK(hx != pc_rando_tweak_totg_content_hash(z.data(), z.size(), tags, 2));
    // The tag order is part of the hash.
    const char* swapped[2] = {"EVNT", "ACTR"};
    RT_CHECK(hx != pc_rando_tweak_totg_content_hash(x.data(), x.size(), swapped, 2));
}

struct Fixture {
    uint32_t evlSize[2];
    uint64_t evl[2], dzs[2], dzr[2];
};

bool loadFixture(Fixture& fx) {
    std::string s;
    if (!rt::readFixture("R3_18_wwrando_totg.txt", s)) {
        return false;
    }
    unsigned long long e0, e1, s0, s1, r0, r1;
    unsigned z0, z1;
    const int n = sscanf(s.c_str(), "evl %u %llx %u %llx\ndzs %llx %llx\ndzr %llx %llx", &z0, &e0, &z1, &e1, &s0, &s1, &r0,
                         &r1);
    if (!RT_CHECK_EQ(n, 8)) {
        return false;
    }
    fx = Fixture{{z0, z1}, {e0, e1}, {s0, s1}, {r0, r1}};
    return true;
}

void testRefusals() {
    pc_rando_tweak_totg_reset_status();
    // A Room7 room.dzr before any stage.dzs patch: left vanilla.
    Bytes room = makeDzx({{"ACTR", actr("Hsh", 0x29)}}, 0);
    const Bytes roomCopy = room;
    PcRandoResFile f = resFile("Room7", "room.dzr", room);
    pc_rando_tweak_totg_patch(&f);
    PcRandoTotgStatus st;
    pc_rando_tweak_totg_status(PCR_TOTG_DZR, &st);
    RT_CHECK_EQ(st.seen, 1);
    RT_CHECK_EQ(st.applied, 0);
    RT_CHECK_EQ(st.last, (int)PCR_DZX_MISMATCH);
    RT_CHECK(f.data == room.data() && room == roomCopy);
    // A stage.dzs without the doors: left vanilla, no EVNT index.
    Bytes stage = makeDzx({{"EVNT", Bytes(0x18, 0xFF)}}, 0);
    const Bytes stageCopy = stage;
    f = resFile("Stage", "stage.dzs", stage);
    pc_rando_tweak_totg_patch(&f);
    pc_rando_tweak_totg_status(PCR_TOTG_DZS, &st);
    RT_CHECK_EQ(st.applied, 0);
    RT_CHECK(st.last != 0);
    RT_CHECK(f.data == stage.data() && stage == stageCopy);
    RT_CHECK_EQ(pc_rando_tweak_totg_evnt_index(), -1);
    // An event list without the servants' events: left vanilla.
    Bytes evl(0x40, 0);
    for (int i = 0; i < 7; i++) {
        put32(evl, (size_t)i * 8, 0x40);
    }
    const Bytes evlCopy = evl;
    f = resFile("Stage", "event_list.dat", evl);
    pc_rando_tweak_totg_patch(&f);
    pc_rando_tweak_totg_status(PCR_TOTG_EVL, &st);
    RT_CHECK_EQ(st.seen, 1);
    RT_CHECK_EQ(st.last, (int)PCR_EVL_NO_EVENT);
    RT_CHECK(evl == evlCopy);
    // Other stages and other files are not touched.
    PcRandoResFile other = resFile("Room7", "room.dzr", room);
    other.stage = "M_NewD2";
    pc_rando_tweak_totg_patch(&other);
    other = resFile("Room6", "room.dzr", room);
    pc_rando_tweak_totg_patch(&other);
    pc_rando_tweak_totg_status(PCR_TOTG_DZR, &st);
    RT_CHECK_EQ(st.seen, 1);
}

bool readBytes(const std::string& path, Bytes& out) {
    std::string s;
    if (!rt::readFile(path, s)) {
        return false;
    }
    out.assign(s.begin(), s.end());
    return true;
}

// Dev only: the disc's files against wwrando's output on them.
void testDisc(const char* dir, const Fixture& fx) {
    pc_rando_tweak_totg_reset_status();
    const char* dzsTags[2] = {"TGDR", "EVNT"};
    const char* dzrTags[2] = {"ACTR", "SCOB"};
    Bytes dzs, dzr, evl, wwDzs, wwDzr, wwEvl;
    const std::string d(dir);
    if (!readBytes(d + "/vanilla_stage.dzs", dzs) || !readBytes(d + "/vanilla_room.dzr", dzr) ||
        !readBytes(d + "/vanilla_event_list.dat", evl) || !readBytes(d + "/wwrando_stage.dzs", wwDzs) ||
        !readBytes(d + "/wwrando_room.dzr", wwDzr) || !readBytes(d + "/wwrando_event_list.dat", wwEvl)) {
        return;
    }
    RT_CHECK_EQ(pc_rando_tweak_totg_content_hash(dzs.data(), dzs.size(), dzsTags, 2), fx.dzs[0]);
    RT_CHECK_EQ(pc_rando_tweak_totg_content_hash(dzr.data(), dzr.size(), dzrTags, 2), fx.dzr[0]);
    RT_CHECK_EQ((uint32_t)evl.size(), fx.evlSize[0]);

    PcRandoResFile f = resFile("Stage", "stage.dzs", dzs);
    pc_rando_tweak_totg_patch(&f);
    PcRandoTotgStatus st;
    pc_rando_tweak_totg_status(PCR_TOTG_DZS, &st);
    RT_CHECK_EQ(st.last, 0);
    RT_CHECK_EQ(pc_rando_tweak_totg_evnt_index(), 8);
    RT_CHECK_EQ(pc_rando_tweak_totg_content_hash(f.data, f.size, dzsTags, 2),
                pc_rando_tweak_totg_content_hash(wwDzs.data(), wwDzs.size(), dzsTags, 2));
    RT_CHECK_EQ(st.hash, fx.dzs[1]);

    f = resFile("Room7", "room.dzr", dzr);
    pc_rando_tweak_totg_patch(&f);
    pc_rando_tweak_totg_status(PCR_TOTG_DZR, &st);
    RT_CHECK_EQ(st.last, 0);
    RT_CHECK_EQ(pc_rando_tweak_totg_content_hash(f.data, f.size, dzrTags, 2),
                pc_rando_tweak_totg_content_hash(wwDzr.data(), wwDzr.size(), dzrTags, 2));
    RT_CHECK_EQ(st.hash, fx.dzr[1]);

    f = resFile("Stage", "event_list.dat", evl);
    pc_rando_tweak_totg_patch(&f);
    pc_rando_tweak_totg_status(PCR_TOTG_EVL, &st);
    RT_CHECK_EQ(st.last, 0);
    RT_CHECK_EQ(st.size, (uint32_t)wwEvl.size());
    RT_CHECK_EQ(st.hash, fx.evl[1]);
    RT_CHECK_EQ(st.size, fx.evlSize[1]);
    if (RT_CHECK(f.size >= wwEvl.size())) {
        size_t firstDiff = SIZE_MAX;
        for (size_t i = 0; i < wwEvl.size(); i++) {
            if (f.data[i] != wwEvl[i]) {
                firstDiff = i;
                break;
            }
        }
        if (!RT_CHECK_EQ(firstDiff, (size_t)SIZE_MAX)) {
            fprintf(stderr, "event_list.dat differs from wwrando's at 0x%zx\n", firstDiff);
        }
        bool zeroTail = true;
        for (size_t i = wwEvl.size(); i < f.size; i++) {
            zeroTail = zeroTail && f.data[i] == 0;
        }
        RT_CHECK(zeroTail);
    }
    fprintf(stderr, "disc: stage.dzs %016" PRIx64 ", room.dzr %016" PRIx64 ", event_list.dat %u bytes %016" PRIx64 "\n",
            fx.dzs[1], fx.dzr[1], st.size, st.hash);
}

} // namespace

int main() {
    testHash();
    Fixture fx = {};
    const bool haveFixture = RT_CHECK(loadFixture(fx));
    testRefusals();
    if (const char* dir = getenv("COS_R3_18_DISC_DIR")) {
        if (haveFixture) {
            testDisc(dir, fx);
        }
    }
    for (void* p : sAllocs) {
        free(p);
    }
    return rt::finish();
}
