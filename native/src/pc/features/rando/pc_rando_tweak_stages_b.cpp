// Stage tweaks B (pc_rando_tweak_stages_b.h, docs/RANDOMIZER_PLAN.md step R3.16): the dzs/dzr edits
// of tweaks.py add_failsafe_id_0_spawns, add_spawns_outside_boss_doors, remove_minor_panning_cutscenes,
// add_shortcut_warps_into_dungeons, add_more_magic_jars and add_hint_signs (wwrando 1.10.0), applied
// by the archive-load hook while pc_rando() is 1. Pure: no game headers, no game calls.
//
// wwlib/dzx.py semantics this file follows:
//   - entries_by_type(T) walks every chunk of base type T in the file's chunk order, layers included
//     (ACTR + ACT0..ACTb, SCOB + SCO0..SCOb); entries_by_type_and_layer(T, Default) only the base
//     chunk;
//   - remove_entity clears the 8-byte name (pc_rando_dzx_suppress), so indexes do not move;
//   - add_entity appends to the chunk of that type (pc_rando_res_grow_dzx), with the class defaults:
//     ACTR rotation 0 and enemy_number 0xFFFF, SCOB also scale 10 and padding 0xFF, PLYR also the
//     name "Link", unknown_param_4 0xFF, evnt_index 0xFF and ship_id 0xFF;
//   - floats are Python doubles written as float32 (round to nearest).
#include "pc_rando_tweak_stages_b.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_msg.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

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

constexpr uint32_t kActrSize = 0x20;
constexpr uint32_t kScobSize = 0x24;
constexpr uint16_t kHintMsg = 847;
constexpr uint8_t kSmallMagic = 0x09; // item_names.txt "Small Magic Jar (Pickup)"
constexpr uint8_t kLargeMagic = 0x0A; // "Large Magic Jar (Pickup)"
constexpr uint32_t kWarpParams = 0x10FF067Fu;  // Ysdls00: type 1, event 0xFF, exit 6, switch 0x7F
constexpr uint32_t kSwc00Params = 0x0003FF7Fu; // SW_C00: behavior 3, prerequisite 0xFF, switch 0x7F
constexpr uint32_t kGrassParams = 0x00000E00u; // kusax1: drop type 0x38, type 0, subtype 0
constexpr uint8_t kBossSpawnId = 27;

enum Kind {
    kDrop,       // #8: name[nth] (entries_by_type(ACTR)) dropped_item_id (params & 0x3F) = value
    kGrass,      // #8: two kusax1 on the small islands around Dragon Roost Island
    kSign,       // #12: default-layer BFlower[1] -> Kanban, params 847, y_rot 0x2000
    kCopySpawn,  // #19: spawn 0 copied from spawn `src`
    kDoorSpawn0, // #19: spawn 0 in front of the first stage.dzs TGDR door of the room
    kBossSpawn,  // #20: spawn 27 in front of the boss door
    kPanning,    // #21: TagEv with evnt_index `evnt` removed, PLYR evnt_index `evnt` -> 0xFF
    kFhWarp,     // #22: Ysdls00 + SW_C00 on Forest Haven's shore
};

// Where a boss door and its new spawn live (add_spawns_outside_boss_doors).
enum BossShape {
    kNone,
    kDoorStageSpawnRoom,  // TGDR door20 in stage.dzs, spawn in the room's room.dzr
    kDoorStageSpawnStage, // TGDR door12 in stage.dzs, spawn in stage.dzs (Earth and Wind Temples)
    kDoorRoomSpawnRoom,   // ACT1 SMBdor in sea/Room1, spawn in the same file, ship_id 0
};

