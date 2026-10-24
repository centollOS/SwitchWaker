// cos_rando_test_R7_3c: the enemy randomizer's generator (docs/RANDOMIZER_PLAN.md step R7.3c).
//
// Vectors from wwrando (fixtures/R7_3c_enemies.txt, native/tools/rando/ww_export_enemies.py --vectors):
// 5 seeds (default x2, all progression categories, Swordless, 2 extra starting items with starting
// gear) with randomize_enemies. Per seed: every done enemy location (path, new type, params, x_rot,
// z_rot, flags after the RNG part of EnemyRandomizer._save), the 3 Homen choices and a CRC-32 of the
// item placement (the logic filter's probes reorder the item fill's input, so it must stay golden).
// Also: the menu's request generates it since R7.4d (it was refused until then), the "enemies" JSON round trip and its errors, the seed file
// keeps (and hashes) the key, and the key is absent with the option off.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_enemy_data.h"
#include "pc_rando_gen.h"
#include "pc_rando_gen_enemies.h"
#include "pc_rando_json.h"
#include "pc_rando_options.h"
#include "pc_rando_seedfile.h"
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

struct Case {
    std::string name, seed;
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<std::string> enemies; // "path\ttype\tparams\tx_rot\tz_rot\tflags"
    std::string homen, locations, error;
};

bool loadFixture(std::vector<Case>& cases) {
    std::string text;
    if (!rt::readFixture("R7_3c_enemies.txt", text)) {
        return false;
    }
    bool ended = false;
    for (const std::string& line : split(text, '\n')) {
        if (line.empty()) {
            continue;
        }
        const size_t tab = line.find('\t');
        const std::string tag = line.substr(0, tab);
        const std::string rest = tab == std::string::npos ? std::string() : line.substr(tab + 1);
        if (tag == "version") {
            RT_CHECK_EQ(rest, std::string(pcr::kWwrandoVersion));
        } else if (tag == "case") {
            std::vector<std::string> f = split(rest, '\t');
            cases.emplace_back();
            cases.back().name = f[0];
            cases.back().seed = f.size() > 1 ? f[1] : std::string();
        } else if (cases.empty()) {
            RT_CHECK(tag == "end");
        } else if (tag == "opt") {
            std::vector<std::string> f = split(rest, '\t');
            cases.back().opts.emplace_back(f[0], f.size() > 1 ? f[1] : std::string());
        } else if (tag == "enemy") {
            cases.back().enemies.push_back(rest);
        } else if (tag == "homen") {
            cases.back().homen = rest;
        } else if (tag == "locations") {
            cases.back().locations = rest;
        } else if (tag == "error") {
            cases.back().error = rest;
        } else if (tag == "end") {
            ended = true;
        } else {
            RT_CHECK(tag == "endcase");
        }
    }
    return RT_CHECK(ended) && RT_CHECK_EQ((int)cases.size(), 5);
}

pcr::GenRequest request(const Case& c) {
    pcr::GenRequest req;
    req.seed = c.seed;
    req.allowMissingHints = true; // hints run after the items on their own RNG
    req.allowNotRuntimeReady = true;
    req.version = "test";
    for (const auto& kv : c.opts) {
        const int id = pcr::optionFind(kv.first.c_str());
        if (!RT_CHECK(id >= 0) || !RT_CHECK(pcr::optionFromString(req.options, id, kv.second.c_str()))) {
            std::fprintf(stderr, "  option %s=%s\n", kv.first.c_str(), kv.second.c_str());
        }
    }
    return req;
}

std::string hex(uint32_t v, int digits) {
    char b[16];
    std::snprintf(b, sizeof b, "%0*x", digits, v);
    return b;
}

std::string enemyLine(const pcr::EnemyPlacement& p) {
    return std::string(pcr::enemy_locations()[p.location].path.raw) + "\t" + pcr::enemy_types()[p.type].pretty_name +
           "\t" + hex(p.params, 8) + "\t" + hex(p.x_rot, 4) + "\t" + hex(p.z_rot, 4) + "\t" + std::to_string(p.flags);
}

