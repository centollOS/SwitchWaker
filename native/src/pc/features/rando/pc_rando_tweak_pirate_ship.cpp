// The pirate ship at Windfall (pc_rando_tweak_pirate_ship.h, docs/RANDOMIZER_PLAN.md step R3.17):
// tweaks.py add_pirate_ship_to_windfall (wwrando 1.10.0), applied by the archive-load hook while
// pc_rando() is 1. Pure: no game headers, no game calls.
#include "pc_rando_tweak_pirate_ship.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_event_ser.h"
#include "pc_rando_wrap.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

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

constexpr uint32_t kActrSize = 0x20, kScobSize = 0x24, kEvntSize = 0x18;
// The USA disc's Asoko stage.dzs has 2 EVNT entries (Hlift_up, RopeGame2Shut).
constexpr int kVanillaEvntIndex = 2;

// --- big-endian helpers ---

uint32_t be32(const uint8_t* p) {
    return pc_rando_dzx_be32(p);
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

// wwlib's ACTR()/SCOB() defaults: params 0, rotations 0, enemy_number 0xFFFF (SCOB: scale 10, padding
// 0xFF), then the name and position.
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

// d_a_obj_pirateship pirate_ship_door_type, d_a_obj_ashut switch_to_check (params).
constexpr uint32_t kShipDoorType = 0x0000FF00;
constexpr uint32_t kAshutSwitch = 0x000000FF;

// Aryll on the ship (tweaks.py: aryll.x/y/z_pos, y_rot; which_aryll 0).
constexpr float kAryllX = 600.0f, kAryllY = -550.0f, kAryllZ = -200.0f;

PcRandoPirateShipSiteInfo sSites[PCR_PS_SITE_COUNT] = {
    {"sea", "Room11", "room.dzr", "the pirate ship on Windfall's default layer", 0, 0, 0},
    {"Asoko", "Room0", "room.dzr", "Niko out, Aryll, SW_C00, 3 SwOps, Ashut on 0xC4", 0, 0, 0},
    {"Asoko", "Stage", "stage.dzs", "EVNT AryllOpensDoor", 0, 0, 0},
    {"Asoko", "Stage", "event_list.dat", "event AryllOpensDoor", 1, 0, 0},
};
bool sInit = false;
int sEvntIndex = -1; // from the last stage patch

int siteOf(const PcRandoResFile* f) {
    for (int i = 0; i < PCR_PS_SITE_COUNT; i++) {
        const PcRandoPirateShipSiteInfo& s = sSites[i];
        if (strcmp(s.stage, f->stage) == 0 && strcmp(s.arc, f->arc) == 0 && strcmp(s.name, f->name) == 0) {
            return i;
        }
    }
    return -1;
}

void finish(int site, int result) {
    PcRandoPirateShipSiteInfo& s = sSites[site];
    s.runs++;
    s.result = result;
    if (result != 0) {
        logf("[cos] rando: pirate ship %s/%s/%s (%s): %s; left vanilla\n", s.stage, s.arc, s.name, s.what,
             pc_rando_tweak_pirate_ship_result_name(site));
    } else {
        logf("[cos] rando: pirate ship %s/%s/%s: %s\n", s.stage, s.arc, s.name, s.what);
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

constexpr int kMaxAdd = 6;
struct Adds {
    PcRandoDzxAdd list[kMaxAdd];
    uint8_t bytes[kMaxAdd][kScobSize];
    int n = 0;
    void push(const char* tag, const uint8_t* e, uint32_t size) {
        memcpy(bytes[n], e, size);
        list[n] = {tag, bytes[n], 1};
        n++;
    }
};

const char* const kLayers[2] = {"ACT2", "ACT3"};

// Checks the file and queues the appends; the in-place edits come after the grow (applyInPlace).
int planAdds(int site, const PcRandoDzx* dzx, Adds& adds) {
    uint8_t e[kScobSize];
    switch (site) {
    case PCR_PS_WINDFALL: {
        const int ship = firstNamed(dzx, "ACT2", "Pirates");
        if (ship < 0) {
            return PCR_DZX_MISMATCH;
        }
        memcpy(e, pc_rando_dzx_entry(dzx, "ACT2", (uint32_t)ship), kActrSize);
        put32(e + 0x08, be32(e + 0x08) & ~kShipDoorType); // pirate_ship_door_type = 0
        adds.push("ACTR", e, kActrSize);
        return PCR_DZX_OK;
    }
    case PCR_PS_SHIP_ROOM:
        for (const char* layer : kLayers) {
            if (firstNamed(dzx, layer, "P2b") < 0 || firstNamed(dzx, layer, "Ashut") < 0) {
                return PCR_DZX_MISMATCH;
            }
        }
        // tweaks.py's add_entity order: Aryll, SW_C00, then the three SwOps.
        pc_rando_tweak_pirate_ship_aryll(e);
        adds.push("ACTR", e, kActrSize);
        pc_rando_tweak_pirate_ship_swc00(e);
        adds.push("SCOB", e, kScobSize);
        for (int which = 0; which < 3; which++) {
            pc_rando_tweak_pirate_ship_swop(which, pc_rando_tweak_pirate_ship_evnt_index(), e);
            adds.push("ACTR", e, kActrSize);
        }
        return PCR_DZX_OK;
    case PCR_PS_SHIP_STAGE: {
        const uint32_t n = pc_rando_dzx_count(dzx, "EVNT");
        for (uint32_t i = 0; i < n; i++) {
            char name[16] = {};
            memcpy(name, pc_rando_dzx_entry(dzx, "EVNT", i) + 1, 0xF);
            if (strcmp(name, PCR_PS_EVENT_NAME) == 0) {
                return PCR_DZX_MISMATCH; // already there
            }
        }
        pc_rando_tweak_pirate_ship_evnt(e);
        adds.push("EVNT", e, kEvntSize);
        return PCR_DZX_OK;
    }
    default:
        return PCR_DZX_BAD_FILE;
    }
}

void applyInPlace(int site, PcRandoDzx* dzx) {
    if (site != PCR_PS_SHIP_ROOM) {
        return;
    }
    for (const char* layer : kLayers) {
        pc_rando_dzx_suppress(dzx, layer, (uint32_t)firstNamed(dzx, layer, "P2b"), nullptr);
        uint8_t* ashut = pc_rando_dzx_entry(dzx, layer, (uint32_t)firstNamed(dzx, layer, "Ashut"));
        put32(ashut + 0x08, (be32(ashut + 0x08) & ~kAshutSwitch) | PCR_PS_SW_DOOR_OPEN);
    }
}

void patchDzx(PcRandoResFile* f, int site) {
    PcRandoDzx dzx;
    int r = pc_rando_dzx_open(&dzx, f->data, f->size);
    Adds adds;
    if (r == PCR_DZX_OK) {
        r = planAdds(site, &dzx, adds);
    }
    uint32_t first[kMaxAdd] = {};
    if (r == PCR_DZX_OK) {
        r = pc_rando_res_grow_dzx(f, adds.list, adds.n, first);
    }
    if (r == PCR_DZX_OK) {
        r = pc_rando_dzx_open(&dzx, f->data, f->size);
    }
    if (r == PCR_DZX_OK) {
        applyInPlace(site, &dzx);
        if (site == PCR_PS_SHIP_STAGE) {
            sEvntIndex = (int)first[0];
            if (sEvntIndex != kVanillaEvntIndex) {
                logf("[cos] rando: pirate ship: EVNT %s is index %d (the USA disc gives %d)\n", PCR_PS_EVENT_NAME,
                     sEvntIndex, kVanillaEvntIndex);
            }
        }
    }
    finish(site, r);
}

// --- the event ---

int addProp(pcr::EvsList& l, int action, const char* name, int32_t v) {
    return l.addPropInt(action, name, v);
}

int patchEvents(PcRandoResFile* f) {
    pcr::EvsList l;
    int r = l.read(f->data, f->size);
    if (r != PCR_EVL_OK) {
        return r;
    }
    if (l.findEvent(PCR_PS_EVENT_NAME) >= 0) {
        return PCR_EVL_MISMATCH;
    }
    const int ev = l.addEvent(PCR_PS_EVENT_NAME);
    const int camera = ev >= 0 ? l.addActor(ev, "CAMERA") : -1;
    const int aryll = ev >= 0 ? l.addActor(ev, "Ls1") : -1;
    const int link = ev >= 0 ? l.addActor(ev, "Link") : -1;
    if (camera < 0 || aryll < 0 || link < 0) {
        return PCR_EVL_BAD_RANGE;
    }
    l.actors[camera].staffType = 2;
    l.actors[aryll].staffType = 0;
    l.actors[link].staffType = 0;
    bool ok = true;
    // Python ints in a tuple: written as floats by save_changes.
    int a = l.addAction(camera, "FIXEDFRM");
    ok = ok && a >= 0 && l.addPropVec(a, "Eye", kAryllX, kAryllY + 90, kAryllZ - 120) >= 0 &&
         l.addPropVec(a, "Center", kAryllX, kAryllY + 70, kAryllZ) >= 0 && l.addPropFloat(a, "Fovy", 60.0f) >= 0 &&
         addProp(l, a, "Timer", 30) >= 0;
    struct Step {
        const char* action;
        const char* prop;
        int32_t value;
    };
    static const Step kAryll[] = {
        {"LOK_PLYER", "prm_0", 8},                  // look at the player
        {"ANM_CHG", "AnmNo", 8},                    // looking through the telescope
        {"WAIT", "Timer", 30},
        {"TALK_MSG", "msg_num", PCR_PS_MSG_STUCK},  // "Oh! Did you get stuck in there, Big Brother?"
        {"ANM_CHG", "AnmNo", 4},                    // arms behind her back, swaying her head
        {"TALK_MSG", "msg_num", PCR_PS_MSG_OPEN},   // "Don't worry, I'll open the door for you."
        {"ANM_CHG", "AnmNo", 5},                    // idle
    };
    for (const Step& s : kAryll) {
        a = ok ? l.addAction(aryll, s.action) : -1;
        ok = a >= 0 && addProp(l, a, s.prop, s.value) >= 0;
    }
    ok = ok && l.addAction(link, "001wait") >= 0;
    if (!ok) {
        return PCR_EVL_BAD_RANGE;
    }
    // event.ending_flags[0] = aryll_actor.actions[-1].flag_id_to_set
    l.events[ev].endingFlags[0] = l.actions[l.actors[aryll].actions.back()].flag;
    return pcr::pc_rando_evser_write_res(f, l);
}

bool fail(char* why, size_t cap, const char* msg) {
    if (why != nullptr && cap > 0) {
        snprintf(why, cap, "%s", msg);
    }
    return false;
}

const char* const kAryllParts[2] = {
    "'Hoy! Big Brother!\nWanna play a game? It's fun, trust me!",
    "Just \\{1A 06 FF 00 00 01}step on this button\\{1A 06 FF 00 00 00}, and try to swing across the ropes to "
    "reach that door over there before time's up!",
};

} // namespace

extern "C" {

void pc_rando_tweak_pirate_ship_patch_dzx(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->arc == nullptr || file->name == nullptr ||
        (file->type != PCR_RES_DZS && file->type != PCR_RES_DZR)) {
        return;
    }
    const int site = siteOf(file);
    if (site < 0 || sSites[site].is_event) {
        return;
    }
    patchDzx(file, site);
}

void pc_rando_tweak_pirate_ship_patch_event(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->arc == nullptr || file->name == nullptr ||
        file->type != PCR_RES_DAT) {
        return;
    }
    const int site = siteOf(file);
    if (site < 0 || !sSites[site].is_event) {
        return;
    }
    finish(site, patchEvents(file));
}

void pc_rando_tweak_pirate_ship_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_dzx_patcher(pc_rando_tweak_pirate_ship_patch_dzx);
    pc_rando_register_res_patcher(pc_rando_tweak_pirate_ship_patch_event, PCR_RES_DAT);
}

