// Minimal JSON reader and writer (pc_rando_json.h, docs/RANDOMIZER_PLAN.md step R0.6). Pure C++: no
// game headers or calls (cos_rando_core links it into the unit tests without game code).
#include "pc_rando_json.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pcr {

namespace {

const Json kNullJson;
const std::string kEmpty;

constexpr int kMaxDepth = 200;

bool isIntegerText(const std::string& t) {
    if (t.empty()) {
        return false;
    }
    for (char c : t) {
        if (c == '.' || c == 'e' || c == 'E') {
            return false;
        }
    }
    return true;
}

} // namespace

// ---- values ---------------------------------------------------------------------------------------

Json Json::boolean(bool b) {
    Json j;
    j.mType = Bool;
    j.mBool = b;
    return j;
}

Json Json::integer(int64_t v) {
    Json j;
    j.mType = Number;
    char b[32];
    snprintf(b, sizeof b, "%lld", (long long)v);
    j.mText = b;
    return j;
}

Json Json::numberText(std::string text) {
    Json j;
    j.mType = Number;
    j.mText = std::move(text);
    return j;
}

Json Json::real(double v) {
    Json j;
    j.mType = Number;
    if (!std::isfinite(v)) {
        j.mText = "0"; // JSON has no NaN/Infinity (Python would write NaN, which strict JSON rejects)
        return j;
    }
    char b[40];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(b, sizeof b, "%.*g", prec, v);
        if (strtod(b, nullptr) == v) {
            break;
        }
    }
    std::string t = b;
    if (t.find_first_of(".eEn") == std::string::npos) {
        t += ".0";
    }
    j.mText = t;
    return j;
}

Json Json::string(std::string s) {
    Json j;
    j.mType = String;
    j.mText = std::move(s);
    return j;
}

Json Json::array() {
    Json j;
    j.mType = Array;
    return j;
}

Json Json::object() {
    Json j;
    j.mType = Object;
    return j;
}

bool Json::isInteger() const {
    if (mType != Number || !isIntegerText(mText)) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    (void)strtoll(mText.c_str(), &end, 10);
    return errno == 0 && end != nullptr && *end == '\0';
}

int64_t Json::asInt(int64_t def) const {
    if (mType != Number) {
        return def;
    }
    if (isInteger()) {
        return strtoll(mText.c_str(), nullptr, 10);
    }
    const double d = strtod(mText.c_str(), nullptr);
    if (!(d >= -9.2e18 && d <= 9.2e18)) {
        return def;
    }
    return (int64_t)d;
}

double Json::asDouble(double def) const { return mType == Number ? strtod(mText.c_str(), nullptr) : def; }

const std::string& Json::text() const { return (mType == String || mType == Number) ? mText : kEmpty; }

const Json& Json::at(size_t i) const { return i < mItems.size() ? mItems[i] : kNullJson; }

Json& Json::at(size_t i) {
    static Json sScratch;
    if (i < mItems.size()) {
        return mItems[i];
    }
    sScratch = Json();
    return sScratch;
}

Json& Json::push(Json v) {
    if (mType != Array) {
        *this = array();
    }
    mItems.push_back(std::move(v));
    return mItems.back();
}

const std::string& Json::keyAt(size_t i) const { return (mType == Object && i < mKeys.size()) ? mKeys[i] : kEmpty; }

const Json* Json::find(std::string_view key) const {
    if (mType != Object) {
        return nullptr;
    }
    for (size_t i = 0; i < mKeys.size(); i++) {
        if (mKeys[i] == key) {
            return &mItems[i];
        }
    }
    return nullptr;
}

Json* Json::find(std::string_view key) { return const_cast<Json*>(static_cast<const Json*>(this)->find(key)); }

Json& Json::set(std::string_view key, Json v) {
    if (mType != Object) {
        *this = object();
    }
    if (Json* e = find(key)) {
        *e = std::move(v);
        return *e;
    }
    mKeys.emplace_back(key);
    mItems.push_back(std::move(v));
    return mItems.back();
}

