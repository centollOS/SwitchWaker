// cos_rando_test_R6_11: entrance randomizer, placement and logic (docs/RANDOMIZER_PLAN.md step R6.11).
//
// 1. Vectors from wwrando (fixtures/R6_11_entrances.txt, native/tools/rando/gen_entrance_vectors.py):
//    for 26 cases (each randomize_*_entrances option alone, all of them separate and mixed, all
//    categories, required bosses with randomized and with vanilla dungeon entrances, dungeons-only
//    and dungeons-and-caves-only starts, i.e. the safety entrance) EntranceRandomizer.randomize()
//    gives the same entrance_connections (dict order), nested_entrance_paths, nesting_enabled and
//    "Can Access <exit>" macros; where it raises (inner caves alone: "Not enough island entrances
//    left to split entrances.") the port fails with the same text.
// 2. generate() end to end: the placement's "entrances", the spoiler log's Entrances and Nested
//    entrance paths sections, no refusal, and the raise reaching GenResult::error.
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_entrances.h"
#include "pc_rando_gen.h"
#include "pc_rando_gen_bosses.h"
#include "pc_rando_gen_entrances.h"
#include "pc_rando_gen_items.h"
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

struct Case {
    std::string seed;
    std::vector<std::pair<std::string, std::string>> opts;
    std::string conn, paths;
    bool nesting = false;
    std::vector<std::pair<std::string, std::string>> macros;
    std::string initError, error;
};

