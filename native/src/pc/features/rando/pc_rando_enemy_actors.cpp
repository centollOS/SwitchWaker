// The enemy randomizer's added, removed and converted actors (pc_rando_enemy_actors.h,
// docs/RANDOMIZER_PLAN.md step R7.4b): wwrando 1.10.0_9775811 randomizers/enemies.py
// add_modify_and_replace_actors_for_enemy_rando and randomize_enemies_spawned_by_objects on the raw
// stage files. Pure: no game headers, no game calls.
#include "pc_rando_enemy_actors.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_enemies.h"
#include "pc_rando_enemy_data.h"
#include "pc_rando_gen_enemies.h"
#include "pc_rando_seedfile.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

using pcr::EnemyActorData;
using pcr::EnemyActorPath;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
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
void putf(uint8_t* p, double d) {
    const float f = (float)d; // fs.write_float: struct.pack(">f")
    uint32_t v;
    memcpy(&v, &f, 4);
    put32(p, v);
}
// fs.write_str(data, off, s, 8): at most 7 bytes, zero padded.
void putName(uint8_t* e, const char* s) {
    memset(e, 0, 8);
    memcpy(e, s, strnlen(s, 7));
}

// The 0x20 / 0x24-byte actor-like records share this head (dzx.py ACTR / SCOB / TGDR): name[8],
// params u32 0x08, x/y/z f32 0x0C/0x10/0x14, x/y/z_rot u16 0x18/0x1A/0x1C, enemy_number u16 0x1E.
constexpr uint32_t kActrSize = 0x20;

const char* chunkBase(pcr::EnemyChunk c) {
    switch (c) {
    case pcr::EnemyChunk::Actor:
        return "ACTR";
    case pcr::EnemyChunk::ScalableObject:
        return "SCOB";
    case pcr::EnemyChunk::Door:
        return "DOOR";
    default:
        return "TGDR";
    }
}

// The names "Existing actors to modify" / "Actors to remove" find at their path (D9). wwrando's
// comments name them: the pots given sticks, the Zenshut door, the ring of flames (Yswdr00, on
// kenroom's default layer once R2.10 copied it there, as add_chest_in_place_of_master_sword does
// before _save), the miniboss intro event trigger.
struct KnownName {
    const char* path;
    const char* name;
};
constexpr KnownName kKnownNames[] = {
    {"M_NewD2/Room0.arc/Actor003", "ootubo1"},
    {"M_NewD2/Room11.arc/Actor004", "ootubo1"},
    {"TF_01/Stage.arc/ScalableDoor005", "Zenshut"},
    {"kenroom/Room0.arc/Actor007", "Yswdr00"},
    {"SirenMB/Room23.arc/ScalableObject000", "TagEv"},
};
const char* knownName(const char* path) {
    for (const KnownName& k : kKnownNames) {
        if (strcmp(k.path, path) == 0) {
            return k.name;
        }
    }
    return nullptr;
}

constexpr const char* kAlldieFrom = "AND_SW0|AND_SW2|Kbota_A|Kbota_B|KbotaC";
constexpr const char* kHomenNames = "Homen1|Homen2";

// randomize_enemies_spawned_by_objects: (file, entries_by_type(ACTR) index), in draw order.
struct HomenSite {
    const char* stage;
    int room;
    int index;
    const char* label;
};
constexpr HomenSite kHomen[pcr::kNumHomen] = {
    {"kaze", 2, 0x27, "kaze/Room2.arc/Homen027"},
    {"kaze", 10, 0x00, "kaze/Room10.arc/Homen000"},
    {"kaze", 10, 0x09, "kaze/Room10.arc/Homen009"},
};

// A target plus what the edit needs. Every table of this module is static storage: the patcher runs
// while the game's current JKRHeap is an archive or room heap (operator new goes there on the port),
// which a stage change frees.
struct Target {
    PcRandoEnemyActorTarget t;
    const EnemyActorData* data; // modify / add
    int homen;                  // homen slot, or -1
    char label[48];             // t.label of an add
};

constexpr int kMaxTargets = 64;
struct TargetList {
    Target items[kMaxTargets];
    int n = 0;
    const Target* begin() const { return items; }
    const Target* end() const { return items + n; }
    size_t size() const { return (size_t)n; }
    const Target& operator[](size_t i) const { return items[i]; }
};
TargetList sTargets;
bool sTargetsBuilt = false;
PcRandoEnemyActorTarget sPublic[kMaxTargets];

