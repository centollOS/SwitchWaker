// pc_rando_place_event.cpp - the 11 event item locations, patched into a stage's event_list.dat while
// it loads (docs/RANDOMIZER_PLAN.md step R2.4; pc_rando_place_event.h).
// wwrando: randomizers/items.py change_item / change_event_item. Pure: no game headers
// (cos_rando_core).
#include "pc_rando_place_event.h"

#include "pc/pc_rando.h"
#include "pc_rando_event.h"
#include "pc_rando_lockeys.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kMaxKeys = 1024;

uint8_t sResult[kMaxKeys];
uint8_t sError[kMaxKeys];
uint8_t sItem[kMaxKeys];
int16_t sAction[kMaxKeys];

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

bool sameStr(const char* a, const char* b) {
    return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

} // namespace

extern "C" {

void pc_rando_place_event_patch(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->type != PCR_RES_DAT || !sameStr(file->name, "event_list.dat")) {
        return;
    }
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    PcRandoEvl evl;
    bool opened = false;
    int placed = 0, changed = 0, bad = 0;
    for (size_t i = 0; i < keys.size && i < (size_t)kMaxKeys; i++) {
        const pcr::LocKey& k = keys[i];
        if (k.kind != pcr::KeyKind::Event || !sameStr(k.stage, file->stage) || !sameStr(k.arc, file->arc)) {
            continue;
        }
        if (!opened) {
            const int r = pc_rando_evl_open(&evl, file->data, file->size);
            if (r != PCR_EVL_OK) {
                logf("[cos] rando: event %s/%s: event_list.dat does not open (%s); event locations left vanilla\n",
                     file->stage, file->arc, pc_rando_evl_result_name(r));
                return;
            }
            opened = true;
        }
        const uint8_t item = pc_rando_loc_item(k.loc, k.vanilla);
        int action = -1;
        const int r = pc_rando_evl_change_item(&evl, k.event, k.actor, k.action, item, k.action_name, k.prm0,
                                               &action, nullptr);
        if (r != PCR_EVL_OK) {
            sResult[i] = PCR_EVLOC_FAILED;
            sError[i] = (uint8_t)r;
            sItem[i] = 0xFF;
            sAction[i] = -1;
            bad++;
            logf("[cos] rando: event %s/%s %s actor %u action %u (%s, prm0 %d): %s; location %d left vanilla\n",
                 file->stage, file->arc, k.event, (unsigned)k.actor, (unsigned)k.action, k.action_name,
                 (int)k.prm0, pc_rando_evl_result_name(r), (int)k.loc);
            continue;
        }
        if (action != (int)k.gaction) {
            // The fingerprint held, so this is the location's action; only the index moved (an
            // earlier patcher relinked the chain). Logged for the record.
            logf("[cos] rando: event %s/%s %s: action at global index %d (key %u)\n", file->stage, file->arc,
                 k.event, action, (unsigned)k.gaction);
        }
        sResult[i] = PCR_EVLOC_PLACED;
        sError[i] = PCR_EVL_OK;
        sItem[i] = item;
        sAction[i] = (int16_t)action;
        placed++;
        if (item != k.vanilla) {
            changed++;
        }
    }
    if (opened) {
        logf("[cos] rando: event %s/%s: %d placed (%d changed), %d left vanilla\n", file->stage, file->arc, placed,
             changed, bad);
    }
}

void pc_rando_place_event_init(void) {
    pc_rando_register_res_patcher(pc_rando_place_event_patch, PCR_RES_DAT);
}

int pc_rando_place_event_result(int keyIndex) {
    return keyIndex >= 0 && keyIndex < kMaxKeys ? sResult[keyIndex] : PCR_EVLOC_NOT_SEEN;
}

int pc_rando_place_event_error(int keyIndex) {
    return keyIndex >= 0 && keyIndex < kMaxKeys && sResult[keyIndex] == PCR_EVLOC_FAILED ? sError[keyIndex]
                                                                                        : PCR_EVL_OK;
}

uint8_t pc_rando_place_event_item(int keyIndex) {
    return keyIndex >= 0 && keyIndex < kMaxKeys && sResult[keyIndex] == PCR_EVLOC_PLACED ? sItem[keyIndex] : 0xFF;
}

int pc_rando_place_event_action(int keyIndex) {
    return keyIndex >= 0 && keyIndex < kMaxKeys && sResult[keyIndex] == PCR_EVLOC_PLACED ? sAction[keyIndex] : -1;
}

int pc_rando_place_event_key_count(void) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    int n = 0;
    for (size_t i = 0; i < keys.size; i++) {
        n += keys[i].kind == pcr::KeyKind::Event ? 1 : 0;
    }
    return n;
}

void pc_rando_place_event_reset(void) {
    memset(sResult, 0, sizeof sResult);
    memset(sError, 0, sizeof sError);
    memset(sItem, 0xFF, sizeof sItem);
    memset(sAction, 0xFF, sizeof sAction);
}

} // extern "C"
