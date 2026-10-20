// The randomizer's text input, pure part (docs/RANDOMIZER_PLAN.md step R4.2): the characters a seed
// name and a permalink may hold, the D-pad character grid, and the permalink file import. No game
// headers and no game calls: cos_rando_core (the unit tests) links this file. The drawing, the
// desktop text field and the Switch keyboard applet are in game_hooks/pc_rando_hooks_text.cpp
// (API: pc_rando_text_ui.h).
#pragma once

#include <stdint.h>
#include <string>

namespace pc::rando {

// What is typed: a seed name (wwrando's VALID_SEED_CHARACTERS, at most MAX_SEED_LENGTH = 42) or a
// permalink (standard base64).
enum class TextKind { Seed, Permalink };

constexpr int kSeedMaxLength = 42;      // randomizer.py MAX_SEED_LENGTH
constexpr int kPermalinkMaxLength = 400; // a default-preset permalink is about 70 characters

// The version string a permalink starts with (plan D1).
constexpr const char* kPermalinkVersion = "1.10.0_9775811";

int textMaxLength(TextKind kind);
bool textCharAllowed(TextKind kind, char c);
// As wwrando's sanitize_seed for a seed: surrounding whitespace stripped, characters outside the set
// dropped, cut to the maximum length. A permalink loses every whitespace character (a pasted
// permalink may be wrapped) and any other character outside base64 the same way.
std::string textSanitize(TextKind kind, const char* text);

// The grid's input: the same bits as the options menu's keys (pc_menu.cpp Key), pressed this frame.
enum TextKey : uint32_t {
    kTextUp = 1u << 0,
    kTextDown = 1u << 1,
    kTextLeft = 1u << 2,
    kTextRight = 1u << 3,
    kTextConfirm = 1u << 4,  // A: press the cell
    kTextBack = 1u << 5,     // B: delete the last character; with no text, cancel
    kTextShiftL = 1u << 6,   // L / R: upper / lower case
    kTextShiftR = 1u << 7,
};

enum class TextState { Editing, Done, Cancelled };

enum class TextAction { Char, Shift, Delete, Ok, Cancel };

struct TextCell {
    TextAction action;
    char ch; // TextAction::Char: the character it types (in the grid's current case)
};

// The D-pad character grid: rows of letters (10, 10, 6), digits (10), the kind's symbols, then the
// actions (Shift, Delete, OK, Cancel). Directions wrap around and keep the column (clamped to a
// shorter row); the cursor starts on 'a'.
class TextGrid {
public:
    explicit TextGrid(TextKind kind = TextKind::Seed, const char* initial = "");

    // Keys pressed this frame (TextKey bits); ignored once Done or Cancelled.
    void press(uint32_t keys);
    // Moves the cursor to a cell and presses it (a mouse click on the grid). Out of range: nothing.
    void pressCell(int row, int col);
    // The text field's edits (the desktop keyboard, the console's keyboard): sanitized.
    void setText(const char* text);
    void finish(bool ok);

    TextKind kind() const { return mKind; }
    const std::string& text() const { return mText; }
    TextState state() const { return mState; }
    bool upper() const { return mUpper; }
    int row() const { return mRow; }
    int col() const { return mCol; }
    int rows() const;
    int rowLength(int row) const;
    TextCell cellAt(int row, int col) const;
    // Bumped on every change of the text (the desktop field reloads its buffer then).
    unsigned version() const { return mVersion; }

private:
    void type(char c);
    void activate(const TextCell& cell);
    TextKind mKind;
    std::string mText;
    TextState mState = TextState::Editing;
    bool mUpper = false;
    int mRow = 0;
    int mCol = 0;
    unsigned mVersion = 0;
};

// ---- permalink import (<userPath>/rando/permalink.txt) ---------------------------------------------

enum class PermalinkStatus {
    Ok,
    NoFile,       // the file cannot be opened
    Empty,        // nothing but whitespace
    BadChars,     // a character outside base64
    TooLong,      // more than kPermalinkMaxLength characters
    BadBase64,    // not valid base64
    BadFormat,    // not "<version>\0<seed>\0<option bits>"
    OtherVersion, // made by another wwrando version (wwrando refuses those too)
};

struct PermalinkInfo {
    PermalinkStatus status = PermalinkStatus::Empty;
    std::string text;    // the permalink, whitespace removed (empty unless BadFormat or better)
    std::string version; // from the decoded bytes (Ok, OtherVersion)
    std::string seed;    // the seed name inside it (Ok, OtherVersion)
    std::string options; // the packed option bytes after the seed (Ok, OtherVersion)
};

const char* permalinkStatusName(PermalinkStatus status);
// Standard base64 with padding, as Python's base64.b64decode accepts it (no whitespace). False on
// a bad length, character or padding.
bool base64Decode(const std::string& in, std::string* out);
// Checks a permalink as wwrando's decode_permalink starts to (strip, base64, version, seed). The
// option bits are left to the options code (R1.2).
PermalinkInfo permalinkParse(const char* text);
// Reads the whole file (a UTF-8 byte order mark and every whitespace character are dropped).
PermalinkInfo permalinkReadFile(const char* path);

} // namespace pc::rando
