// Permalinks (docs/RANDOMIZER_PLAN.md step R1.2): WWRandomizer.sanitize_seed / encode_permalink /
// decode_permalink and packedbits.py of wwrando 1.10.0_9775811, byte for byte. Pure C++
// (cos_rando_core).
//
// Layout: base64( VERSION ascii, \0, seed ascii, \0, packed bytes ). The packed bits follow
// Options.all() order, skipping permalink=False options, LSB first in each byte:
//   bool 1 bit; StrEnum its index in (len-1).bit_length() bits; int value-min in
//   (max-min).bit_length() bits; starting_gear 1 bit per REGULAR_ITEMS entry, then 2 bits (a count)
//   per sorted unique progressive item; randomized_gear no bits (derived from starting_gear).
// randomize_enemy_palettes is written as 0 unless randomize_enemies. The writer always flushes the
// last byte, even when it is empty.
#include "pc_rando_options.h"

#include <algorithm>
#include <cstring>

namespace pcr {

namespace {

int bitLength(unsigned v) {
    int n = 0;
    while (v != 0) {
        n++;
        v >>= 1;
    }
    return n;
}

// packedbits.PackedBitsWriter
struct BitsWriter {
    int bitsLeft = 8;
    unsigned cur = 0;
    std::string bytes;

    void write(unsigned value, int length) {
        while (length != 0) {
            const int n = length >= bitsLeft ? bitsLeft : length;
            const unsigned mask = (1u << n) - 1;
            cur |= (value & mask) << (8 - bitsLeft);
            bitsLeft -= n;
            length -= n;
            value >>= n;
            if (bitsLeft != 0) {
                continue;
            }
            flush();
        }
    }
    void flush() {
        bytes += (char)(uint8_t)cur;
        cur = 0;
        bitsLeft = 8;
    }
};

// packedbits.PackedBitsReader; reading past the end sets `overrun` (Python raises IndexError).
struct BitsReader {
    const uint8_t* bytes;
    size_t size;
    size_t byteIndex = 0;
    int bitIndex = 0;
    bool overrun = false;

    unsigned read(int length) {
        int bitsRead = 0;
        unsigned value = 0;
        int left = length;
        while (bitsRead != length) {
            int n = left > 8 ? 8 : left;
            if (n + bitIndex > 8) {
                n = 8 - bitIndex;
            }
            if (byteIndex >= size) {
                overrun = true;
                return 0;
            }
            const unsigned mask = ((1u << n) - 1) << bitIndex;
            value = ((bytes[byteIndex] & mask) >> bitIndex) << bitsRead | value;
            bitIndex += n;
            byteIndex += (size_t)(bitIndex >> 3);
            bitIndex %= 8;
            left -= n;
            bitsRead += n;
        }
        return value;
    }
};

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::string& in) {
    std::string out;
    const auto* p = (const uint8_t*)in.data();
    size_t i = 0;
    for (; i + 3 <= in.size(); i += 3) {
        const unsigned v = (unsigned)p[i] << 16 | (unsigned)p[i + 1] << 8 | p[i + 2];
        out += kB64[v >> 18 & 63];
        out += kB64[v >> 12 & 63];
        out += kB64[v >> 6 & 63];
        out += kB64[v & 63];
    }
    if (in.size() - i == 1) {
        const unsigned v = (unsigned)p[i] << 16;
        out += kB64[v >> 18 & 63];
        out += kB64[v >> 12 & 63];
        out += "==";
    } else if (in.size() - i == 2) {
        const unsigned v = (unsigned)p[i] << 16 | (unsigned)p[i + 1] << 8;
        out += kB64[v >> 18 & 63];
        out += kB64[v >> 12 & 63];
        out += kB64[v >> 6 & 63];
        out += '=';
    }
    return out;
}

int b64Value(char c) {
    const char* p = std::strchr(kB64, c);
    return c != '\0' && p != nullptr ? (int)(p - kB64) : -1;
}

// base64.b64decode(s) with validate=False: characters outside the alphabet are discarded; the data
// stops at the first padding that completes a quantum; a quantum left incomplete without enough
// padding is an error (binascii "Incorrect padding"), as is a single leftover character.
bool base64Decode(const std::string& in, std::string* out) {
    std::string res;
    unsigned acc = 0;
    int quad = 0;
    int pads = 0;
    bool done = false;
    for (char c : in) {
        if (c == '=') {
            if (quad >= 2 && quad + ++pads >= 4) {
                done = true;
                break;
            }
            continue;
        }
        const int v = b64Value(c);
        if (v < 0) {
            continue;
        }
        acc = acc << 6 | (unsigned)v;
        quad++;
        if (quad == 4) {
            res += (char)(uint8_t)(acc >> 16);
            res += (char)(uint8_t)(acc >> 8);
            res += (char)(uint8_t)acc;
            acc = 0;
            quad = 0;
        }
    }
    if (done) {
        if (quad == 2) {
            res += (char)(uint8_t)(acc >> 4);
        } else {
            res += (char)(uint8_t)(acc >> 10);
            res += (char)(uint8_t)(acc >> 2);
        }
    } else if (quad != 0) {
        return false;
    }
    *out = res;
    return true;
}

bool isAscii(const std::string& s) {
    for (char c : s) {
        if ((uint8_t)c >= 0x80) {
            return false;
        }
    }
    return true;
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

std::string strip(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && isSpace(s[b])) {
        b++;
    }
    while (e > b && isSpace(s[e - 1])) {
        e--;
    }
    return s.substr(b, e - b);
}

int progressiveCount(const std::vector<std::string>& v, const char* item) {
    int n = 0;
    for (const std::string& s : v) {
        n += s == item;
    }
    return n;
}

bool contains(const std::vector<std::string>& v, const char* item) {
    for (const std::string& s : v) {
        if (s == item) {
            return true;
        }
    }
    return false;
}

} // namespace

