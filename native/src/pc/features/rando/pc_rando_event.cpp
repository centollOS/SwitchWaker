// pc_rando_event.cpp - in-place patch library for event_list.dat (docs/RANDOMIZER_PLAN.md, R0.11).
// See pc_rando_event.h. Pure: no game headers (cos_rando_core links it into the unit tests).
#include "pc_rando_event.h"

#include <string.h>

namespace {

constexpr uint32_t kHeaderSize = 0x40;
constexpr uint32_t kEventSize = 0xB0;
constexpr uint32_t kActorSize = 0x50;
constexpr uint32_t kActionSize = 0x50;
constexpr uint32_t kPropSize = 0x40;

// event
constexpr uint32_t kEvActors = 0x2C;
constexpr uint32_t kEvStartFlags = 0x80;
constexpr uint32_t kEvEndFlags = 0x88;
// actor
constexpr uint32_t kAcFlag = 0x28;
constexpr uint32_t kAcInitial = 0x30;
// action
constexpr uint32_t kActStartFlags = 0x28;
constexpr uint32_t kActFlag = 0x34;
constexpr uint32_t kActFirstProp = 0x38;
constexpr uint32_t kActNext = 0x3C;
// property
constexpr uint32_t kPrType = 0x24;
constexpr uint32_t kPrIndex = 0x28;
constexpr uint32_t kPrSize = 0x2C;
constexpr uint32_t kPrNext = 0x30;

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

int32_t bes32(const uint8_t* p) {
    return (int32_t)be32(p);
}

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

bool listFits(size_t size, uint32_t top, uint32_t num, uint32_t recSize) {
    if (num == 0) {
        return true;
    }
    if (num > 0x7FFFFFFFu / recSize) {
        return false;
    }
    return (uint64_t)top + (uint64_t)num * recSize <= size;
}

// Compares a 32-byte name field with a C string (the string must fit with its NUL).
bool nameIs(const uint8_t* rec, const char* name) {
    if (name == nullptr) {
        return false;
    }
    const size_t n = strlen(name);
    if (n >= PCR_EVL_NAME_MAX) {
        return false;
    }
    return memcmp(rec, name, n) == 0 && rec[n] == 0;
}

// The element slots [first, first + count) a float/int property uses in its list, by type: int
// properties use the integer list, float and vec3 properties the float list (vec3: 3 per element).
bool propRange(const PcRandoEvl* evl, const uint8_t* p, int* list, uint32_t* first, uint32_t* count) {
    const uint32_t type = be32(p + kPrType);
    const uint32_t idx = be32(p + kPrIndex);
    const uint32_t size = be32(p + kPrSize);
    uint64_t n;
    if (type == PCR_EVL_TYPE_INT) {
        *list = 1;
        n = size;
        if ((uint64_t)idx + n > evl->intNum) {
            return false;
        }
    } else if (type == PCR_EVL_TYPE_FLOAT || type == PCR_EVL_TYPE_VEC) {
        *list = 0;
        n = type == PCR_EVL_TYPE_VEC ? (uint64_t)size * 3 : size;
        if ((uint64_t)idx + n > evl->floatNum) {
            return false;
        }
    } else {
        return false;
    }
    *first = idx;
    *count = (uint32_t)n;
    return true;
}

// Whether any property other than `prop` uses a slot of [first, first + count) of the same list.
bool rangeShared(const PcRandoEvl* evl, int prop, int list, uint32_t first, uint32_t count) {
    for (uint32_t i = 0; i < evl->propNum; i++) {
        if ((int)i == prop) {
            continue;
        }
        const uint8_t* q = evl->data + evl->propTop + i * kPropSize;
        int l;
        uint32_t f, c;
        if (!propRange(evl, q, &l, &f, &c) || l != list || c == 0) {
            continue;
        }
        if (f < first + count && first < f + c) {
            return true;
        }
    }
    return false;
}

// The actor's chain into out (global action indexes, at most cap); returns the count, or -1 when the
// chain loops or leaves the action list.
int chainOf(const PcRandoEvl* evl, int actor, int* out, int cap) {
    const uint8_t* a = pc_rando_evl_actor_p(evl, actor);
    if (a == nullptr) {
        return -1;
    }
    int32_t act = bes32(a + kAcInitial);
    int n = 0;
    while (act != -1) {
        if (act < 0 || (uint32_t)act >= evl->actionNum || (uint32_t)n >= evl->actionNum) {
            return -1;
        }
        if (out != nullptr && n < cap) {
            out[n] = act;
        }
        n++;
        act = bes32(evl->data + evl->actionTop + (uint32_t)act * kActionSize + kActNext);
    }
    return n;
}

// The chain in a small fixed buffer for relinking (actors have far fewer actions in the game files).
constexpr int kChainMax = 256;

} // namespace

