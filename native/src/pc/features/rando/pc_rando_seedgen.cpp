// Seed names and seed hashes (docs/RANDOMIZER_PLAN.md step R1.2): wwrando 1.10.0_9775811
// seedgen/seedgen.py make_random_seed_name, WWRandomizer.get_seed_hash and
// convert_string_to_integer_md5. Pure C++ (cos_rando_core).
//
// The seed hash needs CPython's random.Random (MT19937 seeded from an int with init_by_array,
// _randbelow by getrandbits rejection, sample). This file keeps a minimal private copy of exactly
// what it uses; the generator's full port of Random is R0.2's pc_rando_rng (same algorithm).
#include "pc_rando_options.h"

#include <cstring>

namespace pcr {

const char* const kSwitchWakerSeedKey = "SwitchWaker-rando-key-1";

namespace {

#include "generated/pc_rando_seedgen_words.inc"

template <class T, size_t N> constexpr size_t countOf(T (&)[N]) { return N; }

// ---------------------------------------------------------------------------------------------
// MD5 (RFC 1321)

struct Md5 {
    uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
    uint8_t buf[64];
    uint64_t len = 0;

    static uint32_t rotl(uint32_t x, int n) { return x << n | x >> (32 - n); }

    void block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
        };
        static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                  5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t m[16];
        for (int i = 0; i < 16; i++) {
            m[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 |
                   (uint32_t)p[4 * i + 3] << 24;
        }
        uint32_t A = a, B = b, C = c, D = d;
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) {
                f = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                f = (D & B) | (~D & C);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = B ^ C ^ D;
                g = (3 * i + 5) % 16;
            } else {
                f = C ^ (B | ~D);
                g = (7 * i) % 16;
            }
            const uint32_t t = D;
            D = C;
            C = B;
            B = B + rotl(A + f + K[i] + m[g], R[i]);
            A = t;
        }
        a += A;
        b += B;
        c += C;
        d += D;
    }

    void update(const uint8_t* p, size_t n) {
        size_t have = (size_t)(len % 64);
        len += n;
        while (n != 0) {
            const size_t take = 64 - have < n ? 64 - have : n;
            std::memcpy(buf + have, p, take);
            have += take;
            p += take;
            n -= take;
            if (have == 64) {
                block(buf);
                have = 0;
            }
        }
    }

    void final(uint8_t out[16]) {
        const uint64_t bits = len * 8;
        const uint8_t pad = 0x80;
        update(&pad, 1);
        const uint8_t zero = 0;
        while (len % 64 != 56) {
            update(&zero, 1);
        }
        uint8_t l[8];
        for (int i = 0; i < 8; i++) {
            l[i] = (uint8_t)(bits >> (8 * i));
        }
        update(l, 8);
        const uint32_t s[4] = {a, b, c, d};
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                out[4 * i + j] = (uint8_t)(s[i] >> (8 * j));
            }
        }
    }
};

// ---------------------------------------------------------------------------------------------
// CPython random.Random, only what get_seed_hash and make_random_seed_name use.

struct PyRandom {
    uint32_t mt[624];
    int mti = 625;

