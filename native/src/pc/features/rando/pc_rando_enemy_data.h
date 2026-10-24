/*
 * pc_rando_enemy_data.h - the enemy randomizer's data as read-only C++ tables (docs/RANDOMIZER_PLAN.md,
 * step R7.3a).
 *
 * Generated offline from wwrando 1.10.0_9775811 (MIT, ref/wwrando) by native/tools/rando/gen_enemy_data.py
 * into generated/rando_enemies.inc (decision D11); the build never runs Python. Sources:
 *   data/enemy_types.txt        46 enemy types, file order, every field (EnemyType)
 *   logic/enemy_locations.txt   82 stage keys (47 stage folders and 35 "sea/RoomN.arc" keys) with 222 groups
 *                               and 1,275 enemy locations, file order, plus the groups' optional fields
 *   data/actor_info.txt         824 actor names -> class (lower case), id, subtype, GBA name
 *   data/actor_parameters.txt   the bitfields of the enemy actors' classes and of Homen*, ALLdie, AND_SW*,
 *                               Kbota* (class names lower case, as wwrando's data_tables.py)
 *   randomizers/enemies.py      its module constants
 * Lists keep wwrando's order: the enemy generator ports Python list semantics (D1).
 *
 * Strings are the exact Python values ("Original requirements" and "Requirements to defeat" as the
 * YAML loader folds them; R1.1 compiles them). Optional YAML fields carry a flag (or -1 / nullptr).
 *
 * Pure: no game headers, no allocation, no static constructors (cos_rando_core and the unit tests link
 * it). Lookups by name are linear scans.
 */
#ifndef PC_RANDO_ENEMY_DATA_H
#define PC_RANDO_ENEMY_DATA_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_data.h" // Span, StrSpan

