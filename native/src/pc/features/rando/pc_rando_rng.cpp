// Python-compatible random numbers and MD5 (pc_rando_rng.h, docs/RANDOMIZER_PLAN.md step R0.2).
// Pure C++: no game headers or game calls (cos_rando_core links this file without the game).
#include "pc_rando_rng.h"

#include <cstring>

namespace pc::rando {

// ===================================================================================================
// MD5

namespace {

inline uint32_t rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

const uint32_t kMd5K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
const int kMd5S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    5, 9,  14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21,
    6, 10, 15, 21, 6, 10, 15, 21,
};

} // namespace

void Md5::reset() {
    mState[0] = 0x67452301;
    mState[1] = 0xefcdab89;
    mState[2] = 0x98badcfe;
    mState[3] = 0x10325476;
    mBytes = 0;
    mBufLen = 0;
}

void Md5::block(const uint8_t* p) {
    uint32_t m[16];
    for (int i = 0; i < 16; i++) {
        m[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 |
               (uint32_t)p[4 * i + 3] << 24;
    }
    uint32_t a = mState[0], b = mState[1], c = mState[2], d = mState[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }
        uint32_t t = d;
        d = c;
        c = b;
        b = b + rotl(a + f + kMd5K[i] + m[g], kMd5S[i]);
        a = t;
    }
    mState[0] += a;
    mState[1] += b;
    mState[2] += c;
    mState[3] += d;
}

void Md5::update(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    mBytes += len;
    while (len > 0) {
        if (mBufLen == 0 && len >= 64) {
            block(p);
            p += 64;
            len -= 64;
            continue;
        }
        size_t n = 64 - mBufLen;
        if (n > len) {
            n = len;
        }
        std::memcpy(mBuf + mBufLen, p, n);
        mBufLen += n;
        p += n;
        len -= n;
        if (mBufLen == 64) {
            block(mBuf);
            mBufLen = 0;
        }
    }
}

void Md5::finish(uint8_t out[16]) {
    uint64_t bits = mBytes * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while (mBufLen != 56) {
        update(&zero, 1);
    }
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; i++) {
        lenBytes[i] = (uint8_t)(bits >> (8 * i));
    }
    update(lenBytes, 8);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[4 * i + j] = (uint8_t)(mState[i] >> (8 * j));
        }
    }
}

void md5(const void* data, size_t len, uint8_t out[16]) {
    Md5 m;
    m.update(data, len);
    m.finish(out);
}

void md5Hex(const uint8_t digest[16], char out[33]) {
    static const char kHex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[2 * i] = kHex[digest[i] >> 4];
        out[2 * i + 1] = kHex[digest[i] & 15];
    }
    out[32] = '\0';
}

// ===================================================================================================
// MT19937, as CPython's Modules/_randommodule.c

namespace {
constexpr int kN = 624;
constexpr int kM = 397;
constexpr uint32_t kMatrixA = 0x9908b0dfu;
constexpr uint32_t kUpper = 0x80000000u;
constexpr uint32_t kLower = 0x7fffffffu;

int bitLength(uint64_t n) {
    int b = 0;
    while (n != 0) {
        b++;
        n >>= 1;
    }
    return b;
}
} // namespace

void PyRandom::initGenrand(uint32_t s) {
    mMt[0] = s;
    for (int i = 1; i < kN; i++) {
        mMt[i] = 1812433253u * (mMt[i - 1] ^ (mMt[i - 1] >> 30)) + (uint32_t)i;
    }
    mMti = kN;
}

