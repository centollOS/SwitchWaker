// cos_rando_test_R1_2 (docs/RANDOMIZER_PLAN.md step R1.2): options, permalinks and seed names
// (features/rando/pc_rando_options.h) against wwrando 1.10.0_9775811.
//   - the option table (77 options, 66 in the permalink) and its defaults equal wwrando_options.py;
//   - for 50 permalinks made by wwrando (fixture R1_2_permalinks.txt, from
//     native/tools/rando/gen_permalink_vectors.py): the seed sanitizes the same, the options encode to
//     the same permalink, the permalink decodes to the same options and re-encodes byte-identical, and
//     the seed hash and the integer seed's md5 match (with and without a seed key);
//   - permalink errors, Options.validate, ensure_valid_combination_of_options and random seed names.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_options.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace pcr;

namespace {

struct Case {
    int index = -1;
    std::string inputSeed, key, seed, permalink, hash, intseed;
    std::vector<std::pair<std::string, std::string>> in, out;
};

struct Fixture {
    std::string version;
    std::vector<std::vector<std::string>> options; // option lines, without the tag
    std::vector<std::string> regular, progressive;
    std::vector<Case> cases;
};

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> f;
    size_t p = 0;
    for (;;) {
        const size_t t = line.find('\t', p);
        if (t == std::string::npos) {
            f.push_back(line.substr(p));
            return f;
        }
        f.push_back(line.substr(p, t - p));
        p = t + 1;
    }
}

bool loadFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R1_2_permalinks.txt", text)) {
        return false;
    }
    Case cur;
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) {
            e = text.size();
        }
        const std::string line = text.substr(p, e - p);
        p = e + 1;
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = splitTabs(line);
        const std::string& tag = f[0];
        const std::string v = f.size() > 1 ? f[1] : std::string();
        if (tag == "version") {
            fx.version = v;
        } else if (tag == "option") {
            fx.options.emplace_back(f.begin() + 1, f.end());
        } else if (tag == "regular") {
            fx.regular.push_back(v);
        } else if (tag == "progressive") {
            fx.progressive.push_back(v);
        } else if (tag == "case") {
            cur = Case();
            cur.index = std::atoi(v.c_str());
        } else if (tag == "input_seed") {
            cur.inputSeed = v;
        } else if (tag == "key") {
            cur.key = v;
        } else if (tag == "seed") {
            cur.seed = v;
        } else if (tag == "permalink") {
            cur.permalink = v;
        } else if (tag == "hash") {
            cur.hash = v;
        } else if (tag == "intseed") {
            cur.intseed = v;
        } else if (tag == "in" || tag == "out") {
            (tag == "in" ? cur.in : cur.out).emplace_back(v, f.size() > 2 ? f[2] : std::string());
        } else if (tag == "end") {
            fx.cases.push_back(cur);
        } else {
            std::fprintf(stderr, "FAIL fixture: unknown tag %s\n", tag.c_str());
            rt::failures()++;
        }
    }
    return true;
}

const char* kindName(OptKind k) {
    switch (k) {
    case OptKind::Bool:
        return "bool";
    case OptKind::Enum:
        return "enum";
    case OptKind::Int:
        return "int";
    case OptKind::Str:
        return "str";
    case OptKind::List:
        return "list";
    case OptKind::Dict:
        return "dict";
    }
    return "?";
}

std::string hex(const uint8_t* d, int n) {
    std::string s;
    char b[3];
    for (int i = 0; i < n; i++) {
        std::snprintf(b, sizeof b, "%02x", d[i]);
        s += b;
    }
    return s;
}

std::string b64(const std::string& in) {
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    const auto* p = (const uint8_t*)in.data();
    for (; i + 3 <= in.size(); i += 3) {
        const unsigned v = (unsigned)p[i] << 16 | (unsigned)p[i + 1] << 8 | p[i + 2];
        out += a[v >> 18 & 63], out += a[v >> 12 & 63], out += a[v >> 6 & 63], out += a[v & 63];
    }
    if (in.size() - i == 1) {
        const unsigned v = (unsigned)p[i] << 16;
        out += a[v >> 18 & 63], out += a[v >> 12 & 63], out += "==";
    } else if (in.size() - i == 2) {
        const unsigned v = (unsigned)p[i] << 16 | (unsigned)p[i + 1] << 8;
        out += a[v >> 18 & 63], out += a[v >> 12 & 63], out += a[v >> 6 & 63], out += '=';
    }
    return out;
}

