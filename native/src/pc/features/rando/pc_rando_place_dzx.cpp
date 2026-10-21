// pc_rando_place_dzx.cpp - the placement written into stage.dzs / room.dzr as they load
// (docs/RANDOMIZER_PLAN.md R2.1, the TRES part; see pc_rando_place_dzx.h). wwrando reference:
// randomizers/items.py change_item / change_chest_item (chest.item_id, the high byte of angle z).
// Pure: no game code (cos_rando_core).
#include "pc_rando_place_dzx.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_lockeys.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

using pcr::DzxField;
using pcr::KeyKind;
using pcr::LocKey;

constexpr int kMaxKeys = 1024;
PcRandoPlaceStatus sStatus[kMaxKeys];
PcRandoPlaceStats sStats;

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

// The file a key's archive holds its chunks in: Stage.arc -> stage.dzs, Room<N>.arc -> room.dzr.
bool keyFileMatches(const LocKey& key, const PcRandoResFile* file) {
    if (key.stage == nullptr || key.arc == nullptr || strcmp(key.stage, file->stage) != 0 ||
        strcmp(key.arc, file->arc) != 0) {
        return false;
    }
    if (strcmp(key.arc, "Stage") == 0) {
        return file->type == PCR_RES_DZS && strcmp(file->name, "stage.dzs") == 0;
    }
    return file->type == PCR_RES_DZR && strcmp(file->name, "room.dzr") == 0;
}

// R2.1: the chests. R2.2 (ACTR) and R2.3 (SCOB) add their fields here and in placeEntry.
bool handled(DzxField f) {
    return f == DzxField::TboxItem;
}

// Writes the placed item into one entry whose fingerprint held. Returns the value written.
uint8_t placeEntry(const LocKey& key, uint8_t* entry, uint8_t item) {
    switch (key.field) {
    case DzxField::TboxItem:
        // items.py change_chest_item: chest.item_id = item_id (angle z & 0xFF00). The trap flag and
        // chest_type_matches_contents are R6.5's.
        pcr::dzx_key_write(key, entry, item);
        return item;
    default:
        return (uint8_t)pcr::dzx_key_read(key, entry);
    }
}

} // namespace

extern "C" {

int pc_rando_place_dzx_handles(int field) {
    return handled((DzxField)field) ? 1 : 0;
}

void pc_rando_place_dzx_patch(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->stage == nullptr || file->stage[0] == '\0' ||
        (file->type != PCR_RES_DZS && file->type != PCR_RES_DZR)) {
        return;
    }
    const pcr::Span<LocKey> keys = pcr::location_keys();
    PcRandoDzx dzx;
    bool opened = false;
    int nWritten = 0, nSame = 0, nBad = 0;
    for (size_t i = 0; i < keys.size && i < (size_t)kMaxKeys; i++) {
        const LocKey& key = keys[i];
        if (key.kind != KeyKind::Dzx || !handled(key.field) || !keyFileMatches(key, file)) {
            continue;
        }
        if (!opened) {
            const int r = pc_rando_dzx_open(&dzx, file->data, file->size);
            if (r != PCR_DZX_OK) {
                logf("[cos] rando place: %s/%s/%s: not a dzx (%s); left vanilla\n", file->stage, file->arc,
                     file->name, pc_rando_dzx_result_name(r));
                return;
            }
            opened = true;
            sStats.files++;
        }
        PcRandoPlaceStatus& st = sStatus[i];
        st.patches++;
        const uint8_t item = pc_rando_loc_item(key.loc, key.vanilla);
        st.item = item;
        uint8_t* entry = pc_rando_dzx_entry(&dzx, key.tag, key.index);
        if (entry == nullptr || (uint32_t)(key.word_off + key.width) > pc_rando_dzx_entry_size(key.tag)) {
            st.result = PCR_PLACE_MISSING;
            st.entry_off = 0;
            sStats.missing++;
            nBad++;
            logf("[cos] rando place: %s/%s/%s: %s[%u] missing; %s left vanilla\n", file->stage, file->arc,
                 file->name, key.tag, key.index, pcr::location_info(key.loc)->enum_name);
            continue;
        }
        st.entry_off = (uint32_t)(entry - file->data);
        st.found = (uint8_t)pcr::dzx_key_read(key, entry);
        if (!pcr::dzx_key_fingerprint_ok(key, entry)) {
            // D9: replaced content (HD issue #42 style): leave the location vanilla.
            st.result = PCR_PLACE_MISMATCH;
            sStats.mismatch++;
            nBad++;
            logf("[cos] rando place: %s/%s/%s: %s[%u] fingerprint mismatch (\"%.8s\" 0x%02X, expected "
                 "\"%.8s\" 0x%02X); %s left vanilla\n",
                 file->stage, file->arc, file->name, key.tag, key.index, (const char*)entry, st.found,
                 (const char*)key.fp_name, key.disc_byte, pcr::location_info(key.loc)->enum_name);
            continue;
        }
        if (item == st.found) {
            st.result = PCR_PLACE_SAME;
            sStats.same++;
            nSame++;
            continue;
        }
        st.item = placeEntry(key, entry, item);
        st.result = PCR_PLACE_WRITTEN;
        sStats.written++;
        nWritten++;
    }
    if (opened) {
        logf("[cos] rando place: %s/%s/%s: %d written, %d unchanged, %d left vanilla\n", file->stage,
             file->arc, file->name, nWritten, nSame, nBad);
    }
}

void pc_rando_place_dzx_init(void) {
    pc_rando_register_dzx_patcher(pc_rando_place_dzx_patch);
}

const PcRandoPlaceStatus* pc_rando_place_dzx_status(int key_index) {
    if (key_index < 0 || key_index >= kMaxKeys || (size_t)key_index >= pcr::location_keys().size) {
        return nullptr;
    }
    return &sStatus[key_index];
}

void pc_rando_place_dzx_reset(void) {
    memset(sStatus, 0, sizeof sStatus);
    memset(&sStats, 0, sizeof sStats);
}

void pc_rando_place_dzx_stats(PcRandoPlaceStats* out) {
    if (out != nullptr) {
        *out = sStats;
    }
}

} // extern "C"
