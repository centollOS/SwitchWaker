// cos_rando_test_R1_1: the requirement compiler and evaluator (docs/RANDOMIZER_PLAN.md step R1.1).
//
// 1. Vectors from wwrando (fixtures/R1_1_logic_vectors.txt, native/tools/rando/gen_logic_vectors.py):
//    4 option sets x 30 inventories x 320 locations. Per inventory, met/not met and items_needed of
//    every location, items_needed of "Can Reach and Defeat Ganondorf" and the met result of every
//    macro match Python, in wwrando's call order (so cache-dependent results match too), and where
//    wwrando raised, the evaluator fails at the same call with the same message.
// 2. Synthetic cases: grammar errors, the recursion guard and assert, the AND/OR items_needed union,
//    set_macro, snapshots.
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_data.h"
#include "pc_rando_logic_expr.h"
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

struct Inventory {
    std::string counts; // "<name id>*<n>,..."
    std::string locs;
    std::string needs;
    int gan = -1;
    std::string mac;
    std::string error;
};

struct Set {
    std::string label;
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<std::string> items;
    std::vector<std::pair<std::string, std::string>> macros;
    std::vector<std::pair<std::string, std::string>> nested;
    std::vector<Inventory> invs;
};

struct Fixture {
    std::vector<std::string> names;
    std::vector<std::string> needs;
    std::vector<Set> sets;
};

bool loadFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R1_1_logic_vectors.txt", text)) {
        return false;
    }
    bool ended = false;
    for (const std::string& line : split(text, '\n')) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = split(line, '\t');
        const std::string& tag = f[0];
        if (tag == "version") {
            RT_CHECK_EQ(f[1], std::string(pcr::kWwrandoVersion));
        } else if (tag == "name") {
            RT_CHECK_EQ((size_t)std::stoi(f[1]), fx.names.size());
            fx.names.push_back(f[2]);
        } else if (tag == "need") {
            RT_CHECK_EQ((size_t)std::stoi(f[1]), fx.needs.size());
            fx.needs.push_back(f.size() > 2 ? f[2] : std::string());
        } else if (tag == "set") {
            fx.sets.push_back(Set());
            fx.sets.back().label = f[2];
        } else if (tag == "opt") {
            fx.sets.back().opts.emplace_back(f[1], f.size() > 2 ? f[2] : std::string());
        } else if (tag == "item") {
            fx.sets.back().items.push_back(f[1]);
        } else if (tag == "macro") {
            fx.sets.back().macros.emplace_back(f[1], f.size() > 2 ? f[2] : std::string());
        } else if (tag == "nested") {
            fx.sets.back().nested.emplace_back(f[1], f[2]);
        } else if (tag == "inv") {
            fx.sets.back().invs.push_back(Inventory());
            fx.sets.back().invs.back().counts = f.size() > 2 ? f[2] : std::string();
        } else if (tag == "locs") {
            fx.sets.back().invs.back().locs = f.size() > 1 ? f[1] : std::string();
        } else if (tag == "needs") {
            fx.sets.back().invs.back().needs = f.size() > 1 ? f[1] : std::string();
        } else if (tag == "gan") {
            fx.sets.back().invs.back().gan = std::stoi(f[1]);
        } else if (tag == "mac") {
            fx.sets.back().invs.back().mac = f.size() > 1 ? f[1] : std::string();
        } else if (tag == "error") {
            fx.sets.back().invs.back().error = f[1];
        } else if (tag == "end") {
            ended = true;
        } else {
            std::fprintf(stderr, "FAIL unknown fixture line: %s\n", line.c_str());
            return false;
        }
    }
    return RT_CHECK(ended);
}

// "<name id>*<n>,..." -> ItemsNeeded in the evaluator's ids. False when a name was never interned
// (the evaluator cannot have produced it).
bool toNeeded(const Fixture& fx, const pcr::ReqLogic& L, const std::string& s, pcr::ItemsNeeded& out) {
    out.clear();
    if (s.empty()) {
        return true;
    }
    for (const std::string& p : split(s, ',')) {
        size_t star = p.find('*');
        int id = L.find(fx.names[std::stoi(p.substr(0, star))]);
        if (id < 0) {
            return false;
        }
        out.push_back(pcr::ItemCount{id, std::stoi(p.substr(star + 1))});
    }
    return true;
}