struct Site {
    int tweak;
    const char* stage;
    int room;        // the file the record lands in (-1: stage.dzs)
    const char* what;
    Kind kind;
    const char* name; // kDrop: actor name; kBossSpawn: door name
    int nth;          // kDrop: index among the named entries
    int value;        // kDrop: item; kCopySpawn: the spawn to copy; kPanning: evnt_index;
                      // kBossSpawn / kDoorSpawn0: the room the spawn is for
    BossShape boss;
    // state
    int runs;
    int result;
    bool pending;          // a spawn computed from stage.dzs, waiting for its room.dzr
    uint8_t entry[0x20];   // that spawn
};

#define S_DROP(stage, room, name, nth, item) \
    {PCR_SB_MAGIC_JAR, stage, room, name " drop", kDrop, name, nth, item, kNone, 0, 0, false, {}}
#define S_COPY(stage, room, src) \
    {PCR_SB_FAILSAFE_SPAWN, stage, room, "spawn 0 (copy)", kCopySpawn, nullptr, 0, src, kNone, 0, 0, false, {}}
#define S_DOOR0(stage, room) \
    {PCR_SB_FAILSAFE_SPAWN, stage, room, "spawn 0 (door)", kDoorSpawn0, nullptr, 0, room, kNone, 0, 0, false, {}}
#define S_BOSS(stage, file, room, door, shape) \
    {PCR_SB_BOSS_SPAWN, stage, file, "spawn 27", kBossSpawn, door, 0, room, shape, 0, 0, false, {}}
#define S_PAN(stage, room, evnt) \
    {PCR_SB_PANNING, stage, room, "TagEv / spawn event", kPanning, nullptr, 0, evnt, kNone, 0, 0, false, {}}

// In tweaks.py's order (apply_necessary_tweaks), so appends to one chunk come in wwrando's order.
Site sSites[] = {
    // add_more_magic_jars
    S_DROP("M_NewD2", 2, "Odokuro", 2, kSmallMagic),
    S_DROP("M_NewD2", 2, "Odokuro", 5, kLargeMagic),
    S_DROP("M_NewD2", 10, "Odokuro", 0, kLargeMagic),
    S_DROP("M_NewD2", 10, "Odokuro", 9, kLargeMagic),
    {PCR_SB_MAGIC_JAR, "sea", 13, "kusax1 x2", kGrass, nullptr, 0, 0, kNone, 0, 0, false, {}},
    S_DROP("Siren", 14, "kotubo", 1, kLargeMagic),
    // add_hint_signs
    {PCR_SB_HINT_SIGN, "M_NewD2", 2, "Kanban", kSign, nullptr, 0, 0, kNone, 0, 0, false, {}},
    // add_failsafe_id_0_spawns
    S_COPY("Asoko", 0, 255),
    S_COPY("I_TestM", 0, 1),
    S_COPY("M_Dai", 20, 23),
    S_COPY("M_NewD2", 1, 20),
    S_COPY("M_NewD2", 2, 1),
    S_COPY("M_NewD2", 3, 6),
    S_COPY("M_NewD2", 4, 7),
    S_COPY("M_NewD2", 6, 9),
    S_COPY("M_NewD2", 8, 14),
    S_COPY("M_NewD2", 11, 2),
    S_COPY("M_NewD2", 12, 3),
    S_COPY("M_NewD2", 13, 4),
    S_COPY("M_NewD2", 14, 5),
    S_COPY("M_NewD2", 15, 18),
    S_COPY("TF_06", 1, 1),
    S_COPY("TF_06", 2, 2),
    S_COPY("TF_06", 3, 2),
    S_COPY("TF_06", 4, 2),
    S_COPY("TF_06", 5, 2),
    S_COPY("TF_06", 6, 6),
    S_COPY("ma2room", 1, 2),
    S_COPY("ma2room", 2, 15),
    S_COPY("ma2room", 3, 9),
    S_COPY("ma2room", 4, 6),
    S_COPY("ma3room", 1, 2),
    S_COPY("ma3room", 2, 15),
    S_COPY("ma3room", 3, 9),
    S_COPY("ma3room", 4, 6),
    S_COPY("majroom", 1, 2),
    S_COPY("majroom", 2, 15),
    S_COPY("majroom", 3, 9),
    S_COPY("majroom", 4, 6),
    S_DOOR0("TF_01", 1),
    S_DOOR0("TF_01", 2),
    S_DOOR0("TF_01", 3),
    S_DOOR0("TF_01", 4),
    S_DOOR0("TF_01", 5),
    S_DOOR0("TF_01", 6),
    S_DOOR0("TF_02", 1),
    S_DOOR0("TF_02", 2),
    S_DOOR0("TF_02", 3),
    S_DOOR0("TF_02", 4),
    S_DOOR0("TF_02", 5),
    S_DOOR0("TF_02", 6),
    // add_spawns_outside_boss_doors
    S_BOSS("M_NewD2", 10, 10, "door20", kDoorStageSpawnRoom),
    S_BOSS("kindan", 16, 16, "door20", kDoorStageSpawnRoom),
    S_BOSS("Siren", 18, 18, "door20", kDoorStageSpawnRoom),
    S_BOSS("sea", 1, 1, "SMBdor", kDoorRoomSpawnRoom),
    S_BOSS("M_Dai", -1, 15, "door12", kDoorStageSpawnStage),
    S_BOSS("kaze", -1, 12, "door12", kDoorStageSpawnStage),
    // remove_minor_panning_cutscenes
    S_PAN("M_NewD2", 2, 4),
    S_PAN("kindan", -1, 2),
    S_PAN("Siren", 18, 2),
    S_PAN("M_Dai", 3, 7),
    S_PAN("sea", 41, 19),
    S_PAN("sea", 41, 22),
    S_PAN("sea", 41, 23),
    // add_shortcut_warps_into_dungeons
    {PCR_SB_FH_WARP, "sea", 41, "Ysdls00 + SW_C00", kFhWarp, nullptr, 0, 0, kNone, 0, 0, false, {}},
};