Target* newTarget(int kind) {
    if (sTargets.n >= kMaxTargets) {
        return nullptr; // the table has 26; more would be a generator change
    }
    Target* x = &sTargets.items[sTargets.n++];
    memset(x, 0, sizeof *x);
    x->t.kind = kind;
    x->homen = -1;
    return x;
}

void setFromPath(Target& x, const EnemyActorPath& p) {
    x.t.label = p.raw;
    x.t.stage = p.stage;
    x.t.room = p.room;
    if (!pc_rando_dzx_tag(chunkBase(p.chunk), p.layer, x.t.tag)) {
        memcpy(x.t.tag, "????", 5);
    }
    x.t.index = p.index;
}

const TargetList& targets() {
    if (sTargetsBuilt) {
        return sTargets;
    }
    sTargetsBuilt = true;
    sTargets.n = 0;
    Target* x;
    for (const EnemyActorPath& p : pcr::enemy_alldie_paths()) {
        if ((x = newTarget(PCR_EACTOR_ALLDIE)) != nullptr) {
            setFromPath(*x, p);
            x->t.vanilla_name = kAlldieFrom;
        }
    }
    for (const EnemyActorData& d : pcr::enemy_modify_actors()) {
        if ((x = newTarget(PCR_EACTOR_MODIFY)) != nullptr) {
            setFromPath(*x, d.path);
            x->t.vanilla_name = knownName(d.path.raw);
            x->data = &d;
        }
    }
    for (const pcr::EnemyGroup& g : pcr::enemy_groups()) {
        const pcr::Span<EnemyActorData> adds = pcr::enemy_group_adds(g);
        if (adds.size == 0 || g.num_enemies == 0) {
            continue;
        }
        // dzx, _, layer = get_dzx_fourcc_and_layer_by_path(enemy_group["Enemies"][0]["Path"])
        const EnemyActorPath& first = pcr::enemy_group_locations(g)[0].path;
        for (size_t k = 0; k < adds.size; k++) {
            if ((x = newTarget(PCR_EACTOR_ADD)) == nullptr) {
                continue;
            }
            x->t.stage = first.stage;
            x->t.room = first.room;
            if (!pc_rando_dzx_tag("ACTR", first.layer, x->t.tag)) {
                memcpy(x->t.tag, "????", 5);
            }
            x->t.index = -1;
            x->t.vanilla_name = nullptr;
            x->data = &adds[k];
            snprintf(x->label, sizeof x->label, "%s/Add%zu", first.file, k);
            x->t.label = x->label;
        }
    }
    for (const EnemyActorPath& p : pcr::enemy_remove_paths()) {
        if ((x = newTarget(PCR_EACTOR_REMOVE)) != nullptr) {
            setFromPath(*x, p);
            x->t.vanilla_name = knownName(p.raw);
        }
    }
    for (int h = 0; h < pcr::kNumHomen; h++) {
        if ((x = newTarget(PCR_EACTOR_HOMEN)) == nullptr) {
            continue;
        }
        x->t.label = kHomen[h].label;
        x->t.stage = kHomen[h].stage;
        x->t.room = kHomen[h].room;
        memcpy(x->t.tag, "ACTR", 5);
        x->t.index = kHomen[h].index;
        x->t.vanilla_name = kHomenNames;
        x->homen = h;
    }
    for (int i = 0; i < sTargets.n; i++) {
        sPublic[i] = sTargets.items[i].t;
    }
    return sTargets;
}

bool nameIn(const uint8_t* e, const char* list) {
    char name[9];
    memcpy(name, e, 8);
    name[8] = '\0';
    const size_t n = strlen(name);
    const char* p = list;
    while (p != nullptr && *p != '\0') {
        const char* bar = strchr(p, '|');
        const size_t len = bar != nullptr ? (size_t)(bar - p) : strlen(p);
        if (len == n && memcmp(p, name, n) == 0) {
            return true;
        }
        p = bar != nullptr ? bar + 1 : nullptr;
    }
    return false;
}

struct Name {
    char s[9];
};
Name nameOf(const uint8_t* e) {
    Name n;
    memcpy(n.s, e, 8);
    n.s[8] = '\0';
    return n;
}