std::string showNeeded(const pcr::ReqLogic& L, const pcr::ItemsNeeded& n) {
    std::string s = "{";
    for (const pcr::ItemCount& c : n) {
        s += L.name(c.name) + ":" + std::to_string(c.count) + ", ";
    }
    return s + "}";
}

int gMismatches = 0;

bool sameNeeded(const Fixture& fx, const pcr::ReqLogic& L, int needId, const pcr::ItemsNeeded& got,
                const char* what) {
    pcr::ItemsNeeded want;
    bool ok = toNeeded(fx, L, fx.needs[needId], want) && want == got;
    if (!ok && gMismatches++ < 10) {
        std::fprintf(stderr, "FAIL %s: items needed %s, want need %d (%s)\n", what, showNeeded(L, got).c_str(),
                     needId, fx.needs[needId].c_str());
    }
    return ok;
}

void runSet(const Fixture& fx, const Set& set) {
    pcr::Options opts;
    for (const auto& o : set.opts) {
        int id = pcr::optionFind(o.first.c_str());
        RT_CHECK(id >= 0 && pcr::optionFromString(opts, id, o.second.c_str()));
    }
    pcr::ReqLogic L;
    RT_CHECK(L.init(opts, set.items));
    RT_CHECK_EQ((int)L.itemNames().size(), (int)set.items.size());
    for (const auto& m : set.macros) {
        RT_CHECK(L.setMacro(m.first, m.second));
    }
    for (const auto& n : set.nested) {
        L.setNestedEntranceMacro(n.first, n.second);
    }
    RT_CHECK(!L.failed());
    const int nLocs = L.locationCount();
    RT_CHECK_EQ(nLocs, 320);

    int checked = 0, metCount = 0, errors = 0;
    for (size_t k = 0; k < set.invs.size(); k++) {
        const Inventory& inv = set.invs[k];
        char what[160];
        L.clearError();
        L.clearOwned();
        if (!inv.counts.empty()) {
            for (const std::string& p : split(inv.counts, ',')) {
                size_t star = p.find('*');
                int id = L.find(fx.names[std::stoi(p.substr(0, star))]);
                RT_CHECK(L.isItem(id));
                L.setOwned(id, std::stoi(p.substr(star + 1)));
            }
        }
        L.clearCaches();
        std::vector<std::string> needIds = inv.needs.empty() ? std::vector<std::string>() : split(inv.needs, ',');
        RT_CHECK_EQ(needIds.size(), inv.locs.size());

        // Locations, then Ganondorf, then every macro: wwrando's order (stop at its error).
        std::string bits;
        pcr::ItemsNeeded need;
        bool stopped = false;
        for (int i = 0; i < nLocs && !stopped; i++) {
            bool met = L.checkLocationAccessible(i);
            L.itemsNeededForLocation(i, need);
            if (L.failed()) {
                stopped = true;
                break;
            }
            bits += met ? '1' : '0';
            if ((size_t)i < needIds.size()) {
                std::snprintf(what, sizeof what, "set %s inv %zu loc %d (%s)", set.label.c_str(), k, i,
                              pcr::locations()[i].name);
                RT_CHECK(sameNeeded(fx, L, std::stoi(needIds[i]), need, what));
                if ((size_t)i < inv.locs.size() && inv.locs[i] != bits.back() && gMismatches++ < 10) {
                    std::fprintf(stderr, "FAIL %s: met %c, want %c\n", what, bits.back(), inv.locs[i]);
                }
            }
            metCount += met ? 1 : 0;
            checked++;
        }
        RT_CHECK_EQ(bits, inv.locs);
        if (!stopped) {
            L.itemsNeededByReq("Can Reach and Defeat Ganondorf", need);
            if (L.failed()) {
                stopped = true;
            } else {
                std::snprintf(what, sizeof what, "set %s inv %zu Ganondorf", set.label.c_str(), k);
                RT_CHECK(inv.gan >= 0 && sameNeeded(fx, L, inv.gan, need, what));
            }
        }
        std::string mac;
        if (!stopped) {
            for (int m : L.macroOrder()) {
                bool met = L.checkRequirementMet(m);
                if (L.failed()) {
                    stopped = true;
                    break;
                }
                mac += met ? '1' : '0';
            }
        }
        RT_CHECK_EQ(mac, inv.mac);
        if (inv.error.empty()) {
            RT_CHECK(!stopped);
            if (L.failed()) {
                std::fprintf(stderr, "  error: %s\n", L.error().c_str());
            }
        } else {
            errors++;
            RT_CHECK(stopped);
            // Python's text is "<ExceptionType>: <message>".
            const std::string& e = inv.error;
            bool same = e.size() >= L.error().size() && !L.error().empty() &&
                        e.compare(e.size() - L.error().size(), std::string::npos, L.error()) == 0;
            if (!RT_CHECK(same)) {
                std::fprintf(stderr, "  error %s, want %s\n", L.error().c_str(), e.c_str());
            }
        }
    }
    std::printf("set %-34s %zu inventories, %d location checks (%d met), %d raised\n", set.label.c_str(),
                set.invs.size(), checked, metCount, errors);
}

