// Messages on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.6): the COS_SMOKE=msg-sweep test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. It mounts the
// archives the logo scene mounts for messages (/res/Msg/bmgres.arc, bmgresh.arc, fontres.arc and
// rubyres.arc, MEM mounts in the archive heap) and hands them to dComIfGp as d_s_logo's phase_2
// does. Each BMG (zel_00.bmg, zel_01.bmg) is read independently first: plain big-endian loads at
// the format's offsets, no game struct. Then, through the game's own code:
// - JMessage, as dMesg_parse does: a TParse over both files into one TResourceContainer; each
//   TResource's header, INF1 and DAT1 pointers, entry count, entry size and group; for every
//   message index, TControl::getMessageEntry and getMessageData; and a TRenderingProcessor run
//   over every message (characters and tags, against the independent walk);
// - fopMsgM, as the message windows read it: every INF1 entry through
//   fopMsgM_msgGet_c::getMesgEntry (all fields); for every message number, getMesgHeader routes
//   to the file the game's language check (fopMsgM) chooses, and in that file fopMsgM_msgGet_c and
//   fopMsgM_itemMsgGet_c::getMessage return the first entry with the number, and
//   fopMsgM_messageGet decodes it (control tags dropped, the player name inserted, 0x1A for tag
//   0x1E) to the same text as the independent decoder;
// - the colour table: fopMsgM_getColorTable for every CLT1 entry of color.bmc;
// - the message fonts: mDoExt_getMesgFont (rock_24_20_4i_usa.bfn) and mDoExt_getRubyFont
//   (kRubyFont) are JUTResFonts checked as the font smoke test checks a disc font (checkResFont).
// <COS_RUN_DIR>/msg_sweep.txt gets what the game's code read (BMG, BMC and FONT lines in
// disc_manifest.py's names); native/tools/run.sh compares it with the manifest
// (disc_manifest.py --check-msg): every BMG's counts and message-ID digest, the colour digest and
// the fonts' BFN headers. Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JMessage/control.h"
#include "JSystem/JMessage/processor.h"
#include "JSystem/JMessage/resource.h"
#include "JSystem/JUtility/JUTResFont.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_ext.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

// The ruby (furigana) font of the message archives, as mDoExt_initFontCommon looks it up.
constexpr char kRubyFont[] = "hyrule.bfn";
constexpr char kRubyFontPath[] = "/res/Msg/rubyres.arc:hyrule.bfn";

int sErrors = 0;

void fail(const char* where, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void fail(const char* where, const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] msg-sweep: %s: %s\n", where, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

constexpr uint64_t kFnvBasis = 0xCBF29CE484222325ull;

// FNV-1a 64 (disc_manifest.py's fnv1a64).
uint64_t fnv(uint64_t h, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 0x100000001B3ull;
    }
    return h;
}

uint64_t fnvBE32(uint64_t h, uint32_t v) {
    uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    return fnv(h, b, 4);
}

uint64_t fnvBE16(uint64_t h, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
    return fnv(h, b, 2);
}