constexpr int kSiteCount = (int)(sizeof sSites / sizeof sSites[0]);

// --- big-endian helpers ---

uint32_t be32(const uint8_t* p) {
    return pc_rando_dzx_be32(p);
}
uint16_t be16(const uint8_t* p) {
    return pc_rando_dzx_be16(p);
}
void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void putf(uint8_t* p, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    put32(p, u);
}

bool nameIs(const uint8_t* e, const char* name) {
    char n[8] = {};
    strncpy(n, name, sizeof n);
    return memcmp(e, n, 8) == 0;
}

// The base type of a chunk tag ("ACT3" -> "ACTR"), as wwlib's Chunk.read decides it.
bool hasBase(const uint8_t* row, const char* base) {
    if (memcmp(row, base, 4) == 0) {
        return true;
    }
    const char c = (char)row[3];
    const bool layer = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'b');
    return layer && memcmp(row, base, 3) == 0 &&
           (strcmp(base, "ACTR") == 0 || strcmp(base, "TRES") == 0 || strcmp(base, "SCOB") == 0);
}

// Walks entries_by_type(base) (all layers, chunk order); calls fn(entry) until it returns true.
// Returns the entry fn accepted, or nullptr. The file was checked by pc_rando_dzx_open.
template <class Fn>
uint8_t* walk(const PcRandoDzx* dzx, const char* base, uint32_t esize, Fn fn) {
    for (uint32_t c = 0; c < dzx->chunkCount; c++) {
        const uint8_t* row = dzx->data + 4 + c * 0xC;
        if (!hasBase(row, base)) {
            continue;
        }
        const uint32_t n = be32(row + 4);
        uint8_t* e = dzx->data + be32(row + 8);
        for (uint32_t i = 0; i < n; i++, e += esize) {
            if (fn(e)) {
                return e;
            }
        }
    }
    return nullptr;
}