const char* pc_rando_evl_result_name(int result) {
    switch (result) {
    case PCR_EVL_OK: return "ok";
    case PCR_EVL_BAD_FILE: return "bad file";
    case PCR_EVL_SWAPPED: return "already swapped";
    case PCR_EVL_NO_EVENT: return "no event";
    case PCR_EVL_NO_ACTOR: return "no actor";
    case PCR_EVL_NO_ACTION: return "no action";
    case PCR_EVL_NO_PROPERTY: return "no property";
    case PCR_EVL_BAD_TYPE: return "bad type";
    case PCR_EVL_BAD_RANGE: return "bad range";
    case PCR_EVL_MISMATCH: return "mismatch";
    case PCR_EVL_SHARED: return "shared slots";
    case PCR_EVL_LOOP: return "broken chain";
    default: return "?";
    }
}

int pc_rando_evl_open(PcRandoEvl* evl, void* data, size_t size) {
    memset(evl, 0, sizeof *evl);
    if (data == nullptr || size < kHeaderSize) {
        return PCR_EVL_BAD_FILE;
    }
    const uint8_t* d = (const uint8_t*)data;
    PcRandoEvl v = {};
    v.data = (uint8_t*)data;
    v.size = size;
    v.eventTop = be32(d + 0x00);
    v.eventNum = be32(d + 0x04);
    v.actorTop = be32(d + 0x08);
    v.actorNum = be32(d + 0x0C);
    v.actionTop = be32(d + 0x10);
    v.actionNum = be32(d + 0x14);
    v.propTop = be32(d + 0x18);
    v.propNum = be32(d + 0x1C);
    v.floatTop = be32(d + 0x20);
    v.floatNum = be32(d + 0x24);
    v.intTop = be32(d + 0x28);
    v.intNum = be32(d + 0x2C);
    v.strTop = be32(d + 0x30);
    v.strSize = be32(d + 0x34);
    if (!listFits(size, v.eventTop, v.eventNum, kEventSize) || !listFits(size, v.actorTop, v.actorNum, kActorSize) ||
        !listFits(size, v.actionTop, v.actionNum, kActionSize) || !listFits(size, v.propTop, v.propNum, kPropSize) ||
        !listFits(size, v.floatTop, v.floatNum, 4) || !listFits(size, v.intTop, v.intNum, 4) ||
        !listFits(size, v.strTop, v.strSize, 1)) {
        return PCR_EVL_BAD_FILE;
    }
    // dEvent_manager_c::setData (TARGET_PC) marks a swapped buffer with header byte 0x38 = 1.
    if (d[0x38] != 0) {
        return PCR_EVL_SWAPPED;
    }
    *evl = v;
    return PCR_EVL_OK;
}

uint8_t* pc_rando_evl_event_p(const PcRandoEvl* evl, int event) {
    return event >= 0 && (uint32_t)event < evl->eventNum ? evl->data + evl->eventTop + (uint32_t)event * kEventSize
                                                          : nullptr;
}

