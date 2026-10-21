// cos_rando_test_R3_14 (docs/RANDOMIZER_PLAN.md step R3.14): the event_list.dat serializer
// (features/rando/pc_rando_event_ser.h), a port of wwrando's wwlib/events.py, on synthetic files (no
// disc bytes):
//   - the fixture R3_14_wwrando_evser.txt (fixtures/R3_14_make_fixture.py): a file built and saved
//     by wwrando's EventList; read + serialize gives it back byte for byte; the edits of
//     edit_synth() (add_event, add_actor, add_action, properties of every kind, an action inserted
//     into a chain, an actor removed from an event, a longer string, an empty list), made here,
//     serialize to wwrando's own save_changes output byte for byte, with the same flags handed out;
//   - the edited output reads back and serializes to itself;
//   - the R0.11 fixture's file (also a wwrando save) round-trips, and the in-place library
//     (pc_rando_event.h) opens the serializer's output;
//   - refusals: truncated and swapped files, broken chains, duplicate event names, flags used
//     twice, an actor with no actions, more than 20 actors, names that do not fit;
//   - pc_rando_evser_write_res in place and through a grown copy.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_event.h"
#include "pc_rando_event_ser.h"
#include "pc_rando_resources.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
using pcr::EvsBytes;
using pcr::EvsList;

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

void wr32(uint8_t* p, uint32_t v) {
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

bool same(const EvsBytes& a, const Bytes& b) {
    if (a.size() != b.size()) {
        std::fprintf(stderr, "  size %zu vs %zu\n", a.size(), b.size());
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            std::fprintf(stderr, "  first difference at 0x%zx: %02x vs %02x\n", i, a[i], b[i]);
            return false;
        }
    }
    return true;
}

struct Fixture {
    Bytes src, out;
    std::vector<int32_t> flags;
};

