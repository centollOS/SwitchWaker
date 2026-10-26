// The pure part of the update (pc_update_core.h; docs/AUTO_UPDATE_PLAN.md).
#include "features/pc_update_core.h"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace pc::update {

// ---- versions ------------------------------------------------------------------------------------

namespace {

bool readNumber(const char*& p, int* out) {
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    long v = 0;
    while (isdigit((unsigned char)*p)) {
        v = v * 10 + (*p - '0');
        if (v > 1000000) {
            return false;
        }
        p++;
    }
    *out = (int)v;
    return true;
}

std::string bareTag(const std::string& tag) {
    return !tag.empty() && (tag[0] == 'v' || tag[0] == 'V') ? tag.substr(1) : tag;
}

} // namespace

bool parseVersion(const char* text, Version* out) {
    if (text == nullptr) {
        return false;
    }
    const char* p = text;
    if (*p == 'v' || *p == 'V') {
        p++;
    }
    Version v;
    if (!readNumber(p, &v.major) || *p++ != '.' || !readNumber(p, &v.minor) || *p++ != '.' ||
        !readNumber(p, &v.patch)) {
        return false;
    }
    if (*p == '-') {
        // git describe's "-N-gHASH" (or "-N" once the hash is cut to fit the NACP); "-dirty" alone is the tag
        const char* q = p + 1;
        int after = 0;
        if (readNumber(q, &after) && (*q == '\0' || *q == '-')) {
            v.after = after;
        }
    } else if (*p != '\0') {
        return false;
    }
    *out = v;
    return true;
}

bool isNewer(const Version& release, const Version& running) {
    if (release.major != running.major) {
        return release.major > running.major;
    }
    if (release.minor != running.minor) {
        return release.minor > running.minor;
    }
    return release.patch > running.patch;
}

bool sameTag(const std::string& a, const std::string& b) {
    return !a.empty() && bareTag(a) == bareTag(b);
}

// ---- SHA-256 (FIPS 180-4) ------------------------------------------------------------------------

namespace {

constexpr uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

} // namespace