uint8_t* pc_rando_evl_actor_p(const PcRandoEvl* evl, int actor) {
    return actor >= 0 && (uint32_t)actor < evl->actorNum ? evl->data + evl->actorTop + (uint32_t)actor * kActorSize
                                                          : nullptr;
}

uint8_t* pc_rando_evl_action_p(const PcRandoEvl* evl, int action) {
    return action >= 0 && (uint32_t)action < evl->actionNum
               ? evl->data + evl->actionTop + (uint32_t)action * kActionSize
               : nullptr;
}

uint8_t* pc_rando_evl_property_p(const PcRandoEvl* evl, int prop) {
    return prop >= 0 && (uint32_t)prop < evl->propNum ? evl->data + evl->propTop + (uint32_t)prop * kPropSize
                                                       : nullptr;
}

void pc_rando_evl_name(const uint8_t* record, char out[PCR_EVL_NAME_MAX + 1]) {
    if (record == nullptr) {
        out[0] = 0;
        return;
    }
    memcpy(out, record, PCR_EVL_NAME_MAX);
    out[PCR_EVL_NAME_MAX] = 0;
}

int pc_rando_evl_find_event(const PcRandoEvl* evl, const char* name) {
    for (uint32_t i = 0; i < evl->eventNum; i++) {
        if (nameIs(evl->data + evl->eventTop + i * kEventSize, name)) {
            return (int)i;
        }
    }
    return -1;
}

int pc_rando_evl_event_actor(const PcRandoEvl* evl, int event, int slot) {
    const uint8_t* e = pc_rando_evl_event_p(evl, event);
    if (e == nullptr || slot < 0 || slot >= PCR_EVL_EVENT_ACTORS) {
        return -1;
    }
    // wwrando's reader stops at the first blank slot (later slots must be blank too).
    for (int i = 0; i <= slot; i++) {
        if (bes32(e + kEvActors + (uint32_t)i * 4) == -1) {
            return -1;
        }
    }
    const int32_t actor = bes32(e + kEvActors + (uint32_t)slot * 4);
    return actor >= 0 && (uint32_t)actor < evl->actorNum ? actor : -1;
}

int pc_rando_evl_find_actor(const PcRandoEvl* evl, int event, const char* name) {
    for (int slot = 0; slot < PCR_EVL_EVENT_ACTORS; slot++) {
        const int actor = pc_rando_evl_event_actor(evl, event, slot);
        if (actor < 0) {
            return -1;
        }
        if (nameIs(pc_rando_evl_actor_p(evl, actor), name)) {
            return slot;
        }
    }
    return -1;
}

int pc_rando_evl_action_count(const PcRandoEvl* evl, int actor) {
    return chainOf(evl, actor, nullptr, 0);
}

int pc_rando_evl_actor_action(const PcRandoEvl* evl, int actor, int ordinal) {
    const uint8_t* a = pc_rando_evl_actor_p(evl, actor);
    if (a == nullptr || ordinal < 0) {
        return -1;
    }
    int32_t act = bes32(a + kAcInitial);
    for (int i = 0;; i++) {
        if (act < 0 || (uint32_t)act >= evl->actionNum || (uint32_t)i >= evl->actionNum) {
            return -1;
        }
        if (i == ordinal) {
            return act;
        }
        act = bes32(evl->data + evl->actionTop + (uint32_t)act * kActionSize + kActNext);
    }
}

int pc_rando_evl_find_action(const PcRandoEvl* evl, int actor, const char* name, int nth) {
    const int n = pc_rando_evl_action_count(evl, actor);
    for (int i = 0; i < n; i++) {
        if (nameIs(pc_rando_evl_action_p(evl, pc_rando_evl_actor_action(evl, actor, i)), name) && nth-- == 0) {
            return i;
        }
    }
    return -1;
}

