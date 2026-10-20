// Item dispatch overrides and item/shop table snapshots (pc_rando_items.h, docs/RANDOMIZER_PLAN.md
// step R0.8). No game headers or game symbols here: cos_rando_core links this file into the unit tests.
// The game side (the dispatch hooks and the binding of the real tables) is
// native/src/pc/game_hooks/pc_rando_hooks_items.cpp.
#include "pc_rando_items.h"

#include "pc/pc_rando.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

PcRandoItemFunc sItemFn[0x100];
PcRandoItemCheckFunc sCheckFn[0x100];

struct Edit {
    uint8_t table;
    uint8_t len;
    uint16_t index;
    uint32_t offset;
    uint8_t bytes[PC_RANDO_TABLE_EDIT_MAX];
};

constexpr int kMaxEdits = 512;
Edit sEdits[kMaxEdits];
int sEditCount = 0;

PcRandoTableDesc sTables[PCR_TBL_COUNT];
// The snapshot: every bound table's entries, back to back, in table order.
constexpr size_t kSnapshotBytes = 64 * 1024;
uint8_t sSnapshot[kSnapshotBytes];
bool sApplied = false;
bool sListening = false;

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

bool bound(int t) {
    return sTables[t].entries != nullptr && sTables[t].count > 0;
}

// Copies every bound table into the snapshot (save) or back (restore).
void snapshotCopy(bool save) {
    size_t pos = 0;
    for (int t = 0; t < PCR_TBL_COUNT; t++) {
        if (!bound(t)) {
            continue;
        }
        for (int i = 0; i < sTables[t].count; i++) {
            void* e = sTables[t].entries[i];
            if (save) {
                memcpy(sSnapshot + pos, e, sTables[t].entry_size);
            } else {
                memcpy(e, sSnapshot + pos, sTables[t].entry_size);
            }
            pos += sTables[t].entry_size;
        }
    }
}

void applyEdit(const Edit& e) {
    if (!bound(e.table) || e.index >= sTables[e.table].count) {
        return;
    }
    memcpy((uint8_t*)sTables[e.table].entries[e.index] + e.offset, e.bytes, e.len);
}

void applyAll() {
    for (int i = 0; i < sEditCount; i++) {
        applyEdit(sEdits[i]);
    }
}

void onActive(int active) {
    if (active) {
        if (sApplied) {
            // Re-activation (a new seed, or deactivate under COS_RANDO_FORCE): start again from boot.
            snapshotCopy(false);
        } else {
            snapshotCopy(true);
            sApplied = true;
        }
        applyAll();
        logf("[cos] rando: item tables snapshotted, %d edit(s) applied, %d item override(s)\n", sEditCount,
             pc_rando_item_override_count());
    } else if (sApplied) {
        snapshotCopy(false);
        sApplied = false;
        logf("[cos] rando: item tables restored\n");
    }
}

} // namespace

extern "C" {

void pc_rando_item_override(uint8_t id, PcRandoItemFunc fn) {
    sItemFn[id] = fn;
}

void pc_rando_item_getcheck_override(uint8_t id, PcRandoItemCheckFunc fn) {
    sCheckFn[id] = fn;
}

PcRandoItemFunc pc_rando_item_override_get(uint8_t id) {
    return sItemFn[id];
}

PcRandoItemCheckFunc pc_rando_item_getcheck_override_get(uint8_t id) {
    return sCheckFn[id];
}

int pc_rando_item_override_count(void) {
    int n = 0;
    for (PcRandoItemFunc f : sItemFn) {
        n += f != nullptr;
    }
    return n;
}

int pc_rando_items_bind_tables(const PcRandoTableDesc* tables, int count) {
    if (sApplied) {
        snapshotCopy(false); // put the old tables back before forgetting them
        sApplied = false;
    }
    memset(sTables, 0, sizeof sTables);
    size_t total = 0;
    for (int t = 0; t < count && t < PCR_TBL_COUNT; t++) {
        total += tables[t].entries != nullptr ? (size_t)tables[t].count * tables[t].entry_size : 0;
    }
    if (total > kSnapshotBytes) {
        logf("[cos] rando: item tables need %zu snapshot bytes (> %zu); not bound\n", total, kSnapshotBytes);
        return 0;
    }
    for (int t = 0; t < count && t < PCR_TBL_COUNT; t++) {
        sTables[t] = tables[t];
    }
    if (!sListening) {
        sListening = true;
        pc_rando_add_listener(onActive); // called with 1 at once when already active
    } else if (pc_rando()) {
        onActive(1);
    }
    return 1;
}

int pc_rando_table_edit(int table, int index, size_t offset, const void* bytes, size_t len) {
    if (table < 0 || table >= PCR_TBL_COUNT || index < 0 || index > 0xFFFF || bytes == nullptr || len == 0 ||
        len > PC_RANDO_TABLE_EDIT_MAX) {
        return 0;
    }
    if (bound(table) && (index >= sTables[table].count || offset + len > sTables[table].entry_size)) {
        return 0;
    }
    Edit* slot = nullptr;
    for (int i = 0; i < sEditCount; i++) {
        Edit& e = sEdits[i];
        if (e.table == table && e.index == index && e.offset == offset && e.len == len) {
            slot = &e;
            break;
        }
    }
    if (slot == nullptr) {
        if (sEditCount >= kMaxEdits) {
            return 0;
        }
        slot = &sEdits[sEditCount++];
    }
    slot->table = (uint8_t)table;
    slot->index = (uint16_t)index;
    slot->offset = (uint32_t)offset;
    slot->len = (uint8_t)len;
    memcpy(slot->bytes, bytes, len);
    if (sApplied) {
        applyEdit(*slot);
    }
    return 1;
}

void pc_rando_table_edits_clear(void) {
    sEditCount = 0;
    if (sApplied) {
        snapshotCopy(false);
    }
}

int pc_rando_table_edit_count(void) {
    return sEditCount;
}

int pc_rando_tables_applied(void) {
    return sApplied ? 1 : 0;
}

} // extern "C"
