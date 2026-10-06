// J2D screens on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.13): the COS_SMOKE=blo-sweep test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc under /res
// (the FST walked with DVDOpenDir/DVDReadDir) is mounted in main RAM into a heap of this test, as
// the game mounts its message and object archives; an archive stored inside an archive (the
// disc's dat/file_select.arc) is mounted in place with JKRMemArchive::mountFixed, as d_s_name
// does. /res/Msg/fontres.arc stays mounted for the whole sweep, as the logo scene leaves it, so
// the text boxes find their fonts. Every SCRNblo1 file (*.blo) is read twice:
// - independently: plain big-endian loads at the format's offsets (no J2D struct), taken before
//   the game touches the bytes: the file header, INF1, and the block list walked as a tree
//   (BGN1/END1 open and close a level, EXT1 ends it), each PAN1/PIC1/WIN1/TBX1 block's pane
//   fields (visibility, tag, bounds, the optional rotation, base position, alpha and alpha
//   inheritance), then its kind's fields (PIC1: texture and palette references, binding, flags
//   and the optional colours; WIN1: the window box, four frame textures, palette, colours and the
//   optional contents texture and colours; TBX1: font reference, colours, binding, spacing, font
//   size, the text and the optional connect flag and colours);
// - through the game: a J2DScreen made in the test heap and J2DScreen::set(<file name>, archive),
//   as every screen of the game is made (J2DScreen::set builds the panes from a
//   JSUMemoryInputStream: J2DPane, J2DPicture, J2DWindow, J2DTextBox). set must return true; the
//   screen's size and colour must equal INF1; the pane tree walked in pre-order must hold the
//   panes of the file in order, each at its depth, with every field equal to the file's. A
//   texture or font reference whose resource is in the archive (or, for a font, in fontres.arc)
//   must give a JUTTexture on that very ResTIMG (or a JUTResFont); one to a resource of another
//   archive is only counted (the game resolves those against what it has mounted at the time).
//   Deleting the screen must give back every byte it took from the test heap.
// <COS_RUN_DIR>/blo_sweep.txt gets what the game built, one line per file in disc_manifest.py's
// names ('BLO <archive>:<path> PAN1=n PIC1=n WIN1=n TBX1=n', the panes counted in the game's pane
// tree); native/tools/run.sh compares it with the manifest (disc_manifest.py --check-blo):
// every BLO file of the disc, with its pane counts.
// The frame counter ticks once per archive, so the stall watchdog sees the sweep advance.
// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h), not in the heap
// whose free size the sweep measures.
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JSystem.h" // IWYU pragma: keep

#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/J2DGraph/J2DWindow.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/JUtility/JUTTexture.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <unistd.h>

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 48 * 1024 * 1024;
constexpr const char* kFontArchive = "/res/Msg/fontres.arc";

int sErrors = 0;
const char* sWhere = "";

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] blo-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