constexpr uint32_t tag(char a, char b, char c, char d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

// ---- the independent reading of a BMG ---------------------------------------------------------

struct RawBmg {
    const char* path = nullptr; // disc_manifest.py's name: "<archive>:<file>"
    const uint8_t* base = nullptr;
    uint32_t length = 0;
    uint32_t numBlocks = 0;
    uint8_t encoding = 0;
    const uint8_t* inf = nullptr;
    const uint8_t* dat = nullptr;
    uint32_t datSize = 0;
    uint16_t entries = 0;
    uint16_t entrySize = 0;
    uint16_t group = 0;

    const uint8_t* entry(int i) const { return inf + 0x10 + (size_t)i * entrySize; }
    uint32_t offset(int i) const { return rd32(entry(i)); }
    uint16_t number(int i) const { return rd16(entry(i) + 4); }
    const char* text(int i) const { return (const char*)dat + 8 + offset(i); }
};

bool parseRawBmg(const char* path, const uint8_t* base, uint32_t length, RawBmg& raw) {
    raw.path = path;
    raw.base = base;
    raw.length = length;
    if (length < 0x20 || memcmp(base, "MESGbmg1", 8) != 0) {
        fail(path, "not a MESGbmg1 file");
        return false;
    }
    raw.numBlocks = rd32(base + 0x0C);
    raw.encoding = base[0x10];
    const uint8_t* block = base + 0x20;
    for (uint32_t i = 0; i < raw.numBlocks; i++) {
        if (block + 8 > base + length || rd32(block + 4) < 8 || block + rd32(block + 4) > base + length) {
            fail(path, "block %u lies past the end", i);
            return false;
        }
        if (rd32(block) == tag('I', 'N', 'F', '1')) {
            raw.inf = block;
        } else if (rd32(block) == tag('D', 'A', 'T', '1')) {
            raw.dat = block;
            raw.datSize = rd32(block + 4);
        }
        block += rd32(block + 4);
    }
    if (raw.inf == nullptr || raw.dat == nullptr) {
        fail(path, "no INF1 or no DAT1 block");
        return false;
    }
    raw.entries = rd16(raw.inf + 0x08);
    raw.entrySize = rd16(raw.inf + 0x0A);
    raw.group = rd16(raw.inf + 0x0C);
    if (raw.entrySize < 0x18 || raw.entry(raw.entries) > raw.inf + rd32(raw.inf + 4)) {
        fail(path, "%u entries of %u bytes do not fit INF1", raw.entries, raw.entrySize);
        return false;
    }
    for (int i = 0; i < raw.entries; i++) {
        if (raw.offset(i) >= raw.datSize - 8) {
            fail(path, "entry %d: text offset 0x%x outside DAT1", i, raw.offset(i));
            return false;
        }
    }
    return true;
}

// The independent walk of one message, as JMessage's processor reads it with the 1-byte encoding:
// characters, and tags (0x1A, total length, group, code u16). Returns false on a malformed tag.
struct Walk {
    uint32_t characters = 0;
    uint32_t tags = 0;
    uint64_t digest = kFnvBasis; // characters as u16, tags as (group << 16 | code) u32
};

bool walkText(const RawBmg& raw, const char* text, Walk& w) {
    const uint8_t* end = raw.dat + raw.datSize;
    const uint8_t* p = (const uint8_t*)text;
    while (p < end && *p != 0) {
        if (*p == 0x1A) {
            if (p + 5 > end || p[1] < 5) {
                return false;
            }
            w.tags++;
            w.digest = fnvBE32(w.digest, ((uint32_t)p[2] << 16) | rd16(p + 3));
            p += p[1];
        } else {
            w.characters++;
            w.digest = fnvBE16(w.digest, *p);
            p++;
        }
    }
    return p < end;
}

// fopMsgM_messageGet's output, decoded independently (VERSION_USA): tag 0 is the player name, tag
// 0x1E a 0x1A, other tags are dropped; a byte with high nibble 8 or 9 is copied with the next.
// PAL language 1's possessive suffix is not modelled (the sweep requires language 0).
bool decodeText(const RawBmg& raw, const char* text, const char* playerName, char* out, size_t cap) {
    const uint8_t* end = raw.dat + raw.datSize;
    const uint8_t* p = (const uint8_t*)text;
    size_t n = 0;
    auto put = [&](char c) {
        if (n + 1 < cap) {
            out[n] = c;
        }
        n++;
    };
    while (p < end && *p != 0) {
        if (*p == 0x1A) {
            if (p + 5 > end || p[1] < 5) {
                return false;
            }
            uint32_t code = ((uint32_t)p[2] << 16) | rd16(p + 3);
            if (code == 0x1E) {
                put(0x1A);
            } else if (code == 0) {
                for (const char* q = playerName; *q != '\0'; q++) {
                    put(*q);
                }
            }
            p += p[1];
        } else if ((*p >> 4) == 8 || (*p >> 4) == 9) {
            put((char)p[0]);
            put((char)p[1]);
            p += 2;
        } else {
            put((char)*p);
            p++;
        }
    }
    if (n + 1 > cap) {
        return false;
    }
    out[n] = '\0';
    return true;
}

// ---- JMessage ---------------------------------------------------------------------------------

// A TRenderingProcessor that counts what it reads; tags still go through the game's own handling
// (do_tag returns false).
struct CountingProcessor : public JMessage::TRenderingProcessor {
    CountingProcessor(JMessage::TControl* control) : TRenderingProcessor(control) {}
    void do_character(int c) override {
        walk.characters++;
        walk.digest = fnvBE16(walk.digest, (uint16_t)c);
    }
    bool do_tag(u32 t, const void*, u32) override {
        walk.tags++;
        walk.digest = fnvBE32(walk.digest, t);
        return false;
    }
    void do_end() override { ended = true; }

    Walk walk;
    bool ended = false;
};

void checkJMessage(JMessage::TResourceContainer& container, const RawBmg* bmgs, int count,
                   uint32_t& processed) {
    JMessage::TControl control;
    control.mResourceContainer = &container;
    CountingProcessor proc(&control);
    for (int k = 0; k < count; k++) {
        const RawBmg& raw = bmgs[k];
        JMessage::TResource* res = container.Get_groupID(raw.group);
        if (res == nullptr) {
            fail(raw.path, "JMessage: no resource for group %u", raw.group);
            continue;
        }
        if (res->mHeader.getRaw() != raw.base || res->mInfo.getRaw() != raw.inf ||
            res->mMessageData != (const char*)raw.dat + 8) {
            fail(raw.path, "JMessage: header/INF1/DAT1 at %p/%p/%p, the file has %p/%p/%p",
                 res->mHeader.getRaw(), res->mInfo.getRaw(), res->mMessageData, raw.base, raw.inf,
                 raw.dat + 8);
            continue;
        }
        if (res->getMessageEntryNumber() != raw.entries || res->getMessageEntrySize() != raw.entrySize ||
            res->getGroupID() != raw.group || res->mHeader.get_blockNumber() != raw.numBlocks ||
            res->mHeader.get_encoding() != raw.encoding) {
            fail(raw.path, "JMessage: %u entries of %u bytes, group %u, %u blocks, encoding %u; the "
                           "file has %u, %u, %u, %u, %u",
                 res->getMessageEntryNumber(), res->getMessageEntrySize(), res->getGroupID(),
                 res->mHeader.get_blockNumber(), res->mHeader.get_encoding(), raw.entries,
                 raw.entrySize, raw.group, raw.numBlocks, raw.encoding);
            continue;
        }
        int bad = 0;
        for (int i = 0; i < raw.entries; i++) {
            const void* entry = control.getMessageEntry(raw.group, (u16)i);
            const char* data = control.getMessageData(raw.group, (u16)i);
            if (entry != raw.entry(i) || data != raw.text(i)) {
                if (bad++ < 5) {
                    fail(raw.path, "JMessage: index %d entry %p text %p, the file has %p %p", i, entry,
                         data, raw.entry(i), raw.text(i));
                }
                continue;
            }
            if (raw.offset(i) == 0) {
                continue;
            }
            Walk want;
            if (!walkText(raw, raw.text(i), want)) {
                fail(raw.path, "index %d: malformed text", i);
                continue;
            }
            proc.walk = Walk();
            proc.ended = false;
            proc.setBegin(entry, data);
            const char* stop = proc.process(nullptr);
            if (stop != nullptr || !proc.ended || proc.walk.characters != want.characters ||
                proc.walk.tags != want.tags || proc.walk.digest != want.digest) {
                if (bad++ < 5) {
                    fail(raw.path, "JMessage: index %d: processor read %u characters, %u tags%s; the "
                                   "text has %u, %u",
                         i, proc.walk.characters, proc.walk.tags,
                         proc.ended ? (proc.walk.digest != want.digest ? " (other values)" : "")
                                    : " and did not end",
                         want.characters, want.tags);
                }
                continue;
            }
            processed++;
        }
        if (bad > 5) {
            fail(raw.path, "JMessage: %d indexes wrong in all", bad);
        }
    }
}

// ---- fopMsgM ----------------------------------------------------------------------------------

struct FopResult {
    uint32_t messages = 0;     // entries with text, read through getMesgEntry
    uint32_t distinct = 0;     // distinct numbers among them
    uint64_t idsDigest = kFnvBasis;
    uint32_t decoded = 0;      // numbers fopMsgM routes to this file, checked and decoded
    uint32_t elsewhere = 0;    // numbers the language check routes to the other file
    uint32_t duplicates = 0;   // entries whose number an earlier entry already has
    uint32_t decodedBytes = 0;
};

void checkFopMsgM(const RawBmg& raw, mesg_header* header, bool secondFile, const char* playerName,
                  FopResult& r) {
    static char got[8192];
    static char want[8192];
    static uint8_t seen[0x10000];
    memset(seen, 0, sizeof(seen));
    fopMsgM_msgGet_c entryGet;
    int bad = 0;
    for (int i = 0; i < raw.entries; i++) {
        entryGet.mMsgIdx = i;
        JMSMesgEntry_c e = entryGet.getMesgEntry(header);
        const uint8_t* re = raw.entry(i);
        if (e.mDataOffs != rd32(re) || e.mMsgNo != rd16(re + 4) || e.mItemPrice != (s16)rd16(re + 6) ||
            e.mNextMsgNo != rd16(re + 8) || e.field_0x0a != rd16(re + 0x0A) ||
            memcmp(&e.mTextboxType, re + 0x0C, 0x0C) != 0) {
            if (bad++ < 5) {
                fail(raw.path, "getMesgEntry(%d): offset 0x%x number %u price %d next %u; the file "
                               "has 0x%x %u %d %u",
                     i, (unsigned)e.mDataOffs, (unsigned)e.mMsgNo, (int)e.mItemPrice,
                     (unsigned)e.mNextMsgNo, rd32(re), rd16(re + 4), (s16)rd16(re + 6), rd16(re + 8));
            }
        }
        // What the manifest digests: the entries as the game read them.
        r.idsDigest = fnvBE16(fnvBE32(r.idsDigest, e.mDataOffs), e.mMsgNo);
        if (e.mDataOffs == 0) {
            continue;
        }
        r.messages++;
        u16 number = e.mMsgNo;
        if (seen[number]) {
            r.duplicates++;
            continue;
        }
        seen[number] = 1;
        r.distinct++;

        if (fopMsgM_hyrule_language_check(number) != secondFile) {
            r.elsewhere++;
            continue;
        }
        fopMsgM_msgGet_c msgGet;
        mesg_header* h = msgGet.getMesgHeader(number);
        const char* text = h == header ? msgGet.getMessage(h) : nullptr;
        fopMsgM_itemMsgGet_c itemGet;
        mesg_header* h2 = itemGet.getMesgHeader(number);
        const char* text2 = h2 == header ? itemGet.getMessage(h2) : nullptr;
        if (h != header || text != raw.text(i) || msgGet.mResMsgNo != number || h2 != header ||
            text2 != raw.text(i) || itemGet.mResMsgNo != number) {
            if (bad++ < 5) {
                fail(raw.path, "message %u: msgGet header %p text %p (%u), itemMsgGet %p %p (%u); "
                               "the file is %p, text %p",
                     number, (void*)h, text, msgGet.mResMsgNo, (void*)h2, text2, itemGet.mResMsgNo,
                     (void*)header, raw.text(i));
            }
            continue;
        }
        if (!decodeText(raw, raw.text(i), playerName, want, sizeof(want))) {
            fail(raw.path, "message %u: malformed or longer than %zu bytes", number, sizeof(want));
            continue;
        }
        fopMsgM_messageGet(got, number);
        if (strcmp(got, want) != 0) {
            if (bad++ < 5) {
                fail(raw.path, "message %u: fopMsgM_messageGet gave %zu bytes, the text decodes to %zu",
                     number, strlen(got), strlen(want));
            }
            continue;
        }
        r.decoded++;
        r.decodedBytes += (uint32_t)strlen(got);
    }
    if (bad > 5) {
        fail(raw.path, "fopMsgM: %d entries or messages wrong in all", bad);
    }
}

// ---- archives, the colour table and fonts -----------------------------------------------------

JKRArchive* mountMem(const char* path) {
    JKRArchive* arc = JKRArchive::mount(path, JKRArchive::MOUNT_MEM, mDoExt_getArchiveHeap(),
                                        JKRArchive::DEFAULT_MOUNT_DIRECTION);
    if (arc == nullptr) {
        fail(path, "mount failed");
    }
    return arc;
}

void checkColors(JKRArchive* arc, const char* path, int fd) {
    const uint8_t* bmc = (const uint8_t*)JKRGetTypeResource('ROOT', "color.bmc", arc);
    if (bmc == nullptr || arc->getResSize(bmc) < 0x2C || memcmp(bmc, "MGCLbmc1", 8) != 0 ||
        rd32(bmc + 0x20) != tag('C', 'L', 'T', '1')) {
        fail(path, "no MGCLbmc1 file with a CLT1 block");
        return;
    }
    uint16_t n = rd16(bmc + 0x28);
    if (0x2C + 4u * n > arc->getResSize(bmc)) {
        fail(path, "%u colours do not fit", n);
        return;
    }
    uint64_t digest = kFnvBasis;
    int bad = 0;
    for (u16 i = 0; i < n; i++) {
        u32 color = fopMsgM_getColorTable(i);
        if (color != rd32(bmc + 0x2C + 4 * i) && bad++ < 3) {
            fail(path, "fopMsgM_getColorTable(%u) 0x%08x, the file has 0x%08x", i, color,
                 rd32(bmc + 0x2C + 4 * i));
        }
        digest = fnvBE32(digest, color);
    }
    writef(STDERR_FILENO, "[cos] msg-sweep: %s: %u colours, %d wrong\n", path, n, bad);
    if (fd >= 0) {
        writef(fd, "BMC %s block_count=%u CLT1=1\n", path, rd32(bmc + 0x0C));
        writef(fd, "CLT1 %s 0 entries=%u colors_fnv=%016llx\n", path, n,
               (unsigned long long)digest);
    }
}

void checkFont(JUTFont* font, JKRArchive* arc, const char* name, const char* path, int fd) {
    if (font == nullptr || !font->isValid()) {
        fail(path, "no valid font");
        return;
    }
    // USA builds both with mDoExt_initFontCommon's JUTResFont branch (a cache font needs param 0).
    JUTResFont* res = static_cast<JUTResFont*>(font);
    const uint8_t* bytes = (const uint8_t*)res->getResFont();
    // mDoExt_initFontCommon's lookup (the ruby font's archive is found by the global search).
    const uint8_t* file = (const uint8_t*)JKRArchive::getGlbResource('ROOT', name, nullptr);
    if (bytes == nullptr || bytes != file || arc->getResource('ROOT', name) != file) {
        fail(path, "the font's ResFONT %p is not the archive's %s (%p)", bytes, name, file);
        return;
    }
    sErrors += checkResFont("msg-sweep", path, *res, bytes, arc->getResSize(file), fd);
}

} // namespace

