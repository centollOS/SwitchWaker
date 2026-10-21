// The 4 chests wwrando adds (pc_rando_tweak_added_chests.h, docs/RANDOMIZER_PLAN.md step R2.10):
// tweaks.py add_chest_in_place_of_jabun_cutscene, add_chest_in_place_medli_grappling_hook_gift,
// add_chest_in_place_of_master_sword and add_chest_in_place_queen_fairy_cutscene (wwrando 1.10.0),
// applied by the archive-load hook while pc_rando() is 1. Pure: no game headers, no game calls.
#include "pc_rando_tweak_added_chests.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_lockeys.h"

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
    char n[9] = {};
    memcpy(n, e, 8);
    return strcmp(n, name) == 0;
}

// wwlib ChunkEntry.__setattr__ on a params bitfield: (old & ~mask) | ((value << shift) & mask).
uint32_t setBits(uint32_t word, uint32_t mask, uint32_t value) {
    int shift = 0;
    while (!(mask & (1u << shift))) {
        shift++;
    }
    return (word & ~mask) | ((value << shift) & mask);
}

// d_a_tbox (data/actor_parameters.txt)
constexpr uint32_t kTboxBehavior = 0x0000007F;  // params
constexpr uint32_t kTboxOpenedFlag = 0x00000F80; // params
constexpr uint32_t kTboxAppearSw = 0x000FF000;  // params
constexpr uint32_t kTboxType = 0x00F00000;      // params
constexpr uint32_t kTboxRoom = 0x003F;          // x_rot
// FILI
constexpr uint32_t kFiliWind = 0x000C0000;
// PLYR spawn_id = z_rot & 0xFF
constexpr int kMsSpawnId = 10;

// One added chest as tweaks.py sets it (before change_item writes the placed item).
struct ChestDef {
    int added;
    int behavior, openedFlag, appearSw; // appearSw < 0: left at 0 (tweaks.py does not set it)
    int room;
    double x, y, z;
    uint16_t yRot;
};

const ChestDef kChests[] = {
    // add_chest_in_place_of_jabun_cutscene
    {PCR_ADDED_JABUN_CHEST, 5, 6, 0xFF, 0, 0.0, 300.0, -200.0, 0x8000},
    // add_chest_in_place_medli_grappling_hook_gift
    {PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST, -1, 0x11, -1, 9, -1620.81, 13600.0, 263.034, 0xCC16},
    // add_chest_in_place_of_master_sword: z_pos = -7787.13 - 50
    {PCR_ADDED_MASTER_SWORD_CHEST, 4, 0, 5, 0, -123.495, -3220.0, -7787.13 - 50, 0x0000},
    // add_chest_in_place_queen_fairy_cutscene
    {PCR_ADDED_QUEEN_FAIRY_CHEST, -1, 0x1C, -1, 9, -180031.0, 723.0, -199995.0, 0x1000},
};

const ChestDef* chestDef(int added) {
    for (const ChestDef& c : kChests) {
        if (c.added == added) {
            return &c;
        }
    }
    return nullptr;
}

// The R0.5 Added key of a chest: its location and vanilla item.
const pcr::LocKey* addedKey(int added) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    for (size_t i = 0; i < keys.size; i++) {
        if (keys[i].kind == pcr::KeyKind::Added && keys[i].added == added) {
            return &keys[i];
        }
    }
    return nullptr;
}

