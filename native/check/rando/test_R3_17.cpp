// cos_rando_test_R3_17 (docs/RANDOMIZER_PLAN.md step R3.17): the pirate ship at Windfall
// (features/rando/pc_rando_tweak_pirate_ship.h) on synthetic files, without the game:
//   - the fixture R3_17_wwrando_pirate_ship.txt (fixtures/R3_17_make_fixture.py): 4 synthetic files
//     shaped like the vanilla ones, edited by wwrando's own add_pirate_ship_to_windfall; the
//     patchers' result on the same bytes gives wwrando's logical dump (the stage files) and its exact
//     bytes (event_list.dat), and pc_rando_tweak_pirate_ship_verify accepts it;
//   - the messages (3008's parts and initial sound, 849/850's fields and texts) and the main.dol /
//     REL writes (the audio slot and facial animation constants the game hooks use);
//   - D9: files that are not the vanilla ones (no P2b, no Ashut, the event or EVNT already there)
//     stay byte-identical and their site reports the refusal; pc_rando() 0: nothing is patched.
// With COS_R3_17_DISC_DIR=<dir> (dev only, never in regress: it needs the disc) it also compares the
// patchers' result on the disc's files (<dir>/vanilla/<stage>_<arc>_<name>) with wwrando's
// (<dir>/ww/...), as R3_17_make_fixture.py --disc --disc-dir writes them.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_msg.h"
#include "pc_rando_resources.h"
#include "pc_rando_tweak_pirate_ship.h"
#include "pc_rando_wrap.h"

#include <algorithm>
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

