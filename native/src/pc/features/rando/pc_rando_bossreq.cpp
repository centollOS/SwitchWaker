// Required bosses at runtime (pc_rando_bossreq.h, docs/RANDOMIZER_PLAN.md step R6.9): wwrando
// 1.10.0_9775811 randomizers/boss_reqs.py RequiredBossesRandomizer._save. Pure: no game headers, no
// game calls.
#include "pc_rando_bossreq.h"

#include "pc/pc_rando.h"
#include "pc_rando_data.h"
#include "pc_rando_dzx.h"
#include "pc_rando_seedfile.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
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

constexpr uint32_t kActrSize = 0x20, kScobSize = 0x24;

const PcRandoSeed* sTestSeed = nullptr;
bool sUseTestSeed = false;
bool sInit = false;
PcRandoBossreqStatus sStatus = {};

const PcRandoSeed* activeSeed() {
    return sUseTestSeed ? sTestSeed : pc_rando_seed();
}

// --- big-endian helpers ---

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
    char n[9] = {};
    memcpy(n, e, 8);
    return strcmp(n, name) == 0;
}

// wwlib's ACTR()/SCOB() defaults (params 0, rotations 0, enemy_number 0xFFFF; SCOB scale 10 and
// padding 0xFF), then the name, params and position.
void makeEntry(uint8_t* out, uint32_t size, const char* name, uint32_t params, float x, float y, float z) {
    memset(out, 0, size);
    strncpy((char*)out, name, 8);
    put32(out + 0x08, params);
    putf(out + 0x0C, x);
    putf(out + 0x10, y);
    putf(out + 0x14, z);
    put16(out + 0x1E, 0xFFFF);
    if (size == kScobSize) {
        out[0x20] = out[0x21] = out[0x22] = 10;
        out[0x23] = 0xFF;
    }
}

int firstNamed(const PcRandoDzx* dzx, const char* tag, const char* name) {
    const uint32_t n = pc_rando_dzx_count(dzx, tag);
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
        if (e != nullptr && nameIs(e, name)) {
            return (int)i;
        }
    }
    return -1;
}

// d_a_alldie switch_to_set.
constexpr uint32_t kAlldieSwitch = 0x0000FF00;

// --- entrances (randomizers/entrances.py) ---