const PcRandoPirateShipSiteInfo* pc_rando_tweak_pirate_ship_site(int site) {
    return site >= 0 && site < PCR_PS_SITE_COUNT ? &sSites[site] : nullptr;
}

const char* pc_rando_tweak_pirate_ship_result_name(int site) {
    if (site < 0 || site >= PCR_PS_SITE_COUNT) {
        return "?";
    }
    return sSites[site].is_event ? pc_rando_evl_result_name(sSites[site].result)
                                 : pc_rando_dzx_result_name(sSites[site].result);
}

void pc_rando_tweak_pirate_ship_reset(void) {
    for (PcRandoPirateShipSiteInfo& s : sSites) {
        s.runs = 0;
        s.result = 0;
    }
}

int pc_rando_tweak_pirate_ship_evnt_index(void) {
    return sEvntIndex >= 0 ? sEvntIndex : kVanillaEvntIndex;
}

void pc_rando_tweak_pirate_ship_aryll(uint8_t out[0x20]) {
    makeEntry(out, kActrSize, "Ls1", 0, kAryllX, kAryllY, kAryllZ);
    put16(out + 0x1A, 0xC000);
}

void pc_rando_tweak_pirate_ship_swc00(uint8_t out[0x24]) {
    // d_a_swc00: switch_to_set 0xFF, prerequisite_switch 0xFF00, behavior_type 0x30000 (0: the switch
    // is cleared again when the player leaves the area).
    makeEntry(out, kScobSize, "SW_C00", (0xFFu << 8) | PCR_PS_SW_IN_CHEST_ROOM, 0.0f, -550.0f, -3900.0f);
    out[0x20] = 64;
    out[0x21] = 36;
    out[0x22] = 64;
}