[[noreturn]] void smokeMsgSweep() {
    uint64_t start = elapsedMs();
    JKRArchive* msgArc = mountMem("/res/Msg/bmgres.arc");
    JKRArchive* msg2Arc = mountMem("/res/Msg/bmgresh.arc");
    JKRArchive* fontArc = mountMem("/res/Msg/fontres.arc");
    JKRArchive* rubyArc = mountMem("/res/Msg/rubyres.arc");
    if (msgArc == nullptr || msg2Arc == nullptr || fontArc == nullptr || rubyArc == nullptr) {
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    // As d_s_logo's phase_2 (the ruby archive is found by getGlbResource's search there too).
    dComIfGp_setMsgDtArchive(msgArc);
    dComIfGp_setMsgDt2Archive(msg2Arc);
    dComIfGp_setFontArchive(fontArc);

    // The player name fopMsgM_messageGet inserts; a fresh save has none.
    g_dComIfG_gameInfo.save.getPlayer().getPlayerInfo().setPlayerName("Link");
    const char* playerName = dComIfGs_getPlayerName();
    if (dComIfGs_getPalLanguage() != 0 || dComIfGs_getClearCount() != 0) {
        fail("save", "PAL language %u, clear count %u: the sweep expects a fresh save",
             dComIfGs_getPalLanguage(), dComIfGs_getClearCount());
    }

    struct File {
        JKRArchive* arc;
        const char* name;
        const char* path;
        bool kingdom;
    } files[] = {
        {msgArc, "zel_00.bmg", "/res/Msg/bmgres.arc:zel_00.bmg", false},
        {msg2Arc, "zel_01.bmg", "/res/Msg/bmgresh.arc:zel_01.bmg", true},
    };
    constexpr int kFiles = sizeof(files) / sizeof(files[0]);
    RawBmg raw[kFiles];
    bool ok[kFiles] = {};
    JMessage::TResourceContainer* container = new JMessage::TResourceContainer();
    JMessage::TParse* parse = new JMessage::TParse(container);
    for (int k = 0; k < kFiles; k++) {
        const uint8_t* res = (const uint8_t*)JKRGetResource('ROOT', files[k].name, files[k].arc);
        if (res == nullptr) {
            fail(files[k].path, "not in the archive");
            continue;
        }
        ok[k] = parseRawBmg(files[k].path, res, files[k].arc->getResSize(res), raw[k]);
        // dMesg_parse: parse(header, 0), whose result it ignores; the container must hold it.
        if (!parse->parse(res, 0)) {
            fail(files[k].path, "JMessage::TParse::parse failed");
        }
    }

    int fd = openRunFile("msg_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# msg-sweep (COS_SMOKE=msg-sweep): what the game's message code read, in "
                   "disc_manifest.py's names\n");
    }
    uint32_t processed = 0;
    uint32_t total = 0, decoded = 0;
    if (ok[0] && ok[1]) {
        checkJMessage(*container, raw, kFiles, processed);
    }
    for (int k = 0; k < kFiles; k++) {
        if (!ok[k]) {
            continue;
        }
        mesg_header* header = (mesg_header*)raw[k].base;
        FopResult r;
        checkFopMsgM(raw[k], header, files[k].kingdom, playerName, r);
        total += r.messages;
        decoded += r.decoded;
        // The JMessage resource is the game's reading of the header and INF1; the counts and the
        // digest are fopMsgM's reading of every entry.
        JMessage::TResource* res = container->Get_groupID(raw[k].group);
        if (fd >= 0 && res != nullptr) {
            writef(fd, "BMG %s block_count=%u INF1=1 DAT1=1\n", raw[k].path,
                   (unsigned)res->mHeader.get_blockNumber());
            writef(fd, "INF1 %s 0 entries=%u entry_size=%u group=%u messages=%u distinct_ids=%u "
                       "ids_fnv=%016llx\n",
                   raw[k].path, (unsigned)res->getMessageEntryNumber(),
                   (unsigned)res->getMessageEntrySize(), (unsigned)res->getGroupID(), r.messages,
                   r.distinct, (unsigned long long)r.idsDigest);
            writef(fd, "DAT1 %s 0 size=%u\n", raw[k].path,
                   (unsigned)JMessage::data::TParse_TBlock(res->mMessageData - 8).get_size());
        }
        writef(STDERR_FILENO,
               "[cos] msg-sweep: %s: group %u, %u entries, %u messages (%u numbers, %u repeated), "
               "%u decoded by fopMsgM (%u bytes), %u routed to the other file\n",
               raw[k].path, raw[k].group, raw[k].entries, r.messages, r.distinct, r.duplicates,
               r.decoded, r.decodedBytes, r.elsewhere);
    }
    checkColors(msgArc, "/res/Msg/bmgres.arc:color.bmc", fd);
    checkFont(mDoExt_getMesgFont(), fontArc, "rock_24_20_4i_usa.bfn",
              "/res/Msg/fontres.arc:rock_24_20_4i_usa.bfn", fd);
    checkFont(mDoExt_getRubyFont(), rubyArc, kRubyFont, kRubyFontPath, fd);
    if (fd >= 0) {
        close(fd);
    }
    writef(STDERR_FILENO,
           "[cos] msg-sweep: %u messages, %u decoded by fopMsgM, %u run through JMessage's "
           "processor, %llu ms; %d error(s)%s\n",
           total, decoded, processed, (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in msg_sweep.txt)" : "");
    bool pass = sErrors == 0 && ok[0] && ok[1] && processed == total && decoded > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