    void initGenrand(uint32_t s) {
        mt[0] = s;
        for (mti = 1; mti < 624; mti++) {
            mt[mti] = 1812433253u * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + (uint32_t)mti;
        }
    }
    void initByArray(const uint32_t* key, int len) {
        initGenrand(19650218u);
        int i = 1;
        int j = 0;
        for (int k = 624 > len ? 624 : len; k != 0; k--) {
            mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525u)) + key[j] + (uint32_t)j;
            i++;
            j++;
            if (i >= 624) {
                mt[0] = mt[623];
                i = 1;
            }
            if (j >= len) {
                j = 0;
            }
        }
        for (int k = 623; k != 0; k--) {
            mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
            i++;
            if (i >= 624) {
                mt[0] = mt[623];
                i = 1;
            }
        }
        mt[0] = 0x80000000u;
        mti = 624;
    }
    // random.seed(int(hexdigest, 16)): the 128-bit integer as 32-bit words, least significant first,
    // without the leading zero words (at least one word).
    void seedDigest(const uint8_t be[16]) {
        uint32_t w[4];
        for (int i = 0; i < 4; i++) {
            const uint8_t* p = be + 12 - 4 * i;
            w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
        }
        int used = 4;
        while (used > 1 && w[used - 1] == 0) {
            used--;
        }
        initByArray(w, used);
    }
    // random.seed(int) for a non-negative 64-bit value.
    void seedU64(uint64_t v) {
        uint32_t w[2] = {(uint32_t)v, (uint32_t)(v >> 32)};
        initByArray(w, w[1] != 0 ? 2 : 1);
    }
    uint32_t genrand() {
        static const uint32_t mag[2] = {0, 0x9908b0dfu};
        uint32_t y;
        if (mti >= 624) {
            int kk;
            for (kk = 0; kk < 624 - 397; kk++) {
                y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7fffffffu);
                mt[kk] = mt[kk + 397] ^ (y >> 1) ^ mag[y & 1];
            }
            for (; kk < 623; kk++) {
                y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7fffffffu);
                mt[kk] = mt[kk + (397 - 624)] ^ (y >> 1) ^ mag[y & 1];
            }
            y = (mt[623] & 0x80000000u) | (mt[0] & 0x7fffffffu);
            mt[623] = mt[396] ^ (y >> 1) ^ mag[y & 1];
            mti = 0;
        }
        y = mt[mti++];
        y ^= y >> 11;
        y ^= (y << 7) & 0x9d2c5680u;
        y ^= (y << 15) & 0xefc60000u;
        y ^= y >> 18;
        return y;
    }
    // getrandbits(k) for 0 < k <= 32.
    uint32_t getrandbits(int k) { return genrand() >> (32 - k); }
    // _randbelow_with_getrandbits(n), n < 2^32.
    uint32_t randbelow(uint32_t n) {
        int k = 0;
        for (uint32_t v = n; v != 0; v >>= 1) {
            k++;
        }
        uint32_t r = getrandbits(k);
        while (r >= n) {
            r = getrandbits(k);
        }
        return r;
    }
    // sample(range(n), k) as indices, k <= 5 here (no set-size growth past 21).
    void sampleIndices(uint32_t n, int k, uint32_t* out) {
        const uint32_t setsize = 21;
        if (n <= setsize) {
            uint32_t pool[21];
            for (uint32_t i = 0; i < n; i++) {
                pool[i] = i;
            }
            for (int i = 0; i < k; i++) {
                const uint32_t j = randbelow(n - (uint32_t)i);
                out[i] = pool[j];
                pool[j] = pool[n - (uint32_t)i - 1];
            }
        } else {
            for (int i = 0; i < k; i++) {
                uint32_t j = randbelow(n);
                for (;;) {
                    bool seen = false;
                    for (int s = 0; s < i; s++) {
                        seen = seen || out[s] == j;
                    }
                    if (!seen) {
                        break;
                    }
                    j = randbelow(n);
                }
                out[i] = j;
            }
        }
    }
};

void md5String(const std::string& s, uint8_t out[16]) { md5(s.data(), s.size(), out); }

// tweaks.upper_first_letter
std::string upperFirst(const char* s) {
    std::string r = s;
    if (!r.empty() && r[0] >= 'a' && r[0] <= 'z') {
        r[0] = (char)(r[0] - 'a' + 'A');
    }
    return r;
}

// seedgen's capitalization: the first ASCII letter of the word is upper-cased.
std::string capitalizeFirstLetter(const char* s) {
    std::string r = s;
    for (char& c : r) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            if (c >= 'a') {
                c = (char)(c - 'a' + 'A');
            }
            break;
        }
    }
    return r;
}

} // namespace

void md5(const void* data, size_t len, uint8_t out[16]) {
    Md5 m;
    m.update((const uint8_t*)data, len);
    m.final(out);
}

std::string seedHash(const std::string& permalink, const Options& o, const char* seedKey) {
    if (permalink.empty()) {
        return std::string();
    }
    std::string s = permalink;
    if (o.do_not_generate_spoiler_log && seedKey != nullptr) {
        s += seedKey;
    }
    uint8_t digest[16];
    md5String(s, digest);
    PyRandom rng;
    rng.seedDigest(digest);

    const char* valid[countOf(kSeedgen_names)];
    uint32_t n = 0;
    for (const char* name : kSeedgen_names) {
        if (std::strlen(name) <= 5) {
            valid[n++] = name;
        }
    }
    uint32_t pick[2];
    rng.sampleIndices(n, 2, pick);
    return upperFirst(valid[pick[0]]) + " " + upperFirst(valid[pick[1]]);
}

void integerSeedMd5(const std::string& sanitizedSeed, const Options& o, uint8_t out[16], const char* seedKey) {
    std::string s = sanitizedSeed;
    if (o.do_not_generate_spoiler_log && seedKey != nullptr) {
        s += seedKey;
    }
    md5String(s, out);
}

std::string randomSeedName(uint64_t entropy) {
    PyRandom rng;
    rng.seedU64(entropy);
    uint32_t adj[2];
    rng.sampleIndices((uint32_t)countOf(kSeedgen_adjectives), 2, adj);
    const bool useNames = rng.randbelow(2) == 1; // choice(["nouns.txt", "names.txt"])
    const char* noun = useNames ? kSeedgen_names[rng.randbelow((uint32_t)countOf(kSeedgen_names))]
                                : kSeedgen_nouns[rng.randbelow((uint32_t)countOf(kSeedgen_nouns))];
    return capitalizeFirstLetter(kSeedgen_adjectives[adj[0]]) + capitalizeFirstLetter(kSeedgen_adjectives[adj[1]]) +
           capitalizeFirstLetter(noun);
}

} // namespace pcr