void checkTable(const Fixture& fx) {
    RT_CHECK_EQ(fx.version, std::string(kWwrandoVersion));
    RT_CHECK_EQ((int)fx.options.size(), (int)OPT_COUNT);
    const Options def;
    int permalinkCount = 0;
    for (int i = 0; i < OPT_COUNT && i < (int)fx.options.size(); i++) {
        const std::vector<std::string>& f = fx.options[i];
        RT_CHECK_EQ((int)f.size(), 8);
        if (f.size() != 8) {
            continue;
        }
        const OptionInfo& info = kOptionInfo[i];
        RT_CHECK_EQ(std::string(info.name), f[0]);
        RT_CHECK_EQ(std::string(kindName(info.kind)), f[1]);
        RT_CHECK_EQ((info.flags & kOptPermalink) != 0 ? 1 : 0, std::atoi(f[2].c_str()));
        RT_CHECK_EQ((info.flags & kOptHidden) != 0 ? 1 : 0, std::atoi(f[3].c_str()));
        RT_CHECK_EQ((info.flags & kOptUnbeatable) != 0 ? 1 : 0, std::atoi(f[4].c_str()));
        if (info.kind == OptKind::Int) {
            RT_CHECK_EQ(std::to_string(info.minimum), f[5]);
            RT_CHECK_EQ(std::to_string(info.maximum), f[6]);
        } else {
            RT_CHECK(f[5].empty() && f[6].empty());
        }
        if (!RT_CHECK_EQ(optionToString(def, i), f[7])) {
            std::fprintf(stderr, "  default of %s\n", info.name);
        }
        RT_CHECK_EQ(optionFind(f[0].c_str()), i);
        permalinkCount += (info.flags & kOptPermalink) != 0;
    }
    RT_CHECK_EQ(permalinkCount, 66);
    RT_CHECK_EQ(optionFind("no_such_option"), -1);

    RT_CHECK_EQ((int)fx.regular.size(), kRegularItemCount);
    for (int i = 0; i < kRegularItemCount && i < (int)fx.regular.size(); i++) {
        RT_CHECK_EQ(std::string(kRegularItems[i]), fx.regular[i]);
    }
    RT_CHECK_EQ((int)fx.progressive.size(), kProgressiveItemCount);
    for (int i = 0; i < kProgressiveItemCount && i < (int)fx.progressive.size(); i++) {
        RT_CHECK_EQ(std::string(kProgressiveItems[i]), fx.progressive[i]);
    }
    RT_CHECK_EQ((int)inventoryItems().size(), 74);
}

void checkCases(const Fixture& fx) {
    RT_CHECK_EQ((int)fx.cases.size(), 50);
    int keyedHashes = 0;
    for (const Case& c : fx.cases) {
        Options in;
        for (const auto& kv : c.in) {
            const int id = optionFind(kv.first.c_str());
            RT_CHECK(id >= 0 && optionFromString(in, id, kv.second.c_str()));
        }
        RT_CHECK_EQ(sanitizeSeed(c.inputSeed), c.seed);
        const std::string link = encodePermalink(c.inputSeed, in);
        if (!RT_CHECK_EQ(link, c.permalink)) {
            std::fprintf(stderr, "  case %d (encode)\n", c.index);
        }

        std::string seed;
        Options dec;
        const PermalinkError err = decodePermalink(c.permalink, &seed, &dec);
        RT_CHECK_EQ((int)err, (int)PermalinkError::None);
        RT_CHECK_EQ(seed, c.seed);
        RT_CHECK_EQ((int)c.out.size(), (int)OPT_COUNT);
        for (const auto& kv : c.out) {
            const int id = optionFind(kv.first.c_str());
            if (!RT_CHECK(id >= 0) || !RT_CHECK_EQ(optionToString(dec, id), kv.second)) {
                std::fprintf(stderr, "  case %d option %s\n", c.index, kv.first.c_str());
            }
        }
        std::string seed2;
        Options dec2;
        RT_CHECK_EQ((int)decodePermalink("  " + c.permalink + "\n", &seed2, &dec2), (int)PermalinkError::None);
        RT_CHECK(seed2 == seed && optionsEqual(dec2, dec)); // stripped, as wwrando does
        if (!RT_CHECK_EQ(encodePermalink(seed, dec), c.permalink)) {
            std::fprintf(stderr, "  case %d (re-encode)\n", c.index);
        }

        if (!RT_CHECK_EQ(seedHash(c.permalink, in, c.key.c_str()), c.hash)) {
            std::fprintf(stderr, "  case %d (seed hash)\n", c.index);
        }
        keyedHashes += !c.key.empty() && in.do_not_generate_spoiler_log;
        uint8_t d[16];
        integerSeedMd5(c.seed, in, d, c.key.c_str());
        RT_CHECK_EQ(hex(d, 16), c.intseed);
    }
    RT_CHECK(keyedHashes > 0); // the seed key reaches the hash in some cases
}

