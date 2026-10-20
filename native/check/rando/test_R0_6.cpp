// cos_rando_test_R0_6 (docs/RANDOMIZER_PLAN.md step R0.6): the JSON reader/writer
// (features/rando/pc_rando_json.h) and the placement file (pc_rando_seedfile.h), without a game.
//   - JSON: parse/write round trips, Python json.dumps formatting (indent=1, ensure_ascii escapes,
//     surrogate pairs, empty containers), key order, numbers kept as text, error positions;
//   - placements written by the oracle load, save and load again byte-identical, with a stable hash
//     equal to Python's hashlib.md5 of the file (the file is the serialization);
//   - item names (logic and original) map to wwrando's ids; charts, bosses, entrances, starting
//     island, hints and version are read and written back; bad files are refused with a reason;
//   - save/load through a real file (temporary directory).
//
// Fixtures (native/check/rando/fixtures/, written by the oracle, wwrando 1.10.0_9775811, MIT data):
//   R0_6_TestSeed1.json  python3 -I native/tools/rando/ww_export.py TestSeed1 <out>
//   R0_6_abc_full.json   python3 -I native/tools/rando/ww_export.py abc <out> randomize_charts=true
//                        required_bosses=true randomize_dungeon_entrances=true
//                        randomize_secret_cave_entrances=true randomize_starting_island=true
//   R0_6_identity.json   python3 -I native/tools/rando/make_test_placement.py --kind identity <out>
//   R0_6_rotation.json   python3 -I native/tools/rando/make_test_placement.py --kind rotation <out>
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_json.h"
#include "pc_rando_seedfile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

using pcr::Json;

std::string roundTrip(const std::string& text, int indent) {
    Json v;
    std::string err;
    if (!pcr::jsonParse(text, &v, &err)) {
        return "PARSE ERROR " + err;
    }
    return pcr::jsonDump(v, indent);
}