// The nth entry named `name` of entries_by_type(base), or nullptr.
uint8_t* nthNamed(const PcRandoDzx* dzx, const char* base, uint32_t esize, const char* name, int nth) {
    int k = 0;
    return walk(dzx, base, esize, [&](uint8_t* e) { return nameIs(e, name) && k++ == nth; });
}

// The default-layer chunk only (entries_by_type_and_layer(T, Default)).
uint8_t* nthNamedDefault(const PcRandoDzx* dzx, const char* tag, uint32_t esize, const char* name, int nth) {
    const uint32_t n = pc_rando_dzx_count(dzx, tag);
    int k = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
        if (e != nullptr && nameIs(e, name) && k++ == nth) {
            return e;
        }
    }
    (void)esize;
    return nullptr;
}

uint8_t* spawnWithId(const PcRandoDzx* dzx, int id) {
    // spawn_id is the low byte of z_rot (+0x1D).
    return walk(dzx, "PLYR", kActrSize, [&](uint8_t* e) { return e[0x1D] == (uint8_t)id; });
}

// A new PLYR (wwlib PLYR() defaults, then the fields tweaks.py sets).
void makeSpawn(uint8_t out[0x20], int spawnType, int roomNum, float x, float y, float z, uint16_t yRot,
               uint8_t shipId, uint8_t spawnId) {
    memset(out, 0, 0x20);
    memcpy(out, "Link", 4);
    put32(out + 0x08, 0xFFFF0000u | ((uint32_t)(spawnType & 0xF) << 12) | (uint32_t)(roomNum & 0x3F));
    putf(out + 0x0C, x);
    putf(out + 0x10, y);
    putf(out + 0x14, z);
    put16(out + 0x18, 0);
    put16(out + 0x1A, yRot);
    put16(out + 0x1C, (uint16_t)(shipId << 8 | spawnId));
    put16(out + 0x1E, 0xFFFF);
}

// tweaks.py's spawn placement: 200 units along y_rot from the door, as Python doubles. The volatile
// stores keep the compiler from fusing the multiply and the add (Python rounds each step).
void inFrontOf(const uint8_t* door, uint16_t yRot, float* x, float* z) {
    const double deg = (double)yRot * (90.0 / 0x4000);
    const double rad = deg * (3.14159265358979323846 / 180.0);
    volatile double xOff = std::sin(rad);
    xOff = xOff * 200;
    volatile double zOff = std::cos(rad);
    zOff = zOff * 200;
    volatile double xs = (double)pc_rando_dzx_bef32(door + 0x0C);
    xs = xs + xOff;
    volatile double zs = (double)pc_rando_dzx_bef32(door + 0x14);
    zs = zs + zOff;
    *x = (float)xs;
    *z = (float)zs;
}

int doorFrom(const uint8_t* door) {
    return be16(door + 0x18) & 0x3F;
}
int doorTo(const uint8_t* door) {
    return (be16(door + 0x18) >> 6) & 0x3F;
}

// add_spawns_outside_boss_doors for one site: the door must be the only match.
int bossSpawn(const PcRandoDzx* dzx, const Site& s, uint8_t out[0x20]) {
    const bool sea = s.boss == kDoorRoomSpawnRoom;
    const char* tag = sea ? "ACT1" : "TGDR";
    const uint32_t n = pc_rando_dzx_count(dzx, tag);
    const uint8_t* door = nullptr;
    int matches = 0;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
        if (e == nullptr || !nameIs(e, s.name)) {
            continue;
        }
        if (!sea && !(doorFrom(e) == doorTo(e) && doorTo(e) == s.value)) {
            continue;
        }
        door = e;
        matches++;
    }
    if (matches != 1) {
        return matches == 0 ? PCR_DZX_NO_CHUNK : PCR_DZX_MISMATCH;
    }
    uint16_t yRot = be16(door + 0x1A);
    if (doorFrom(door) != s.value && doorFrom(door) != 63) {
        yRot = (uint16_t)((yRot + 0x8000) % 0x10000);
    }
    float x, z;
    inFrontOf(door, yRot, &x, &z);
    makeSpawn(out, 0, s.value, x, pc_rando_dzx_bef32(door + 0x10), z, yRot, sea ? 0 : 0xFF, kBossSpawnId);
    return PCR_DZX_OK;
}

