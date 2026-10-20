// Python-compatible random numbers and MD5 for the randomizer's seed generator
// (docs/RANDOMIZER_PLAN.md, step R0.2, decision D1).
//
// A seed made by SwitchWaker must place every item where wwrando 1.10.0_9775811 does. wwrando draws all
// its randomness from CPython's `random.Random` (MT19937), so this file reproduces CPython 3.12's
// algorithms bit for bit:
//   - Random.seed(int): abs(n) split into little-endian 32-bit words, trimmed to n.bit_length() (at
//     least one word), then init_by_array;
//   - getrandbits(k): k <= 32 is genrand >> (32-k); k > 32 takes ceil(k/32) words, low word first,
//     the last one shifted right; k = 0 takes nothing;
//   - _randbelow(n): getrandbits(n.bit_length()) until < n. choice, shuffle, sample (also with
//     counts=, i.e. WWRandomizer.weighted_choice), randrange and randint all draw through it;
//   - random(): two words, (a>>5 * 2^26 + b>>6) / 2^53; only choices() (hints) uses it;
//   - WWRandomizer.get_new_rng(): md5 of the sanitized seed (+ the seed key when there is no spoiler
//     log) and the warm-up over the 11 RNG_CHANGING_OPTIONS.
// Checked against CPython by cos_rando_test_R0_2 with vectors from native/tools/rando/gen_rng_vectors.py.
//
// Pure C++: no game headers and no game calls (cos_rando_core links it without the game; see
// native/check/rando/README.md). Not thread-safe per object; each generator owns its PyRandom.
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace pc::rando {

// ---------------------------------------------------------------------------------------------------
// MD5 (RFC 1321), as Python's hashlib.md5.

class Md5 {
public:
    Md5() { reset(); }
    void reset();
    void update(const void* data, size_t len);
    // Writes the 16-byte digest (hashlib's digest() order). The object must be reset() before reuse.
    void finish(uint8_t out[16]);

private:
    void block(const uint8_t* p);
    uint32_t mState[4];
    uint64_t mBytes;
    uint8_t mBuf[64];
    size_t mBufLen;
};

void md5(const void* data, size_t len, uint8_t out[16]);
// hexdigest(): 32 lowercase hex characters plus the terminator.
void md5Hex(const uint8_t digest[16], char out[33]);

// ---------------------------------------------------------------------------------------------------
// CPython random.Random.

class PyRandom {
public:
    // Called after each draw when set (for COS_RANDO_TRACE, ww_trace.py's format): randbelow(n) -> r,
    // and random() -> its value * 2^53 (with n = 0).
    enum TraceKind { kTraceRandbelow, kTraceRandom };
    typedef void (*TraceFn)(void* user, TraceKind kind, uint64_t n, uint64_t result);

    PyRandom() { seedInt(0); }

    // Random.seed(n) for an integer 0 <= n < 2^64 (Python seeds with abs(n)).
    void seedInt(uint64_t n);
    // Random.seed(int.from_bytes(bytes, "big")) for an integer of any size.
    void seedBigEndian(const uint8_t* bytes, size_t len);
    // Random.seed(int(hashlib.md5(data).hexdigest(), 16)).
    void seedMd5Of(const void* data, size_t len);
    // MT19937's init_by_array, as CPython's (len >= 1).
    void initByArray(const uint32_t* key, size_t len);

    // One 32-bit output of MT19937 (getrandbits(32)).
    uint32_t genrand();
    // getrandbits(k) for any k >= 0; returns the low 64 bits (all of it when k <= 64).
    uint64_t getrandbits(int k);
    // getrandbits(k) as ceil(k/32) little-endian 32-bit words in out (none when k = 0).
    void getrandbitsWords(int k, uint32_t* out);
    // Random._randbelow(n), n > 0 (n = 0 returns 0 and draws nothing; Python would raise).
    uint64_t randbelow(uint64_t n);
    // Random.random(): a double in [0, 1) with 53 random bits.
    double random();
    // randrange(start, stop) (step 1) and randint(a, b) = randrange(a, b + 1). stop > start.
    int64_t randrange(int64_t start, int64_t stop);
    int64_t randint(int64_t a, int64_t b) { return randrange(a, b + 1); }

    // choice(seq): the index of the element chosen from a sequence of n > 0 elements. A one-element
    // sequence still draws.
    size_t choiceIndex(size_t n) { return (size_t)randbelow(n); }
    template <class T> T& choice(std::vector<T>& v) { return v[choiceIndex(v.size())]; }
    template <class T> const T& choice(const std::vector<T>& v) { return v[choiceIndex(v.size())]; }

    // shuffle(x) in place: i from n-1 down to 1, swap x[i] with x[randbelow(i+1)].
    template <class T> void shuffle(T* x, size_t n) {
        for (size_t i = n; i-- > 1;) {
            size_t j = (size_t)randbelow(i + 1);
            std::swap(x[i], x[j]);
        }
    }
    template <class T> void shuffle(std::vector<T>& v) { shuffle(v.data(), v.size()); }

    // sample(range(n), k): the k selected indices, in selection order (0 <= k <= n). Pool method when
    // n <= 21 (+ 4**ceil(log4(3k)) when k > 5), else rejection against the indices already chosen.
    void sampleIndices(size_t n, size_t k, std::vector<size_t>& out);
    template <class T> std::vector<T> sample(const std::vector<T>& pop, size_t k) {
        std::vector<size_t> idx;
        sampleIndices(pop.size(), k, idx);
        std::vector<T> res;
        res.reserve(k);
        for (size_t i : idx) {
            res.push_back(pop[i]);
        }
        return res;
    }

    // sample(population, 1, counts=counts)[0] as an index: what WWRandomizer.weighted_choice does with
    // its integer weights. counts are >= 0 with a positive total.
    size_t weightedIndex(const uint64_t* counts, size_t n);
    // choices(population, weights=weights)[0] as an index (hints.py's barren zones): bisect_right of
    // random() * total in the running sums, capped at n-1. weights as Python has them (ints or floats).
    size_t choicesIndex(const double* weights, size_t n);

    void setTrace(TraceFn fn, void* user) {
        mTrace = fn;
        mTraceUser = user;
    }

    // getstate()[1]: the 624 MT words and the index (copy the object to save and restore a state).
    const uint32_t* stateWords() const { return mMt; }
    int stateIndex() const { return mMti; }
    bool operator==(const PyRandom& o) const;

private:
    void initGenrand(uint32_t s);

    uint32_t mMt[624];
    int mMti = 625;
    TraceFn mTrace = nullptr;
    void* mTraceUser = nullptr;
};

// ---------------------------------------------------------------------------------------------------
// WWRandomizer's seed handling.

// WWRandomizer.MAX_SEED_LENGTH.
constexpr size_t kMaxSeedLength = 42;
// len(RNG_CHANGING_OPTIONS) in randomizer.py: fishmen_hints, hoho_hints, korl_hints, num_path_hints,
// num_barren_hints, num_location_hints, num_item_hints, cryptic_hints, prioritize_remote_hints,
// hint_importance, do_not_generate_spoiler_log (in that order; bools are 0 or 1).
constexpr int kRngChangingOptionCount = 11;
constexpr int kRngOptDoNotGenerateSpoilerLog = 10;

// WWRandomizer.sanitize_seed: keeps the characters -_'%. and ASCII letters and digits, at most 42.
// out holds kMaxSeedLength + 1 bytes; returns the length.
size_t sanitizeSeed(const char* seed, char out[kMaxSeedLength + 1]);

// WWRandomizer.integer_seed as its 16 md5 bytes: md5(sanitized seed + seedKey when
// do_not_generate_spoiler_log). seedKey may be null (wwrando's source checkout has SEED_KEY = "").
void integerSeedMd5(const char* sanitizedSeed, bool doNotGenerateSpoilerLog, const char* seedKey,
                    uint8_t out[16]);

// WWRandomizer.get_new_rng(): rng.seed(integer_seed), then for each RNG-changing option i with value v,
// getrandbits(v + 20*i + j) for j in 1..99+i.
void getNewRng(PyRandom& rng, const uint8_t integerSeed[16], const int optionValues[kRngChangingOptionCount]);
// The same from the sanitized seed, the options and the key.
void getNewRng(PyRandom& rng, const char* sanitizedSeed, const int optionValues[kRngChangingOptionCount],
               const char* seedKey);

} // namespace pc::rando
