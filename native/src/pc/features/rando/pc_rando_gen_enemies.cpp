// pc_rando_gen_enemies.cpp - the enemy randomizer's generator side (docs/RANDOMIZER_PLAN.md step
// R7.3c); see pc_rando_gen_enemies.h. Function and variable names follow randomizers/enemies.py.
#include "pc_rando_gen_enemies.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

#include "pc_rando_enemy_data.h"
#include "pc_rando_gen_enemy_logic.h"
#include "pc_rando_gen_items.h"
#include "pc_rando_logic.h"

namespace pcr {

namespace {

using pc::rando::PyRandom;

// random.choice on an empty sequence.
constexpr const char* kEmptyChoice = "Cannot choose from an empty sequence";

bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string joinPretty(const std::vector<int>& types) {
    std::string s;
    for (size_t i = 0; i < types.size(); i++) {
        s += (i ? ", " : "") + std::string(enemy_types()[types[i]].pretty_name);
    }
    return s;
}

// Actor names of `types` without duplicates, in order, joined with ", ".
std::string joinActorNamesUnique(const std::vector<int>& types) {
    std::vector<const char*> names;
    for (int t : types) {
        const char* n = enemy_types()[t].actor_name;
        bool seen = false;
        for (const char* m : names) {
            seen = seen || std::strcmp(m, n) == 0;
        }
        if (!seen) {
            names.push_back(n);
        }
    }
    std::string s;
    for (size_t i = 0; i < names.size(); i++) {
        s += (i ? ", " : "") + std::string(names[i]);
    }
    return s;
}

bool allowedIn(int type, const EnemyLocation& loc) {
    return enemy_type_allowed_in(enemy_types()[type], loc.category, loc.conds);
}

struct Ctx {
    PyRandom& rng;
    EnemyLogicFilter filter;
    std::vector<int> toRandomizeTo;  // enemies_to_randomize_to
    std::vector<int> mustKillList;   // enemies_to_randomize_to_when_all_enemies_must_be_killed
    std::string err;

    Ctx(Logic& logic, PyRandom& r)
        : rng(r), filter(logic), toRandomizeTo(enemyTypesToRandomizeTo()),
          mustKillList(enemyTypesToRandomizeToWhenAllMustBeKilled()) {}

    // The enemies_logically_allowed_in_this_group computation shared by both functions.
    bool allowedForGroup(const EnemyGroup& g, std::vector<int>& out) {
        if (g.must_defeat) {
            return filter.filter(g.original_req, g.has_throwable, g.has_bomb_flowers, mustKillList, out, &err);
        }
        out = toRandomizeTo;
        return true;
    }

