// Trap chests and chest type matches contents (pc_rando_trap.h, docs/RANDOMIZER_PLAN.md step R6.5).
// wwrando reference: randomizers/items.py change_chest_item and get_ctmc_chest_type_for_item,
// tweaks.py add_trap_chest_event_to_stage. Pure: no game code (cos_rando_core).
#include "pc_rando_trap.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_data.h"
#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_lockeys.h"
#include "pc_rando_logic.h"
#include "pc_rando_place_dzx.h"
#include "pc_rando_seedfile.h"
#include "pc_rando_tweak_added_chests.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

using pcr::DzxField;
using pcr::KeyKind;
using pcr::LocKey;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool isChestKey(const LocKey& k) {
    return (k.kind == KeyKind::Dzx || k.kind == KeyKind::Added) && k.field == DzxField::TboxItem;
}

// The file a key's archive holds its chunks in: Stage.arc -> stage.dzs, Room<N>.arc -> room.dzr.
bool keyFileMatches(const LocKey& key, const PcRandoResFile* file) {
    if (key.stage == nullptr || key.arc == nullptr || strcmp(key.stage, file->stage) != 0 ||
        strcmp(key.arc, file->arc) != 0) {
        return false;
    }
    if (strcmp(key.arc, "Stage") == 0) {
        return file->type == PCR_RES_DZS && strcmp(file->name, "stage.dzs") == 0;
    }
    return file->type == PCR_RES_DZR && strcmp(file->name, "room.dzr") == 0;
}

uint32_t be32(const uint8_t* p) {
    return pc_rando_dzx_be32(p);
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// --- the cached plan of the active seed ---

pcr::ChestPlan sPlan;
const PcRandoSeed* sPlanSeed = nullptr;
uint8_t sPlanHash[16];
bool sPlanBuilt = false;

constexpr int kMaxKeys = 1024;
PcRandoTrapKeyStatus sKeyStatus[kMaxKeys];
PcRandoTrapEventStats sEventStats;

// The placement held for chest key `i` in the file being patched: R2.1's status for a Dzx key (the
// D9 fingerprint), R2.10's site for an Added chest.
bool placementHeld(size_t i, const LocKey& k, const PcRandoResFile* file) {
    if (k.kind == KeyKind::Dzx) {
        const PcRandoPlaceStatus* st = pc_rando_place_dzx_status((int)i);
        return st != nullptr && (st->result == PCR_PLACE_WRITTEN || st->result == PCR_PLACE_SAME);
    }
    for (int s = 0; s < PCR_AC_SITE_COUNT; s++) {
        const PcRandoAddedSiteInfo* site = pc_rando_tweak_added_chests_site(s);
        if (site != nullptr && site->added == (int)k.added && !site->is_event && strcmp(site->stage, file->stage) == 0 &&
            strcmp(site->arc, file->arc) == 0) {
            return site->runs > 0 && site->result == 0;
        }
    }
    return false;
}

bool entryNameOk(const LocKey& k, const uint8_t* e) {
    if (k.kind == KeyKind::Dzx) {
        return memcmp(e, k.fp_name, 8) == 0;
    }
    return memcmp(e, "takara3\0", 8) == 0; // pc_rando_added_chest_entry
}

} // namespace