void testJson() {
    // Python json.dumps(..., indent=1) and json.dumps(...) of the same value.
    const std::string src = "{\"b\": [1, -2.50, 3e+2, true, false, null, [], {}], \"a\": {\"x\": \"y\"}}";
    RT_CHECK_EQ(roundTrip(src, -1), src);
    RT_CHECK_EQ(roundTrip(src, 1), std::string("{\n \"b\": [\n  1,\n  -2.50,\n  3e+2,\n  true,\n  false,\n"
                                               "  null,\n  [],\n  {}\n ],\n \"a\": {\n  \"x\": \"y\"\n }\n}"));
    RT_CHECK_EQ(roundTrip("  [ ]  ", 1), std::string("[]"));
    RT_CHECK_EQ(roundTrip("\"\"", 1), std::string("\"\""));

    // ensure_ascii: json.dumps("\"\\/\b\f\n\r\t\x01\x7f é € 😀") ==
    //   "\"\\\"\\\\/\\b\\f\\n\\r\\t\\u0001\\u007f \\u00e9 \\u20ac \\ud83d\\ude00\""
    const std::string esc = "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0001\\u007f \\u00E9 \xE2\x82\xAC \\uD83D\\uDE00\"";
    RT_CHECK_EQ(roundTrip(esc, -1),
                std::string("\"\\\"\\\\/\\b\\f\\n\\r\\t\\u0001\\u007f \\u00e9 \\u20ac \\ud83d\\ude00\""));
    Json s;
    RT_CHECK(pcr::jsonParse("\"\\u00e9\\ud83d\\ude00\"", &s, nullptr));
    RT_CHECK_EQ(s.text(), std::string("\xC3\xA9\xF0\x9F\x98\x80"));
    // A lone surrogate is kept (Python accepts it) and written back the same.
    RT_CHECK_EQ(roundTrip("\"\\ud800x\"", -1), std::string("\"\\ud800x\""));

    // Key order kept; a repeated key keeps its first place and its last value (json.load).
    Json o;
    RT_CHECK(pcr::jsonParse("{\"z\":1,\"a\":2,\"z\":3}", &o, nullptr));
    RT_CHECK_EQ(o.size(), (size_t)2);
    RT_CHECK_EQ(o.keyAt(0), std::string("z"));
    RT_CHECK_EQ(o.find("z")->asInt(), (int64_t)3);
    RT_CHECK(o.find("missing") == nullptr);
    o.set("a", Json::string("b"));
    o.set("n", Json::integer(-7));
    RT_CHECK_EQ(pcr::jsonDump(o, -1), std::string("{\"z\": 3, \"a\": \"b\", \"n\": -7}"));
    RT_CHECK(o.erase("z"));
    RT_CHECK(!o.erase("z"));
    RT_CHECK_EQ(pcr::jsonDump(o, -1), std::string("{\"a\": \"b\", \"n\": -7}"));

    // Numbers.
    Json n;
    RT_CHECK(pcr::jsonParse("12345678901234", &n, nullptr));
    RT_CHECK(n.isInteger());
    RT_CHECK_EQ(n.asInt(), (int64_t)12345678901234LL);
    RT_CHECK(pcr::jsonParse("1.5e3", &n, nullptr));
    RT_CHECK(!n.isInteger());
    RT_CHECK_EQ(n.asInt(), (int64_t)1500);
    RT_CHECK_EQ(n.asDouble(), 1500.0);
    RT_CHECK_EQ(pcr::jsonDump(Json::real(1.0), -1), std::string("1.0"));
    RT_CHECK_EQ(pcr::jsonDump(Json::real(0.1), -1), std::string("0.1"));
    RT_CHECK_EQ(pcr::jsonDump(Json::real(4.0), -1), std::string("4.0"));
    RT_CHECK_EQ(pcr::jsonDump(Json::real(1.0 / 3.0), -1), std::string("0.3333333333333333"));

    // Equality.
    Json a, b;
    RT_CHECK(pcr::jsonParse("{\"k\":[1,{\"x\":null}]}", &a, nullptr));
    RT_CHECK(pcr::jsonParse(" { \"k\" : [ 1 , { \"x\" : null } ] } ", &b, nullptr));
    RT_CHECK(a == b);
    RT_CHECK(pcr::jsonParse("{\"k\":[1,{\"x\":0}]}", &b, nullptr));
    RT_CHECK(a != b);

    // Errors (strict JSON), with a position.
    const char* bad[] = {"", "{", "[1,]", "{\"a\":1,}", "01", "1.", "-", "NaN", "Infinity", "'x'", "\"\\x\"",
                         "\"a", "{a:1}", "[1 2]", "1 2", "\"\x01\"", "\"\xC0\xAF\"", "\"\xED\xA0\x80\"", "tru",
                         "// c\n1"};
    for (const char* t : bad) {
        Json v = Json::integer(1);
        std::string err;
        const bool ok = pcr::jsonParse(t, &v, &err);
        if (!RT_CHECK(!ok)) {
            fprintf(stderr, "  accepted: %s\n", t);
        }
        RT_CHECK(v.isNull());
        RT_CHECK(err.rfind("line ", 0) == 0);
    }
    std::string err;
    RT_CHECK(!pcr::jsonParse("{\n \"a\": [1,\n  x]\n}", &o, &err));
    RT_CHECK_EQ(err, std::string("line 3 col 3: unexpected character"));
    std::string deep(300, '[');
    RT_CHECK(!pcr::jsonParse(deep, &o, &err));
    RT_CHECK(err.find("nesting too deep") != std::string::npos);
    // A UTF-8 BOM is skipped.
    RT_CHECK(pcr::jsonParse("\xEF\xBB\xBF[1]", &o, nullptr));
}

void testItems() {
    RT_CHECK_EQ(pc_rando_item_id("Heart (Pickup)"), 0x00);
    RT_CHECK_EQ(pc_rando_item_id("Hero's Sword"), 0x38);
    RT_CHECK_EQ(pc_rando_item_id("Progressive Sword"), 0x38);
    RT_CHECK_EQ(pc_rando_item_id("Progressive Magic Meter"), 0xB1);
    RT_CHECK_EQ(pc_rando_item_id("Small Key"), 0x15);
    RT_CHECK_EQ(pc_rando_item_id("DRC Small Key"), 0x13);
    RT_CHECK_EQ(pc_rando_item_id("WT Compass"), 0x85);
    RT_CHECK_EQ(pc_rando_item_id("Rainbow Rupee"), 0xB8);
    RT_CHECK_EQ(pc_rando_item_id("Treasure Chart 25"), pc_rando_item_id("Treasure Chart 25"));
    RT_CHECK(pc_rando_item_id("Treasure Chart 25") > 0);
    RT_CHECK_EQ(pc_rando_item_id("Nothing"), -1);
    RT_CHECK_EQ(pc_rando_item_id(""), -1);
    RT_CHECK_EQ(std::string(pc_rando_item_name(0x38)), std::string("Progressive Sword"));
    RT_CHECK_EQ(std::string(pc_rando_item_name(0x13)), std::string("DRC Small Key"));
    RT_CHECK_EQ(std::string(pc_rando_item_name(0x01)), std::string("Green Rupee"));
    RT_CHECK(pc_rando_item_name(0xFF) == nullptr);
}