PcRandoAddedSiteInfo sSites[PCR_AC_SITE_COUNT] = {
    {"Pjavdou", "Room0", "room.dzr", "Jabun's raft, wind and chest", PCR_ADDED_JABUN_CHEST, -1, 0, 0, 0, 0, 0},
    {"sea", "Room44", "room.dzr", "Jabun's door and whirlpool on the default layer", PCR_ADDED_NONE, -1, 0, 0, 0, 0, 0},
    {"sea", "Stage", "event_list.dat", "ajav_uzu shortened", PCR_ADDED_NONE, -1, 1, 0, 0, 0, 0},
    {"M_Dra09", "Stage", "stage.dzs", "Medli's chest", PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST, -1, 0, 0, 0, 0, 0},
    {"M_NewD2", "Stage", "stage.dzs", "Medli's dummy chest", PCR_ADDED_NONE, -1, 0, 0, 0, 0, 0},
    {"kenroom", "Room0", "room.dzr", "Master Sword chamber chest", PCR_ADDED_MASTER_SWORD_CHEST, -1, 0, 0, 0, 0, 0},
    {"sea", "Room9", "room.dzr", "Mother Isle chest", PCR_ADDED_QUEEN_FAIRY_CHEST, -1, 0, 0, 0, 0, 0},
};

bool sLocsResolved = false;
void resolveLocs() {
    if (sLocsResolved) {
        return;
    }
    for (PcRandoAddedSiteInfo& s : sSites) {
        const pcr::LocKey* k = s.added != PCR_ADDED_NONE ? addedKey(s.added) : nullptr;
        s.loc = k != nullptr ? k->loc : -1;
    }
    sLocsResolved = true;
}

uint8_t placedItem(int added) {
    const pcr::LocKey* k = addedKey(added);
    return k != nullptr ? pc_rando_loc_item(k->loc, k->vanilla) : 0xFF;
}

int siteOf(const PcRandoResFile* f) {
    for (int i = 0; i < PCR_AC_SITE_COUNT; i++) {
        const PcRandoAddedSiteInfo& s = sSites[i];
        if (strcmp(s.stage, f->stage) == 0 && strcmp(s.arc, f->arc) == 0 && strcmp(s.name, f->name) == 0) {
            return i;
        }
    }
    return -1;
}

void finish(int site, int result) {
    PcRandoAddedSiteInfo& s = sSites[site];
    s.runs++;
    s.result = result;
    if (result != 0) {
        logf("[cos] rando: added chests %s/%s/%s (%s): %s; left vanilla\n", s.stage, s.arc, s.name, s.what,
             pc_rando_tweak_added_chests_result_name(site));
    }
}

// The index of the first entry of `tag` named `name` (wwrando's next(x for x in ... if x.name == ..)).
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

int spawnIndex(const PcRandoDzx* dzx, int spawnId) {
    const uint32_t n = pc_rando_dzx_count(dzx, "PLYR");
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, "PLYR", i);
        if (e != nullptr && (be16(e + 0x1C) & 0xFF) == spawnId) {
            return (int)i;
        }
    }
    return -1;
}

void makeActor(uint8_t out[kActrSize], const char* name, uint16_t yRot) {
    memset(out, 0, kActrSize);
    strncpy((char*)out, name, 8);
    put16(out + 0x1A, yRot);
    put16(out + 0x1E, 0xFFFF);
}

constexpr int kMaxAdd = 8;
struct Adds {
    PcRandoDzxAdd list[kMaxAdd];
    uint8_t bytes[kMaxAdd][kActrSize];
    int n = 0;
    void push(const char* tag, const uint8_t* e) {
        memcpy(bytes[n], e, kActrSize);
        list[n] = {tag, bytes[n], 1};
        n++;
    }
};

// kenroom/Room0: the ACT5 actors tweaks.py copies (in layer order).
bool msCopied(const uint8_t* e) {
    return nameIs(e, "Tn") || nameIs(e, "ALLdie") || nameIs(e, "Yswdr00");
}

