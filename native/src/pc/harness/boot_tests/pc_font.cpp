// Fonts on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.3): the COS_SMOKE=font test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create, so after JFWSystem::init made the
// system font and console), then exits:
// - the system font (JFWSystem's JUTResFont over the compiled-in JUTResFONT_Ascfont_fix12) and
//   /res/Menu/kanfont_fix16.bfn (a Shift-JIS disc font with MAP1 tables, read with DVDReadPrio
//   into the game heap) are each compared with an independent reading of the same bytes: plain
//   big-endian loads at the format's offsets, no ResFONT struct. Checked: the block counts, the
//   INF1 fields through JUTResFont's accessors, getFontCode and getWidthEntry for every code a
//   font maps (0-0xFF for the system font; every MAP1 range and one code past each end for the
//   disc font), and for each code loadImage's cell position and texture object (data pointer,
//   size, format) on the GLY1 page that holds it; getWidth('A') and getWidth()/getHeight();
// - a console line drawn: a JUTConsole with the system font prints one line, which must be in
//   its buffer, and draws it in one Aurora frame; Aurora's counters for that frame must show draw
//   calls and a texture upload (the console's fill box has no texture: the upload is the font
//   page);
// - <COS_RUN_DIR>/font.txt: what JUTResFont read from the disc font (block counts, INF1, every
//   WID1/MAP1/GLY1 header), in the manifest's names; native/tools/run.sh compares it with
//   build/native-mac/disc_manifest.json (disc_manifest.py --check-font).
// Exit 0 when every check holds, 1 otherwise. The disc-font check is checkResFont, which
// COS_SMOKE=msg-sweep (pc_msg.cpp) also runs on the message fonts (step 4.6).
#include "pc_internal.h"

#include "JSystem/JFramework/JFWSystem.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTConsole.h"
#include "JSystem/JUtility/JUTFontData_Ascfont_fix12.h"
#include "JSystem/JUtility/JUTResFont.h"
#include "m_Do/m_Do_ext.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <dolphin/dvd.h>
#include <dolphin/gx.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

const char kDiscFont[] = "/res/Menu/kanfont_fix16.bfn";

int sErrors = 0;

void fail(const char* font, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void fail(const char* font, const char* fmt, ...) {
    if (sErrors < 50) {
        char text[256];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] font: %s: %s\n", font, text);
    }
    sErrors++;
}

// ---- the independent reading: big-endian loads at the BFN offsets ----------------------------

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