constexpr uint32_t tag(char a, char b, char c, char d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

String tagText(uint32_t t) {
    char s[5] = {(char)(t >> 24), (char)(t >> 16), (char)(t >> 8), (char)t, 0};
    for (int i = 0; i < 4; i++) {
        if (s[i] < 0x20 || s[i] > 0x7E) {
            s[i] = '?';
        }
    }
    return String(s);
}

struct Totals {
    unsigned int archives = 0;
    unsigned int nested = 0;
    unsigned int files = 0;
    unsigned int panes = 0;
    unsigned int textures = 0;
    unsigned int fonts = 0;
    unsigned int external = 0;
};
Totals sTotals;

// ---- the independent reading of a BLO ---------------------------------------------------------

// A JUTResReference in the file: type (1 = none, 2-4 = a resource by name), name.
struct RawRef {
    uint8_t type = 0;
    String name;
};

struct RawPane {
    uint32_t magic = 0;
    uint32_t offset = 0; // of the block in the file
    int depth = 0;       // 1 = a child of the screen
    bool visible = false;
    uint32_t tag = 0;
    int16_t x = 0, y = 0, w = 0, h = 0;
    uint16_t rotation = 0;
    bool hasBase = false;
    uint8_t base = 0;
    uint8_t alpha = 0xFF;
    bool influencedAlpha = true;
    // PIC1
    RawRef timg, tlut;
    uint8_t binding = 0, flag = 0;
    uint32_t black = 0, white = 0xFFFFFFFF;
    uint32_t corner[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    // WIN1
    uint16_t wx = 0, wy = 0, ww = 0, wh = 0;
    RawRef frame[4], contents;
    uint8_t field110 = 0;
    uint32_t colorTL = 0, colorTR = 0, colorBL = 0, colorBR = 0;
    // TBX1 (black/white shared with PIC1 and WIN1, tlut with WIN1's palette)
    RawRef font;
    uint32_t charColor = 0, gradColor = 0;
    uint8_t bindingH = 0, bindingV = 0;
    int16_t charSpace = 0, lineSpace = 0;
    uint16_t fontSizeX = 0, fontSizeY = 0;
    String text;
    bool connectParent = false;
};

struct RawBlo {
    uint32_t numBlocks = 0;
    uint16_t width = 0, height = 0;
    uint32_t color = 0;
    Vector<RawPane> panes;
    unsigned int counts[4] = {}; // PAN1, PIC1, WIN1, TBX1
};

int paneKind(uint32_t magic) {
    switch (magic) {
    case tag('P', 'A', 'N', '1'): return 0;
    case tag('P', 'I', 'C', '1'): return 1;
    case tag('W', 'I', 'N', '1'): return 2;
    case tag('T', 'B', 'X', '1'): return 3;
    default: return -1;
    }
}

const char* const kKindNames[4] = {"PAN1", "PIC1", "WIN1", "TBX1"};

// A cursor over one block's bytes; reading past the block's end is an error of the file.
struct Cursor {
    const uint8_t* base; // the file
    uint32_t pos;
    uint32_t end;        // the block's end
    bool bad = false;

    bool need(uint32_t n) {
        if (pos + n > end) {
            bad = true;
            return false;
        }
        return true;
    }
    uint8_t u8() { return need(1) ? base[pos++] : 0; }
    uint16_t u16() {
        if (!need(2)) {
            return 0;
        }
        pos += 2;
        return rd16(base + pos - 2);
    }
    uint32_t u32() {
        if (!need(4)) {
            return 0;
        }
        pos += 4;
        return rd32(base + pos - 4);
    }
    RawRef ref() {
        RawRef r;
        r.type = u8();
        uint8_t len = u8();
        if (need(len)) {
            r.name.assign((const char*)base + pos, len);
            pos += len;
        }
        return r;
    }
};

bool readRawPane(const uint8_t* b, uint32_t offset, uint32_t size, int depth, RawPane& p) {
    Cursor c{b, offset + 8, offset + size};
    p.magic = rd32(b + offset);
    p.offset = offset;
    p.depth = depth;
    uint8_t num = c.u8();
    p.visible = c.u8() != 0;
    c.pos += 2;
    p.tag = c.u32();
    p.x = (int16_t)c.u16();
    p.y = (int16_t)c.u16();
    p.w = (int16_t)c.u16();
    p.h = (int16_t)c.u16();
    num -= 6;
    if (num != 0) {
        p.rotation = c.u16();
        num--;
    }
    if (num != 0) {
        p.hasBase = true;
        p.base = c.u8();
        num--;
    }
    if (num != 0) {
        p.alpha = c.u8();
        num--;
    }
    if (num != 0) {
        p.influencedAlpha = c.u8() != 0;
        num--;
    }
    c.pos = (c.pos + 3) & ~3u; // aligned in the file, as the pane stream is
    switch (paneKind(p.magic)) {
    case 1: { // PIC1
        uint8_t n = c.u8();
        p.timg = c.ref();
        p.tlut = c.ref();
        p.binding = c.u8();
        n -= 3;
        if (n != 0) {
            p.flag = c.u8();
            n--;
        }
        if (n != 0) {
            c.u8();
            n--;
        }
        p.black = 0;
        if (n != 0) {
            p.black = c.u32();
            n--;
        }
        if (n != 0) {
            p.white = c.u32();
            n--;
        }
        for (int i = 0; n > 0 && i < 4; i++) {
            p.corner[i] = c.u32();
            n--;
        }
        break;
    }
    case 2: { // WIN1
        uint8_t n = c.u8();
        p.wx = c.u16();
        p.wy = c.u16();
        p.ww = c.u16();
        p.wh = c.u16();
        for (RawRef& r : p.frame) {
            r = c.ref();
        }
        p.tlut = c.ref();
        p.field110 = c.u8();
        p.colorTL = c.u32();
        p.colorTR = c.u32();
        p.colorBL = c.u32();
        p.colorBR = c.u32();
        n -= 14;
        if (n != 0) {
            p.contents = c.ref();
            n--;
        }
        p.black = 0;
        if (n != 0) {
            p.black = c.u32();
            n--;
        }
        if (n != 0) {
            p.white = c.u32();
        }
        break;
    }
    case 3: { // TBX1
        uint8_t n = c.u8();
        p.font = c.ref();
        p.charColor = c.u32();
        p.gradColor = c.u32();
        uint8_t binding = c.u8();
        p.bindingH = (binding >> 2) & 3;
        p.bindingV = binding & 3;
        p.charSpace = (int16_t)c.u16();
        p.lineSpace = (int16_t)c.u16();
        p.fontSizeX = c.u16();
        p.fontSizeY = c.u16();
        int16_t len = (int16_t)c.u16();
        if (len > 0 && c.need((uint32_t)len)) {
            p.text.assign((const char*)b + c.pos, (size_t)len);
            c.pos += (uint32_t)len;
        }
        // The game stops the text at its first NUL.
        p.text.resize(strnlen(p.text.c_str(), p.text.size()));
        n -= 10;
        if (n != 0) {
            p.connectParent = c.u8() != 0;
            n--;
        }
        p.black = 0;
        if (n != 0) {
            p.black = c.u32();
            n--;
        }
        if (n != 0) {
            p.white = c.u32();
        }
        break;
    }
    default: break;
    }
    if (c.bad) {
        fail("%s block at 0x%x: its fields run past its %u bytes", tagText(p.magic).c_str(), offset,
             size);
        return false;
    }
    return true;
}

bool parseRawBlo(const uint8_t* b, uint32_t length, RawBlo& raw) {
    if (length < 0x30 || memcmp(b, "SCRNblo1", 8) != 0) {
        fail("not an SCRNblo1 file (%u bytes)", length);
        return false;
    }
    raw.numBlocks = rd32(b + 0x0C);
    uint32_t o = 0x20;
    if (rd32(b + o) != tag('I', 'N', 'F', '1') || rd32(b + o + 4) < 0x10) {
        fail("no INF1 block at 0x20");
        return false;
    }
    raw.width = rd16(b + o + 8);
    raw.height = rd16(b + o + 0xA);
    raw.color = rd32(b + o + 0xC);
    o += rd32(b + o + 4);
    int depth = 1;
    bool ended = false;
    uint32_t blocks = 1;
    while (!ended) {
        if (o + 8 > length || rd32(b + o + 4) < 8 || o + rd32(b + o + 4) > length) {
            fail("block %u at 0x%x lies past the end (%u bytes)", blocks, o, length);
            return false;
        }
        uint32_t magic = rd32(b + o);
        uint32_t size = rd32(b + o + 4);
        blocks++;
        if (magic == tag('E', 'X', 'T', '1')) {
            ended = true;
        } else if (magic == tag('B', 'G', 'N', '1')) {
            depth++;
        } else if (magic == tag('E', 'N', 'D', '1')) {
            if (--depth < 1) {
                fail("END1 at 0x%x closes the screen's level", o);
                return false;
            }
        } else {
            int kind = paneKind(magic);
            if (kind < 0) {
                fail("unknown block %s at 0x%x", tagText(magic).c_str(), o);
                return false;
            }
            RawPane p;
            if (!readRawPane(b, o, size, depth, p)) {
                return false;
            }
            raw.panes.push_back(p);
            raw.counts[kind]++;
        }
        o += size;
    }
    if (depth != 1 || blocks != raw.numBlocks) {
        fail("EXT1 after %u of %u blocks at depth %d", blocks, raw.numBlocks, depth);
        return false;
    }
    return true;
}

// ---- the game's panes --------------------------------------------------------------------------

// The protected members of the pane classes, through pointers to members named in derived classes
// (the game's classes are not changed for the test).
struct PaneAccess : J2DPane {
    static constexpr u32 J2DPane::*magic = &PaneAccess::mMagic;
    static constexpr int J2DPane::*tag = &PaneAccess::mTag;
    static constexpr bool J2DPane::*visible = &PaneAccess::mVisible;
    static constexpr JGeometry::TBox2<f32> J2DPane::*bounds = &PaneAccess::mBounds;
    static constexpr f32 J2DPane::*rotation = &PaneAccess::mRotation;
    static constexpr u8 J2DPane::*basePosition = &PaneAccess::m2DBasePosition;
    static constexpr bool J2DPane::*influencedAlpha = &PaneAccess::mInfluencedAlpha;
};

struct PictureAccess : J2DPicture {
    static constexpr JUTTexture* (J2DPicture::*texture)[4] = &PictureAccess::mpTexture;
    static constexpr u8 J2DPicture::*numTexture = &PictureAccess::mNumTexture;
    static constexpr u8 J2DPicture::*binding = &PictureAccess::mBinding;
    static constexpr u8 J2DPicture::*flag = &PictureAccess::mFlag;
    static constexpr JUTPalette* J2DPicture::*palette = &PictureAccess::mpPalette;
    static constexpr JUtility::TColor (J2DPicture::*corner)[4] = &PictureAccess::mCornerColor;
};

struct GamePane {
    J2DPane* pane;
    int depth;
};

void walkPanes(J2DPane* parent, int depth, Vector<GamePane>& out) {
    for (JSUTree<J2DPane>* t = parent->getFirstChild(); t != parent->getEndChild();
         t = t->getNextChild()) {
        out.push_back({t->getObject(), depth});
        walkPanes(t->getObject(), depth + 1, out);
    }
}

// What a reference to `ref` should give: the resource of the same name in `arc` (or `extra`),
// else nullptr (a resource of another archive: counted, not checked).
void* expectedResource(const RawRef& ref, JKRArchive* arc, const Vector<String>& paths,
                       JKRArchive* extra, const Vector<String>& extraPaths) {
    if (ref.type < 2 || ref.type > 4 || ref.name.empty()) {
        return nullptr;
    }
    auto find = [&](JKRArchive* a, const Vector<String>& list) -> void* {
        for (const String& p : list) {
            const char* slash = strrchr(p.c_str(), '/');
            const char* base = slash != nullptr ? slash + 1 : p.c_str();
            if (strcasecmp(base, ref.name.c_str()) == 0) {
                return a->getResource((String("/") + p).c_str());
            }
        }
        return nullptr;
    };
    void* res = find(arc, paths);
    if (res == nullptr && extra != nullptr) {
        res = find(extra, extraPaths);
    }
    if (res == nullptr) {
        sTotals.external++;
    }
    return res;
}

void checkTexture(const char* what, const RawRef& ref, const JUTTexture* tex, JKRArchive* arc,
                  const Vector<String>& paths, uint32_t paneIndex) {
    void* want = expectedResource(ref, arc, paths, nullptr, Vector<String>());
    if (want == nullptr) {
        return;
    }
    sTotals.textures++;
    if (tex == nullptr || tex->getTexInfo() != want) {
        fail("pane %u: %s \"%s\": texture %p on %p, the archive's resource is %p", paneIndex, what,
             ref.name.c_str(), (const void*)tex, tex != nullptr ? (const void*)tex->getTexInfo() : nullptr,
             want);
        return;
    }
    const uint8_t* timg = (const uint8_t*)want;
    if ((uint32_t)tex->getWidth() != rd16(timg + 2) || (uint32_t)tex->getHeight() != rd16(timg + 4)) {
        fail("pane %u: %s \"%s\" is %dx%d, the file says %ux%u", paneIndex, what, ref.name.c_str(),
             (int)tex->getWidth(), (int)tex->getHeight(), rd16(timg + 2), rd16(timg + 4));
    }
}

void checkColor(uint32_t paneIndex, const char* what, uint32_t got, uint32_t want) {
    if (got != want) {
        fail("pane %u: %s 0x%08x, the file has 0x%08x", paneIndex, what, got, want);
    }
}

void checkPane(J2DPane* pane, const RawPane& raw, uint32_t i, JKRArchive* arc,
               const Vector<String>& paths, JKRArchive* fontArc, const Vector<String>& fontPaths) {
    u32 magic = pane->*PaneAccess::magic;
    const JGeometry::TBox2<f32>& bx = pane->*PaneAccess::bounds;
    float x0 = raw.x, y0 = raw.y, x1 = x0 + raw.w, y1 = y0 + raw.h;
    if (magic != raw.magic || (u32)(pane->*PaneAccess::tag) != raw.tag ||
        pane->*PaneAccess::visible != raw.visible || bx.i.x != x0 || bx.i.y != y0 ||
        bx.f.x != x1 || bx.f.y != y1 || pane->*PaneAccess::rotation != (f32)raw.rotation ||
        pane->*PaneAccess::basePosition != (raw.hasBase ? raw.base : 0) ||
        pane->getAlpha() != raw.alpha ||
        pane->*PaneAccess::influencedAlpha != raw.influencedAlpha) {
        fail("pane %u (block at 0x%x): %s '%s' visible %d (%g,%g)-(%g,%g) rot %g base %u alpha %u "
             "inherit %d; the file has %s '%s' %d (%g,%g)-(%g,%g) %u %u %u %d",
             i, raw.offset, tagText(magic).c_str(), tagText((u32)(pane->*PaneAccess::tag)).c_str(),
             (int)(pane->*PaneAccess::visible), bx.i.x, bx.i.y, bx.f.x, bx.f.y,
             pane->*PaneAccess::rotation, pane->*PaneAccess::basePosition, pane->getAlpha(),
             (int)(pane->*PaneAccess::influencedAlpha), tagText(raw.magic).c_str(),
             tagText(raw.tag).c_str(), (int)raw.visible, x0, y0, x1, y1, raw.rotation,
             raw.hasBase ? raw.base : 0, raw.alpha, (int)raw.influencedAlpha);
        return;
    }
    switch (paneKind(raw.magic)) {
    case 1: {
        J2DPicture* pic = (J2DPicture*)pane;
        if (pane->getTypeID() != 0x12) {
            fail("pane %u: PIC1 made a pane of type 0x%x", i, pane->getTypeID());
            return;
        }
        if (pic->*PictureAccess::binding != raw.binding || pic->*PictureAccess::flag != raw.flag) {
            fail("pane %u: binding 0x%x flag 0x%x, the file has 0x%x 0x%x", i,
                 pic->*PictureAccess::binding, pic->*PictureAccess::flag, raw.binding, raw.flag);
        }
        checkColor(i, "black", pic->getBlack().toUInt32(), raw.black);
        checkColor(i, "white", pic->getWhite().toUInt32(), raw.white);
        for (int k = 0; k < 4; k++) {
            checkColor(i, "corner colour", (pic->*PictureAccess::corner)[k].toUInt32(),
                       raw.corner[k]);
        }
        checkTexture("texture", raw.timg, (pic->*PictureAccess::texture)[0], arc, paths, i);
        if (raw.tlut.type >= 2 && raw.tlut.type <= 4 &&
            expectedResource(raw.tlut, arc, paths, nullptr, Vector<String>()) != nullptr &&
            pic->*PictureAccess::palette == nullptr) {
            fail("pane %u: palette \"%s\" not made", i, raw.tlut.name.c_str());
        }
        break;
    }
    case 2: {
        J2DWindow* win = (J2DWindow*)pane;
        if (pane->getTypeID() != 0x11) {
            fail("pane %u: WIN1 made a pane of type 0x%x", i, pane->getTypeID());
            return;
        }
        const JGeometry::TBox2<f32>& wb = win->mWindowBox;
        if (wb.i.x != raw.wx || wb.i.y != raw.wy || wb.f.x != (f32)raw.wx + raw.ww ||
            wb.f.y != (f32)raw.wy + raw.wh || win->field_0x110 != raw.field110) {
            fail("pane %u: window box (%g,%g)-(%g,%g) 0x%x, the file has %u,%u %ux%u 0x%x", i,
                 wb.i.x, wb.i.y, wb.f.x, wb.f.y, win->field_0x110, raw.wx, raw.wy, raw.ww, raw.wh,
                 raw.field110);
        }
        checkColor(i, "top left", win->mColorTL.toUInt32(), raw.colorTL);
        checkColor(i, "top right", win->mColorTR.toUInt32(), raw.colorTR);
        checkColor(i, "bottom left", win->mColorBL.toUInt32(), raw.colorBL);
        checkColor(i, "bottom right", win->mColorBR.toUInt32(), raw.colorBR);
        checkColor(i, "black", win->getBlack().toUInt32(), raw.black);
        checkColor(i, "white", win->getWhite().toUInt32(), raw.white);
        JUTTexture* frames[4] = {win->mpFrameTexture1, win->mpFrameTexture2, win->mpFrameTexture3,
                                 win->mpFrameTexture4};
        for (int k = 0; k < 4; k++) {
            checkTexture("frame texture", raw.frame[k], frames[k], arc, paths, i);
        }
        checkTexture("contents texture", raw.contents, win->mpContentsTexture, arc, paths, i);
        break;
    }
    case 3: {
        J2DTextBox* tbx = (J2DTextBox*)pane;
        if (pane->getTypeID() != 19) {
            fail("pane %u: TBX1 made a pane of type 0x%x", i, pane->getTypeID());
            return;
        }
        checkColor(i, "character colour", tbx->mCharColor.toUInt32(), raw.charColor);
        checkColor(i, "gradient colour", tbx->mGradColor.toUInt32(), raw.gradColor);
        checkColor(i, "black", tbx->mBlack.toUInt32(), raw.black);
        checkColor(i, "white", tbx->mWhite.toUInt32(), raw.white);
        if (tbx->mBindingH != raw.bindingH || tbx->mBindingV != raw.bindingV ||
            tbx->mCharSpace != (f32)raw.charSpace || tbx->mLineSpace != (f32)raw.lineSpace ||
            tbx->mFontSizeX != (f32)raw.fontSizeX || tbx->mFontSizeY != (f32)raw.fontSizeY) {
            fail("pane %u: binding %u/%u spacing %g/%g size %gx%g, the file has %u/%u %d/%d %ux%u",
                 i, tbx->mBindingH, tbx->mBindingV, tbx->mCharSpace, tbx->mLineSpace,
                 tbx->mFontSizeX, tbx->mFontSizeY, raw.bindingH, raw.bindingV, raw.charSpace,
                 raw.lineSpace, raw.fontSizeX, raw.fontSizeY);
        }
        if (tbx->mStringPtr == nullptr || raw.text != tbx->mStringPtr) {
            fail("pane %u: text \"%.40s\", the file has \"%.40s\"", i,
                 tbx->mStringPtr != nullptr ? tbx->mStringPtr : "(null)", raw.text.c_str());
        }
        if (pane->isConnectParent() != raw.connectParent) {
            fail("pane %u: connect-parent %d, the file has %d", i, (int)pane->isConnectParent(),
                 (int)raw.connectParent);
        }
        if (expectedResource(raw.font, arc, paths, fontArc, fontPaths) != nullptr) {
            sTotals.fonts++;
            if (tbx->getFont() == nullptr) {
                fail("pane %u: font \"%s\" not made", i, raw.font.name.c_str());
            }
        }
        break;
    }
    default:
        if (pane->getTypeID() != 0x10) {
            fail("pane %u: PAN1 made a pane of type 0x%x", i, pane->getTypeID());
        }
        break;
    }
}

// ---- the archives -------------------------------------------------------------------------------

// Every file of the archive by its path inside it (the manifest's names), from the game's tables.
void archivePaths(JKRArchive* arc, uint32_t node, const String& prefix, int depth,
                  Vector<String>& out) {
    uint32_t numNodes = arc->mArcInfoBlock->num_nodes;
    uint32_t numEntries = arc->mArcInfoBlock->num_file_entries;
    if (depth > 32 || node >= numNodes) {
        fail("bad directory node %u", node);
        return;
    }
    JKRArchive::SDIDirEntry* n = arc->mNodes + node;
    for (uint32_t k = n->first_file_index; k < n->first_file_index + n->num_entries; k++) {
        if (k >= numEntries) {
            fail("entry %u out of range", k);
            return;
        }
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        const char* name = arc->mStringTable + e->getNameOffset();
        if (e->isDirectory()) {
            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                archivePaths(arc, e->data_offset, prefix + name + "/", depth + 1, out);
            }
            continue;
        }
        out.push_back(prefix + name);
    }
}

bool hasSuffix(const String& s, const char* suffix) {
    size_t n = strlen(suffix);
    return s.size() > n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}

struct FontArchive {
    JKRArchive* arc = nullptr;
    Vector<String> paths;
};
FontArchive sFonts;

void sweepBlo(const String& name, const String& inner, JKRArchive* arc, const Vector<String>& paths,
              JKRHeap* heap, int fd) {
    sWhere = name.c_str();
    void* res = arc->getResource((String("/") + inner).c_str());
    uint32_t length = res != nullptr ? arc->getExpandedResSize(res) : 0;
    if (res == nullptr || length == 0) {
        fail("no resource");
        return;
    }
    // The independent reading, before the game touches the bytes.
    Vector<uint8_t> copy((const uint8_t*)res, (const uint8_t*)res + length);
    RawBlo raw;
    if (!parseRawBlo(copy.data(), length, raw)) {
        return;
    }

    const char* slash = strrchr(inner.c_str(), '/');
    const char* fileName = slash != nullptr ? slash + 1 : inner.c_str();
    s32 heapFree = heap->getTotalFreeSize();
    JKRHeap* oldHeap = heap->becomeCurrentHeap();
    J2DScreen* screen = new J2DScreen();
    bool ok = screen != nullptr && screen->set(fileName, arc);
    oldHeap->becomeCurrentHeap();
    sTotals.files++;
    if (!ok) {
        fail("J2DScreen::set(\"%s\") failed", fileName);
    } else {
        const JGeometry::TBox2<f32>& bx = screen->getBounds();
        if (bx.i.x != 0 || bx.i.y != 0 || bx.f.x != raw.width || bx.f.y != raw.height) {
            fail("screen (%g,%g)-(%g,%g), INF1 says %ux%u", bx.i.x, bx.i.y, bx.f.x, bx.f.y,
                 raw.width, raw.height);
        }
        Vector<GamePane> panes;
        walkPanes(screen, 1, panes);
        unsigned int counts[4] = {};
        for (const GamePane& g : panes) {
            int kind = paneKind(g.pane->*PaneAccess::magic);
            if (kind >= 0) {
                counts[kind]++;
            }
        }
        if (panes.size() != raw.panes.size()) {
            fail("%zu panes made, the file has %zu", panes.size(), raw.panes.size());
        } else {
            for (uint32_t i = 0; i < panes.size(); i++) {
                if (panes[i].depth != raw.panes[i].depth) {
                    fail("pane %u (block at 0x%x) at depth %d, the file has it at %d", i,
                         raw.panes[i].offset, panes[i].depth, raw.panes[i].depth);
                    continue;
                }
                checkPane(panes[i].pane, raw.panes[i], i, arc, paths, sFonts.arc, sFonts.paths);
            }
        }
        sTotals.panes += panes.size();
        if (fd >= 0) {
            writef(fd, "BLO %s PAN1=%u PIC1=%u WIN1=%u TBX1=%u\n", name.c_str(), counts[0],
                   counts[1], counts[2], counts[3]);
        }
    }
    delete screen;
    if (heap->getTotalFreeSize() != heapFree) {
        fail("deleting the screen left %d bytes of the test heap taken",
             (int)(heapFree - heap->getTotalFreeSize()));
    }
}

void sweepMounted(JKRArchive* arc, const String& name, JKRHeap* heap, int fd, int depth) {
    Vector<String> paths;
    archivePaths(arc, 0, "", 0, paths);
    bool screens = false;
    for (const String& inner : paths) {
        screens = screens || hasSuffix(inner, ".blo");
    }
    if (screens) {
        // Every file fetched first (an ARAM mount keeps what it fetched until the unmount), so
        // that the heap check around each screen sees only what the screen took.
        for (const String& inner : paths) {
            arc->getResource((String("/") + inner).c_str());
        }
    }
    for (const String& inner : paths) {
        if (hasSuffix(inner, ".blo")) {
            sweepBlo(name + ":" + inner, inner, arc, paths, heap, fd);
            sWhere = name.c_str();
        } else if (hasSuffix(inner, ".arc") && depth == 0) {
            // An archive inside an archive, mounted in place as d_s_name mounts it.
            void* data = arc->getResource((String("/") + inner).c_str());
            String nested = name + ":" + inner;
            sWhere = nested.c_str();
            JKRMemArchive* sub = data != nullptr ? new (heap, 0) JKRMemArchive() : nullptr;
            if (sub == nullptr || !sub->mountFixed(data, JKRMEMBREAK_FLAG_UNKNOWN0)) {
                fail("mountFixed failed");
            } else {
                sTotals.nested++;
                sweepMounted(sub, nested, heap, fd, depth + 1);
                sub->unmountFixed();
            }
            delete sub;
            sWhere = name.c_str();
        }
    }
}

// Whether any file of the archive is stored compressed.
bool hasCompressedFiles(JKRArchive* arc) {
    for (uint32_t k = 0; k < arc->mArcInfoBlock->num_file_entries; k++) {
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        if (!e->isDirectory() && e->isCompressed()) {
            return true;
        }
    }
    return false;
}

void sweepArchive(const String& path, JKRHeap* heap, int fd) {
    sWhere = path.c_str();
    s32 heapFree = heap->getTotalFreeSize();
    // In main RAM, as dRes_info_c and the logo scene's onMemMount mount archives; one with files
    // stored compressed in ARAM, as the logo scene's aramMount mounts those (/res/Msg's screen
    // archives): a MEM mount hands out a compressed file as it is stored.
    JKRArchive* arc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_MEM, heap,
                                        JKRArchive::MOUNT_DIRECTION_HEAD);
    if (arc != nullptr && hasCompressedFiles(arc)) {
        arc->unmount();
        arc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_ARAM, heap,
                                JKRArchive::MOUNT_DIRECTION_HEAD);
    }
    if (arc == nullptr) {
        fail("mount failed");
        return;
    }
    sTotals.archives++;
    sweepMounted(arc, path, heap, fd, 0);
    arc->unmount();
    if (heap->getTotalFreeSize() != heapFree) {
        fail("after unmount the test heap lost %d bytes", (int)(heapFree - heap->getTotalFreeSize()));
    }
}

