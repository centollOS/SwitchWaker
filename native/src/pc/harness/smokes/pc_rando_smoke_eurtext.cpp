// COS_SMOKE=rando COS_RANDO_SMOKE=eurtext (docs/RANDOMIZER_PLAN.md step R7.6c): the randomizer's texts
// in each language of the European disc (and on USA, as the reference). A debug boot with a seed:
//   COS_BUILD_DIR=build/native-mac-eur COS_GAME_LANGUAGE=de run.sh rando --disc GZLP01.iso --stage sea:44 \
//     --env COS_RANDO_SMOKE=eurtext --env COS_RANDO_PLACEMENT=$PWD/native/check/rando/fixtures/R5_3_Heavy5.json
//     [--env COS_RANDO_EURTEXT_DUMP=<file>]
// Once the play scene has run 10 frames:
//   1. the game language is COS_GAME_LANGUAGE's (en 0, de 1, fr 2, es 3, it 4; USA: 0);
//   2. the seed's texts are built again (R3.21's post texts, then R5.3's hints, as at a stage load) and
//      the overlay holds them (R3.4's 22 dungeon-item messages, the post texts, the hints);
//   3. every overlay text is English: no character outside ASCII (outside control tags), so no
//      vanilla text of the game language is mixed in;
//   4. every overlay line fits its box: measured with the loaded message font (control tags without
//      width, as wwrando's wrap), no line is wider than the box type's line (gclib's
//      TEXT_BOX_TYPE_TO_MAX_LINE_LENGTH: 419 for item get and song boxes, 503 else), except the R3.21
//      texts wwrando itself writes without a wrap (chart numbers, shop and auction names), only logged
//      (they are as wide on USA; step 7's dumps show they are the same bytes);
//   5. R3.4's pre-wrapped dungeon-item texts equal a wrap at run time with the loaded font (the
//      European font is the USA one, byte for byte; this proves the line breaks hold);
//   6. one message is shown through d_msg (COS_RANDO_EURTEXT_SHOW=<number>, default 120: the DRC small
//      key's item-get box; the regress lines also show Beedle's refusal 3957 and KoRL's 3443; parchment and
//      wood boxes such as the letter 3325 open in d_msg2 and are not shown here): the process reads the
//      overlay's bytes and the line widths the game itself measures
//      (fopMsgM_msgDataProc_c) stay within its line width;
//   7. COS_RANDO_EURTEXT_DUMP=<file>: every overlay record (number, entry, bytes) as hex, one per line,
//      for comparing the languages with each other and with USA (all must be equal).
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_dungeon_items.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_posttext.h"
#include "features/rando/pc_rando_wrap.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_msg.h"
#include "f_op/f_op_msg.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "eurtext";
constexpr unsigned int kShowFrames = 90;

// The message shown through d_msg (step 6): COS_RANDO_EURTEXT_SHOW, default 120 (101 + 0x13, the DRC
// small key's item-get box). One per run: a box closed by deleting its process leaves the message
// manager busy, so a second messageSet would not open.
u16 showNo() {
    const char* v = std::getenv("COS_RANDO_EURTEXT_SHOW");
    return v != nullptr && v[0] != '\0' ? (u16)std::strtoul(v, nullptr, 10) : (u16)(101 + 0x13);
}

int sStep = -1; // -1 static checks not run; 0 showing showNo()
unsigned int sShowEnd = 0;
fpc_ProcID sMsgId = fpcM_ERROR_PROCESS_ID_e;
bool sSeen = false;
bool sFromOverlay = false;
int sMaxWidth = 0;
int sLineWidth = 0;

int expectedLanguage() {
    const char* l = std::getenv("COS_GAME_LANGUAGE");
#if defined(VERSION) && VERSION == VERSION_PAL
    static const char* const kCodes[] = {"en", "de", "fr", "es", "it"};
    for (int i = 0; l != nullptr && i < 5; i++) {
        if (std::strcmp(l, kCodes[i]) == 0) {
            return i;
        }
    }
    return l == nullptr || l[0] == '\0' || std::strcmp(l, "auto") == 0 ? -1 : -2;
#else
    (void)l;
    return 0;
#endif
}

// R3.21 texts wwrando writes without a word wrap (tweaks.py: the chart numbers put into the item-get
// texts by a string replace, the shop descriptions and auction names by a format string, the sail's
// name): a line of these may pass the box on USA too, so step 4 only logs them. The letter (3325),
// KoRL (3443) and the Battlesquid texts (7520, 7521) are wrapped and checked.
bool unwrappedByWwrando(uint16_t no) {
    if (no == 3325 || no == 3443 || no == 7520 || no == 7521) {
        return false;
    }
    for (uint16_t n : pcr::postTextMessageNumbers()) {
        if (n == no) {
            return true;
        }
    }
    return false;
}