// ZoneEntrance.island_name, or nested_in's ZoneExit.unique_name, for the 44 entrances.
struct EntranceWhere {
    const char* entrance;
    const char* island;   // an island entrance
    const char* nestedIn; // a nested entrance: the exit it is inside
};
const EntranceWhere kEntrances[] = {
    {"Dungeon Entrance on Dragon Roost Island", "Dragon Roost Island", nullptr},
    {"Dungeon Entrance in Forest Haven Sector", "Forest Haven", nullptr},
    {"Dungeon Entrance in Tower of the Gods Sector", "Tower of the Gods Sector", nullptr},
    {"Dungeon Entrance on Headstone Island", "Headstone Island", nullptr},
    {"Dungeon Entrance on Gale Isle", "Gale Isle", nullptr},
    {"Miniboss Entrance in Forbidden Woods", nullptr, "Forbidden Woods"},
    {"Miniboss Entrance in Tower of the Gods", nullptr, "Tower of the Gods"},
    {"Miniboss Entrance in Earth Temple", nullptr, "Earth Temple"},
    {"Miniboss Entrance in Wind Temple", nullptr, "Wind Temple"},
    {"Miniboss Entrance in Hyrule Castle", "Tower of the Gods Sector", nullptr},
    {"Boss Entrance in Dragon Roost Cavern", nullptr, "Dragon Roost Cavern"},
    {"Boss Entrance in Forbidden Woods", nullptr, "Forbidden Woods"},
    {"Boss Entrance in Tower of the Gods", nullptr, "Tower of the Gods"},
    {"Boss Entrance in Forsaken Fortress", "Forsaken Fortress Sector", nullptr},
    {"Boss Entrance in Earth Temple", nullptr, "Earth Temple"},
    {"Boss Entrance in Wind Temple", nullptr, "Wind Temple"},
    {"Secret Cave Entrance on Outset Island", "Outset Island", nullptr},
    {"Secret Cave Entrance on Dragon Roost Island", "Dragon Roost Island", nullptr},
    {"Secret Cave Entrance on Fire Mountain", "Fire Mountain", nullptr},
    {"Secret Cave Entrance on Ice Ring Isle", "Ice Ring Isle", nullptr},
    {"Secret Cave Entrance on Private Oasis", "Private Oasis", nullptr},
    {"Secret Cave Entrance on Needle Rock Isle", "Needle Rock Isle", nullptr},
    {"Secret Cave Entrance on Angular Isles", "Angular Isles", nullptr},
    {"Secret Cave Entrance on Boating Course", "Boating Course", nullptr},
    {"Secret Cave Entrance on Stone Watcher Island", "Stone Watcher Island", nullptr},
    {"Secret Cave Entrance on Overlook Island", "Overlook Island", nullptr},
    {"Secret Cave Entrance on Bird's Peak Rock", "Bird's Peak Rock", nullptr},
    {"Secret Cave Entrance on Pawprint Isle", "Pawprint Isle", nullptr},
    {"Secret Cave Entrance on Pawprint Isle Side Isle", "Pawprint Isle", nullptr},
    {"Secret Cave Entrance on Diamond Steppe Island", "Diamond Steppe Island", nullptr},
    {"Secret Cave Entrance on Bomb Island", "Bomb Island", nullptr},
    {"Secret Cave Entrance on Rock Spire Isle", "Rock Spire Isle", nullptr},
    {"Secret Cave Entrance on Shark Island", "Shark Island", nullptr},
    {"Secret Cave Entrance on Cliff Plateau Isles", "Cliff Plateau Isles", nullptr},
    {"Secret Cave Entrance on Horseshoe Island", "Horseshoe Island", nullptr},
    {"Secret Cave Entrance on Star Island", "Star Island", nullptr},
    {"Inner Entrance in Ice Ring Isle Secret Cave", nullptr, "Ice Ring Isle Secret Cave"},
    {"Inner Entrance in Cliff Plateau Isles Secret Cave", nullptr, "Cliff Plateau Isles Secret Cave"},
    {"Fairy Fountain Entrance on Outset Island", "Outset Island", nullptr},
    {"Fairy Fountain Entrance on Thorned Fairy Island", "Thorned Fairy Island", nullptr},
    {"Fairy Fountain Entrance on Eastern Fairy Island", "Eastern Fairy Island", nullptr},
    {"Fairy Fountain Entrance on Western Fairy Island", "Western Fairy Island", nullptr},
    {"Fairy Fountain Entrance on Southern Fairy Island", "Southern Fairy Island", nullptr},
    {"Fairy Fountain Entrance on Northern Fairy Island", "Northern Fairy Island", nullptr},
};
static_assert(sizeof(kEntrances) / sizeof(kEntrances[0]) == 44, "entrances.py has 44 entrances");

const EntranceWhere* entranceInfo(const std::string& name) {
    for (const EntranceWhere& e : kEntrances) {
        if (name == e.entrance) {
            return &e;
        }
    }
    return nullptr;
}

// done_exits_to_entrances[exit]: the entrance the seed connects to `exit`.
const std::string* entranceFor(const PcRandoSeed& seed, const std::string& exit) {
    for (const auto& [entrance, zoneExit] : seed.entrances) {
        if (zoneExit == exit) {
            return &entrance;
        }
    }
    return nullptr;
}

// get_entrance_zone_for_boss: get_outermost_entrance_for_exit(ZoneExit.all["<boss> Boss Arena"])
// .island_name. NULL where wwrando would raise (unknown names) or loop.
const char* bossIsland(const PcRandoSeed& seed, const char* boss) {
    std::string exit = std::string(boss) + " Boss Arena";
    for (int depth = 0; depth < 44; depth++) {
        const std::string* entrance = entranceFor(seed, exit);
        const EntranceWhere* info = entrance != nullptr ? entranceInfo(*entrance) : nullptr;
        if (info == nullptr) {
            return nullptr;
        }
        if (info->island != nullptr) {
            return info->island;
        }
        exit = info->nestedIn;
    }
    return nullptr;
}