    bool choose(const std::vector<int>& from, int* out) {
        if (from.empty()) {
            err = kEmptyChoice;
            return false;
        }
        *out = rng.choice(from);
        return true;
    }
};

// decide_on_enemy_pool_for_stage. False on an error (ctx.err).
bool decideOnEnemyPoolForStage(Ctx& ctx, const EnemyStage& stage, std::vector<int>& pool) {
    pool.clear();
    if (std::strcmp(stage.key, "sea") == 0) {
        // Never true at 1.10.0_9775811: the sea keys are "sea/RoomN.arc" (reproduced, not fixed).
        pool = ctx.toRandomizeTo;
        return true;
    }
    // category_and_logic_combos_needed: (Placement category string, logically allowed list).
    struct Combo {
        const EnemyLocation* loc; // the first location with this category string (for its parsed form)
        std::vector<int> allowed;
    };
    std::vector<Combo> combos;
    for (const EnemyGroup& g : enemy_stage_groups(stage)) {
        std::vector<int> allowed;
        if (!ctx.allowedForGroup(g, allowed)) {
            return false;
        }
        for (const EnemyLocation& loc : enemy_group_locations(g)) {
            bool found = false;
            for (const Combo& c : combos) {
                if (std::strcmp(c.loc->category_raw, loc.category_raw) == 0 && c.allowed == allowed) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                combos.push_back(Combo{&loc, allowed});
            }
        }
    }

    std::vector<int> allPossible;
    for (const Combo& c : combos) {
        std::vector<int> allowedForCombo;
        for (int t : c.allowed) {
            if (allowedIn(t, *c.loc)) {
                allowedForCombo.push_back(t);
            }
        }
        for (int t : allowedForCombo) {
            if (!contains(allPossible, t)) {
                allPossible.push_back(t);
            }
        }
        bool alreadyInPool = false;
        for (int t : allowedForCombo) {
            alreadyInPool = alreadyInPool || contains(pool, t);
        }
        if (alreadyInPool) {
            continue;
        }
        int chosen;
        if (!ctx.choose(allowedForCombo, &chosen)) {
            return false;
        }
        pool.push_back(chosen);
    }

    const int maxSpecies = enemy_max_species_per_stage();
    const int numChosen = (int)pool.size();
    if (numChosen > maxSpecies) {
        ctx.err = fmt("Enemy species pool for %s has %d species in it instead of %d", stage.key, numChosen, maxSpecies);
        return false;
    }
    for (int i = 0; i < maxSpecies - numChosen; i++) {
        std::vector<int> minusChosen;
        for (int t : allPossible) {
            if (!contains(pool, t)) {
                minusChosen.push_back(t);
            }
        }
        if (minusChosen.empty()) {
            break;
        }
        pool.push_back(ctx.rng.choice(minusChosen));
    }
    return true;
}

enum class GroupResult { Done, Retry, Error };

// randomize_enemy_group: Retry is Python's `return False` (not enough memory left).
GroupResult randomizeEnemyGroup(Ctx& ctx, const EnemyStage& stage, const EnemyGroup& group,
                                const std::vector<int>& stagePool, std::vector<EnemyPlacement>& doneForRoom) {
    doneForRoom.clear();
    // get_free_memory_for_group; Python divides by 2 into a float.
    double freeMemory = group.free_memory >= 0 ? (double)group.free_memory : 1000000.0;
    if (enemy_is_two_room_stage(stage.key)) {
        freeMemory = freeMemory / 2;
    }

    std::vector<int> allowed;
    if (!ctx.allowedForGroup(group, allowed)) {
        return GroupResult::Error;
    }

    Span<EnemyLocation> locs = enemy_group_locations(group);
    std::vector<const EnemyLocation*> uniqueCategories; // first location of each category string
    for (const EnemyLocation& loc : locs) {
        bool seen = false;
        for (const EnemyLocation* u : uniqueCategories) {
            seen = seen || std::strcmp(u->category_raw, loc.category_raw) == 0;
        }
        if (!seen) {
            uniqueCategories.push_back(&loc);
        }
    }

    std::vector<int> groupPool, allPossible;
    for (const EnemyLocation* cat : uniqueCategories) {
        std::vector<int> enemiesAllowed;
        for (int t : allowed) {
            if (allowedIn(t, *cat) && contains(stagePool, t)) {
                enemiesAllowed.push_back(t);
            }
        }
        for (int t : enemiesAllowed) {
            if (!contains(allPossible, t)) {
                allPossible.push_back(t);
            }
        }
        bool alreadyInPool = false;
        for (int t : enemiesAllowed) {
            alreadyInPool = alreadyInPool || contains(groupPool, t);
        }
        if (alreadyInPool) {
            continue;
        }
        int chosen;
        if (!ctx.choose(enemiesAllowed, &chosen)) {
            return GroupResult::Error;
        }
        groupPool.push_back(chosen);
    }

    const int maxSpecies = enemy_max_species_per_group();
    const int numChosen = (int)groupPool.size();
    if (numChosen > maxSpecies) {
        ctx.err = fmt("Enemy species pool for group has %d species in it instead of %d", numChosen, maxSpecies);
        return GroupResult::Error;
    }
    for (int i = 0; i < maxSpecies - numChosen; i++) {
        std::vector<int> minusChosen;
        for (int t : allPossible) {
            if (!contains(groupPool, t)) {
                minusChosen.push_back(t);
            }
        }
        if (minusChosen.empty()) {
            break;
        }
        groupPool.push_back(ctx.rng.choice(minusChosen));
    }

    std::vector<const char*> actorNamesPlaced; // enemy_actor_names_already_placed_in_room
    const double minFree = (double)enemy_min_free_space_per_room();
    for (const EnemyLocation& loc : locs) {
        std::vector<int> candidates;
        for (int t : groupPool) {
            if (allowedIn(t, loc)) {
                candidates.push_back(t);
            }
        }
        if (candidates.empty()) {
            std::string msg = fmt("No possible enemies to place in %s of the correct category\n", loc.path.arc);
            msg += "Enemies in this stage's enemy pool: " + joinPretty(stagePool) + "\n";
            msg += "Enemies logically allowed in this group: " + joinActorNamesUnique(allowed) + "\n";
            msg += "Enemies in this group's enemy pool: " + joinPretty(groupPool) + "\n";
            std::vector<int> ofCategory;
            for (int t = 0; t < (int)enemy_types().size; t++) {
                if (allowedIn(t, loc)) {
                    ofCategory.push_back(t);
                }
            }
            msg += std::string("Enemies of the correct category (") + loc.category_raw +
                   "): " + joinActorNamesUnique(ofCategory);
            ctx.err = msg;
            return GroupResult::Error;
        }
        const int newType = ctx.rng.choice(candidates);
        const EnemyType& nt = enemy_types()[newType];

        // get_amount_of_memory_for_enemy.
        bool placed = false;
        for (const char* n : actorNamesPlaced) {
            placed = placed || std::strcmp(n, nt.actor_name) == 0;
        }
        freeMemory -= placed ? nt.mem_subsequent : nt.mem_first;
        if (freeMemory < minFree) {
            return GroupResult::Retry;
        }
        if (!placed) {
            actorNamesPlaced.push_back(nt.actor_name);
        }
        EnemyPlacement p;
        p.location = (int)(&loc - enemy_locations().data);
        p.type = newType;
        doneForRoom.push_back(p);
    }
    return GroupResult::Done;
}

// randomize_enemy_groups_for_stage: Retry is Python's `return False`.
GroupResult randomizeEnemyGroupsForStage(Ctx& ctx, const EnemyStage& stage, std::vector<EnemyPlacement>& doneForStage) {
    doneForStage.clear();
    std::vector<int> stagePool;
    if (!decideOnEnemyPoolForStage(ctx, stage, stagePool)) {
        return GroupResult::Error;
    }
    const bool isSea = std::strcmp(stage.key, "sea") == 0; // never at 1.10.0_9775811
    for (const EnemyGroup& g : enemy_stage_groups(stage)) {
        int attempts = 0;
        int maxAttempts = enemy_max_redos_per_room();
        if (isSea) {
            maxAttempts *= 5;
        }
        std::vector<EnemyPlacement> doneForRoom;
        for (;;) {
            const GroupResult r = randomizeEnemyGroup(ctx, stage, g, stagePool, doneForRoom);
            if (r == GroupResult::Error) {
                return r;
            }
            attempts++;
            if (r == GroupResult::Done) {
                doneForStage.insert(doneForStage.end(), doneForRoom.begin(), doneForRoom.end());
                break;
            }
            if (attempts >= maxAttempts) {
                return GroupResult::Retry;
            }
        }
    }
    return GroupResult::Done;
}

// A stub of the ACTR entry _save edits: its bitfields through actor_parameters.txt (dzx.py ChunkEntry).
struct Stub {
    const char* className;
    uint32_t params;
    uint16_t xRot, zRot;
    std::string* err;
    bool ok = true;