// add_failsafe_id_0_spawns for a room without spawns: the first stage door that touches the room.
int doorSpawn0(const PcRandoDzx* dzs, const Site& s, uint8_t out[0x20]) {
    const uint32_t n = pc_rando_dzx_count(dzs, "TGDR");
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* door = pc_rando_dzx_entry(dzs, "TGDR", i);
        if (door == nullptr || (doorFrom(door) != s.value && doorTo(door) != s.value)) {
            continue;
        }
        uint16_t yRot = be16(door + 0x1A);
        if (doorFrom(door) != s.value) {
            yRot = (uint16_t)((yRot + 0x8000) % 0x10000);
        }
        float x, z;
        inFrontOf(door, yRot, &x, &z);
        makeSpawn(out, 0, s.value, x, pc_rando_dzx_bef32(door + 0x10), z, yRot, 0xFF, 0);
        return PCR_DZX_OK;
    }
    return PCR_DZX_NO_CHUNK;
}

void makeScob(uint8_t out[kScobSize], const char* name, uint32_t params, float x, float y, float z,
              uint8_t scale) {
    memset(out, 0, kScobSize);
    strncpy((char*)out, name, 8);
    put32(out + 0x08, params);
    putf(out + 0x0C, x);
    putf(out + 0x10, y);
    putf(out + 0x14, z);
    put16(out + 0x1E, 0xFFFF);
    out[0x20] = out[0x21] = out[0x22] = scale;
    out[0x23] = 0xFF;
}

void makeGrass(uint8_t out[kActrSize], float x, float y, float z) {
    memset(out, 0, kActrSize);
    memcpy(out, "kusax1", 6);
    put32(out + 0x08, kGrassParams);
    putf(out + 0x0C, x);
    putf(out + 0x10, y);
    putf(out + 0x14, z);
    put16(out + 0x1E, 0xFFFF);
}

// The two grass patches (add_more_magic_jars) and the Forest Haven warp (add_shortcut_warps_into_
// dungeons). The positions are tweaks.py's double literals rounded to float32.
void grassEntries(uint8_t out[2][kActrSize]) {
    makeGrass(out[0], (float)209694.0, (float)1900.0, (float)-202463.0);
    makeGrass(out[1], (float)209333.0, (float)1300.0, (float)-210145.0);
}
void warpEntries(uint8_t out[2][kScobSize]) {
    makeScob(out[0], "Ysdls00", kWarpParams, (float)217178.1, (float)34.99997, (float)195407.7, 10);
    makeScob(out[1], "SW_C00", kSwc00Params, (float)196755.7, (float)2952.929, (float)198147.1, 3 * 0x10);
}

bool sameStage(const Site& s, const PcRandoResFile* f) {
    return strcmp(s.stage, f->stage) == 0;
}

void finish(Site& s, int result) {
    s.runs++;
    if (result != PCR_DZX_OK && (s.result != result || s.runs == 1)) {
        logf("[cos] rando: stage tweak #%d %s/%d %s: %s; left vanilla\n", s.tweak, s.stage, s.room, s.what,
             pc_rando_dzx_result_name(result));
    }
    s.result = result;
}

// stage.dzs: compute the spawns that a stage door gives to a room file.
void prepareFromStage(const PcRandoResFile* f, const PcRandoDzx* dzs) {
    for (Site& s : sSites) {
        if (!sameStage(s, f) || s.room < 0) {
            continue;
        }
        if (s.kind == kDoorSpawn0) {
            s.pending = doorSpawn0(dzs, s, s.entry) == PCR_DZX_OK;
        } else if (s.kind == kBossSpawn && s.boss == kDoorStageSpawnRoom) {
            s.pending = bossSpawn(dzs, s, s.entry) == PCR_DZX_OK;
        }
    }
}