void checkCase(const Case& c, std::vector<pcr::GenResult>& results) {
    pcr::GenResult res;
    const bool ok = pcr::generate(request(c), res);
    if (!c.error.empty()) {
        RT_CHECK(!ok);
        RT_CHECK_EQ(res.error.substr(0, res.error.find('\n')), c.error);
        return;
    }
    if (!RT_CHECK(ok)) {
        std::fprintf(stderr, "  case %s %s: %s\n", c.name.c_str(), c.seed.c_str(), res.error.c_str());
        return;
    }
    const pcr::Json* ej = res.seed.extra.find("enemies");
    pcr::EnemyRandoResult er;
    std::string err;
    if (!RT_CHECK(ej != nullptr) || !RT_CHECK(pcr::enemiesFromJson(*ej, er, &err))) {
        std::fprintf(stderr, "  case %s: %s\n", c.name.c_str(), err.c_str());
        return;
    }
    if (RT_CHECK_EQ(er.placements.size(), c.enemies.size())) {
        int bad = 0;
        for (size_t i = 0; i < c.enemies.size() && bad < 5; i++) {
            if (!RT_CHECK_EQ(enemyLine(er.placements[i]), c.enemies[i])) {
                std::fprintf(stderr, "  case %s %s: enemy %zu\n", c.name.c_str(), c.seed.c_str(), i);
                bad++;
            }
        }
    }
    RT_CHECK_EQ(std::to_string(er.homen[0]) + "\t" + std::to_string(er.homen[1]) + "\t" + std::to_string(er.homen[2]),
                c.homen);
    RT_CHECK(pcr::enemiesToJson(er) == *ej);

    // The item placement in wwrando's done_item_locations order.
    pcr::Json pj;
    if (RT_CHECK(pcr::jsonParse(res.placementJson, &pj, &err))) {
        const pcr::Json* locs = pj.find("locations");
        std::string s;
        for (size_t i = 0; locs != nullptr && i < locs->size(); i++) {
            s += (i ? "|" : "") + locs->keyAt(i) + "=" + locs->at(i).text();
        }
        if (!RT_CHECK_EQ(hex(crc32(s), 8), c.locations)) {
            std::fprintf(stderr, "  case %s %s: item placement differs\n", c.name.c_str(), c.seed.c_str());
        }
    }
    results.push_back(res);
}

void testVectors() {
    std::vector<Case> cases;
    if (!loadFixture(cases)) {
        return;
    }
    std::vector<pcr::GenResult> results;
    size_t enemies = 0;
    for (const Case& c : cases) {
        checkCase(c, results);
        enemies += c.enemies.size();
    }
    RT_CHECK(enemies >= 5 * 1000);
    if (results.empty()) {
        return;
    }
    const pcr::GenResult& res = results[0];

    // The seed file keeps the key through a round trip, and it is part of the hash.
    std::string err;
    PcRandoSeed back;
    RT_CHECK(pc_rando_seed_parse(pc_rando_seed_serialize(res.seed), &back, &err));
    RT_CHECK(back.extra.find("enemies") != nullptr && *back.extra.find("enemies") == *res.seed.extra.find("enemies"));
    RT_CHECK(std::memcmp(back.hash, res.seed.hash, 16) == 0);
    PcRandoSeed changed = res.seed;
    pcr::Json* h = changed.extra.find("enemies")->find("homen");
    h->at(0) = pcr::Json::integer(h->at(0).asInt() == 0 ? 1 : 0);
    pc_rando_seed_update_hash(&changed);
    RT_CHECK(std::memcmp(changed.hash, res.seed.hash, 16) != 0);

    // From JSON: errors.
    const pcr::Json good = *res.seed.extra.find("enemies");
    pcr::EnemyRandoResult out;
    RT_CHECK(pcr::enemiesFromJson(good, out, &err));
    pcr::Json bad = good;
    bad.find("homen")->at(1) = pcr::Json::integer(9); // Dark ChuChu is not a candidate
    RT_CHECK(!pcr::enemiesFromJson(bad, out, &err) && err.find("homen") != std::string::npos);
    RT_CHECK(out.placements.empty());
    bad = good;
    bad.find("locations")->set("sea/Room1.arc/Actor7FF", bad.find("locations")->at(0));
    RT_CHECK(!pcr::enemiesFromJson(bad, out, &err) && err.find("unknown enemy location") != std::string::npos);
    bad = good;
    bad.find("locations")->at(0).set("name", pcr::Json::string("Zz"));
    RT_CHECK(!pcr::enemiesFromJson(bad, out, &err) && err.find("unknown type or actor name") != std::string::npos);
    bad = good;
    bad.find("locations")->at(0).set("x_rot", pcr::Json::integer(0x10000));
    RT_CHECK(!pcr::enemiesFromJson(bad, out, &err) && err.find("out of range") != std::string::npos);
    RT_CHECK(!pcr::enemiesFromJson(pcr::Json::array(), out, &err));
}