    const ActorParamField* field(const char* name) {
        const ActorParamField* f = className != nullptr ? actor_param(className, name) : nullptr;
        if (f == nullptr && ok) {
            ok = false;
            *err = fmt("Tried to set unknown actor parameter \"%s\" for actor class %s", name,
                       className != nullptr ? className : "None");
        }
        return f;
    }
    uint32_t get(const char* name) {
        const ActorParamField* f = field(name);
        if (f == nullptr) {
            return 0;
        }
        switch (f->bitfield) {
        case ActorBitfield::params: return actor_param_get(*f, params);
        case ActorBitfield::x_rot: return actor_param_get(*f, xRot);
        case ActorBitfield::z_rot: return actor_param_get(*f, zRot);
        }
        return 0;
    }
    void set(const char* name, uint32_t v) {
        const ActorParamField* f = field(name);
        if (f == nullptr) {
            return;
        }
        switch (f->bitfield) {
        case ActorBitfield::params: params = actor_param_set(*f, params, v); break;
        case ActorBitfield::x_rot: xRot = (uint16_t)actor_param_set(*f, xRot, v); break;
        case ActorBitfield::z_rot: zRot = (uint16_t)actor_param_set(*f, zRot, v); break;
        }
    }
};

template <size_t N> int pick(PyRandom& rng, const int (&choices)[N]) { return choices[rng.choiceIndex(N)]; }

bool isOneOf(const char* s, std::initializer_list<const char*> names) {
    for (const char* n : names) {
        if (std::strcmp(s, n) == 0) {
            return true;
        }
    }
    return false;
}

// randomize_enemy_params(enemy_data, enemy, category, ...): `enemy` already holds the new type's
// name, params and rotations.
void randomizeEnemyParamsFor(PyRandom& rng, const EnemyType& data, const char* category, Stub& e) {
    const char* name = data.actor_name;
    auto cat = [&](const char* c) { return std::strcmp(category, c) == 0; };
    if (std::strcmp(name, "Bk") == 0) {
        static const int kColor[] = {0, 1, 2}; // blue, green, pink
        const int color = pick(rng, kColor);
        if (cat("Pot")) {
            static const int k[] = {2, 3};
            e.set("bokoblin_type", pick(rng, k));
        } else if (color == 2) {
            e.set("bokoblin_type", 0xB);
        } else {
            static const int k[] = {0, 4};
            e.set("bokoblin_type", pick(rng, k));
        }
        e.set("is_green", color == 1 ? 1 : 0);
        static const int kWeapon[] = {0, 1, 2, 3};
        e.set("weapon", pick(rng, kWeapon));
    } else if (isOneOf(name, {"c_green", "c_red", "c_kiiro", "c_blue", "c_black"})) {
        e.set("behavior_type", cat("Ceiling") ? 1 : cat("Pot") ? 4 : 0);
    } else if (std::strcmp(name, "Bb") == 0) {
        if (cat("Air")) {
            static const int k[] = {0, 1, 2, 3};
            e.set("behavior_type", pick(rng, k));
        } else if (cat("Ceiling")) {
            static const int k[] = {3, 4, 7};
            e.set("behavior_type", pick(rng, k));
        } else {
            static const int k[] = {4, 7};
            e.set("behavior_type", pick(rng, k));
        }
    } else if (std::strcmp(name, "p_hat") == 0) {
        if (cat("Ceiling")) {
            e.set("horizontal_range", 20);
            e.set("vertical_range", 50);
        }
    } else if (isOneOf(name, {"keeth", "Fkeeth"})) {
        if (cat("Ceiling")) {
            e.set("behavior_type", 0);
            e.set("range", 2);
        } else {
            e.set("behavior_type", 1);
        }
    } else if (std::strcmp(name, "Oq") == 0) {
        static const int k[] = {0, 1};
        e.set("octorok_projectile_type", pick(rng, k));
    } else if (std::strcmp(name, "Oqw") == 0) {
        if (std::strcmp(data.pretty_name, "Saltwater Octorok") == 0) {
            static const int k[] = {1, 4};
            e.set("octorok_type", pick(rng, k));
        } else if (std::strcmp(data.pretty_name, "Saltwater Octorok Spawner") == 0) {
            e.set("octorok_type", 3);
        }
    } else if (isOneOf(name, {"Rdead1", "Rdead2"})) {
        static const int k[] = {0, 1};
        e.set("idle_animation", pick(rng, k));
    } else if (std::strcmp(name, "pow") == 0) {
        static const int kType[] = {0, 1, 2};
        e.set("poe_type", pick(rng, kType));
        static const int kFloats[] = {0, 1};
        e.set("poe_floats", pick(rng, kFloats));
        static const int kColor[] = {0, 1, 2, 3, 4, 5};
        e.set("poe_color", pick(rng, kColor));
    } else if (isOneOf(name, {"kuro_s", "kuro_t"})) {
        if (cat("Pot")) {
            e.set("behavior_type", 6);
            static const int k[] = {0, 20, 60};
            e.set("pot_notice_range", pick(rng, k));
        } else {
            static const int k[] = {0, 1};
            e.set("behavior_type", pick(rng, k));
        }
        e.set("num_morths_in_group", (uint32_t)rng.randrange(1, 10 + 1));
    } else if (std::strcmp(name, "Puti") == 0) {
        static const int k[] = {0, 1};
        e.set("initial_spawn_delay", pick(rng, k));
    } else if (std::strcmp(name, "nezuana") == 0) {
        e.set("num_spawned_rats", (uint32_t)rng.randrange(1, 5 + 1));
    } else if (std::strcmp(name, "Stal") == 0) {
        static const int k[] = {0, 0, 0, 1, 1, 0xE};
        e.set("stalfos_type", pick(rng, k));
    } else if (std::strcmp(name, "Tn") == 0) {
        static const int k[] = {0, 4};
        e.set("behavior_type", pick(rng, k));
        e.set("color", (uint32_t)rng.randrange(0, 5 + 1));
        e.set("equipment", (uint32_t)rng.randrange(0, 5 + 1));
    } else if (std::strcmp(name, "bable") == 0) {
        if (cat("Air")) {
            e.set("bubble_should_float", 1);
        } else if (cat("Ground") || cat("Ceiling")) {
            e.set("bubble_should_float", 0);
        }
    } else if (std::strcmp(name, "gmos") == 0) {
        if (std::strcmp(data.pretty_name, "Winged Mothula") == 0) {
            static const int k[] = {1, 2, 2, 3, 3, 4, 4, 4, 4, 4};
            const int numWings = pick(rng, k);
            const int numMissing = 4 - numWings;
            int wings[4] = {0, 1, 2, 3};
            rng.shuffle(wings, 4);
            e.set("mothula_initially_missing_wings", 0);
            for (int i = 0; i < numMissing; i++) {
                e.set("mothula_initially_missing_wings", e.get("mothula_initially_missing_wings") | (1u << wings[i]));
            }
        }
    } else if (isOneOf(name, {"GyCtrl", "GyCtrlB"})) {
        static const int k[] = {1, 1, 1, 1, 2, 2, 3, 3, 4, 5};
        e.set("num_spawned_gyorgs", pick(rng, k));
    } else if (isOneOf(name, {"Fmaster", "Fmastr1", "Fmastr2"})) {
        static const int k[] = {0, 1};
        e.set("targeting_behavior_type", pick(rng, k));
    }
    // mo2, amos, amos2, Sss, wiz_r, nezumi, bbaba, magtail: no params change.
}

uint8_t placementFlags(const EnemyLocation& loc, const EnemyType& t) {
    uint8_t f = 0;
    if (loc.conds & ECOND_CHECKS_CONDITION_SWITCH) {
        f |= EFLAG_SPAWN_SWITCH;
    }
    if (loc.conds & ECOND_SETS_DEATH_SWITCH) {
        f |= EFLAG_DEATH_SWITCH;
    }
    if (std::strcmp(loc.category_raw, "Ceiling") == 0) {
        bool typeHasCeiling = false;
        for (int i = 0; i < t.num_categories; i++) {
            typeHasCeiling = typeHasCeiling || t.categories[i] == EnemyCat::Ceiling;
        }
        if (!typeHasCeiling) {
            f |= EFLAG_CEILING_DROP;
        }
    }
    if (std::strcmp(t.actor_name, "magtail") == 0) {
        f |= EFLAG_MAGTAIL_RAISE;
    } else if (isOneOf(t.actor_name, {"c_green", "c_red", "c_kiiro", "c_blue", "c_black"}) &&
               std::strcmp(loc.category_raw, "Pot") == 0) {
        f |= EFLAG_POT_SNAP;
    }
    return f;
}

void setErr(std::string* err, const std::string& msg) {
    if (err != nullptr) {
        *err = msg;
    }
}

} // namespace

bool randomizeEnemyLocations(Logic& logic, PyRandom& rng, std::vector<EnemyPlacement>& done, std::string* err) {
    done.clear();
    Ctx ctx(logic, rng);
    const int maxStageAttempts = enemy_max_redos_per_stage();
    for (const EnemyStage& stage : enemy_stages()) {
        int attempts = 0;
        const int maxAttempts = std::strcmp(stage.key, "sea") == 0 ? 1 : maxStageAttempts;
        std::vector<EnemyPlacement> doneForStage;
        for (;;) {
            const GroupResult r = randomizeEnemyGroupsForStage(ctx, stage, doneForStage);
            if (r == GroupResult::Error) {
                setErr(err, ctx.err);
                return false;
            }
            attempts++;
            if (r == GroupResult::Done) {
                done.insert(done.end(), doneForStage.begin(), doneForStage.end());
                break;
            }
            if (attempts >= maxAttempts) {
                setErr(err, fmt("Enemy rando failed %d times for one stage (%s)", attempts, stage.key));
                return false;
            }
        }
    }
    return true;
}

bool randomizeEnemyParams(PyRandom& rng, EnemyRandoResult& out, std::string* err) {
    std::string e;
    for (EnemyPlacement& p : out.placements) {
        const EnemyLocation& loc = enemy_locations()[p.location];
        const EnemyType& t = enemy_types()[p.type];
        Stub stub{actor_class_name(t.actor_name), t.params, t.x_rot, t.z_rot, &e};
        randomizeEnemyParamsFor(rng, t, loc.category_raw, stub);
        if (!stub.ok) {
            setErr(err, e);
            return false;
        }
        p.params = stub.params;
        p.x_rot = stub.xRot;
        p.z_rot = stub.zRot;
        p.flags = placementFlags(loc, t);
    }
    // randomize_enemies_spawned_by_objects (add_modify_and_replace_actors_for_enemy_rando draws nothing).
    for (int i = 0; i < kNumHomen; i++) {
        out.homen[i] = pick(rng, kHomenEnemyChoices);
    }
    return true;
}

bool runEnemyRandomizer(Logic& logic, const std::string& sanitizedSeed, const Options& o, const char* seedKey,
                        EnemyRandoResult& out, std::string* err) {
    out = EnemyRandoResult();
    {
        PyRandom rng;
        newRandomizerRng(rng, sanitizedSeed, o, seedKey);
        traceAttach(rng, "EnemyRandomizer");
        if (!randomizeEnemyLocations(logic, rng, out.placements, err)) {
            return false;
        }
    }
    // _save: reset_rng(), then the params (wwrando runs it after every randomizer's _randomize; the
    // RNG is fresh, so running it here gives the same draws).
    PyRandom rng;
    newRandomizerRng(rng, sanitizedSeed, o, seedKey);
    traceAttach(rng, "EnemyRandomizer._save");
    return randomizeEnemyParams(rng, out, err);
}

Json enemiesToJson(const EnemyRandoResult& r) {
    Json locs = Json::object();
    auto triple = [](const double* v) {
        Json a = Json::array();
        for (int i = 0; i < 3; i++) {
            // The data's YAML numbers: integers, and two floats (kindan/Room13 563.4323) that Python
            // writes with repr (Json::real is the shortest round trip too).
            const double r = v[i];
            a.push(r == (double)(int64_t)r ? Json::integer((int64_t)r) : Json::real(r));
        }
        return a;
    };
    for (const EnemyPlacement& p : r.placements) {
        const EnemyLocation& loc = enemy_locations()[p.location];
        const EnemyType& t = enemy_types()[p.type];
        Json e = Json::object();
        e.set("type", Json::string(t.pretty_name));
        e.set("name", Json::string(t.actor_name));
        e.set("params", Json::integer(p.params));
        e.set("x_rot", Json::integer(p.x_rot));
        e.set("z_rot", Json::integer(p.z_rot));
        e.set("category", Json::string(loc.category_raw));
        e.set("flags", Json::integer(p.flags));
        if (loc.flags & ELOC_POSITION) {
            e.set("pos", triple(loc.position));
        }
        if (loc.flags & ELOC_POSITION_OFFSET) {
            e.set("pos_offset", triple(loc.position_offset));
        }
        if (loc.flags & ELOC_Y_ROTATION) {
            e.set("y_rot", Json::integer(loc.y_rot));
        }
        if (loc.flags & ELOC_Y_ROTATION_OFFSET) {
            e.set("y_rot_offset", Json::integer(loc.y_rot_offset));
        }
        locs.set(loc.path.raw, std::move(e));
    }
    Json homen = Json::array();
    for (int i = 0; i < kNumHomen; i++) {
        homen.push(Json::integer(r.homen[i]));
    }
    Json out = Json::object();
    out.set("locations", std::move(locs));
    out.set("homen", std::move(homen));
    return out;
}

bool enemiesFromJson(const Json& j, EnemyRandoResult& out, std::string* err) {
    out = EnemyRandoResult();
    auto failWith = [&](const std::string& msg) {
        setErr(err, "enemies: " + msg);
        out = EnemyRandoResult();
        return false;
    };
    if (!j.isObject()) {
        return failWith("not an object");
    }
    const Json* locs = j.find("locations");
    const Json* homen = j.find("homen");
    if (locs == nullptr || !locs->isObject() || homen == nullptr || !homen->isArray() || homen->size() != kNumHomen) {
        return failWith("needs \"locations\" (an object) and \"homen\" (3 integers)");
    }
    Span<EnemyLocation> table = enemy_locations();
    for (size_t k = 0; k < locs->size(); k++) {
        const std::string& path = locs->keyAt(k);
        const Json& e = locs->at(k);
        int loc = -1;
        for (size_t i = 0; i < table.size; i++) {
            if (path == table[i].path.raw) {
                loc = (int)i;
                break;
            }
        }
        if (loc < 0) {
            return failWith("unknown enemy location " + path);
        }
        if (!e.isObject()) {
            return failWith(path + " is not an object");
        }
        const Json* type = e.find("type");
        const Json* name = e.find("name");
        const Json* params = e.find("params");
        const Json* xr = e.find("x_rot");
        const Json* zr = e.find("z_rot");
        const Json* flags = e.find("flags");
        if (type == nullptr || !type->isString() || name == nullptr || !name->isString() || params == nullptr ||
            !params->isInteger() || xr == nullptr || !xr->isInteger() || zr == nullptr || !zr->isInteger() ||
            flags == nullptr || !flags->isInteger()) {
            return failWith(path + ": needs type, name, params, x_rot, z_rot and flags");
        }
        EnemyPlacement p;
        p.location = loc;
        p.type = enemy_type_index(type->text().c_str());
        if (p.type < 0 || name->text() != enemy_types()[p.type].actor_name) {
            return failWith(path + ": unknown type or actor name");
        }
        const int64_t pv = params->asInt(), xv = xr->asInt(), zv = zr->asInt(), fv = flags->asInt();
        if (pv < 0 || pv > 0xFFFFFFFFll || xv < 0 || xv > 0xFFFF || zv < 0 || zv > 0xFFFF || fv < 0 || fv > 0x1F) {
            return failWith(path + ": a value is out of range");
        }
        p.params = (uint32_t)pv;
        p.x_rot = (uint16_t)xv;
        p.z_rot = (uint16_t)zv;
        p.flags = (uint8_t)fv;
        out.placements.push_back(p);
    }
    for (int i = 0; i < kNumHomen; i++) {
        const Json& h = homen->at(i);
        bool valid = h.isInteger();
        if (valid) {
            valid = false;
            for (int c : kHomenEnemyChoices) {
                valid = valid || h.asInt() == c;
            }
        }
        if (!valid) {
            return failWith("homen values must be enemy_to_spawn candidates");
        }
        out.homen[i] = (int)h.asInt();
    }
    return true;
}

} // namespace pcr