std::string unhexStr(const std::string& s) {
    Bytes b;
    unhex(s, b);
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

std::string fhex(float f) {
    uint32_t b;
    memcpy(&b, &f, 4);
    char buf[16];
    snprintf(buf, sizeof buf, "%08x", b);
    return buf;
}

// --- dumps (R3_17_make_fixture.py dump_dzx and R0_11_make_fixture.py dump) ---

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

uint32_t fixtureSize(const std::string& t) {
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

Lines dumpDzx(const Bytes& d, bool fixtureSizes) {
    std::map<std::string, Lines> chunks;
    const uint32_t n = rd32(d.data());
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* row = d.data() + 4 + 12 * i;
        const std::string tag((const char*)row, 4);
        const uint32_t count = rd32(row + 4), off = rd32(row + 8);
        const uint32_t size = fixtureSizes ? fixtureSize(tag) : (tag == "RTBL" ? 0 : pc_rando_dzx_entry_size(tag.c_str()));
        if (size == 0) {
            continue;
        }
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

Lines dumpEvl(const Bytes& data) {
    PcRandoEvl evl;
    Lines out;
    if (pc_rando_evl_open(&evl, const_cast<uint8_t*>(data.data()), data.size()) != PCR_EVL_OK) {
        out.push_back("(not an event list)");
        return out;
    }
    char line[256];
    char name[PCR_EVL_NAME_MAX + 1];
    for (uint32_t e = 0; e < evl.eventNum; e++) {
        const uint8_t* ev = pc_rando_evl_event_p(&evl, (int)e);
        pc_rando_evl_name(ev, name);
        snprintf(line, sizeof line, "event %u %s prio %d start %d %d end %d %d %d jingle %d", e, name,
                 (int)rd32(ev + 0x28), (int)rd32(ev + 0x80), (int)rd32(ev + 0x84), (int)rd32(ev + 0x88),
                 (int)rd32(ev + 0x8C), (int)rd32(ev + 0x90), ev[0x94]);
        out.push_back(line);
        for (int slot = 0;; slot++) {
            const int actor = pc_rando_evl_event_actor(&evl, (int)e, slot);
            if (actor < 0) {
                break;
            }
            const uint8_t* ac = pc_rando_evl_actor_p(&evl, actor);
            pc_rando_evl_name(ac, name);
            snprintf(line, sizeof line, " actor %s id %d type %d flag %d", name, (int)rd32(ac + 0x20),
                     (int)rd32(ac + 0x2C), (int)pc_rando_evl_actor_flag(&evl, actor));
            out.push_back(line);
            const int n = pc_rando_evl_action_count(&evl, actor);
            for (int i = 0; i < n; i++) {
                const int action = pc_rando_evl_actor_action(&evl, actor, i);
                const uint8_t* act = pc_rando_evl_action_p(&evl, action);
                pc_rando_evl_name(act, name);
                snprintf(line, sizeof line, "  action %s dup %d start %d %d %d flag %d", name, (int)rd32(act + 0x20),
                         (int)rd32(act + 0x28), (int)rd32(act + 0x2C), (int)rd32(act + 0x30),
                         (int)pc_rando_evl_action_flag(&evl, action));
                out.push_back(line);
                for (int k = 0;; k++) {
                    const int prop = pc_rando_evl_action_property_at(&evl, action, k);
                    if (prop < 0) {
                        break;
                    }
                    pc_rando_evl_name(pc_rando_evl_property_p(&evl, prop), name);
                    std::string s = std::string("   prop ") + name;
                    const int type = pc_rando_evl_property_type(&evl, prop);
                    const int size = pc_rando_evl_property_size(&evl, prop);
                    if (type == PCR_EVL_TYPE_STRING) {
                        const char* str = pc_rando_evl_get_string(&evl, prop);
                        s += std::string(" str ") + (str ? str : "(bad)");
                    } else if (type == PCR_EVL_TYPE_INT) {
                        s += " int";
                        for (int j = 0; j < size; j++) {
                            int32_t v = 0;
                            pc_rando_evl_get_int(&evl, prop, j, &v);
                            s += " " + std::to_string(v);
                        }
                    } else {
                        s += type == PCR_EVL_TYPE_VEC ? " vec" : " float";
                        const int nf = type == PCR_EVL_TYPE_VEC ? size * 3 : size;
                        for (int j = 0; j < nf; j++) {
                            float v = 0;
                            pc_rando_evl_get_float(&evl, prop, j, &v);
                            s += " " + fhex(v);
                        }
                    }
                    out.push_back(s);
                }
            }
        }
    }
    return out;
}

bool sameLines(const Lines& got, const Lines& want, const std::string& what) {
    if (got == want) {
        return true;
    }
    fprintf(stderr, "FAIL %s: dumps differ (%zu vs %zu lines)\n", what.c_str(), got.size(), want.size());
    for (size_t i = 0; i < std::max(got.size(), want.size()); i++) {
        const std::string g = i < got.size() ? got[i] : "(none)";
        const std::string w = i < want.size() ? want[i] : "(none)";
        if (g != w) {
            fprintf(stderr, "  line %zu:\n    got  %s\n    want %s\n", i, g.c_str(), w.c_str());
            break;
        }
    }
    rt::failures()++;
    return false;
}

// --- files ---

std::vector<void*> sAllocs;
void* testAlloc(size_t size, void*) {
    void* p = calloc(1, size);
    sAllocs.push_back(p);
    return p;
}

struct FileId {
    std::string stage, arc, name;
};

PcRandoResFile resFile(const FileId& id, Bytes& data) {
    PcRandoResFile f = {};
    f.dir = "";
    f.stage = id.stage.c_str();
    f.arc = id.arc.c_str();
    f.room = id.arc.rfind("Room", 0) == 0 ? atoi(id.arc.c_str() + 4) : -1;
    f.name = id.name.c_str();
    f.type = id.name == "event_list.dat" ? PCR_RES_DAT : (id.name == "stage.dzs" ? PCR_RES_DZS : PCR_RES_DZR);
    f.data = data.data();
    f.size = data.size();
    f.alloc = testAlloc;
    return f;
}

// Runs both patchers (as the dispatcher would: each takes its own type) and returns the final bytes.
Bytes patch(const FileId& id, const Bytes& src) {
    Bytes data = src;
    PcRandoResFile f = resFile(id, data);
    pc_rando_tweak_pirate_ship_patch_dzx(&f);
    pc_rando_tweak_pirate_ship_patch_event(&f);
    return Bytes(f.data, f.data + f.size);
}

int siteOf(const FileId& id) {
    for (int i = 0; i < PCR_PS_SITE_COUNT; i++) {
        const PcRandoPirateShipSiteInfo* s = pc_rando_tweak_pirate_ship_site(i);
        if (id.stage == s->stage && id.arc == s->arc && id.name == s->name) {
            return i;
        }
    }
    return -1;
}

struct FixtureFile {
    FileId id;
    Bytes src, out;
    Lines before, after;
};

struct Fixture {
    std::vector<FixtureFile> files;
    std::vector<std::string> parts;
    int sound = -1;
    struct Msg {
        int id, box, draw, align;
        std::string text;
    };
    std::vector<Msg> msgs;
    Lines writes; // "dol ..." and "rel ..."
};

bool loadFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R3_17_wwrando_pirate_ship.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "file") {
            FixtureFile f;
            ls >> f.id.stage >> f.id.arc >> f.id.name;
            fx.files.push_back(f);
        } else if (key == "parts") {
            std::string h;
            while (ls >> h) {
                fx.parts.push_back(unhexStr(h));
            }
        } else if (key == "sound") {
            ls >> fx.sound;
        } else if (key == "msg") {
            Fixture::Msg m;
            std::string h;
            ls >> m.id >> m.box >> m.draw >> m.align >> h;
            m.text = unhexStr(h);
            fx.msgs.push_back(m);
        } else if (key == "dol" || key == "rel") {
            fx.writes.push_back(line);
        } else if (fx.files.empty()) {
            continue;
        } else if (key == "src") {
            unhex(line.substr(4), fx.files.back().src);
        } else if (key == "out") {
            unhex(line.substr(4), fx.files.back().out);
        } else if (key == "before") {
            fx.files.back().before.push_back(line.substr(7));
        } else if (key == "after") {
            fx.files.back().after.push_back(line.substr(6));
        }
    }
    return fx.files.size() == PCR_PS_SITE_COUNT;
}