bool loadFixture(std::vector<Case>& out) {
    std::string text;
    if (!rt::readFixture("R6_11_entrances.txt", text)) {
        return false;
    }
    bool ended = false;
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
            out.push_back(Case());
            out.back().seed = arg(1);
        } else if (tag == "opt") {
            out.back().opts.emplace_back(arg(1), arg(2));
        } else if (tag == "conn") {
            out.back().conn = arg(1);
        } else if (tag == "paths") {
            out.back().paths = arg(1);
        } else if (tag == "nesting") {
            out.back().nesting = arg(1) == "1";
        } else if (tag == "macro") {
            out.back().macros.emplace_back(arg(1), arg(2));
        } else if (tag == "initerror") {
            out.back().initError = unescape(arg(1));
        } else if (tag == "error") {
            out.back().error = unescape(arg(1));
        } else if (tag == "endcase") {
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

void checkCase(const Case& c, int* checked) {
    pcr::Options o;
    if (!buildOptions(c, o)) {
        return;
    }
    pcr::LogicState st = pcr::defaultLogicState(o);
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    const bool ok = pcr::initRandomizerLogic(L, o, st, init);
    if (!c.initError.empty()) {
        if (!RT_CHECK(!ok) || !RT_CHECK_EQ(init.error, c.initError)) {
            std::fprintf(stderr, "  case %s\n", c.seed.c_str());
        }
        (*checked)++;
        return;
    }
    if (!RT_CHECK(ok)) {
        std::fprintf(stderr, "  case %s: %s\n", c.seed.c_str(), init.error.c_str());
        return;
    }
    std::string err;
    pcr::RequiredBosses rb;
    if (L.state().bossReqsEnabled) {
        pc::rando::PyRandom rng;
        pcr::newRandomizerRng(rng, c.seed, o);
        if (!RT_CHECK(pcr::randomizeRequiredBosses(L, rng, rb, &err))) {
            std::fprintf(stderr, "  case %s: %s\n", c.seed.c_str(), err.c_str());
            return;
        }
    }
    pc::rando::PyRandom rng;
    pcr::newRandomizerRng(rng, c.seed, o);
    pcr::EntranceRandoResult er;
    const bool eok = pcr::randomizeEntrances(L, o, rng, rb, init.dungeonsAndCavesOnlyStart, er, &err);
    if (!c.error.empty()) {
        if (!RT_CHECK(!eok) || !RT_CHECK_EQ(err, c.error)) {
            std::fprintf(stderr, "  case %s\n", c.seed.c_str());
        }
        (*checked)++;
        return;
    }
    if (!RT_CHECK(eok)) {
        std::fprintf(stderr, "  case %s: %s\n", c.seed.c_str(), err.c_str());
        return;
    }
    const int before = rt::failures();
    std::string conn;
    for (size_t i = 0; i < L.state().entranceConnections.size(); i++) {
        conn += (i ? "|" : "") + L.state().entranceConnections[i].second;
    }
    RT_CHECK_EQ(conn, c.conn);
    std::string paths;
    for (size_t i = 0; i < er.nestedEntrancePaths.size(); i++) {
        paths += i ? "|" : "";
        for (size_t j = 0; j < er.nestedEntrancePaths[i].size(); j++) {
            paths += (j ? ">" : "") + er.nestedEntrancePaths[i][j];
        }
    }
    RT_CHECK_EQ(paths, c.paths);
    RT_CHECK_EQ(er.nestingEnabled, c.nesting);
    for (const auto& m : c.macros) {
        const std::string* body = L.req().macroBody(L.req().find("Can Access " + m.first));
        if (!RT_CHECK(body != nullptr) || !RT_CHECK_EQ(*body, m.second)) {
            std::fprintf(stderr, "  macro Can Access %s\n", m.first.c_str());
        }
    }
    // The connections are a bijection onto the 44 exits.
    std::vector<int> seen(pcr::kNumZoneExits, 0);
    for (const auto& p : L.state().entranceConnections) {
        const int ex = pcr::zoneExitIndex(p.second);
        if (RT_CHECK(ex >= 0)) {
            seen[ex]++;
        }
    }
    for (int n : seen) {
        RT_CHECK_EQ(n, 1);
    }
    if (rt::failures() != before) {
        std::fprintf(stderr, "  case %s\n", c.seed.c_str());
    }
    (*checked)++;
}

void testVectors() {
    std::vector<Case> cases;
    if (!loadFixture(cases)) {
        return;
    }
    RT_CHECK(cases.size() >= 20);
    int checked = 0, errors = 0;
    for (const Case& c : cases) {
        checkCase(c, &checked);
        errors += !c.error.empty();
    }
    RT_CHECK_EQ(checked, (int)cases.size());
    RT_CHECK(errors >= 1); // the raise is covered
}

pcr::GenRequest request(const char* seed) {
    pcr::GenRequest req;
    req.seed = seed;
    req.allowMissingHints = true;
    req.version = "test";
    return req;
}

void setAllEntrances(pcr::Options& o, bool mix) {
    o.randomize_dungeon_entrances = true;
    o.randomize_secret_cave_entrances = true;
    o.randomize_miniboss_entrances = true;
    o.randomize_boss_entrances = true;
    o.randomize_secret_cave_inner_entrances = true;
    o.randomize_fairy_fountain_entrances = true;
    if (mix) {
        o.mix_entrances = pcr::EntranceMixMode::MixDungeons;
    }
}

void testGenerate() {
    // Every entrance type, mixed: the placement carries the shuffled connections and the spoiler
    // log both sections.
    pcr::GenRequest req = request("EntA_mix");
    setAllEntrances(req.options, true);
    pcr::GenResult res;
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  %s\n", res.error.c_str());
        return;
    }
    RT_CHECK(!res.unsupported);
    RT_CHECK(res.spoilerLog.find("Entrances:\n") != std::string::npos);
    RT_CHECK(res.spoilerLog.find("\nNested entrance paths:\n") != std::string::npos);
    // The placement JSON lists 44 entrances; at least one differs from vanilla.
    int moved = 0;
    for (int en = 0; en < pcr::kNumZoneEntrances; en++) {
        const pcr::ZoneEntranceDef& e = pcr::zoneEntrances()[en];
        const std::string vanilla = pcr::zoneExits()[pcr::defaultConnectionExit(en)].name;
        const std::string key = std::string("\"") + e.name + "\": \"";
        const size_t at = res.placementJson.find(key);
        if (!RT_CHECK(at != std::string::npos)) {
            continue;
        }
        const size_t b = at + key.size();
        const std::string got = res.placementJson.substr(b, res.placementJson.find('"', b) - b);
        RT_CHECK(pcr::zoneExitIndex(got) >= 0);
        moved += got != vanilla;
        RT_CHECK(res.spoilerLog.find(std::string("  ") + e.name + ":") != std::string::npos);
    }
    RT_CHECK(moved > 0);

    // Dungeon entrances only, separate: no nesting section.
    pcr::GenRequest req2 = request("EntA_dungeon");
    req2.options.randomize_dungeon_entrances = true;
    pcr::GenResult res2;
    RT_CHECK(pcr::generate(req2, res2));
    RT_CHECK(res2.spoilerLog.find("Nested entrance paths:") == std::string::npos);

    // Inner caves alone: wwrando's raise, as the error (not a refusal).
    pcr::GenRequest req3 = request("EntA_secret_cave_inner");
    req3.options.randomize_secret_cave_inner_entrances = true;
    pcr::GenResult res3;
    RT_CHECK(!pcr::generate(req3, res3));
    RT_CHECK(!res3.unsupported);
    RT_CHECK_EQ(res3.error, std::string("Not enough island entrances left to split entrances."));
    RT_CHECK(pcr::unsupportedOptionsError(req3.options, true).empty());

    // Off: the vanilla connections and no draw (the default seed is unchanged by this step).
    pcr::GenResult res4;
    RT_CHECK(pcr::generate(request("EntA_off"), res4));
    for (int en = 0; en < pcr::kNumZoneEntrances; en++) {
        const std::string key = std::string("\"") + pcr::zoneEntrances()[en].name + "\": \"" +
                                pcr::zoneExits()[pcr::defaultConnectionExit(en)].name + "\"";
        RT_CHECK(res4.placementJson.find(key) != std::string::npos);
    }
}

} // namespace

int main() {
    testVectors();
    testGenerate();
    return rt::finish();
}
