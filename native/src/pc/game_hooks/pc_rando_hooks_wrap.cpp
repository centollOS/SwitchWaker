// The message font's glyph widths for the randomizer's word wrap (docs/RANDOMIZER_PLAN.md step R3.20,
// decision D10: wrap at runtime with the loaded font, so a EUR font works the same way later).
// The pure wrap code is features/rando/pc_rando_wrap.cpp; this file reads the font (game code, not
// in cos_rando_core). No hunk in game/: it only calls the font's own JUTFont::getWidthEntry.
#include "features/rando/pc_rando_wrap.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JUtility/JUTResFont.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_ext.h"

#include <cstdio>

extern JUTFont* mDoExt_font0; // m_Do_ext.cpp: the message font while something holds it

namespace {

pcr::GlyphWidths sWidths;
bool sHave = false;

void readWidths(const JUTFont& font, pcr::GlyphWidths* out) {
    for (int c = 0; c < 256; c++) {
        out->code[c] = (uint8_t)font.getWidth(c);
    }
    // A code no MAP1 block holds: getFontCode gives the INF1 default (replacement) code.
    out->replacement = (uint8_t)font.getWidth(0xFFFF);
}

} // namespace

bool pc_rando_font_widths_from_archive(pcr::GlyphWidths* out) {
    JKRArchive* archive = dComIfGp_getFontArchive();
    if (archive == nullptr) {
        return false;
    }
    const ResFONT* res = (const ResFONT*)archive->getResource('ROOT', "rock_24_20_4i_usa.bfn");
    if (res == nullptr) {
        return false;
    }
    JUTResFont font(res, nullptr);
    if (!font.isValid()) {
        return false;
    }
    readWidths(font, out);
    return true;
}

const pcr::GlyphWidths* pc_rando_font_widths() {
    if (sHave) {
        return &sWidths;
    }
    if (mDoExt_font0 != nullptr && mDoExt_font0->isValid()) {
        readWidths(*mDoExt_font0, &sWidths);
        sHave = true;
        std::fprintf(stderr, "[cos] rando: glyph widths from the loaded message font\n");
        return &sWidths;
    }
    // Not loaded right now (mDoExt_getMesgFont is reference counted and frees the font at zero, so
    // it is not taken here): read the same BFN from the font archive.
    if (!pc_rando_font_widths_from_archive(&sWidths)) {
        return nullptr;
    }
    sHave = true;
    std::fprintf(stderr, "[cos] rando: glyph widths from the font archive\n");
    return &sWidths;
}
