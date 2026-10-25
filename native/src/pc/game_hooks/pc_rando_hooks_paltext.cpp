// The English BMG on the European disc (docs/RANDOMIZER_PLAN.md step R7.6c; pure lookup in
// features/rando/pc_rando_bmg.h). Declared in native/include/pc/pc_rando_hooks.h (section R7.6c).
//
// The European game loads the BMG of its language (/res/Msg/data<language>/bmgres.arc: 0 English,
// 1 German, 2 French, 3 Spanish, 4 Italian). The five BMGs have the same 4327 message numbers and,
// apart from the language menu's own entries, the same INF1 fields; the English one has USA's texts
// but 7 (none of them a message wwrando edits). wwrando's texts stay English in every language (the
// step's language rule), so the texts R3.21 builds from vanilla text need the English BMG in German,
// French, Spanish and Italian too: this file reads /res/Msg/data0/bmgres.arc's zel_00.bmg from the disc
// into host memory the first time one is asked for in an activation and drops it when pc_rando() turns
// off. On the USA build and in English the loaded BMG is the English one and nothing is read.
//
// No hunk in game/.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_bmg.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_start.h"

#include "d/d_com_inf_game.h"

#include "cos_sdk/host_alloc.h"

#include <dolphin/dvd.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kPath = "/res/Msg/data0/bmgres.arc";
constexpr const char* kFile = "zel_00.bmg";

// 0 not read in this activation, 1 read (sBmg), -1 the read failed (not tried again until the next
// activation).
int sState = 0;
cos_sdk::HostVector<uint8_t> sBmg;
bool sListening = false;

void release() {
    sState = 0;
    cos_sdk::HostVector<uint8_t>().swap(sBmg);
}

void onRando(int active) {
    if (!active) {
        release();
    }
}

// The expanded copy of a Yaz0 block, or the bytes themselves.
bool expand(const uint8_t* data, size_t size, cos_sdk::HostVector<uint8_t>* out) {
    if (const uint32_t n = pc_rando_start_yaz0_size(data, size); n != 0) {
        out->resize(n);
        return pc_rando_start_yaz0_expand(data, size, out->data(), out->size()) != 0;
    }
    out->assign(data, data + size);
    return true;
}

bool readEnglishBmg() {
    DVDFileInfo info;
    if (!DVDOpen(kPath, &info)) {
        return false;
    }
    cos_sdk::HostVector<uint8_t> file(info.length);
    const s32 got = DVDReadPrio(&info, file.data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    if (got != (s32)info.length) {
        return false;
    }
    cos_sdk::HostVector<uint8_t> arc;
    if (!expand(file.data(), file.size(), &arc)) {
        return false;
    }
    cos_sdk::HostVector<uint8_t>().swap(file);
    const uint8_t* bmg = nullptr;
    uint32_t size = 0;
    if (!pc_rando_start_rarc_find(arc.data(), arc.size(), kFile, &bmg, &size)) {
        return false;
    }
    return expand(bmg, size, &sBmg);
}

const uint8_t* englishBmg(size_t* size) {
    if (!sListening) {
        sListening = pc_rando_add_listener(onRando) != 0;
    }
    if (sState == 0) {
        sState = readEnglishBmg() ? 1 : -1;
        if (sState != 1) {
            release();
            sState = -1;
        }
        std::fprintf(stderr, "[cos] rando: EUR text: %s %s %s (%zu bytes)\n", kPath, kFile,
                     sState == 1 ? "read for the English base texts" : "cannot be read, the base texts stay the loaded BMG's",
                     sBmg.size());
    }
    *size = sBmg.size();
    return sState == 1 ? sBmg.data() : nullptr;
}

} // namespace

extern "C" int pc_rando_eur_text_language(void) {
#if defined(VERSION) && VERSION == VERSION_PAL
    return dComIfGs_getPalLanguage();
#else
    return 0;
#endif
}

extern "C" int pc_rando_en_bmg_message(uint16_t msg_no, const uint8_t** text, size_t* text_max, uint8_t entry[0x18]) {
    if (pc_rando_eur_text_language() == 0) {
        return 0; // USA, or the European game in English: the loaded BMG is the English one
    }
    size_t size = 0;
    const uint8_t* bmg = englishBmg(&size);
    pcr::BmgMessage m;
    if (bmg == nullptr || !pcr::bmgFind(bmg, size, msg_no, &m)) {
        return 0;
    }
    *text = m.text;
    *text_max = m.text_max;
    std::memcpy(entry, m.entry, PCR_MSG_ENTRY_SIZE);
    return 1;
}