// Loads a fixture, saves, loads again: identical text and hash; the hash is md5(file).
void testFixture(const char* name, const char* md5hex, PcRandoSeed* out) {
    std::string text;
    if (!rt::readFixture(name, text)) {
        return;
    }
    std::string err;
    PcRandoSeed s;
    if (!RT_CHECK(pc_rando_seed_parse(text, &s, &err))) {
        fprintf(stderr, "  %s: %s\n", name, err.c_str());
        return;
    }
    const std::string saved = pc_rando_seed_serialize(s);
    RT_CHECK_EQ(saved.size(), text.size());
    RT_CHECK(saved == text);
    PcRandoSeed s2;
    RT_CHECK(pc_rando_seed_parse(saved, &s2, &err));
    RT_CHECK(pc_rando_seed_serialize(s2) == saved);
    RT_CHECK(memcmp(s.hash, s2.hash, 16) == 0);
    RT_CHECK_EQ(pc_rando_seed_hash_hex(s), std::string(md5hex));
    RT_CHECK_EQ(pc_rando_seed_hash_hex(s2), std::string(md5hex));
    pc_rando_seed_update_hash(&s2);
    RT_CHECK_EQ(pc_rando_seed_hash_hex(s2), std::string(md5hex));
    RT_CHECK_EQ(s.location_names.size(), (size_t)PC_RANDO_SEED_LOCATIONS);
    RT_CHECK_EQ(s.location_names[0], std::string("Outset Island - Underneath Link's House"));
    RT_CHECK_EQ(s.location_names[319], std::string("Five-Star Isles - Sunken Treasure"));
    for (int i = 0; i < PC_RANDO_SEED_LOCATIONS; i++) {
        RT_CHECK_EQ((int)s.locations[i], pc_rando_item_id(s.location_item_names[(size_t)i]));
    }
    RT_CHECK_EQ(s.charts.size(), (size_t)49);
    RT_CHECK(!s.has_hints);
    RT_CHECK(!s.has_version);
    RT_CHECK_EQ(s.extra.size(), (size_t)0);
    if (out != nullptr) {
        *out = s;
    }
}