std::string sanitizeSeed(const std::string& seed) {
    // str.strip() then the VALID_SEED_CHARACTERS filter; whitespace is never valid, so the filter
    // alone gives the same result. Non-ASCII (UTF-8) bytes are never valid either.
    std::string out;
    for (char c : seed) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '-' || c == '_' || c == '\'' || c == '%' || c == '.';
        if (ok) {
            out += c;
            if ((int)out.size() == kMaxSeedLength) {
                break;
            }
        }
    }
    return out;
}

std::string encodePermalink(const std::string& seed, const Options& o) {
    std::string raw = kWwrandoVersion;
    raw += '\0';
    raw += sanitizeSeed(seed);
    raw += '\0';

    BitsWriter w;
    for (int id = 0; id < OPT_COUNT; id++) {
        const OptionInfo& info = kOptionInfo[id];
        if ((info.flags & kOptPermalink) == 0) {
            continue;
        }
        int value = optionGetInt(o, id);
        if (id == OPT_randomize_enemy_palettes && !o.randomize_enemies) {
            value = 0;
        }
        switch (info.kind) {
        case OptKind::Bool:
            w.write((unsigned)value, 1);
            break;
        case OptKind::Enum:
            w.write((unsigned)value, bitLength((unsigned)(info.numChoices - 1)));
            break;
        case OptKind::Int:
            w.write((unsigned)(value - info.minimum), bitLength((unsigned)(info.maximum - info.minimum)));
            break;
        case OptKind::List:
            if (id == OPT_starting_gear) {
                for (int i = 0; i < kRegularItemCount; i++) {
                    w.write(contains(o.starting_gear, kRegularItems[i]) ? 1u : 0u, 1);
                }
                for (int i = 0; i < kProgressiveItemCount; i++) {
                    w.write((unsigned)progressiveCount(o.starting_gear, kProgressiveItems[i]), 2);
                }
            }
            // randomized_gear: no bits
            break;
        case OptKind::Str:
        case OptKind::Dict:
            break; // never in the permalink
        }
    }
    w.flush();
    raw += w.bytes;
    return base64Encode(raw);
}