Lines dumpOf(const FileId& id, const Bytes& d, bool fixtureSizes) {
    return id.name == "event_list.dat" ? dumpEvl(d) : dumpDzx(d, fixtureSizes);
}

// The game loads Asoko's Stage.arc before Room0.arc: the stage patch sets the EVNT index first.
std::vector<const FixtureFile*> gameOrder(const std::vector<FixtureFile>& files) {
    std::vector<const FixtureFile*> out;
    for (int site : {PCR_PS_WINDFALL, PCR_PS_SHIP_STAGE, PCR_PS_SHIP_EVENTS, PCR_PS_SHIP_ROOM}) {
        for (const FixtureFile& f : files) {
            if (siteOf(f.id) == site) {
                out.push_back(&f);
            }
        }
    }
    return out;
}

void testFixture(const Fixture& fx) {
    pc_rando_tweak_pirate_ship_reset();
    for (const FixtureFile* fp : gameOrder(fx.files)) {
        const FixtureFile& f = *fp;
        const std::string what = f.id.stage + "/" + f.id.arc + "/" + f.id.name;
        const int site = siteOf(f.id);
        RT_CHECK(site >= 0);
        sameLines(dumpOf(f.id, f.src, true), f.before, what + " before");
        const Bytes out = patch(f.id, f.src);
        sameLines(dumpOf(f.id, out, true), f.after, what + " after");
        if (site == PCR_PS_SHIP_EVENTS) {
            // The serializer writes wwrando's save_changes bytes; the copy may be longer (0x20 padding
            // of the archive heap copy is zero).
            RT_CHECK(out.size() >= f.out.size());
            RT_CHECK(std::equal(f.out.begin(), f.out.end(), out.begin()));
        }
        const PcRandoPirateShipSiteInfo* s = pc_rando_tweak_pirate_ship_site(site);
        RT_CHECK_EQ(s->runs, 1);
        RT_CHECK_EQ(s->result, 0);
        char why[160] = "";
        const int ok = pc_rando_tweak_pirate_ship_verify(site, out.data(), out.size(), why, sizeof why);
        if (!ok) {
            fprintf(stderr, "FAIL verify %s: %s\n", what.c_str(), why);
        }
        RT_CHECK(ok);
        RT_CHECK(!pc_rando_tweak_pirate_ship_verify(site, f.src.data(), f.src.size(), nullptr, 0));
    }
    RT_CHECK_EQ(pc_rando_tweak_pirate_ship_evnt_index(), 2);
}

