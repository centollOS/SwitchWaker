// COS_SMOKE=rando COS_RANDO_SMOKE=stagesA (docs/RANDOMIZER_PLAN.md step R3.15): stage tweaks A
// (features/rando/pc_rando_tweak_stages_a.h) on the real disc files, one boot per room, run as
//   run.sh rando --stage <stage>:<room>:<point> --env COS_RANDO_SMOKE=stagesA [--env COS_RANDO_FORCE=1]
// (the lines are in native/check/regress_rando/R3.15.txt).
//
// From the first frame the smoke puts its own two res patchers around the step's two (R0.10's API):
// a first one keeps a copy of the raw bytes of every file a tweak targets, a last one compares that
// copy with what loadResource stores, independently of the patcher's code:
//   - M2tower/Room0 room.dzr: PLYR spawn 16 evnt_index 0xFF, the SCLS to M2ganon now "sea" room 1
//     spawn 0, the default-layer ACTR Otble's name cleared;
//   - sea/Room1 room.dzr: ACT1 has one more entry, a byte copy of ACT2's Warpmj; sea/Room1 room.dzb:
//     face 0x1493's property 0x20 -> 0x11;
//   - kaze/Room3: AND_SW2's name cleared, wiz_r's enable_spawn_switch (byte +9) 0xFF;
//   - M_NewD2/Room2: SCOB Akabe's disable_spawn_switch (byte +0xB) 0xFF;
//   - M_NewD2/Room13, Room14: SCOB has one more entry, SW_C00 with params 0x0003FF05 / 0x0003FF06 at
//     wwrando's position, rotation and scale;
//   - PShip stage.dzs: every TRES whose room_num is not 2 has x + 2000, the room 2 one is unchanged;
//   - sea/Room26: PLYR spawn 1 z + 1000; sea/Room15: spawn 1 x = -320170;
//   - sea/Room24, 46, 22, 8, 37, 25: every Ocanon with enable switch 0x2A and every Oship with x_rot
//     low byte 0x2A now 0xFF, at least one each room;
// and, for the files edited in place, that no other byte changed (for the grown ones: every other
// chunk's entries are the same). Once the play scene has run 10 frames:
//   1. with pc_rando() 0: no file was dispatched; on M2tower the Otble actor exists;
//   2. with pc_rando() 1: the boot room's target file(s) were seen and verified, every file seen
//      verified, the step's counters show no refused edit; on M2tower no Otble actor exists, on
//      kaze:3 no AND_SW2 actor;
//   3. the game then runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_stages_a.h"

#include "SSystem/SComponent/c_bg_w.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "stagesA";

// The raw copies live in malloc memory: operator new goes to the JKR heaps, and while loadResource
// runs the current heap is the archive's, freed with it.
struct RawCopy {
    char key[48];
    uint8_t* bytes;
    size_t size;
};

constexpr int kMaxRaw = 16;
RawCopy sRaw[kMaxRaw];
int sRawCount = 0;
bool sRegistered = false;
unsigned int sEndFrame = 0;
char sBootStage[16] = "";
int sBootRoom = -1;
int sTargetSeen = 0;      // target files of the boot room seen and verified
int sTargetExpected = 0;  // target files of the boot room
int sVerified = 0, sVerifyFailed = 0;

// --- what the boot room's tweak files are ---

bool isEyeReef(int room) {
    return room == 24 || room == 46 || room == 22 || room == 8 || room == 37 || room == 25;
}

// The number of tweak files for (stage, room) as the smoke sees them: 0 when none.
int targetFiles(const char* stage, int room) {
    if (strcmp(stage, "M2tower") == 0 && room == 0) return 1;
    if (strcmp(stage, "sea") == 0 && room == 1) return 2; // room.dzr + room.dzb
    if (strcmp(stage, "sea") == 0 && (room == 26 || room == 15 || isEyeReef(room))) return 1;
    if (strcmp(stage, "kaze") == 0 && room == 3) return 1;
    if (strcmp(stage, "M_NewD2") == 0 && (room == 2 || room == 13 || room == 14)) return 1;
    if (strcmp(stage, "PShip") == 0) return 1; // stage.dzs
    return 0;
}