void pc_rando_tweak_pirate_ship_swop(int which, int evntIndex, uint8_t out[0x20]) {
    // d_a_switch_op: operation 0xF (0 AND, 2 OR, 3 NOR), is_continuous 0x10, switch_to_set 0xFF00,
    // first_switch_to_check 0xFF0000, num_switches_to_check 0xFF000000; evnt_index x_rot & 0xFF; delay
    // z_rot.
    struct Op {
        uint32_t op, num, first, set;
        float z;
    };
    static const Op kOps[3] = {
        {3, 1, PCR_PS_SW_COUNTDOWN, PCR_PS_SW_NO_COUNTDOWN, -4400.0f},
        {0, 2, PCR_PS_SW_NO_COUNTDOWN, PCR_PS_SW_ARYLL_OPENED, -3900.0f},
        {2, 2, PCR_PS_SW_COUNTDOWN, PCR_PS_SW_DOOR_OPEN, -3400.0f},
    };
    const Op& o = kOps[which < 0 || which > 2 ? 0 : which];
    makeEntry(out, kActrSize, "SwOp", (o.num << 24) | (o.first << 16) | (o.set << 8) | 0x10 | o.op, 0.0f, 0.0f, o.z);
    put16(out + 0x18, which == 1 ? (uint16_t)(evntIndex & 0xFF) : 0xFF);
    put16(out + 0x1C, which == 1 ? PCR_PS_ARYLL_DELAY : 0);
}