// Checks the file and queues the appends; the in-place edits come after the grow (planInPlace).
int planAdds(int site, const PcRandoDzx* dzx, Adds& adds, uint8_t* chestItem) {
    const PcRandoAddedSiteInfo& s = sSites[site];
    uint8_t e[kActrSize];
    // Every chest becomes Chest000 of its file (the R0.5 key) except the dummy.
    if (s.added != PCR_ADDED_NONE && pc_rando_dzx_count(dzx, "TRES") != 0) {
        return PCR_DZX_MISMATCH;
    }
    switch (site) {
    case PCR_AC_JABUN_ROOM:
        if (pc_rando_dzx_count(dzx, "FILI") < 1) {
            return PCR_DZX_NO_CHUNK;
        }
        makeActor(e, "Ikada", 0x8000);
        adds.push("ACTR", e);
        break;
    case PCR_AC_JABUN_DOOR: {
        const int door = firstNamed(dzx, "ACT5", "Ajav");
        const int pool = firstNamed(dzx, "ACT5", "Auzu");
        if (door < 0 || pool < 0) {
            return PCR_DZX_MISMATCH;
        }
        adds.push("ACTR", pc_rando_dzx_entry(dzx, "ACT5", (uint32_t)door));
        adds.push("ACTR", pc_rando_dzx_entry(dzx, "ACT5", (uint32_t)pool));
        break;
    }
    case PCR_AC_MEDLI_CHEST:
    case PCR_AC_QUEEN_FAIRY:
        break;
    case PCR_AC_MEDLI_DUMMY:
        pc_rando_added_chest_entry(PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST, 0xFF, e);
        adds.push("TRES", e);
        *chestItem = 0xFF;
        return PCR_DZX_OK;
    case PCR_AC_MASTER_SWORD: {
        if (firstNamed(dzx, "ACTR", "VmsMS") < 0 || firstNamed(dzx, "ACTR", "VmsDZ") < 0 ||
            spawnIndex(dzx, kMsSpawnId) < 0) {
            return PCR_DZX_MISMATCH;
        }
        const uint32_t n = pc_rando_dzx_count(dzx, "ACT5");
        int copied = 0;
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t* a = pc_rando_dzx_entry(dzx, "ACT5", i);
            if (a != nullptr && msCopied(a)) {
                if (adds.n >= kMaxAdd - 1) {
                    return PCR_DZX_MISMATCH; // more Darknuts than the vanilla room has
                }
                adds.push("ACTR", a);
                copied++;
            }
        }
        if (copied == 0) {
            return PCR_DZX_MISMATCH;
        }
        break;
    }
    default:
        return PCR_DZX_BAD_FILE;
    }
    if (s.added != PCR_ADDED_NONE) {
        *chestItem = placedItem(s.added);
        pc_rando_added_chest_entry(s.added, *chestItem, e);
        adds.push("TRES", e);
    }
    return PCR_DZX_OK;
}

void applyInPlace(int site, PcRandoDzx* dzx) {
    switch (site) {
    case PCR_AC_JABUN_ROOM: {
        uint8_t* fili = pc_rando_dzx_entry(dzx, "FILI", 0);
        put32(fili, setBits(be32(fili), kFiliWind, 0)); // weakest wind (0.3), so the raft's flag blows
        break;
    }
    case PCR_AC_JABUN_DOOR:
        pc_rando_dzx_suppress(dzx, "ACT5", (uint32_t)firstNamed(dzx, "ACT5", "Ajav"), nullptr);
        pc_rando_dzx_suppress(dzx, "ACT5", (uint32_t)firstNamed(dzx, "ACT5", "Auzu"), nullptr);
        break;
    case PCR_AC_MASTER_SWORD: {
        const uint32_t nActr = pc_rando_dzx_count(dzx, "ACTR");
        for (uint32_t i = 0; i < nActr; i++) {
            const uint8_t* a = pc_rando_dzx_entry(dzx, "ACTR", i);
            if (nameIs(a, "VmsMS") || nameIs(a, "VmsDZ")) {
                pc_rando_dzx_suppress(dzx, "ACTR", i, nullptr);
            }
        }
        const uint32_t n5 = pc_rando_dzx_count(dzx, "ACT5");
        for (uint32_t i = 0; i < n5; i++) {
            pc_rando_dzx_suppress(dzx, "ACT5", i, nullptr);
        }
        uint8_t* spawn = pc_rando_dzx_entry(dzx, "PLYR", (uint32_t)spawnIndex(dzx, kMsSpawnId));
        putf(spawn + 0x10, (float)-2949.39);
        putf(spawn + 0x14, (float)-4240.7);
        break;
    }
    default:
        break;
    }
}