namespace pcr {

bool isTrapChestItem(const std::string& item) {
    return endsWith(item, " Trap Chest");
}

int ctmcChestType(const std::string& item, const std::vector<std::string>& progress, bool requiredBosses,
                  const std::vector<std::string>& requiredDungeons) {
    if (std::find(progress.begin(), progress.end(), item) == progress.end()) {
        return 0; // Light wood chests for non-progress items and consumables
    }
    if (!endsWith(item, " Key")) {
        return 2; // Metal chests for progress items
    }
    if (!requiredBosses) {
        return 1; // Dark wood chest for Small and Big Keys
    }
    // item_name.split()[0]: the dungeon's short name.
    const size_t sp = item.find(' ');
    const std::string shortName = item.substr(0, sp);
    const char* dungeon = nullptr;
    for (const DungeonName& d : dungeon_names()) { // logic.py DUNGEON_NAMES
        if (shortName == d.short_name) {
            dungeon = d.name;
        }
    }
    if (dungeon == nullptr) {
        return 0; // Python raises KeyError; no progress key has another prefix
    }
    return std::find(requiredDungeons.begin(), requiredDungeons.end(), dungeon) != requiredDungeons.end() ? 1 : 0;
}

std::vector<std::string> requiredDungeonsOf(const std::vector<std::string>& requiredBossLocations) {
    std::vector<std::string> out;
    for (const std::string& loc : requiredBossLocations) {
        const size_t p = loc.find(" - ");
        out.push_back(p == std::string::npos ? loc : loc.substr(0, p));
    }
    return out;
}

bool optionsFromJson(const Json& options, Options* out, std::string* err) {
    Options o;
    if (!options.isObject()) {
        if (err != nullptr) {
            *err = "the options are not an object";
        }
        return false;
    }
    for (int id = 0; id < OPT_COUNT; id++) {
        const OptionInfo& info = kOptionInfo[id];
        const Json* v = options.find(info.name);
        if (v == nullptr) {
            continue;
        }
        std::string text;
        bool good = true;
        switch (info.kind) {
        case OptKind::Bool:
            good = v->isBool();
            text = v->asBool() ? "1" : "0";
            break;
        case OptKind::Int:
            good = v->isInteger();
            text = std::to_string((long long)v->asInt());
            break;
        case OptKind::Enum:
        case OptKind::Str:
            good = v->isString();
            if (good) {
                text = v->text();
            }
            break;
        case OptKind::List:
            good = v->isArray();
            for (size_t i = 0; good && i < v->size(); i++) {
                good = v->at(i).isString();
                if (good) {
                    text += (i != 0 ? "|" : "") + v->at(i).text();
                }
            }
            break;
        case OptKind::Dict:
            continue; // custom_colors: cosmetic, not read by the logic
        }
        if (!good || !optionFromString(o, id, text.c_str())) {
            if (err != nullptr) {
                *err = std::string("bad value for option ") + info.name;
            }
            return false;
        }
    }
    *out = o;
    return true;
}

bool allProgressItemNames(const Options& opts, std::vector<std::string>* out, std::string* err) {
    Logic logic;
    const LogicState state = defaultLogicState(opts);
    if (!state.error.empty()) {
        if (err != nullptr) {
            *err = state.error;
        }
        return false;
    }
    if (!logic.create(opts, state)) {
        if (err != nullptr) {
            *err = logic.error().empty() ? std::string("the logic failed") : logic.error();
        }
        return false;
    }
    out->clear();
    for (int id : logic.allProgressItems) {
        const std::string& n = logic.name(id);
        if (std::find(out->begin(), out->end(), n) == out->end()) {
            out->push_back(n);
        }
    }
    return true;
}

bool buildChestPlan(const PcRandoSeed& seed, ChestPlan* out) {
    ChestPlan p;
    p.trap.assign(PCR_LOC_COUNT, 0);
    p.type.assign(PCR_LOC_COUNT, PC_RANDO_CHEST_TYPE_KEEP);
    if (seed.location_item_names.size() != (size_t)PCR_LOC_COUNT) {
        p.error = "the seed has no location item names";
        *out = p;
        return false;
    }
    Options opts;
    if (!optionsFromJson(seed.options, &opts, &p.error)) {
        *out = p;
        return false;
    }
    p.ctmc = opts.chest_type_matches_contents;
    std::vector<std::string> progress, requiredDungeons;
    if (p.ctmc) {
        if (!allProgressItemNames(opts, &progress, &p.error)) {
            *out = p;
            return false;
        }
        requiredDungeons = requiredDungeonsOf(seed.required_boss_locations);
    }
    std::vector<uint8_t> chestLoc(PCR_LOC_COUNT, 0);
    const Span<LocKey> keys = location_keys();
    for (size_t i = 0; i < keys.size; i++) {
        if (isChestKey(keys[i]) && keys[i].loc < PCR_LOC_COUNT) {
            chestLoc[keys[i].loc] = 1;
        }
    }
    for (int loc = 0; loc < PCR_LOC_COUNT; loc++) {
        if (!chestLoc[loc]) {
            continue;
        }
        const std::string& item = seed.location_item_names[(size_t)loc];
        if (isTrapChestItem(item)) {
            p.trap[loc] = 1;
            p.numTraps++;
        }
        if (p.ctmc) {
            p.type[loc] = (uint8_t)ctmcChestType(item, progress, opts.required_bosses, requiredDungeons);
        }
    }
    for (size_t i = 0; i < keys.size; i++) {
        const LocKey& k = keys[i];
        if (isChestKey(k) && k.loc < PCR_LOC_COUNT && p.trap[k.loc] &&
            std::find(p.trapStages.begin(), p.trapStages.end(), k.stage) == p.trapStages.end()) {
            p.trapStages.push_back(k.stage);
        }
    }
    p.ok = true;
    *out = p;
    return true;
}

int addTrapChestEvent(EvsList& el) {
    if (el.findEvent("DEFAULT_TREASURE_TRAP") >= 0) {
        return 0;
    }
    // tweaks.py add_trap_chest_event_to_stage, in its order (the flags are handed out in it).
    const int ev = el.addEvent("DEFAULT_TREASURE_TRAP");
    if (ev < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    const int chest = el.addActor(ev, "TREASURE");
    if (chest < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.actors[chest].staffType = 0;
    if (el.addAction(chest, "OPEN_SHORT") < 0 || el.addAction(chest, "SPRING_TRAP") < 0 ||
        el.addAction(chest, "WAIT") < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    const int tk = el.addActor(ev, "TIMEKEEPER");
    if (tk < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.actors[tk].staffType = 4;
    if (el.addAction(tk, "WAIT") < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    const int link = el.addActor(ev, "Link");
    if (link < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.actors[link].staffType = 0;
    const int open = el.addAction(link, "010open_treasure");
    if (open < 0 || el.addPropInt(open, "prm0", 1) < 0 || el.addAction(link, "057rd_stop") < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    const int countdown = el.addAction(tk, "COUNTDOWN");
    if (countdown < 0 || el.addPropInt(countdown, "Timer", 92) < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.actions[countdown].startingFlags[0] = el.actions[open].flag;
    const int surprised = el.addAction(link, "024surprised");
    if (surprised < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.actions[surprised].startingFlags[0] = el.actions[countdown].flag;
    const int cam = el.addActor(ev, "CAMERA");
    if (cam < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.actors[cam].staffType = 2;
    if (el.addAction(cam, "PAUSE") < 0) {
        return -PCR_EVL_BAD_RANGE;
    }
    el.events[ev].endingFlags[0] = el.actions[surprised].flag;
    return 1;
}

} // namespace pcr

extern "C" {

const pcr::ChestPlan* pc_rando_trap_plan(void) {
    if (!pc_rando()) {
        return nullptr;
    }
    const PcRandoSeed* seed = pc_rando_seed();
    if (seed == nullptr) {
        return nullptr;
    }
    if (!sPlanBuilt || sPlanSeed != seed || memcmp(sPlanHash, seed->hash, sizeof sPlanHash) != 0) {
        sPlanBuilt = true;
        sPlanSeed = seed;
        memcpy(sPlanHash, seed->hash, sizeof sPlanHash);
        if (pcr::buildChestPlan(*seed, &sPlan)) {
            logf("[cos] rando trap: plan: %d trap chests in %zu stages, chest type matches contents %s\n",
                 sPlan.numTraps, sPlan.trapStages.size(), sPlan.ctmc ? "on" : "off");
        } else {
            logf("[cos] rando trap: no chest plan (%s); chests stay as placed\n", sPlan.error.c_str());
        }
    }
    return sPlan.ok ? &sPlan : nullptr;
}

const char* pc_rando_trap_plan_error(void) {
    return sPlan.error.c_str();
}

int pc_rando_trap_loc(int loc) {
    const pcr::ChestPlan* p = pc_rando_trap_plan();
    return p != nullptr && loc >= 0 && loc < (int)p->trap.size() && p->trap[(size_t)loc] ? 1 : 0;
}

void pc_rando_trap_patch_dzx(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->stage[0] == '\0' ||
        (file->type != PCR_RES_DZS && file->type != PCR_RES_DZR)) {
        return;
    }
    const pcr::ChestPlan* p = pc_rando_trap_plan();
    if (p == nullptr || (p->numTraps == 0 && !p->ctmc)) {
        return;
    }
    const pcr::Span<LocKey> keys = pcr::location_keys();
    PcRandoDzx dzx;
    bool opened = false;
    int nDone = 0, nSkipped = 0, nTraps = 0;
    for (size_t i = 0; i < keys.size && i < (size_t)kMaxKeys; i++) {
        const LocKey& k = keys[i];
        if (!isChestKey(k) || k.loc >= PCR_LOC_COUNT || !keyFileMatches(k, file)) {
            continue;
        }
        const bool trap = p->trap[k.loc] != 0;
        const uint8_t type = p->type[k.loc];
        if (!trap && type == PC_RANDO_CHEST_TYPE_KEEP) {
            continue;
        }
        if (!opened) {
            if (pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK) {
                return; // R2.1 logged it
            }
            opened = true;
        }
        PcRandoTrapKeyStatus& st = sKeyStatus[i];
        st.patches++;
        uint8_t* e = pc_rando_dzx_entry(&dzx, k.tag, k.index);
        if (e == nullptr || !placementHeld(i, k, file) || !entryNameOk(k, e)) {
            // D9: the chest is not the one the placement was written into; leave it.
            st.result = PCR_TRAP_KEY_SKIPPED;
            nSkipped++;
            logf("[cos] rando trap: %s/%s/%s: %s[%u] (%s) not placed; no trap or chest type\n", file->stage, file->arc,
                 file->name, k.tag, k.index, pcr::location_info(k.loc)->enum_name);
            continue;
        }
        uint32_t params = be32(e + 0x08);
        if (trap) {
            // items.py change_chest_item: chest.behavior_type |= 0x40
            params |= PC_RANDO_TBOX_TRAP_BIT;
            nTraps++;
        }
        if (type != PC_RANDO_CHEST_TYPE_KEEP) {
            // chest.chest_type = get_ctmc_chest_type_for_item(item_name)
            params = (params & ~PC_RANDO_TBOX_TYPE_MASK) |
                     (((uint32_t)type << PC_RANDO_TBOX_TYPE_SHIFT) & PC_RANDO_TBOX_TYPE_MASK);
        }
        put32(e + 0x08, params);
        st.result = PCR_TRAP_KEY_DONE;
        st.params = params;
        nDone++;
    }
    if (opened) {
        logf("[cos] rando trap: %s/%s/%s: %d chests (%d traps), %d left as they were\n", file->stage, file->arc,
             file->name, nDone, nTraps, nSkipped);
    }
}

void pc_rando_trap_patch_event(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->type != PCR_RES_DAT || file->stage == nullptr ||
        file->stage[0] == '\0' || strcmp(file->arc, "Stage") != 0 || strcmp(file->name, "event_list.dat") != 0) {
        return;
    }
    const pcr::ChestPlan* p = pc_rando_trap_plan();
    if (p == nullptr ||
        std::find(p->trapStages.begin(), p->trapStages.end(), std::string(file->stage)) == p->trapStages.end()) {
        return;
    }
    snprintf(sEventStats.lastStage, sizeof sEventStats.lastStage, "%s", file->stage);
    pcr::EvsList el;
    const int rd = el.read(file->data, file->size);
    int r = rd == PCR_EVL_OK ? pcr::addTrapChestEvent(el) : -rd;
    if (r == 1) {
        const int w = pcr::pc_rando_evser_write_res(file, el);
        if (w != PCR_EVL_OK) {
            r = -w;
        }
    }
    sEventStats.last = r;
    if (r == 1) {
        sEventStats.added++;
        logf("[cos] rando trap: %s/%s/%s: DEFAULT_TREASURE_TRAP added (%zu bytes)\n", file->stage, file->arc,
             file->name, file->size);
    } else if (r == 0) {
        sEventStats.present++;
    } else {
        sEventStats.failed++;
        logf("[cos] rando trap: %s/%s/%s: DEFAULT_TREASURE_TRAP not added (%s); its trap chests will not open\n",
             file->stage, file->arc, file->name, pc_rando_evl_result_name(-r));
    }
}

void pc_rando_trap_init(void) {
    pc_rando_register_dzx_patcher(pc_rando_trap_patch_dzx);
    pc_rando_register_res_patcher(pc_rando_trap_patch_event, PCR_RES_DAT);
}

const PcRandoTrapKeyStatus* pc_rando_trap_key_status(int key_index) {
    if (key_index < 0 || key_index >= kMaxKeys || (size_t)key_index >= pcr::location_keys().size) {
        return nullptr;
    }
    return &sKeyStatus[key_index];
}

void pc_rando_trap_event_stats(PcRandoTrapEventStats* out) {
    if (out != nullptr) {
        *out = sEventStats;
    }
}

void pc_rando_trap_reset(void) {
    memset(sKeyStatus, 0, sizeof sKeyStatus);
    memset(&sEventStats, 0, sizeof sEventStats);
    sPlan = pcr::ChestPlan();
    sPlanSeed = nullptr;
    sPlanBuilt = false;
}

} // extern "C"
