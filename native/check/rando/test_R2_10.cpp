// cos_rando_test_R2_10 (docs/RANDOMIZER_PLAN.md step R2.10): the 4 added chests
// (features/rando/pc_rando_tweak_added_chests.h) on synthetic files, without the game:
//   - the fixture R2_10_wwrando_added_chests.txt (fixtures/R2_10_make_fixture.py): 7 synthetic files
//     shaped like the vanilla ones, edited by wwrando's own tweaks.py functions and items.py
//     change_item; the patchers' result on the same bytes gives wwrando's logical dump, and
//     pc_rando_tweak_added_chests_verify accepts it;
//   - D9: a file that is not the vanilla one (no VmsMS, a TRES where the chest must be Chest000, no
//     ajav_uzu) is left byte-identical and its site reports the refusal;
//   - pc_rando() 0: nothing is patched; the cutscene items (pc_rando_added_chests_demo_item);
//   - the content hash ignores the chunk offsets.
// With COS_R2_10_DISC_DIR=<dir> (dev only, never in regress: it needs the disc) it also compares the
// patchers' result on the disc's files (<dir>/vanilla/<stage>_<arc>_<name>) with wwrando's output on
// them (<dir>/ww/..., with the vanilla items in the chests), as dumped by a local script.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_resources.h"
#include "pc_rando_tweak_added_chests.h"

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

// --- dumps (R2_10_make_fixture.py dump_dzx and R0_11_make_fixture.py dump) ---

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
    if (t == "SCOB" || t == "TGSC" || t == "DOOR" || t == "TGDR" || t.substr(0, 3) == "SCO") {
        return 0x24;
    }
    return 0x20;
}

// sizes: the fixture's (synthetic files: every chunk has a known size); the disc mode uses
// pc_rando_dzx_entry_size and leaves unknown chunks out.
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
    pc_rando_tweak_added_chests_patch_dzx(&f);
    pc_rando_tweak_added_chests_patch_event(&f);
    return Bytes(f.data, f.data + f.size);
}

int siteOf(const FileId& id) {
    for (int i = 0; i < PCR_AC_SITE_COUNT; i++) {
        const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(i);
        if (id.stage == s->stage && id.arc == s->arc && id.name == s->name) {
            return i;
        }
    }
    return -1;
}

struct FixtureFile {
    FileId id;
    Bytes src;
    Lines before, after;
};

bool loadFixture(std::vector<FixtureFile>& files) {
    std::string text;
    if (!rt::readFixture("R2_10_wwrando_added_chests.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("file ", 0) == 0) {
            FixtureFile f;
            std::istringstream ls(line.substr(5));
            ls >> f.id.stage >> f.id.arc >> f.id.name;
            files.push_back(f);
        } else if (files.empty()) {
            continue;
        } else if (line.rfind("src ", 0) == 0) {
            unhex(line.substr(4), files.back().src);
        } else if (line.rfind("before ", 0) == 0) {
            files.back().before.push_back(line.substr(7));
        } else if (line.rfind("after ", 0) == 0) {
            files.back().after.push_back(line.substr(6));
        }
    }
    return files.size() == PCR_AC_SITE_COUNT;
}

// The fixture's placement (R2_10_make_fixture.py PLACED).
uint8_t sItems[PCR_LOC_COUNT];
uint8_t placedFor(int site) {
    switch (site) {
    case PCR_AC_JABUN_ROOM:
        return 0x31;
    case PCR_AC_MEDLI_CHEST:
        return 0x26;
    case PCR_AC_MASTER_SWORD:
        return 0x2A;
    case PCR_AC_QUEEN_FAIRY:
        return 0x0B;
    default:
        return 0xFF;
    }
}

void activate() {
    memset(sItems, 0x01, sizeof sItems);
    sItems[PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE] = 0x31;
    sItems[PCR_LOC_DRAGON_ROOST_CAVERN_MINIBOSS] = 0x26;
    sItems[PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER] = 0x2A;
    sItems[PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE] = 0x0B;
    pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
}

Lines dumpOf(const FileId& id, const Bytes& d, bool fixtureSizes) {
    return id.name == "event_list.dat" ? dumpEvl(d) : dumpDzx(d, fixtureSizes);
}

