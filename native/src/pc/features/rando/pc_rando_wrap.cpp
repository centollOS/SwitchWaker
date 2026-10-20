// The randomizer's text helpers (docs/RANDOMIZER_PLAN.md step R3.20); see pc_rando_wrap.h.
// Pure code: cos_rando_core and the unit tests link it.
#include "pc_rando_wrap.h"

#include "pc_rando_data.h"

#include <cstring>

namespace pcr {

namespace {

// One character of a UTF-8 string, as Python iterates a str. Bytes that are not valid UTF-8 count
// as one character each, with the byte as its code point.
uint32_t decodeChar(std::string_view s, size_t i, size_t* len) {
    const uint8_t b0 = (uint8_t)s[i];
    size_t n = 0;
    uint32_t cp = 0;
    if (b0 < 0x80) {
        *len = 1;
        return b0;
    } else if ((b0 & 0xE0) == 0xC0) {
        n = 2;
        cp = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
        n = 3;
        cp = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
        n = 4;
        cp = b0 & 0x07;
    } else {
        *len = 1;
        return b0;
    }
    if (i + n > s.size()) {
        *len = 1;
        return b0;
    }
    for (size_t k = 1; k < n; k++) {
        const uint8_t b = (uint8_t)s[i + k];
        if ((b & 0xC0) != 0x80) {
            *len = 1;
            return b0;
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    *len = n;
    return cp;
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back((char)cp);
    } else if (cp < 0x800) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

// cp1252's characters outside Latin-1: code point -> byte 0x80-0x9F.
struct Cp1252Extra {
    uint16_t cp;
    uint8_t byte;
};
const Cp1252Extra kCp1252[] = {
    {0x20AC, 0x80}, {0x201A, 0x82}, {0x0192, 0x83}, {0x201E, 0x84}, {0x2026, 0x85}, {0x2020, 0x86},
    {0x2021, 0x87}, {0x02C6, 0x88}, {0x2030, 0x89}, {0x0160, 0x8A}, {0x2039, 0x8B}, {0x0152, 0x8C},
    {0x017D, 0x8E}, {0x2018, 0x91}, {0x2019, 0x92}, {0x201C, 0x93}, {0x201D, 0x94}, {0x2022, 0x95},
    {0x2013, 0x96}, {0x2014, 0x97}, {0x02DC, 0x98}, {0x2122, 0x99}, {0x0161, 0x9A}, {0x203A, 0x9B},
    {0x0153, 0x9C}, {0x017E, 0x9E}, {0x0178, 0x9F},
};

void rstripSpaces(std::string& s) {
    while (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
}

bool inList(StrSpan list, std::string_view name) {
    for (const char* s : list) {
        if (name == s) {
            return true;
        }
    }
    return false;
}

int countIn(StrSpan list, std::string_view name) {
    int n = 0;
    for (const char* s : list) {
        n += name == s;
    }
    return n;
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

// s.lower().endswith(suffix) for an ASCII, lower-case suffix.
bool endsWithLower(std::string_view s, std::string_view suffix) {
    if (s.size() < suffix.size()) {
        return false;
    }
    const size_t base = s.size() - suffix.size();
    for (size_t k = 0; k < suffix.size(); k++) {
        char c = s[base + k];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != suffix[k]) {
            return false;
        }
    }
    return true;
}

bool isPyWhitespace(char c) {
    // str.strip() with no argument; item names are ASCII, so the ASCII whitespace set is enough.
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
           c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}

} // namespace

int glyphWidth(const GlyphWidths& w, uint32_t cp) {
    if (cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF)) {
        return w.code[cp];
    }
    for (const Cp1252Extra& e : kCp1252) {
        if (e.cp == cp) {
            return w.code[e.byte];
        }
    }
    return w.replacement;
}

int textBoxMaxLineLength(uint8_t text_box_type) {
    return (text_box_type == 0x9 || text_box_type == 0xE) ? 419 : 503;
}

std::string wordWrapString(const GlyphWidths& w, std::string_view s, int max_line_length,
                           bool preserve_control_codes) {
    const int space_width = glyphWidth(w, ' ');
    std::string out;
    std::string word;
    int word_length = 0;
    int line_length = 0;
    size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '\\' && i + 1 < s.size() && s[i + 1] == '{') {
            size_t end = s.find('}', i);
            const size_t len = end == std::string_view::npos ? s.size() - i : end + 1 - i;
            if (preserve_control_codes) {
                word.append(s.substr(i, len));
            }
            i += len;
        } else if (c == '\n') {
            out += word;
            rstripSpaces(out);
            out.push_back('\n');
            line_length = 0;
            word.clear();
            word_length = 0;
            i++;
        } else if (c == ' ') {
            out += word;
            line_length += word_length;
            word.clear();
            word_length = 0;
            i++;
            if (line_length + space_width >= max_line_length) {
                out.push_back('\n');
                line_length = 0;
            } else {
                out.push_back(' ');
                line_length += space_width;
            }
        } else {
            size_t len = 1;
            const uint32_t cp = decodeChar(s, i, &len);
            word.append(s.substr(i, len));
            word_length += glyphWidth(w, cp);
            i += len;
            if (line_length + word_length > max_line_length) {
                rstripSpaces(out);
                if (!out.empty() && out.back() != '\n') {
                    out.push_back('\n');
                }
                line_length = 0;
                if (word_length >= max_line_length) {
                    out += word;
                    out.push_back('\n');
                    word.clear();
                    word_length = 0;
                }
            }
        }
    }
    out += word;
    return out;
}

std::string wordWrapMessage(const GlyphWidths& w, uint8_t text_box_type, std::string_view text,
                            int extra_line_length) {
    return wordWrapString(w, text, textBoxMaxLineLength(text_box_type) + extra_line_length);
}

std::string padStringToNext4Lines(std::string_view text) {
    // lines = string.split("\n"): the line count is the newline count plus one.
    size_t lines = 1;
    for (char c : text) {
        lines += c == '\n';
    }
    std::string out(text);
    for (size_t pad = (4 - lines % 4) % 4; pad > 0; pad--) {
        out.push_back('\n');
    }
    out.push_back('\n');
    return out;
}

std::string constructStringFromParts(const GlyphWidths& w, uint8_t text_box_type,
                                     const std::vector<std::string>& parts, int extra_line_length) {
    std::string out;
    for (const std::string& part : parts) {
        out += padStringToNext4Lines(wordWrapMessage(w, text_box_type, part, extra_line_length));
    }
    return out;
}

const char* indefiniteArticle(std::string_view text) {
    size_t i = 0;
    while (i < text.size() && isPyWhitespace(text[i])) {
        i++;
    }
    if (i == text.size()) {
        return "a";
    }
    switch (text[i]) {
    case 'a': case 'e': case 'i': case 'o': case 'u':
    case 'A': case 'E': case 'I': case 'O': case 'U':
        return "an";
    default:
        return "a";
    }
}

std::string addArticleBeforeItemName(std::string_view name) {
    const char* article = nullptr;
    if (!name.empty() && name.back() >= '0' && name.back() <= '9') {
        article = nullptr; // re.search(r"\d$"): ends with a digit
    } else if (countIn(progress_items(), name) + countIn(nonprogress_items(), name) > 1) {
        article = indefiniteArticle(name);
    } else if (inList(consumable_items(), name)) {
        article = indefiniteArticle(name);
    } else if (inList(duplicatable_consumable_items(), name)) {
        article = indefiniteArticle(name);
    } else if (endsWithLower(name, " small key")) {
        article = indefiniteArticle(name);
    } else if (endsWith(name, " Trap Chest")) {
        article = indefiniteArticle(name);
    } else if (name == "Delivery Bag" || name == "Boat's Sail" || name == "Note to Mom") {
        article = indefiniteArticle(name);
    } else if (name == "Beedle's Chart" || name == "Bombs" || name == "Tingle's Chart" ||
               name == "Maggie's Letter" || name == "Father's Letter") {
        article = nullptr;
    } else if (name == "Nayru's Pearl" || name == "Din's Pearl" || name == "Farore's Pearl") {
        article = nullptr;
    } else {
        article = "the";
    }
    std::string out;
    if (article != nullptr) {
        out = article;
        out.push_back(' ');
    }
    out.append(name);
    return out;
}

std::string upperFirstLetter(std::string_view text) {
    if (text.empty()) {
        return std::string();
    }
    size_t len = 1;
    uint32_t cp = decodeChar(text, 0, &len);
    if (cp >= 'a' && cp <= 'z') {
        cp -= 0x20;
    } else if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7 && len == 2) {
        cp -= 0x20;
    } else {
        return std::string(text);
    }
    std::string out;
    appendUtf8(out, cp);
    out.append(text.substr(len));
    return out;
}

std::string itemNameWithArticle(uint8_t item_id, bool upper) {
    const char* name = item_name(item_id);
    if (name == nullptr) {
        return std::string();
    }
    std::string s = addArticleBeforeItemName(name);
    return upper ? upperFirstLetter(s) : s;
}

} // namespace pcr