int pc_rando_evl_action_property_at(const PcRandoEvl* evl, int action, int ordinal) {
    const uint8_t* a = pc_rando_evl_action_p(evl, action);
    if (a == nullptr || ordinal < 0) {
        return -1;
    }
    int32_t prop = bes32(a + kActFirstProp);
    for (int i = 0;; i++) {
        if (prop < 0 || (uint32_t)prop >= evl->propNum || (uint32_t)i >= evl->propNum) {
            return -1;
        }
        if (i == ordinal) {
            return prop;
        }
        prop = bes32(evl->data + evl->propTop + (uint32_t)prop * kPropSize + kPrNext);
    }
}

int pc_rando_evl_action_property(const PcRandoEvl* evl, int action, const char* name) {
    for (int i = 0;; i++) {
        const int prop = pc_rando_evl_action_property_at(evl, action, i);
        if (prop < 0) {
            return -1;
        }
        if (nameIs(pc_rando_evl_property_p(evl, prop), name)) {
            return prop;
        }
    }
}

int32_t pc_rando_evl_actor_flag(const PcRandoEvl* evl, int actor) {
    const uint8_t* a = pc_rando_evl_actor_p(evl, actor);
    return a != nullptr ? bes32(a + kAcFlag) : -1;
}

int32_t pc_rando_evl_action_flag(const PcRandoEvl* evl, int action) {
    const uint8_t* a = pc_rando_evl_action_p(evl, action);
    return a != nullptr ? bes32(a + kActFlag) : -1;
}

int pc_rando_evl_property_type(const PcRandoEvl* evl, int prop) {
    const uint8_t* p = pc_rando_evl_property_p(evl, prop);
    return p != nullptr ? (int)be32(p + kPrType) : -1;
}

int pc_rando_evl_property_size(const PcRandoEvl* evl, int prop) {
    const uint8_t* p = pc_rando_evl_property_p(evl, prop);
    return p != nullptr ? (int)be32(p + kPrSize) : -1;
}

int pc_rando_evl_property_slot(const PcRandoEvl* evl, int prop) {
    const uint8_t* p = pc_rando_evl_property_p(evl, prop);
    return p != nullptr ? (int)be32(p + kPrIndex) : -1;
}

int pc_rando_evl_get_int(const PcRandoEvl* evl, int prop, int elem, int32_t* out) {
    const uint8_t* p = pc_rando_evl_property_p(evl, prop);
    if (p == nullptr) {
        return PCR_EVL_NO_PROPERTY;
    }
    int list;
    uint32_t first, count;
    if (be32(p + kPrType) != PCR_EVL_TYPE_INT) {
        return PCR_EVL_BAD_TYPE;
    }
    if (!propRange(evl, p, &list, &first, &count) || elem < 0 || (uint32_t)elem >= count) {
        return PCR_EVL_BAD_RANGE;
    }
    *out = bes32(evl->data + evl->intTop + (first + (uint32_t)elem) * 4);
    return PCR_EVL_OK;
}

int pc_rando_evl_get_float(const PcRandoEvl* evl, int prop, int elem, float* out) {
    const uint8_t* p = pc_rando_evl_property_p(evl, prop);
    if (p == nullptr) {
        return PCR_EVL_NO_PROPERTY;
    }
    const uint32_t type = be32(p + kPrType);
    if (type != PCR_EVL_TYPE_FLOAT && type != PCR_EVL_TYPE_VEC) {
        return PCR_EVL_BAD_TYPE;
    }
    int list;
    uint32_t first, count;
    if (!propRange(evl, p, &list, &first, &count) || elem < 0 || (uint32_t)elem >= count) {
        return PCR_EVL_BAD_RANGE;
    }
    const uint32_t bits = be32(evl->data + evl->floatTop + (first + (uint32_t)elem) * 4);
    memcpy(out, &bits, 4);
    return PCR_EVL_OK;
}

