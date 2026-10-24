// pc_rando_enemy_data.cpp - the enemy randomizer's data tables (docs/RANDOMIZER_PLAN.md, step R7.3a).
// The data lives in generated/rando_enemies.inc (native/tools/rando/gen_enemy_data.py); this file wraps
// it and ports the small pure helpers of enemies.py's EnemyCategory and dzx.py's bitfield access.
// Pure: no game headers, no allocation (pc_rando_enemy_data.h).
#include "pc_rando_enemy_data.h"

#include <string.h>

namespace pcr {
namespace gen_enemy {
#include "generated/rando_enemies.inc"
} // namespace gen_enemy

namespace {

template <class T, size_t N> constexpr Span<T> span(const T (&a)[N]) { return Span<T>{a, N}; }

template <class T> Span<T> slice(Span<T> all, size_t first, size_t n) { return Span<T>{all.data + first, n}; }

const char* const kCatNames[kNumEnemyCats] = {"Ground", "Pot", "Air", "StationaryAir", "Ceiling", "Sea", "Water"};

// EnemyCategory.__eq__ after the conditions: does a type category `mine` accept a location category `other`?
bool catAccepts(EnemyCat mine, EnemyCat other) {
    if (mine == other) {
        return true;
    }
    if (other == EnemyCat::Pot && mine == EnemyCat::Ground) {
        return true;
    }
    if ((other == EnemyCat::Ground || other == EnemyCat::Pot) && mine == EnemyCat::Air) {
        return true;
    }
    if (other == EnemyCat::StationaryAir && mine == EnemyCat::Air) {
        return true;
    }
    return false;
}

int lowestSetBit(uint32_t mask) {
    for (int i = 0; i < 32; i++) {
        if (mask & (1u << i)) {
            return i;
        }
    }
    return 0;
}

} // namespace

const char* enemy_cat_name(EnemyCat c) {
    int i = (int)c;
    return i >= 0 && i < kNumEnemyCats ? kCatNames[i] : nullptr;
}

Span<EnemyType> enemy_types() { return span(gen_enemy::kEnemyTypes); }
Span<uint16_t> enemy_particle_ids() { return span(gen_enemy::kEnemyParticleIds); }
Span<uint16_t> enemy_type_particles(const EnemyType& t) {
    return slice(enemy_particle_ids(), t.first_particle, t.num_particles);
}
int enemy_type_index(const char* pretty_name) {
    if (pretty_name == nullptr) {
        return -1;
    }
    Span<EnemyType> s = enemy_types();
    for (size_t i = 0; i < s.size; i++) {
        if (strcmp(s[i].pretty_name, pretty_name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

uint8_t enemy_type_conditions(const EnemyType& t) {
    uint8_t bits = 0;
    if (t.spawn_switch_param != nullptr) {
        bits |= ECOND_CHECKS_CONDITION_SWITCH;
    }
    if (t.death_switch_param != nullptr) {
        bits |= ECOND_SETS_DEATH_SWITCH;
    }
    if (t.allow_near_pits) {
        bits |= ECOND_HAS_PIT;
    }
    return bits;
}

bool enemy_type_allowed_in(const EnemyType& t, EnemyCat cat, uint8_t conds) {
    if ((conds & ~enemy_type_conditions(t)) != 0) {
        return false; // every compiled category fails the same condition
    }
    for (int i = 0; i < t.num_categories; i++) {
        if (catAccepts(t.categories[i], cat)) {
            return true;
        }
    }
    return false;
}

bool parse_enemy_category(const char* s, EnemyCat* cat, uint8_t* conds) {
    if (s == nullptr) {
        return false;
    }
    const char* colon = strchr(s, ':');
    size_t baseLen = colon != nullptr ? (size_t)(colon - s) : strlen(s);
    int found = -1;
    for (int i = 0; i < kNumEnemyCats; i++) {
        if (strlen(kCatNames[i]) == baseLen && strncmp(kCatNames[i], s, baseLen) == 0) {
            found = i;
        }
    }
    uint8_t bits = 0;
    if (colon != nullptr) {
        // other_conditions_string.split(","): every piece, including empty ones, must be known.
        const char* p = colon + 1;
        for (;;) {
            const char* comma = strchr(p, ',');
            size_t n = comma != nullptr ? (size_t)(comma - p) : strlen(p);
            if (n == 21 && strncmp(p, "ChecksConditionSwitch", n) == 0) {
                bits |= ECOND_CHECKS_CONDITION_SWITCH;
            } else if (n == 15 && strncmp(p, "SetsDeathSwitch", n) == 0) {
                bits |= ECOND_SETS_DEATH_SWITCH;
            } else if (n == 6 && strncmp(p, "HasPit", n) == 0) {
                bits |= ECOND_HAS_PIT;
            } else {
                return false;
            }
            if (comma == nullptr) {
                break;
            }
            p = comma + 1;
        }
    }
    if (found < 0) {
        return false;
    }
    *cat = (EnemyCat)found;
    *conds = bits;
    return true;
}

Span<EnemyStage> enemy_stages() { return span(gen_enemy::kEnemyStages); }
Span<EnemyGroup> enemy_groups() { return span(gen_enemy::kEnemyGroups); }
Span<EnemyLocation> enemy_locations() { return span(gen_enemy::kEnemyLocations); }
Span<EnemyActorPath> enemy_alldie_paths() { return span(gen_enemy::kEnemyAlldiePaths); }
Span<EnemyActorPath> enemy_remove_paths() { return span(gen_enemy::kEnemyRemovePaths); }
Span<EnemyActorData> enemy_modify_actors() { return span(gen_enemy::kEnemyModifyActors); }
Span<EnemyActorData> enemy_add_actors() { return span(gen_enemy::kEnemyAddActors); }

Span<EnemyGroup> enemy_stage_groups(const EnemyStage& s) { return slice(enemy_groups(), s.first_group, s.num_groups); }
Span<EnemyLocation> enemy_group_locations(const EnemyGroup& g) {
    return slice(enemy_locations(), g.first_enemy, g.num_enemies);
}
Span<EnemyActorPath> enemy_group_alldies(const EnemyGroup& g) {
    return slice(enemy_alldie_paths(), g.first_alldie, g.num_alldie);
}
Span<EnemyActorData> enemy_group_modifies(const EnemyGroup& g) {
    return slice(enemy_modify_actors(), g.first_modify, g.num_modify);
}
Span<EnemyActorData> enemy_group_adds(const EnemyGroup& g) { return slice(enemy_add_actors(), g.first_add, g.num_add); }
Span<EnemyActorPath> enemy_group_removes(const EnemyGroup& g) {
    return slice(enemy_remove_paths(), g.first_remove, g.num_remove);
}
int enemy_stage_index(const char* key) {
    if (key == nullptr) {
        return -1;
    }
    Span<EnemyStage> s = enemy_stages();
    for (size_t i = 0; i < s.size; i++) {
        if (strcmp(s[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int enemy_max_species_per_stage() { return gen_enemy::kMaxEnemySpeciesPerStage; }
int enemy_max_species_per_group() { return gen_enemy::kMaxEnemySpeciesPerGroup; }
int enemy_max_redos_per_stage() { return gen_enemy::kMaxRandomizationRedosPerStage; }
int enemy_max_redos_per_room() { return gen_enemy::kMaxRandomizationRedosPerRoom; }
int enemy_min_free_space_per_room() { return gen_enemy::kMinFreeSpaceToLeavePerRoom; }
StrSpan enemy_two_room_stages() { return span(gen_enemy::kTwoRoomStages); }
bool enemy_is_two_room_stage(const char* stage_folder) {
    if (stage_folder == nullptr) {
        return false;
    }
    for (const char* s : enemy_two_room_stages()) {
        if (strcmp(s, stage_folder) == 0) {
            return true;
        }
    }
    return false;
}

Span<ActorInfo> actor_infos() { return span(gen_enemy::kActorInfo); }
const char* actor_class_name(const char* actor_name) {
    if (actor_name == nullptr) {
        return nullptr;
    }
    for (const ActorInfo& a : actor_infos()) {
        if (strcmp(a.name, actor_name) == 0) {
            return a.class_name;
        }
    }
    return nullptr;
}

Span<ActorParamField> actor_param_fields() { return span(gen_enemy::kActorParamFields); }
Span<ActorParamClass> actor_param_classes() { return span(gen_enemy::kActorParamClasses); }
const ActorParamField* actor_param(const char* class_name, const char* param) {
    if (class_name == nullptr || param == nullptr) {
        return nullptr;
    }
    for (const ActorParamClass& c : actor_param_classes()) {
        if (strcmp(c.class_name, class_name) != 0) {
            continue;
        }
        for (const ActorParamField& f : slice(actor_param_fields(), c.first_field, c.num_fields)) {
            if (strcmp(f.name, param) == 0) {
                return &f;
            }
        }
        return nullptr;
    }
    return nullptr;
}

uint32_t actor_param_get(const ActorParamField& f, uint32_t field) { return (field & f.mask) >> lowestSetBit(f.mask); }
uint32_t actor_param_set(const ActorParamField& f, uint32_t field, uint32_t value) {
    return (field & ~f.mask) | ((value << lowestSetBit(f.mask)) & f.mask);
}

} // namespace pcr