bool isTarget(const PcRandoResFile* f) {
    if (strcmp(f->name, "stage.dzs") == 0) {
        return strcmp(f->stage, "PShip") == 0 && strcmp(f->arc, "Stage") == 0;
    }
    if (strcmp(f->name, "room.dzb") == 0) {
        return strcmp(f->stage, "sea") == 0 && f->room == 1;
    }
    if (strcmp(f->name, "room.dzr") != 0) {
        return false;
    }
    return targetFiles(f->stage, f->room) > 0 && strcmp(f->stage, "PShip") != 0;
}

bool bootTarget(const PcRandoResFile* f) {
    if (strcmp(f->stage, sBootStage) != 0) {
        return false;
    }
    if (strcmp(f->stage, "PShip") == 0) {
        return true;
    }
    return f->room == sBootRoom;
}

void keyOf(const PcRandoResFile* f, char* out, size_t n) {
    snprintf(out, n, "%s/%s/%s", f->stage, f->arc, f->name);
}

// --- dzx lookups (the smoke's own, as wwrando's DZx finds records) ---

bool layerChar(char c) {
    return (c >= '0' && c <= '9') || c == 'a' || c == 'b';
}

bool tagOf(const char* tag, const char* base, bool layers) {
    return memcmp(tag, base, 4) == 0 || (layers && memcmp(tag, base, 3) == 0 && layerChar(tag[3]));
}

struct Ent {
    char tag[5];
    uint32_t index;
    size_t off; // file offset of the entry
    uint8_t* e;
};

// Every entry of `base` in chunk order (a static list, valid until the next call).
struct EntList {
    Ent v[1024];
    int n;
    const Ent* begin() const { return v; }
    const Ent* end() const { return v + n; }
};
EntList sList;

const EntList& entries(const PcRandoDzx& d, const char* base, bool layers) {
    EntList& out = sList;
    out.n = 0;
    for (uint32_t row = 0; row < d.chunkCount; row++) {
        Ent x;
        memcpy(x.tag, d.data + 4 + row * 12, 4);
        x.tag[4] = '\0';
        if (!tagOf(x.tag, base, layers)) {
            continue;
        }
        const uint32_t n = pc_rando_dzx_count(&d, x.tag);
        for (uint32_t i = 0; i < n; i++) {
            x.index = i;
            x.e = pc_rando_dzx_entry(&d, x.tag, i);
            if (x.e == nullptr) {
                break;
            }
            x.off = (size_t)(x.e - d.data);
            if (out.n < 1024) {
                out.v[out.n++] = x;
            }
        }
    }
    return out;
}

bool named(const uint8_t* e, const char* name) {
    return strncmp((const char*)e, name, 8) == 0;
}

const Ent* first(const EntList& v, const char* name) {
    for (const Ent& x : v) {
        if (named(x.e, name)) {
            return &x;
        }
    }
    return nullptr;
}

const Ent* spawn(const EntList& v, int id) {
    for (const Ent& x : v) {
        if (x.e[0x1D] == id) {
            return &x;
        }
    }
    return nullptr;
}

// --- the comparison ---

struct Cmp {
    const char* key;
    PcRandoDzx raw, fin;
    uint8_t* allowed = nullptr; // in place: bytes allowed to differ (malloc, raw.size bytes)
    size_t allowedSize = 0;
    bool ok = true;

    bool check(const char* what, bool cond) {
        char buf[160];
        snprintf(buf, sizeof buf, "%s: %s", key, what);
        randoSmokeCheck(kName, buf, cond);
        ok = ok && cond;
        return cond;
    }
    void allow(size_t off, size_t n) {
        for (size_t i = 0; i < n && off + i < allowedSize; i++) {
            allowed[off + i] = 1;
        }
    }
    // In place: same size and no byte changed outside allow()ed ranges.
    void confined() {
        bool same = raw.size == fin.size;
        size_t bad = 0;
        if (same) {
            for (size_t i = 0; i < raw.size; i++) {
                if (raw.data[i] != fin.data[i] && !allowed[i]) {
                    bad++;
                }
            }
        }
        char buf[64];
        snprintf(buf, sizeof buf, "edited in place, no other byte changed (%zu)", bad);
        check(buf, same && bad == 0);
    }
    // Grown: every chunk of the raw file other than `grownTag` has the same entries in the copy, and
    // grownTag kept its raw entries in front.
    void othersSame(const char* grownTag, uint32_t added) {
        bool same = true;
        for (uint32_t row = 0; row < raw.chunkCount; row++) {
            char tag[5];
            memcpy(tag, raw.data + 4 + row * 12, 4);
            tag[4] = '\0';
            const uint32_t sz = pc_rando_dzx_entry_size(tag);
            const uint32_t n = pc_rando_dzx_count(&raw, tag);
            const uint32_t m = pc_rando_dzx_count(&fin, tag);
            const uint32_t want = strcmp(tag, grownTag) == 0 ? n + added : n;
            if (m != want) {
                same = false;
                continue;
            }
            for (uint32_t i = 0; sz != 0 && i < n; i++) {
                const uint8_t* a = pc_rando_dzx_entry(&raw, tag, i);
                const uint8_t* b = pc_rando_dzx_entry(&fin, tag, i);
                same = same && a != nullptr && b != nullptr && memcmp(a, b, sz) == 0;
            }
        }
        check("grown copy: every other entry unchanged", same);
    }
};