// The text box type the game reads for msg_no (the overlay's entry, or the BMG's).
bool boxType(u16 msgNo, u8* type) {
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msgNo);
    if (head == nullptr || get.getMessage(head) == nullptr || get.getMesgNumber() != msgNo) {
        return false;
    }
    *type = get.getMesgEntry(head).mTextboxType;
    return true;
}

// Walks encoded BMG bytes: the widest line (tags without width) and whether any plain character is
// outside ASCII.
void measure(const uint8_t* p, size_t len, const pcr::GlyphWidths& w, int* widest, bool* nonAscii) {
    int line = 0;
    *widest = 0;
    *nonAscii = false;
    for (size_t i = 0; i < len && p[i] != 0;) {
        if (p[i] == 0x1A) {
            const size_t n = i + 1 < len ? p[i + 1] : 1;
            i += n < 2 ? 2 : n;
            continue;
        }
        if (p[i] == '\n') {
            line = 0;
        } else {
            line += w.code[p[i]];
            *nonAscii = *nonAscii || p[i] >= 0x80;
            if (line > *widest) {
                *widest = line;
            }
        }
        i++;
    }
}

std::string toText(const uint8_t* p, size_t len) {
    std::vector<char> buf(len * 4 + 64);
    const size_t n = pc_rando_msg_decode(p, len, buf.data(), buf.size());
    return std::string(buf.data(), n < buf.size() ? n : buf.size() - 1);
}

void hex(FILE* f, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        std::fprintf(f, "%02x", p[i]);
    }
}

void* isMsg(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_MSG_e ? proc : nullptr;
}

void staticChecks() {
    // 1.
    const int lang = pc_rando_eur_text_language();
    const int want = expectedLanguage();
    writef(STDERR_FILENO, "[cos] rando-smoke eurtext: game language %d (COS_GAME_LANGUAGE %s)\n", lang,
           std::getenv("COS_GAME_LANGUAGE") != nullptr ? std::getenv("COS_GAME_LANGUAGE") : "unset");
    randoSmokeCheck(kName, "the game language is COS_GAME_LANGUAGE's", want == -1 || lang == want);

    // 2.
    if (!randoSmokeCheck(kName, "a seed is active (COS_RANDO_PLACEMENT)", pc_rando() != 0 && pc_rando_seed() != nullptr)) {
        return;
    }
    const pcr::GlyphWidths* w = pc_rando_font_widths();
    if (!randoSmokeCheck(kName, "message font widths", w != nullptr)) {
        return;
    }
    const int posts = pc_rando_posttext_build();
    const int hints = pc_rando_hints_build();
    writef(STDERR_FILENO, "[cos] rando-smoke eurtext: built %d post texts, %d hint messages; %d records\n", posts,
           hints, pc_rando_msg_count());
    int ditems = 0;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        ditems += pc_rando_msg_find((uint16_t)(101 + pc_rando_dungeon_item(i)->id)) != nullptr;
    }
    randoSmokeCheck(kName, "the 22 dungeon-item messages are in the overlay", ditems == PCR_DITEM_COUNT);
    randoSmokeCheck(kName, "post texts and hints are in the overlay", posts > 0 && hints > 0);

    // 3. and 4.
    FILE* dump = nullptr;
    if (const char* path = std::getenv("COS_RANDO_EURTEXT_DUMP"); path != nullptr && path[0] != '\0') {
        dump = std::fopen(path, "w");
        randoSmokeCheck(kName, "dump file opened", dump != nullptr);
    }
    int texts = 0, english = 0, fit = 0, widest = 0, wwrandoWide = 0;
    for (uint32_t no = 0; no <= 0xFFFF; no++) {
        const PcRandoMsg* r = pc_rando_msg_find((uint16_t)no);
        if (r == nullptr) {
            continue;
        }
        if (dump != nullptr) {
            std::fprintf(dump, "%u ", no);
            if (r->has_entry) {
                uint8_t e[PCR_MSG_ENTRY_SIZE];
                pc_rando_msg_entry_pack(&r->entry, (uint16_t)no, 0, e);
                hex(dump, e, sizeof e);
            } else {
                std::fprintf(dump, "-");
            }
            std::fprintf(dump, " ");
            if (r->bytes != nullptr) {
                hex(dump, r->bytes, r->len);
            } else {
                std::fprintf(dump, "-");
            }
            std::fprintf(dump, "\n");
        }
        if (r->bytes == nullptr) {
            continue;
        }
        texts++;
        int wide = 0;
        bool nonAscii = false;
        measure(r->bytes, r->len, *w, &wide, &nonAscii);
        u8 type = 0;
        const bool haveType = boxType((u16)no, &type);
        const int max = pcr::textBoxMaxLineLength(type);
        english += !nonAscii;
        const bool unwrapped = unwrappedByWwrando((uint16_t)no);
        const bool fits = haveType && (wide <= max || unwrapped);
        fit += fits;
        wwrandoWide += haveType && wide > max && unwrapped;
        widest = wide > widest ? wide : widest;
        if (nonAscii || !fits || wide > max) {
            writef(STDERR_FILENO, "[cos] rando-smoke eurtext: message %u:%s%s (box %d, widest line %d of %d): \"%.200s\"\n",
                   no, nonAscii ? " not English" : "", fits ? (wide > max ? " wider than the box as wwrando writes it (no wrap)" : "") : " too wide", haveType ? type : -1, wide, max,
                   toText(r->bytes, r->len).c_str());
        }
    }
    if (dump != nullptr) {
        std::fclose(dump);
    }
    writef(STDERR_FILENO,
           "[cos] rando-smoke eurtext: %d texts: %d English, %d fit their box (%d of them unwrapped by wwrando and wider; widest "
           "line %d)\n",
           texts, english, fit, wwrandoWide, widest);
    randoSmokeCheck(kName, "every overlay text is English", texts > 0 && english == texts);
    randoSmokeCheck(kName, "every overlay line fits its box", texts > 0 && fit == texts);

    // 5.
    int same = 0;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* d = pc_rando_dungeon_item(i);
        std::string flat = d->text;
        for (char& c : flat) {
            c = c == '\n' ? ' ' : c;
        }
        const std::string wrapped = pcr::wordWrapMessage(*w, 9, flat);
        if (wrapped == d->text) {
            same++;
        } else {
            writef(STDERR_FILENO, "[cos] rando-smoke eurtext: %s wraps as \"%s\"\n", d->name, wrapped.c_str());
        }
    }
    randoSmokeCheck(kName, "R3.4's pre-wrapped texts = a wrap with the loaded font", same == PCR_DITEM_COUNT);
    sStep = 0;
}