constexpr uint32_t tag(char a, char b, char c, char d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

constexpr int kMaxBlocks = 64;

struct RawFont {
    const uint8_t* base = nullptr;
    uint32_t numBlocks = 0;
    const uint8_t* inf = nullptr;
    const uint8_t* wid[kMaxBlocks] = {};
    const uint8_t* map[kMaxBlocks] = {};
    const uint8_t* gly[kMaxBlocks] = {};
    int numWid = 0, numMap = 0, numGly = 0, unknown = 0;
    uint16_t minMapStart = 0xFFFF; // JUTResFont::mMaxCode: the smallest MAP1 start code

    uint16_t fontType() const { return rd16(inf + 0x08); }
    uint16_t ascent() const { return rd16(inf + 0x0A); }
    uint16_t descent() const { return rd16(inf + 0x0C); }
    uint16_t width() const { return rd16(inf + 0x0E); }
    uint16_t leading() const { return rd16(inf + 0x10); }
    uint16_t defaultCode() const { return rd16(inf + 0x12); }
};

bool parseRaw(const char* name, const uint8_t* base, uint32_t length, RawFont& raw) {
    raw.base = base;
    if (length < 0x20 || memcmp(base, "FONTbfn1", 8) != 0) {
        fail(name, "not a FONTbfn1 file");
        return false;
    }
    raw.numBlocks = rd32(base + 0x0C);
    const uint8_t* block = base + 0x20;
    for (uint32_t i = 0; i < raw.numBlocks; i++) {
        if (block + 8 > base + length) {
            fail(name, "block %u lies past the end", i);
            return false;
        }
        uint32_t type = rd32(block);
        uint32_t size = rd32(block + 4);
        if (type == tag('I', 'N', 'F', '1')) {
            raw.inf = block;
        } else if (type == tag('W', 'I', 'D', '1') && raw.numWid < kMaxBlocks) {
            raw.wid[raw.numWid++] = block;
        } else if (type == tag('M', 'A', 'P', '1') && raw.numMap < kMaxBlocks) {
            raw.map[raw.numMap++] = block;
            if (rd16(block + 0x0A) < raw.minMapStart) {
                raw.minMapStart = rd16(block + 0x0A);
            }
        } else if (type == tag('G', 'L', 'Y', '1') && raw.numGly < kMaxBlocks) {
            raw.gly[raw.numGly++] = block;
        } else {
            raw.unknown++;
        }
        if (size < 8) {
            fail(name, "block %u has size %u", i, size);
            return false;
        }
        block += size;
    }
    if (raw.inf == nullptr) {
        fail(name, "no INF1 block");
        return false;
    }
    return true;
}

// JUTResFont::convertSjis over the raw lead code.
int rawConvertSjis(int chr, int lead) {
    int hi = (chr >> 8) & 0xFF;
    int lo = (chr & 0xFF) - 0x40;
    if (lo >= 0x40) {
        lo--;
    }
    return lo + (hi - 0x88) * 0xBC - 0x5E + lead;
}

// The glyph index of a character, as the BFN format defines it (JUTResFont::getFontCode). Fonts
// of type 2 with Shift-JIS maps first turn ASCII into full-width codes through a table; the
// callers leave those characters out (asciiRemapped).
int rawFontCode(const RawFont& raw, int chr) {
    int ret = raw.defaultCode();
    for (int i = 0; i < raw.numMap; i++) {
        const uint8_t* m = raw.map[i];
        int method = rd16(m + 0x08), start = rd16(m + 0x0A), end = rd16(m + 0x0C);
        int entries = rd16(m + 0x0E);
        const uint8_t* table = m + 0x10;
        if (chr < start || chr > end) {
            continue;
        }
        if (method == 0) {
            ret = chr - start;
        } else if (method == 2) {
            ret = rd16(table + 2 * (chr - start));
        } else if (method == 3) {
            int lo = 0, hi = entries - 1;
            while (hi >= lo) {
                int mid = (lo + hi) / 2;
                int key = rd16(table + 4 * mid);
                if (chr < key) {
                    hi = mid - 1;
                } else if (chr > key) {
                    lo = mid + 1;
                } else {
                    ret = rd16(table + 4 * mid + 2);
                    break;
                }
            }
        } else if (method == 1) {
            ret = rawConvertSjis(chr, entries == 1 ? rd16(table) : 0x31C);
        }
        break;
    }
    return ret;
}

bool asciiRemapped(const RawFont& raw, int chr) {
    return raw.fontType() == 2 && raw.minMapStart >= 0x8000 && chr >= 0x20 && chr < 0x7F;
}

// ---- JUTResFont against the raw reading --------------------------------------------------------

void checkHeader(const char* name, const JUTResFont& font, const RawFont& raw) {
    if (!font.isValid()) {
        fail(name, "JUTResFont is not valid");
        return;
    }
    if (font.mResFont->numBlocks != raw.numBlocks) {
        fail(name, "numBlocks %u, the file has %u", (unsigned)font.mResFont->numBlocks, raw.numBlocks);
    }
    if (font.mInfoBlock != (const void*)raw.inf) {
        fail(name, "INF1 at %p, the file has it at %p", (void*)font.mInfoBlock, (const void*)raw.inf);
    }
    if (font.mWidthBlockNum != raw.numWid || font.mMapBlockNum != raw.numMap ||
        font.mGlyphBlockNum != raw.numGly) {
        fail(name, "blocks WID1/MAP1/GLY1 %d/%d/%d, the file has %d/%d/%d", font.mWidthBlockNum,
             font.mMapBlockNum, font.mGlyphBlockNum, raw.numWid, raw.numMap, raw.numGly);
    }
    struct {
        const char* field;
        int got;
        int want;
    } fields[] = {
        {"fontType", font.getFontType(), raw.fontType()},
        {"ascent", font.getAscent(), raw.ascent()},
        {"descent", font.getDescent(), raw.descent()},
        {"width", font.getWidth(), raw.width()},
        {"leading", font.getLeading(), raw.leading()},
        {"height", font.getHeight(), raw.ascent() + raw.descent()},
        {"defaultCode", font.mInfoBlock->defaultCode, raw.defaultCode()},
        {"mMaxCode", font.mMaxCode, raw.minMapStart},
    };
    for (auto& f : fields) {
        if (f.got != f.want) {
            fail(name, "%s %d, the file has %d", f.field, f.got, f.want);
        }
    }
    if (raw.numGly > 0) {
        if (font.getCellWidth() != rd16(raw.gly[0] + 0x0C) ||
            font.getCellHeight() != rd16(raw.gly[0] + 0x0E)) {
            fail(name, "cell %dx%d, the file has %ux%u", font.getCellWidth(), font.getCellHeight(),
                 rd16(raw.gly[0] + 0x0C), rd16(raw.gly[0] + 0x0E));
        }
    }
}

// getFontCode, getWidthEntry and loadImage for one character. Returns false on a mismatch.
bool checkChar(const char* name, JUTResFont& font, const RawFont& raw, int chr) {
    bool ok = true;
    int code = rawFontCode(raw, chr);
    int got = font.getFontCode(chr);
    if (got != code) {
        fail(name, "getFontCode(0x%04x) %d, the file maps it to %d", chr, got, code);
        ok = false;
    }

    int offset = 0, width = raw.width();
    for (int i = 0; i < raw.numWid; i++) {
        int start = rd16(raw.wid[i] + 0x08), end = rd16(raw.wid[i] + 0x0A);
        if (start <= code && code <= end) {
            const uint8_t* entry = raw.wid[i] + 0x0C + 2 * (code - start);
            offset = entry[0];
            width = entry[1];
            break;
        }
    }
    JUTFont::TWidth w;
    font.getWidthEntry(chr, &w);
    if (w.field_0x0 != offset || w.field_0x1 != width) {
        fail(name, "getWidthEntry(0x%04x) offset %d width %d, the file has %d %d", chr, w.field_0x0,
             w.field_0x1, offset, width);
        ok = false;
    }

    for (int i = 0; i < raw.numGly; i++) {
        const uint8_t* g = raw.gly[i];
        int start = rd16(g + 0x08), end = rd16(g + 0x0A);
        if (code < start || code > end) {
            continue;
        }
        int cellW = rd16(g + 0x0C), cellH = rd16(g + 0x0E);
        uint32_t texSize = rd32(g + 0x10);
        int fmt = rd16(g + 0x14), rows = rd16(g + 0x16), cols = rd16(g + 0x18);
        int texW = rd16(g + 0x1A), texH = rd16(g + 0x1C);
        int cell = code - start;
        int pageCells = rows * cols;
        int page = cell / pageCells;
        int inPage = cell % pageCells;
        int x = (inPage % rows) * cellW;
        int y = (inPage / rows) * cellH;
        const void* data = g + 0x20 + (size_t)page * texSize;

        font.loadImage(code, GX_TEXMAP0);
        const GXTexObj* obj = &font.mTexObj;
        if (font.mWidth != x || font.mHeight != y) {
            fail(name, "loadImage(%d) cell at %d,%d, the file puts it at %d,%d", code, font.mWidth,
                 font.mHeight, x, y);
            ok = false;
        }
        if (GXGetTexObjData(obj) != data || GXGetTexObjWidth(obj) != texW ||
            GXGetTexObjHeight(obj) != texH || (int)GXGetTexObjFmt(obj) != fmt) {
            fail(name, "loadImage(%d) texture %p %ux%u format %d, the file has %p %dx%d format %d",
                 code, GXGetTexObjData(obj), GXGetTexObjWidth(obj), GXGetTexObjHeight(obj),
                 (int)GXGetTexObjFmt(obj), data, texW, texH, fmt);
            ok = false;
        }
        break;
    }
    return ok;
}

// ---- the disc font --------------------------------------------------------------------------

void reportFont(int fd, const char* path, const JUTResFont& font) {
    if (fd < 0) {
        return;
    }
    writef(fd, "FONT %s block_count=%u INF1=1 WID1=%d MAP1=%d GLY1=%d\n", path,
           (unsigned)font.mResFont->numBlocks, font.mWidthBlockNum, font.mMapBlockNum,
           font.mGlyphBlockNum);
    const ResFONT::INF1* inf = font.mInfoBlock;
    writef(fd, "INF1 %s 0 font_type=%d ascent=%d descent=%d width=%d leading=%d default_code=%d\n",
           path, font.getFontType(), font.getAscent(), font.getDescent(), font.getWidth(),
           font.getLeading(), (int)inf->defaultCode);
    for (int i = 0; i < font.mWidthBlockNum; i++) {
        const ResFONT::WID1* b = font.mpWidthBlocks[i];
        writef(fd, "WID1 %s %d size=%u start=%d end=%d\n", path, i, (unsigned)b->mSize,
               (int)b->startCode, (int)b->endCode);
    }
    for (int i = 0; i < font.mMapBlockNum; i++) {
        const ResFONT::MAP1* b = font.mpMapBlocks[i];
        writef(fd, "MAP1 %s %d size=%u method=%d start=%d end=%d entries=%d\n", path, i,
               (unsigned)b->mSize, (int)b->mappingMethod, (int)b->startCode, (int)b->endCode,
               (int)b->numEntries);
    }
    for (int i = 0; i < font.mGlyphBlockNum; i++) {
        const ResFONT::GLY1* b = font.mpGlyphBlocks[i];
        writef(fd,
               "GLY1 %s %d size=%u start=%d end=%d cell_width=%d cell_height=%d texture_size=%u "
               "texture_format=%d rows=%d columns=%d texture_width=%d texture_height=%d\n",
               path, i, (unsigned)b->mSize, (int)b->startCode, (int)b->endCode,
               (int)b->cellWidth, (int)b->cellHeight, (unsigned)b->textureSize,
               (int)b->textureFormat, (int)b->numRows, (int)b->numColumns, (int)b->textureWidth,
               (int)b->textureHeight);
    }
}

} // namespace

