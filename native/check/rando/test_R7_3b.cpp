// cos_rando_test_R7_3b: the enemy randomizer's logic filter (docs/RANDOMIZER_PLAN.md step R7.3b).
//
// Vectors from wwrando (fixtures/R7_3b_enemy_logic.txt, native/tools/rando/gen_enemy_logic_vectors.py):
//   - 4 option sets (default, all progression categories, Swordless, starting gear with No Starting
//     Sword): Logic.filter_out_enemies_that_add_new_requirements over the groups of
//     enemy_locations.txt in file order (must-defeat groups, the same again from the cache, then the
//     other groups) with enemies_to_randomize_to_when_all_enemies_must_be_killed. Per call: the
//     allowed list, the combo counts (or a cache answer), the raised error and CRC-32s of the unplaced
//     progress / nonprogress and owned lists; at the end the full lists (the item fill's input order);
//   - repr(parse_logic_expression(s)) of every requirement string (the guard's error text);
//   - get_all_item_combo_subsets_meeting_req on 10-14 item combos: counts and the 10,000-combo guard.
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

#include "pc_rando_enemy_data.h"
#include "pc_rando_gen_enemy_logic.h"
#include "pc_rando_logic.h"
#include "pc_rando_options.h"
#include "rando_test.h"

namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t b = 0;
    for (;;) {
        size_t e = s.find(sep, b);
        if (e == std::string::npos) {
            out.push_back(s.substr(b));
            return out;
        }
        out.push_back(s.substr(b, e - b));
        b = e + 1;
    }
}

std::string unescape(const std::string& e) {
    std::string u;
    for (size_t i = 0; i < e.size(); i++) {
        if (e[i] == '\\' && i + 1 < e.size() && e[i + 1] == 'n') {
            u.push_back('\n');
            i++;
        } else {
            u.push_back(e[i]);
        }
    }
    return u;
}

uint32_t crc32(const std::string& s) {
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : s) {
        c ^= b;
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

std::string names(pcr::Logic& L, const std::vector<int>& ids) {
    std::string s;
    for (size_t i = 0; i < ids.size(); i++) {
        s += (i ? "|" : "") + L.name(ids[i]);
    }
    return s;
}

std::string hex8(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%08x", v);
    return b;
}

std::string orderLine(pcr::Logic& L) {
    return hex8(crc32(names(L, L.unplacedProgressItems))) + "\t" + hex8(crc32(names(L, L.unplacedNonprogressItems))) +
           "\t" + hex8(crc32(names(L, L.currentlyOwnedItems)));
}

struct Call {
    int pass = 0, group = 0;
    bool throwable = false, bomb = false;
    std::string req;
    std::string allowed, combos, order, error;
    bool hasAllowed = false;
};

struct Case {
    std::string name;
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<Call> calls;
    std::vector<std::pair<std::string, std::string>> lists;
};

struct Fixture {
    std::vector<Case> cases;
    std::vector<std::pair<std::string, std::string>> reprs;
    std::vector<std::vector<std::string>> guards; // combo, req, result...
    std::vector<std::pair<std::string, std::string>> guardLists;
};

bool loadFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R7_3b_enemy_logic.txt", text)) {
        return false;
    }
    bool ended = false, inCase = false;
    for (const std::string& line : split(text, '\n')) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = split(line, '\t');
        auto arg = [&](size_t i) { return i < f.size() ? f[i] : std::string(); };
        const std::string& tag = f[0];
        if (tag == "version") {
            RT_CHECK_EQ(arg(1), std::string(pcr::kWwrandoVersion));
        } else if (tag == "case") {
            fx.cases.push_back(Case());
            fx.cases.back().name = arg(1);
            inCase = true;
        } else if (tag == "opt") {
            fx.cases.back().opts.emplace_back(arg(1), arg(2));
        } else if (tag == "call") {
            Call c;
            c.pass = std::atoi(arg(1).c_str());
            c.group = std::atoi(arg(2).c_str());
            c.throwable = arg(3) == "1";
            c.bomb = arg(4) == "1";
            c.req = arg(5);
            fx.cases.back().calls.push_back(c);
        } else if (tag == "allowed") {
            fx.cases.back().calls.back().allowed = arg(1);
            fx.cases.back().calls.back().hasAllowed = true;
        } else if (tag == "combos") {
            fx.cases.back().calls.back().combos = f.size() > 2 ? arg(1) + "\t" + arg(2) : arg(1);
        } else if (tag == "order") {
            fx.cases.back().calls.back().order = arg(1) + "\t" + arg(2) + "\t" + arg(3);
        } else if (tag == "error") {
            fx.cases.back().calls.back().error = unescape(arg(1));
        } else if (tag == "list") {
            if (inCase) {
                fx.cases.back().lists.emplace_back(arg(1), arg(2));
            } else {
                fx.guardLists.emplace_back(arg(1), arg(2));
            }
        } else if (tag == "endcase") {
            inCase = false;
        } else if (tag == "repr") {
            fx.reprs.emplace_back(arg(1), arg(2));
        } else if (tag == "guard") {
            fx.guards.push_back(std::vector<std::string>(f.begin() + 1, f.end()));
        } else if (tag == "end") {
            ended = true;
        } else {
            std::fprintf(stderr, "FAIL unknown fixture line: %s\n", line.c_str());
            return false;
        }
    }
    return RT_CHECK(ended);
}