// getattr / setattr of a params-bitfield (all the fields used here live in params).
bool getParam(const char* actorName, const char* param, uint32_t params, uint32_t* out) {
    const char* cls = pcr::actor_class_name(actorName);
    const pcr::ActorParamField* f = cls != nullptr ? pcr::actor_param(cls, param) : nullptr;
    if (f == nullptr || f->bitfield != pcr::ActorBitfield::params) {
        return false;
    }
    *out = pcr::actor_param_get(*f, params);
    return true;
}
bool setParam(const char* actorName, const char* param, uint32_t* params, uint32_t value) {
    const char* cls = pcr::actor_class_name(actorName);
    const pcr::ActorParamField* f = cls != nullptr ? pcr::actor_param(cls, param) : nullptr;
    if (f == nullptr || f->bitfield != pcr::ActorBitfield::params) {
        return false;
    }
    *params = pcr::actor_param_set(*f, *params, value);
    return true;
}

// entries_by_type(ACTR)[index]: every chunk whose tag is ACTR or ACT0-ACTb, in chunk-table order.
uint8_t* actrByType(const PcRandoDzx* dzx, int index) {
    int seen = 0;
    for (uint32_t c = 0; c < dzx->chunkCount; c++) {
        const uint8_t* row = dzx->data + 4 + c * 12;
        if (memcmp(row, "ACT", 3) != 0 || !(row[3] == 'R' || (row[3] >= '0' && row[3] <= '9') || row[3] == 'a' ||
                                            row[3] == 'b')) {
            continue;
        }
        char tag[5];
        memcpy(tag, row, 4);
        tag[4] = '\0';
        const int n = (int)pc_rando_dzx_be32(row + 4);
        if (index < seen + n) {
            return pc_rando_dzx_entry(dzx, tag, (uint32_t)(index - seen));
        }
        seen += n;
    }
    return nullptr;
}

bool sameFile(const PcRandoEnemyActorTarget& t, const char* stage, int room) {
    return t.room == room && strcmp(t.stage, stage) == 0;
}

// One in-place edit. Returns true when made; *why on a refusal, *mismatch for D9.
// why: a buffer of kWhy bytes.
constexpr size_t kWhy = 160;
bool applyInPlace(const Target& x, PcRandoDzx* dzx, const int homen[3], char* why, bool* mismatch) {
    *mismatch = false;
    uint8_t* e = x.t.kind == PCR_EACTOR_HOMEN ? actrByType(dzx, x.t.index)
                                              : pc_rando_dzx_entry(dzx, x.t.tag, (uint32_t)x.t.index);
    if (e == nullptr) {
        snprintf(why, kWhy, "no %s entry there", x.t.tag);
        *mismatch = true;
        return false;
    }
    if (x.t.vanilla_name != nullptr && !nameIn(e, x.t.vanilla_name)) {
        snprintf(why, kWhy, "the entry is \"%s\", not %s", nameOf(e).s, x.t.vanilla_name);
        *mismatch = true;
        return false;
    }
    switch (x.t.kind) {
    case PCR_EACTOR_ALLDIE: {
        const Name orig = nameOf(e);
        uint32_t sw = 0;
        if (!getParam(orig.s, "switch_to_set", pc_rando_dzx_be32(e + 0x08), &sw)) {
            snprintf(why, kWhy, "no switch_to_set param for %s", orig.s);
            return false;
        }
        uint32_t params = 0xFFFFFFFFu;
        if (!setParam("ALLdie", "switch_to_set", &params, sw)) {
            snprintf(why, kWhy, "no switch_to_set param for ALLdie");
            return false;
        }
        putName(e, "ALLdie");
        put32(e + 0x08, params);
        put16(e + 0x18, 0);
        put16(e + 0x1C, 0);
        return true;
    }
    case PCR_EACTOR_MODIFY: {
        const EnemyActorData& d = *x.data;
        if (d.flags & pcr::EACT_NAME) {
            putName(e, d.name);
        }
        if (d.flags & pcr::EACT_PARAMS) {
            put32(e + 0x08, d.params);
        }
        if (d.flags & pcr::EACT_X_ROTATION) {
            put16(e + 0x18, d.x_rot);
        }
        if (d.flags & pcr::EACT_Y_ROTATION) {
            put16(e + 0x1A, d.y_rot);
        }
        if (d.flags & pcr::EACT_Z_ROTATION) {
            put16(e + 0x1C, d.z_rot);
        }
        if (d.flags & pcr::EACT_POSITION) {
            putf(e + 0x0C, d.position[0]);
            putf(e + 0x10, d.position[1]);
            putf(e + 0x14, d.position[2]);
        }
        return true;
    }
    case PCR_EACTOR_REMOVE:
        memset(e, 0, 8); // remove_entity: name = "" (pc_rando_dzx_suppress)
        return true;
    case PCR_EACTOR_HOMEN: {
        const Name orig = nameOf(e);
        uint32_t params = pc_rando_dzx_be32(e + 0x08);
        if (!setParam(orig.s, "enemy_to_spawn", &params, (uint32_t)homen[x.homen])) {
            snprintf(why, kWhy, "no enemy_to_spawn param for %s", orig.s);
            return false;
        }
        put32(e + 0x08, params);
        return true;
    }
    default:
        snprintf(why, kWhy, "unknown edit");
        return false;
    }
}