void testRefusalAndOff() {
    Case c;
    c.seed = "EnemyVec0";
    c.opts.emplace_back("randomize_enemies", "1");
    pcr::GenRequest req = request(c);
    // R7.4d: live; the menu's request (no allowNotRuntimeReady) generates the same enemies.
    pcr::GenResult cli;
    RT_CHECK(pcr::generate(req, cli));
    req.allowNotRuntimeReady = false; // the menu
    pcr::GenResult res;
    if (RT_CHECK(pcr::generate(req, res))) {
        RT_CHECK(!res.unsupported);
        const pcr::Json* a = res.seed.extra.find("enemies");
        const pcr::Json* b = cli.seed.extra.find("enemies");
        RT_CHECK(a != nullptr && b != nullptr && pcr::jsonDump(*a, -1) == pcr::jsonDump(*b, -1));
    }
    RT_CHECK(pcr::unsupportedOptionsError(req.options, true, false).empty());
    RT_CHECK(pcr::unsupportedOptionsError(req.options, true, true).empty());

    req.options.randomize_enemies = false;
    pcr::GenResult off;
    if (RT_CHECK(pcr::generate(req, off))) {
        RT_CHECK(off.seed.extra.find("enemies") == nullptr);
    }
}

// The bitfield packing on one known case: a Winged Mothula's missing wings and a Bokoblin's fields
// go into params through actor_parameters.txt's masks.
void testParamsShape() {
    pcr::EnemyRandoResult r;
    for (size_t i = 0; i < pcr::enemy_locations().size; i++) {
        pcr::EnemyPlacement p;
        p.location = (int)i;
        p.type = pcr::enemy_type_index("Bokoblin");
        r.placements.push_back(p);
    }
    pc::rando::PyRandom rng;
    rng.seedInt(12345);
    std::string err;
    if (!RT_CHECK(pcr::randomizeEnemyParams(rng, r, &err))) {
        std::fprintf(stderr, "  %s\n", err.c_str());
        return;
    }
    const pcr::ActorParamField* type = pcr::actor_param("d_a_bk", "bokoblin_type");
    if (!RT_CHECK(type != nullptr)) {
        return;
    }
    bool valid = true;
    for (const pcr::EnemyPlacement& p : r.placements) {
        const uint32_t t = pcr::actor_param_get(*type, p.params);
        const bool pot = std::strcmp(pcr::enemy_locations()[p.location].category_raw, "Pot") == 0;
        valid = valid && (pot ? (t == 2 || t == 3) : (t == 0 || t == 4 || t == 0xB));
    }
    RT_CHECK(valid);
}

} // namespace

int main() {
    testVectors();
    testRefusalAndOff();
    testParamsShape();
    return rt::finish();
}
