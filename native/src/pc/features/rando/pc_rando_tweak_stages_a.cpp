// Stage tweaks A: logic and softlock fixes on stage.dzs / room.dzr (docs/RANDOMIZER_PLAN.md step
// R3.15). See pc_rando_tweak_stages_a.h for the list; every edit follows ref/wwrando tweaks.py.
// Pure (cos_rando_core): no game code.
#include "pc_rando_tweak_stages_a.h"

#include "pc_rando_dzx.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

PcRandoStagesAStats sStats;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

const char* const kNames[PCR_SA_COUNT] = {
    "ff2-cutscenes", "gt-warp", "makar", "invisible-walls", "boulders", "ghost-ship",
    "totg-spawn", "eye-reefs", "helmaroc-table", "fairy-spawn", "ff-door",
};

void count(int t, bool ok, const PcRandoResFile* f, const char* what) {
    if (t < 0 || t >= PCR_SA_COUNT) {
        return;
    }
    if (ok) {
        sStats.applied[t]++;
    } else {
        sStats.failed[t]++;
    }
    logf("[cos] rando: stagesA %s: %s/%s/%s %s%s\n", kNames[t], f->stage, f->arc, f->name, what,
         ok ? "" : " (not applied)");
}

// --- record lookup, as wwrando's DZx does ---

// The tag of chunk row `row`, NUL-terminated.
void rowTag(const PcRandoDzx* dzx, uint32_t row, char out[5]) {
    memcpy(out, dzx->data + 4 + row * 12, 4);
    out[4] = '\0';
}

bool isLayerChar(char c) {
    return (c >= '0' && c <= '9') || c == 'a' || c == 'b';
}

// Does chunk `tag` hold entries of `base` ("ACTR", "TRES", "SCOB", "PLYR", "SCLS")? allLayers:
// wwrando's entries_by_type (the default chunk and every layer chunk), else the default chunk only.
bool tagIs(const char* tag, const char* base, bool allLayers) {
    if (memcmp(tag, base, 4) == 0) {
        return true;
    }
    return allLayers && memcmp(tag, base, 3) == 0 && isLayerChar(tag[3]) &&
           (strcmp(base, "ACTR") == 0 || strcmp(base, "TRES") == 0 || strcmp(base, "SCOB") == 0);
}

bool nameIs(const uint8_t* e, const char* name) {
    return strncmp((const char*)e, name, 8) == 0;
}

struct Ref {
    char tag[5];
    uint32_t index;
    uint8_t* e;
};

// Calls fn(ref) for every entry of `base` in chunk order; stops when fn returns false.
template <typename Fn>
void forEach(const PcRandoDzx* dzx, const char* base, bool allLayers, Fn fn) {
    for (uint32_t row = 0; row < dzx->chunkCount; row++) {
        Ref r;
        rowTag(dzx, row, r.tag);
        if (!tagIs(r.tag, base, allLayers)) {
            continue;
        }
        const uint32_t n = pc_rando_dzx_count(dzx, r.tag);
        for (uint32_t i = 0; i < n; i++) {
            r.index = i;
            r.e = pc_rando_dzx_entry(dzx, r.tag, i);
            if (r.e == nullptr || !fn(r)) {
                return;
            }
        }
    }
}

// wwrando's next(x for x in entries if x.name == name), or false.
bool findNamed(const PcRandoDzx* dzx, const char* base, bool allLayers, const char* name, Ref* out) {
    bool found = false;
    forEach(dzx, base, allLayers, [&](const Ref& r) {
        if (nameIs(r.e, name)) {
            *out = r;
            found = true;
            return false;
        }
        return true;
    });
    return found;
}

// The PLYR whose spawn_id (z_rot & 0xFF, byte 0x1D) is `id`.
bool findSpawn(const PcRandoDzx* dzx, int id, Ref* out) {
    bool found = false;
    forEach(dzx, "PLYR", false, [&](const Ref& r) {
        if (r.e[0x1D] == id) {
            *out = r;
            found = true;
            return false;
        }
        return true;
    });
    return found;
}

PcRandoDzxFingerprint nameFp(const char* name) {
    PcRandoDzxFingerprint fp = {name, -1, nullptr, 0};
    return fp;
}

// --- the tweaks ---