Sha256::Sha256()
    : h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::block(const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
    for (int i = 0; i < 64; i++) {
        const uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[i] + w[i];
        const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
    h_[5] += f;
    h_[6] += g;
    h_[7] += h;
}

void Sha256::update(const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    total_ += size;
    while (size > 0) {
        const size_t n = std::min(size, sizeof(buf_) - used_);
        memcpy(buf_ + used_, p, n);
        used_ += n;
        p += n;
        size -= n;
        if (used_ == sizeof(buf_)) {
            block(buf_);
            used_ = 0;
        }
    }
}

void Sha256::final(uint8_t out[32]) {
    const uint64_t bits = total_ * 8;
    const uint8_t one = 0x80;
    update(&one, 1);
    const uint8_t zero = 0;
    while (used_ != 56) {
        update(&zero, 1);
    }
    uint8_t len[8];
    for (int i = 0; i < 8; i++) {
        len[i] = (uint8_t)(bits >> (56 - i * 8));
    }
    update(len, 8);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(h_[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(h_[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(h_[i] >> 8);
        out[i * 4 + 3] = (uint8_t)h_[i];
    }
}

std::string toHex(const uint8_t* data, size_t size) {
    static const char kDigits[] = "0123456789abcdef";
    std::string s;
    s.reserve(size * 2);
    for (size_t i = 0; i < size; i++) {
        s += kDigits[data[i] >> 4];
        s += kDigits[data[i] & 15];
    }
    return s;
}

std::string sha256File(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        return "";
    }
    Sha256 sha;
    std::vector<uint8_t> buf(256 * 1024);
    size_t n;
    while ((n = fread(buf.data(), 1, buf.size(), f)) > 0) {
        sha.update(buf.data(), n);
    }
    const bool ok = !ferror(f);
    fclose(f);
    if (!ok) {
        return "";
    }
    uint8_t digest[32];
    sha.final(digest);
    return toHex(digest, sizeof(digest));
}

std::string parseSha256Line(const std::string& text) {
    size_t i = 0;
    while (i < text.size() && isspace((unsigned char)text[i])) {
        i++;
    }
    std::string hex;
    while (i < text.size() && isxdigit((unsigned char)text[i]) && hex.size() < 65) {
        hex += (char)tolower((unsigned char)text[i++]);
    }
    if (hex.size() != 64 || (i < text.size() && !isspace((unsigned char)text[i]))) {
        return "";
    }
    return hex;
}

// ---- the release JSON ----------------------------------------------------------------------------
//
// A small JSON reader: it walks the whole document (so a malformed answer is an error) and keeps
// only the strings and numbers the release fields need.

namespace {

struct Json {
    const std::string& s;
    size_t i = 0;
    std::string error;

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
            i++;
        }
    }
    bool fail(const char* what) {
        if (error.empty()) {
            error = std::string(what) + " at byte " + std::to_string(i);
        }
        return false;
    }
    bool eat(char c) {
        ws();
        if (i < s.size() && s[i] == c) {
            i++;
            return true;
        }
        return false;
    }
    static void utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    bool hex4(uint32_t* out) {
        if (i + 4 > s.size()) {
            return fail("short \\u escape");
        }
        uint32_t v = 0;
        for (int k = 0; k < 4; k++) {
            const char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= (uint32_t)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= (uint32_t)(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= (uint32_t)(c - 'A' + 10);
            } else {
                return fail("bad \\u escape");
            }
        }
        *out = v;
        return true;
    }
    bool string(std::string* out) {
        ws();
        if (i >= s.size() || s[i] != '"') {
            return fail("string expected");
        }
        i++;
        std::string v;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if ((unsigned char)c < 0x20) {
                return fail("control character in a string");
            }
            if (c != '\\') {
                v += c;
                continue;
            }
            if (i >= s.size()) {
                break;
            }
            c = s[i++];
            switch (c) {
            case '"': v += '"'; break;
            case '\\': v += '\\'; break;
            case '/': v += '/'; break;
            case 'b': v += '\b'; break;
            case 'f': v += '\f'; break;
            case 'n': v += '\n'; break;
            case 'r': v += '\r'; break;
            case 't': v += '\t'; break;
            case 'u': {
                uint32_t cp;
                if (!hex4(&cp)) {
                    return false;
                }
                if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    i += 2;
                    uint32_t lo;
                    if (!hex4(&lo)) {
                        return false;
                    }
                    cp = lo >= 0xDC00 && lo < 0xE000 ? 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00) : 0xFFFD;
                } else if (cp >= 0xD800 && cp < 0xE000) {
                    cp = 0xFFFD;
                }
                utf8(v, cp);
                break;
            }
            default:
                return fail("bad escape");
            }
        }
        if (i >= s.size()) {
            return fail("unterminated string");
        }
        i++;
        if (out != nullptr) {
            *out = std::move(v);
        }
        return true;
    }
    bool number(double* out) {
        ws();
        const size_t start = i;
        if (i < s.size() && s[i] == '-') {
            i++;
        }
        while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                                s[i] == '+' || s[i] == '-')) {
            i++;
        }
        if (i == start) {
            return fail("number expected");
        }
        if (out != nullptr) {
            *out = strtod(s.substr(start, i - start).c_str(), nullptr);
        }
        return true;
    }
    bool literal(const char* word) {
        ws();
        const size_t n = strlen(word);
        if (s.compare(i, n, word) != 0) {
            return fail("value expected");
        }
        i += n;
        return true;
    }
    // Any value, thrown away.
    bool skip(int depth = 0) {
        if (depth > 64) {
            return fail("too deep");
        }
        ws();
        if (i >= s.size()) {
            return fail("value expected");
        }
        switch (s[i]) {
        case '"':
            return string(nullptr);
        case '{':
            return object(depth, [&](const std::string&) { return skip(depth + 1); });
        case '[':
            return array([&] { return skip(depth + 1); });
        case 't':
            return literal("true");
        case 'f':
            return literal("false");
        case 'n':
            return literal("null");
        default:
            return number(nullptr);
        }
    }
    template <class F> bool object(int, F member) {
        if (!eat('{')) {
            return fail("object expected");
        }
        if (eat('}')) {
            return true;
        }
        do {
            std::string key;
            if (!string(&key) || !eat(':')) {
                return fail("member expected");
            }
            if (!member(key)) {
                return false;
            }
        } while (eat(','));
        return eat('}') || fail("'}' expected");
    }
    template <class F> bool array(F element) {
        if (!eat('[')) {
            return fail("array expected");
        }
        if (eat(']')) {
            return true;
        }
        do {
            if (!element()) {
                return false;
            }
        } while (eat(','));
        return eat(']') || fail("']' expected");
    }
    // A string, or null as "".
    bool stringOrNull(std::string* out) {
        ws();
        if (i < s.size() && s[i] == 'n') {
            out->clear();
            return literal("null");
        }
        return string(out);
    }
};

} // namespace

