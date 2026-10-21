// pc_rando_event_ser.h - event_list.dat serializer (docs/RANDOMIZER_PLAN.md, step R3.14): a port of
// wwrando's wwlib/events.py (EventList, Event, Actor, Action, Property: read, save_changes,
// add_event, add_actor, add_action, add_property, get_unused_flag_id).
//
// pc_rando_event.h (R0.11) edits a file in place and never grows it. This one reads the whole file
// into a model, lets a patcher add events, actors (staff), actions (cuts) and properties (data) or
// change anything, and writes the file again exactly as wwrando's save_changes does: the same
// layout, the same order (events; each event's actors; each actor's actions; each action's
// properties; floats, integers, then the 8-byte-padded strings), the indexes renumbered and the
// links (initial action, next action, first/next property, event actor slots) rebuilt from the
// lists. All 154 vanilla event_list.dat files (every stage's Stage.arc) come out byte-identical
// through wwrando, and through this port (smoke `evser`).
//
// Pure: no game headers or calls (cos_rando_core links it into the unit tests). The model lives in
// host memory (malloc, not the game's operator new), so a patcher running inside
// dRes_info_c::loadResource does not take memory from the archive heap except for the final file
// (pc_rando_evser_write_res).
//
// Records keep every field they were read with (the fixed-size 32-byte name as raw bytes, the
// runtime areas) and the lists are plain vectors of pool indexes, so a patcher edits them the way
// wwrando's tweaks do (actor.actions.insert(...), event.actors.remove(...), action.properties...).
// A record may be listed twice (Python objects can be); serialize then behaves as wwrando does
// (the last position wins for its index; its record is written there).
//
// Differences from Python, all only on files wwrando itself cannot read or write:
//   - Python raises; here read/serialize return a PCR_EVL_* error and write nothing;
//   - action/property chains that loop are refused (Python never ends), as are negative indexes
//     (Python would wrap them to the end of the list);
//   - string lengths are counted in bytes (Python counts shift_jis characters, which only differs
//     for non-ASCII strings; no vanilla string has one).
#pragma once

#include "pc_rando_event.h"
#include "pc_rando_resources.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <new>
#include <string>
#include <vector>

namespace pcr {

// malloc-backed allocator (the game's global operator new goes to the current JKRHeap).
template <class T> struct EvsAlloc {
    using value_type = T;
    EvsAlloc() noexcept = default;
    template <class U> EvsAlloc(const EvsAlloc<U>&) noexcept {}
    T* allocate(size_t n) {
        void* p = malloc(n * sizeof(T) > 0 ? n * sizeof(T) : 1);
        if (p == nullptr) {
            abort();
        }
        return static_cast<T*>(p);
    }
    void deallocate(T* p, size_t) noexcept { free(p); }
    template <class U> bool operator==(const EvsAlloc<U>&) const noexcept { return true; }
    template <class U> bool operator!=(const EvsAlloc<U>&) const noexcept { return false; }
};
template <class T> using EvsVec = std::vector<T, EvsAlloc<T>>;
using EvsString = std::basic_string<char, std::char_traits<char>, EvsAlloc<char>>;
using EvsBytes = EvsVec<uint8_t>;

constexpr int kEvsNameSize = 0x20;
constexpr int kEvsEventActors = 0x14;
constexpr int32_t kEvsTotalFlags = 0x2800; // EventList.TOTAL_NUM_FLAGS
constexpr uint32_t kEvsEventSize = 0xB0, kEvsActorSize = 0x50, kEvsActionSize = 0x50, kEvsPropSize = 0x40;

// A fixed 32-byte name field, kept raw (wwrando's read_str/write_str round-trip it unchanged).
struct EvsName {
    uint8_t raw[kEvsNameSize];
    // Zero-filled; false (unchanged) when the name does not fit (31 bytes + NUL, as write_str).
    bool set(const char* name);
    // The bytes up to the first NUL.
    EvsString str() const;
    // Python's equality of the decoded names (raw bytes equal once the given name is zero-padded).
    bool is(const char* name) const;
};

struct EvsProperty {
    EvsName name;
    // as read (recomputed by serialize)
    int32_t index = -1;
    uint32_t dataIndex = 0, dataSize = 0;
    int32_t nextIndex = -1;
    // value: type PCR_EVL_TYPE_FLOAT / VEC / INT / STRING; words holds the s32 values or the float
    // bits (a vec3 is 3 words); a string property's text is str.
    uint32_t type = PCR_EVL_TYPE_INT;
    EvsVec<uint32_t> words;
    EvsString str;
    uint8_t runtime[0xC] = {};
    // serialize scratch
    uint32_t outOffset = 0;
    int32_t outIndex = -1;
    int nextRef = -1;

