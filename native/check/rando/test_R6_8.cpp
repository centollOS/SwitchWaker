// cos_rando_test_R6_8: required bosses mode, generator half (docs/RANDOMIZER_PLAN.md step R6.8).
//
// 1. Vectors from wwrando (fixtures/R6_8_bossreqs.txt, native/tools/rando/gen_bossreq_vectors.py):
//    for 22 cases (3 seeds x num_required_bosses 1-6 at the defaults, all categories with 1/3/5,
//    key-lunacy with 2) RequiredBossesRandomizer.randomize() gives the same required boss item
//    locations (sample order), required/banned dungeons and bosses, banned locations and
//    "Can Defeat All Required Bosses" macro; where WWRandomizer.__init__ raises
//    TooFewProgressionLocationsError the port raises the same text.
// 2. The errors randomize_required_bosses raises, and generate() end to end: the placement's
//    required_boss_locations and the spoiler log's required / non-required dungeon sections.
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_gen.h"
#include "pc_rando_gen_bosses.h"
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

std::string join(const std::vector<std::string>& v, const char* sep) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        s += (i ? sep : "") + v[i];
    }
    return s;
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
    std::vector<std::pair<std::string, std::string>> lists;
    std::string bannedLocs;
    std::string macro;
    std::string initError, error;
};

bool loadFixture(std::vector<Case>& out) {
    std::string text;
    if (!rt::readFixture("R6_8_bossreqs.txt", text)) {
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
        } else if (tag == "list") {
            out.back().lists.emplace_back(arg(1), arg(2));
        } else if (tag == "locs") {
            out.back().bannedLocs = arg(2);
        } else if (tag == "macro") {
            out.back().macro = arg(1);
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
        if (!RT_CHECK(!ok) || !RT_CHECK(init.tooFewProgressionLocations) || !RT_CHECK_EQ(init.error, c.initError)) {
            std::fprintf(stderr, "  case %s\n", c.seed.c_str());
        }
        (*checked)++;
        return;
    }
    if (!RT_CHECK(ok)) {
        std::fprintf(stderr, "  case %s: %s\n", c.seed.c_str(), init.error.c_str());
        return;
    }
    pc::rando::PyRandom rng;
    pcr::newRandomizerRng(rng, c.seed, o);
    pcr::RequiredBosses rb;
    std::string err;
    const bool rok = pcr::randomizeRequiredBosses(L, rng, rb, &err);
    if (!c.error.empty()) {
        RT_CHECK(!rok);
        RT_CHECK_EQ(err, c.error);
        (*checked)++;
        return;
    }
    if (!RT_CHECK(rok)) {
        std::fprintf(stderr, "  case %s: %s\n", c.seed.c_str(), err.c_str());
        return;
    }
    const int before = rt::failures();
    for (const auto& kv : c.lists) {
        const std::vector<std::string>* got = nullptr;
        if (kv.first == "required_boss_item_locations") {
            got = &rb.requiredBossItemLocations;
        } else if (kv.first == "required_dungeons") {
            got = &rb.requiredDungeons;
        } else if (kv.first == "required_bosses") {
            got = &rb.requiredBosses;
        } else if (kv.first == "banned_dungeons") {
            got = &rb.bannedDungeons;
        } else if (kv.first == "banned_bosses") {
            got = &rb.bannedBosses;
        }
        if (!RT_CHECK(got != nullptr)) {
            continue;
        }
        if (!RT_CHECK_EQ(join(*got, "|"), kv.second)) {
            std::fprintf(stderr, "  list %s\n", kv.first.c_str());
        }
    }
    std::string locs;
    for (size_t i = 0; i < L.state().bannedLocations.size(); i++) {
        locs += (i ? "," : "") + std::to_string(L.state().bannedLocations[i]);
    }
    RT_CHECK_EQ(locs, c.bannedLocs);
    RT_CHECK_EQ(rb.bannedLocations.size(), L.state().bannedLocations.size());
    for (size_t i = 0; i < rb.bannedLocations.size() && i < L.state().bannedLocations.size(); i++) {
        RT_CHECK_EQ(rb.bannedLocations[i], std::string(L.locationName(L.state().bannedLocations[i])));
    }
    RT_CHECK(L.state().requiredBossItemLocations == rb.requiredBossItemLocations);
    const std::string* body = L.req().macroBody(L.req().find("Can Defeat All Required Bosses"));
    if (RT_CHECK(body != nullptr)) {
        RT_CHECK_EQ(*body, c.macro);
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
    int checked = 0;
    for (const Case& c : cases) {
        checkCase(c, &checked);
    }
    RT_CHECK_EQ(checked, 22);
    RT_CHECK_EQ((int)cases.size(), 22);
}

void testErrors() {
    // randomize_required_bosses with progression_dungeons off (the options' validation normally
    // turns the mode off first; the randomizer itself still refuses).
    pcr::Options o;
    o.required_bosses = true;
    o.num_required_bosses = 4;
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init));
    pcr::Options off = o;
    off.progression_dungeons = false;
    pcr::Logic L2;
    if (RT_CHECK(L2.create(off, pcr::defaultLogicState(off)))) {
        pc::rando::PyRandom rng;
        pcr::newRandomizerRng(rng, "X", off);
        pcr::RequiredBosses rb;
        std::string err;
        RT_CHECK(!pcr::randomizeRequiredBosses(L2, rng, rb, &err));
        RT_CHECK_EQ(err, std::string("Cannot make bosses required when progression dungeons are disabled."));
        RT_CHECK(L2.state().bannedLocations.empty());
    }
}