bool parseRelease(const std::string& json, Release* out, std::string* error) {
    Json j{json};
    Release r;
    const bool ok = j.object(0, [&](const std::string& key) {
        if (key == "tag_name") {
            return j.string(&r.tag);
        }
        if (key == "name") {
            return j.stringOrNull(&r.name);
        }
        if (key == "html_url") {
            return j.string(&r.htmlUrl);
        }
        if (key == "body") {
            return j.stringOrNull(&r.body);
        }
        if (key == "assets") {
            return j.array([&] {
                Asset a;
                const bool assetOk = j.object(1, [&](const std::string& k) {
                    if (k == "name") {
                        return j.string(&a.name);
                    }
                    if (k == "browser_download_url") {
                        return j.string(&a.url);
                    }
                    if (k == "size") {
                        double d = 0;
                        if (!j.number(&d)) {
                            return false;
                        }
                        a.size = d > 0 ? (uint64_t)d : 0;
                        return true;
                    }
                    return j.skip(2);
                });
                if (assetOk) {
                    r.assets.push_back(std::move(a));
                }
                return assetOk;
            });
        }
        return j.skip(1);
    });
    j.ws();
    if (ok && j.i != json.size()) {
        j.fail("text after the document");
    }
    if (!ok || !j.error.empty()) {
        if (error != nullptr) {
            *error = j.error.empty() ? "malformed JSON" : j.error;
        }
        return false;
    }
    if (r.tag.empty()) {
        if (error != nullptr) {
            *error = "no tag_name";
        }
        return false;
    }
    *out = std::move(r);
    return true;
}

const Asset* findAsset(const Release& release, const std::string& name) {
    for (const Asset& a : release.assets) {
        if (a.name == name) {
            return &a;
        }
    }
    return nullptr;
}

// ---- release notes ---------------------------------------------------------------------------------

namespace {

// UTF-8 to what the menu's Latin-1 font can draw.
std::string latin1Text(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        uint32_t cp = c;
        size_t n = 1;
        if (c >= 0xF0 && i + 3 < s.size()) {
            cp = (c & 0x07u) << 18 | ((unsigned char)s[i + 1] & 0x3Fu) << 12 | ((unsigned char)s[i + 2] & 0x3Fu) << 6 |
                 ((unsigned char)s[i + 3] & 0x3Fu);
            n = 4;
        } else if (c >= 0xE0 && i + 2 < s.size()) {
            cp = (c & 0x0Fu) << 12 | ((unsigned char)s[i + 1] & 0x3Fu) << 6 | ((unsigned char)s[i + 2] & 0x3Fu);
            n = 3;
        } else if (c >= 0xC0 && i + 1 < s.size()) {
            cp = (c & 0x1Fu) << 6 | ((unsigned char)s[i + 1] & 0x3Fu);
            n = 2;
        } else if (c >= 0x80) {
            i++; // a stray continuation byte
            continue;
        }
        i += n;
        switch (cp) {
        case 0x2018: case 0x2019: case 0x201A: case 0x2032: out += '\''; continue;
        case 0x201C: case 0x201D: case 0x201E: case 0x2033: out += '"'; continue;
        case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: case 0x2212: out += '-'; continue;
        case 0x2026: out += "..."; continue;
        case 0x2022: case 0x2023: case 0x25CF: out += '-'; continue;
        case 0x2192: case 0x21D2: case 0x27F6: out += "->"; continue;
        case 0x2190: out += "<-"; continue;
        case 0x00A0: case 0x2002: case 0x2003: case 0x2009: case 0x202F: out += ' '; continue;
        case 0x2264: out += "<="; continue;
        case 0x2265: out += ">="; continue;
        case 0x2248: out += "~"; continue;
        default: break;
        }
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp <= 0xFF) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } // emoji and the rest: the font has no glyph
    }
    return out;
}