const char* pc_rando_evl_get_string(const PcRandoEvl* evl, int prop) {
    const uint8_t* p = pc_rando_evl_property_p(evl, prop);
    if (p == nullptr || be32(p + kPrType) != PCR_EVL_TYPE_STRING) {
        return nullptr;
    }
    const uint32_t off = be32(p + kPrIndex);
    if (off >= evl->strSize) {
        return nullptr;
    }
    const char* s = (const char*)evl->data + evl->strTop + off;
    if (memchr(s, 0, evl->strSize - off) == nullptr) {
        return nullptr;
    }
    return s;
}

int pc_rando_evl_rename_action(PcRandoEvl* evl, int action, const char* name, const char* expectOld) {
    uint8_t* a = pc_rando_evl_action_p(evl, action);
    if (a == nullptr) {
        return PCR_EVL_NO_ACTION;
    }
    if (name == nullptr || strlen(name) >= PCR_EVL_NAME_MAX) {
        return PCR_EVL_BAD_RANGE;
    }
    if (expectOld != nullptr && !nameIs(a, expectOld)) {
        return PCR_EVL_MISMATCH;
    }
    // wwrando's fs.write_str pads the field with zeros.
    memset(a, 0, PCR_EVL_NAME_MAX);
    memcpy(a, name, strlen(name));
    return PCR_EVL_OK;
}

int pc_rando_evl_set_ints(PcRandoEvl* evl, int prop, const int32_t* values, int n, const int32_t* expectOld) {
    uint8_t* p = pc_rando_evl_property_p(evl, prop);
    if (p == nullptr) {
        return PCR_EVL_NO_PROPERTY;
    }
    if (be32(p + kPrType) != PCR_EVL_TYPE_INT) {
        return PCR_EVL_BAD_TYPE;
    }
    int list;
    uint32_t first, count;
    if (values == nullptr || !propRange(evl, p, &list, &first, &count) || n < 1 || (uint32_t)n > count) {
        return PCR_EVL_BAD_RANGE;
    }
    uint8_t* slots = evl->data + evl->intTop + first * 4;
    if (expectOld != nullptr) {
        for (int i = 0; i < n; i++) {
            if (bes32(slots + (uint32_t)i * 4) != expectOld[i]) {
                return PCR_EVL_MISMATCH;
            }
        }
    }
    if (rangeShared(evl, prop, list, first, (uint32_t)n)) {
        return PCR_EVL_SHARED;
    }
    for (int i = 0; i < n; i++) {
        put32(slots + (uint32_t)i * 4, (uint32_t)values[i]);
    }
    put32(p + kPrSize, (uint32_t)n);
    return PCR_EVL_OK;
}

int pc_rando_evl_set_int(PcRandoEvl* evl, int prop, int elem, int32_t value, const int32_t* expectOld) {
    uint8_t* p = pc_rando_evl_property_p(evl, prop);
    if (p == nullptr) {
        return PCR_EVL_NO_PROPERTY;
    }
    if (be32(p + kPrType) != PCR_EVL_TYPE_INT) {
        return PCR_EVL_BAD_TYPE;
    }
    int list;
    uint32_t first, count;
    if (!propRange(evl, p, &list, &first, &count) || elem < 0 || (uint32_t)elem >= count) {
        return PCR_EVL_BAD_RANGE;
    }
    uint8_t* slot = evl->data + evl->intTop + (first + (uint32_t)elem) * 4;
    if (expectOld != nullptr && bes32(slot) != *expectOld) {
        return PCR_EVL_MISMATCH;
    }
    if (rangeShared(evl, prop, list, first + (uint32_t)elem, 1)) {
        return PCR_EVL_SHARED;
    }
    put32(slot, (uint32_t)value);
    return PCR_EVL_OK;
}