void pc_rando_tweak_pirate_ship_evnt(uint8_t out[0x18]) {
    // wwlib EVNT(): unknown_1 0xFF, name (15 bytes), unknown_2/3 0xFF, unknown_4 0,
    // event_played_by_spawn_switch 0xFF, room_index 0xFF, padding FF FF FF.
    memset(out, 0, kEvntSize);
    out[0] = 0xFF;
    strncpy((char*)out + 1, PCR_PS_EVENT_NAME, 0xF);
    out[0x10] = out[0x11] = 0xFF;
    out[0x12] = 0;
    out[0x13] = out[0x14] = 0xFF;
    out[0x15] = out[0x16] = out[0x17] = 0xFF;
}

int pc_rando_tweak_pirate_ship_verify(int site, const uint8_t* data, size_t size, char* why, size_t whyCap) {
    if (site < 0 || site >= PCR_PS_SITE_COUNT || data == nullptr) {
        return fail(why, whyCap, "bad site or no data");
    }
    if (site == PCR_PS_SHIP_EVENTS) {
        PcRandoEvl evl;
        if (pc_rando_evl_open(&evl, const_cast<uint8_t*>(data), size) != PCR_EVL_OK) {
            return fail(why, whyCap, "not an event list");
        }
        const int ev = pc_rando_evl_find_event(&evl, PCR_PS_EVENT_NAME);
        if (ev < 0) {
            return fail(why, whyCap, "no event AryllOpensDoor");
        }
        const char* staff[3] = {"CAMERA", "Ls1", "Link"};
        const int count[3] = {1, 7, 1};
        int aryll = -1;
        for (int i = 0; i < 3; i++) {
            const int slot = pc_rando_evl_find_actor(&evl, ev, staff[i]);
            const int actor = slot >= 0 ? pc_rando_evl_event_actor(&evl, ev, slot) : -1;
            if (actor < 0 || pc_rando_evl_action_count(&evl, actor) != count[i]) {
                return fail(why, whyCap, "CAMERA/Ls1/Link missing or not 1/7/1 actions");
            }
            if (i == 1) {
                aryll = actor;
            }
        }
        const int last = pc_rando_evl_actor_action(&evl, aryll, 6);
        if ((int32_t)be32(pc_rando_evl_event_p(&evl, ev) + 0x88) != pc_rando_evl_action_flag(&evl, last)) {
            return fail(why, whyCap, "ending flag 0 is not Ls1's last action's flag");
        }
        return 1;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<uint8_t*>(data), size) != PCR_DZX_OK) {
        return fail(why, whyCap, "not a dzx");
    }
    switch (site) {
    case PCR_PS_WINDFALL: {
        const int ship = firstNamed(&dzx, "ACT2", "Pirates");
        if (ship < 0) {
            return fail(why, whyCap, "no Pirates on ACT2");
        }
        uint8_t want[kActrSize];
        memcpy(want, pc_rando_dzx_entry(&dzx, "ACT2", (uint32_t)ship), kActrSize);
        put32(want + 0x08, be32(want + 0x08) & ~kShipDoorType);
        const uint32_t n = pc_rando_dzx_count(&dzx, "ACTR");
        for (uint32_t i = 0; i < n; i++) {
            if (memcmp(pc_rando_dzx_entry(&dzx, "ACTR", i), want, kActrSize) == 0) {
                return 1;
            }
        }
        return fail(why, whyCap, "no copy of ACT2's Pirates with door type 0 on ACTR");
    }
    case PCR_PS_SHIP_ROOM: {
        for (const char* layer : kLayers) {
            if (firstNamed(&dzx, layer, "P2b") >= 0) {
                return fail(why, whyCap, "P2b still on ACT2/ACT3");
            }
            const int ashut = firstNamed(&dzx, layer, "Ashut");
            if (ashut < 0 || (be32(pc_rando_dzx_entry(&dzx, layer, (uint32_t)ashut) + 8) & kAshutSwitch) != PCR_PS_SW_DOOR_OPEN) {
                return fail(why, whyCap, "Ashut missing or not on switch 0xC4");
            }
        }
        const uint32_t n = pc_rando_dzx_count(&dzx, "ACTR");
        if (n < 4) {
            return fail(why, whyCap, "ACTR too short");
        }
        uint8_t want[kScobSize];
        pc_rando_tweak_pirate_ship_aryll(want);
        if (memcmp(pc_rando_dzx_entry(&dzx, "ACTR", n - 4), want, kActrSize) != 0) {
            return fail(why, whyCap, "ACTR[-4] is not Aryll");
        }
        for (int which = 0; which < 3; which++) {
            pc_rando_tweak_pirate_ship_swop(which, pc_rando_tweak_pirate_ship_evnt_index(), want);
            if (memcmp(pc_rando_dzx_entry(&dzx, "ACTR", n - 3 + which), want, kActrSize) != 0) {
                return fail(why, whyCap, "ACTR[-3..-1] are not the 3 SwOps");
            }
        }
        const uint32_t ns = pc_rando_dzx_count(&dzx, "SCOB");
        pc_rando_tweak_pirate_ship_swc00(want);
        if (ns == 0 || memcmp(pc_rando_dzx_entry(&dzx, "SCOB", ns - 1), want, kScobSize) != 0) {
            return fail(why, whyCap, "SCOB[-1] is not SW_C00");
        }
        return 1;
    }
    case PCR_PS_SHIP_STAGE: {
        const int idx = pc_rando_tweak_pirate_ship_evnt_index();
        uint8_t want[kEvntSize];
        pc_rando_tweak_pirate_ship_evnt(want);
        const uint8_t* e = (uint32_t)idx < pc_rando_dzx_count(&dzx, "EVNT") ? pc_rando_dzx_entry(&dzx, "EVNT", (uint32_t)idx)
                                                                               : nullptr;
        if (e == nullptr || memcmp(e, want, kEvntSize) != 0) {
            return fail(why, whyCap, "EVNT[evnt index] is not AryllOpensDoor");
        }
        return 1;
    }
    default:
        return fail(why, whyCap, "bad site");
    }
}