void patchDzx(PcRandoResFile* f, int site) {
    PcRandoAddedSiteInfo& s = sSites[site];
    PcRandoDzx dzx;
    int r = pc_rando_dzx_open(&dzx, f->data, f->size);
    if (r != PCR_DZX_OK) {
        s.in_hash = 0;
        finish(site, r);
        return;
    }
    s.in_hash = pc_rando_tweak_added_chests_content_hash(f->data, f->size);
    Adds adds;
    uint8_t item = 0xFF;
    r = planAdds(site, &dzx, adds, &item);
    if (r == PCR_DZX_OK) {
        r = pc_rando_res_grow_dzx(f, adds.list, adds.n, nullptr);
    }
    if (r == PCR_DZX_OK) {
        r = pc_rando_dzx_open(&dzx, f->data, f->size);
    }
    if (r == PCR_DZX_OK) {
        applyInPlace(site, &dzx);
        s.item = item;
    }
    finish(site, r);
    if (r == PCR_DZX_OK) {
        if (s.added != PCR_ADDED_NONE || site == PCR_AC_MEDLI_DUMMY) {
            logf("[cos] rando: added chests %s/%s/%s: %s (item 0x%02X)\n", s.stage, s.arc, s.name, s.what, item);
        } else {
            logf("[cos] rando: added chests %s/%s/%s: %s\n", s.stage, s.arc, s.name, s.what);
        }
    }
}

// sea/Stage event_list.dat "ajav_uzu".
struct AjavActors {
    int event, director, camera, ship;
};

int findAjav(const PcRandoEvl* evl, AjavActors* a) {
    a->event = pc_rando_evl_find_event(evl, "ajav_uzu");
    if (a->event < 0) {
        return PCR_EVL_NO_EVENT;
    }
    const int sd = pc_rando_evl_find_actor(evl, a->event, "DIRECTOR");
    const int sc = pc_rando_evl_find_actor(evl, a->event, "CAMERA");
    const int ss = pc_rando_evl_find_actor(evl, a->event, "Ship");
    if (sd < 0 || sc < 0 || ss < 0) {
        return PCR_EVL_NO_ACTOR;
    }
    a->director = pc_rando_evl_event_actor(evl, a->event, sd);
    a->camera = pc_rando_evl_event_actor(evl, a->event, sc);
    a->ship = pc_rando_evl_event_actor(evl, a->event, ss);
    return PCR_EVL_OK;
}

constexpr int kKeepDirector = 1, kKeepCamera = 2, kKeepShip = 2;

int32_t lastFlag(const PcRandoEvl* evl, int actor) {
    const int n = pc_rando_evl_action_count(evl, actor);
    return n > 0 ? pc_rando_evl_action_flag(evl, pc_rando_evl_actor_action(evl, actor, n - 1)) : -1;
}

int patchAjav(PcRandoEvl* evl) {
    AjavActors a;
    int r = findAjav(evl, &a);
    if (r != PCR_EVL_OK) {
        return r;
    }
    // Python slicing keeps what is there; the vanilla event has more actions than are kept.
    if (pc_rando_evl_action_count(evl, a.director) < kKeepDirector ||
        pc_rando_evl_action_count(evl, a.camera) < kKeepCamera ||
        pc_rando_evl_action_count(evl, a.ship) < kKeepShip) {
        return PCR_EVL_NO_ACTION;
    }
    if ((r = pc_rando_evl_truncate_actions(evl, a.director, kKeepDirector)) != PCR_EVL_OK ||
        (r = pc_rando_evl_truncate_actions(evl, a.camera, kKeepCamera)) != PCR_EVL_OK ||
        (r = pc_rando_evl_truncate_actions(evl, a.ship, kKeepShip)) != PCR_EVL_OK) {
        return r;
    }
    const int32_t flags[3] = {lastFlag(evl, a.director), lastFlag(evl, a.camera), -1};
    for (int i = 0; i < 3; i++) {
        if ((r = pc_rando_evl_set_ending_flag(evl, a.event, i, flags[i])) != PCR_EVL_OK) {
            return r;
        }
    }
    return PCR_EVL_OK;
}