void testFixtures() {
    // md5 of each fixture file (Python: hashlib.md5(open(f, "rb").read()).hexdigest()).
    PcRandoSeed t;
    testFixture("R0_6_TestSeed1.json", "b5ad99908ff3a3437af5e6dcc55887a6", &t);
    RT_CHECK_EQ(t.seed, std::string("TestSeed1"));
    RT_CHECK_EQ(t.permalink, std::string("MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAEkQUBgAAPsCyCUAAAAAAAEAQQCAAA=="));
    RT_CHECK_EQ(t.format, 1);
    RT_CHECK_EQ(t.options.size(), (size_t)77);
    RT_CHECK(t.options.find("keylunacy") != nullptr && !t.options.find("keylunacy")->asBool(true));
    RT_CHECK_EQ(t.options.find("num_required_bosses")->asInt(), (int64_t)4);
    RT_CHECK_EQ(t.options.find("sword_mode")->text(), std::string("Start with Hero's Sword"));
    RT_CHECK_EQ(t.locations[0], (uint8_t)0x27); // Progressive Bow
    RT_CHECK_EQ(t.location_item_names[0], std::string("Progressive Bow"));
    RT_CHECK_EQ(t.locations[319], (uint8_t)0x04); // Red Rupee
    RT_CHECK_EQ(t.charts[0].first, 1);
    RT_CHECK_EQ(t.charts[0].second, std::string("Treasure Chart 25"));
    RT_CHECK_EQ(t.entrances.size(), (size_t)44);
    RT_CHECK_EQ(t.entrances[0].second, std::string("Dragon Roost Cavern"));
    RT_CHECK_EQ(t.required_boss_locations.size(), (size_t)0);
    RT_CHECK_EQ(t.extra_starting_items.size(), (size_t)0);
    RT_CHECK_EQ(t.starting_island_room, 44);
    RT_CHECK_EQ(pc_rando_seed_hash32(t), 0xb5ad9990u);
    RT_CHECK_EQ(pc_rando_seed_sidecar_name(t), std::string("switchwaker-rando-b5ad9990.json"));

    PcRandoSeed f;
    testFixture("R0_6_abc_full.json", "68b0e9cba9e7e63e08f6365b324623f7", &f);
    RT_CHECK_EQ(f.seed, std::string("abc"));
    RT_CHECK_EQ(f.starting_island_room, 30);
    RT_CHECK_EQ(f.required_boss_locations.size(), (size_t)4);
    RT_CHECK_EQ(f.required_boss_locations[0], std::string("Forsaken Fortress - Helmaroc King Heart Container"));
    RT_CHECK_EQ(f.entrances[0].first, std::string("Dungeon Entrance on Dragon Roost Island"));
    RT_CHECK_EQ(f.entrances[0].second, std::string("Forbidden Woods"));
    RT_CHECK_EQ(f.charts[0].second, std::string("Treasure Chart 40"));
    RT_CHECK_EQ(f.location_item_names[0], std::string("Orange Rupee"));

    PcRandoSeed id;
    testFixture("R0_6_identity.json", "f684609f2c27e4b5f2e0e03575f98825", &id);
    RT_CHECK_EQ(id.locations[0], (uint8_t)pc_rando_item_id("Orange Rupee")); // vanilla: Underneath Link's House
    testFixture("R0_6_rotation.json", "029d5c73336164c6801a2538016e0290", nullptr);
}

// Our generator's fields: hints, version and unknown keys come back in place.
void testOwnFields() {
    std::string text;
    if (!rt::readFixture("R0_6_TestSeed1.json", text)) {
        return;
    }
    PcRandoSeed s;
    std::string err;
    RT_CHECK(pc_rando_seed_parse(text, &s, &err));
    const std::string before = pc_rando_seed_hash_hex(s);
    s.has_hints = true;
    RT_CHECK(pcr::jsonParse("{\"item\": [{\"location\": \"Outset Island - Great Fairy\", \"text\": \"\\u00e9\"}]}",
                            &s.hints, nullptr));
    s.has_version = true;
    s.version = "SwitchWaker 0.6.0";
    s.extra.set("note", Json::integer(1));
    pc_rando_seed_update_hash(&s);
    RT_CHECK(pc_rando_seed_hash_hex(s) != before);
    const std::string saved = pc_rando_seed_serialize(s);
    RT_CHECK(saved.size() > text.size());
    RT_CHECK(saved.find("\n \"hints\": {\n  \"item\": [") != std::string::npos);
    RT_CHECK(saved.find("\"text\": \"\\u00e9\"") != std::string::npos);
    RT_CHECK(saved.find("\n \"version\": \"SwitchWaker 0.6.0\",\n \"note\": 1\n}\n") != std::string::npos);
    PcRandoSeed s2;
    RT_CHECK(pc_rando_seed_parse(saved, &s2, &err));
    RT_CHECK(s2.has_hints && s2.hints == s.hints);
    RT_CHECK(s2.has_version);
    RT_CHECK_EQ(s2.version, std::string("SwitchWaker 0.6.0"));
    RT_CHECK_EQ(s2.extra.size(), (size_t)1);
    RT_CHECK(pc_rando_seed_serialize(s2) == saved);
    RT_CHECK_EQ(pc_rando_seed_hash_hex(s2), pc_rando_seed_hash_hex(s));
    // Whitespace does not change the hash (it is over our serialization): a compact copy.
    Json compact;
    RT_CHECK(pcr::jsonParse(saved, &compact, nullptr));
    PcRandoSeed s3;
    RT_CHECK(pc_rando_seed_parse(pcr::jsonDump(compact, -1), &s3, &err));
    RT_CHECK_EQ(pc_rando_seed_hash_hex(s3), pc_rando_seed_hash_hex(s));
}