bool buildOptions(const Case& c, pcr::Options& o) {
    o = pcr::Options();
    for (const auto& kv : c.opts) {
        int id = pcr::optionFind(kv.first.c_str());
        if (!RT_CHECK(id >= 0) || !RT_CHECK(pcr::optionFromString(o, id, kv.second.c_str()))) {
            std::fprintf(stderr, "  option %s=%s\n", kv.first.c_str(), kv.second.c_str());
            return false;
        }
    }
    pcr::validateOptions(o);
    return true;
}

std::vector<int>* listByName(pcr::Logic& L, const std::string& n) {
    if (n == "unplaced_progress_items" || n == "guard_unplaced_progress_items") {
        return &L.unplacedProgressItems;
    }
    if (n == "unplaced_nonprogress_items" || n == "guard_unplaced_nonprogress_items") {
        return &L.unplacedNonprogressItems;
    }
    if (n == "currently_owned_items") {
        return &L.currentlyOwnedItems;
    }
    return nullptr;
}

void checkCase(const Case& c, int* calls, int* cachedCalls) {
    pcr::Options o;
    if (!buildOptions(c, o)) {
        return;
    }
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    if (!RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init))) {
        std::fprintf(stderr, "  case %s: %s\n", c.name.c_str(), init.error.c_str());
        return;
    }
    const std::vector<int> pool = pcr::enemyTypesToRandomizeToWhenAllMustBeKilled();
    pcr::Span<pcr::EnemyGroup> groups = pcr::enemy_groups();
    pcr::Span<pcr::EnemyType> types = pcr::enemy_types();
    pcr::EnemyLogicFilter filter(L);
    int failedCalls = 0;
    for (const Call& call : c.calls) {
        const int before = rt::failures();
        // The data tables hold the same group fields wwrando passes.
        if (RT_CHECK(call.group >= 0 && call.group < (int)groups.size)) {
            const pcr::EnemyGroup& g = groups[call.group];
            RT_CHECK_EQ(std::string(g.original_req), call.req);
            RT_CHECK_EQ(g.has_throwable, call.throwable);
            RT_CHECK_EQ(g.has_bomb_flowers, call.bomb);
            RT_CHECK_EQ(g.must_defeat, call.pass != 3);
        }
        std::vector<int> allowed;
        std::string err;
        const bool ok = filter.filter(call.req, call.throwable, call.bomb, pool, allowed, &err);
        if (!call.error.empty()) {
            RT_CHECK(!ok);
            RT_CHECK_EQ(err, call.error);
        } else if (RT_CHECK(ok)) {
            std::string got;
            for (size_t i = 0; i < allowed.size(); i++) {
                got += (i ? "|" : "") + std::string(types[allowed[i]].pretty_name);
            }
            RT_CHECK_EQ(got, call.allowed);
            const std::string combos = filter.lastMatchedCombos() < 0
                                           ? std::string("cached")
                                           : std::to_string(filter.lastMatchedCombos()) + "\t" +
                                                 std::to_string(filter.lastCheckedCombos());
            RT_CHECK_EQ(combos, call.combos);
            if (call.combos == "cached") {
                (*cachedCalls)++;
            }
        } else {
            std::fprintf(stderr, "  error: %s\n", err.c_str());
        }
        RT_CHECK_EQ(orderLine(L), call.order);
        (*calls)++;
        if (rt::failures() != before) {
            std::fprintf(stderr, "  case %s pass %d group %d: %s\n", c.name.c_str(), call.pass, call.group,
                         call.req.c_str());
            if (++failedCalls >= 5) {
                std::fprintf(stderr, "  (stopping case %s)\n", c.name.c_str());
                return;
            }
        }
    }
    for (const auto& kv : c.lists) {
        std::vector<int>* v = listByName(L, kv.first);
        if (RT_CHECK(v != nullptr) && !RT_CHECK_EQ(names(L, *v), kv.second)) {
            std::fprintf(stderr, "  case %s list %s\n", c.name.c_str(), kv.first.c_str());
        }
    }
    RT_CHECK(!L.failed());
}