// A width table: every character 10 pixels, the replacement 12.
pcr::GlyphWidths testWidths() {
    pcr::GlyphWidths w;
    memset(w.code, 10, sizeof w.code);
    w.replacement = 12;
    return w;
}

void testMessages(const Fixture& fx) {
    RT_CHECK_EQ((int)fx.parts.size(), pc_rando_tweak_pirate_ship_aryll_part_count());
    for (size_t i = 0; i < fx.parts.size(); i++) {
        const char* p = pc_rando_tweak_pirate_ship_aryll_part((int)i);
        RT_CHECK(p != nullptr && fx.parts[i] == p);
    }
    RT_CHECK_EQ(fx.sound, PCR_PS_ARYLL_SOUND);
    RT_CHECK_EQ(fx.msgs.size(), (size_t)2);

    pc_rando_msg_clear();
    PcRandoMsgEntry van;
    pc_rando_msg_entry_defaults(&van);
    van.text_box_type = 0;
    van.initial_sound = 3;
    van.next_message_id = 3009;
    const pcr::GlyphWidths w = testWidths();
    RT_CHECK_EQ(pc_rando_tweak_pirate_ship_messages(w, van), 3);
    const PcRandoMsg* m = pc_rando_msg_find(PCR_PS_MSG_ARYLL);
    if (RT_CHECK(m != nullptr && m->has_entry && m->bytes != nullptr)) {
        RT_CHECK_EQ(m->entry.initial_sound, PCR_PS_ARYLL_SOUND);
        RT_CHECK_EQ(m->entry.next_message_id, 3009); // the rest of the BMG's entry is kept
        const std::string want = pcr::constructStringFromParts(w, 0, fx.parts);
        size_t len = 0;
        pc_rando_msg_encode(want.c_str(), nullptr, 0, &len);
        Bytes enc(len);
        pc_rando_msg_encode(want.c_str(), enc.data(), enc.size(), &len);
        RT_CHECK(m->len == len && memcmp(m->bytes, enc.data(), len) == 0);
    }
    PcRandoMsgEntry def;
    pc_rando_msg_entry_defaults(&def);
    for (const Fixture::Msg& fm : fx.msgs) {
        const PcRandoMsg* r = pc_rando_msg_find((uint16_t)fm.id);
        if (!RT_CHECK(r != nullptr && r->has_entry)) {
            continue;
        }
        RT_CHECK_EQ((int)r->entry.text_box_type, fm.box);
        RT_CHECK_EQ((int)r->entry.initial_draw_type, fm.draw);
        RT_CHECK_EQ((int)r->entry.text_alignment, fm.align);
        RT_CHECK_EQ(r->entry.text_box_position, def.text_box_position);
        RT_CHECK_EQ(r->entry.unknown_1, def.unknown_1);
        RT_CHECK(fm.text == pc_rando_tweak_pirate_ship_new_text((uint16_t)fm.id));
        size_t len = 0;
        Bytes enc(256);
        RT_CHECK_EQ(pc_rando_msg_encode(fm.text.c_str(), enc.data(), enc.size(), &len), PCR_MSG_OK);
        RT_CHECK(r->len == len && memcmp(r->bytes, enc.data(), len) == 0);
    }
    pc_rando_msg_clear();
}