bool loadFixture(Fixture& f) {
    std::string text;
    if (!rt::readFixture("R3_14_wwrando_evser.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    bool haveSrc = false, haveOut = false, haveFlags = false;
    while (std::getline(in, line)) {
        if (line.compare(0, 4, "src ") == 0) {
            haveSrc = unhex(line.substr(4), f.src);
        } else if (line.compare(0, 4, "out ") == 0) {
            haveOut = unhex(line.substr(4), f.out);
        } else if (line.compare(0, 6, "flags ") == 0) {
            std::istringstream fl(line.substr(6));
            int32_t v;
            while (fl >> v) {
                f.flags.push_back(v);
            }
            haveFlags = true;
        }
    }
    return haveSrc && haveOut && haveFlags;
}

// R3_14_make_fixture.py edit_synth(), in the same order. Returns the flags handed out.
std::vector<int32_t> editSynth(EvsList& el) {
    std::vector<int32_t> flags;
    auto flagOfActor = [&](int a) { flags.push_back(el.actors[a].flag); };
    auto flagOfAction = [&](int c) { flags.push_back(el.actions[c].flag); };

    const int ev = el.addEvent("PcrNewEvent");
    RT_CHECK(ev >= 0);
    el.events[ev].priority = 2;
    el.events[ev].startingFlags[0] = 5;
    el.events[ev].startingFlags[1] = -1;
    const int a = el.addActor(ev, "Link");
    flagOfActor(a);
    int c = el.addAction(a, "001n_wait");
    flagOfAction(c);
    el.addPropInt(c, "Timer", 30);
    c = el.addAction(a, "011get_item");
    flagOfAction(c);
    const int32_t prm0[1] = {7};
    el.addPropInts(c, "prm0", prm0, 1);
    el.addPropVec(c, "Pos", 1.0f, 2.0f, 3.0f);
    el.addPropFloat(c, "Spd", 1.5f);
    el.addPropString(c, "msg", "hello there evser");
    const int cam = el.addActor(ev, "CAMERA");
    flagOfActor(cam);
    el.actors[cam].staffType = 2;
    c = el.addAction(cam, "FIXEDFRM");
    flagOfAction(c);
    const float eye[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    el.addPropVecs(c, "Eye", eye, 2);
    const float fovy[2] = {1.0f, 2.0f};
    el.addPropFloats(c, "Fovy", fovy, 2);
    el.addPropInts(c, "Empty", nullptr, 0);

    const int gp1 = el.findEvent("Gp1_Get_Itm");
    RT_CHECK(gp1 >= 0);
    const int link = el.events[gp1].actors[0];
    const int ins = el.addAction(link, "PCR_INS");
    flagOfAction(ins);
    el.addPropInt(ins, "Timer", 3);
    auto& la = el.actors[link].actions;
    la.pop_back();             // actions.remove(ins) (it was appended last)
    la.insert(la.begin() + 1, ins);

    const int tact = el.findEvent("TACT_HT");
    RT_CHECK(tact >= 0);
    auto& ta = el.events[tact].actors;
    ta.erase(ta.begin() + 1); // tact.actors.remove(hr)
    const int tk = ta[1];
    c = el.addAction(tk, "PCR_TK");
    flagOfAction(c);
    el.addPropString(c, "msg", "a much longer string than before");

    const int cam0 = el.events[gp1].actors[1];
    const int ratio = el.actions[el.actors[cam0].actions[1]].props[1];
    el.props[ratio].setInts(nullptr, 0); // value = [] (saved as an empty int list)
    const int fov0 = el.actions[el.actors[cam0].actions[0]].props[1];
    const float fov[1] = {12.25f};
    el.props[fov0].setFloats(fov, 1);
    el.events[ev].endingFlags[0] = el.actions[el.actors[a].actions.back()].flag;
    el.events[ev].endingFlags[1] = -1;
    el.events[ev].endingFlags[2] = -1;
    return flags;
}

void testFixture(const Fixture& f) {
    // The wwrando file reads and serializes byte for byte.
    EvsList el;
    RT_CHECK_EQ(el.read(f.src.data(), f.src.size()), (int)PCR_EVL_OK);
    RT_CHECK_EQ(el.events.size(), (size_t)3);
    EvsBytes out;
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_OK);
    RT_CHECK(same(out, f.src));
    // Serializing twice gives the same bytes.
    EvsBytes again;
    RT_CHECK_EQ(el.serialize(again), (int)PCR_EVL_OK);
    RT_CHECK(again == out);

    // Lookups.
    const int gp1 = el.findEvent("Gp1_Get_Itm");
    RT_CHECK_EQ(gp1, 0);
    RT_CHECK_EQ(el.findEvent("TACT_HT"), 1);
    RT_CHECK_EQ(el.findEvent("Gp1_Get"), -1);
    const int hr = el.findActor(1, "Hr");
    RT_CHECK(hr >= 0);
    RT_CHECK_EQ(el.actors[hr].staffId, 3u);
    const int msg2 = el.findAction(hr, "MSG", 1);
    RT_CHECK(msg2 >= 0);
    RT_CHECK_EQ(el.findAction(hr, "MSG", 2), -1);
    const int list = el.getProp(msg2, "list");
    RT_CHECK(list >= 0 && el.props[list].words.size() == 4 && el.props[list].words[3] == 4);
    const int text = el.getProp(el.findAction(hr, "MSG", 0), "text");
    RT_CHECK(text >= 0 && el.props[text].str == "seven77");
    const int cam = el.findActor(0, "CAMERA");
    const int center = el.getProp(el.findAction(cam, "UNITRANS"), "Center");
    RT_CHECK(center >= 0 && el.props[center].type == PCR_EVL_TYPE_VEC && el.props[center].count() == 2);

    // wwrando's edits.
    EvsList ed;
    RT_CHECK_EQ(ed.read(f.src.data(), f.src.size()), (int)PCR_EVL_OK);
    const std::vector<int32_t> flags = editSynth(ed);
    RT_CHECK(flags == f.flags);
    EvsBytes edited;
    RT_CHECK_EQ(ed.serialize(edited), (int)PCR_EVL_OK);
    RT_CHECK(same(edited, f.out));

    // The output reads back and serializes to itself; the in-place library opens it.
    EvsList back;
    RT_CHECK_EQ(back.read(edited.data(), edited.size()), (int)PCR_EVL_OK);
    EvsBytes round;
    RT_CHECK_EQ(back.serialize(round), (int)PCR_EVL_OK);
    RT_CHECK(round == edited);
    PcRandoEvl evl;
    Bytes copy(edited.begin(), edited.end());
    RT_CHECK_EQ(pc_rando_evl_open(&evl, copy.data(), copy.size()), (int)PCR_EVL_OK);
    const int nev = pc_rando_evl_find_event(&evl, "PcrNewEvent");
    RT_CHECK_EQ(nev, 3);
    const int nlink = pc_rando_evl_event_actor(&evl, nev, 0);
    RT_CHECK_EQ(pc_rando_evl_action_count(&evl, nlink), 2);
    const int get = pc_rando_evl_actor_action(&evl, nlink, 1);
    const int msg = pc_rando_evl_action_property(&evl, get, "msg");
    const char* s = pc_rando_evl_get_string(&evl, msg);
    RT_CHECK(s != nullptr && strcmp(s, "hello there evser") == 0);
    float spd = 0;
    RT_CHECK_EQ(pc_rando_evl_get_float(&evl, pc_rando_evl_action_property(&evl, get, "Spd"), 0, &spd), (int)PCR_EVL_OK);
    RT_CHECK_EQ(spd, 1.5f);
    // Gp1_Get_Itm's Link: 001n_wait, PCR_INS, 011get_item, FINISH.
    const int glink = pc_rando_evl_event_actor(&evl, 0, 0);
    RT_CHECK_EQ(pc_rando_evl_find_action(&evl, glink, "PCR_INS", 0), 1);
    RT_CHECK_EQ(pc_rando_evl_find_action(&evl, glink, "FINISH", 0), 3);
}

// The R0.11 fixture's src (also a wwrando save) round-trips.
void testR011() {
    std::string text;
    if (!RT_CHECK(rt::readFixture("R0_11_wwrando_events.txt", text))) {
        return;
    }
    std::istringstream in(text);
    std::string line;
    Bytes src;
    while (std::getline(in, line)) {
        if (line.compare(0, 4, "src ") == 0) {
            unhex(line.substr(4), src);
        }
    }
    EvsList el;
    RT_CHECK_EQ(el.read(src.data(), src.size()), (int)PCR_EVL_OK);
    EvsBytes out;
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_OK);
    RT_CHECK(same(out, src));
}

void testRefusals(const Fixture& f) {
    EvsList el;
    // Truncated, swapped.
    RT_CHECK_EQ(el.read(f.src.data(), 0x20), (int)PCR_EVL_BAD_FILE);
    RT_CHECK_EQ(el.read(f.src.data(), f.src.size() - 8), (int)PCR_EVL_BAD_FILE);
    RT_CHECK(el.events.empty());
    Bytes b = f.src;
    b[0x38] = 1;
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_SWAPPED);

    const uint32_t evTop = rd32(f.src.data() + 0x00), acTop = rd32(f.src.data() + 0x08);
    const uint32_t ctTop = rd32(f.src.data() + 0x10), ctNum = rd32(f.src.data() + 0x14);
    const uint32_t prTop = rd32(f.src.data() + 0x18);
    // A looping action chain (action 0's next is itself).
    b = f.src;
    wr32(b.data() + ctTop + 0x3C, 0);
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_LOOP);
    // A next action out of the list.
    b = f.src;
    wr32(b.data() + ctTop + 0x3C, ctNum);
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_LOOP);
    // A looping property chain.
    b = f.src;
    wr32(b.data() + prTop + 0x30, 0);
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_LOOP);
    // An unknown property type.
    b = f.src;
    wr32(b.data() + prTop + 0x24, 2);
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_BAD_TYPE);
    // Duplicate event name (event 1 renamed to event 0's name).
    b = f.src;
    memcpy(b.data() + evTop + 0xB0, b.data() + evTop, 0x20);
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_BAD_FILE);
    // A used actor slot after a blank one.
    b = f.src;
    wr32(b.data() + evTop + 0x2C, 0xFFFFFFFFu); // slot 0 blank, slot 1 used
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_BAD_FILE);
    // A flag used twice (actor 1's flag = actor 0's).
    b = f.src;
    memcpy(b.data() + acTop + 0x50 + 0x28, b.data() + acTop + 0x28, 4);
    RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_BAD_FILE);
    // A string property not at a string start.
    {
        EvsList ok;
        RT_CHECK_EQ(ok.read(f.src.data(), f.src.size()), (int)PCR_EVL_OK);
        const int hr = ok.findActor(1, "Hr");
        const int text = ok.getProp(ok.findAction(hr, "MSG", 0), "text");
        b = f.src;
        wr32(b.data() + prTop + text * 0x40 + 0x28, ok.props[text].dataIndex + 1);
        RT_CHECK_EQ(el.read(b.data(), b.size()), (int)PCR_EVL_BAD_FILE);
    }

    // Serialize refusals.
    RT_CHECK_EQ(el.read(f.src.data(), f.src.size()), (int)PCR_EVL_OK);
    EvsBytes out;
    const int ev = el.addEvent("NoActions");
    const int a = el.addActor(ev, "Link");
    RT_CHECK(a >= 0);
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_BAD_RANGE); // actor with no actions
    RT_CHECK(out.empty());
    el.addAction(a, "WAIT");
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_OK);
    for (int i = 0; i < 19; i++) {
        el.events[ev].actors.push_back(a); // the same actor 20 times: allowed, as in Python
    }
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_OK);
    el.events[ev].actors.push_back(a);
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_BAD_RANGE); // 21 actors
    el.events[ev].actors.resize(1);
    el.events[ev].actors[0] = 9999;
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_BAD_RANGE);
    el.events[ev].actors[0] = a;

    // Names.
    RT_CHECK_EQ(el.addEvent("Gp1_Get_Itm"), -1);
    RT_CHECK_EQ(el.addEvent("0123456789012345678901234567890123"), -1);
    RT_CHECK(el.addEvent("0123456789012345678901234567890") >= 0); // 31 chars fit
    RT_CHECK_EQ(el.addActor(ev, "0123456789012345678901234567890123"), -1);
    RT_CHECK_EQ(el.addActor(9999, "Link"), -1);
    RT_CHECK_EQ(el.addAction(9999, "WAIT"), -1);

    // Flags run out.
    EvsList fl;
    fl.clear();
    int last = -1;
    for (int i = 0; i < pcr::kEvsTotalFlags; i++) {
        last = fl.unusedFlag();
    }
    RT_CHECK_EQ(last, pcr::kEvsTotalFlags - 1);
    RT_CHECK_EQ(fl.unusedFlag(), -1);
}