int pc_rando_tweak_pirate_ship_aryll_part_count(void) {
    return 2;
}

const char* pc_rando_tweak_pirate_ship_aryll_part(int i) {
    return i >= 0 && i < 2 ? kAryllParts[i] : nullptr;
}

const char* pc_rando_tweak_pirate_ship_new_text(uint16_t msg_no) {
    switch (msg_no) {
    case PCR_PS_MSG_STUCK:
        return "Oh! Did you get stuck in there, Big Brother?";
    case PCR_PS_MSG_OPEN:
        return "Don't worry, I'll open the door for you.";
    default:
        return nullptr;
    }
}

} // extern "C"

int pc_rando_tweak_pirate_ship_messages(const pcr::GlyphWidths& widths, const PcRandoMsgEntry& vanilla3008) {
    int set = 0;
    // msg.initial_sound = 106; msg.construct_string_from_parts(self.bfn, [...]) (the message's own box).
    PcRandoMsgEntry e = vanilla3008;
    e.initial_sound = PCR_PS_ARYLL_SOUND;
    const std::vector<std::string> parts(kAryllParts, kAryllParts + 2);
    const std::string text = pcr::constructStringFromParts(widths, e.text_box_type, parts);
    int r = pc_rando_msg_add(PCR_PS_MSG_ARYLL, &e, text.c_str());
    set += r == PCR_MSG_OK;
    if (r != PCR_MSG_OK) {
        logf("[cos] rando: pirate ship: message %d not set (%d)\n", PCR_PS_MSG_ARYLL, r);
    }
    // bmg.add_new_message: TextBoxType.DIALOG, initial_draw_type 0, text_alignment 4, msg.string.
    for (uint16_t n : {(uint16_t)PCR_PS_MSG_STUCK, (uint16_t)PCR_PS_MSG_OPEN}) {
        PcRandoMsgEntry m;
        pc_rando_msg_entry_defaults(&m);
        m.text_box_type = 0;
        m.initial_draw_type = 0;
        m.text_alignment = 4;
        r = pc_rando_msg_add(n, &m, pc_rando_tweak_pirate_ship_new_text(n));
        set += r == PCR_MSG_OK;
        if (r != PCR_MSG_OK) {
            logf("[cos] rando: pirate ship: message %d not set (%d)\n", n, r);
        }
    }
    return set;
}