// A copy of TestSeed1 with one edit refuses to load with the expected reason.
void expectRefused(const char* what, const std::string& from, const std::string& to, const char* reason) {
    std::string text;
    if (!rt::readFixture("R0_6_TestSeed1.json", text)) {
        return;
    }
    const size_t at = text.find(from);
    if (!RT_CHECK(at != std::string::npos)) {
        fprintf(stderr, "  %s: pattern not in fixture\n", what);
        return;
    }
    text.replace(at, from.size(), to);
    PcRandoSeed s;
    s.seed = "stale";
    std::string err;
    const bool ok = pc_rando_seed_parse(text, &s, &err);
    RT_CHECK(!ok);
    RT_CHECK(s.seed.empty() && s.location_names.empty());
    if (!RT_CHECK(err.find(reason) != std::string::npos)) {
        fprintf(stderr, "  %s: error was \"%s\"\n", what, err.c_str());
    }
}

void testRefused() {
    expectRefused("format", "\"format\": 1,", "\"format\": 2,", "unsupported placement format 2");
    expectRefused("format type", "\"format\": 1,", "\"format\": \"1\",", "\"format\" must be an integer");
    expectRefused("seed", "\"seed\": \"TestSeed1\",", "\"seed\": 5,", "\"seed\" must be a string");
    expectRefused("item", "\"Outset Island - Underneath Link's House\": \"Progressive Bow\"",
                  "\"Outset Island - Underneath Link's House\": \"Bow of Light\"", "unknown item \"Bow of Light\"");
    expectRefused("count", "  \"Outset Island - Underneath Link's House\": \"Progressive Bow\",\n", "",
                  "holds 319 locations, not 320");
    expectRefused("chart key", "\"1\": \"Treasure Chart 25\"", "\"01\": \"Treasure Chart 25\"", "bad island number");
    expectRefused("room", "\"starting_island_room\": 44", "\"starting_island_room\": 50", "starting_island_room");
    expectRefused("json", "\"format\": 1,", "\"format\": 1,,", "bad JSON: line 2");
    PcRandoSeed s;
    std::string err;
    RT_CHECK(!pc_rando_seed_parse("[]", &s, &err));
    RT_CHECK_EQ(err, std::string("the placement is not a JSON object"));
}

void testFiles() {
    char dir[] = "/tmp/cos_rando_R0_6_XXXXXX";
    if (!RT_CHECK(mkdtemp(dir) != nullptr)) {
        return;
    }
    std::string text;
    if (!rt::readFixture("R0_6_abc_full.json", text)) {
        return;
    }
    PcRandoSeed s;
    std::string err;
    RT_CHECK(pc_rando_seed_parse(text, &s, &err));
    const std::string path = std::string(dir) + "/" + pc_rando_seed_sidecar_name(s);
    RT_CHECK(pc_rando_seed_save_file(path.c_str(), s, &err));
    RT_CHECK(pc_rando_seed_save_file(path.c_str(), s, &err)); // over an existing file
    std::string back;
    RT_CHECK(rt::readFile(path, back));
    RT_CHECK(back == text);
    RT_CHECK(access((path + ".tmp").c_str(), F_OK) != 0);
    PcRandoSeed l;
    RT_CHECK(pc_rando_seed_load_file(path.c_str(), &l, &err));
    RT_CHECK(memcmp(l.hash, s.hash, 16) == 0);
    RT_CHECK(!pc_rando_seed_load_file((std::string(dir) + "/missing.json").c_str(), &l, &err));
    RT_CHECK(err.find("cannot open") != std::string::npos);
    RT_CHECK(l.location_names.empty());
    RT_CHECK(!pc_rando_seed_save_file((std::string(dir) + "/no/such/dir.json").c_str(), s, &err));
    remove(path.c_str());
    rmdir(dir);
}

} // namespace

int main() {
    testJson();
    testItems();
    testFixtures();
    testOwnFields();
    testRefused();
    testFiles();
    return rt::finish();
}