void findArchives(const String& dirPath, Vector<String>& out, int depth) {
    DVDDir dir;
    if (!DVDOpenDir(dirPath.c_str(), &dir)) {
        sWhere = dirPath.c_str();
        fail("DVDOpenDir failed");
        return;
    }
    DVDDirEntry entry;
    Vector<String> subdirs;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr) {
            continue;
        }
        String child = dirPath + "/" + entry.name;
        if (entry.isDir) {
            subdirs.push_back(child);
        } else if (hasSuffix(child, ".arc")) {
            out.push_back(child);
        }
    }
    DVDCloseDir(&dir);
    if (depth < 4) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

} // namespace

[[noreturn]] void smokeBloSweep() {
    uint64_t start = elapsedMs();
    sWhere = "blo-sweep";
    Vector<String> archives;
    findArchives("/res", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] blo-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    // The message fonts stay mounted, as the logo scene leaves fontres.arc.
    sWhere = kFontArchive;
    sFonts.arc = JKRArchive::mount(kFontArchive, JKRArchive::MOUNT_MEM, heap,
                                   JKRArchive::MOUNT_DIRECTION_HEAD);
    if (sFonts.arc == nullptr) {
        fail("mount failed");
    } else {
        archivePaths(sFonts.arc, 0, "", 0, sFonts.paths);
    }
    int fd = openRunFile("blo_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# blo-sweep (COS_SMOKE=blo-sweep): the panes of every BLO as J2DScreen::set "
                   "built them, in disc_manifest.py's names\n");
    }
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sWhere = "blo-sweep";
    if (sFonts.arc != nullptr) {
        sFonts.arc->unmount();
    }
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the test heap");
    }
    writef(STDERR_FILENO,
           "[cos] blo-sweep: %u archives (%u nested), %u BLO files, %u panes, %u textures and %u "
           "fonts checked, %u references to other archives; %llu ms; %d error(s)%s\n",
           sTotals.archives, sTotals.nested, sTotals.files, sTotals.panes, sTotals.textures,
           sTotals.fonts, sTotals.external, (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in blo_sweep.txt)" : "");
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && sTotals.files > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