int islandNumber(const char* name) {
    for (const pcr::Island& is : pcr::islands()) {
        if (is.number >= 1 && strcmp(is.name, name) == 0) {
            return is.number;
        }
    }
    return -1;
}

// The boss named by a boss item location "<dungeon> - <boss> Heart Container".
const pcr::Boss* bossOfLocation(const std::string& loc) {
    const size_t dash = loc.find(" - ");
    if (dash == std::string::npos) {
        return nullptr;
    }
    const std::string rest = loc.substr(dash + 3);
    for (const pcr::Boss& b : pcr::bosses()) {
        if (rest == std::string(b.name) + " Heart Container") {
            return &b;
        }
    }
    return nullptr;
}

const char kMessage[] = "You sense that this door will not open until the gigantic monsters lurking throughout the "
                        "world have been defeated...";

} // namespace

extern "C" {

int pc_rando_bossreq_bosses_of(const PcRandoSeed* seed, const char* names[PCR_BR_MAX_BOSSES]) {
    if (seed == nullptr || seed->required_boss_locations.empty()) {
        return 0;
    }
    bool want[PCR_BR_MAX_BOSSES] = {};
    const pcr::Span<pcr::Boss> all = pcr::bosses();
    for (const std::string& loc : seed->required_boss_locations) {
        const pcr::Boss* b = bossOfLocation(loc);
        if (b == nullptr || (size_t)(b - all.data) >= PCR_BR_MAX_BOSSES) {
            logf("[cos] rando: required bosses: \"%s\" is not a boss Heart Container; no requirement\n", loc.c_str());
            return 0;
        }
        want[b - all.data] = true;
    }
    int n = 0;
    for (size_t i = 0; i < all.size && i < PCR_BR_MAX_BOSSES; i++) {
        if (want[i]) {
            if (names != nullptr) {
                names[n] = all[i].name;
            }
            n++;
        }
    }
    return n;
}

uint32_t pc_rando_bossreq_stage_mask(const PcRandoSeed* seed) {
    const char* names[PCR_BR_MAX_BOSSES];
    const int n = pc_rando_bossreq_bosses_of(seed, names);
    uint32_t mask = 0;
    for (int i = 0; i < n; i++) {
        for (const pcr::Boss& b : pcr::bosses()) {
            if (strcmp(b.name, names[i]) == 0 && b.stage_id <= 0x0F) {
                mask |= 1u << b.stage_id;
            }
        }
    }
    return mask;
}

const char* pc_rando_bossreq_boss_island(const PcRandoSeed* seed, const char* boss) {
    return seed != nullptr && boss != nullptr ? bossIsland(*seed, boss) : nullptr;
}

int pc_rando_bossreq_markers(const PcRandoSeed* seed, PcRandoBossMarker out[PCR_BR_MAX_BOSSES]) {
    const char* names[PCR_BR_MAX_BOSSES];
    const int n = pc_rando_bossreq_bosses_of(seed, names);
    // overlap_count_to_extra_rotation
    static const uint8_t kExtra[6] = {0, 45, 22, 68, 33, 56};
    uint8_t overlaps[50] = {};
    int count = 0;
    for (int i = 0; i < n; i++) {
        const pcr::Boss* boss = nullptr;
        for (const pcr::Boss& b : pcr::bosses()) {
            if (strcmp(b.name, names[i]) == 0) {
                boss = &b;
            }
        }
        const char* island = bossIsland(*seed, names[i]);
        const int number = island != nullptr ? islandNumber(island) : -1;
        if (boss == nullptr || number < 1 || number > 49) {
            logf("[cos] rando: required bosses: no island for %s's arena in the entrance map; its marker stays hidden\n",
                 names[i]);
            continue;
        }
        PcRandoBossMarker& m = out[count++];
        m.index = boss->quest_marker_index;
        m.boss = boss->name;
        m.island = (uint8_t)number;
        const int sx = (number - 1) % 7, sy = (number - 1) / 7;
        m.x = (int16_t)(sx * 0x37 - 0xFA);
        m.y = (int16_t)(sy * 0x38 - 0xBC);
        const uint8_t k = overlaps[number];
        m.rotation = (uint8_t)(55 + kExtra[k < 6 ? k : 5]);
        overlaps[number]++;
    }
    return count;
}

uint32_t pc_rando_bossreq_marker_tag(int index) {
    static const uint32_t kTags[PCR_BR_MARKERS] = {
        0x434B3332u, 0x434B3331u, 0x434B3233u, 0x434B3232u, // CK32 CK31 CK23 CK22
        0x434B3231u, 0x434B3133u, 0x434B3132u, 0x434B3131u, // CK21 CK13 CK12 CK11
    };
    return index >= 0 && index < PCR_BR_MARKERS ? kTags[index] : 0;
}

void pc_rando_bossreq_dngsw(uint32_t stageMask, uint8_t out[0x20]) {
    // d_a_dungeon_flag_sw: switch_to_set 0xFF, stage_no_bitmask 0x00FFFF00, flag_to_check 0x07000000.
    makeEntry(out, kActrSize, "DngSw",
              ((uint32_t)PCR_BR_FLAG_BOSS_DEAD << 24) | ((stageMask & 0xFFFF) << 8) | PCR_BR_SW_BOSSES_DEAD, 1800.0f,
              2000.0f, -19000.0f);
}

void pc_rando_bossreq_swop(int nand, uint8_t out[0x20]) {
    // d_a_switch_op: operation 0xF (0 AND, 1 NAND), is_continuous 0x10 (0), switch_to_set 0xFF00,
    // first_switch_to_check 0xFF0000, num_switches_to_check 0xFF000000; evnt_index x_rot 0xFF.
    const uint32_t params = nand ? (1u << 24) | (PCR_BR_SW_BOSSES_DEAD << 16) | (PCR_BR_SW_BOSSES_ALIVE << 8) | 1u
                                 : (2u << 24) | (PCR_BR_SW_BOSSES_DEAD << 16) | (PCR_BR_SW_DOOR << 8) | 0u;
    makeEntry(out, kActrSize, "SwOp", params, 1800.0f, 2000.0f, nand ? -19400.0f : -19200.0f);
    put16(out + 0x18, 0xFF);
}

void pc_rando_bossreq_tagmsg(const uint8_t door[0x20], uint8_t out[0x24]) {
    // d_a_tag_msg: type 0x40 (0), switch_to_set 0xFF00, enable_spawn_switch 0xFF0000, evnt_index
    // 0xFF000000; message_id x_rot; enable_spawn_event_bit z_rot.
    const float x = pc_rando_dzx_bef32(door + 0x0C), y = pc_rando_dzx_bef32(door + 0x10),
                z = pc_rando_dzx_bef32(door + 0x14);
    makeEntry(out, kScobSize, "TagMsg", (0xFFu << 24) | (PCR_BR_SW_BOSSES_ALIVE << 16) | (0xFFu << 8), x, y + 200.0f,
              z);
    put16(out + 0x18, PCR_BR_MSG);
    put16(out + 0x1C, 0xFFFF);
    out[0x20] = 80;
    out[0x21] = 255;
}

void pc_rando_bossreq_akabe(uint8_t out[0x24]) {
    // d_a_obj_akabe: disable_spawn_switch 0xFF, scale_mode 0x300 (3: NBOX10), always_on 0x1000 (0),
    // collision_archive 0xF0000 (3: NBOX).
    makeEntry(out, kScobSize, "NBOX10", (3u << 16) | (3u << 8) | PCR_BR_SW_BOSSES_DEAD, -0.7944336f, 0.0f,
              -37797.72f);
    out[0x20] = 9;
    out[0x21] = 255;
    out[0x22] = 7;
}

void pc_rando_bossreq_patch_dzx(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->type != PCR_RES_DZR || file->stage == nullptr ||
        file->arc == nullptr || strcmp(file->stage, "GanonL") != 0 || strcmp(file->arc, "Room0") != 0) {
        return;
    }
    const uint32_t mask = pc_rando_bossreq_stage_mask(activeSeed());
    if (mask == 0) {
        return; // no required bosses: wwrando leaves the room alone
    }
    sStatus.seen++;
    sStatus.mask = mask;
    PcRandoDzx dzx;
    int r = pc_rando_dzx_open(&dzx, file->data, file->size);
    int alldie = -1, door = -1;
    if (r == PCR_DZX_OK) {
        alldie = firstNamed(&dzx, "ACTR", "ALLdie");
        door = firstNamed(&dzx, "TGOB", "KGBdor");
        if (alldie < 0 || door < 0 || firstNamed(&dzx, "ACTR", "DngSw") >= 0 ||
            ((pc_rando_dzx_be32(pc_rando_dzx_entry(&dzx, "ACTR", (uint32_t)alldie) + 8) & kAlldieSwitch) >> 8) !=
                PCR_BR_SW_DOOR) {
            r = PCR_DZX_MISMATCH;
        }
    }
    uint8_t actr[3][kActrSize];
    uint8_t scob[2][kScobSize];
    if (r == PCR_DZX_OK) {
        // add_entity order: DngSw, SwOp (AND), TagMsg, SwOp (NAND), NBOX10.
        pc_rando_bossreq_dngsw(mask, actr[0]);
        pc_rando_bossreq_swop(0, actr[1]);
        pc_rando_bossreq_tagmsg(pc_rando_dzx_entry(&dzx, "TGOB", (uint32_t)door), scob[0]);
        pc_rando_bossreq_swop(1, actr[2]);
        pc_rando_bossreq_akabe(scob[1]);
        const PcRandoDzxAdd adds[2] = {{"ACTR", actr, 3}, {"SCOB", scob, 2}};
        r = pc_rando_res_grow_dzx(file, adds, 2, nullptr);
    }
    if (r == PCR_DZX_OK) {
        r = pc_rando_dzx_open(&dzx, file->data, file->size);
    }
    if (r == PCR_DZX_OK) {
        uint8_t* e = pc_rando_dzx_entry(&dzx, "ACTR", (uint32_t)alldie);
        put32(e + 8, (pc_rando_dzx_be32(e + 8) & ~kAlldieSwitch) | ((uint32_t)PCR_BR_SW_ENEMIES_DEAD << 8));
        sStatus.applied++;
        logf("[cos] rando: required bosses: GanonL/Room0 door needs stages 0x%04X (DngSw, 2 SwOps, TagMsg %d, NBOX10)\n",
             (unsigned)mask, PCR_BR_MSG);
    } else {
        logf("[cos] rando: required bosses: GanonL/Room0 room.dzr: %s; left vanilla\n", pc_rando_dzx_result_name(r));
    }
    sStatus.result = r;
}

void pc_rando_bossreq_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_dzx_patcher(pc_rando_bossreq_patch_dzx);
}

void pc_rando_bossreq_set_seed_for_test(const PcRandoSeed* seed) {
    sTestSeed = seed;
    sUseTestSeed = seed != nullptr;
}

void pc_rando_bossreq_status(PcRandoBossreqStatus* out) {
    if (out != nullptr) {
        *out = sStatus;
    }
}

void pc_rando_bossreq_reset(void) {
    sStatus = PcRandoBossreqStatus();
}

const char* pc_rando_bossreq_message_text(void) {
    return kMessage;
}

} // extern "C"

void pc_rando_bossreq_message(const pcr::GlyphWidths& widths, PcRandoMsgEntry* entry, std::string* text) {
    // bmg.add_new_message(851): text_box_type SPECIAL, initial_draw_type 0, text_alignment 4, then
    // msg.word_wrap_string(self.rando.bfn).
    pc_rando_msg_entry_defaults(entry);
    entry->text_box_type = PCR_BR_MSG_BOX;
    entry->initial_draw_type = 0;
    entry->text_alignment = PCR_BR_MSG_ALIGN;
    *text = pcr::wordWrapMessage(widths, PCR_BR_MSG_BOX, kMessage);
}