// A new ACTR record (dzx.py ACTR defaults, then the entry's fields).
void buildAdd(const EnemyActorData& d, uint8_t out[kActrSize]) {
    memset(out, 0, kActrSize);
    putName(out, (d.flags & pcr::EACT_NAME) && d.name != nullptr ? d.name : "");
    put32(out + 0x08, (d.flags & pcr::EACT_PARAMS) ? d.params : 0);
    if (d.flags & pcr::EACT_POSITION) {
        putf(out + 0x0C, d.position[0]);
        putf(out + 0x10, d.position[1]);
        putf(out + 0x14, d.position[2]);
    } else {
        putf(out + 0x0C, 0.0);
        putf(out + 0x10, 0.0);
        putf(out + 0x14, 0.0);
    }
    put16(out + 0x18, (d.flags & pcr::EACT_X_ROTATION) ? d.x_rot : 0);
    put16(out + 0x1A, (d.flags & pcr::EACT_Y_ROTATION) ? d.y_rot : 0);
    put16(out + 0x1C, (d.flags & pcr::EACT_Z_ROTATION) ? d.z_rot : 0);
    put16(out + 0x1E, 0xFFFF);
}

int8_t sResults[kMaxTargets];
constexpr size_t sResultsSize = kMaxTargets;
PcRandoEnemyActorStats sStats;
bool sInit = false;
bool sBypass = false;
const PcRandoSeed* sSeed = nullptr;
uint8_t sSeedHash[16];
bool sSeedHas = false;
int sHomen[pcr::kNumHomen] = {0, 0, 0};

void resetResults() {
    targets();
    memset(sResults, -1, sizeof sResults);
    sStats = PcRandoEnemyActorStats{};
}

// The seed's homen values; false when enemy rando is off for this seed (or its key is unusable:
// R7.4a then leaves every enemy vanilla, so this step does nothing either).
bool seedActive() {
    const PcRandoSeed* seed = pc_rando() ? pc_rando_seed() : nullptr;
    if (seed == nullptr || !pc_rando_enemies_active()) {
        if (seed == nullptr) {
            sSeed = nullptr;
            sSeedHas = false;
        }
        return false;
    }
    if (seed != sSeed || memcmp(seed->hash, sSeedHash, sizeof sSeedHash) != 0) {
        sSeed = seed;
        memcpy(sSeedHash, seed->hash, sizeof sSeedHash);
        sSeedHas = false;
        resetResults();
        // pc_rando_enemies_active() checked the whole key (enemiesFromJson: 3 candidates); read the
        // values in place (no allocation, see Target)
        const pcr::Json* j = seed->extra.find("enemies");
        const pcr::Json* h = j != nullptr ? j->find("homen") : nullptr;
        if (h != nullptr && h->isArray() && h->size() == (size_t)pcr::kNumHomen) {
            for (int i = 0; i < pcr::kNumHomen; i++) {
                sHomen[i] = (int)h->at((size_t)i).asInt();
            }
            sSeedHas = true;
        }
    }
    return sSeedHas;
}

// The port grows a file into a copy next to the archive's own bytes (pc_rando_res_grow_dzx), where
// wwrando grows the file inside the archive by the new entries only. In a stage with room memory
// blocks (stage.dzs MEMA sizes, MECO room -> block; d_stage.cpp createMemoryBlock), the copy comes out
// of the room's block, and daBg_c takes what is left of it: M_NewD2 Room1's bg then runs out (cBgW::Set
// in d_a_bg.cpp createHeap). So the block of each room that gets new actors gets this much more
// (GameCube bytes, doubled on the port like the rest of the block); a port-only change, not in wwrando.
constexpr uint32_t kGrowBlockSlack = 0x1000;