constexpr int kMaxAdd = 8;

struct Adds {
    PcRandoDzxAdd list[kMaxAdd];
    uint8_t bytes[kMaxAdd][kScobSize];
    Site* site[kMaxAdd];
    int n = 0;
    // Queues one entry for `tag` (one batch per entry; pc_rando_dzx_grow appends same-tag batches in order).
    bool push(Site* s, const char* tag, const uint8_t* e, uint32_t size) {
        if (n >= kMaxAdd) {
            return false;
        }
        memcpy(bytes[n], e, size);
        list[n] = {tag, bytes[n], 1};
        site[n] = s;
        n++;
        return true;
    }
};

void patchFile(PcRandoResFile* f) {
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        return;
    }
    if (f->room < 0) {
        prepareFromStage(f, &dzx);
    }
    Adds adds;
    Site* addSites[kSiteCount] = {};
    int nAddSites = 0;
    for (Site& s : sSites) {
        if (!sameStage(s, f) || s.room != f->room) {
            continue;
        }
        int r = PCR_DZX_OK;
        switch (s.kind) {
        case kDrop: {
            uint8_t* e = nthNamed(&dzx, "ACTR", kActrSize, s.name, s.nth);
            if (e == nullptr) {
                r = PCR_DZX_MISMATCH;
                break;
            }
            put32(e + 8, (be32(e + 8) & ~0x3Fu) | ((uint32_t)s.value & 0x3F));
            break;
        }
        case kSign: {
            uint8_t* e = nthNamedDefault(&dzx, "ACTR", kActrSize, "BFlower", 1);
            if (e == nullptr) {
                r = PCR_DZX_MISMATCH;
                break;
            }
            memset(e, 0, 8);
            memcpy(e, "Kanban", 6);
            put32(e + 8, kHintMsg);
            put16(e + 0x1A, 0x2000);
            break;
        }
        case kGrass: {
            uint8_t g[2][kActrSize];
            grassEntries(g);
            r = adds.push(&s, "ACTR", g[0], kActrSize) && adds.push(&s, "ACTR", g[1], kActrSize)
                    ? PCR_DZX_OK
                    : PCR_DZX_NO_SPACE;
            break;
        }
        case kCopySpawn: {
            const uint8_t* src = spawnWithId(&dzx, s.value);
            if (src == nullptr) {
                r = PCR_DZX_MISMATCH;
                break;
            }
            if (spawnWithId(&dzx, 0) != nullptr) {
                r = PCR_DZX_MISMATCH; // the room already has a spawn 0: not the vanilla file
                break;
            }
            const uint32_t p = be32(src + 8);
            uint8_t e[0x20];
            makeSpawn(e, (int)(p >> 12) & 0xF, (int)(p & 0x3F), pc_rando_dzx_bef32(src + 0x0C),
                      pc_rando_dzx_bef32(src + 0x10), pc_rando_dzx_bef32(src + 0x14), be16(src + 0x1A), 0xFF, 0);
            r = adds.push(&s, "PLYR", e, kActrSize) ? PCR_DZX_OK : PCR_DZX_NO_SPACE;
            break;
        }
        case kDoorSpawn0:
            if (!s.pending) {
                r = PCR_DZX_NO_CHUNK; // the stage's door was not seen (stage.dzs not dispatched)
            } else if (spawnWithId(&dzx, 0) != nullptr) {
                r = PCR_DZX_MISMATCH;
            } else {
                r = adds.push(&s, "PLYR", s.entry, kActrSize) ? PCR_DZX_OK : PCR_DZX_NO_SPACE;
            }
            break;
        case kBossSpawn: {
            uint8_t e[0x20];
            if (s.boss == kDoorStageSpawnRoom) {
                if (!s.pending) {
                    r = PCR_DZX_NO_CHUNK;
                    break;
                }
                memcpy(e, s.entry, sizeof e);
            } else {
                r = bossSpawn(&dzx, s, e);
                if (r != PCR_DZX_OK) {
                    break;
                }
            }
            if (spawnWithId(&dzx, kBossSpawnId) != nullptr) {
                r = PCR_DZX_MISMATCH;
                break;
            }
            r = adds.push(&s, "PLYR", e, kActrSize) ? PCR_DZX_OK : PCR_DZX_NO_SPACE;
            break;
        }
        case kPanning: {
            const uint8_t ev = (uint8_t)s.value;
            walk(&dzx, "SCOB", kScobSize, [&](uint8_t* e) {
                if (nameIs(e, "TagEv") && e[8] == ev) {
                    memset(e, 0, 8);
                }
                return false;
            });
            walk(&dzx, "PLYR", kActrSize, [&](uint8_t* e) {
                if (e[8] == ev) {
                    e[8] = 0xFF;
                }
                return false;
            });
            break;
        }
        case kFhWarp: {
            uint8_t w[2][kScobSize];
            warpEntries(w);
            r = adds.push(&s, "SCOB", w[0], kScobSize) && adds.push(&s, "SCOB", w[1], kScobSize)
                    ? PCR_DZX_OK
                    : PCR_DZX_NO_SPACE;
            break;
        }
        }
        bool queued = false;
        for (int i = 0; i < adds.n; i++) {
            queued = queued || adds.site[i] == &s;
        }
        if (queued && r == PCR_DZX_OK) {
            addSites[nAddSites++] = &s; // finished after the grow
        } else {
            finish(s, r);
        }
    }
    if (adds.n == 0) {
        return;
    }
    const int r = pc_rando_res_grow_dzx(f, adds.list, adds.n, nullptr);
    for (int i = 0; i < nAddSites; i++) {
        finish(*addSites[i], r);
    }
}