bool Json::erase(std::string_view key) {
    if (mType != Object) {
        return false;
    }
    for (size_t i = 0; i < mKeys.size(); i++) {
        if (mKeys[i] == key) {
            mKeys.erase(mKeys.begin() + (ptrdiff_t)i);
            mItems.erase(mItems.begin() + (ptrdiff_t)i);
            return true;
        }
    }
    return false;
}

bool Json::operator==(const Json& o) const {
    if (mType != o.mType) {
        return false;
    }
    switch (mType) {
    case Null:
        return true;
    case Bool:
        return mBool == o.mBool;
    case Number:
    case String:
        return mText == o.mText;
    case Array:
        return mItems == o.mItems;
    case Object:
        return mKeys == o.mKeys && mItems == o.mItems;
    }
    return false;
}

// ---- parser ---------------------------------------------------------------------------------------

namespace {

struct Parser {
    std::string_view s;
    size_t p = 0;
    std::string err;
    size_t errPos = 0;

    bool fail(const char* msg) {
        if (err.empty()) {
            err = msg;
            errPos = p;
        }
        return false;
    }

    void ws() {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) {
            p++;
        }
    }

    bool lit(const char* word) {
        const size_t n = strlen(word);
        if (s.substr(p, n) != word) {
            return fail("invalid literal");
        }
        p += n;
        return true;
    }

    static void putUtf8(std::string& o, uint32_t c) {
        if (c < 0x80) {
            o += (char)c;
        } else if (c < 0x800) {
            o += (char)(0xC0 | (c >> 6));
            o += (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            o += (char)(0xE0 | (c >> 12));
            o += (char)(0x80 | ((c >> 6) & 0x3F));
            o += (char)(0x80 | (c & 0x3F));
        } else {
            o += (char)(0xF0 | (c >> 18));
            o += (char)(0x80 | ((c >> 12) & 0x3F));
            o += (char)(0x80 | ((c >> 6) & 0x3F));
            o += (char)(0x80 | (c & 0x3F));
        }
    }

    bool hex4(uint32_t* out) {
        if (p + 4 > s.size()) {
            return fail("truncated \\u escape");
        }
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            const char c = s[p++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= (uint32_t)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= (uint32_t)(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= (uint32_t)(c - 'A' + 10);
            } else {
                p--;
                return fail("bad hex digit in \\u escape");
            }
        }
        *out = v;
        return true;
    }

    // Validates one UTF-8 sequence starting at p (raw, in a string) and copies it.
    bool utf8(std::string& o) {
        const unsigned char c0 = (unsigned char)s[p];
        int n;
        uint32_t cp;
        if (c0 >= 0xC2 && c0 <= 0xDF) {
            n = 1;
            cp = c0 & 0x1F;
        } else if (c0 >= 0xE0 && c0 <= 0xEF) {
            n = 2;
            cp = c0 & 0x0F;
        } else if (c0 >= 0xF0 && c0 <= 0xF4) {
            n = 3;
            cp = c0 & 0x07;
        } else {
            return fail("invalid UTF-8");
        }
        if (p + (size_t)n >= s.size()) {
            return fail("truncated UTF-8");
        }
        for (int i = 1; i <= n; i++) {
            const unsigned char c = (unsigned char)s[p + (size_t)i];
            if ((c & 0xC0) != 0x80) {
                return fail("invalid UTF-8");
            }
            cp = (cp << 6) | (c & 0x3F);
        }
        if ((n == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF))) {
            return fail("invalid UTF-8");
        }
        o.append(s.data() + p, (size_t)n + 1);
        p += (size_t)n + 1;
        return true;
    }