int pc_rando_evl_set_float(PcRandoEvl* evl, int prop, int elem, float value, const float* expectOld) {
    uint8_t* p = pc_rando_evl_property_p(evl, prop);
    if (p == nullptr) {
        return PCR_EVL_NO_PROPERTY;
    }
    const uint32_t type = be32(p + kPrType);
    if (type != PCR_EVL_TYPE_FLOAT && type != PCR_EVL_TYPE_VEC) {
        return PCR_EVL_BAD_TYPE;
    }
    int list;
    uint32_t first, count;
    if (!propRange(evl, p, &list, &first, &count) || elem < 0 || (uint32_t)elem >= count) {
        return PCR_EVL_BAD_RANGE;
    }
    uint8_t* slot = evl->data + evl->floatTop + (first + (uint32_t)elem) * 4;
    if (expectOld != nullptr) {
        uint32_t want;
        memcpy(&want, expectOld, 4);
        if (be32(slot) != want) {
            return PCR_EVL_MISMATCH;
        }
    }
    if (rangeShared(evl, prop, list, first + (uint32_t)elem, 1)) {
        return PCR_EVL_SHARED;
    }
    uint32_t bits;
    memcpy(&bits, &value, 4);
    put32(slot, bits);
    return PCR_EVL_OK;
}

int pc_rando_evl_set_chain(PcRandoEvl* evl, int actor, const int* order, int n) {
    uint8_t* a = pc_rando_evl_actor_p(evl, actor);
    if (a == nullptr) {
        return PCR_EVL_NO_ACTOR;
    }
    int chain[kChainMax];
    const int count = chainOf(evl, actor, chain, kChainMax);
    if (count < 0) {
        return PCR_EVL_LOOP;
    }
    if (count > kChainMax) {
        return PCR_EVL_BAD_RANGE;
    }
    if (order == nullptr || n < 1 || n > count) {
        return PCR_EVL_BAD_RANGE;
    }
    bool used[kChainMax] = {};
    for (int i = 0; i < n; i++) {
        if (order[i] < 0 || order[i] >= count) {
            return PCR_EVL_NO_ACTION;
        }
        if (used[order[i]]) {
            return PCR_EVL_BAD_RANGE;
        }
        used[order[i]] = true;
    }
    put32(a + kAcInitial, (uint32_t)chain[order[0]]);
    for (int i = 0; i < n; i++) {
        uint8_t* act = evl->data + evl->actionTop + (uint32_t)chain[order[i]] * kActionSize;
        put32(act + kActNext, i + 1 < n ? (uint32_t)chain[order[i + 1]] : 0xFFFFFFFFu);
    }
    return PCR_EVL_OK;
}

int pc_rando_evl_truncate_actions(PcRandoEvl* evl, int actor, int keep) {
    int order[kChainMax];
    if (keep < 1 || keep > kChainMax) {
        return PCR_EVL_BAD_RANGE;
    }
    for (int i = 0; i < keep; i++) {
        order[i] = i;
    }
    return pc_rando_evl_set_chain(evl, actor, order, keep);
}

int pc_rando_evl_remove_action(PcRandoEvl* evl, int actor, int ordinal) {
    const int count = pc_rando_evl_action_count(evl, actor);
    if (count < 0) {
        return pc_rando_evl_actor_p(evl, actor) == nullptr ? PCR_EVL_NO_ACTOR : PCR_EVL_LOOP;
    }
    if (ordinal < 0 || ordinal >= count) {
        return PCR_EVL_NO_ACTION;
    }
    if (count > kChainMax || count < 2) {
        return PCR_EVL_BAD_RANGE; // an actor keeps at least one action (wwrando refuses to save none)
    }
    int order[kChainMax];
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (i != ordinal) {
            order[n++] = i;
        }
    }
    return pc_rando_evl_set_chain(evl, actor, order, n);
}

int pc_rando_evl_keep_last_actions(PcRandoEvl* evl, int actor, int keep) {
    const int count = pc_rando_evl_action_count(evl, actor);
    if (count < 0) {
        return pc_rando_evl_actor_p(evl, actor) == nullptr ? PCR_EVL_NO_ACTOR : PCR_EVL_LOOP;
    }
    if (keep < 1 || keep > count || count > kChainMax) {
        return PCR_EVL_BAD_RANGE;
    }
    int order[kChainMax];
    for (int i = 0; i < keep; i++) {
        order[i] = count - keep + i;
    }
    return pc_rando_evl_set_chain(evl, actor, order, keep);
}