// One line's inline Markdown: emphasis and code marks go, links and images keep their text.
std::string inlineText(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        const char c = s[i];
        if (c == '\\' && i + 1 < s.size() && strchr("\\`*_[]()#+-.!|<>", s[i + 1]) != nullptr) {
            out += s[i + 1];
            i += 2;
            continue;
        }
        if (c == '`') {
            i++;
            continue;
        }
        if ((c == '*' || c == '_') && i + 1 < s.size() && s[i + 1] == c) {
            i += 2;
            continue;
        }
        if (c == '*' && (i + 1 < s.size() && s[i + 1] != ' ')) {
            i++; // *emphasis*
            continue;
        }
        if ((c == '[' || (c == '!' && i + 1 < s.size() && s[i + 1] == '['))) {
            const size_t open = c == '!' ? i + 1 : i;
            const size_t close = s.find(']', open);
            if (close != std::string::npos && close + 1 < s.size() && s[close + 1] == '(') {
                const size_t end = s.find(')', close + 2);
                if (end != std::string::npos) {
                    out += inlineText(s.substr(open + 1, close - open - 1));
                    i = end + 1;
                    continue;
                }
            }
        }
        if (c == '<' && (s.compare(i + 1, 7, "http://") == 0 || s.compare(i + 1, 8, "https://") == 0)) {
            const size_t end = s.find('>', i);
            if (end != std::string::npos) {
                out += s.substr(i + 1, end - i - 1);
                i = end + 1;
                continue;
            }
        }
        out += c;
        i++;
    }
    return out;
}

std::string trimmed(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) {
        a++;
    }
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        b--;
    }
    return s.substr(a, b - a);
}

} // namespace

std::string notesFromMarkdown(const std::string& markdown) {
    std::vector<std::string> lines;
    int skipLevel = 0; // inside an "Install..." section of this heading level
    size_t start = 0;
    while (start <= markdown.size()) {
        size_t nl = markdown.find('\n', start);
        if (nl == std::string::npos) {
            nl = markdown.size();
        }
        std::string raw = markdown.substr(start, nl - start);
        start = nl + 1;
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }
        const std::string t = trimmed(raw);
        if (t.rfind("<!--", 0) == 0) {
            continue;
        }
        int level = 0;
        while (level < (int)t.size() && t[level] == '#') {
            level++;
        }
        const bool heading = level > 0 && level <= 6 && level < (int)t.size() && t[level] == ' ';
        if (heading) {
            const std::string text = trimmed(t.substr(level + 1));
            if (skipLevel != 0 && level > skipLevel) {
                continue;
            }
            skipLevel = 0;
            std::string lower;
            for (char ch : text) {
                lower += (char)tolower((unsigned char)ch);
            }
            if (lower.rfind("install", 0) == 0) {
                skipLevel = level;
                continue;
            }
            if (!lines.empty() && lines.back().empty()) {
                lines.pop_back(); // the menu spaces headings itself
            }
            lines.push_back("# " + latin1Text(inlineText(text)));
            continue;
        }
        if (skipLevel != 0) {
            continue;
        }
        if (t.empty()) {
            if (!lines.empty() && !lines.back().empty() && lines.back().rfind("# ", 0) != 0) {
                lines.push_back("");
            }
            continue;
        }
        if (t[0] == '|') {
            bool separator = true;
            for (char ch : t) {
                separator = separator && (ch == '|' || ch == '-' || ch == ':' || ch == ' ');
            }
            if (separator) {
                continue;
            }
            std::string row;
            size_t p = 1;
            while (p < t.size()) {
                size_t bar = t.find('|', p);
                if (bar == std::string::npos) {
                    bar = t.size();
                }
                const std::string cell = trimmed(t.substr(p, bar - p));
                if (!cell.empty()) {
                    row += (row.empty() ? "" : " \xC2\xB7 ") + cell;
                }
                p = bar + 1;
            }
            lines.push_back(latin1Text(inlineText(row)));
            continue;
        }
        size_t indent = 0;
        while (indent < raw.size() && (raw[indent] == ' ' || raw[indent] == '\t')) {
            indent += raw[indent] == '\t' ? 4 : 1;
        }
        if (t.size() > 1 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ') {
            lines.push_back(std::string((indent / 2) * 2, ' ') + "- " + latin1Text(inlineText(trimmed(t.substr(2)))));
            continue;
        }
        lines.push_back(std::string((indent / 2) * 2, ' ') + latin1Text(inlineText(t)));
    }
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    std::string out;
    for (const std::string& l : lines) {
        out += l + "\n";
    }
    return out;
}

std::string zipNameFor(const std::string& tag, const char* gameId) {
    if (gameId == nullptr) {
        return "";
    }
    if (strcmp(gameId, "GZLE01") == 0) {
        return "SwitchWaker-" + tag + "-switch.zip";
    }
    if (strcmp(gameId, "GZLP01") == 0) {
        return "SwitchWaker-" + tag + "-switch-eur.zip";
    }
    return "";
}

// ---- the zip reader --------------------------------------------------------------------------------