void testVectors(const Fixture& fx) {
    int calls = 0, cached = 0;
    for (const Case& c : fx.cases) {
        checkCase(c, &calls, &cached);
    }
    RT_CHECK_EQ((int)fx.cases.size(), 4);
    RT_CHECK(calls > 1000);
    RT_CHECK(cached > 500);
}

void testRepr(const Fixture& fx) {
    RT_CHECK(fx.reprs.size() > 100);
    for (const auto& kv : fx.reprs) {
        if (!RT_CHECK_EQ(pcr::pyReprLogicExpression(kv.first), kv.second)) {
            std::fprintf(stderr, "  repr of %s\n", kv.first.c_str());
        }
    }
}

void testGuard(const Fixture& fx) {
    pcr::Options o;
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    if (!RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init))) {
        return;
    }
    pcr::EnemyLogicFilter filter(L);
    RT_CHECK_EQ((int)fx.guards.size(), 5);
    for (const std::vector<std::string>& g : fx.guards) {
        if (!RT_CHECK(g.size() >= 3)) {
            continue;
        }
        std::vector<int> combo;
        for (const std::string& n : split(g[0], '|')) {
            combo.push_back(L.item(n));
        }
        const int expr = L.req().compile(g[1]);
        std::vector<std::vector<int>> matched;
        size_t checked = 0;
        std::string err;
        const bool ok = filter.combosMeetingReq(combo, expr, g[1], matched, &checked, &err);
        if (g[2] == "ok") {
            if (RT_CHECK(ok) && RT_CHECK(g.size() == 5)) {
                RT_CHECK_EQ(std::to_string(matched.size()), g[3]);
                RT_CHECK_EQ(std::to_string(checked), g[4]);
            }
        } else {
            RT_CHECK(!ok);
            RT_CHECK_EQ(err, unescape(g[2]));
        }
    }
    for (const auto& kv : fx.guardLists) {
        std::vector<int>* v = listByName(L, kv.first);
        if (RT_CHECK(v != nullptr) && !RT_CHECK_EQ(names(L, *v), kv.second)) {
            std::fprintf(stderr, "  guard list %s\n", kv.first.c_str());
        }
    }
    RT_CHECK(!L.failed());
}

void testLists() {
    pcr::Span<pcr::EnemyType> types = pcr::enemy_types();
    const std::vector<int> to = pcr::enemyTypesToRandomizeTo();
    const std::vector<int> must = pcr::enemyTypesToRandomizeToWhenAllMustBeKilled();
    RT_CHECK(!to.empty());
    RT_CHECK(must.size() < to.size());
    size_t j = 0;
    for (int t : to) {
        const std::string n = types[t].pretty_name;
        const bool banned = n.find("Morth") != std::string::npos || n == "Dexivine";
        if (!banned) {
            RT_CHECK(j < must.size() && must[j] == t);
            j++;
        }
    }
    RT_CHECK_EQ(j, must.size());
}

} // namespace

int main() {
    Fixture fx;
    if (loadFixture(fx)) {
        testLists();
        testRepr(fx);
        testGuard(fx);
        testVectors(fx);
    }
    return rt::finish();
}