int checkResFont(const char* test, const char* path, JUTResFont& font, const uint8_t* bytes,
                 uint32_t length, int reportFd) {
    int before = sErrors;
    RawFont raw;
    if (!parseRaw(path, bytes, length, raw)) {
        return sErrors - before;
    }
    checkHeader(path, font, raw);
    int checked = 0, bad = 0;
    for (int i = 0; i < raw.numMap && font.isValid(); i++) {
        int start = rd16(raw.map[i] + 0x0A), end = rd16(raw.map[i] + 0x0C);
        for (int chr = start - 1; chr <= end + 1; chr++) {
            if (chr < 0 || chr > 0xFFFF || asciiRemapped(raw, chr)) {
                continue;
            }
            checked++;
            if (!checkChar(path, font, raw, chr)) {
                bad++;
            }
        }
    }
    writef(STDERR_FILENO,
           "[cos] %s: %s: %u blocks (WID1 %d, MAP1 %d, GLY1 %d), type %d, %dx%d; %d codes "
           "checked, %d wrong\n",
           test, path, raw.numBlocks, raw.numWid, raw.numMap, raw.numGly, raw.fontType(),
           raw.width(), raw.ascent() + raw.descent(), checked, bad);
    reportFont(reportFd, path, font);
    return sErrors - before;
}