bool fail(char* why, size_t cap, const char* msg) {
    if (why != nullptr && cap > 0) {
        snprintf(why, cap, "%s", msg);
    }
    return false;
}

} // namespace

extern "C" {

void pc_rando_tweak_added_chests_patch_dzx(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->arc == nullptr || file->name == nullptr ||
        (file->type != PCR_RES_DZS && file->type != PCR_RES_DZR)) {
        return;
    }
    resolveLocs();
    const int site = siteOf(file);
    if (site < 0 || sSites[site].is_event) {
        return;
    }
    patchDzx(file, site);
}

void pc_rando_tweak_added_chests_patch_event(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->arc == nullptr || file->name == nullptr ||
        file->type != PCR_RES_DAT) {
        return;
    }
    resolveLocs();
    const int site = siteOf(file);
    if (site < 0 || !sSites[site].is_event) {
        return;
    }
    PcRandoEvl evl;
    int r = pc_rando_evl_open(&evl, file->data, file->size);
    sSites[site].in_hash = 0;
    if (r == PCR_EVL_OK) {
        // Check first, so a refused edit leaves the whole file vanilla.
        AjavActors a;
        r = findAjav(&evl, &a);
    }
    if (r == PCR_EVL_OK) {
        r = patchAjav(&evl);
    }
    finish(site, r);
    if (r == PCR_EVL_OK) {
        logf("[cos] rando: added chests %s/%s/%s: %s\n", sSites[site].stage, sSites[site].arc, sSites[site].name,
             sSites[site].what);
    }
}

void pc_rando_tweak_added_chests_init(void) {
    resolveLocs();
    pc_rando_register_dzx_patcher(pc_rando_tweak_added_chests_patch_dzx);
    pc_rando_register_res_patcher(pc_rando_tweak_added_chests_patch_event, PCR_RES_DAT);
}

const PcRandoAddedSiteInfo* pc_rando_tweak_added_chests_site(int i) {
    resolveLocs();
    return i >= 0 && i < PCR_AC_SITE_COUNT ? &sSites[i] : nullptr;
}

void pc_rando_tweak_added_chests_reset(void) {
    for (PcRandoAddedSiteInfo& s : sSites) {
        s.runs = 0;
        s.result = 0;
        s.item = 0;
        s.in_hash = 0;
    }
}

const char* pc_rando_tweak_added_chests_result_name(int site) {
    if (site < 0 || site >= PCR_AC_SITE_COUNT) {
        return "?";
    }
    return sSites[site].is_event ? pc_rando_evl_result_name(sSites[site].result)
                                 : pc_rando_dzx_result_name(sSites[site].result);
}

int pc_rando_added_chest_entry(int added, uint8_t item, uint8_t out[0x20]) {
    const ChestDef* c = chestDef(added);
    if (c == nullptr) {
        return 0;
    }
    // tweaks.py: name "takara3", params 0xFF000000, switch_to_set 0xFF, chest_type 2, then the
    // chest's own fields; items.py change_item writes item_id.
    uint32_t params = 0xFF000000u;
    params = setBits(params, kTboxType, 2);
    if (c->appearSw >= 0) {
        params = setBits(params, kTboxAppearSw, (uint32_t)c->appearSw);
    }
    params = setBits(params, kTboxOpenedFlag, (uint32_t)c->openedFlag);
    if (c->behavior >= 0) {
        params = setBits(params, kTboxBehavior, (uint32_t)c->behavior);
    }
    makeActor(out, "takara3", c->yRot);
    put32(out + 0x08, params);
    putf(out + 0x0C, (float)c->x);
    putf(out + 0x10, (float)c->y);
    putf(out + 0x14, (float)c->z);
    put16(out + 0x18, (uint16_t)setBits(0, kTboxRoom, (uint32_t)c->room));
    put16(out + 0x1C, (uint16_t)(((uint16_t)item << 8) | 0xFF));
    return 1;
}