float f32At(const uint8_t* p) {
    return pc_rando_dzx_bef32(p);
}

void verifyM2tower(Cmp& c) {
    const Ent* s = spawn(entries(c.raw, "PLYR", false), 16);
    if (c.check("PLYR spawn 16 exists", s != nullptr)) {
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: M2tower spawn 16 evnt_index raw 0x%02X final 0x%02X\n",
               s->e[0x08], c.fin.data[s->off + 0x08]);
        c.check("PLYR spawn 16 evnt_index 0xFF", c.fin.data[s->off + 0x08] == 0xFF);
        c.allow(s->off + 0x08, 1);
    }
    const Ent* x = first(entries(c.raw, "SCLS", false), "M2ganon");
    if (x != nullptr) {
        const uint8_t* e = c.fin.data + x->off;
        c.check("SCLS M2ganon -> sea, room 1, spawn 0",
                memcmp(e, "sea\0\0\0\0\0", 8) == 0 && e[8] == 0 && e[9] == 1 && e[10] == x->e[10] &&
                    e[11] == x->e[11]);
        c.allow(x->off, 10);
    } else {
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: M2tower has no SCLS to M2ganon\n");
    }
    const Ent* t = first(entries(c.raw, "ACTR", false), "Otble");
    if (c.check("default ACTR Otble exists in the raw file", t != nullptr)) {
        static const uint8_t zero[8] = {};
        c.check("Otble's name cleared", memcmp(c.fin.data + t->off, zero, 8) == 0);
        c.allow(t->off, 8);
    }
    c.confined();
}

void verifySeaRoom1(Cmp& c) {
    const Ent* w = first(entries(c.raw, "ACT2", false), "Warpmj");
    const uint32_t n1 = pc_rando_dzx_count(&c.raw, "ACT1");
    const uint32_t m1 = pc_rando_dzx_count(&c.fin, "ACT1");
    writef(STDERR_FILENO, "[cos] rando-smoke stagesA: sea/Room1 ACT1 %u -> %u entries\n", n1, m1);
    if (c.check("ACT2 Warpmj exists in the raw file", w != nullptr) && c.check("ACT1 has one more entry", m1 == n1 + 1)) {
        const uint8_t* e = pc_rando_dzx_entry(&c.fin, "ACT1", n1);
        c.check("the new ACT1 entry is ACT2's Warpmj, every field", e != nullptr && memcmp(e, w->e, 0x20) == 0);
    }
    c.othersSame("ACT1", 1);
}

void verifyKaze(Cmp& c) {
    const EntList& a = entries(c.raw, "ACTR", false);
    const Ent* s = first(a, "AND_SW2");
    const Ent* w = first(a, "wiz_r");
    if (c.check("AND_SW2 and wiz_r exist in the raw file", s != nullptr && w != nullptr)) {
        static const uint8_t zero[8] = {};
        c.check("AND_SW2's name cleared", memcmp(c.fin.data + s->off, zero, 8) == 0);
        c.allow(s->off, 8);
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: wiz_r enable switch raw 0x%02X\n", w->e[9]);
        c.check("wiz_r enable_spawn_switch 0xFF", c.fin.data[w->off + 9] == 0xFF);
        c.allow(w->off + 9, 1);
    }
    c.confined();
}

void verifyDrc2(Cmp& c) {
    const Ent* a = first(entries(c.raw, "SCOB", true), "Akabe");
    if (c.check("SCOB Akabe exists in the raw file", a != nullptr)) {
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: Akabe (%s[%u]) disable switch raw 0x%02X\n", a->tag,
               a->index, a->e[0x0B]);
        c.check("Akabe disable_spawn_switch 0xFF", c.fin.data[a->off + 0x0B] == 0xFF);
        c.allow(a->off + 0x0B, 1);
    }
    c.confined();
}