int pc_rando_evl_set_starting_flag(PcRandoEvl* evl, int event, int i, int32_t flag) {
    uint8_t* e = pc_rando_evl_event_p(evl, event);
    if (e == nullptr) {
        return PCR_EVL_NO_EVENT;
    }
    if (i < 0 || i >= 2) {
        return PCR_EVL_BAD_RANGE;
    }
    put32(e + kEvStartFlags + (uint32_t)i * 4, (uint32_t)flag);
    return PCR_EVL_OK;
}

int pc_rando_evl_set_ending_flag(PcRandoEvl* evl, int event, int i, int32_t flag) {
    uint8_t* e = pc_rando_evl_event_p(evl, event);
    if (e == nullptr) {
        return PCR_EVL_NO_EVENT;
    }
    if (i < 0 || i >= 3) {
        return PCR_EVL_BAD_RANGE;
    }
    put32(e + kEvEndFlags + (uint32_t)i * 4, (uint32_t)flag);
    return PCR_EVL_OK;
}

int pc_rando_evl_set_action_starting_flag(PcRandoEvl* evl, int action, int i, int32_t flag) {
    uint8_t* a = pc_rando_evl_action_p(evl, action);
    if (a == nullptr) {
        return PCR_EVL_NO_ACTION;
    }
    if (i < 0 || i >= 3) {
        return PCR_EVL_BAD_RANGE;
    }
    put32(a + kActStartFlags + (uint32_t)i * 4, (uint32_t)flag);
    return PCR_EVL_OK;
}

int pc_rando_evl_change_item(PcRandoEvl* evl, const char* eventName, int actorSlot, int actionOrdinal,
                             uint8_t item, const char* expectName, int32_t expectPrm0, int* outAction,
                             int* outSlot) {
    const int event = pc_rando_evl_find_event(evl, eventName);
    if (event < 0) {
        return PCR_EVL_NO_EVENT;
    }
    const int actor = pc_rando_evl_event_actor(evl, event, actorSlot);
    if (actor < 0) {
        return PCR_EVL_NO_ACTOR;
    }
    const int action = pc_rando_evl_actor_action(evl, actor, actionOrdinal);
    if (action < 0) {
        return PCR_EVL_NO_ACTION;
    }
    const int prop = pc_rando_evl_action_property_at(evl, action, 0);
    if (prop < 0) {
        return PCR_EVL_NO_PROPERTY;
    }
    if (pc_rando_evl_property_type(evl, prop) != PCR_EVL_TYPE_INT) {
        return PCR_EVL_BAD_TYPE;
    }
    if (expectName != nullptr) {
        int32_t prm0;
        if (!nameIs(pc_rando_evl_action_p(evl, action), expectName) ||
            pc_rando_evl_get_int(evl, prop, 0, &prm0) != PCR_EVL_OK || prm0 != expectPrm0) {
            return PCR_EVL_MISMATCH;
        }
    }
    const bool song = item >= PCR_EVL_SONG_FIRST && item <= PCR_EVL_SONG_LAST;
    const int32_t value = song ? (int32_t)(item - PCR_EVL_SONG_FIRST) : (int32_t)item;
    // Value first: it is the step that can still be refused (shared slots); the rename cannot fail.
    const int r = pc_rando_evl_set_ints(evl, prop, &value, 1, nullptr);
    if (r != PCR_EVL_OK) {
        return r;
    }
    pc_rando_evl_rename_action(evl, action, song ? "059get_dance" : "011get_item", nullptr);
    if (outAction != nullptr) {
        *outAction = action;
    }
    if (outSlot != nullptr) {
        *outSlot = pc_rando_evl_property_slot(evl, prop);
    }
    return PCR_EVL_OK;
}
