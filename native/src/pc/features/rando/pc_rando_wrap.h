// The randomizer's text helpers (docs/RANDOMIZER_PLAN.md step R3.20): word wrap with the message
// font's glyph widths, wwrando's grammatical articles before item names, and gclib's
// one-text-box-per-part builder. The message texts that later steps put in the overlay
// (pc_rando_msg.h; shop descriptions, hints, item-get texts) are built with these, so they wrap
// exactly as wwrando's BMG does.
//
// Ports, quirks included (strings are wwrando's string syntax, UTF-8; see pc_rando_msg.h):
//   gclib bfn.py   BFN.get_char_width, BFN.word_wrap_string
//   gclib bmg.py   TEXT_BOX_TYPE_TO_MAX_LINE_LENGTH, Message.word_wrap_string_part,
//                  Message.pad_string_to_next_4_lines, Message.construct_string_from_parts
//   wwrando tweaks.py   get_indefinite_article, add_article_before_item_name, upper_first_letter
// Item names come from data/item_names.txt through R0.3's tables (pc_rando_data.h).
//
// Pure: no game headers, no game calls (cos_rando_core and the unit tests link it). The glyph
// widths of the real font are read at runtime by pc_rando_font_widths(), which lives in
// game_hooks/pc_rando_hooks_wrap.cpp (game code, not in cos_rando_core).
#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace pcr {

// A single-byte font's advance widths (the BFN's WID1 "width" byte, found through its MAP1), by
// character code 0-255, plus the width of the font's replacement code (INF1 replacement_code,
// JUTResFont's defaultCode) used for a character the font cannot map.
struct GlyphWidths {
    uint8_t code[256];
    uint8_t replacement;
};

// BFN.get_char_width for one Unicode code point: Python encodes the character in cp1252 and maps
// the byte, so U+0000-U+007F and U+00A0-U+00FF use their own code, the 27 cp1252 extras (U+20AC
// euro, U+2014 em dash, ...) their cp1252 byte, and anything else (U+0080-U+009F included, which
// cp1252 cannot encode) the replacement width.
int glyphWidth(const GlyphWidths& w, uint32_t codepoint);

// gclib's text box widths in pixels (TEXT_BOX_TYPE_TO_MAX_LINE_LENGTH): 419 for an item-get box (9)
// and the Wind Waker song box (0xE), 503 for every other type (gclib has entries 0-0xE only; a
// larger type also gets 503 here).
int textBoxMaxLineLength(uint8_t text_box_type);

// BFN.word_wrap_string. Words are cut at spaces; a line break goes where the next word would pass
// `max_line_length`, and a word at least as wide as a line is cut where it passes. Control tags
// ("\{...}") have no width; they are kept when `preserve_control_codes`, else dropped. Spaces before
// an inserted or existing line break are removed, as gclib's rstrip(" ") does.
// Where gclib raises (a backslash not followed by "{", a tag with no "}"), this keeps going: such a
// backslash is an ordinary character, and an unclosed tag runs to the end of the string.
std::string wordWrapString(const GlyphWidths& w, std::string_view text, int max_line_length,
                           bool preserve_control_codes = true);
// Message.word_wrap_string_part: wrap at the text box type's width plus `extra_line_length`.
std::string wordWrapMessage(const GlyphWidths& w, uint8_t text_box_type, std::string_view text,
                            int extra_line_length = 0);
// Message.pad_string_to_next_4_lines: blank lines up to a multiple of 4 lines, then a final "\n".
std::string padStringToNext4Lines(std::string_view text);
// Message.construct_string_from_parts: each part wrapped and padded to whole text boxes (4 lines),
// concatenated.
std::string constructStringFromParts(const GlyphWidths& w, uint8_t text_box_type,
                                     const std::vector<std::string>& parts,
                                     int extra_line_length = 0);

// tweaks.py get_indefinite_article: "an" when the first non-whitespace character is a vowel
// (a e i o u, either case), else "a" (also for an empty or all-whitespace string, where Python
// would raise).
const char* indefiniteArticle(std::string_view text);
// tweaks.py add_article_before_item_name: "a"/"an", "the" or nothing in front of an item name,
// decided from wwrando's item lists (item_types.py through pc_rando_data.h).
std::string addArticleBeforeItemName(std::string_view item_name);
// tweaks.py upper_first_letter: the first character in upper case. Covers ASCII and the Latin-1
// letters U+00E0-U+00FE (but U+00F7); any other first character (Python would also change a few,
// such as U+00DF to "SS") is kept. An empty string stays empty (Python would raise).
std::string upperFirstLetter(std::string_view text);
// The item's name from data/item_names.txt with its article, first letter upper case when `upper`
// ("the Wind Waker", "A Piece of Heart"). An unnamed id gives "".
std::string itemNameWithArticle(uint8_t item_id, bool upper = false);

} // namespace pcr

// The glyph widths of the game's message font (mDoExt_getMesgFont's rock_24_20_4i_usa.bfn), read
// once through JUTFont::getWidthEntry and cached: from the loaded font when there is one, else
// from a temporary JUTResFont on the font archive's resource. NULL while no font can be read (the
// font archive is not loaded yet, early in boot). Game thread only. Defined in
// game_hooks/pc_rando_hooks_wrap.cpp, which is not part of cos_rando_core.
const pcr::GlyphWidths* pc_rando_font_widths();
// The same table read from the font archive's rock_24_20_4i_usa.bfn with a temporary JUTResFont
// (the path pc_rando_font_widths takes when the font is not loaded), uncached. False when the
// archive or the resource is missing. Also in pc_rando_hooks_wrap.cpp.
bool pc_rando_font_widths_from_archive(pcr::GlyphWidths* out);