// #4 remove_forsaken_fortress_2_cutscenes and #24 fix_helmaroc_king_table_softlock (M2tower/Room0).
void patchM2tower(PcRandoResFile* f, PcRandoDzx* dzx) {
    Ref r;
    bool ok = findSpawn(dzx, 16, &r) &&
              pc_rando_dzx_set_u8(dzx, r.tag, r.index, 0x08, 0xFF, nullptr) == PCR_DZX_OK; // evnt_index
    count(PCR_SA_FF2_CUTSCENES, ok, f, "PLYR spawn 16 evnt_index 0xFF");
    // The exit to Ganon's room (optional in wwrando: `next(..., None)`).
    if (findNamed(dzx, "SCLS", false, "M2ganon", &r)) {
        uint8_t exit[10] = {'s', 'e', 'a', 0, 0, 0, 0, 0, 0 /* spawn_id */, 1 /* room_index */};
        PcRandoDzxFingerprint fp = nameFp("M2ganon");
        ok = pc_rando_dzx_set_field(dzx, r.tag, r.index, 0, exit, sizeof exit, &fp) == PCR_DZX_OK;
        count(PCR_SA_FF2_CUTSCENES, ok, f, "SCLS M2ganon -> sea room 1 spawn 0");
    } else {
        logf("[cos] rando: stagesA %s: %s/%s has no SCLS to M2ganon (wwrando skips it too)\n",
             kNames[PCR_SA_FF2_CUTSCENES], f->stage, f->arc);
    }
    ok = findNamed(dzx, "ACTR", false, "Otble", &r);
    if (ok) {
        PcRandoDzxFingerprint fp = nameFp("Otble");
        ok = pc_rando_dzx_suppress(dzx, r.tag, r.index, &fp) == PCR_DZX_OK;
    }
    count(PCR_SA_HELMAROC, ok, f, "ACTR Otble removed");
}

// #5 add_ganons_tower_warp_to_ff2 (sea/Room1): the ACT2 Warpmj copied, every field, to ACT1.
void patchSeaRoom1(PcRandoResFile* f, PcRandoDzx* dzx) {
    Ref r;
    bool ok = findNamed(dzx, "ACT2", false, "Warpmj", &r);
    if (ok) {
        uint8_t entry[0x20];
        memcpy(entry, r.e, sizeof entry);
        PcRandoDzxAdd add = {"ACT1", entry, 1};
        uint32_t first = 0;
        const int res = pc_rando_res_grow_dzx(f, &add, 1, &first);
        ok = res == PCR_DZX_OK;
        if (!ok) {
            logf("[cos] rando: stagesA gt-warp: grow %s\n", pc_rando_dzx_result_name(res));
        }
    }
    count(PCR_SA_GT_WARP, ok, f, "ACT2 Warpmj copied to ACT1");
}

// #10 remove_makar_kidnapping_event (kaze/Room3).
void patchKazeRoom3(PcRandoResFile* f, PcRandoDzx* dzx) {
    Ref andSw, wiz;
    const bool haveAnd = findNamed(dzx, "ACTR", false, "AND_SW2", &andSw);
    const bool haveWiz = findNamed(dzx, "ACTR", false, "wiz_r", &wiz);
    bool ok = haveAnd;
    if (ok) {
        PcRandoDzxFingerprint fp = nameFp("AND_SW2");
        ok = pc_rando_dzx_suppress(dzx, andSw.tag, andSw.index, &fp) == PCR_DZX_OK;
    }
    count(PCR_SA_MAKAR, ok, f, "ACTR AND_SW2 removed");
    ok = haveWiz;
    if (ok) {
        PcRandoDzxFingerprint fp = nameFp("wiz_r");
        // enable_spawn_switch: params mask 0x00FF0000, the byte at +0x09
        ok = pc_rando_dzx_set_u8(dzx, wiz.tag, wiz.index, 0x09, 0xFF, &fp) == PCR_DZX_OK;
    }
    count(PCR_SA_MAKAR, ok, f, "ACTR wiz_r enable_spawn_switch 0xFF");
}

// #11 disable_invisible_walls (M_NewD2/Room2).
void patchDrcRoom2(PcRandoResFile* f, PcRandoDzx* dzx) {
    Ref r;
    bool ok = findNamed(dzx, "SCOB", true, "Akabe", &r);
    if (ok) {
        PcRandoDzxFingerprint fp = nameFp("Akabe");
        // disable_spawn_switch: params mask 0x000000FF, the byte at +0x0B
        ok = pc_rando_dzx_set_u8(dzx, r.tag, r.index, 0x0B, 0xFF, &fp) == PCR_DZX_OK;
    }
    count(PCR_SA_WALLS, ok, f, "SCOB Akabe disable_spawn_switch 0xFF");
}

void putBe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

void putBe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void putBeF32(uint8_t* p, float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    putBe32(p, u);
}