void verifyBoulder(Cmp& c, int room) {
    const uint32_t n = pc_rando_dzx_count(&c.raw, "SCOB");
    const uint32_t m = pc_rando_dzx_count(&c.fin, "SCOB");
    if (c.check("SCOB has one more entry", m == n + 1)) {
        const uint8_t* e = pc_rando_dzx_entry(&c.fin, "SCOB", n);
        const bool r13 = room == 13;
        const bool ok = e != nullptr && named(e, "SW_C00") &&
                        pc_rando_dzx_be32(e + 8) == (r13 ? 0x0003FF05u : 0x0003FF06u) &&
                        f32At(e + 0x0C) == (r13 ? 2635.0f : -4002.0f) && f32At(e + 0x10) == (r13 ? 0.0f : 1950.0f) &&
                        f32At(e + 0x14) == (r13 ? 227.0f : -2156.0f) && pc_rando_dzx_be16(e + 0x18) == 0 &&
                        pc_rando_dzx_be16(e + 0x1A) == (r13 ? 0xC000 : 0xA000) &&
                        pc_rando_dzx_be16(e + 0x1C) == 0xFFFF && pc_rando_dzx_be16(e + 0x1E) == 0xFFFF &&
                        e[0x20] == 32 && e[0x21] == 16 && e[0x22] == 16 && e[0x23] == 0xFF;
        c.check(r13 ? "SW_C00 switch 5, behavior 3, at (2635, 0, 227)" : "SW_C00 switch 6, behavior 3, at (-4002, 1950, -2156)", ok);
    }
    c.othersSame("SCOB", 1);
}

void verifyGhostShip(Cmp& c) {
    int moved = 0, kept = 0;
    bool ok = true;
    for (const Ent& t : entries(c.raw, "TRES", true)) {
        const int room = pc_rando_dzx_be16(t.e + 0x18) & 0x3F;
        const float x0 = f32At(t.e + 0x0C), x1 = f32At(c.fin.data + t.off + 0x0C);
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: PShip %s[%u] room %d x %.1f -> %.1f\n", t.tag, t.index, room,
               x0, x1);
        if (room == 2) {
            ok = ok && x1 == x0;
            kept++;
        } else {
            ok = ok && x1 == (float)((double)x0 + 2000.0);
            moved++;
            c.allow(t.off + 0x0C, 4);
        }
    }
    c.check("TRES: room 2 chest kept, the others x + 2000", ok && kept >= 1 && moved >= 1);
    c.confined();
}

void verifySpawn(Cmp& c, int id, uint32_t off, bool add, float v, const char* what) {
    const Ent* s = spawn(entries(c.raw, "PLYR", false), id);
    if (c.check("the PLYR spawn exists", s != nullptr)) {
        const float a = f32At(s->e + off), b = f32At(c.fin.data + s->off + off);
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: %s raw %.1f final %.1f\n", c.key, a, b);
        c.check(what, add ? b == (float)((double)a + (double)v) : b == v);
        c.allow(s->off + off, 4);
    }
    c.confined();
}

void verifyEyeReef(Cmp& c) {
    int cannons = 0, ships = 0;
    bool ok = true;
    for (const Ent& a : entries(c.raw, "ACTR", true)) {
        const uint8_t* f = c.fin.data + a.off;
        if (named(a.e, "Ocanon")) {
            const bool hit = a.e[0x0A] == 0x2A;
            ok = ok && f[0x0A] == (hit ? 0xFF : a.e[0x0A]);
            cannons += hit;
            c.allow(a.off + 0x0A, 1);
        } else if (named(a.e, "Oship")) {
            const bool hit = a.e[0x19] == 0x2A;
            ok = ok && f[0x19] == (hit ? 0xFF : a.e[0x19]);
            ships += hit;
            c.allow(a.off + 0x19, 1);
        }
    }
    writef(STDERR_FILENO, "[cos] rando-smoke stagesA: %s: %d Ocanon, %d Oship on switch 0x2A\n", c.key, cannons, ships);
    c.check("Ocanon / Oship switch 0x2A -> 0xFF", ok && cannons + ships > 0);
    c.confined();
}