namespace {

uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

bool seekTo(FILE* f, uint64_t offset) {
    return fseeko(f, (off_t)offset, SEEK_SET) == 0;
}

bool readAt(FILE* f, uint64_t offset, void* out, size_t size) {
    return seekTo(f, offset) && fread(out, 1, size, f) == size;
}

bool zipFail(std::string* error, const std::string& what) {
    if (error != nullptr) {
        *error = what;
    }
    return false;
}

} // namespace

bool zipList(FILE* f, std::vector<ZipEntry>* out, std::string* error) {
    if (fseeko(f, 0, SEEK_END) != 0) {
        return zipFail(error, "cannot seek");
    }
    const off_t end = ftello(f);
    if (end < 22) {
        return zipFail(error, "too short for a zip");
    }
    // The end of central directory record: the last 22 bytes plus a comment of up to 64 KiB.
    const uint64_t tail = std::min<uint64_t>((uint64_t)end, 22 + 65535);
    std::vector<uint8_t> buf(tail);
    if (!readAt(f, (uint64_t)end - tail, buf.data(), buf.size())) {
        return zipFail(error, "cannot read the end of the zip");
    }
    long eocd = -1;
    for (long k = (long)tail - 22; k >= 0; k--) {
        if (le32(&buf[k]) == 0x06054b50) {
            eocd = k;
            break;
        }
    }
    if (eocd < 0) {
        return zipFail(error, "no end of central directory (not a zip)");
    }
    const uint8_t* e = &buf[eocd];
    const uint16_t count = le16(e + 10);
    const uint32_t cdSize = le32(e + 12);
    const uint32_t cdOffset = le32(e + 16);
    if (count == 0xFFFF || cdSize == 0xFFFFFFFF || cdOffset == 0xFFFFFFFF) {
        return zipFail(error, "zip64 is not supported");
    }
    if ((uint64_t)cdOffset + cdSize > (uint64_t)end) {
        return zipFail(error, "central directory outside the file");
    }
    std::vector<uint8_t> cd(cdSize);
    if (cdSize > 0 && !readAt(f, cdOffset, cd.data(), cd.size())) {
        return zipFail(error, "cannot read the central directory");
    }
    std::vector<ZipEntry> entries;
    size_t p = 0;
    for (int k = 0; k < count; k++) {
        if (p + 46 > cd.size() || le32(&cd[p]) != 0x02014b50) {
            return zipFail(error, "bad central directory entry");
        }
        const uint8_t* h = &cd[p];
        ZipEntry z;
        const uint16_t flags = le16(h + 8);
        z.method = le16(h + 10);
        z.crc = le32(h + 16);
        z.packedSize = le32(h + 20);
        z.size = le32(h + 24);
        const uint16_t nameLen = le16(h + 28);
        const uint16_t extraLen = le16(h + 30);
        const uint16_t commentLen = le16(h + 32);
        z.localOffset = le32(h + 42);
        if (p + 46 + nameLen + extraLen + commentLen > cd.size()) {
            return zipFail(error, "central directory entry past its end");
        }
        z.name.assign(reinterpret_cast<const char*>(h + 46), nameLen);
        if (flags & 1) {
            return zipFail(error, "encrypted entry " + z.name);
        }
        if (z.packedSize == 0xFFFFFFFF || z.size == 0xFFFFFFFF || z.localOffset == 0xFFFFFFFF) {
            return zipFail(error, "zip64 entry " + z.name);
        }
        entries.push_back(std::move(z));
        p += 46 + nameLen + extraLen + commentLen;
    }
    *out = std::move(entries);
    return true;
}

