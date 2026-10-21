// pc_rando_event.h - in-place patch library for event_list.dat (docs/RANDOMIZER_PLAN.md, step R0.11).
// Pure: no game headers, no game calls, no allocation (the unit tests link it without the game).
// Every read and write is in the disc's big-endian format, on the raw file as dRes_info_c::
// loadResource leaves it, i.e. before dEvent_manager_c::setData (game/src/d/d_event_manager.cpp)
// swaps the FData/IData lists to host order in place. A patcher registered with
// pc_rando_register_res_patcher (R0.10, type PCR_RES_DAT) runs at that point.
//
// File format (wwrando's wwlib/events.py; the game's names in game/include/d/d_event_data.h):
//   header 0x40: {u32 offset, u32 count} for event, actor (staff), action (cut), property (data),
//                float list, integer list; then {u32 offset, u32 byte size} of the string list;
//                8 bytes padding, of which byte 0x38 is 0 on the disc (the port's setData sets it to
//                1 once it swapped the lists, so a swapped buffer is refused here).
//   event    0xB0: name[32], +0x20 index, +0x24 unknown, +0x28 priority, +0x2C s32 actor[20]
//                  (global actor indexes, -1 when unused), +0x7C actor count, +0x80 s32 starting
//                  flags[2], +0x88 s32 ending flags[3], +0x94 u8 play jingle.
//   actor    0x50: name[32], +0x20 staff id, +0x24 index, +0x28 flag id to set, +0x2C staff type,
//                  +0x30 initial action (global action index).
//   action   0x50: name[32], +0x20 duplicate id, +0x24 index, +0x28 s32 starting flags[3],
//                  +0x34 flag id to set, +0x38 first property (-1: none), +0x3C next action (-1).
//   property 0x40: name[32], +0x20 index, +0x24 type (0 float, 1 vec3, 3 int, 4 string),
//                  +0x28 data index (float/int list slot, or string byte offset), +0x2C data size
//                  (element count; string: padded byte length), +0x30 next property (-1).
//
// "Actor", "action" and "property" follow wwrando. An actor's actions are a chain (initial action,
// then next links), so an actor's action `ordinal` is wwrando's actor.actions[ordinal]; an action's
// properties are a chain too (action.properties[ordinal]).
//
// Operations, all in place (nothing grows; adding events/actors/actions is R3.14's serializer):
//   - rename an action (wwrando action.name = "011get_item");
//   - set integer / float property values (property.value = [...]); a list may also shrink;
//   - relink an actor's action chain from its own actions: truncate (actions[0:n]), remove
//     (actions.remove(a)), keep the last ones (actions[-n:]) or reorder (remove + insert);
//   - set event starting/ending flags and action starting flags;
//   - wwrando's change_event_item (randomizers/items.py), with the plan D9 fingerprint.
// A refused operation writes nothing.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum PcRandoEvlResult {
    PCR_EVL_OK = 0,
    PCR_EVL_BAD_FILE,    // header or a list out of the buffer
    PCR_EVL_SWAPPED,     // the buffer was already swapped to host order by dEvent_manager_c::setData
    PCR_EVL_NO_EVENT,    // no event with this name / index
    PCR_EVL_NO_ACTOR,    // actor slot empty or out of range, or no actor with this name
    PCR_EVL_NO_ACTION,   // action ordinal / index out of range, or no action with this name
    PCR_EVL_NO_PROPERTY, // no property with this name / ordinal
    PCR_EVL_BAD_TYPE,    // the property is not of the type the operation needs
    PCR_EVL_BAD_RANGE,   // element index or count out of the property, bad name length, bad list
    PCR_EVL_MISMATCH,    // a fingerprint does not match (D9: leave the location vanilla)
    PCR_EVL_SHARED,      // the property's list slots are used by another property too
    PCR_EVL_LOOP,        // an action or property chain loops or points out of its list
};

// A short name for logs ("ok", "mismatch", ...).
const char* pc_rando_evl_result_name(int result);

// A view over a raw event_list.dat in memory. Does not own the bytes.
struct PcRandoEvl {
    uint8_t* data;
    size_t size;
    uint32_t eventTop, eventNum;
    uint32_t actorTop, actorNum;
    uint32_t actionTop, actionNum;
    uint32_t propTop, propNum;
    uint32_t floatTop, floatNum;
    uint32_t intTop, intNum;
    uint32_t strTop, strSize;
};

#define PCR_EVL_NAME_MAX 32 /* name fields are 32 bytes; a name is at most 31 chars + NUL */
#define PCR_EVL_EVENT_ACTORS 20
#define PCR_EVL_SONG_FIRST 0x6D /* Wind's Requiem; songs are given by 059get_dance prm0 = id - 0x6D */
#define PCR_EVL_SONG_LAST 0x72

// Checks the header and that every list fits in the buffer; refuses a swapped buffer.
int pc_rando_evl_open(PcRandoEvl* evl, void* data, size_t size);

// --- records (NULL when the index is out of range) ---
uint8_t* pc_rando_evl_event_p(const PcRandoEvl* evl, int event);
uint8_t* pc_rando_evl_actor_p(const PcRandoEvl* evl, int actor);
uint8_t* pc_rando_evl_action_p(const PcRandoEvl* evl, int action);
uint8_t* pc_rando_evl_property_p(const PcRandoEvl* evl, int prop);
// A record's name (the 32-byte field, NUL-terminated into out).
void pc_rando_evl_name(const uint8_t* record, char out[PCR_EVL_NAME_MAX + 1]);

