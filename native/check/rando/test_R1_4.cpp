// cos_rando_test_R1_4: the item fill (docs/RANDOMIZER_PLAN.md step R1.4).
//
// 1. Golden placements from wwrando (fixtures/R1_4_fill.txt, native/tools/rando/gen_fill_vectors.py):
//    for the plan's 10 default seeds and 9 presets (key-lunacy, every category, dungeons-and-caves-only
//    start with its DRC failsafe, no dungeons, swordless, logic difficulty, a do_not_generate_spoiler_log
//    seed with the port's key) our ItemRandomizer fills every one of the 320 locations with the item
//    wwrando places there, after the same number of RNG draws.
// 2. Pool integrity after each fill: nothing left unplaced, no location left, all progress items placed.
// 3. Synthetic checks: rngChangingOptionValues, the trace format, an error on a broken Logic.
//
// A mismatch prints the first differing locations. To find the first diverging draw:
//   COS_RANDO_TRACE=/tmp/ours.trace build/native-mac/cos_rando_test_R1_4
//   python3 -I native/tools/rando/gen_fill_vectors.py /tmp/x.txt --traces /tmp/py
// then diff the case's section of /tmp/ours.trace against /tmp/py/<label>.trace.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_gen_items.h"
#include "pc_rando_logic.h"
#include "pc_rando_options.h"
#include "pc_rando_rng.h"
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

struct Case {
    std::string label, seed, key;
    bool hasKey = false;
    std::vector<std::pair<std::string, std::string>> opts;
    long draws = -1;
    std::vector<std::string> items; // by location index
};

bool loadFixture(std::vector<Case>& out) {
    std::string text;
    if (!rt::readFixture("R1_4_fill.txt", text)) {
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
            out.back().label = arg(1);
            out.back().seed = arg(2);
        } else if (tag == "key") {
            out.back().key = arg(1);
            out.back().hasKey = true;
        } else if (tag == "opt") {
            out.back().opts.emplace_back(arg(1), arg(2));
        } else if (tag == "draws") {
            out.back().draws = std::atol(arg(1).c_str());
        } else if (tag == "loc") {
            const int i = std::atoi(arg(1).c_str());
            if ((int)out.back().items.size() != i) {
                std::fprintf(stderr, "FAIL fixture location order at %s\n", line.c_str());
                return false;
            }
            out.back().items.push_back(arg(2));
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
    return true;
}

long countLines(FILE* f) {
    std::fflush(f);
    std::rewind(f);
    long n = 0;
    int ch;
    while ((ch = std::fgetc(f)) != EOF) {
        n += ch == '\n';
    }
    return n;
}

void runCase(const Case& c) {
    pcr::Options o;
    if (!buildOptions(c, o)) {
        return;
    }
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    if (!RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init))) {
        std::fprintf(stderr, "  case %s: %s\n", c.label.c_str(), init.error.c_str());
        return;
    }

    // The trace: to COS_RANDO_TRACE when set (for diffs), else to a temporary file to count draws.
    const char* env = std::getenv("COS_RANDO_TRACE");
    const bool userTrace = env != nullptr && env[0] != '\0' && std::strcmp(env, "0") != 0;
    FILE* tmp = nullptr;
    if (userTrace) {
        pcr::traceBegin(c.seed);
    } else {
        tmp = std::tmpfile();
        pcr::traceBeginFile(tmp, c.seed);
    }
    pcr::ItemFillResult res;
    const bool ok = pcr::runItemRandomizer(L, init, c.seed, res,
                                           c.hasKey ? c.key.c_str() : pcr::kSwitchWakerSeedKey);
    pcr::traceEnd();
    if (!RT_CHECK(ok)) {
        std::fprintf(stderr, "  case %s: %s\n", c.label.c_str(), res.error.c_str());
    }
    if (tmp != nullptr) {
        // Header + one line per draw.
        const long draws = countLines(tmp) - 1;
        std::fclose(tmp);
        if (!RT_CHECK_EQ(draws, c.draws)) {
            std::fprintf(stderr, "  case %s: RNG draw count\n", c.label.c_str());
        }
    }

    if (!RT_CHECK_EQ((int)c.items.size(), L.locationCount())) {
        return;
    }
    int mismatches = 0;
    for (int i = 0; i < L.locationCount(); i++) {
        const int it = L.doneItemLocations[i];
        const std::string ours = it >= 0 ? L.name(it) : std::string("(none)");
        if (ours != c.items[i]) {
            if (mismatches < 5) {
                std::fprintf(stderr, "FAIL case %s seed %s: %s: ours %s, wwrando %s\n", c.label.c_str(),
                             c.seed.c_str(), L.locationName(i), ours.c_str(), c.items[i].c_str());
            }
            mismatches++;
        }
    }
    if (mismatches > 0) {
        std::fprintf(stderr, "  case %s: %d of %d locations differ\n", c.label.c_str(), mismatches,
                     L.locationCount());
        rt::failures()++;
    }

    // Pool integrity.
    RT_CHECK(L.unplacedProgressItems.empty());
    RT_CHECK(L.unplacedNonprogressItems.empty());
    RT_CHECK(L.remainingItemLocations.empty());
    RT_CHECK(!L.failed());
    for (int it : init.allRandomizedProgressItems) {
        const int placed = (int)std::count(L.doneItemLocations.begin(), L.doneItemLocations.end(), it);
        const int want = (int)std::count(init.allRandomizedProgressItems.begin(),
                                         init.allRandomizedProgressItems.end(), it);
        if (!RT_CHECK(placed >= want)) {
            std::fprintf(stderr, "  case %s: %s placed %d of %d\n", c.label.c_str(), L.name(it).c_str(),
                         placed, want);
            break;
        }
    }
    if (init.dungeonsAndCavesOnlyStart && !o.keylunacy) {
        // The failsafe location holds no DRC big key, map or compass.
        RT_CHECK(res.drcFailsafeLocation >= 0);
        if (res.drcFailsafeLocation >= 0) {
            const std::string& n = L.name(L.doneItemLocations[res.drcFailsafeLocation]);
            RT_CHECK(n != "DRC Big Key" && n != "DRC Dungeon Map" && n != "DRC Compass");
        }
    } else {
        RT_CHECK_EQ(res.drcFailsafeLocation, -1);
    }
}