const char* permalinkErrorText(PermalinkError e) {
    switch (e) {
    case PermalinkError::None:
        return "ok";
    case PermalinkError::Blank:
        return "Permalink is blank.";
    case PermalinkError::BadBase64:
        return "Permalink is not valid base64.";
    case PermalinkError::Malformed:
        return "Permalink is malformed.";
    case PermalinkError::WrongVersion:
        return "The given permalink is for another version of the randomizer.";
    case PermalinkError::WrongCommit:
        return "The given permalink is for another commit of this randomizer version.";
    case PermalinkError::Truncated:
        return "Permalink is too short for its options.";
    case PermalinkError::BadEnum:
        return "Permalink holds an invalid option value.";
    }
    return "?";
}

PermalinkError decodePermalink(const std::string& permalink, std::string* seedOut, Options* opts,
                               bool allowDifferentCommit) {
    const std::string text = strip(permalink);
    if (text.empty()) {
        return PermalinkError::Blank;
    }
    std::string raw;
    if (!base64Decode(text, &raw)) {
        return PermalinkError::BadBase64;
    }
    const size_t z1 = raw.find('\0');
    const size_t z2 = z1 == std::string::npos ? std::string::npos : raw.find('\0', z1 + 1);
    if (z2 == std::string::npos) {
        return PermalinkError::Malformed;
    }
    const std::string version = raw.substr(0, z1);
    const std::string seed = raw.substr(z1 + 1, z2 - z1 - 1);
    if (!isAscii(version) || !isAscii(seed)) {
        return PermalinkError::Malformed;
    }
    if (version != kWwrandoVersion) {
        // wwrando from source: the same "1.10.0" with another commit suffix is a commit mismatch.
        const std::string given = version.substr(0, version.find('_'));
        if (given == kWwrandoVersionNoCommit) {
            if (!allowDifferentCommit) {
                return PermalinkError::WrongCommit;
            }
        } else {
            return PermalinkError::WrongVersion;
        }
    }

    Options o = *opts;
    const bool prevPalettes = o.randomize_enemy_palettes;
    BitsReader r{(const uint8_t*)raw.data() + z2 + 1, raw.size() - z2 - 1};
    for (int id = 0; id < OPT_COUNT; id++) {
        const OptionInfo& info = kOptionInfo[id];
        if ((info.flags & kOptPermalink) == 0) {
            continue;
        }
        switch (info.kind) {
        case OptKind::Bool:
            optionSetInt(o, id, (int)r.read(1));
            break;
        case OptKind::Enum: {
            const unsigned idx = r.read(bitLength((unsigned)(info.numChoices - 1)));
            if (!r.overrun && idx >= (unsigned)info.numChoices) {
                return PermalinkError::BadEnum;
            }
            optionSetInt(o, id, (int)idx);
            break;
        }
        case OptKind::Int: {
            const int v = (int)r.read(bitLength((unsigned)(info.maximum - info.minimum))) + info.minimum;
            optionSetInt(o, id, v < info.minimum || v > info.maximum ? info.def : v);
            break;
        }
        case OptKind::List:
            if (id == OPT_starting_gear) {
                std::vector<std::string> start;
                std::vector<std::string> rand;
                for (int i = 0; i < kRegularItemCount; i++) {
                    (r.read(1) == 1 ? start : rand).emplace_back(kRegularItems[i]);
                }
                for (int i = 0; i < kProgressiveItemCount; i++) {
                    const int amount = (int)r.read(2);
                    const int randAmount = kProgressiveItemCounts[i] - amount;
                    for (int n = 0; n < amount; n++) {
                        start.emplace_back(kProgressiveItems[i]);
                    }
                    for (int n = 0; n < randAmount; n++) {
                        rand.emplace_back(kProgressiveItems[i]);
                    }
                }
                std::sort(start.begin(), start.end());
                std::sort(rand.begin(), rand.end());
                o.starting_gear = start;
                o.randomized_gear = rand;
            }
            break;
        case OptKind::Str:
        case OptKind::Dict:
            break;
        }
        if (r.overrun) {
            return PermalinkError::Truncated;
        }
    }
    if (!o.randomize_enemies) {
        o.randomize_enemy_palettes = prevPalettes;
    }
    *opts = o;
    *seedOut = seed;
    return PermalinkError::None;
}

} // namespace pcr
