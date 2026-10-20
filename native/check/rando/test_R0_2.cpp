// cos_rando_test_R0_2 (docs/RANDOMIZER_PLAN.md step R0.2): the Python-compatible RNG and MD5
// (native/src/pc/features/rando/pc_rando_rng.h) against CPython 3.12 and wwrando 1.10.0_9775811.
//
// The fixture fixtures/R0_2_rng_vectors.txt comes from native/tools/rando/gen_rng_vectors.py (format in
// its docstring): md5 of byte strings of every length around the block boundaries, then per case a seed
// (WWRandomizer.get_new_rng: sanitized seed, key, md5, warm-up over RNG_CHANGING_OPTIONS) or a plain
// Random(n), the MT state after it, and 1000 (or 100) outputs of each primitive, every op block from a
// fresh copy of the case's RNG. Prints "ok" and exits 0 when everything matches.
#include "rando_test.h"

#include "pc_rando_rng.h"

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using pc::rando::PyRandom;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string t;
    while (in >> t) {
        out.push_back(t);
    }
    return out;
}

uint64_t toU64(const std::string& s) { return std::strtoull(s.c_str(), nullptr, 10); }
int64_t toI64(const std::string& s) { return std::strtoll(s.c_str(), nullptr, 10); }

std::vector<uint8_t> fromHex(const std::string& s) {
    std::vector<uint8_t> out;
    if (s == "-") {
        return out;
    }
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        out.push_back((uint8_t)std::strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return out;
}

// Python's format(v, "x") for getrandbits(k) given as little-endian words.
std::string wordsHex(const uint32_t* w, int words) {
    std::string s;
    char b[16];
    int top = words - 1;
    while (top > 0 && w[top] == 0) {
        top--;
    }
    std::snprintf(b, sizeof b, "%x", w[top]);
    s += b;
    for (int i = top - 1; i >= 0; i--) {
        std::snprintf(b, sizeof b, "%08x", w[i]);
        s += b;
    }
    return s;
}

// Splits "<op> <args...> : <results...>".
void argsResults(const std::vector<std::string>& tok, std::vector<std::string>& args,
                 std::vector<std::string>& res) {
    args.clear();
    res.clear();
    bool after = false;
    for (size_t i = 1; i < tok.size(); i++) {
        if (tok[i] == ":") {
            after = true;
        } else {
            (after ? res : args).push_back(tok[i]);
        }
    }
}

struct Counts {
    int md5 = 0, cases = 0, outputs = 0;
};

// Checks one op line against `base` (a fresh copy for the first line of each op block). `cur` is the
// running RNG of the current block, `curOp` its op name.
bool runOp(const std::string& caseName, const std::vector<std::string>& tok, const PyRandom& base, PyRandom& cur,
           std::string& curOp, Counts& counts) {
    const std::string& op = tok[0];
    if (op != curOp) {
        cur = base;
        curOp = op;
    }
    std::vector<std::string> args, res;
    argsResults(tok, args, res);
    char where[96];
    std::snprintf(where, sizeof where, "%s/%s", caseName.c_str(), op.c_str());
    int bad = 0;
    auto expect = [&](bool ok, size_t i, const std::string& want, const std::string& got) {
        if (!ok && bad++ < 3) {
            std::fprintf(stderr, "FAIL %s output %zu: want %s, got %s\n", where, i, want.c_str(), got.c_str());
        }
        counts.outputs++;
    };
    if (op == "u32") {
        for (size_t i = 0; i < res.size(); i++) {
            uint32_t v = cur.genrand();
            expect(v == toU64(res[i]), i, res[i], std::to_string(v));
        }
    } else if (op == "bits") {
        RT_CHECK_EQ(args.size(), res.size());
        for (size_t i = 0; i < res.size() && i < args.size(); i++) {
            int k = (int)toI64(args[i]);
            uint32_t w[4] = {0, 0, 0, 0};
            cur.getrandbitsWords(k, w);
            std::string got = wordsHex(w, k == 0 ? 1 : (k + 31) / 32);
            expect(got == res[i], i, res[i], got);
        }
    } else if (op == "below") {
        RT_CHECK_EQ(args.size(), res.size());
        for (size_t i = 0; i < res.size() && i < args.size(); i++) {
            uint64_t v = cur.randbelow(toU64(args[i]));
            expect(v == toU64(res[i]), i, res[i], std::to_string(v));
        }
    } else if (op == "random") {
        for (size_t i = 0; i < res.size(); i++) {
            double x = cur.random();
            uint64_t v = (uint64_t)(x * 9007199254740992.0);
            expect(v == toU64(res[i]), i, res[i], std::to_string(v));
        }
    } else if (op == "randint" || op == "randrange") {
        RT_CHECK_EQ(args.size(), 2 * res.size());
        for (size_t i = 0; i < res.size() && 2 * i + 1 < args.size(); i++) {
            int64_t a = toI64(args[2 * i]), b = toI64(args[2 * i + 1]);
            int64_t v = op == "randint" ? cur.randint(a, b) : cur.randrange(a, b);
            expect(v == toI64(res[i]), i, res[i], std::to_string(v));
        }
    } else if (op == "choice") {
        RT_CHECK_EQ(args.size(), res.size());
        for (size_t i = 0; i < res.size() && i < args.size(); i++) {
            std::vector<int> seq((size_t)toU64(args[i]));
            for (size_t q = 0; q < seq.size(); q++) {
                seq[q] = (int)q;
            }
            int v = cur.choice(seq);
            expect(v == (int)toI64(res[i]), i, res[i], std::to_string(v));
        }
    } else if (op == "shuffle") {
        size_t n = (size_t)toU64(args.at(0));
        std::vector<int> x(n);
        for (size_t q = 0; q < n; q++) {
            x[q] = (int)q;
        }
        cur.shuffle(x);
        RT_CHECK_EQ(res.size(), n);
        for (size_t i = 0; i < n && i < res.size(); i++) {
            expect(x[i] == (int)toI64(res[i]), i, res[i], std::to_string(x[i]));
        }
    } else if (op == "sample") {
        size_t n = (size_t)toU64(args.at(0)), k = (size_t)toU64(args.at(1));
        std::vector<int> pop(n);
        for (size_t q = 0; q < n; q++) {
            pop[q] = (int)q;
        }
        std::vector<int> s = cur.sample(pop, k);
        RT_CHECK_EQ(res.size(), k);
        RT_CHECK_EQ(s.size(), k);
        for (size_t i = 0; i < k && i < res.size() && i < s.size(); i++) {
            expect(s[i] == (int)toI64(res[i]), i, res[i], std::to_string(s[i]));
        }
    } else if (op == "weighted") {
        std::vector<uint64_t> c;
        for (const std::string& a : args) {
            c.push_back(toU64(a));
        }
        size_t v = cur.weightedIndex(c.data(), c.size());
        expect(res.size() == 1 && v == (size_t)toU64(res[0]), 0, res.empty() ? "?" : res[0], std::to_string(v));
    } else if (op == "choices") {
        std::vector<double> w;
        for (const std::string& a : args) {
            w.push_back((double)toI64(a));
        }
        size_t v = cur.choicesIndex(w.data(), w.size());
        expect(res.size() == 1 && v == (size_t)toU64(res[0]), 0, res.empty() ? "?" : res[0], std::to_string(v));
    } else {
        std::fprintf(stderr, "FAIL unknown op '%s' in the fixture\n", op.c_str());
        return false;
    }
    if (bad != 0) {
        std::fprintf(stderr, "FAIL %s: %d mismatch(es) on one line\n", where, bad);
    }
    return bad == 0;
}

void checkMd5Basics() {
    // RFC 1321 appendix A.5.
    const char* in[] = {"", "a", "abc", "message digest", "abcdefghijklmnopqrstuvwxyz",
                        "12345678901234567890123456789012345678901234567890123456789012345678901234567890"};
    const char* want[] = {"d41d8cd98f00b204e9800998ecf8427e", "0cc175b9c0f1b6a831c399e269772661",
                          "900150983cd24fb0d6963f7d28e17f72", "f96b697d7cb7938d525a2f31aaf161d0",
                          "c3fcd3d76192e4007dfb496cca67e13b", "57edf4a22be3c955ac49da2e2107b67a"};
    for (int i = 0; i < 6; i++) {
        uint8_t d[16];
        char hex[33];
        pc::rando::md5(in[i], std::strlen(in[i]), d);
        pc::rando::md5Hex(d, hex);
        RT_CHECK_EQ(std::string(hex), std::string(want[i]));
    }
    // Incremental updates in odd pieces give the one-shot digest.
    std::string big;
    for (int i = 0; i < 1000; i++) {
        big += (char)(i * 31);
    }
    uint8_t one[16], inc[16];
    pc::rando::md5(big.data(), big.size(), one);
    pc::rando::Md5 m;
    for (size_t p = 0, step = 1; p < big.size(); p += step, step = step % 70 + 3) {
        m.update(big.data() + p, step < big.size() - p ? step : big.size() - p);
    }
    m.finish(inc);
    RT_CHECK(std::memcmp(one, inc, 16) == 0);
}

void checkSanitize() {
    char out[pc::rando::kMaxSeedLength + 1];
    RT_CHECK_EQ(pc::rando::sanitizeSeed("  a b\tc/\xc3\xa9-_'%.Z9  ", out), (size_t)10);
    RT_CHECK_EQ(std::string(out), std::string("abc-_'%.Z9"));
    std::string longSeed(100, 'x');
    RT_CHECK_EQ(pc::rando::sanitizeSeed(longSeed.c_str(), out), pc::rando::kMaxSeedLength);
    RT_CHECK_EQ(pc::rando::sanitizeSeed("", out), (size_t)0);
}

int sTraceCalls = 0;
void traceFn(void*, PyRandom::TraceKind, uint64_t, uint64_t) { sTraceCalls++; }

void checkTrace() {
    PyRandom r;
    r.seedInt(5);
    r.setTrace(traceFn, nullptr);
    std::vector<int> v = {1, 2, 3, 4};
    r.shuffle(v);      // 3 draws
    r.choice(v);       // 1
    r.random();        // 1
    r.getrandbits(40); // not traced
    RT_CHECK_EQ(sTraceCalls, 5);
}

} // namespace

