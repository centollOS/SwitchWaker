// Minimal JSON reader and writer for the randomizer's placement files (docs/RANDOMIZER_PLAN.md step
// R0.6). The build has no JSON library, so this is a small DOM:
//   - objects keep their key order (a placement written by wwrando's oracle, or by us, reads back and
//     writes out byte-identical); a repeated key keeps its first position and its last value, like
//     Python's json.load;
//   - numbers keep their source text, so they are written back exactly as read (asInt/asDouble parse
//     it on demand);
//   - jsonWrite(v, out, indent) writes like Python's json.dumps(v, indent=indent) with the default
//     ensure_ascii=True: ", "/": " separators (indent < 0: one line, as json.dumps without indent),
//     "[]"/"{}" for empty containers, non-ASCII and control characters as lowercase \uXXXX (surrogate
//     pairs above U+FFFF), and no trailing newline.
// The parser accepts strict RFC 8259 JSON (no NaN/Infinity, no comments, no trailing commas), UTF-8,
// nesting up to 200 levels. Pure C++ (no game code): it is part of cos_rando_core and unit-tested by
// native/check/rando/test_R0_6.cpp. Built without exceptions: errors come back as false plus a message.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pcr {

class Json {
public:
    enum Type : uint8_t { Null, Bool, Number, String, Array, Object };

    Json() = default;
    static Json null() { return Json(); }
    static Json boolean(bool b);
    static Json integer(int64_t v);
    // A number from its JSON text (no check here; the parser validates what it reads).
    static Json numberText(std::string text);
    // A double, written as the shortest text that reads back to the same value ("1.0" style for
    // integral values, as Python's repr).
    static Json real(double v);
    static Json string(std::string s);
    static Json array();
    static Json object();

    Type type() const { return mType; }
    bool isNull() const { return mType == Null; }
    bool isBool() const { return mType == Bool; }
    bool isNumber() const { return mType == Number; }
    bool isString() const { return mType == String; }
    bool isArray() const { return mType == Array; }
    bool isObject() const { return mType == Object; }
    // A number whose text is an integer (no fraction or exponent) that fits int64.
    bool isInteger() const;

    bool asBool(bool def = false) const { return mType == Bool ? mBool : def; }
    int64_t asInt(int64_t def = 0) const;
    double asDouble(double def = 0.0) const;
    // The string's UTF-8 text, or a number's JSON text; "" for other types.
    const std::string& text() const;

    // Arrays and objects: element count (0 for scalars).
    size_t size() const { return mItems.size(); }
    // Arrays and objects: element i (values for an object). A null for an index out of range.
    const Json& at(size_t i) const;
    Json& at(size_t i);
    // Arrays: appends (a scalar or null turns into an empty array first). Returns the new element.
    Json& push(Json v);

    // Objects: key i, the value of `key` (NULL when missing or not an object), and set, which
    // replaces the value of an existing key in place or appends a new one (a non-object turns into
    // an empty object first). Returns the stored value.
    const std::string& keyAt(size_t i) const;
    const Json* find(std::string_view key) const;
    Json* find(std::string_view key);
    Json& set(std::string_view key, Json v);
    // Removes `key`; false when absent.
    bool erase(std::string_view key);

    // Deep equality: same types, number texts, strings, and object keys in the same order.
    bool operator==(const Json& o) const;
    bool operator!=(const Json& o) const { return !(*this == o); }

private:
    Type mType = Null;
    bool mBool = false;
    std::string mText;              // String: UTF-8 value; Number: JSON text
    std::vector<Json> mItems;       // Array elements or Object values
    std::vector<std::string> mKeys; // Object keys, parallel to mItems
};

// Parses `text` (a UTF-8 BOM is skipped; whitespace may surround the value). On failure *out is
// null and *err (when not NULL) is "line L col C: message".
bool jsonParse(std::string_view text, Json* out, std::string* err);

// Appends v to *out as Python's json.dumps(v, indent=indent) does (indent < 0: no indent).
void jsonWrite(const Json& v, std::string* out, int indent);
std::string jsonDump(const Json& v, int indent);

} // namespace pcr