void verifyDzb(const char* key, const RawCopy& raw, const PcRandoResFile* f) {
    char buf[160];
    const cBgD_t* d = (const cBgD_t*)f->data;
    bool ok = raw.size == f->size && (u32)d->m_t_num > PCR_SA_FF_DOOR_FACE;
    if (ok) {
        const cBgD_Tri_t* tri = &((const cBgD_Tri_t*)d->m_t_tbl)[PCR_SA_FF_DOOR_FACE];
        const size_t off = (size_t)((const uint8_t*)&tri->id - f->data);
        const u16 was = (u16)((raw.bytes[off] << 8) | raw.bytes[off + 1]);
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: %s face 0x%X property 0x%X -> 0x%X\n", key,
               PCR_SA_FF_DOOR_FACE, was, (u16)tri->id);
        ok = was == 0x20 && (u16)tri->id == PCR_SA_FF_DOOR_PROPERTY;
        size_t bad = 0;
        for (size_t i = 0; i < raw.size; i++) {
            if (raw.bytes[i] != f->data[i] && i != off && i != off + 1) {
                bad++;
            }
        }
        snprintf(buf, sizeof buf, "%s: no other byte changed (%zu)", key, bad);
        randoSmokeCheck(kName, buf, bad == 0);
        ok = ok && bad == 0;
    }
    snprintf(buf, sizeof buf, "%s: face 0x1493 property 0x20 -> 0x11", key);
    randoSmokeCheck(kName, buf, ok);
    sVerified++;
    sVerifyFailed += ok ? 0 : 1;
}

RawCopy* rawOf(const char* key) {
    for (int i = 0; i < sRawCount; i++) {
        if (strcmp(sRaw[i].key, key) == 0) {
            return &sRaw[i];
        }
    }
    return nullptr;
}

void firstPatcher(PcRandoResFile* f) {
    if (!isTarget(f)) {
        return;
    }
    char key[48];
    keyOf(f, key, sizeof key);
    RawCopy* r = rawOf(key);
    if (r == nullptr) {
        if (sRawCount == kMaxRaw) {
            randoSmokeCheck(kName, "raw copy table has room", false);
            return;
        }
        r = &sRaw[sRawCount++];
        snprintf(r->key, sizeof r->key, "%s", key);
        r->bytes = nullptr;
    }
    free(r->bytes);
    r->bytes = (uint8_t*)malloc(f->size);
    r->size = r->bytes != nullptr ? f->size : 0;
    if (r->bytes != nullptr) {
        memcpy(r->bytes, f->data, f->size);
    }
}

void lastPatcher(PcRandoResFile* f) {
    if (!isTarget(f)) {
        return;
    }
    char key[48];
    keyOf(f, key, sizeof key);
    RawCopy* r = rawOf(key);
    if (r == nullptr) {
        randoSmokeCheck(kName, "raw copy kept", false);
        return;
    }
    writef(STDERR_FILENO, "[cos] rando-smoke stagesA: verifying %s (raw 0x%zx, final 0x%zx%s)\n", key,
           r->size, f->size, f->replaced ? ", copy" : "");
    const bool boot = bootTarget(f);
    if (strcmp(f->name, "room.dzb") == 0) {
        const int before = sVerifyFailed;
        verifyDzb(key, *r, f);
        sTargetSeen += boot && sVerifyFailed == before;
        return;
    }
    Cmp c;
    c.key = key;
    const bool parsed = pc_rando_dzx_open(&c.raw, r->bytes, r->size) == PCR_DZX_OK &&
                        pc_rando_dzx_open(&c.fin, f->data, f->size) == PCR_DZX_OK;
    if (!c.check("raw and final parse", parsed)) {
        sVerifyFailed++;
        return;
    }
    c.allowedSize = r->size;
    c.allowed = (uint8_t*)calloc(r->size ? r->size : 1, 1);
    const char* s = f->stage;
    if (strcmp(s, "M2tower") == 0) {
        verifyM2tower(c);
    } else if (strcmp(s, "PShip") == 0) {
        verifyGhostShip(c);
    } else if (strcmp(s, "kaze") == 0) {
        verifyKaze(c);
    } else if (strcmp(s, "M_NewD2") == 0) {
        if (f->room == 2) {
            verifyDrc2(c);
        } else {
            verifyBoulder(c, f->room);
        }
    } else if (f->room == 1) {
        verifySeaRoom1(c);
    } else if (f->room == 26) {
        verifySpawn(c, 1, 0x14, true, (float)PCR_SA_TOTG_DZ, "PLYR spawn 1 z + 1000");
    } else if (f->room == 15) {
        verifySpawn(c, 1, 0x0C, false, PCR_SA_FAIRY_X, "PLYR spawn 1 x = -320170");
    } else {
        verifyEyeReef(c);
    }
    free(c.allowed);
    sVerified++;
    sVerifyFailed += c.ok ? 0 : 1;
    sTargetSeen += boot && c.ok;
}