void onRando(int active) {
    if (active) {
        pc_rando_tweak_stages_b_add_messages();
    }
}

bool fail(char* why, uint32_t cap, const Site& s, const char* msg) {
    if (why != nullptr && cap > 0) {
        snprintf(why, cap, "#%d %s/%d %s: %s", s.tweak, s.stage, s.room, s.what, msg);
    }
    return false;
}

bool verifySite(const PcRandoDzx* dzx, const Site& s, char* why, uint32_t cap) {
    switch (s.kind) {
    case kDrop: {
        const uint8_t* e = nthNamed(dzx, "ACTR", kActrSize, s.name, s.nth);
        return (e != nullptr && (be32(e + 8) & 0x3F) == (uint32_t)s.value) || fail(why, cap, s, "drop not set");
    }
    case kSign: {
        const uint8_t* e = nthNamedDefault(dzx, "ACTR", kActrSize, "Kanban", 0);
        return (e != nullptr && be32(e + 8) == kHintMsg && be16(e + 0x1A) == 0x2000) ||
               fail(why, cap, s, "no Kanban with message 847");
    }
    case kGrass: {
        uint8_t g[2][kActrSize];
        grassEntries(g);
        for (auto& want : g) {
            if (walk(dzx, "ACTR", kActrSize, [&](uint8_t* e) { return memcmp(e, want, kActrSize) == 0; }) ==
                nullptr) {
                return fail(why, cap, s, "grass missing");
            }
        }
        return true;
    }
    case kCopySpawn:
    case kDoorSpawn0: {
        const uint8_t* e = spawnWithId(dzx, 0);
        return (e != nullptr && nameIs(e, "Link")) || fail(why, cap, s, "no spawn 0");
    }
    case kBossSpawn: {
        const uint8_t* e = spawnWithId(dzx, kBossSpawnId);
        return (e != nullptr && (be32(e + 8) & 0x3F) == (uint32_t)s.value) || fail(why, cap, s, "no spawn 27");
    }
    case kPanning: {
        const uint8_t ev = (uint8_t)s.value;
        if (walk(dzx, "SCOB", kScobSize, [&](uint8_t* e) { return nameIs(e, "TagEv") && e[8] == ev; }) != nullptr) {
            return fail(why, cap, s, "TagEv still there");
        }
        if (walk(dzx, "PLYR", kActrSize, [&](uint8_t* e) { return e[8] == ev; }) != nullptr) {
            return fail(why, cap, s, "a spawn still starts the event");
        }
        return true;
    }
    case kFhWarp: {
        uint8_t w[2][kScobSize];
        warpEntries(w);
        for (auto& want : w) {
            if (walk(dzx, "SCOB", kScobSize, [&](uint8_t* e) { return memcmp(e, want, kScobSize) == 0; }) ==
                nullptr) {
                return fail(why, cap, s, "warp entry missing");
            }
        }
        return true;
    }
    }
    return true;
}