void testGenerate() {
    for (int n = 2; n <= 6; n++) {
        pcr::GenRequest req;
        req.seed = "BossReqGen";
        req.allowMissingHints = true;
        req.options.required_bosses = true;
        req.options.num_required_bosses = n;
        pcr::GenResult res;
        if (!RT_CHECK(pcr::generate(req, res))) {
            std::fprintf(stderr, "  n=%d: %s\n", n, res.error.c_str());
            continue;
        }
        RT_CHECK_EQ((int)res.seed.required_boss_locations.size(), n);
        RT_CHECK(res.spoilerLog.find("Required dungeons:\n") != std::string::npos);
        RT_CHECK(res.spoilerLog.find("\n\nNon-required dungeons:\n") != std::string::npos);
        // Each required boss location's dungeon is listed under "Required dungeons".
        const size_t a = res.spoilerLog.find("Required dungeons:\n");
        const size_t b = res.spoilerLog.find("Non-required dungeons:\n");
        int required = 0;
        for (const std::string& loc : res.seed.required_boss_locations) {
            const std::string zone = loc.substr(0, loc.find(" - "));
            const size_t at = res.spoilerLog.find("  " + zone + "\n", a);
            RT_CHECK(at != std::string::npos && at < b);
            required++;
        }
        RT_CHECK_EQ(required, n);
        // The placement's banned dungeons hold no progress item (the fill skipped them); the spheres
        // never list a location of a non-required dungeon (the spheres group by zone: "  <zone>:").
        const size_t sp = res.spoilerLog.find("Playthrough progression spheres:");
        const size_t al = res.spoilerLog.find("All item locations:");
        RT_CHECK(sp != std::string::npos && al != std::string::npos && sp < al);
        // A required dungeon does show up (the check below would pass vacuously otherwise).
        const std::string first = res.seed.required_boss_locations[0];
        RT_CHECK(res.spoilerLog.substr(sp, al - sp).find("\n  " + first.substr(0, first.find(" - ")) + ":\n") !=
                 std::string::npos);
        const std::string spheres = res.spoilerLog.substr(sp, al - sp);
        const std::string non = res.spoilerLog.substr(b, sp - b);
        for (const std::string& line : split(non, '\n')) {
            if (line.size() > 2 && line[0] == ' ' && line[1] == ' ') {
                const std::string zone = line.substr(2);
                if (!RT_CHECK(spheres.find("\n  " + zone + ":\n") == std::string::npos)) {
                    std::fprintf(stderr, "  n=%d: banned %s in the spheres\n", n, zone.c_str());
                }
            }
        }
    }
    // Off: no required boss locations and no section.
    pcr::GenRequest req;
    req.seed = "BossReqGen";
    req.allowMissingHints = true;
    pcr::GenResult res;
    if (RT_CHECK(pcr::generate(req, res))) {
        RT_CHECK(res.seed.required_boss_locations.empty());
        RT_CHECK(res.spoilerLog.find("Required dungeons:") == std::string::npos);
    }
}

} // namespace

int main() {
    testVectors();
    testErrors();
    testGenerate();
    return rt::finish();
}