    bool str(std::string& o) {
        p++; // opening quote
        o.clear();
        while (true) {
            if (p >= s.size()) {
                return fail("unterminated string");
            }
            const unsigned char c = (unsigned char)s[p];
            if (c == '"') {
                p++;
                return true;
            }
            if (c < 0x20) {
                return fail("control character in string");
            }
            if (c >= 0x80) {
                if (!utf8(o)) {
                    return false;
                }
                continue;
            }
            if (c != '\\') {
                o += (char)c;
                p++;
                continue;
            }
            p++;
            if (p >= s.size()) {
                return fail("unterminated string");
            }
            const char e = s[p++];
            switch (e) {
            case '"': o += '"'; break;
            case '\\': o += '\\'; break;
            case '/': o += '/'; break;
            case 'b': o += '\b'; break;
            case 'f': o += '\f'; break;
            case 'n': o += '\n'; break;
            case 'r': o += '\r'; break;
            case 't': o += '\t'; break;
            case 'u': {
                uint32_t cp;
                if (!hex4(&cp)) {
                    return false;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= s.size() && s[p] == '\\' && s[p + 1] == 'u') {
                    const size_t save = p;
                    p += 2;
                    uint32_t lo;
                    if (!hex4(&lo)) {
                        return false;
                    }
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else {
                        p = save; // a lone high surrogate, then another escape
                    }
                }
                // Lone surrogates (which Python accepts) are kept as their 3-byte encoding.
                putUtf8(o, cp);
                break;
            }
            default:
                p--;
                return fail("invalid escape");
            }
        }
    }

    bool num(Json* out) {
        const size_t b = p;
        if (s[p] == '-') {
            p++;
        }
        if (p >= s.size()) {
            return fail("invalid number");
        }
        if (s[p] == '0') {
            p++;
        } else if (s[p] >= '1' && s[p] <= '9') {
            while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
                p++;
            }
        } else {
            return fail("invalid number");
        }
        if (p < s.size() && s[p] == '.') {
            p++;
            if (p >= s.size() || !(s[p] >= '0' && s[p] <= '9')) {
                return fail("invalid number");
            }
            while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
                p++;
            }
        }
        if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
            p++;
            if (p < s.size() && (s[p] == '+' || s[p] == '-')) {
                p++;
            }
            if (p >= s.size() || !(s[p] >= '0' && s[p] <= '9')) {
                return fail("invalid number");
            }
            while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
                p++;
            }
        }
        *out = Json::numberText(std::string(s.substr(b, p - b)));
        return true;
    }

    bool value(Json* out, int depth) {
        if (depth > kMaxDepth) {
            return fail("nesting too deep");
        }
        ws();
        if (p >= s.size()) {
            return fail("unexpected end of input");
        }
        const char c = s[p];
        if (c == '{') {
            p++;
            *out = Json::object();
            ws();
            if (p < s.size() && s[p] == '}') {
                p++;
                return true;
            }
            while (true) {
                ws();
                if (p >= s.size() || s[p] != '"') {
                    return fail("expected a string key");
                }
                std::string key;
                if (!str(key)) {
                    return false;
                }
                ws();
                if (p >= s.size() || s[p] != ':') {
                    return fail("expected ':'");
                }
                p++;
                Json v;
                if (!value(&v, depth + 1)) {
                    return false;
                }
                out->set(key, std::move(v));
                ws();
                if (p < s.size() && s[p] == ',') {
                    p++;
                    continue;
                }
                if (p < s.size() && s[p] == '}') {
                    p++;
                    return true;
                }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            p++;
            *out = Json::array();
            ws();
            if (p < s.size() && s[p] == ']') {
                p++;
                return true;
            }
            while (true) {
                Json v;
                if (!value(&v, depth + 1)) {
                    return false;
                }
                out->push(std::move(v));
                ws();
                if (p < s.size() && s[p] == ',') {
                    p++;
                    continue;
                }
                if (p < s.size() && s[p] == ']') {
                    p++;
                    return true;
                }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            std::string v;
            if (!str(v)) {
                return false;
            }
            *out = Json::string(std::move(v));
            return true;
        }
        if (c == 't') {
            *out = Json::boolean(true);
            return lit("true");
        }
        if (c == 'f') {
            *out = Json::boolean(false);
            return lit("false");
        }
        if (c == 'n') {
            *out = Json::null();
            return lit("null");
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return num(out);
        }
        return fail("unexpected character");
    }
};

void writeString(const std::string& v, std::string* o) {
    static const char kHex[] = "0123456789abcdef";
    auto u = [o](uint32_t c) {
        char b[8] = {'\\', 'u', kHex[(c >> 12) & 15], kHex[(c >> 8) & 15], kHex[(c >> 4) & 15], kHex[c & 15], 0, 0};
        o->append(b, 6);
    };
    *o += '"';
    for (size_t i = 0; i < v.size();) {
        const unsigned char c = (unsigned char)v[i];
        if (c < 0x80) {
            switch (c) {
            case '"': *o += "\\\""; break;
            case '\\': *o += "\\\\"; break;
            case '\n': *o += "\\n"; break;
            case '\r': *o += "\\r"; break;
            case '\t': *o += "\\t"; break;
            case '\b': *o += "\\b"; break;
            case '\f': *o += "\\f"; break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    u(c);
                } else {
                    *o += (char)c;
                }
            }
            i++;
            continue;
        }
        // Decode one UTF-8 sequence (the parser only stores valid ones; anything else is written as
        // U+FFFD so the output stays ASCII).
        int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        uint32_t cp = n == 3 ? (c & 0x07) : n == 2 ? (c & 0x0F) : (c & 0x1F);
        bool ok = n > 0 && i + (size_t)n < v.size();
        for (int k = 1; ok && k <= n; k++) {
            const unsigned char cc = (unsigned char)v[i + (size_t)k];
            ok = (cc & 0xC0) == 0x80;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) {
            u(0xFFFD);
            i++;
            continue;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            u(0xD800 + (cp >> 10));
            u(0xDC00 + (cp & 0x3FF));
        } else {
            u(cp);
        }
        i += (size_t)n + 1;
    }
    *o += '"';
}