void testFixture(const std::vector<FixtureFile>& files) {
    pc_rando_tweak_added_chests_reset();
    for (const FixtureFile& f : files) {
        const std::string what = f.id.stage + "/" + f.id.arc + "/" + f.id.name;
        const int site = siteOf(f.id);
        RT_CHECK(site >= 0);
        sameLines(dumpOf(f.id, f.src, true), f.before, what + " before");
        const Bytes out = patch(f.id, f.src);
        sameLines(dumpOf(f.id, out, true), f.after, what + " after");
        const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(site);
        RT_CHECK_EQ(s->runs, 1);
        RT_CHECK_EQ(s->result, 0);
        if (s->added != PCR_ADDED_NONE) {
            RT_CHECK_EQ(s->item, placedFor(site));
            RT_CHECK(s->loc >= 0);
        }
        char why[160] = "";
        const int ok = pc_rando_tweak_added_chests_verify(site, out.data(), out.size(), placedFor(site), why, sizeof why);
        if (!ok) {
            fprintf(stderr, "FAIL verify %s: %s\n", what.c_str(), why);
        }
        RT_CHECK(ok);
        // The unpatched file fails the check.
        RT_CHECK(!pc_rando_tweak_added_chests_verify(site, f.src.data(), f.src.size(), placedFor(site), nullptr, 0));
        if (f.id.name != "event_list.dat") {
            RT_CHECK_EQ(s->in_hash, pc_rando_tweak_added_chests_content_hash(f.src.data(), f.src.size()));
            RT_CHECK(pc_rando_tweak_added_chests_content_hash(out.data(), out.size()) != s->in_hash);
        }
    }
}

const FixtureFile* find(const std::vector<FixtureFile>& files, int site) {
    for (const FixtureFile& f : files) {
        if (siteOf(f.id) == site) {
            return &f;
        }
    }
    return nullptr;
}

// Replaces the first occurrence of an 8-byte actor name.
bool rename(Bytes& d, const char* from, const char* to) {
    char a[8] = {}, b[8] = {};
    strncpy(a, from, 8);
    strncpy(b, to, 8);
    auto it = std::search(d.begin(), d.end(), a, a + 8);
    if (it == d.end()) {
        return false;
    }
    std::copy(b, b + 8, it);
    return true;
}

void refused(const FixtureFile& f, int site, const char* what) {
    pc_rando_tweak_added_chests_reset();
    const Bytes out = patch(f.id, f.src);
    const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(site);
    if (!RT_CHECK(out == f.src) || !RT_CHECK_EQ(s->runs, 1) || !RT_CHECK(s->result != 0)) {
        fprintf(stderr, "  (%s)\n", what);
    }
}

void testRefusals(const std::vector<FixtureFile>& files) {
    FixtureFile f = *find(files, PCR_AC_MASTER_SWORD);
    RT_CHECK(rename(f.src, "VmsMS", "VmsXX"));
    refused(f, PCR_AC_MASTER_SWORD, "kenroom without VmsMS");

    f = *find(files, PCR_AC_JABUN_DOOR);
    RT_CHECK(rename(f.src, "Auzu", "Auzx"));
    refused(f, PCR_AC_JABUN_DOOR, "sea/Room44 without Auzu");

    // A TRES chunk where the added chest must be Chest000: the M_NewD2 file under M_Dra09's name.
    f = *find(files, PCR_AC_MEDLI_DUMMY);
    f.id = find(files, PCR_AC_MEDLI_CHEST)->id;
    refused(f, PCR_AC_MEDLI_CHEST, "M_Dra09 with a TRES chunk");

    f = *find(files, PCR_AC_JABUN_EVENT);
    RT_CHECK(rename(f.src, "ajav_uzu", "ajav_uzX"));
    refused(f, PCR_AC_JABUN_EVENT, "no ajav_uzu");

    // A file of another stage is not touched (and counts no run).
    pc_rando_tweak_added_chests_reset();
    f = *find(files, PCR_AC_QUEEN_FAIRY);
    f.id.arc = "Room8";
    RT_CHECK(patch(f.id, f.src) == f.src);
    for (int i = 0; i < PCR_AC_SITE_COUNT; i++) {
        RT_CHECK_EQ(pc_rando_tweak_added_chests_site(i)->runs, 0);
    }
}