void startShow(unsigned int frames) {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    layer_class* savedLayer = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(static_cast<base_process_class*>((fopAc_ac_c*)player)->mLyTg.mpLayer);
    // dMsg_initProc opens the box once the camera holds a talk or the demo message flag is on (no NPC
    // here: the flag)
    fopMsgM_demoMsgFlagOn();
    sMsgId = fopMsgM_messageSet(showNo(), &player->eyePos);
    fpcLy_SetCurrentLayer(savedLayer);
    sSeen = false;
    sFromOverlay = false;
    sMaxWidth = 0;
    sLineWidth = 0;
    sShowEnd = frames + kShowFrames;
}

} // namespace

void randoSmokeEurtext(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sStep < 0) {
        staticChecks();
        if (sStep < 0) {
            randoSmokeEnd(kName);
        }
        startShow(frames);
        return;
    }
    if (sub_msg_class* m = (sub_msg_class*)fpcM_Search(isMsg, nullptr); m != nullptr && m->mMsgID == showNo()) {
        sSeen = true;
        sFromOverlay = sFromOverlay || m->mpMesgStr == pc_rando_msg_hook_text(showNo());
        const fopMsgM_msgDataProc_c& d = m->mMsgDataProc;
        if (d.lineWidth > 0 && sLineWidth == 0) {
            writef(STDERR_FILENO, "[cos] rando-smoke eurtext: %u: d_msg line width %d at frame %u\n",
                   (unsigned)showNo(), d.lineWidth, frames);
        }
        if (d.lineWidth > 0) {
            sLineWidth = d.lineWidth;
            for (int i = 0; i < 4; i++) {
                sMaxWidth = d.field_0xD8[i] > sMaxWidth ? d.field_0xD8[i] : sMaxWidth;
            }
        }
    }
    if (frames < sShowEnd) {
        return;
    }
    const bool ok = sMsgId != fpcM_ERROR_PROCESS_ID_e && sSeen && sFromOverlay && sLineWidth > 0 && sMaxWidth <= sLineWidth;
    writef(STDERR_FILENO, "[cos] rando-smoke eurtext: shown %u: process %s, overlay text %s, widest line %d of %d\n",
           (unsigned)showNo(), sSeen ? "yes" : "no", sFromOverlay ? "yes" : "no", sMaxWidth, sLineWidth);
    fopMsgM_demoMsgFlagOff();
    randoSmokeCheck(kName, "the message shown by d_msg from the overlay, lines within the line width", ok);
    randoSmokeEnd(kName);
}

} // namespace pc