namespace pcr {

// --- Placement categories (enemies.py EnemyCategory) ----------------------------------------------

// The category part of a placement category string ("Ground", "Ceiling:ChecksConditionSwitch", ...).
enum class EnemyCat : uint8_t { Ground, Pot, Air, StationaryAir, Ceiling, Sea, Water };
constexpr int kNumEnemyCats = 7;
const char* enemy_cat_name(EnemyCat c);

// The conditions after the ':' (EnemyCategory.__eq__ implements exactly these three).
enum : uint8_t {
    ECOND_CHECKS_CONDITION_SWITCH = 1u << 0, // the type needs an "Enable spawn switch param name"
    ECOND_SETS_DEATH_SWITCH = 1u << 1,       // the type needs a "Death switch param name"
    ECOND_HAS_PIT = 1u << 2,                 // the type needs "Allow near pits"
};

// --- Enemy types (data/enemy_types.txt) -----------------------------------------------------------

struct EnemyType {
    const char* pretty_name;   // "Bokoblin" (unique)
    const char* actor_name;    // "Bk" (shared by subspecies: mo2, p_hat, amos2, Oqw, nezumi, ...)
    uint32_t params;           // Params
    uint16_t x_rot;            // X Rotation
    uint16_t z_rot;            // Z Rotation
    uint16_t first_particle;   // Required particle IDs: into enemy_particle_ids()
    uint16_t num_particles;
    const char* requirements;  // Requirements to defeat
    bool allow_from;           // Allow randomizing from
    bool allow_to;             // Allow randomizing to
    uint8_t num_categories;    // Placement categories, in file order
    EnemyCat categories[3];
    uint32_t mem_first;        // Memory used by first one
    uint32_t mem_subsequent;   // Memory used by subsequent ones
    const char* spawn_switch_param; // Enable spawn switch param name, or nullptr (null)
    const char* death_switch_param; // Death switch param name, or nullptr
    const char* path_param;         // Path param name, or nullptr
    bool allow_near_pits;           // Allow near pits
    bool killable_thrown;           // Can be killed with thrown objects
    bool killable_bomb_flowers;     // Can be killed with bomb flowers
};

Span<EnemyType> enemy_types();
Span<uint16_t> enemy_particle_ids();
Span<uint16_t> enemy_type_particles(const EnemyType& t);
int enemy_type_index(const char* pretty_name); // or -1

// The compiled EnemyCategory facts (the same for each of a type's categories): the ECOND_* bits the
// type satisfies (can_check_switch, can_set_switch, allow_near_pits).
uint8_t enemy_type_conditions(const EnemyType& t);
// EnemyRandomizer.is_enemy_allowed_in_placement_category: `category in enemy_type["Compiled
// categories"]`, i.e. any of the type's categories == the location's (cat, conds) by
// EnemyCategory.__eq__ (conditions first, then same category, Pot <- Ground, Ground/Pot <- Air,
// StationaryAir <- Air).
bool enemy_type_allowed_in(const EnemyType& t, EnemyCat cat, uint8_t conds);
// Parses a placement category string as EnemyCategory.__eq__ does. False for an unknown category or
// condition (Python raises NotImplementedError for the latter).
bool parse_enemy_category(const char* s, EnemyCat* cat, uint8_t* conds);

// --- Actor paths ----------------------------------------------------------------------------------

// get_fourcc_by_english_chunk_name: Actor -> ACTR, ScalableObject -> SCOB, Door -> DOOR, ScalableDoor -> TGDR.
enum class EnemyChunk : uint8_t { Actor, ScalableObject, Door, ScalableDoor };

// "<stage>/<arc>[/Layer<l>]/<Chunk><idx>", parsed with enemies.py's regex for it.
struct EnemyActorPath {
    const char* stage;  // "M_NewD2", "sea"
    const char* arc;    // "Room12.arc" or "Stage.arc"
    const char* file;   // "M_NewD2/Room12.arc" (under files/res/Stage/)
    int8_t room;        // N of RoomN.arc, or -1 for Stage.arc (stage.dzs; else room.dzr)
    int8_t layer;       // 0-11, or -1 for the default layer
    EnemyChunk chunk;
    uint16_t index;     // entry index among that chunk type's entries on that layer
    const char* raw;    // the path as written
};

// --- Enemy locations (logic/enemy_locations.txt) --------------------------------------------------

// A top-level key: a stage folder, or "sea/RoomN.arc" for the sea (each sea room is its own key).
struct EnemyStage {
    const char* key;
    uint16_t first_group; // into enemy_groups()
    uint16_t num_groups;
};

struct EnemyGroup {
    uint16_t stage;            // into enemy_stages()
    bool must_defeat;          // Must defeat enemies
    bool has_throwable;        // Has throwable objects (absent = false)
    bool has_bomb_flowers;     // Has bomb flowers (absent = false)
    int32_t free_memory;       // Free memory, or -1 when absent (get_free_memory_for_group: 1000000)
    const char* original_req;  // Original requirements
    uint16_t first_enemy, num_enemies;     // enemy_locations()
    uint16_t first_alldie, num_alldie;     // enemy_alldie_paths(): Actors to replace with ALLdies
    uint16_t first_modify, num_modify;     // enemy_modify_actors(): Existing actors to modify
    uint16_t first_add, num_add;           // enemy_add_actors(): New actors to add
    uint16_t first_remove, num_remove;     // enemy_remove_paths(): Actors to remove
};

enum : uint8_t {
    ELOC_POSITION = 1u << 0,          // Position: set x/y/z
    ELOC_POSITION_OFFSET = 1u << 1,   // Position offset: add to x/y/z
    ELOC_Y_ROTATION = 1u << 2,        // Y Rotation: set
    ELOC_Y_ROTATION_OFFSET = 1u << 3, // Y Rotation offset: add, then & 0xFFFF
};

struct EnemyLocation {
    uint16_t group;              // into enemy_groups()
    uint16_t original_type;      // into enemy_types() (Original enemy)
    const char* original_enemy;  // Original enemy (a Pretty name)
    const char* category_raw;    // Placement category as written ("Ground:SetsDeathSwitch,HasPit")
    EnemyCat category;
    uint8_t conds;               // ECOND_* bits of the category string
    EnemyActorPath path;         // always an Actor path
    uint8_t flags;               // ELOC_*
    double position[3];
    double position_offset[3];
    uint16_t y_rot;
    int32_t y_rot_offset;
};

enum : uint8_t {
    EACT_NAME = 1u << 0,
    EACT_PARAMS = 1u << 1,
    EACT_X_ROTATION = 1u << 2,
    EACT_Y_ROTATION = 1u << 3,
    EACT_Z_ROTATION = 1u << 4,
    EACT_POSITION = 1u << 5,
};

// An "Existing actors to modify" entry (path set) or a "New actors to add" entry (path.raw == nullptr;
// it goes on the layer of the group's first enemy). Only fields with their EACT_* flag are written.
struct EnemyActorData {
    EnemyActorPath path;
    uint8_t flags;
    const char* name;
    uint32_t params;
    uint16_t x_rot, y_rot, z_rot;
    double position[3];
};

Span<EnemyStage> enemy_stages();
Span<EnemyGroup> enemy_groups();
Span<EnemyLocation> enemy_locations();
Span<EnemyActorPath> enemy_alldie_paths();
Span<EnemyActorPath> enemy_remove_paths();
Span<EnemyActorData> enemy_modify_actors();
Span<EnemyActorData> enemy_add_actors();
// The slices of one group / stage.
Span<EnemyGroup> enemy_stage_groups(const EnemyStage& s);
Span<EnemyLocation> enemy_group_locations(const EnemyGroup& g);
Span<EnemyActorPath> enemy_group_alldies(const EnemyGroup& g);
Span<EnemyActorData> enemy_group_modifies(const EnemyGroup& g);
Span<EnemyActorData> enemy_group_adds(const EnemyGroup& g);
Span<EnemyActorPath> enemy_group_removes(const EnemyGroup& g);
int enemy_stage_index(const char* key); // or -1

// --- enemies.py constants -------------------------------------------------------------------------

int enemy_max_species_per_stage();        // MAX_ENEMY_SPECIES_PER_STAGE (10)
int enemy_max_species_per_group();        // MAX_ENEMY_SPECIES_PER_GROUP (5)
int enemy_max_redos_per_stage();          // MAX_RANDOMIZATION_REDOS_PER_STAGE (50)
int enemy_max_redos_per_room();           // MAX_RANDOMIZATION_REDOS_PER_ROOM (20)
int enemy_min_free_space_per_room();      // MIN_FREE_SPACE_TO_LEAVE_PER_ROOM (60000)
// STAGE_NAMES_WHERE_MULTIPLE_ROOMS_CAN_BE_LOADED_AT_ONCE, in order.
StrSpan enemy_two_room_stages();
bool enemy_is_two_room_stage(const char* stage_folder);

// --- Actors (data/actor_info.txt, data/actor_parameters.txt) --------------------------------------

struct ActorInfo {
    const char* name;       // "Bk"
    const char* class_name; // "d_a_bk" (lower case)
    uint16_t actor_id;
    uint8_t subtype;
    uint8_t gba_name;
};
Span<ActorInfo> actor_infos();
// DataTables.actor_name_to_class_name; nullptr for a name actor_info.txt does not list.
const char* actor_class_name(const char* actor_name);

// The ACTR field a bitfield lives in.
enum class ActorBitfield : uint8_t { params, x_rot, z_rot };

struct ActorParamField {
    const char* name; // "bokoblin_type"
    ActorBitfield bitfield;
    uint32_t mask;
};
struct ActorParamClass {
    const char* class_name; // lower case
    uint16_t first_field;   // into actor_param_fields()
    uint16_t num_fields;
};
Span<ActorParamField> actor_param_fields();
Span<ActorParamClass> actor_param_classes();
// The bitfield `param` of class `class_name` (lower case), or nullptr when the class is not emitted
// or has no such field.
const ActorParamField* actor_param(const char* class_name, const char* param);
// dzx.py ChunkEntry's bitfield access: get = (field & mask) >> lowest_set_bit(mask);
// set = (field & ~mask) | ((value << lowest_set_bit(mask)) & mask). A zero mask never occurs (the
// generator rejects it).
uint32_t actor_param_get(const ActorParamField& f, uint32_t field);
uint32_t actor_param_set(const ActorParamField& f, uint32_t field, uint32_t value);

} // namespace pcr

#endif