void syntheticChecks() {
    pcr::Options opts;
    std::vector<std::string> items = {"A", "B", "C", "Progressive Sword", "DRC Small Key"};
    pcr::ReqLogic L;
    RT_CHECK(L.init(opts, items));
    const int A = L.find("A"), B = L.find("B"), C = L.find("C"), PS = L.find("Progressive Sword");

    // Constants and the token order.
    RT_CHECK(L.checkExpr(L.compile("Nothing")));
    RT_CHECK(!L.checkExpr(L.compile("Impossible")));
    RT_CHECK(L.checkExpr(L.compile("( )"))); // all([]) is True
    RT_CHECK(L.checkExpr(L.compile("Option \"sword_mode\" Is \"Start with Hero's Sword\"")));
    RT_CHECK(L.checkExpr(L.compile("Option \"sword_mode\" Is Not \"Swordless\"")));
    RT_CHECK(L.checkExpr(L.compile("Option \"progression_dungeons\" Enabled")));
    RT_CHECK(L.checkExpr(L.compile("Option \"keylunacy\" Disabled")));
    RT_CHECK(L.checkExpr(L.compile("Option \"starting_gear\" Contains \"Ballad of Gales\"")));
    RT_CHECK(!L.checkExpr(L.compile("Option \"starting_gear\" Does Not Contain \"Ballad of Gales\"")));
    RT_CHECK(!L.checkExpr(L.compile("Option \"num_required_bosses\" Is \"4\""))); // int != str
    RT_CHECK(!L.failed());

    // AND / OR, every child checked; repeated and trailing operators are tolerated as in Python.
    L.setOwned(A, 1);
    RT_CHECK(L.checkExpr(L.compile("A | B")));
    RT_CHECK(!L.checkExpr(L.compile("A & B")));
    RT_CHECK(L.checkExpr(L.compile("A & & (B | Nothing) &")));
    RT_CHECK(!L.checkExpr(L.compile("Progressive Sword x2 | B")));
    L.setOwned(PS, 2);
    RT_CHECK(L.checkExpr(L.compile("Progressive Sword x2 | B")));
    RT_CHECK(!L.checkExpr(L.compile("Progressive Sword x3")));
    RT_CHECK(!L.checkExpr(L.compile("DRC Small Key x1")));
    RT_CHECK(!L.failed());

    // items_needed: {} when met; otherwise the union of every child with max counts, AND and OR alike,
    // in insertion order; owned items are not removed.
    pcr::ItemsNeeded n;
    L.itemsNeededForExpr(L.compile("A | B"), n);
    RT_CHECK(n.empty());
    L.itemsNeededForExpr(L.compile("B | Progressive Sword x4 | (A & Progressive Sword x3) | C"), n);
    pcr::ItemsNeeded want = {{B, 1}, {PS, 4}, {A, 1}, {C, 1}};
    RT_CHECK(n == want);
    // A met group inside an unmet expression contributes nothing.
    L.itemsNeededForExpr(L.compile("B & (A | C)"), n);
    want = {{B, 1}};
    RT_CHECK(n == want);
    std::vector<int> flat;
    pcr::ReqLogic::flatten(pcr::ItemsNeeded{{PS, 2}, {B, 1}}, flat);
    RT_CHECK((flat == std::vector<int>{PS, PS, B}));

    // set_macro recompiles and drops the caches; macro bodies are kept.
    RT_CHECK(L.setMacro("M", "B"));
    RT_CHECK(!L.checkRequirementMet("M"));
    RT_CHECK(L.setMacro("M", "A"));
    RT_CHECK(L.checkRequirementMet("M"));
    RT_CHECK(*L.macroBody(L.find("M")) == "A");
    std::vector<int> top;
    L.topLevelTokens(L.macroExpr(L.find("Chart for Island 1")), top);
    RT_CHECK(!top.empty() && L.name(top[0]).rfind("Treasure Chart", 0) == 0);

    // Snapshots restore the counts and the caches.
    pcr::ReqLogic::Snapshot s = L.snapshot();
    L.addOwned(B);
    RT_CHECK(L.checkExpr(L.compile("A & B")));
    L.restore(s);
    RT_CHECK(!L.checkExpr(L.compile("A & B")));
    RT_CHECK_EQ(L.owned(B), 0);
    RT_CHECK(!L.failed());

    // Errors where wwrando raises.
    L.clearError();
    RT_CHECK(!L.checkExpr(L.compile("A & B | C")));
    RT_CHECK(L.failed());
    L.clearError();
    RT_CHECK(L.compile("(A & B") < 0);
    RT_CHECK(L.failed());
    L.clearError();
    RT_CHECK(L.compile("A & B)") < 0);
    L.clearError();
    L.checkExpr(L.compile("(A) (B)"));
    RT_CHECK(L.failed());
    L.clearError();
    L.checkRequirementMet("No Such Thing");
    RT_CHECK(L.failed() && L.error() == "Unknown requirement name: No Such Thing");
    L.clearError();
    L.checkRequirementMet("Option \"not_an_option\" Enabled");
    RT_CHECK(L.failed());
    L.clearError();
    L.checkRequirementMet("Can Access Item Location \"Nowhere\"");
    RT_CHECK(L.failed());
    L.clearError();
    L.addOwned(L.intern("Not An Item"));
    RT_CHECK(L.failed());
    L.clearError();

    // Recursion: asserted unless whitelisted by the nested-entrance map, which returns True / {}.
    RT_CHECK(L.setMacro("X", "Y"));
    RT_CHECK(L.setMacro("Y", "X"));
    L.checkRequirementMet("X");
    RT_CHECK(L.failed() && L.error().find("Recursive requirement check") == 0);
    L.clearError();
    RT_CHECK(L.setMacro("Can Access Zone", "Can Access Entrance & B"));
    RT_CHECK(L.setMacro("Can Access Entrance", "Can Access Zone | C"));
    L.setNestedEntranceMacro("Can Access Entrance", "Can Access Zone");
    RT_CHECK(!L.checkRequirementMet("Can Access Zone")); // Entrance is True through the guard; B missing
    RT_CHECK(!L.failed());
    L.itemsNeededByReq("Can Access Zone", n);
    want = {{B, 1}};
    RT_CHECK(n == want);
    L.addOwned(B);
    RT_CHECK(L.checkRequirementMet("Can Access Zone"));
    // From the other side the guard is checked before the assert: Entrance -> Zone -> Entrance is
    // whitelisted (its outer zone is on the stack) and returns True.
    L.clearCaches();
    RT_CHECK(L.checkRequirementMet("Can Access Entrance"));
    RT_CHECK(!L.failed());
    // Without the map, Zone -> Entrance -> Zone asserts.
    L.clearNestedEntranceMacros();
    L.clearCaches();
    L.checkRequirementMet("Can Access Zone");
    RT_CHECK(L.failed());
}

} // namespace

int main() {
    syntheticChecks();

    Fixture fx;
    if (loadFixture(fx)) {
        RT_CHECK_EQ(fx.sets.size(), (size_t)4);
        auto t0 = std::chrono::steady_clock::now();
        for (const Set& s : fx.sets) {
            RT_CHECK_EQ(s.invs.size(), (size_t)30);
            runSet(fx, s);
        }
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("vectors checked in %.0f ms\n", ms);
    }
    return rt::finish();
}