void checkErrors() {
    std::string seed = "unchanged";
    Options o;
    o.keylunacy = true;
    const Options before = o;
    RT_CHECK_EQ((int)decodePermalink("  \n", &seed, &o), (int)PermalinkError::Blank);
    RT_CHECK_EQ((int)decodePermalink("QUJD=", &seed, &o), (int)PermalinkError::Malformed); // b64decode: "ABC"
    RT_CHECK_EQ((int)decodePermalink("QUI", &seed, &o), (int)PermalinkError::BadBase64);
    RT_CHECK_EQ((int)decodePermalink("QQ", &seed, &o), (int)PermalinkError::BadBase64);
    RT_CHECK_EQ((int)decodePermalink(b64("1.10.0_9775811"), &seed, &o), (int)PermalinkError::Malformed);
    RT_CHECK_EQ((int)decodePermalink(b64(std::string("1.10.0_9775811\0x", 16)), &seed, &o), (int)PermalinkError::Malformed);
    const std::string full = encodePermalink("abc", Options());
    // The test's base64 agrees with the permalink's on the version prefix.
    RT_CHECK_EQ(b64(std::string("1.10.0_9775811\0abc\0", 19)).substr(0, 24), full.substr(0, 24));
    RT_CHECK_EQ((int)decodePermalink(b64(std::string("1.9.0\0abc\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 31)),
                                     &seed, &o),
                (int)PermalinkError::WrongVersion);
    const std::string otherCommit("1.10.0_1234567\0abc\0", 19);
    {
        // a whole default option block taken from our own permalink
        std::string seed2;
        Options d;
        RT_CHECK_EQ((int)decodePermalink(full, &seed2, &d), (int)PermalinkError::None);
        RT_CHECK_EQ(seed2, std::string("abc"));
        RT_CHECK(optionsEqual(d, Options()));
    }
    {
        // Build "1.10.0_1234567\0abc\0" + 21 zero bytes: a commit mismatch, accepted on request.
        const std::string link = b64(otherCommit + std::string(21, '\0'));
        RT_CHECK_EQ((int)decodePermalink(link, &seed, &o), (int)PermalinkError::WrongCommit);
        RT_CHECK(seed == "unchanged" && optionsEqual(o, before));
        std::string s2;
        Options o2;
        RT_CHECK_EQ((int)decodePermalink(link, &s2, &o2, true), (int)PermalinkError::None);
        RT_CHECK_EQ(s2, std::string("abc"));
        // All-zero bits: every int at its minimum, starting gear empty.
        RT_CHECK_EQ(o2.num_required_bosses, 1);
        RT_CHECK_EQ(o2.starting_hcs, 1);
        RT_CHECK(o2.starting_gear.empty());
        RT_CHECK_EQ((int)o2.randomized_gear.size(), 74);
    }
    // Truncated option bits.
    RT_CHECK_EQ((int)decodePermalink(b64(std::string("1.10.0_9775811\0abc\0\0\0\0", 22)), &seed, &o),
                (int)PermalinkError::Truncated);
    // An enum index past the end: sword_mode is bits 24-25 (after 24 bools); 3 is invalid.
    {
        std::string bits(21, '\0');
        bits[3] = 0x03;
        RT_CHECK_EQ((int)decodePermalink(b64(std::string("1.10.0_9775811\0abc\0", 19) + bits), &seed, &o),
                    (int)PermalinkError::BadEnum);
    }
    RT_CHECK(seed == "unchanged" && optionsEqual(o, before));
    // An int read outside its range goes back to its default (num_required_bosses: 3 bits, 1..6).
    {
        std::string bits(21, '\0');
        bits[3] = (char)(7 << 3); // bit 24-25 sword_mode, 26 required_bosses, 27-29 num_required_bosses
        std::string s2;
        Options o2;
        RT_CHECK_EQ((int)decodePermalink(b64(std::string("1.10.0_9775811\0abc\0", 19) + bits), &s2, &o2),
                    (int)PermalinkError::None);
        RT_CHECK_EQ(o2.num_required_bosses, 4);
    }
    // Palettes: with enemy rando off, the permalink's bit is ignored and the given value kept.
    {
        Options p;
        p.randomize_enemy_palettes = true;
        const std::string link = encodePermalink("x", p); // written as 0
        Options q;
        q.randomize_enemy_palettes = true;
        std::string s2;
        RT_CHECK_EQ((int)decodePermalink(link, &s2, &q), (int)PermalinkError::None);
        RT_CHECK(q.randomize_enemy_palettes);
        p.randomize_enemy_palettes = false;
        RT_CHECK_EQ(encodePermalink("x", p), link);
    }
    // Seeds.
    RT_CHECK_EQ(sanitizeSeed("  Hello World!\t"), std::string("HelloWorld"));
    RT_CHECK_EQ(sanitizeSeed(std::string(50, 'a')), std::string(42, 'a'));
    RT_CHECK_EQ(sanitizeSeed("caf\xc3\xa9-_'%.9"), std::string("caf-_'%.9"));
}

void checkValidate() {
    Options o;
    o.num_required_bosses = 0;
    o.starting_hcs = 10;
    o.num_item_hints = 15;
    o.starting_pohs = -1;
    validateOptions(o);
    RT_CHECK_EQ(o.num_required_bosses, 4);
    RT_CHECK_EQ(o.starting_hcs, 3);
    RT_CHECK_EQ(o.num_item_hints, 15);
    RT_CHECK_EQ(o.starting_pohs, 0);
}

int count(const std::vector<std::string>& v, const char* s) {
    int n = 0;
    for (const auto& x : v) {
        n += x == s;
    }
    return n;
}

void checkEnsureValid() {
    {
        Options o;
        ensureValidCombination(o);
        RT_CHECK(optionsEqual(o, Options()));
    }
    {
        Options o;
        o.progression_dungeons = false;
        o.required_bosses = true;
        o.num_location_hints = 0;
        o.prioritize_remote_hints = true;
        o.randomize_dungeon_entrances = true;
        o.mix_entrances = EntranceMixMode::MixDungeons;
        o.randomize_enemies = true;
        RT_CHECK(!optionEnabled(o, OPT_required_bosses));
        RT_CHECK(optionEnabled(o, OPT_num_required_bosses));
        RT_CHECK(!optionEnabled(o, OPT_mix_entrances));
        ensureValidCombination(o);
        RT_CHECK(!o.required_bosses);
        RT_CHECK(!o.prioritize_remote_hints);
        RT_CHECK(o.mix_entrances == EntranceMixMode::MixDungeons); // a combo box keeps its value
        RT_CHECK(!o.randomize_enemies);
        RT_CHECK(!optionEnabled(o, OPT_num_required_bosses));
        o.randomize_enemies = true;
        ensureValidCombination(o, true);
        RT_CHECK(o.randomize_enemies);
    }
    {
        // Swordless with a starting sword and Hurricane Spin: both move to randomized_gear.
        Options o;
        o.sword_mode = SwordMode::Swordless;
        o.starting_gear.push_back("Progressive Sword");
        o.starting_gear.push_back("Hurricane Spin");
        std::vector<std::string>& r = o.randomized_gear;
        r.erase(std::find(r.begin(), r.end(), "Progressive Sword"));
        r.erase(std::find(r.begin(), r.end(), "Hurricane Spin"));
        ensureValidCombination(o);
        RT_CHECK_EQ(count(o.starting_gear, "Progressive Sword"), 0);
        RT_CHECK_EQ(count(o.starting_gear, "Hurricane Spin"), 0);
        RT_CHECK_EQ(count(o.randomized_gear, "Progressive Sword"), 3);
        RT_CHECK_EQ(count(o.randomized_gear, "Hurricane Spin"), 1);
        RT_CHECK(o.starting_gear == defaultStartingGear());
        RT_CHECK(std::is_sorted(o.randomized_gear.begin(), o.randomized_gear.end()));
    }
    {
        // No Starting Sword: swords leave the starting gear, Hurricane Spin stays.
        Options o;
        o.sword_mode = SwordMode::NoStartingSword;
        o.starting_gear.push_back("Progressive Sword");
        o.starting_gear.push_back("Progressive Sword");
        o.starting_gear.push_back("Hurricane Spin");
        std::vector<std::string>& r = o.randomized_gear;
        r.erase(std::find(r.begin(), r.end(), "Progressive Sword"));
        r.erase(std::find(r.begin(), r.end(), "Progressive Sword"));
        r.erase(std::find(r.begin(), r.end(), "Hurricane Spin"));
        ensureValidCombination(o);
        RT_CHECK_EQ(count(o.starting_gear, "Progressive Sword"), 0);
        RT_CHECK_EQ(count(o.starting_gear, "Hurricane Spin"), 1);
        RT_CHECK_EQ(count(o.randomized_gear, "Progressive Sword"), 3);
        RT_CHECK_EQ((int)(o.starting_gear.size() + o.randomized_gear.size()), 74);
    }
    {
        // A gear list that is not INVENTORY_ITEMS goes back to the defaults.
        Options o;
        o.starting_gear.push_back("Telescope"); // now twice in all gear
        ensureValidCombination(o);
        RT_CHECK(o.starting_gear == defaultStartingGear());
        RT_CHECK(o.randomized_gear == defaultRandomizedGear());
    }
}

void checkSeedgen() {
    uint8_t d[16];
    md5("", 0, d);
    RT_CHECK_EQ(hex(d, 16), std::string("d41d8cd98f00b204e9800998ecf8427e"));
    md5("The quick brown fox jumps over the lazy dog", 43, d);
    RT_CHECK_EQ(hex(d, 16), std::string("9e107d9d372bb6826bd81d3542a419d6"));
    const std::string longText(1000, 'x');
    md5(longText.data(), longText.size(), d);
    RT_CHECK_EQ(hex(d, 16), std::string("398533d48111e9f664b1f64cb10c4b63")); // spans several blocks

    RT_CHECK_EQ(seedHash("", Options()), std::string());
    std::map<std::string, int> seen;
    for (uint64_t e = 0; e < 200; e++) {
        const std::string name = randomSeedName(e * 0x9E3779B97F4A7C15ull);
        RT_CHECK(!name.empty() && name[0] >= 'A' && name[0] <= 'Z');
        RT_CHECK_EQ(sanitizeSeed(name), name.substr(0, kMaxSeedLength));
        seen[name]++;
    }
    RT_CHECK((int)seen.size() > 190);
    RT_CHECK_EQ(randomSeedName(12345), randomSeedName(12345));
    // The port's own seed key changes the hash of no-spoiler-log seeds only.
    Options ns;
    ns.do_not_generate_spoiler_log = true;
    const std::string link = encodePermalink("abc", ns);
    RT_CHECK(seedHash(link, ns) != seedHash(link, ns, ""));
    RT_CHECK_EQ(seedHash(link, Options()), seedHash(link, Options(), ""));
}

} // namespace

int main() {
    Fixture fx;
    if (loadFixture(fx)) {
        checkTable(fx);
        checkCases(fx);
    }
    checkErrors();
    checkValidate();
    checkEnsureValid();
    checkSeedgen();
    return rt::finish();
}