void testInactive(const std::vector<FixtureFile>& files) {
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    pc_rando_tweak_added_chests_reset();
    for (const FixtureFile& f : files) {
        RT_CHECK(patch(f.id, f.src) == f.src);
    }
    for (int i = 0; i < PCR_AC_SITE_COUNT; i++) {
        RT_CHECK_EQ(pc_rando_tweak_added_chests_site(i)->runs, 0);
    }
    for (int arg = 0; arg < 10; arg++) {
        RT_CHECK_EQ(pc_rando_added_chests_demo_item(arg, (uint8_t)(0x38 + arg)), (uint8_t)(0x38 + arg));
    }
}

void testDemoItems() {
    activate();
    RT_CHECK_EQ(pc_rando_added_chests_demo_item(0, 0x38), 0x2A);
    RT_CHECK_EQ(pc_rando_added_chests_demo_item(5, 0x69), 0x31);
    RT_CHECK_EQ(pc_rando_added_chests_demo_item(8, 0x35), 0x0B);
    for (int arg : {1, 2, 3, 4, 6, 7, 9}) {
        RT_CHECK_EQ(pc_rando_added_chests_demo_item(arg, 0x77), 0x77);
    }
}

void testHashLayout() {
    // The same chunks at other offsets (a 0x20 gap after the rows) hash the same.
    const uint8_t entry[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    Bytes a(0x20, 0xFF), b(0x40, 0xFF);
    for (Bytes* d : {&a, &b}) {
        const uint32_t off = d == &a ? 0x10 : 0x30;
        (*d)[0] = 0, (*d)[1] = 0, (*d)[2] = 0, (*d)[3] = 1;
        memcpy(&(*d)[4], "FILI", 4);
        (*d)[8] = 0, (*d)[9] = 0, (*d)[10] = 0, (*d)[11] = 1;
        (*d)[12] = 0, (*d)[13] = 0, (*d)[14] = 0, (*d)[15] = (uint8_t)off;
        memcpy(&(*d)[off], entry, 8);
    }
    const uint64_t ha = pc_rando_tweak_added_chests_content_hash(a.data(), a.size());
    RT_CHECK(ha != 0);
    RT_CHECK_EQ(ha, pc_rando_tweak_added_chests_content_hash(b.data(), b.size()));
    b[0x30] ^= 1;
    RT_CHECK(ha != pc_rando_tweak_added_chests_content_hash(b.data(), b.size()));
}

bool readBytes(const std::string& path, Bytes& out) {
    std::string s;
    if (!rt::readFile(path, s)) {
        return false;
    }
    out.assign(s.begin(), s.end());
    return true;
}

// Dev only: the disc's files against wwrando's output on them (identity placement).
void testDisc(const char* dir) {
    static uint8_t identity[PCR_LOC_COUNT];
    pc_rando_deactivate();
    // The vanilla items of the 4 chests (R0.5 keys); the rest does not matter here.
    memset(identity, 0x01, sizeof identity);
    identity[PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE] = 0x69;
    identity[PCR_LOC_DRAGON_ROOST_CAVERN_MINIBOSS] = 0x25;
    identity[PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER] = 0x39;
    identity[PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE] = 0x35;
    pc_rando_set_loc_items(identity, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
    pc_rando_tweak_added_chests_reset();
    for (int i = 0; i < PCR_AC_SITE_COUNT; i++) {
        const PcRandoAddedSiteInfo* s = pc_rando_tweak_added_chests_site(i);
        const FileId id = {s->stage, s->arc, s->name};
        const std::string base = id.stage + "_" + id.arc + "_" + id.name;
        Bytes van, ww;
        if (!readBytes(std::string(dir) + "/vanilla/" + base, van) || !readBytes(std::string(dir) + "/ww/" + base, ww)) {
            continue;
        }
        const Bytes out = patch(id, van);
        RT_CHECK_EQ(s->result, 0);
        sameLines(dumpOf(id, out, false), dumpOf(id, ww, false), "disc " + base);
        fprintf(stderr, "disc %s: %s, content hash %016llx\n", base.c_str(), pc_rando_tweak_added_chests_result_name(i),
                (unsigned long long)s->in_hash);
    }
}

} // namespace

int main() {
    std::vector<FixtureFile> files;
    if (!RT_CHECK(loadFixture(files))) {
        return rt::finish();
    }
    activate();
    testFixture(files);
    testRefusals(files);
    testDemoItems();
    testInactive(files);
    testHashLayout();
    if (const char* dir = getenv("COS_R2_10_DISC_DIR")) {
        testDisc(dir);
    }
    pc_rando_deactivate();
    for (void* p : sAllocs) {
        free(p);
    }
    return rt::finish();
}
