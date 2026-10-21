// pc_rando_place_scob.cpp - the SCOB item locations and the Needle Rock salvage flags, patched into
// room.dzr while it loads (docs/RANDOMIZER_PLAN.md step R2.3; pc_rando_place_scob.h).
// wwrando: randomizers/items.py change_item (Salvage / SwSlvg item_id = params & 0x00000FF0, TagKb
// item_id = params & 0x000000FF, data/actor_parameters.txt) and tweaks.py
// fix_needle_rock_island_salvage_flags. Pure: no game headers (cos_rando_core).
#include "pc_rando_place_scob.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_lockeys.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kMaxKeys = 1024;

uint8_t sResult[kMaxKeys];
uint8_t sItem[kMaxKeys];
int sNeedleRock = -1;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

bool isScobKey(const pcr::LocKey& k) {
    return k.kind == pcr::KeyKind::Dzx &&
           (k.field == pcr::DzxField::SalvageItem || k.field == pcr::DzxField::TagKbItem);
}

bool sameStr(const char* a, const char* b) {
    return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

// wwrando's DataTables.actor_name_to_class_name[name] == "d_a_salvage" (data/actor_info.txt).
bool isSalvageName(const uint8_t* entry) {
    static const char* const kNames[] = {"Salvage", "SwSlvg", "Salvag2", "SalvagN", "SalvagE", "SalvFM"};
    char name[9];
    memcpy(name, entry, 8);
    name[8] = '\0';
    for (const char* n : kNames) {
        if (strcmp(name, n) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

extern "C" {

void pc_rando_place_scob_patch(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->type != PCR_RES_DZR) {
        return;
    }
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    PcRandoDzx dzx;
    bool opened = false;
    int placed = 0, changed = 0, bad = 0;
    for (size_t i = 0; i < keys.size && i < (size_t)kMaxKeys; i++) {
        const pcr::LocKey& k = keys[i];
        if (!isScobKey(k) || !sameStr(k.stage, file->stage) || !sameStr(k.arc, file->arc)) {
            continue;
        }
        if (!opened) {
            if (pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK) {
                logf("[cos] rando: scob %s/%s: room.dzr does not parse; SCOB locations left vanilla\n",
                     file->stage, file->arc);
                return;
            }
            opened = true;
        }
        sItem[i] = 0xFF;
        uint8_t* e = pc_rando_dzx_entry(&dzx, k.tag, k.index);
        if (e == nullptr) {
            sResult[i] = PCR_SCOB_MISSING;
            bad++;
            logf("[cos] rando: scob %s/%s %s[%u]: no such entry; location %d left vanilla\n", file->stage,
                 file->arc, k.tag, (unsigned)k.index, (int)k.loc);
            continue;
        }
        if (!pcr::dzx_key_fingerprint_ok(k, e)) {
            sResult[i] = PCR_SCOB_MISMATCH;
            bad++;
            logf("[cos] rando: scob %s/%s %s[%u]: fingerprint mismatch (%.8s item 0x%02X, expected %.8s "
                 "0x%02X); location %d left vanilla\n",
                 file->stage, file->arc, k.tag, (unsigned)k.index, (const char*)e,
                 (unsigned)pcr::dzx_key_read(k, e), (const char*)k.fp_name, (unsigned)k.disc_byte, (int)k.loc);
            continue;
        }
        const uint8_t item = pc_rando_loc_item(k.loc, k.vanilla);
        pcr::dzx_key_write(k, e, item);
        sResult[i] = PCR_SCOB_PLACED;
        sItem[i] = item;
        placed++;
        if (item != k.disc_byte) {
            changed++;
        }
    }
    if (opened) {
        logf("[cos] rando: scob %s/%s: %d placed (%d changed), %d left vanilla\n", file->stage, file->arc,
             placed, changed, bad);
    }
}

// tweaks.py fix_needle_rock_island_salvage_flags: in sea/Room29, the d_a_salvage SCOBs (every SCOB
// chunk, in file order, as DZx.entries_by_type(SCOB)) whose salvage_type (params >> 28) is 2, 3 or 4
// and whose salvage_flag ((params >> 20) & 0xFF) is 0 or 1: the first gets flag 8, the second flag 9
// (both unused in vanilla). On the USA disc these are SCOB[0] and SCOB[1] (`Salvag2`, the light
// rings).
void pc_rando_place_scob_needle_rock_patch(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->type != PCR_RES_DZR || !sameStr(file->stage, "sea") ||
        !sameStr(file->arc, "Room29")) {
        return;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK) {
        sNeedleRock = -2;
        return;
    }
    uint8_t* found[2] = {nullptr, nullptr};
    int nFound = 0;
    for (uint32_t c = 0; c < dzx.chunkCount && nFound < 2; c++) {
        const uint8_t* row = dzx.data + 4 + c * 12;
        char tag[5];
        memcpy(tag, row, 4);
        tag[4] = '\0';
        if (strncmp(tag, "SCO", 3) != 0) {
            continue;
        }
        const uint32_t count = pc_rando_dzx_count(&dzx, tag);
        for (uint32_t i = 0; i < count && nFound < 2; i++) {
            uint8_t* e = pc_rando_dzx_entry(&dzx, tag, i);
            if (e == nullptr || !isSalvageName(e)) {
                continue;
            }
            const uint32_t params = pc_rando_dzx_be32(e + 0x08);
            const uint32_t type = params >> 28;
            const uint32_t flag = (params >> 20) & 0xFF;
            if (type >= 2 && type <= 4 && flag <= 1) {
                found[nFound++] = e;
            }
        }
    }
    if (nFound < 2) {
        sNeedleRock = -2;
        logf("[cos] rando: scob sea/Room29: %d Needle Rock salvage flag candidate(s), expected 2; left vanilla\n",
             nFound);
        return;
    }
    static const uint32_t kNewFlags[2] = {8, 9};
    for (int i = 0; i < 2; i++) {
        uint32_t params = pc_rando_dzx_be32(found[i] + 0x08);
        params = (params & ~0x0FF00000u) | (kNewFlags[i] << 20);
        found[i][0x08] = (uint8_t)(params >> 24);
        found[i][0x09] = (uint8_t)(params >> 16);
        found[i][0x0A] = (uint8_t)(params >> 8);
        found[i][0x0B] = (uint8_t)params;
    }
    sNeedleRock = 2;
    logf("[cos] rando: scob sea/Room29: Needle Rock salvage flags 8 and 9 set\n");
}

void pc_rando_place_scob_init(void) {
    pc_rando_register_dzx_patcher(pc_rando_place_scob_patch);
    pc_rando_register_dzx_patcher(pc_rando_place_scob_needle_rock_patch);
}

int pc_rando_place_scob_result(int keyIndex) {
    return keyIndex >= 0 && keyIndex < kMaxKeys ? sResult[keyIndex] : PCR_SCOB_NOT_SEEN;
}

uint8_t pc_rando_place_scob_item(int keyIndex) {
    return keyIndex >= 0 && keyIndex < kMaxKeys && sResult[keyIndex] == PCR_SCOB_PLACED ? sItem[keyIndex] : 0xFF;
}

int pc_rando_place_scob_key_count(void) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    int n = 0;
    for (size_t i = 0; i < keys.size; i++) {
        n += isScobKey(keys[i]) ? 1 : 0;
    }
    return n;
}

int pc_rando_place_scob_needle_rock(void) {
    return sNeedleRock;
}

void pc_rando_place_scob_reset(void) {
    memset(sResult, 0, sizeof sResult);
    memset(sItem, 0xFF, sizeof sItem);
    sNeedleRock = -1;
}

} // extern "C"