uint8_t pc_rando_added_chests_demo_item(int argID, uint8_t vanilla) {
    if (!pc_rando()) {
        return vanilla;
    }
    // main.dol 0x8035BD34 + argID is l_itemNo[argID] (item_locations.txt paths).
    switch (argID) {
    case 0:
        return pc_rando_loc_item(PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER, vanilla);
    case 5:
        return pc_rando_loc_item(PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE, vanilla);
    case 8:
        return pc_rando_loc_item(PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE, vanilla);
    default:
        return vanilla;
    }
}

int pc_rando_tweak_added_chests_verify(int site, const uint8_t* data, size_t size, uint8_t item, char* why,
                                       size_t whyCap) {
    if (site < 0 || site >= PCR_AC_SITE_COUNT || data == nullptr) {
        return fail(why, whyCap, "bad site or no data");
    }
    const PcRandoAddedSiteInfo& s = sSites[site];
    if (s.is_event) {
        PcRandoEvl evl;
        if (pc_rando_evl_open(&evl, const_cast<uint8_t*>(data), size) != PCR_EVL_OK) {
            return fail(why, whyCap, "not an event list");
        }
        AjavActors a;
        if (findAjav(&evl, &a) != PCR_EVL_OK) {
            return fail(why, whyCap, "no ajav_uzu with DIRECTOR, CAMERA and Ship");
        }
        if (pc_rando_evl_action_count(&evl, a.director) != kKeepDirector ||
            pc_rando_evl_action_count(&evl, a.camera) != kKeepCamera ||
            pc_rando_evl_action_count(&evl, a.ship) != kKeepShip) {
            return fail(why, whyCap, "action chains not shortened to 1/2/2");
        }
        const uint8_t* ev = pc_rando_evl_event_p(&evl, a.event);
        const int32_t want[3] = {lastFlag(&evl, a.director), lastFlag(&evl, a.camera), -1};
        for (int i = 0; i < 3; i++) {
            if ((int32_t)be32(ev + 0x88 + 4 * i) != want[i]) {
                return fail(why, whyCap, "ending flags not [DIRECTOR last, CAMERA last, -1]");
            }
        }
        return 1;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<uint8_t*>(data), size) != PCR_DZX_OK) {
        return fail(why, whyCap, "not a dzx");
    }
    uint8_t want[kActrSize];
    if (s.added != PCR_ADDED_NONE || site == PCR_AC_MEDLI_DUMMY) {
        const int added = site == PCR_AC_MEDLI_DUMMY ? PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST : s.added;
        pc_rando_added_chest_entry(added, site == PCR_AC_MEDLI_DUMMY ? 0xFF : item, want);
        const uint32_t n = pc_rando_dzx_count(&dzx, "TRES");
        const uint32_t idx = site == PCR_AC_MEDLI_DUMMY ? n - 1 : 0;
        const uint8_t* e = n > 0 ? pc_rando_dzx_entry(&dzx, "TRES", idx) : nullptr;
        if (e == nullptr || memcmp(e, want, kActrSize) != 0) {
            return fail(why, whyCap, site == PCR_AC_MEDLI_DUMMY ? "last TRES is not the dummy chest"
                                                                 : "TRES[0] is not the added chest with the item");
        }
    }
    switch (site) {
    case PCR_AC_JABUN_ROOM: {
        makeActor(want, "Ikada", 0x8000);
        const uint32_t n = pc_rando_dzx_count(&dzx, "ACTR");
        const uint8_t* e = n > 0 ? pc_rando_dzx_entry(&dzx, "ACTR", n - 1) : nullptr;
        if (e == nullptr || memcmp(e, want, kActrSize) != 0) {
            return fail(why, whyCap, "no raft (Ikada) at the end of ACTR");
        }
        const uint8_t* fili = pc_rando_dzx_entry(&dzx, "FILI", 0);
        if (fili == nullptr || (be32(fili) & kFiliWind) != 0) {
            return fail(why, whyCap, "FILI[0] wind_type is not 0");
        }
        break;
    }
    case PCR_AC_JABUN_DOOR:
        if (firstNamed(&dzx, "ACT5", "Ajav") >= 0 || firstNamed(&dzx, "ACT5", "Auzu") >= 0) {
            return fail(why, whyCap, "Ajav/Auzu still on ACT5");
        }
        if (firstNamed(&dzx, "ACTR", "Ajav") < 0 || firstNamed(&dzx, "ACTR", "Auzu") < 0) {
            return fail(why, whyCap, "Ajav/Auzu missing from ACTR");
        }
        break;
    case PCR_AC_MASTER_SWORD: {
        if (firstNamed(&dzx, "ACTR", "VmsMS") >= 0 || firstNamed(&dzx, "ACTR", "VmsDZ") >= 0) {
            return fail(why, whyCap, "VmsMS/VmsDZ still on ACTR");
        }
        const uint32_t n5 = pc_rando_dzx_count(&dzx, "ACT5");
        for (uint32_t i = 0; i < n5; i++) {
            if (pc_rando_dzx_entry(&dzx, "ACT5", i)[0] != 0) {
                return fail(why, whyCap, "an ACT5 actor is left");
            }
        }
        if (firstNamed(&dzx, "ACTR", "Tn") < 0 || firstNamed(&dzx, "ACTR", "ALLdie") < 0 ||
            firstNamed(&dzx, "ACTR", "Yswdr00") < 0) {
            return fail(why, whyCap, "Tn/ALLdie/Yswdr00 missing from ACTR");
        }
        const int sp = spawnIndex(&dzx, kMsSpawnId);
        const uint8_t* e = sp >= 0 ? pc_rando_dzx_entry(&dzx, "PLYR", (uint32_t)sp) : nullptr;
        if (e == nullptr || pc_rando_dzx_bef32(e + 0x10) != (float)-2949.39 ||
            pc_rando_dzx_bef32(e + 0x14) != (float)-4240.7) {
            return fail(why, whyCap, "spawn 10 not moved");
        }
        break;
    }
    default:
        break;
    }
    return 1;
}