// An empty list (the seven offsets 0x40, as R0_11_make_fixture.py's empty_file) serializes to it.
void testEmpty() {
    EvsList el;
    el.clear();
    EvsBytes out;
    RT_CHECK_EQ(el.serialize(out), (int)PCR_EVL_OK);
    Bytes want(0x40, 0);
    for (int i = 0; i < 7; i++) {
        wr32(want.data() + i * 8, 0x40);
    }
    RT_CHECK(same(out, want));
    RT_CHECK_EQ(el.unusedFlag(), 0);
    RT_CHECK_EQ(el.unusedFlag(), 1);
}

// pc_rando_evser_write_res: in place when the output fits, else a grown copy from the allocator.
int sAllocs = 0;
void* testAlloc(size_t size, void*) {
    sAllocs++;
    return aligned_alloc(0x20, (size + 0x1F) & ~(size_t)0x1F);
}

void testWriteRes(const Fixture& f) {
    Bytes buf(f.src.size() + 64, 0xAA);
    memcpy(buf.data(), f.src.data(), f.src.size());
    PcRandoResFile file = {};
    file.dir = "/res/Stage/Test/";
    file.stage = "Test";
    file.arc = "Stage";
    file.room = -1;
    file.name = "event_list.dat";
    file.type = PCR_RES_DAT;
    file.data = buf.data();
    file.size = buf.size();
    file.alloc = testAlloc;

    EvsList el;
    RT_CHECK_EQ(el.read(file.data, file.size), (int)PCR_EVL_OK);
    RT_CHECK_EQ(pcr::pc_rando_evser_write_res(&file, el), (int)PCR_EVL_OK);
    RT_CHECK(file.data == buf.data() && !file.replaced && sAllocs == 0);
    RT_CHECK(memcmp(buf.data(), f.src.data(), f.src.size()) == 0);
    RT_CHECK(buf[f.src.size()] == 0 && buf.back() == 0); // the tail is zeroed

    editSynth(el);
    RT_CHECK_EQ(pcr::pc_rando_evser_write_res(&file, el), (int)PCR_EVL_OK);
    RT_CHECK(file.replaced && file.data != buf.data() && sAllocs == 1);
    RT_CHECK_EQ(file.size, f.out.size());
    RT_CHECK(memcmp(file.data, f.out.data(), f.out.size()) == 0);
    free(file.data);

    // No allocator: the file is unchanged.
    PcRandoResFile small = file;
    Bytes sb = f.src;
    small.data = sb.data();
    small.size = sb.size();
    small.replaced = 0;
    small.alloc = nullptr;
    RT_CHECK(pcr::pc_rando_evser_write_res(&small, el) != PCR_EVL_OK);
    RT_CHECK(small.data == sb.data() && sb == f.src);

    RT_CHECK_EQ(pcr::pc_rando_evser_fnv64("", 0), 0xcbf29ce484222325ull);
    RT_CHECK_EQ(pcr::pc_rando_evser_fnv64("a", 1), 0xaf63dc4c8601ec8cull);
}

} // namespace

int main() {
    Fixture f;
    if (RT_CHECK(loadFixture(f))) {
        testFixture(f);
        testRefusals(f);
        testWriteRes(f);
    }
    testR011();
    testEmpty();
    return rt::finish();
}