// A stage.dzs with room memory blocks: adds the slack to the block of every room of the stage that
// gets new actors. Returns the number of blocks grown.
int addBlockSlack(PcRandoResFile* file, const TargetList& ts) {
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK) {
        return 0;
    }
    const int mema = pc_rando_dzx_find(&dzx, "MEMA"), meco = pc_rando_dzx_find(&dzx, "MECO");
    if (mema < 0 || meco < 0) {
        return 0;
    }
    const uint8_t* memaRow = dzx.data + 4 + mema * 12;
    const uint8_t* mecoRow = dzx.data + 4 + meco * 12;
    const uint32_t nBlocks = pc_rando_dzx_be32(memaRow + 4), memaOff = pc_rando_dzx_be32(memaRow + 8);
    const uint32_t nRooms = pc_rando_dzx_be32(mecoRow + 4), mecoOff = pc_rando_dzx_be32(mecoRow + 8);
    if ((uint64_t)memaOff + 4ull * nBlocks > file->size || (uint64_t)mecoOff + 2ull * nRooms > file->size) {
        return 0;
    }
    bool grown[64] = {};
    int n = 0;
    for (const Target& x : ts) {
        if (x.t.kind != PCR_EACTOR_ADD || x.t.room < 0 || strcmp(x.t.stage, file->stage) != 0) {
            continue;
        }
        for (uint32_t r = 0; r < nRooms; r++) {
            const uint8_t* m = file->data + mecoOff + 2 * r;
            if (m[0] != x.t.room || m[1] >= nBlocks || m[1] >= 64 || grown[m[1]]) {
                continue;
            }
            grown[m[1]] = true;
            uint8_t* size = file->data + memaOff + 4 * m[1];
            put32(size, pc_rando_dzx_be32(size) + kGrowBlockSlack);
            logf("[cos] rando: enemy actors: %s: memory block %u of room %d + 0x%X for its new actors\n",
                 file->stage, (unsigned)m[1], x.t.room, (unsigned)kGrowBlockSlack);
            n++;
        }
    }
    return n;
}

bool stageGetsAdds(const char* stage) {
    for (const Target& x : targets()) {
        if (x.t.kind == PCR_EACTOR_ADD && x.t.room >= 0 && strcmp(x.t.stage, stage) == 0) {
            return true;
        }
    }
    return false;
}

bool fileMatches(const PcRandoResFile* file) {
    if (file == nullptr || file->stage == nullptr || file->name == nullptr) {
        return false;
    }
    return file->room >= 0 ? strcmp(file->name, "room.dzr") == 0 : strcmp(file->name, "stage.dzs") == 0;
}

} // namespace