void newline(std::string* o, int indent, int level) {
    *o += '\n';
    o->append((size_t)indent * (size_t)level, ' ');
}

void write(const Json& v, std::string* o, int indent, int level) {
    switch (v.type()) {
    case Json::Null:
        *o += "null";
        return;
    case Json::Bool:
        *o += v.asBool() ? "true" : "false";
        return;
    case Json::Number:
        *o += v.text().empty() ? std::string("0") : v.text();
        return;
    case Json::String:
        writeString(v.text(), o);
        return;
    case Json::Array:
    case Json::Object: {
        const bool obj = v.type() == Json::Object;
        *o += obj ? '{' : '[';
        if (v.size() == 0) {
            *o += obj ? '}' : ']';
            return;
        }
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) {
                *o += indent >= 0 ? "," : ", ";
            }
            if (indent >= 0) {
                newline(o, indent, level + 1);
            }
            if (obj) {
                writeString(v.keyAt(i), o);
                *o += ": ";
            }
            write(v.at(i), o, indent, level + 1);
        }
        if (indent >= 0) {
            newline(o, indent, level);
        }
        *o += obj ? '}' : ']';
        return;
    }
    }
}

} // namespace

bool jsonParse(std::string_view text, Json* out, std::string* err) {
    Parser ps;
    ps.s = text;
    if (ps.s.size() >= 3 && (unsigned char)ps.s[0] == 0xEF && (unsigned char)ps.s[1] == 0xBB &&
        (unsigned char)ps.s[2] == 0xBF) {
        ps.p = 3;
    }
    Json v;
    bool ok = ps.value(&v, 0);
    if (ok) {
        ps.ws();
        if (ps.p != ps.s.size()) {
            ok = ps.fail("extra data after the value");
        }
    }
    if (!ok) {
        *out = Json();
        if (err != nullptr) {
            int line = 1;
            int col = 1;
            for (size_t i = 0; i < ps.errPos && i < ps.s.size(); i++) {
                if (ps.s[i] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
            }
            char b[64];
            snprintf(b, sizeof b, "line %d col %d: ", line, col);
            *err = b + ps.err;
        }
        return false;
    }
    *out = std::move(v);
    return true;
}

void jsonWrite(const Json& v, std::string* out, int indent) { write(v, out, indent, 0); }

std::string jsonDump(const Json& v, int indent) {
    std::string o;
    write(v, &o, indent, 0);
    return o;
}

} // namespace pcr