// --- lookups: global indexes, or -1 ---
int pc_rando_evl_find_event(const PcRandoEvl* evl, const char* name);
// The global actor index in an event's actor slot (wwrando event.actors[slot]).
int pc_rando_evl_event_actor(const PcRandoEvl* evl, int event, int slot);
// The slot of the first actor of an event with this name.
int pc_rando_evl_find_actor(const PcRandoEvl* evl, int event, const char* name);
// The number of actions in an actor's chain (-1 when the chain loops or is broken).
int pc_rando_evl_action_count(const PcRandoEvl* evl, int actor);
// The global action index of actor.actions[ordinal].
int pc_rando_evl_actor_action(const PcRandoEvl* evl, int actor, int ordinal);
// The ordinal of the nth (0-based) action named `name` in an actor's chain.
int pc_rando_evl_find_action(const PcRandoEvl* evl, int actor, const char* name, int nth);
// The global property index of action.properties[ordinal], or of its first property named `name`.
int pc_rando_evl_action_property_at(const PcRandoEvl* evl, int action, int ordinal);
int pc_rando_evl_action_property(const PcRandoEvl* evl, int action, const char* name);
// The flag an actor / action sets when it finishes (flag_id_to_set); -1 on a bad index.
int32_t pc_rando_evl_actor_flag(const PcRandoEvl* evl, int actor);
int32_t pc_rando_evl_action_flag(const PcRandoEvl* evl, int action);

// --- property values ---
// Type (PCR_EVL_TYPE_*) and element count of a property; -1 on a bad index.
#define PCR_EVL_TYPE_FLOAT 0
#define PCR_EVL_TYPE_VEC 1
#define PCR_EVL_TYPE_INT 3
#define PCR_EVL_TYPE_STRING 4
int pc_rando_evl_property_type(const PcRandoEvl* evl, int prop);
int pc_rando_evl_property_size(const PcRandoEvl* evl, int prop);
// The property's first slot in the integer / float list (data index), -1 on a bad index.
int pc_rando_evl_property_slot(const PcRandoEvl* evl, int prop);
// Element `elem` of an int property / of a float property (vec3: elem counts floats, 3 per vector).
int pc_rando_evl_get_int(const PcRandoEvl* evl, int prop, int elem, int32_t* out);
int pc_rando_evl_get_float(const PcRandoEvl* evl, int prop, int elem, float* out);
// A string property's text (inside the buffer), or NULL.
const char* pc_rando_evl_get_string(const PcRandoEvl* evl, int prop);

// --- operations ---
// Renames an action. expectOld (NULL: no check) is the fingerprint of the current name.
int pc_rando_evl_rename_action(PcRandoEvl* evl, int action, const char* name, const char* expectOld);
// Sets element `elem` of an int property. expectOld (NULL: no check) is the current value.
int pc_rando_evl_set_int(PcRandoEvl* evl, int prop, int elem, int32_t value, const int32_t* expectOld);
// Replaces an int property's value list with n values (1 <= n <= its current size; the size becomes
// n), as wwrando's property.value = [..]. expectOld (NULL: no check) holds the current first n values.
int pc_rando_evl_set_ints(PcRandoEvl* evl, int prop, const int32_t* values, int n, const int32_t* expectOld);
// Sets float `elem` of a float or vec3 property.
int pc_rando_evl_set_float(PcRandoEvl* evl, int prop, int elem, float value, const float* expectOld);
// The set_* functions refuse (PCR_EVL_SHARED) when another property's list range overlaps the
// written slots, since the edit would change that property too.

// Relinks an actor's action chain to n of its own current actions, in this order: order[i] is an
// ordinal of the current chain (0 <= order[i] < count, no repeats, n >= 1). The first becomes the
// initial action and the last one ends the chain. The dropped actions stay in the file, unlinked.
int pc_rando_evl_set_chain(PcRandoEvl* evl, int actor, const int* order, int n);
// actions = actions[0:keep]
int pc_rando_evl_truncate_actions(PcRandoEvl* evl, int actor, int keep);
// actions.remove(actions[ordinal])
int pc_rando_evl_remove_action(PcRandoEvl* evl, int actor, int ordinal);
// actions = actions[-keep:]
int pc_rando_evl_keep_last_actions(PcRandoEvl* evl, int actor, int keep);

// event.starting_flags[i] (i < 2) and event.ending_flags[i] (i < 3) = flag (-1: none).
int pc_rando_evl_set_starting_flag(PcRandoEvl* evl, int event, int i, int32_t flag);
int pc_rando_evl_set_ending_flag(PcRandoEvl* evl, int event, int i, int32_t flag);
// action.starting_flags[i] (i < 3) = flag.
int pc_rando_evl_set_action_starting_flag(PcRandoEvl* evl, int action, int i, int32_t flag);

// wwrando's change_event_item (randomizers/items.py): event `eventName`, actor slot, action ordinal
// (the location key's path). The action becomes "059get_dance" with prm0 = item - 0x6D for a song
// (0x6D-0x72), else "011get_item" with prm0 = item; its first property gets the single value.
// Fingerprint (D9; NULL expectName skips it): the action's current name is expectName and its first
// property's first value is expectPrm0. *outAction / *outSlot (optional) get the global action index
// and the integer-list slot written.
int pc_rando_evl_change_item(PcRandoEvl* evl, const char* eventName, int actorSlot, int actionOrdinal,
                             uint8_t item, const char* expectName, int32_t expectPrm0, int* outAction,
                             int* outSlot);