bool zipExtract(FILE* f, const ZipEntry& z, const char* outPath, std::string* error) {
    uint8_t lh[30];
    if (!readAt(f, z.localOffset, lh, sizeof(lh)) || le32(lh) != 0x04034b50) {
        return zipFail(error, "bad local header of " + z.name);
    }
    const uint64_t dataOffset = z.localOffset + 30 + le16(lh + 26) + le16(lh + 28);
    if (!seekTo(f, dataOffset)) {
        return zipFail(error, "cannot seek to " + z.name);
    }
    if (z.method != 0 && z.method != 8) {
        return zipFail(error, "unsupported compression method " + std::to_string(z.method) + " of " + z.name);
    }
    remove(outPath);
    FILE* out = fopen(outPath, "wb");
    if (out == nullptr) {
        return zipFail(error, std::string("cannot write ") + outPath + ": " + strerror(errno));
    }
    std::vector<uint8_t> in(64 * 1024), buf(256 * 1024);
    uLong crc = crc32(0, nullptr, 0);
    uint64_t written = 0;
    uint64_t left = z.packedSize;
    bool ok = true;
    std::string why;
    z_stream zs{};
    if (z.method == 8 && inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        ok = false;
        why = "inflateInit failed";
    }
    bool done = false;
    while (ok && !done) {
        const size_t want = (size_t)std::min<uint64_t>(left, in.size());
        const size_t got = want > 0 ? fread(in.data(), 1, want, f) : 0;
        if (got != want) {
            ok = false;
            why = "short read of " + z.name;
            break;
        }
        left -= got;
        if (z.method == 0) {
            if (got > 0 && fwrite(in.data(), 1, got, out) != got) {
                ok = false;
                why = std::string("cannot write ") + outPath;
                break;
            }
            crc = crc32(crc, in.data(), (uInt)got);
            written += got;
            done = left == 0;
            continue;
        }
        zs.next_in = in.data();
        zs.avail_in = (uInt)got;
        do {
            zs.next_out = buf.data();
            zs.avail_out = (uInt)buf.size();
            const int rc = inflate(&zs, Z_NO_FLUSH);
            if (rc != Z_OK && rc != Z_STREAM_END && !(rc == Z_BUF_ERROR && got == 0)) {
                ok = false;
                why = "corrupt data in " + z.name;
                break;
            }
            const size_t n = buf.size() - zs.avail_out;
            if (n > 0 && fwrite(buf.data(), 1, n, out) != n) {
                ok = false;
                why = std::string("cannot write ") + outPath;
                break;
            }
            crc = crc32(crc, buf.data(), (uInt)n);
            written += n;
            if (rc == Z_STREAM_END) {
                done = true;
                break;
            }
            if (got == 0 && n == 0) {
                ok = false;
                why = "truncated data in " + z.name;
                break;
            }
        } while (zs.avail_out == 0 || zs.avail_in > 0);
    }
    if (z.method == 8) {
        inflateEnd(&zs);
    }
    if (fflush(out) != 0) {
        ok = false;
        why = std::string("cannot write ") + outPath;
    }
    fclose(out);
    if (ok && written != z.size) {
        ok = false;
        why = z.name + ": " + std::to_string(written) + " bytes, the zip says " + std::to_string(z.size);
    }
    if (ok && (uint32_t)crc != z.crc) {
        ok = false;
        why = z.name + ": CRC-32 mismatch";
    }
    if (!ok) {
        remove(outPath);
        return zipFail(error, why);
    }
    return true;
}

bool installName(const std::string& entry, std::string* name) {
    static const char kPrefix[] = "switch/switchwaker/";
    const size_t n = sizeof(kPrefix) - 1;
    if (entry.compare(0, n, kPrefix) != 0 || entry.size() == n) {
        return false;
    }
    const std::string rest = entry.substr(n);
    if (rest == "." || rest == ".." || rest.find_first_of("/\\:") != std::string::npos) {
        return false;
    }
    *name = rest;
    return true;
}

// ---- the journal -----------------------------------------------------------------------------------

namespace {

std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) {
        s += (s.empty() ? "" : ",") + x;
    }
    return s;
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v;
    size_t start = 0;
    while (start < s.size()) {
        size_t comma = s.find(',', start);
        if (comma == std::string::npos) {
            comma = s.size();
        }
        if (comma > start) {
            v.push_back(s.substr(start, comma - start));
        }
        start = comma + 1;
    }
    return v;
}

} // namespace

std::string formatJournal(const Journal& j) {
    return "state=" + j.state + "\ntag=" + j.tag + "\nfrom=" + j.from + "\nnames=" + join(j.names) +
           "\nexisted=" + join(j.existed) + "\n";
}

bool parseJournal(const std::string& text, Journal* out) {
    Journal j;
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            nl = text.size();
        }
        std::string line = text.substr(start, nl - start);
        start = nl + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "state") {
            j.state = value;
        } else if (key == "tag") {
            j.tag = value;
        } else if (key == "from") {
            j.from = value;
        } else if (key == "names") {
            j.names = split(value);
        } else if (key == "existed") {
            j.existed = split(value);
        }
    }
    if ((j.state != "installing" && j.state != "installed" && j.state != "booting") || j.tag.empty() ||
        j.names.empty()) {
        return false;
    }
    for (const std::string& n : j.names) {
        std::string plain;
        if (!installName("switch/switchwaker/" + n, &plain)) {
            return false;
        }
    }
    *out = std::move(j);
    return true;
}

} // namespace pc::update