// A SW_C00 SCOB as prevent_door_boulder_softlocks builds it (wwrando's SCOB defaults: enemy_number
// 0xFFFF, padding 0xFF).
void buildSwc00(uint8_t* e, uint8_t switchToSet, float x, float y, float z, uint16_t yRot) {
    memset(e, 0, 0x24);
    memcpy(e, "SW_C00", 6);
    // params 0x0000FF00 | switch_to_set (mask 0xFF) | behavior_type 3 (mask 0x00030000)
    putBe32(e + 0x08, 0x0000FF00u | switchToSet | (3u << 16));
    putBeF32(e + 0x0C, x);
    putBeF32(e + 0x10, y);
    putBeF32(e + 0x14, z);
    putBe16(e + 0x18, 0);
    putBe16(e + 0x1A, yRot);
    putBe16(e + 0x1C, 0xFFFF);
    putBe16(e + 0x1E, 0xFFFF);
    e[0x20] = 32;
    e[0x21] = 16;
    e[0x22] = 16;
    e[0x23] = 0xFF;
}

uint8_t sSwc00[2][0x24];
bool sSwc00Built = false;

void buildSwc00s() {
    if (sSwc00Built) {
        return;
    }
    sSwc00Built = true;
    buildSwc00(sSwc00[0], 5, 2635.0f, 0.0f, 227.0f, 0xC000);
    buildSwc00(sSwc00[1], 6, -4002.0f, 1950.0f, -2156.0f, 0xA000);
}

// #13 prevent_door_boulder_softlocks (M_NewD2/Room13 and Room14).
void patchDrcBoulder(PcRandoResFile* f, int room) {
    const uint8_t* e = pc_rando_stages_a_swc00(room);
    PcRandoDzxAdd add = {"SCOB", e, 1};
    uint32_t first = 0;
    const int res = pc_rando_res_grow_dzx(f, &add, 1, &first);
    if (res != PCR_DZX_OK) {
        logf("[cos] rando: stagesA boulders: grow %s\n", pc_rando_dzx_result_name(res));
    }
    count(PCR_SA_BOULDERS, res == PCR_DZX_OK, f, room == 13 ? "+ SCOB SW_C00 switch 5" : "+ SCOB SW_C00 switch 6");
}

// x / z += d, computed as Python does (the f32 widened to double, the sum rounded back to f32).
int addF32(PcRandoDzx* dzx, const Ref& r, uint32_t off, double d) {
    const double v = (double)pc_rando_dzx_bef32(r.e + off) + d;
    return pc_rando_dzx_set_f32(dzx, r.tag, r.index, off, (float)v, nullptr);
}

// #14 fix_ghost_ship_chest_crash (PShip/Stage stage.dzs).
void patchGhostShip(PcRandoResFile* f, PcRandoDzx* dzx) {
    int moved = 0, bad = 0;
    forEach(dzx, "TRES", true, [&](const Ref& r) {
        if ((pc_rando_dzx_be16(r.e + 0x18) & 0x3F) == 2) { // room_num: x_rot & 0x3F
            return true;
        }
        if (addF32(dzx, r, 0x0C, PCR_SA_GHOST_SHIP_DX) == PCR_DZX_OK) {
            moved++;
        } else {
            bad++;
        }
        return true;
    });
    char what[64];
    snprintf(what, sizeof what, "%d TRES with room_num != 2 moved x += 2000", moved);
    count(PCR_SA_GHOST_SHIP, moved > 0 && bad == 0, f, what);
}

// #17 fix_totg_warp_out_spawn_pos (sea/Room26).
void patchTotgSpawn(PcRandoResFile* f, PcRandoDzx* dzx) {
    Ref r;
    const bool ok = findSpawn(dzx, 1, &r) && addF32(dzx, r, 0x14, PCR_SA_TOTG_DZ) == PCR_DZX_OK;
    count(PCR_SA_TOTG, ok, f, "PLYR spawn 1 z += 1000");
}