uint64_t pc_rando_tweak_added_chests_hash_chunk(uint64_t h, const char tag[4], uint32_t count, const uint8_t* entries) {
    auto feed = [&h](const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; i++) {
            h ^= p[i];
            h *= 0x100000001B3ull;
        }
    };
    const bool edited = memcmp(tag, "TRES", 4) == 0 || memcmp(tag, "PLYR", 4) == 0 || memcmp(tag, "FILI", 4) == 0 ||
                        memcmp(tag, "ACT", 3) == 0;
    if (!edited) {
        return h;
    }
    uint8_t cnt[4];
    put32(cnt, count);
    feed((const uint8_t*)tag, 4);
    feed(cnt, 4);
    const char t[5] = {tag[0], tag[1], tag[2], tag[3], 0};
    const uint32_t esize = pc_rando_dzx_entry_size(t);
    if (esize != 0 && entries != nullptr) {
        feed(entries, (size_t)esize * count);
    }
    return h;
}

uint64_t pc_rando_tweak_added_chests_hash(const void* data, size_t size) {
    uint64_t h = PCR_AC_HASH_SEED;
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

uint64_t pc_rando_tweak_added_chests_content_hash(const uint8_t* data, size_t size) {
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<uint8_t*>(data), size) != PCR_DZX_OK) {
        return 0;
    }
    uint64_t h = PCR_AC_HASH_SEED;
    for (uint32_t i = 0; i < dzx.chunkCount; i++) {
        const uint8_t* row = data + 4 + 12 * i;
        const uint32_t count = be32(row + 4);
        const uint32_t off = be32(row + 8);
        h = pc_rando_tweak_added_chests_hash_chunk(h, (const char*)row, count, off < size ? data + off : nullptr);
    }
    return h;
}

} // extern "C"