int main() {
    checkMd5Basics();
    checkSanitize();
    checkTrace();

    std::string text;
    if (!rt::readFixture("R0_2_rng_vectors.txt", text)) {
        return rt::finish();
    }
    Counts counts;
    std::istringstream in(text);
    std::string line;
    std::string caseName, curOp;
    PyRandom base, cur;
    bool haveCase = false;
    int badLines = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<std::string> tok = split(line);
        const std::string& kind = tok[0];
        if (kind == "md5") {
            std::vector<uint8_t> data = fromHex(tok.at(1));
            uint8_t d[16];
            char hex[33];
            pc::rando::md5(data.data(), data.size(), d);
            pc::rando::md5Hex(d, hex);
            RT_CHECK_EQ(std::string(hex), tok.at(2));
            counts.md5++;
        } else if (kind == "case") {
            caseName = tok.at(1);
            curOp.clear();
            haveCase = false;
            counts.cases++;
        } else if (kind == "seed") {
            // seed <sanitized> <key|-> <md5 hex> <11 values>
            RT_CHECK_EQ(tok.size(), (size_t)(4 + pc::rando::kRngChangingOptionCount));
            int values[pc::rando::kRngChangingOptionCount] = {};
            for (int i = 0; i < pc::rando::kRngChangingOptionCount && 4 + i < (int)tok.size(); i++) {
                values[i] = (int)toI64(tok[4 + i]);
            }
            const char* key = tok.at(2) == "-" ? nullptr : tok[2].c_str();
            uint8_t d[16];
            char hex[33];
            pc::rando::integerSeedMd5(tok.at(1).c_str(),
                                      values[pc::rando::kRngOptDoNotGenerateSpoilerLog] != 0, key, d);
            pc::rando::md5Hex(d, hex);
            RT_CHECK_EQ(std::string(hex), tok.at(3));
            pc::rando::getNewRng(base, tok[1].c_str(), values, key);
            PyRandom viaDigest;
            pc::rando::getNewRng(viaDigest, d, values);
            RT_CHECK(viaDigest == base);
            haveCase = true;
        } else if (kind == "int") {
            // Random(n) seeds with abs(n), as big-endian bytes of any length.
            std::string s = tok.at(1);
            if (!s.empty() && s[0] == '-') {
                s = s.substr(1);
            }
            // decimal -> big-endian bytes (16 bytes is enough for the fixture's values)
            uint8_t be[16] = {};
            for (char c : s) {
                unsigned carry = (unsigned)(c - '0');
                for (int i = 15; i >= 0; i--) {
                    unsigned v = be[i] * 10u + carry;
                    be[i] = (uint8_t)v;
                    carry = v >> 8;
                }
            }
            base.seedBigEndian(be, 16);
            if (s.size() <= 19 && toU64(s) <= UINT64_MAX) {
                PyRandom viaInt;
                viaInt.seedInt(toU64(s));
                RT_CHECK(viaInt == base);
            }
            haveCase = true;
        } else if (kind == "state") {
            // getstate() after seeding (+ warm-up): the first 8 MT words and the index.
            RT_CHECK_EQ(tok.size(), (size_t)10);
            for (int i = 0; i < 8 && 1 + i < (int)tok.size(); i++) {
                RT_CHECK_EQ((uint64_t)base.stateWords()[i], toU64(tok[1 + i]));
            }
            RT_CHECK_EQ((int64_t)base.stateIndex(), toI64(tok.at(9)));
            RT_CHECK(haveCase);
        } else {
            if (!haveCase) {
                RT_CHECK(haveCase);
                continue;
            }
            if (!runOp(caseName, tok, base, cur, curOp, counts)) {
                badLines++;
                rt::failures()++;
                if (badLines > 20) {
                    std::fprintf(stderr, "FAIL too many mismatching lines, stopping\n");
                    break;
                }
            }
        }
    }
    RT_CHECK(counts.md5 >= 130);
    RT_CHECK_EQ(counts.cases, 13);
    RT_CHECK(counts.outputs > 50000);
    std::printf("R0_2: %d md5 vectors, %d cases, %d RNG outputs checked\n", counts.md5, counts.cases,
                counts.outputs);
    return rt::finish();
}