void PyRandom::initByArray(const uint32_t* key, size_t len) {
    initGenrand(19650218u);
    size_t i = 1, j = 0;
    size_t k = (size_t)kN > len ? (size_t)kN : len;
    for (; k != 0; k--) {
        mMt[i] = (mMt[i] ^ ((mMt[i - 1] ^ (mMt[i - 1] >> 30)) * 1664525u)) + key[j] + (uint32_t)j;
        i++;
        j++;
        if (i >= (size_t)kN) {
            mMt[0] = mMt[kN - 1];
            i = 1;
        }
        if (j >= len) {
            j = 0;
        }
    }
    for (k = kN - 1; k != 0; k--) {
        mMt[i] = (mMt[i] ^ ((mMt[i - 1] ^ (mMt[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
        i++;
        if (i >= (size_t)kN) {
            mMt[0] = mMt[kN - 1];
            i = 1;
        }
    }
    mMt[0] = 0x80000000u;
    mMti = kN;
}

void PyRandom::seedInt(uint64_t n) {
    uint32_t key[2] = {(uint32_t)n, (uint32_t)(n >> 32)};
    initByArray(key, key[1] != 0 ? 2 : 1);
}

void PyRandom::seedBigEndian(const uint8_t* bytes, size_t len) {
    // Skip leading zero bytes; the key is the little-endian words of what is left (at least one word).
    while (len > 0 && bytes[0] == 0) {
        bytes++;
        len--;
    }
    size_t words = len == 0 ? 1 : (len + 3) / 4;
    std::vector<uint32_t> key(words, 0);
    for (size_t b = 0; b < len; b++) {
        size_t bitPos = 8 * (len - 1 - b); // bytes[len-1] is the least significant
        key[bitPos / 32] |= (uint32_t)bytes[b] << (bitPos % 32);
    }
    initByArray(key.data(), words);
}

void PyRandom::seedMd5Of(const void* data, size_t len) {
    uint8_t d[16];
    md5(data, len, d);
    seedBigEndian(d, 16);
}

uint32_t PyRandom::genrand() {
    static const uint32_t mag01[2] = {0, kMatrixA};
    uint32_t y;
    if (mMti >= kN) {
        int kk;
        for (kk = 0; kk < kN - kM; kk++) {
            y = (mMt[kk] & kUpper) | (mMt[kk + 1] & kLower);
            mMt[kk] = mMt[kk + kM] ^ (y >> 1) ^ mag01[y & 1];
        }
        for (; kk < kN - 1; kk++) {
            y = (mMt[kk] & kUpper) | (mMt[kk + 1] & kLower);
            mMt[kk] = mMt[kk + (kM - kN)] ^ (y >> 1) ^ mag01[y & 1];
        }
        y = (mMt[kN - 1] & kUpper) | (mMt[0] & kLower);
        mMt[kN - 1] = mMt[kM - 1] ^ (y >> 1) ^ mag01[y & 1];
        mMti = 0;
    }
    y = mMt[mMti++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y;
}

void PyRandom::getrandbitsWords(int k, uint32_t* out) {
    if (k <= 0) {
        return;
    }
    if (k <= 32) {
        out[0] = genrand() >> (32 - k);
        return;
    }
    for (int i = 0; k > 0; i++, k -= 32) {
        uint32_t r = genrand();
        if (k < 32) {
            r >>= (32 - k);
        }
        out[i] = r;
    }
}

uint64_t PyRandom::getrandbits(int k) {
    if (k <= 0) {
        return 0;
    }
    if (k <= 32) {
        return genrand() >> (32 - k);
    }
    uint64_t v = 0;
    for (int i = 0; k > 0; i++, k -= 32) {
        uint32_t r = genrand();
        if (k < 32) {
            r >>= (32 - k);
        }
        if (i < 2) {
            v |= (uint64_t)r << (32 * i);
        }
    }
    return v;
}

uint64_t PyRandom::randbelow(uint64_t n) {
    if (n == 0) {
        return 0;
    }
    int k = bitLength(n);
    uint64_t r = getrandbits(k);
    while (r >= n) {
        r = getrandbits(k);
    }
    if (mTrace != nullptr) {
        mTrace(mTraceUser, kTraceRandbelow, n, r);
    }
    return r;
}

double PyRandom::random() {
    uint32_t a = genrand() >> 5;
    uint32_t b = genrand() >> 6;
    if (mTrace != nullptr) {
        mTrace(mTraceUser, kTraceRandom, 0, (uint64_t)a * 67108864u + b);
    }
    return ((double)a * 67108864.0 + (double)b) * (1.0 / 9007199254740992.0);
}

int64_t PyRandom::randrange(int64_t start, int64_t stop) {
    if (stop <= start) {
        return start;
    }
    return start + (int64_t)randbelow((uint64_t)(stop - start));
}

void PyRandom::sampleIndices(size_t n, size_t k, std::vector<size_t>& out) {
    out.clear();
    if (k > n) {
        return; // Python raises ValueError
    }
    out.resize(k);
    size_t setsize = 21;
    if (k > 5) {
        // 4 ** ceil(log(3k, 4)); 3k is never a power of 4, so this is the smallest power above 3k.
        size_t p = 1;
        while (p < 3 * k) {
            p *= 4;
        }
        setsize += p;
    }
    if (n <= setsize) {
        std::vector<size_t> pool(n);
        for (size_t i = 0; i < n; i++) {
            pool[i] = i;
        }
        for (size_t i = 0; i < k; i++) {
            size_t j = (size_t)randbelow(n - i);
            out[i] = pool[j];
            pool[j] = pool[n - i - 1];
        }
    } else {
        for (size_t i = 0; i < k; i++) {
            size_t j;
            bool seen;
            do {
                j = (size_t)randbelow(n);
                seen = false;
                for (size_t q = 0; q < i; q++) {
                    if (out[q] == j) {
                        seen = true;
                        break;
                    }
                }
            } while (seen);
            out[i] = j;
        }
    }
}

size_t PyRandom::weightedIndex(const uint64_t* counts, size_t n) {
    // cum_counts = accumulate(counts); total = cum_counts.pop(); s = sample(range(total), 1)[0];
    // population[bisect_right(cum_counts, s)].
    uint64_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += counts[i];
    }
    std::vector<size_t> sel;
    sampleIndices((size_t)total, 1, sel);
    uint64_t s = sel.empty() ? 0 : sel[0];
    uint64_t cum = 0;
    size_t idx = 0;
    for (; idx + 1 < n; idx++) { // cum_counts without the popped total
        cum += counts[idx];
        if (cum > s) {
            break;
        }
    }
    return idx;
}

size_t PyRandom::choicesIndex(const double* weights, size_t n) {
    if (n == 0) {
        return 0;
    }
    std::vector<double> cum(n);
    double acc = 0;
    for (size_t i = 0; i < n; i++) {
        acc += weights[i];
        cum[i] = acc;
    }
    double x = random() * (cum[n - 1] + 0.0);
    // bisect(cum_weights, x, 0, n-1): first index in [0, n-1) whose value is > x, else n-1.
    size_t lo = 0, hi = n - 1;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (x < cum[mid]) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

bool PyRandom::operator==(const PyRandom& o) const {
    return mMti == o.mMti && std::memcmp(mMt, o.mMt, sizeof mMt) == 0;
}

// ===================================================================================================
// WWRandomizer seed handling

size_t sanitizeSeed(const char* seed, char out[kMaxSeedLength + 1]) {
    size_t n = 0;
    for (const char* p = seed; p != nullptr && *p != '\0' && n < kMaxSeedLength; p++) {
        unsigned char c = (unsigned char)*p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                  c == '_' || c == '\'' || c == '%' || c == '.';
        if (ok) {
            out[n++] = (char)c;
        }
    }
    out[n] = '\0';
    return n;
}

void integerSeedMd5(const char* sanitizedSeed, bool doNotGenerateSpoilerLog, const char* seedKey,
                    uint8_t out[16]) {
    Md5 m;
    m.update(sanitizedSeed, std::strlen(sanitizedSeed));
    if (doNotGenerateSpoilerLog && seedKey != nullptr) {
        m.update(seedKey, std::strlen(seedKey));
    }
    m.finish(out);
}

void getNewRng(PyRandom& rng, const uint8_t integerSeed[16], const int optionValues[kRngChangingOptionCount]) {
    rng.seedBigEndian(integerSeed, 16);
    for (int i = 0; i < kRngChangingOptionCount; i++) {
        for (int j = 1; j < 100 + i; j++) {
            rng.getrandbits(optionValues[i] + 20 * i + j);
        }
    }
}

void getNewRng(PyRandom& rng, const char* sanitizedSeed, const int optionValues[kRngChangingOptionCount],
               const char* seedKey) {
    uint8_t d[16];
    integerSeedMd5(sanitizedSeed, optionValues[kRngOptDoNotGenerateSpoilerLog] != 0, seedKey, d);
    getNewRng(rng, d, optionValues);
}

} // namespace pc::rando
