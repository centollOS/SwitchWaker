// The randomizer's text input, pure part (pc_rando_text.h; docs/RANDOMIZER_PLAN.md step R4.2). Keep
// this file free of game headers and game calls: cos_rando_core (the unit tests) links it alone.
#include "pc_rando_text.h"

#include <cstdio>
#include <cstring>

namespace pc::rando {

namespace {

constexpr const char* kLower = "abcdefghijklmnopqrstuvwxyz";
constexpr const char* kUpper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr const char* kDigits = "0123456789";
// randomizer.py VALID_SEED_CHARACTERS = "-_'%%.%s%s" % (ascii_letters, digits): the symbols.
constexpr const char* kSeedSymbols = "-_'%.";
constexpr const char* kPermalinkSymbols = "+/=";

// Grid rows: letters 0-9, 10-19, 20-25; digits; symbols; actions.
constexpr int kLetterRows = 3;
constexpr int kRowDigits = 3;
constexpr int kRowSymbols = 4;
constexpr int kRowActions = 5;
constexpr int kGridRows = 6;
constexpr TextAction kActions[] = {TextAction::Shift, TextAction::Delete, TextAction::Ok, TextAction::Cancel};
constexpr int kActionCount = (int)(sizeof(kActions) / sizeof(kActions[0]));

bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool isDigit(char c) {
    return c >= '0' && c <= '9';
}
bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

const char* symbols(TextKind kind) {
    return kind == TextKind::Seed ? kSeedSymbols : kPermalinkSymbols;
}

int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

} // namespace

int textMaxLength(TextKind kind) {
    return kind == TextKind::Seed ? kSeedMaxLength : kPermalinkMaxLength;
}

bool textCharAllowed(TextKind kind, char c) {
    return c != '\0' && (isLetter(c) || isDigit(c) || strchr(symbols(kind), c) != nullptr);
}

std::string textSanitize(TextKind kind, const char* text) {
    std::string out;
    if (text == nullptr) {
        return out;
    }
    const char* begin = text;
    const char* end = text + strlen(text);
    // Python's str.strip(): surrounding whitespace (a permalink drops all of it below anyway).
    while (begin < end && isSpace(*begin)) {
        begin++;
    }
    while (end > begin && isSpace(end[-1])) {
        end--;
    }
    for (const char* p = begin; p < end && (int)out.size() < textMaxLength(kind); p++) {
        if (textCharAllowed(kind, *p)) {
            out += *p;
        }
    }
    return out;
}

// ---- the grid ------------------------------------------------------------------------------------

TextGrid::TextGrid(TextKind kind, const char* initial) : mKind(kind), mText(textSanitize(kind, initial)) {}

int TextGrid::rows() const {
    return kGridRows;
}

int TextGrid::rowLength(int row) const {
    if (row >= 0 && row < kLetterRows) {
        return row < 2 ? 10 : 6;
    }
    switch (row) {
    case kRowDigits: return 10;
    case kRowSymbols: return (int)strlen(symbols(mKind));
    case kRowActions: return kActionCount;
    default: return 0;
    }
}

TextCell TextGrid::cellAt(int row, int col) const {
    if (col < 0 || col >= rowLength(row)) {
        return {TextAction::Char, '\0'}; // types nothing
    }
    if (row < kLetterRows) {
        return {TextAction::Char, (mUpper ? kUpper : kLower)[row * 10 + col]};
    }
    if (row == kRowDigits) {
        return {TextAction::Char, kDigits[col]};
    }
    if (row == kRowSymbols) {
        return {TextAction::Char, symbols(mKind)[col]};
    }
    return {kActions[col], '\0'};
}

void TextGrid::type(char c) {
    if ((int)mText.size() < textMaxLength(mKind) && textCharAllowed(mKind, c)) {
        mText += c;
        mVersion++;
    }
}

void TextGrid::activate(const TextCell& cell) {
    switch (cell.action) {
    case TextAction::Char: type(cell.ch); break;
    case TextAction::Shift: mUpper = !mUpper; break;
    case TextAction::Delete:
        if (!mText.empty()) {
            mText.pop_back();
            mVersion++;
        }
        break;
    case TextAction::Ok: finish(true); break;
    case TextAction::Cancel: finish(false); break;
    }
}

void TextGrid::press(uint32_t keys) {
    if (mState != TextState::Editing || keys == 0) {
        return;
    }
    if (keys & (kTextShiftL | kTextShiftR)) {
        mUpper = !mUpper;
    }
    if (keys & (kTextUp | kTextDown)) {
        mRow = (mRow + ((keys & kTextUp) ? kGridRows - 1 : 1)) % kGridRows;
        mCol = mCol < rowLength(mRow) ? mCol : rowLength(mRow) - 1;
    }
    if (keys & (kTextLeft | kTextRight)) {
        const int n = rowLength(mRow);
        mCol = (mCol + ((keys & kTextLeft) ? n - 1 : 1)) % n;
    }
    if (keys & kTextConfirm) {
        activate(cellAt(mRow, mCol));
    } else if (keys & kTextBack) {
        if (mText.empty()) {
            finish(false);
        } else {
            mText.pop_back();
            mVersion++;
        }
    }
}

void TextGrid::pressCell(int row, int col) {
    if (mState != TextState::Editing || row < 0 || row >= kGridRows || col < 0 || col >= rowLength(row)) {
        return;
    }
    mRow = row;
    mCol = col;
    activate(cellAt(row, col));
}

void TextGrid::setText(const char* text) {
    std::string s = textSanitize(mKind, text);
    if (s != mText) {
        mText = std::move(s);
        mVersion++;
    }
}

void TextGrid::finish(bool ok) {
    if (mState == TextState::Editing) {
        mState = ok ? TextState::Done : TextState::Cancelled;
    }
}

// ---- permalinks ----------------------------------------------------------------------------------

const char* permalinkStatusName(PermalinkStatus status) {
    switch (status) {
    case PermalinkStatus::Ok: return "ok";
    case PermalinkStatus::NoFile: return "no file";
    case PermalinkStatus::Empty: return "empty";
    case PermalinkStatus::BadChars: return "a character outside base64";
    case PermalinkStatus::TooLong: return "too long";
    case PermalinkStatus::BadBase64: return "not valid base64";
    case PermalinkStatus::BadFormat: return "not a wwrando permalink";
    case PermalinkStatus::OtherVersion: return "another wwrando version";
    }
    return "?";
}

bool base64Decode(const std::string& in, std::string* out) {
    out->clear();
    if (in.size() % 4 != 0) {
        return false;
    }
    for (size_t i = 0; i < in.size(); i += 4) {
        int v[4];
        int pad = 0;
        for (int j = 0; j < 4; j++) {
            const char c = in[i + j];
            if (c == '=') {
                // padding only in the last group's last one or two places
                if (i + 4 != in.size() || j < 2) {
                    return false;
                }
                pad++;
                v[j] = 0;
                continue;
            }
            if (pad != 0) {
                return false; // a character after '='
            }
            v[j] = base64Value(c);
            if (v[j] < 0) {
                return false;
            }
        }
        const uint32_t bits = (uint32_t)v[0] << 18 | (uint32_t)v[1] << 12 | (uint32_t)v[2] << 6 | (uint32_t)v[3];
        out->push_back((char)(bits >> 16));
        if (pad < 2) {
            out->push_back((char)(bits >> 8));
        }
        if (pad < 1) {
            out->push_back((char)bits);
        }
    }
    return true;
}

PermalinkInfo permalinkParse(const char* text) {
    PermalinkInfo info;
    std::string s;
    for (const char* p = text != nullptr ? text : ""; *p != '\0'; p++) {
        if (isSpace(*p)) {
            continue;
        }
        if (!textCharAllowed(TextKind::Permalink, *p)) {
            info.status = PermalinkStatus::BadChars;
            return info;
        }
        s += *p;
    }
    if (s.empty()) {
        info.status = PermalinkStatus::Empty;
        return info;
    }
    if ((int)s.size() > kPermalinkMaxLength) {
        info.status = PermalinkStatus::TooLong;
        return info;
    }
    std::string bytes;
    if (!base64Decode(s, &bytes)) {
        info.status = PermalinkStatus::BadBase64;
        return info;
    }
    info.text = s;
    // permalink.split(b"\0", 2): the version, the seed, the option bytes.
    const size_t nul1 = bytes.find('\0');
    const size_t nul2 = nul1 == std::string::npos ? std::string::npos : bytes.find('\0', nul1 + 1);
    if (nul2 == std::string::npos) {
        info.status = PermalinkStatus::BadFormat;
        return info;
    }
    info.version = bytes.substr(0, nul1);
    info.seed = bytes.substr(nul1 + 1, nul2 - nul1 - 1);
    info.options = bytes.substr(nul2 + 1);
    for (char c : info.version + info.seed) {
        if ((unsigned char)c >= 0x80 || (unsigned char)c < 0x20) {
            info.status = PermalinkStatus::BadFormat; // .decode("ascii") (and no control characters)
            return info;
        }
    }
    info.status = info.version == kPermalinkVersion ? PermalinkStatus::Ok : PermalinkStatus::OtherVersion;
    return info;
}

PermalinkInfo permalinkReadFile(const char* path) {
    FILE* f = path != nullptr ? fopen(path, "rb") : nullptr;
    if (f == nullptr) {
        PermalinkInfo info;
        info.status = PermalinkStatus::NoFile;
        return info;
    }
    std::string data;
    char buf[512];
    size_t n;
    // A permalink is short: more than a few KiB is not one (TooLong without reading on).
    while ((n = fread(buf, 1, sizeof buf, f)) > 0 && data.size() < 16384) {
        data.append(buf, n);
    }
    fclose(f);
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF) {
        data.erase(0, 3);
    }
    if (data.find('\0') != std::string::npos) {
        PermalinkInfo info;
        info.status = PermalinkStatus::BadChars;
        return info;
    }
    return permalinkParse(data.c_str());
}

} // namespace pc::rando