namespace {

void checkDiscFont() {
    DVDFileInfo info;
    if (!DVDOpen(kDiscFont, &info)) {
        fail(kDiscFont, "DVDOpen failed");
        return;
    }
    uint32_t length = info.length;
    uint32_t padded = (length + 0x1F) & ~0x1Fu;
    uint8_t* buf = (uint8_t*)JKRHeap::alloc(padded, 0x20, mDoExt_getGameHeap());
    if (buf == nullptr) {
        fail(kDiscFont, "no 0x%x bytes in the game heap", padded);
        DVDClose(&info);
        return;
    }
    s32 read = DVDReadPrio(&info, buf, (s32)padded, 0, 2);
    DVDClose(&info);
    if (read < (s32)length) {
        fail(kDiscFont, "DVDReadPrio read %d of %u bytes", (int)read, length);
        JKRHeap::free(buf, nullptr);
        return;
    }

    JUTResFont* font = new JUTResFont((const ResFONT*)buf, nullptr);
    int fd = openRunFile("font.txt");
    if (fd >= 0) {
        writef(fd, "# font (COS_SMOKE=font): what JUTResFont read, in disc_manifest.py's names\n");
    }
    checkResFont("font", kDiscFont, *font, buf, length, fd);
    if (fd >= 0) {
        close(fd);
    }
    delete font;
    JKRHeap::free(buf, nullptr);
}

// ---- the system font and a console line ------------------------------------------------------

void checkSystemFont(JUTResFont* font) {
    const char* name = "system font";
    if (font == nullptr) {
        fail(name, "JFWSystem has no system font");
        return;
    }
    if (font->getResFont() != (const ResFONT*)JUTResFONT_Ascfont_fix12) {
        fail(name, "the font is not JUTResFONT_Ascfont_fix12");
        return;
    }
    if (((uintptr_t)JUTResFONT_Ascfont_fix12 & 0x1F) != 0) {
        fail(name, "JUTResFONT_Ascfont_fix12 at %p is not 32-byte aligned", JUTResFONT_Ascfont_fix12);
    }
    RawFont raw;
    // The file size is in the header (0x08).
    if (!parseRaw(name, JUTResFONT_Ascfont_fix12, rd32(JUTResFONT_Ascfont_fix12 + 0x08), raw)) {
        return;
    }
    checkHeader(name, *font, raw);
    int bad = 0;
    for (int chr = 0; chr <= 0xFF; chr++) {
        if (!asciiRemapped(raw, chr) && !checkChar(name, *font, raw, chr)) {
            bad++;
        }
    }
    int widthA = static_cast<JUTFont*>(font)->getWidth('A');
    JUTFont::TWidth w;
    font->getWidthEntry('A', &w);
    if (widthA <= 0 || widthA > font->getCellWidth() || widthA != w.field_0x1) {
        fail(name, "getWidth('A') %d (cell width %d)", widthA, font->getCellWidth());
    }
    writef(STDERR_FILENO,
           "[cos] font: %s: %u blocks (WID1 %d, MAP1 %d, GLY1 %d), type %d, width %d height %d "
           "cell %dx%d, getWidth('A') %d; 256 codes checked, %d wrong\n",
           name, raw.numBlocks, raw.numWid, raw.numMap, raw.numGly, font->getFontType(),
           font->getWidth(), font->getHeight(), font->getCellWidth(), font->getCellHeight(), widthA,
           bad);
}

void checkConsoleLine(JUTResFont* font) {
    const char* name = "console";
    if (font == nullptr) {
        return;
    }
    static const char kLine[] = "the game font smoke: ABCxyz 0123 !?";
    JUTConsole* console = JUTConsole::create(60, 4, mDoExt_getGameHeap());
    if (console == nullptr) {
        fail(name, "JUTConsole::create failed");
        return;
    }
    console->setFont(font);
    console->setPosition(40, 60);
    console->setHeight(4);
    // JUTConsole::print: bit 0 of the output writes the line buffer, bit 1 OSReport (the
    // OutputFlag names in JUTConsole.h do not follow the code).
    console->setOutput(1);
    console->setVisible(true);
    console->print(kLine);
    console->print("\n");
    bool found = false;
    for (int i = 0; i < 4; i++) {
        if (strcmp((const char*)console->getLinePtr(i), kLine) == 0) {
            found = true;
        }
    }
    if (!found || console->getUsedLine() < 1) {
        fail(name, "the printed line is not in the console buffer (%d lines used)",
             console->getUsedLine());
    }

    // One frame with the console in it. Aurora fills its counters when the render worker has
    // taken the frame, so wait for them (the frame's own counters, not a later empty frame's).
    bool drawn = false;
    for (int attempt = 0; attempt < 5 && !drawn; attempt++) {
        aurora_update();
        if (!aurora_begin_frame()) {
            usleep(20 * 1000);
            continue;
        }
        console->doDraw(JUTConsole::ACTIVE);
        aurora_end_frame();
        drawn = true;
    }
    if (!drawn) {
        fail(name, "aurora_begin_frame refused 5 frames");
    } else {
        const AuroraStats* stats = aurora_get_stats();
        for (int i = 0; i < 200 && (stats->drawCallCount == 0 || stats->lastTextureUploadSize == 0);
             i++) {
            usleep(10 * 1000);
        }
        writef(STDERR_FILENO, "[cos] font: console line drawn: %u draw calls, %u bytes of texture "
                              "upload, %u bytes of vertices\n",
               stats->drawCallCount, stats->lastTextureUploadSize, stats->lastVertSize);
        if (stats->drawCallCount == 0 || stats->lastTextureUploadSize == 0) {
            fail(name, "the frame has no draw call or no texture upload");
        }
    }
    delete console;
}

} // namespace

[[noreturn]] void smokeFont() {
    JUTResFont* font = JFWSystem::getSystemFont();
    checkSystemFont(font);
    checkDiscFont();
    checkConsoleLine(font);
    writef(STDERR_FILENO, "[cos] font: %d error(s)\n", sErrors);
    pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