// #18 remove_phantom_ganon_requirement_from_eye_reefs (sea/Room24, 46, 22, 8, 37, 25).
void patchEyeReef(PcRandoResFile* f, PcRandoDzx* dzx) {
    int cannons = 0, ships = 0, bad = 0;
    forEach(dzx, "ACTR", true, [&](const Ref& r) {
        if (nameIs(r.e, "Ocanon") && r.e[0x0A] == 0x2A) { // enable_spawn_switch: params mask 0xFF00
            const uint8_t was = 0x2A;
            PcRandoDzxFingerprint fp = {"Ocanon", 0x0A, &was, 1};
            if (pc_rando_dzx_set_u8(dzx, r.tag, r.index, 0x0A, 0xFF, &fp) == PCR_DZX_OK) {
                cannons++;
            } else {
                bad++;
            }
        } else if (nameIs(r.e, "Oship") && r.e[0x19] == 0x2A) { // x_rot & 0xFF
            const uint8_t was = 0x2A;
            PcRandoDzxFingerprint fp = {"Oship", 0x19, &was, 1};
            if (pc_rando_dzx_set_u8(dzx, r.tag, r.index, 0x19, 0xFF, &fp) == PCR_DZX_OK) {
                ships++;
            } else {
                bad++;
            }
        }
        return true;
    });
    char what[80];
    snprintf(what, sizeof what, "%d Ocanon and %d Oship switch 0x2A -> 0xFF", cannons, ships);
    count(PCR_SA_EYE_REEFS, cannons + ships > 0 && bad == 0, f, what);
}

// #26 prevent_fairy_island_softlocks (sea/Room15).
void patchFairySpawn(PcRandoResFile* f, PcRandoDzx* dzx) {
    Ref r;
    const bool ok = findSpawn(dzx, 1, &r) &&
                    pc_rando_dzx_set_f32(dzx, r.tag, r.index, 0x0C, PCR_SA_FAIRY_X, nullptr) == PCR_DZX_OK;
    count(PCR_SA_FAIRY, ok, f, "PLYR spawn 1 x = -320170");
}

bool isEyeReef(int room) {
    return room == 24 || room == 46 || room == 22 || room == 8 || room == 37 || room == 25;
}

} // namespace

extern "C" {

void pc_rando_stages_a_patch(PcRandoResFile* f) {
    if (f == nullptr || f->stage == nullptr || f->name == nullptr) {
        return;
    }
    const bool dzr = strcmp(f->name, "room.dzr") == 0;
    const bool dzs = strcmp(f->name, "stage.dzs") == 0;
    if (!dzr && !dzs) {
        return;
    }
    enum Which { kNone, kM2tower, kSea1, kKaze3, kDrc2, kDrcBoulder, kGhost, kTotg, kEye, kFairy } w = kNone;
    const char* s = f->stage;
    if (dzr && strcmp(s, "M2tower") == 0 && f->room == 0) {
        w = kM2tower;
    } else if (dzr && strcmp(s, "sea") == 0) {
        w = f->room == 1 ? kSea1 : f->room == 26 ? kTotg : f->room == 15 ? kFairy : isEyeReef(f->room) ? kEye : kNone;
    } else if (dzr && strcmp(s, "kaze") == 0 && f->room == 3) {
        w = kKaze3;
    } else if (dzr && strcmp(s, "M_NewD2") == 0) {
        w = f->room == 2 ? kDrc2 : (f->room == 13 || f->room == 14) ? kDrcBoulder : kNone;
    } else if (dzs && strcmp(s, "PShip") == 0 && strcmp(f->arc, "Stage") == 0) {
        w = kGhost;
    }
    if (w == kNone) {
        return;
    }
    if (w == kDrcBoulder) {
        patchDrcBoulder(f, f->room);
        return;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        logf("[cos] rando: stagesA: %s/%s/%s does not parse; left vanilla\n", f->stage, f->arc, f->name);
        return;
    }
    switch (w) {
    case kM2tower: patchM2tower(f, &dzx); break;
    case kSea1: patchSeaRoom1(f, &dzx); break;
    case kKaze3: patchKazeRoom3(f, &dzx); break;
    case kDrc2: patchDrcRoom2(f, &dzx); break;
    case kGhost: patchGhostShip(f, &dzx); break;
    case kTotg: patchTotgSpawn(f, &dzx); break;
    case kEye: patchEyeReef(f, &dzx); break;
    case kFairy: patchFairySpawn(f, &dzx); break;
    default: break;
    }
}

void pc_rando_stages_a_stats(PcRandoStagesAStats* out) {
    if (out != nullptr) {
        *out = sStats;
    }
}

void pc_rando_stages_a_reset_stats(void) {
    memset(&sStats, 0, sizeof sStats);
}

void pc_rando_stages_a_count(int t, int ok) {
    if (t < 0 || t >= PCR_SA_COUNT) {
        return;
    }
    if (ok) {
        sStats.applied[t]++;
    } else {
        sStats.failed[t]++;
    }
}

const char* pc_rando_stages_a_name(int t) {
    return t >= 0 && t < PCR_SA_COUNT ? kNames[t] : "?";
}

const uint8_t* pc_rando_stages_a_swc00(int room) {
    buildSwc00s();
    return room == 13 ? sSwc00[0] : room == 14 ? sSwc00[1] : nullptr;
}

} // extern "C"
