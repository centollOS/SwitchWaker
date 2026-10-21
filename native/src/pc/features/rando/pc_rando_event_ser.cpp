// pc_rando_event_ser.cpp - event_list.dat serializer (docs/RANDOMIZER_PLAN.md step R3.14), a port of
// wwrando's wwlib/events.py. See pc_rando_event_ser.h.
#include "pc_rando_event_ser.h"

#include <string.h>

namespace pcr {

namespace {

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

bool fits(size_t size, uint32_t off, uint64_t len) {
    return (uint64_t)off + len <= size;
}

uint32_t floatBits(float f) {
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

} // namespace

// --- EvsName ---

bool EvsName::set(const char* name) {
    const size_t n = name != nullptr ? strlen(name) : 0;
    if (name == nullptr || n >= (size_t)kEvsNameSize) {
        return false;
    }
    memset(raw, 0, sizeof raw);
    memcpy(raw, name, n);
    return true;
}

EvsString EvsName::str() const {
    size_t n = 0;
    while (n < (size_t)kEvsNameSize && raw[n] != 0) {
        n++;
    }
    return EvsString((const char*)raw, n);
}

bool EvsName::is(const char* name) const {
    EvsName other;
    return other.set(name) && memcmp(raw, other.raw, sizeof raw) == 0;
}

// --- EvsProperty ---

void EvsProperty::setInts(const int32_t* v, int n) {
    type = PCR_EVL_TYPE_INT;
    words.assign(v, v + (n > 0 ? n : 0));
    str.clear();
}

void EvsProperty::setFloats(const float* v, int n) {
    type = PCR_EVL_TYPE_FLOAT;
    words.clear();
    for (int i = 0; i < n; i++) {
        words.push_back(floatBits(v[i]));
    }
    str.clear();
}

void EvsProperty::setVecs(const float* xyz, int nVecs) {
    setFloats(xyz, nVecs > 0 ? nVecs * 3 : 0);
    type = PCR_EVL_TYPE_VEC;
}

void EvsProperty::setString(const char* s) {
    type = PCR_EVL_TYPE_STRING;
    words.clear();
    str = s != nullptr ? s : "";
}

int EvsProperty::count() const {
    if (type == PCR_EVL_TYPE_VEC) {
        return (int)(words.size() / 3);
    }
    return (int)words.size();
}

// --- EvsList: read ---

void EvsList::clear() {
    events.clear();
    actors.clear();
    actions.clear();
    props.clear();
    memset(headerPadding, 0, sizeof headerPadding);
    mFlagUsed.assign(kEvsTotalFlags, 0);
    mFlagCursor = 0;
}

int EvsList::read(const void* data, size_t size) {
    clear();
    const uint8_t* d = (const uint8_t*)data;
    if (d == nullptr || size < 0x40) {
        return PCR_EVL_BAD_FILE;
    }
    if (d[0x38] != 0) {
        return PCR_EVL_SWAPPED; // dEvent_manager_c::setData (TARGET_PC) marks a swapped buffer
    }
    const uint32_t evTop = rd32(d + 0x00), evNum = rd32(d + 0x04);
    const uint32_t acTop = rd32(d + 0x08), acNum = rd32(d + 0x0C);
    const uint32_t ctTop = rd32(d + 0x10), ctNum = rd32(d + 0x14);
    const uint32_t prTop = rd32(d + 0x18), prNum = rd32(d + 0x1C);
    const uint32_t flTop = rd32(d + 0x20), flNum = rd32(d + 0x24);
    const uint32_t inTop = rd32(d + 0x28), inNum = rd32(d + 0x2C);
    const uint32_t stTop = rd32(d + 0x30), stSize = rd32(d + 0x34);
    if (!fits(size, evTop, (uint64_t)evNum * kEvsEventSize) || !fits(size, acTop, (uint64_t)acNum * kEvsActorSize) ||
        !fits(size, ctTop, (uint64_t)ctNum * kEvsActionSize) || !fits(size, prTop, (uint64_t)prNum * kEvsPropSize) ||
        !fits(size, flTop, (uint64_t)flNum * 4) || !fits(size, inTop, (uint64_t)inNum * 4) ||
        !fits(size, stTop, stSize)) {
        return PCR_EVL_BAD_FILE;
    }
    memcpy(headerPadding, d + 0x38, 8);
    int err = PCR_EVL_OK;

    events.resize(evNum);
    for (uint32_t i = 0; i < evNum && err == PCR_EVL_OK; i++) {
        const uint8_t* p = d + evTop + i * kEvsEventSize;
        EvsEvent& e = events[i];
        memcpy(e.name.raw, p, kEvsNameSize);
        e.index = (int32_t)rd32(p + 0x20);
        e.unknown1 = rd32(p + 0x24);
        e.priority = rd32(p + 0x28);
        for (int k = 0; k < kEvsEventActors; k++) {
            e.actorIndexes[k] = (int32_t)rd32(p + 0x2C + k * 4);
        }
        e.numActors = rd32(p + 0x7C);
        e.startingFlags[0] = (int32_t)rd32(p + 0x80);
        e.startingFlags[1] = (int32_t)rd32(p + 0x84);
        for (int k = 0; k < 3; k++) {
            e.endingFlags[k] = (int32_t)rd32(p + 0x88 + k * 4);
        }
        e.playJingle = p[0x94] != 0 ? 1 : 0; // bool(read_u8): written back as 0/1
        memcpy(e.runtime, p + 0x95, sizeof e.runtime);
        for (uint32_t j = 0; j < i; j++) {
            if (memcmp(events[j].name.raw, e.name.raw, kEvsNameSize) == 0) {
                err = PCR_EVL_BAD_FILE; // "Duplicate event name"
            }
        }
    }

    actors.resize(acNum);
    for (uint32_t i = 0; i < acNum; i++) {
        const uint8_t* p = d + acTop + i * kEvsActorSize;
        EvsActor& a = actors[i];
        memcpy(a.name.raw, p, kEvsNameSize);
        a.staffId = rd32(p + 0x20);
        a.index = (int32_t)rd32(p + 0x24);
        a.flag = (int32_t)rd32(p + 0x28);
        a.staffType = rd32(p + 0x2C);
        a.initialActionIndex = (int32_t)rd32(p + 0x30);
        memcpy(a.runtime, p + 0x34, sizeof a.runtime);
    }

    actions.resize(ctNum);
    for (uint32_t i = 0; i < ctNum; i++) {
        const uint8_t* p = d + ctTop + i * kEvsActionSize;
        EvsAction& c = actions[i];
        memcpy(c.name.raw, p, kEvsNameSize);
        c.duplicateId = rd32(p + 0x20);
        c.index = (int32_t)rd32(p + 0x24);
        for (int k = 0; k < 3; k++) {
            c.startingFlags[k] = (int32_t)rd32(p + 0x28 + k * 4);
        }
        c.flag = (int32_t)rd32(p + 0x34);
        c.firstPropIndex = (int32_t)rd32(p + 0x38);
        c.nextIndex = (int32_t)rd32(p + 0x3C);
        memcpy(c.runtime, p + 0x40, sizeof c.runtime);
    }

    // Each event's actors: blank (-1) slots only at the end.
    for (uint32_t i = 0; i < evNum && err == PCR_EVL_OK; i++) {
        EvsEvent& e = events[i];
        bool blank = false;
        for (int k = 0; k < kEvsEventActors; k++) {
            const int32_t idx = e.actorIndexes[k];
            if (idx == -1) {
                blank = true;
            } else if (blank || idx < 0 || (uint32_t)idx >= acNum) {
                err = PCR_EVL_BAD_FILE;
                break;
            } else {
                e.actors.push_back(idx);
            }
        }
    }

    // Each actor's actions (every actor of the list, as Python does).
    for (uint32_t i = 0; i < acNum && err == PCR_EVL_OK; i++) {
        EvsActor& a = actors[i];
        int32_t idx = a.initialActionIndex;
        while (true) {
            if (idx < 0 || (uint32_t)idx >= ctNum) {
                err = PCR_EVL_LOOP;
                break;
            }
            if (a.actions.size() > ctNum) {
                err = PCR_EVL_LOOP;
                break;
            }
            a.actions.push_back(idx);
            if (actions[idx].nextIndex == -1) {
                break;
            }
            idx = actions[idx].nextIndex;
        }
    }

    props.resize(prNum);
    for (uint32_t i = 0; i < prNum; i++) {
        const uint8_t* p = d + prTop + i * kEvsPropSize;
        EvsProperty& q = props[i];
        memcpy(q.name.raw, p, kEvsNameSize);
        q.index = (int32_t)rd32(p + 0x20);
        q.type = rd32(p + 0x24);
        q.dataIndex = rd32(p + 0x28);
        q.dataSize = rd32(p + 0x2C);
        q.nextIndex = (int32_t)rd32(p + 0x30);
        memcpy(q.runtime, p + 0x34, sizeof q.runtime);
    }

    // Each action's properties.
    for (uint32_t i = 0; i < ctNum && err == PCR_EVL_OK; i++) {
        EvsAction& c = actions[i];
        if (c.firstPropIndex == -1) {
            continue;
        }
        int32_t idx = c.firstPropIndex;
        while (true) {
            if (idx < 0 || (uint32_t)idx >= prNum || c.props.size() > prNum) {
                err = PCR_EVL_LOOP;
                break;
            }
            c.props.push_back(idx);
            if (props[idx].nextIndex == -1) {
                break;
            }
            idx = props[idx].nextIndex;
        }
    }

    // The string list: NUL-terminated strings, each padded with NULs to a multiple of 8 bytes. A
    // string property must point at the start of one (all_strings_by_offset).
    EvsVec<uint32_t> strStarts;
    if (err == PCR_EVL_OK) {
        uint32_t off = 0;
        while (off < stSize) {
            strStarts.push_back(off);
            uint32_t n = 0;
            while (stTop + off + n < size && d[stTop + off + n] != 0) {
                n++;
            }
            off += n + 1;
            const uint32_t withNul = n + 1;
            if (withNul % 8 != 0) {
                const uint32_t pad = 8 - withNul % 8;
                for (uint32_t k = 0; k < pad; k++) {
                    if (stTop + off + k >= size || d[stTop + off + k] != 0) {
                        err = PCR_EVL_BAD_FILE; // the assert on the padding bytes
                        break;
                    }
                }
                off += pad;
            }
            if (err != PCR_EVL_OK) {
                break;
            }
        }
    }

    // Each property's value.
    for (uint32_t i = 0; i < prNum && err == PCR_EVL_OK; i++) {
        EvsProperty& q = props[i];
        switch (q.type) {
        case PCR_EVL_TYPE_FLOAT:
        case PCR_EVL_TYPE_VEC: {
            const uint64_t n = (uint64_t)q.dataSize * (q.type == PCR_EVL_TYPE_VEC ? 3 : 1);
            if ((uint64_t)q.dataIndex + n > flNum) {
                err = PCR_EVL_BAD_FILE;
                break;
            }
            for (uint64_t k = 0; k < n; k++) {
                q.words.push_back(rd32(d + flTop + (q.dataIndex + k) * 4));
            }
            break;
        }
        case PCR_EVL_TYPE_INT: {
            if ((uint64_t)q.dataIndex + q.dataSize > inNum) {
                err = PCR_EVL_BAD_FILE;
                break;
            }
            for (uint32_t k = 0; k < q.dataSize; k++) {
                q.words.push_back(rd32(d + inTop + (q.dataIndex + k) * 4));
            }
            break;
        }
        case PCR_EVL_TYPE_STRING: {
            bool found = false;
            for (uint32_t s : strStarts) {
                if (s == q.dataIndex) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                err = PCR_EVL_BAD_FILE;
                break;
            }
            const uint8_t* s = d + stTop + q.dataIndex;
            size_t n = 0;
            while (stTop + q.dataIndex + n < size && s[n] != 0) {
                n++;
            }
            q.str.assign((const char*)s, n);
            break;
        }
        default:
            err = PCR_EVL_BAD_TYPE; // "Reading property data type %d not implemented"
            break;
        }
    }

    // unused_flag_ids: every flag minus those of the events' actors and their actions (list.remove
    // raises when a flag is out of range or used twice).
    for (uint32_t i = 0; i < evNum && err == PCR_EVL_OK; i++) {
        for (int a : events[i].actors) {
            const int32_t f = actors[a].flag;
            if (f < 0 || f >= kEvsTotalFlags || mFlagUsed[f]) {
                err = PCR_EVL_BAD_FILE;
                break;
            }
            mFlagUsed[f] = 1;
            for (int c : actors[a].actions) {
                const int32_t g = actions[c].flag;
                if (g < 0 || g >= kEvsTotalFlags || mFlagUsed[g]) {
                    err = PCR_EVL_BAD_FILE;
                    break;
                }
                mFlagUsed[g] = 1;
            }
            if (err != PCR_EVL_OK) {
                break;
            }
        }
    }

    if (err != PCR_EVL_OK) {
        clear();
    }
    return err;
}

// --- EvsList: serialize (save_changes) ---

int EvsList::serialize(EvsBytes& out) {
    out.clear();
    const int nActorsPool = (int)actors.size(), nActionsPool = (int)actions.size(), nPropsPool = (int)props.size();
    uint32_t offset = 0x40;

    EvsVec<int> allActors, allActions, allProps;
    EvsVec<uint32_t> allFloats, allInts;

    const uint32_t eventTop = offset;
    for (size_t i = 0; i < events.size(); i++) {
        EvsEvent& e = events[i];
        e.outOffset = offset;
        e.index = (int32_t)i;
        offset += kEvsEventSize;
        if (e.actors.size() > (size_t)kEvsEventActors) {
            return PCR_EVL_BAD_RANGE;
        }
        for (int a : e.actors) {
            if (a < 0 || a >= nActorsPool) {
                return PCR_EVL_BAD_RANGE;
            }
            allActors.push_back(a);
        }
    }

    const uint32_t actorTop = offset;
    for (size_t i = 0; i < allActors.size(); i++) {
        EvsActor& a = actors[allActors[i]];
        a.outOffset = offset;
        a.outIndex = (int32_t)i;
        offset += kEvsActorSize;
        if (a.actions.empty()) {
            return PCR_EVL_BAD_RANGE; // "Cannot save actor with no actions!"
        }
        for (size_t j = 0; j < a.actions.size(); j++) {
            const int c = a.actions[j];
            if (c < 0 || c >= nActionsPool) {
                return PCR_EVL_BAD_RANGE;
            }
            allActions.push_back(c);
            actions[c].nextRef = j + 1 == a.actions.size() ? -1 : a.actions[j + 1];
        }
    }

    const uint32_t actionTop = offset;
    for (size_t i = 0; i < allActions.size(); i++) {
        EvsAction& c = actions[allActions[i]];
        c.outOffset = offset;
        c.outIndex = (int32_t)i;
        offset += kEvsActionSize;
        for (size_t j = 0; j < c.props.size(); j++) {
            const int q = c.props[j];
            if (q < 0 || q >= nPropsPool) {
                return PCR_EVL_BAD_RANGE;
            }
            allProps.push_back(q);
            props[q].nextRef = j + 1 == c.props.size() ? -1 : c.props[j + 1];
        }
    }

    const uint32_t propTop = offset;
    for (size_t i = 0; i < allProps.size(); i++) {
        EvsProperty& q = props[allProps[i]];
        q.outOffset = offset;
        q.outIndex = (int32_t)i;
        offset += kEvsPropSize;
        if (q.type == PCR_EVL_TYPE_STRING) {
            continue; // offset and size set when the string is written
        }
        if (q.type != PCR_EVL_TYPE_INT && q.type != PCR_EVL_TYPE_FLOAT && q.type != PCR_EVL_TYPE_VEC) {
            return PCR_EVL_BAD_RANGE;
        }
        if (q.type == PCR_EVL_TYPE_VEC && q.words.size() % 3 != 0) {
            return PCR_EVL_BAD_RANGE;
        }
        q.dataSize = (uint32_t)q.count();
        if (q.words.empty()) {
            q.type = PCR_EVL_TYPE_INT; // "Default to int for empty properties"
        }
        if (q.type == PCR_EVL_TYPE_INT) {
            q.dataIndex = (uint32_t)allInts.size();
            allInts.insert(allInts.end(), q.words.begin(), q.words.end());
        } else {
            q.dataIndex = (uint32_t)allFloats.size();
            allFloats.insert(allFloats.end(), q.words.begin(), q.words.end());
        }
    }

    const uint32_t floatTop = offset;
    offset += (uint32_t)allFloats.size() * 4;
    const uint32_t intTop = offset;
    offset += (uint32_t)allInts.size() * 4;
    const uint32_t strTop = offset;
    for (int qi : allProps) {
        EvsProperty& q = props[qi];
        if (q.type != PCR_EVL_TYPE_STRING) {
            continue;
        }
        if (q.str.find('\0') != EvsString::npos) {
            return PCR_EVL_BAD_RANGE;
        }
        const uint32_t withNul = (uint32_t)q.str.size() + 1;
        const uint32_t padded = (withNul + 7) & ~7u;
        q.dataIndex = offset - strTop;
        q.dataSize = padded;
        offset += padded;
    }
    const uint32_t strSize = offset - strTop;

    // Python writes into a BytesIO cut to the header; the file ends with the string list.
    out.assign(offset, 0);
    uint8_t* d = out.data();

    for (size_t i = 0; i < allFloats.size(); i++) {
        wr32(d + floatTop + i * 4, allFloats[i]);
    }
    for (size_t i = 0; i < allInts.size(); i++) {
        wr32(d + intTop + i * 4, allInts[i]);
    }
    for (int qi : allProps) {
        const EvsProperty& q = props[qi];
        if (q.type == PCR_EVL_TYPE_STRING) {
            memcpy(d + strTop + q.dataIndex, q.str.data(), q.str.size());
        }
    }

    for (size_t i = 0; i < events.size(); i++) {
        EvsEvent& e = events[i];
        uint8_t* p = d + e.outOffset;
        memcpy(p, e.name.raw, kEvsNameSize);
        wr32(p + 0x20, (uint32_t)e.index);
        wr32(p + 0x24, e.unknown1);
        wr32(p + 0x28, e.priority);
        for (int k = 0; k < kEvsEventActors; k++) {
            e.actorIndexes[k] = (size_t)k < e.actors.size() ? actors[e.actors[k]].outIndex : -1;
            wr32(p + 0x2C + k * 4, (uint32_t)e.actorIndexes[k]);
        }
        e.numActors = (uint32_t)e.actors.size();
        wr32(p + 0x7C, e.numActors);
        wr32(p + 0x80, (uint32_t)e.startingFlags[0]);
        wr32(p + 0x84, (uint32_t)e.startingFlags[1]);
        for (int k = 0; k < 3; k++) {
            wr32(p + 0x88 + k * 4, (uint32_t)e.endingFlags[k]);
        }
        p[0x94] = e.playJingle != 0 ? 1 : 0;
        memcpy(p + 0x95, e.runtime, sizeof e.runtime);
    }
    for (int ai : allActors) {
        EvsActor& a = actors[ai];
        uint8_t* p = d + a.outOffset;
        memcpy(p, a.name.raw, kEvsNameSize);
        wr32(p + 0x20, a.staffId);
        a.index = a.outIndex;
        wr32(p + 0x24, (uint32_t)a.index);
        wr32(p + 0x28, (uint32_t)a.flag);
        wr32(p + 0x2C, a.staffType);
        a.initialActionIndex = actions[a.actions[0]].outIndex;
        wr32(p + 0x30, (uint32_t)a.initialActionIndex);
        memcpy(p + 0x34, a.runtime, sizeof a.runtime);
    }
    for (int ci : allActions) {
        EvsAction& c = actions[ci];
        uint8_t* p = d + c.outOffset;
        memcpy(p, c.name.raw, kEvsNameSize);
        wr32(p + 0x20, c.duplicateId);
        c.index = c.outIndex;
        wr32(p + 0x24, (uint32_t)c.index);
        for (int k = 0; k < 3; k++) {
            wr32(p + 0x28 + k * 4, (uint32_t)c.startingFlags[k]);
        }
        wr32(p + 0x34, (uint32_t)c.flag);
        c.firstPropIndex = c.props.empty() ? -1 : props[c.props[0]].outIndex;
        wr32(p + 0x38, (uint32_t)c.firstPropIndex);
        c.nextIndex = c.nextRef < 0 ? -1 : actions[c.nextRef].outIndex;
        wr32(p + 0x3C, (uint32_t)c.nextIndex);
        memcpy(p + 0x40, c.runtime, sizeof c.runtime);
    }
    for (int qi : allProps) {
        EvsProperty& q = props[qi];
        uint8_t* p = d + q.outOffset;
        memcpy(p, q.name.raw, kEvsNameSize);
        q.index = q.outIndex;
        wr32(p + 0x20, (uint32_t)q.index);
        wr32(p + 0x24, q.type);
        wr32(p + 0x28, q.dataIndex);
        wr32(p + 0x2C, q.dataSize);
        q.nextIndex = q.nextRef < 0 ? -1 : props[q.nextRef].outIndex;
        wr32(p + 0x30, (uint32_t)q.nextIndex);
        memcpy(p + 0x34, q.runtime, sizeof q.runtime);
    }

    wr32(d + 0x00, eventTop);
    wr32(d + 0x04, (uint32_t)events.size());
    wr32(d + 0x08, actorTop);
    wr32(d + 0x0C, (uint32_t)allActors.size());
    wr32(d + 0x10, actionTop);
    wr32(d + 0x14, (uint32_t)allActions.size());
    wr32(d + 0x18, propTop);
    wr32(d + 0x1C, (uint32_t)allProps.size());
    wr32(d + 0x20, floatTop);
    wr32(d + 0x24, (uint32_t)allFloats.size());
    wr32(d + 0x28, intTop);
    wr32(d + 0x2C, (uint32_t)allInts.size());
    wr32(d + 0x30, strTop);
    wr32(d + 0x34, strSize);
    memcpy(d + 0x38, headerPadding, 8);
    return PCR_EVL_OK;
}

// --- lookups ---

int EvsList::findEvent(const char* name) const {
    for (size_t i = 0; i < events.size(); i++) {
        if (events[i].name.is(name)) {
            return (int)i;
        }
    }
    return -1;
}

int EvsList::findActor(int event, const char* name) const {
    if (event < 0 || (size_t)event >= events.size()) {
        return -1;
    }
    for (int a : events[event].actors) {
        if (a >= 0 && (size_t)a < actors.size() && actors[a].name.is(name)) {
            return a;
        }
    }
    return -1;
}

int EvsList::findAction(int actor, const char* name, int nth) const {
    if (actor < 0 || (size_t)actor >= actors.size()) {
        return -1;
    }
    for (int c : actors[actor].actions) {
        if (c >= 0 && (size_t)c < actions.size() && actions[c].name.is(name) && nth-- == 0) {
            return c;
        }
    }
    return -1;
}

int EvsList::getProp(int action, const char* name) const {
    if (action < 0 || (size_t)action >= actions.size()) {
        return -1;
    }
    for (int q : actions[action].props) {
        if (q >= 0 && (size_t)q < props.size() && props[q].name.is(name)) {
            return q;
        }
    }
    return -1;
}

// --- additions ---

int32_t EvsList::unusedFlag() {
    if (mFlagUsed.size() != (size_t)kEvsTotalFlags) {
        mFlagUsed.assign(kEvsTotalFlags, 0);
    }
    while (mFlagCursor < kEvsTotalFlags && mFlagUsed[mFlagCursor]) {
        mFlagCursor++;
    }
    if (mFlagCursor >= kEvsTotalFlags) {
        return -1; // "No unused flags left"
    }
    mFlagUsed[mFlagCursor] = 1;
    return mFlagCursor++;
}

int EvsList::addEvent(const char* name) {
    EvsEvent e;
    if (!e.name.set(name) || findEvent(name) >= 0) {
        return -1;
    }
    events.push_back(e);
    return (int)events.size() - 1;
}

int EvsList::addActor(int event, const char* name) {
    EvsActor a;
    if (event < 0 || (size_t)event >= events.size() || !a.name.set(name)) {
        return -1;
    }
    a.flag = unusedFlag();
    if (a.flag < 0) {
        return -1;
    }
    actors.push_back(a);
    events[event].actors.push_back((int)actors.size() - 1);
    return (int)actors.size() - 1;
}

int EvsList::addAction(int actor, const char* name) {
    EvsAction c;
    if (actor < 0 || (size_t)actor >= actors.size() || !c.name.set(name)) {
        return -1;
    }
    c.flag = unusedFlag();
    if (c.flag < 0) {
        return -1;
    }
    actions.push_back(c);
    actors[actor].actions.push_back((int)actions.size() - 1);
    return (int)actions.size() - 1;
}

int EvsList::addProp(int action, const char* name) {
    EvsProperty q;
    if (action < 0 || (size_t)action >= actions.size() || !q.name.set(name)) {
        return -1;
    }
    props.push_back(q);
    actions[action].props.push_back((int)props.size() - 1);
    return (int)props.size() - 1;
}

int EvsList::addPropInts(int action, const char* name, const int32_t* v, int n) {
    const int q = addProp(action, name);
    if (q >= 0) {
        props[q].setInts(v, n);
    }
    return q;
}

int EvsList::addPropFloats(int action, const char* name, const float* v, int n) {
    const int q = addProp(action, name);
    if (q >= 0) {
        props[q].setFloats(v, n);
    }
    return q;
}

int EvsList::addPropVecs(int action, const char* name, const float* xyz, int nVecs) {
    const int q = addProp(action, name);
    if (q >= 0) {
        props[q].setVecs(xyz, nVecs);
    }
    return q;
}

int EvsList::addPropString(int action, const char* name, const char* s) {
    const int q = addProp(action, name);
    if (q >= 0) {
        props[q].setString(s);
    }
    return q;
}

// --- resource files ---

int pc_rando_evser_write_res(PcRandoResFile* file, EvsList& list) {
    EvsBytes bytes;
    const int r = list.serialize(bytes);
    if (r != PCR_EVL_OK) {
        return r;
    }
    if (bytes.size() <= file->size) {
        memcpy(file->data, bytes.data(), bytes.size());
        memset(file->data + bytes.size(), 0, file->size - bytes.size());
        return PCR_EVL_OK;
    }
    uint8_t* copy = pc_rando_res_replace(file, bytes.size());
    if (copy == nullptr) {
        return PCR_EVL_BAD_RANGE;
    }
    memcpy(copy, bytes.data(), bytes.size());
    return PCR_EVL_OK;
}

uint64_t pc_rando_evser_fnv64(const void* data, size_t size) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

} // namespace pcr