// wwrando's main.dol and REL writes are the constants the game hooks use.
void testWrites(const Fixture& fx) {
    char a[64], b[64], c[64];
    snprintf(a, sizeof a, "dol %08X %d", 0x8039C30C + PCR_PS_ASOKO_SCENE * 4 + 3, PCR_PS_SCENE_WAVE2);
    snprintf(b, sizeof b, "dol %08X %d", 0x8039C2E4 + PCR_PS_SCENE_WAVE2 * 2, PCR_PS_ISLE_LINK_WAVE);
    // d_a_npc_ls1.rel .data starts at 0x5CB8; a_anm_prm_tbl$4744 (setAnm_NUM's 14 x 0x10) at .data+0x60,
    // btpNum at +1 of each entry.
    snprintf(c, sizeof c, "rel d_a_npc_ls1.rel %X %d", 0x5CB8 + 0x60 + PCR_PS_LS1_ANIM * 0x10 + 1, PCR_PS_LS1_FACE);
    RT_CHECK_EQ(fx.writes.size(), (size_t)3);
    RT_CHECK(std::find(fx.writes.begin(), fx.writes.end(), a) != fx.writes.end());
    RT_CHECK(std::find(fx.writes.begin(), fx.writes.end(), b) != fx.writes.end());
    RT_CHECK(std::find(fx.writes.begin(), fx.writes.end(), c) != fx.writes.end());
}

const FixtureFile* find(const std::vector<FixtureFile>& files, int site) {
    for (const FixtureFile& f : files) {
        if (siteOf(f.id) == site) {
            return &f;
        }
    }
    return nullptr;
}

// Replaces the first occurrence of an 8-byte actor name (or any byte string).
bool rename(Bytes& d, const char* from, const char* to, size_t n = 8) {
    std::vector<char> a(n, 0), b(n, 0);
    strncpy(a.data(), from, n);
    strncpy(b.data(), to, n);
    auto it = std::search(d.begin(), d.end(), a.begin(), a.end());
    if (it == d.end()) {
        return false;
    }
    std::copy(b.begin(), b.end(), it);
    return true;
}

void refused(const FixtureFile& f, int site, const char* what) {
    pc_rando_tweak_pirate_ship_reset();
    const Bytes out = patch(f.id, f.src);
    const PcRandoPirateShipSiteInfo* s = pc_rando_tweak_pirate_ship_site(site);
    if (!RT_CHECK(out == f.src) || !RT_CHECK_EQ(s->runs, 1) || !RT_CHECK(s->result != 0)) {
        fprintf(stderr, "  (%s)\n", what);
    }
}

void testRefusals(const std::vector<FixtureFile>& files) {
    FixtureFile f = *find(files, PCR_PS_WINDFALL);
    RT_CHECK(rename(f.src, "Pirates", "Pirate"));
    RT_CHECK(rename(f.src, "Pirates", "Pirate")); // both ACT2 ships
    refused(f, PCR_PS_WINDFALL, "sea/Room11 without a layer-2 Pirates");

    // Niko only on layer 2: the ACT3 one is missing.
    f = *find(files, PCR_PS_SHIP_ROOM);
    {
        PcRandoDzx dzx;
        RT_CHECK_EQ(pc_rando_dzx_open(&dzx, f.src.data(), f.src.size()), PCR_DZX_OK);
        const uint32_t n = pc_rando_dzx_count(&dzx, "ACT3");
        for (uint32_t i = 0; i < n; i++) {
            uint8_t* e = pc_rando_dzx_entry(&dzx, "ACT3", i);
            if (memcmp(e, "P2b", 4) == 0) {
                memcpy(e, "P2c", 3);
            }
        }
    }
    refused(f, PCR_PS_SHIP_ROOM, "Asoko/Room0 without Niko on ACT3");

    f = *find(files, PCR_PS_SHIP_ROOM);
    RT_CHECK(rename(f.src, "Ashut", "Ashux"));
    refused(f, PCR_PS_SHIP_ROOM, "Asoko/Room0 without Ashut on ACT2");

    // The event / EVNT already there (e.g. patched twice): refused, left as they are.
    pc_rando_tweak_pirate_ship_reset();
    FixtureFile s = *find(files, PCR_PS_SHIP_STAGE);
    s.src = patch(s.id, s.src);
    refused(s, PCR_PS_SHIP_STAGE, "stage.dzs with EVNT AryllOpensDoor");
    pc_rando_tweak_pirate_ship_reset();
    FixtureFile e = *find(files, PCR_PS_SHIP_EVENTS);
    e.src = patch(e.id, e.src);
    refused(e, PCR_PS_SHIP_EVENTS, "event_list.dat with AryllOpensDoor");

    // A file of another stage or room is not touched (and counts no run).
    pc_rando_tweak_pirate_ship_reset();
    f = *find(files, PCR_PS_WINDFALL);
    f.id.arc = "Room12";
    RT_CHECK(patch(f.id, f.src) == f.src);
    f = *find(files, PCR_PS_SHIP_EVENTS);
    f.id.stage = "Asoko2";
    RT_CHECK(patch(f.id, f.src) == f.src);
    for (int i = 0; i < PCR_PS_SITE_COUNT; i++) {
        RT_CHECK_EQ(pc_rando_tweak_pirate_ship_site(i)->runs, 0);
    }
}