int countActors(s16 name) {
    struct Ctx {
        s16 name;
        int n;
    } ctx = {name, 0};
    fopAcM_Search(
        [](void* proc, void* data) -> void* {
            Ctx* c = static_cast<Ctx*>(data);
            if (fpcM_GetName(proc) == c->name) {
                c->n++;
            }
            return nullptr;
        },
        &ctx);
    return ctx.n;
}

void parseBootStage() {
    const char* spec = getenv("COS_BOOT_STAGE");
    if (spec == nullptr) {
        return;
    }
    size_t i = 0;
    while (spec[i] != '\0' && spec[i] != ':' && i + 1 < sizeof sBootStage) {
        sBootStage[i] = spec[i];
        i++;
    }
    sBootStage[i] = '\0';
    sBootRoom = spec[i] == ':' ? atoi(spec + i + 1) : -1;
}

} // namespace

void randoSmokeStagesA(unsigned int frames) {
    if (!sRegistered) {
        sRegistered = true;
        parseBootStage();
        sTargetExpected = targetFiles(sBootStage, sBootRoom);
        // The order: the smoke's first patcher, the step's dzx and dzb patchers, the smoke's last one.
        pc_rando_unregister_res_patcher(pc_rando_stages_a_patch);
        pc_rando_unregister_res_patcher(pc_rando_stages_a_patch_dzb);
        pc_rando_register_res_patcher(firstPatcher, PCR_RES_ANY);
        pc_rando_stages_a_init();
        pc_rando_register_res_patcher(lastPatcher, PCR_RES_ANY);
        writef(STDERR_FILENO, "[cos] rando-smoke stagesA: boot %s room %d (%d target file(s)), pc_rando() %d\n",
               sBootStage, sBootRoom, sTargetExpected, pc_rando());
    }
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    sEndFrame = frames + 30;

    PcRandoResStats st;
    pc_rando_res_stats(&st);
    const bool m2tower = strcmp(sBootStage, "M2tower") == 0;
    const int otble = countActors(fpcNm_OBJ_OTBLE_e);
    const int andsw2 = countActors(fpcNm_ANDSW2_e);
    writef(STDERR_FILENO, "[cos] rando-smoke stagesA: actors Otble %d, AND_SW2 %d\n", otble, andsw2);
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: no file dispatched", st.dispatched == 0);
        randoSmokeCheck(kName, "vanilla: no file verified (nothing patched)", sVerified == 0);
        if (m2tower) {
            randoSmokeCheck(kName, "vanilla: the Otble actor exists", otble > 0);
        }
        return;
    }
    randoSmokeCheck(kName, "the boot stage has tweak files", sTargetExpected > 0);
    char buf[96];
    snprintf(buf, sizeof buf, "the boot room's %d target file(s) seen and verified (%d)", sTargetExpected, sTargetSeen);
    randoSmokeCheck(kName, buf, sTargetSeen == sTargetExpected);
    snprintf(buf, sizeof buf, "every file seen verified (%d, %d failed)", sVerified, sVerifyFailed);
    randoSmokeCheck(kName, buf, sVerified > 0 && sVerifyFailed == 0);
    PcRandoStagesAStats sa;
    pc_rando_stages_a_stats(&sa);
    bool none = true;
    for (int t = 0; t < PCR_SA_COUNT; t++) {
        if (sa.applied[t] != 0 || sa.failed[t] != 0) {
            writef(STDERR_FILENO, "[cos] rando-smoke stagesA: %s applied %u failed %u\n", pc_rando_stages_a_name(t),
                   sa.applied[t], sa.failed[t]);
        }
        none = none && sa.failed[t] == 0;
    }
    randoSmokeCheck(kName, "no edit refused", none);
    if (m2tower) {
        randoSmokeCheck(kName, "no Otble actor", otble == 0);
    }
    if (strcmp(sBootStage, "kaze") == 0 && sBootRoom == 3) {
        randoSmokeCheck(kName, "no AND_SW2 actor", andsw2 == 0);
    }
}

} // namespace pc