extern "C" {

const PcRandoEnemyActorTarget* pc_rando_enemy_actors_targets(int* count) {
    targets();
    if (count != nullptr) {
        *count = sTargets.n;
    }
    return sPublic;
}

void pc_rando_enemy_actors_stats(PcRandoEnemyActorStats* out) {
    if (out != nullptr) {
        *out = sStats;
    }
}

int pc_rando_enemy_actors_last_result(int i) {
    if (i < 0 || (size_t)i >= sResultsSize) {
        return -1;
    }
    return sResults[i];
}

int pc_rando_enemy_actors_selfcheck(char* why, size_t whySize) {
    int conflicts = 0;
    auto note = [&](const char* a, const char* b) {
        if (conflicts++ == 0 && why != nullptr && whySize > 0) {
            snprintf(why, whySize, "%s names the removed entry %s", a, b);
        }
    };
    auto same = [](const EnemyActorPath& r, const char* stage, int room, const char* tag, int index) {
        char rt[5];
        return pc_rando_dzx_tag(chunkBase(r.chunk), r.layer, rt) && r.room == room && strcmp(r.stage, stage) == 0 &&
               strcmp(rt, tag) == 0 && (int)r.index == index;
    };
    for (const EnemyActorPath& r : pcr::enemy_remove_paths()) {
        for (const pcr::EnemyLocation& l : pcr::enemy_locations()) {
            char tag[5];
            if (pc_rando_dzx_tag("ACTR", l.path.layer, tag) && same(r, l.path.stage, l.path.room, tag, l.path.index)) {
                note(l.path.raw, r.raw);
            }
        }
        for (const Target& x : targets()) {
            if (x.t.kind == PCR_EACTOR_REMOVE || x.t.kind == PCR_EACTOR_ADD) {
                continue;
            }
            // a stone head's index is over every ACTR-type chunk: the same entry as ACTR[i] only when
            // the file has no ACTR layer chunk before it, which the removed entries (SCOB, kenroom)
            // never share with kaze
            if (same(r, x.t.stage, x.t.room, x.t.tag, x.t.index)) {
                note(x.t.label, r.raw);
            }
        }
    }
    return conflicts;
}

void pc_rando_enemy_actors_set_bypass(int on) {
    sBypass = on != 0;
}

int pc_rando_enemy_actors_apply(const int homen[3], PcRandoResFile* file, int8_t* results, int* handled) {
    if (handled != nullptr) {
        *handled = 0;
    }
    if (!fileMatches(file)) {
        return 0;
    }
    const TargetList& ts = targets();
    if (file->room < 0) {
        addBlockSlack(file, ts);
    }
    int h = 0, n = 0;
    uint8_t adds[kMaxTargets * kActrSize];
    size_t addIdx[kMaxTargets];
    size_t nAdd = 0;
    char addTag[5] = {};
    PcRandoDzx dzx;
    bool opened = false, openFailed = false;
    for (size_t i = 0; i < ts.size(); i++) {
        const Target& x = ts[i];
        if (!sameFile(x.t, file->stage, file->room)) {
            continue;
        }
        h++;
        if (!opened && !openFailed) {
            openFailed = pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK;
            opened = !openFailed;
        }
        if (openFailed) {
            logf("[cos] rando: enemy actors: %s: the file does not parse, left as it is\n", x.t.label);
            if (results != nullptr) {
                results[i] = 0;
            }
            continue;
        }
        if (x.t.kind == PCR_EACTOR_ADD) {
            buildAdd(*x.data, adds + nAdd * kActrSize);
            addIdx[nAdd++] = i;
            memcpy(addTag, x.t.tag, 5); // one group per file: one tag
            continue;
        }
        char why[kWhy] = {};
        bool mismatch = false;
        const bool ok = applyInPlace(x, &dzx, homen, why, &mismatch);
        if (results != nullptr) {
            results[i] = ok ? 1 : mismatch ? 0 : 2;
        }
        if (ok) {
            n++;
        } else {
            logf("[cos] rando: enemy actors: %s: %s, left as it is (%s)\n", x.t.label,
                 mismatch ? "mismatch" : "refused", why);
        }
    }
    if (nAdd != 0) {
        PcRandoDzxAdd add = {addTag, adds, (uint32_t)nAdd};
        const int r = pc_rando_res_grow_dzx(file, &add, 1, nullptr);
        for (size_t k = 0; k < nAdd; k++) {
            if (results != nullptr) {
                results[addIdx[k]] = r == PCR_DZX_OK ? 1 : 2;
            }
        }
        if (r == PCR_DZX_OK) {
            n += (int)nAdd;
        } else {
            logf("[cos] rando: enemy actors: %s/Room%d: %zu new actor(s) not added (%s)\n", file->stage, file->room,
                 nAdd, pc_rando_dzx_result_name(r));
        }
    }
    if (handled != nullptr) {
        *handled = h;
    }
    return n;
}

void pc_rando_enemy_actors_patch(PcRandoResFile* file) {
    if (sBypass || !fileMatches(file) || file->stage[0] == '\0') {
        return;
    }
    bool any = file->room < 0 && stageGetsAdds(file->stage);
    for (const Target& x : targets()) {
        any = any || sameFile(x.t, file->stage, file->room);
    }
    if (!any) {
        return;
    }
    if (!seedActive()) {
        sStats.skipped++;
        return;
    }
    int handled = 0;
    const int n = pc_rando_enemy_actors_apply(sHomen, file, sResults, &handled);
    if (handled == 0) {
        return;
    }
    sStats.files++;
    sStats.done += (uint32_t)n;
    const TargetList& ts = targets();
    for (size_t i = 0; i < ts.size(); i++) {
        if (sameFile(ts[i].t, file->stage, file->room)) {
            sStats.mismatched += sResults[i] == 0 ? 1u : 0u;
            sStats.failed += sResults[i] == 2 ? 1u : 0u;
        }
    }
    logf("[cos] rando: enemy actors: %s %s: %d of %d edit(s) made\n", file->stage,
         file->room >= 0 ? "room.dzr" : "stage.dzs", n, handled);
}

void pc_rando_enemy_actors_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    resetResults();
    char why[160] = {};
    if (pc_rando_enemy_actors_selfcheck(why, sizeof why) != 0) {
        logf("[cos] rando: enemy actors: table check failed: %s\n", why);
    }
    pc_rando_register_dzx_patcher(pc_rando_enemy_actors_patch);
}

} // extern "C"