void testInactive(const std::vector<FixtureFile>& files) {
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    pc_rando_tweak_pirate_ship_reset();
    for (const FixtureFile& f : files) {
        RT_CHECK(patch(f.id, f.src) == f.src);
    }
    for (int i = 0; i < PCR_PS_SITE_COUNT; i++) {
        RT_CHECK_EQ(pc_rando_tweak_pirate_ship_site(i)->runs, 0);
    }
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
void testDisc(const char* dir) {
    pc_rando_set_active(nullptr, nullptr);
    pc_rando_tweak_pirate_ship_reset();
    int compared = 0;
    for (int site : {PCR_PS_WINDFALL, PCR_PS_SHIP_STAGE, PCR_PS_SHIP_EVENTS, PCR_PS_SHIP_ROOM}) {
        const PcRandoPirateShipSiteInfo* s = pc_rando_tweak_pirate_ship_site(site);
        const FileId id = {s->stage, s->arc, s->name};
        const std::string base = id.stage + "_" + id.arc + "_" + id.name;
        Bytes van, ww;
        if (!RT_CHECK(readBytes(std::string(dir) + "/vanilla/" + base, van) &&
                      readBytes(std::string(dir) + "/ww/" + base, ww))) {
            continue;
        }
        const Bytes out = patch(id, van);
        RT_CHECK_EQ(s->result, 0);
        sameLines(dumpOf(id, out, false), dumpOf(id, ww, false), "disc " + base);
        if (site == PCR_PS_SHIP_EVENTS) {
            RT_CHECK(out.size() >= ww.size() && std::equal(ww.begin(), ww.end(), out.begin()));
        }
        RT_CHECK(pc_rando_tweak_pirate_ship_verify(site, out.data(), out.size(), nullptr, 0));
        fprintf(stderr, "disc %s: %s (%zu -> %zu bytes)\n", base.c_str(), pc_rando_tweak_pirate_ship_result_name(site),
                van.size(), out.size());
        compared++;
    }
    RT_CHECK_EQ(compared, 4);
    RT_CHECK_EQ(pc_rando_tweak_pirate_ship_evnt_index(), 2);
}

} // namespace

int main() {
    Fixture fx;
    if (!RT_CHECK(loadFixture(fx))) {
        return rt::finish();
    }
    pc_rando_set_active(nullptr, nullptr);
    testFixture(fx);
    testMessages(fx);
    testWrites(fx);
    testRefusals(fx.files);
    testInactive(fx.files);
    if (const char* dir = getenv("COS_R3_17_DISC_DIR")) {
        testDisc(dir);
    }
    pc_rando_deactivate();
    for (void* p : sAllocs) {
        free(p);
    }
    return rt::finish();
}