bool sInit = false;

} // namespace

extern "C" void pc_rando_tweak_stages_b_patch(PcRandoResFile* file) {
    if (file == nullptr || file->data == nullptr || file->stage == nullptr || file->stage[0] == 0) {
        return;
    }
    if (!(file->room < 0 ? strcmp(file->name, "stage.dzs") == 0 : strcmp(file->name, "room.dzr") == 0)) {
        return;
    }
    for (const Site& s : sSites) {
        if (strcmp(s.stage, file->stage) == 0) {
            patchFile(file);
            return;
        }
    }
}

extern "C" int pc_rando_tweak_stages_b_add_messages(void) {
    // tweaks.py add_hint_signs: a right arrow on a wooden sign, instant, centered.
    PcRandoMsgEntry e;
    pc_rando_msg_entry_defaults(&e);
    e.text_box_type = 2;     // TextBoxType.WOOD
    e.initial_draw_type = 1; // instant
    e.text_alignment = 3;    // centered
    const int r = pc_rando_msg_add(kHintMsg, &e, "\\{1A 05 00 00 15}");
    if (r != PCR_MSG_OK) {
        logf("[cos] rando: hint sign message %u not added (%d)\n", (unsigned)kHintMsg, r);
        return 0;
    }
    return 1;
}

extern "C" void pc_rando_tweak_stages_b_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_dzx_patcher(pc_rando_tweak_stages_b_patch);
    pc_rando_add_listener(onRando); // called with 1 at once when already active
}

extern "C" int pc_rando_tweak_stages_b_site_count(void) {
    return kSiteCount;
}

extern "C" const PcRandoStagesBSite* pc_rando_tweak_stages_b_site(int i) {
    static PcRandoStagesBSite out;
    if (i < 0 || i >= kSiteCount) {
        return nullptr;
    }
    const Site& s = sSites[i];
    out = {s.tweak, s.stage, s.room, s.what, s.runs, s.result};
    return &out;
}

extern "C" void pc_rando_tweak_stages_b_reset(void) {
    for (Site& s : sSites) {
        s.runs = 0;
        s.result = 0;
        s.pending = false;
    }
}

extern "C" int pc_rando_tweak_stages_b_verify(const char* stage, int room, const uint8_t* data, uint32_t size,
                                              char* why, uint32_t whyCap) {
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<uint8_t*>(data), size) != PCR_DZX_OK) {
        if (why != nullptr && whyCap > 0) {
            snprintf(why, whyCap, "%s/%d: not a dzx file", stage, room);
        }
        return 0;
    }
    for (const Site& s : sSites) {
        if (strcmp(s.stage, stage) == 0 && s.room == room && !verifySite(&dzx, s, why, whyCap)) {
            return 0;
        }
    }
    return 1;
}