void syntheticChecks() {
    // RNG_CHANGING_OPTIONS at the defaults: fishmen and hoho on, korl off, 0 path, 0 barren,
    // 5 location, 15 item hints, cryptic on, the rest off.
    pcr::Options o;
    int v[pc::rando::kRngChangingOptionCount];
    pcr::rngChangingOptionValues(o, v);
    const int want[pc::rando::kRngChangingOptionCount] = {1, 1, 0, 0, 0, 5, 15, 1, 0, 0, 0};
    for (int i = 0; i < pc::rando::kRngChangingOptionCount; i++) {
        RT_CHECK_EQ(v[i], want[i]);
    }

    // The trace format: the header, then "<owner> randbelow <n> <r>".
    FILE* tmp = std::tmpfile();
    pcr::traceBeginFile(tmp, "abc");
    pc::rando::PyRandom rng;
    pcr::newRandomizerRng(rng, "abc", o);
    pcr::traceAttach(rng, "ItemRandomizer");
    const uint64_t r = rng.randbelow(10);
    pcr::traceEnd();
    std::fflush(tmp);
    std::rewind(tmp);
    char buf[256] = {0};
    std::string text;
    while (std::fgets(buf, sizeof buf, tmp) != nullptr) {
        text += buf;
    }
    std::fclose(tmp);
    char expect[256];
    std::snprintf(expect, sizeof expect, "# ww_trace 1 seed=abc version=%s\nItemRandomizer randbelow 10 %llu\n",
                  pcr::kWwrandoVersion, (unsigned long long)r);
    RT_CHECK_EQ(text, std::string(expect));
    // After traceEnd a new attach logs nothing (and must not crash).
    pcr::traceAttach(rng, "ItemRandomizer");
    rng.randbelow(10);

    // A Logic that failed before the fill: the fill reports the error and places nothing.
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init));
    L.fail("broken on purpose");
    pcr::ItemFillResult res;
    RT_CHECK(!pcr::runItemRandomizer(L, init, "abc", res));
    RT_CHECK_EQ(res.error, std::string("broken on purpose"));
    RT_CHECK(!L.remainingItemLocations.empty());
}

} // namespace

int main() {
    std::vector<Case> cases;
    if (loadFixture(cases)) {
        RT_CHECK(cases.size() >= 10);
        int defaults = 0;
        for (const Case& c : cases) {
            defaults += c.opts.empty() && !c.hasKey;
            runCase(c);
        }
        RT_CHECK(defaults >= 10); // the plan's 10 default seeds
    }
    syntheticChecks();
    return rt::finish();
}