    void setInts(const int32_t* v, int n);
    void setFloats(const float* v, int n);
    void setVecs(const float* xyz, int nVecs); // 3 floats per vector
    void setString(const char* s);
    int count() const; // data_size as serialize writes it (vectors for vec3)
};

struct EvsAction {
    EvsName name;
    uint32_t duplicateId = 0;
    int32_t index = -1;
    int32_t startingFlags[3] = {-1, -1, -1};
    int32_t flag = -1; // flag_id_to_set
    int32_t firstPropIndex = -1, nextIndex = -1;
    uint8_t runtime[0x10] = {};
    EvsVec<int> props; // pool indexes (EventList::props), action.properties
    uint32_t outOffset = 0;
    int32_t outIndex = -1;
    int nextRef = -1;
};

struct EvsActor {
    EvsName name;
    uint32_t staffId = 0; // staff_identifier
    int32_t index = -1;
    int32_t flag = -1;
    uint32_t staffType = 0;
    int32_t initialActionIndex = -1;
    uint8_t runtime[0x1C] = {};
    EvsVec<int> actions; // pool indexes (EventList::actions), actor.actions
    uint32_t outOffset = 0;
    int32_t outIndex = -1;
};

struct EvsEvent {
    EvsName name;
    int32_t index = -1;
    uint32_t unknown1 = 0, priority = 0;
    int32_t actorIndexes[kEvsEventActors];
    uint32_t numActors = 0;
    int32_t startingFlags[2] = {-1, -1};
    int32_t endingFlags[3] = {-1, -1, -1};
    uint8_t playJingle = 0;
    uint8_t runtime[0x1B] = {};
    EvsVec<int> actors; // pool indexes (EventList::actors), event.actors
    uint32_t outOffset = 0;
    EvsEvent() {
        for (int i = 0; i < kEvsEventActors; i++) {
            actorIndexes[i] = -1;
        }
    }
};

class EvsList {
public:
    // event.events in file order; the pools hold every record read (also ones no event uses, which
    // serialize drops, as wwrando does) and every one added.
    EvsVec<EvsEvent> events;
    EvsVec<EvsActor> actors;
    EvsVec<EvsAction> actions;
    EvsVec<EvsProperty> props;
    uint8_t headerPadding[8] = {};

    // EventList.read. PCR_EVL_OK, PCR_EVL_BAD_FILE (layout, duplicate event name, a used actor slot
    // after a blank one, a flag used twice or out of range: where Python raises), PCR_EVL_SWAPPED
    // (dEvent_manager_c::setData already swapped the buffer), PCR_EVL_BAD_TYPE (unknown property
    // type), PCR_EVL_LOOP. On an error the list is empty.
    int read(const void* data, size_t size);
    // An empty list (no events), as a file whose seven list offsets are 0x40 and counts 0.
    void clear();

    // EventList.save_changes into out (replaced). PCR_EVL_OK, or PCR_EVL_BAD_RANGE (an actor with no
    // actions, more than 20 actors in an event, a bad pool index, a name that does not fit),
    // in which case out is cleared.
    int serialize(EvsBytes& out);

    // --- lookups (pool indexes, or -1) ---
    int findEvent(const char* name) const;                        // events_by_name[name] (events index)
    int findActor(int event, const char* name) const;             // first actor of the event named so
    int findAction(int actor, const char* name, int nth = 0) const; // nth action of the actor named so
    int getProp(int action, const char* name) const;              // action.get_prop(name)

    // --- additions (wwrando's add_*; each returns the new pool index, or -1) ---
    // add_event: -1 when the name is used or does not fit.
    int addEvent(const char* name);
    // event.add_actor: flag = get_unused_flag_id(); -1 when no flag is left or a bad event/name.
    int addActor(int event, const char* name);
    // actor.add_action (appended to the actor's list): flag = get_unused_flag_id().
    int addAction(int actor, const char* name);
    // action.add_property (appended), with its value.
    int addPropInts(int action, const char* name, const int32_t* v, int n);
    int addPropInt(int action, const char* name, int32_t v) { return addPropInts(action, name, &v, 1); }
    int addPropFloats(int action, const char* name, const float* v, int n);
    int addPropFloat(int action, const char* name, float v) { return addPropFloats(action, name, &v, 1); }
    int addPropVecs(int action, const char* name, const float* xyz, int nVecs);
    int addPropVec(int action, const char* name, float x, float y, float z) {
        const float v[3] = {x, y, z};
        return addPropVecs(action, name, v, 1);
    }
    int addPropString(int action, const char* name, const char* s);

    // get_unused_flag_id: the lowest flag in [0, 0x2800) no event's actor or action used when the
    // file was read and that was not handed out since; -1 when none is left.
    int32_t unusedFlag();

private:
    int addProp(int action, const char* name);
    EvsVec<uint8_t> mFlagUsed; // kEvsTotalFlags entries, from read (or clear)
    int32_t mFlagCursor = 0;
};

// Serializes `list` into the resource file (a patcher's PcRandoResFile, R0.10): in place when it
// fits (the rest of the old bytes zeroed; the game reads the lists through the header's offsets),
// else into a bigger copy on the archive heap (pc_rando_res_replace). PCR_EVL_OK, a serialize
// error, or PCR_EVL_BAD_RANGE when the copy cannot be allocated (the file is then unchanged).
int pc_rando_evser_write_res(PcRandoResFile* file, EvsList& list);

// 64-bit FNV-1a (smoke and test fixtures compare files by it).
uint64_t pc_rando_evser_fnv64(const void* data, size_t size);

} // namespace pcr
