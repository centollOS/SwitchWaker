// pc_rando_charts.cpp - randomized charts at run time (docs/RANDOMIZER_PLAN.md step R6.6;
// pc_rando_charts.h). wwrando: randomizers/charts.py ChartRandomizer._save, wwlib/charts.py,
// data/actor_parameters.txt (d_a_salvage chart_index_plus_1 = params & 0x0FF00000, salvage_type =
// params & 0xF0000000). Pure: no game headers (cos_rando_core).
#include "pc_rando_charts.h"

#include "pc/pc_rando.h"
#include "pc_rando_data.h"
#include "pc_rando_dzx.h"
#include "pc_rando_seedfile.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

namespace {

constexpr int kIslands = 49;

int sCmapStatus = PCR_CHARTS_NOT_RUN;
const void* sCmapBuffer = nullptr;
bool sHaveVanilla = false;
uint8_t sVanilla[kIslands + 1][PCR_CMAP_RECORD_SIZE];       // by island, before the last patch
uint8_t sPatchedImage[(kIslands + 16) * PCR_CMAP_RECORD_SIZE]; // the records after the last patch
size_t sPatchedSize = 0;

int sRoomStatus[kIslands + 1];
int sRoomWritten[kIslands + 1];
int sRoomIndex[kIslands + 1];

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

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Chart.type in [0, 1, 2, 6]: the charts ChartRandomizer moves (Triforce and Treasure Charts).
bool randomizableType(uint8_t type) {
    return type == 0 || type == 1 || type == 2 || type == 6;
}

std::string chartNameOfNumber(int number) {
    char b[32];
    if (number <= 8) {
        snprintf(b, sizeof b, "Triforce Chart %d", number);
    } else {
        snprintf(b, sizeof b, "Treasure Chart %d", number - 8);
    }
    return b;
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

// The active seed's chart per island; false unless it names a chart for every island 1-49.
bool seedIslandCharts(const char* out[kIslands + 1]) {
    const PcRandoSeed* seed = pc_rando_seed();
    for (int i = 0; i <= kIslands; i++) {
        out[i] = nullptr;
    }
    if (seed == nullptr) {
        return false;
    }
    for (const auto& c : seed->charts) {
        if (c.first >= 1 && c.first <= kIslands) {
            out[c.first] = c.second.c_str();
        }
    }
    for (int i = 1; i <= kIslands; i++) {
        if (out[i] == nullptr) {
            return false;
        }
    }
    return true;
}

} // namespace

extern "C" {

int pc_rando_chart_number(const char* name) {
    if (name == nullptr) {
        return -1;
    }
    int base;
    const char* digits;
    if (strncmp(name, "Triforce Chart ", 15) == 0) {
        base = 0;
        digits = name + 15;
    } else if (strncmp(name, "Treasure Chart ", 15) == 0) {
        base = 8;
        digits = name + 15;
    } else {
        return -1;
    }
    if (*digits < '1' || *digits > '9') {
        return -1;
    }
    char* end = nullptr;
    const long k = strtol(digits, &end, 10);
    if (end == nullptr || *end != '\0' || k < 1 || k > (base == 0 ? 8 : 41)) {
        return -1;
    }
    return base + (int)k;
}

int pc_rando_chart_owned_index(const char* name) {
    if (pc_rando_chart_number(name) < 0) {
        return -1;
    }
    const int id = pcr::item_id(name);
    // item_func_collectmap<k> is the function of item 0xFF - k (d_item.cpp item_func_ptr).
    return id >= 0xBF && id <= 0xFE ? 0xFF - id : -1;
}

int pc_rando_chart_island_of_sector(int sectorX, int sectorY) {
    return sectorX + 3 + (sectorY + 3) * 7 + 1;
}

int pc_rando_charts_apply_cmapdat(uint8_t* data, size_t size, const char* const islandCharts[50]) {
    if (data == nullptr || size < 4) {
        return PCR_CHARTS_MISMATCH;
    }
    const uint32_t num = be32(data);
    if (num > 0x400 || 4 + (size_t)num * PCR_CMAP_RECORD_SIZE > size) {
        return PCR_CHARTS_MISMATCH;
    }
    // The randomizable charts: exactly the 49 charts, each at its own island, with the owned index
    // of its item (D9).
    int byIsland[kIslands + 1];
    int byNumber[kIslands + 1];
    for (int i = 0; i <= kIslands; i++) {
        byIsland[i] = byNumber[i] = -1;
    }
    int count = 0;
    for (uint32_t r = 0; r < num; r++) {
        const uint8_t* rec = data + 4 + r * PCR_CMAP_RECORD_SIZE;
        if (!randomizableType(rec[3])) {
            continue;
        }
        count++;
        const int number = rec[2];
        const int island = pc_rando_chart_island_of_sector((int8_t)rec[4], (int8_t)rec[5]);
        if (number < 1 || number > kIslands || island < 1 || island > kIslands || byIsland[island] >= 0 ||
            byNumber[number] >= 0) {
            return PCR_CHARTS_MISMATCH;
        }
        if (rec[1] != pc_rando_chart_owned_index(chartNameOfNumber(number).c_str())) {
            return PCR_CHARTS_MISMATCH;
        }
        byIsland[island] = (int)r;
        byNumber[number] = (int)r;
    }
    if (count != kIslands) {
        return PCR_CHARTS_MISMATCH;
    }
    // The seed: each chart once.
    int numberOfIsland[kIslands + 1];
    bool used[kIslands + 1] = {};
    for (int island = 1; island <= kIslands; island++) {
        const int n = islandCharts == nullptr ? -1 : pc_rando_chart_number(islandCharts[island]);
        if (n < 1 || used[n]) {
            return PCR_CHARTS_BAD_SEED;
        }
        used[n] = true;
        numberOfIsland[island] = n;
    }
    // original_charts = copy.deepcopy(randomizable_charts)
    uint8_t original[kIslands + 1][PCR_CMAP_RECORD_SIZE];
    for (int island = 1; island <= kIslands; island++) {
        memcpy(original[island], data + 4 + byIsland[island] * PCR_CMAP_RECORD_SIZE, PCR_CMAP_RECORD_SIZE);
    }
    for (int island = 1; island <= kIslands; island++) {
        const uint8_t* from = original[island]; // chart_to_copy_from: the chart of this island
        uint8_t* chart = data + 4 + byNumber[numberOfIsland[island]] * PCR_CMAP_RECORD_SIZE;
        chart[0] = from[0];                                   // texture_id
        chart[4] = from[4];                                   // sector_x
        chart[5] = from[5];                                   // sector_y
        memcpy(chart + 6, from + 6, PCR_CMAP_RECORD_SIZE - 6); // the 4 possible random positions
    }
    return PCR_CHARTS_PATCHED;
}

void pc_rando_charts_cmapdat_loaded(void* p) {
    if (!pc_rando()) {
        return;
    }
    uint8_t* data = (uint8_t*)p;
    const void* last = sCmapBuffer;
    sCmapBuffer = p;
    const char* charts[kIslands + 1];
    if (!seedIslandCharts(charts)) {
        sCmapStatus = PCR_CHARTS_NO_SEED;
        return;
    }
    const uint32_t num = data != nullptr ? be32(data) : 0;
    const size_t recs = num <= 0x400 ? (size_t)num * PCR_CMAP_RECORD_SIZE : 0;
    const size_t size = 4 + recs;
    if (data != nullptr && p == last && sCmapStatus != PCR_CHARTS_NOT_RUN &&
        sPatchedSize != 0 && recs == sPatchedSize && 4 + recs <= size &&
        memcmp(data + 4, sPatchedImage, recs) == 0 && sCmapStatus != PCR_CHARTS_MISMATCH) {
        // The menu read the same resource again (still loaded): it holds our patch already.
        sCmapStatus = PCR_CHARTS_ALREADY;
        return;
    }
    // Keep the vanilla records per island for the smoke, as they are before the patch.
    uint8_t before[kIslands + 1][PCR_CMAP_RECORD_SIZE];
    bool haveBefore = false;
    if (data != nullptr && recs != 0 && 4 + recs <= size) {
        for (size_t r = 0; r < recs / PCR_CMAP_RECORD_SIZE; r++) {
            const uint8_t* rec = data + 4 + r * PCR_CMAP_RECORD_SIZE;
            const int island = pc_rando_chart_island_of_sector((int8_t)rec[4], (int8_t)rec[5]);
            if (randomizableType(rec[3]) && island >= 1 && island <= kIslands) {
                memcpy(before[island], rec, PCR_CMAP_RECORD_SIZE);
            }
        }
        haveBefore = true;
    }
    sCmapStatus = pc_rando_charts_apply_cmapdat(data, size, charts);
    if (sCmapStatus == PCR_CHARTS_PATCHED) {
        if (haveBefore) {
            memcpy(sVanilla, before, sizeof sVanilla);
            sHaveVanilla = true;
        }
        sPatchedSize = recs <= sizeof sPatchedImage ? recs : 0;
        if (sPatchedSize != 0) {
            memcpy(sPatchedImage, data + 4, sPatchedSize);
        }
        logf("[cos] rando: charts: CmapDat.bin patched (49 charts)\n");
    } else {
        sPatchedSize = 0;
        logf("[cos] rando: charts: CmapDat.bin %s; the sea chart stays vanilla\n",
             sCmapStatus == PCR_CHARTS_MISMATCH ? "does not hold the 49 vanilla charts (D9)"
                                                : "not patched: the seed's charts are not 49 distinct charts");
    }
}

int pc_rando_charts_cmapdat_status(void) {
    return sCmapStatus;
}

const void* pc_rando_charts_cmapdat_buffer(void) {
    return sCmapBuffer;
}

const uint8_t* pc_rando_charts_vanilla_record(int island) {
    return sHaveVanilla && island >= 1 && island <= kIslands ? sVanilla[island] : nullptr;
}

// ChartRandomizer._save, second half: in sea/Room<chart.island_number>, every d_a_salvage SCOB with
// salvage_type 0 gets chart_index_plus_1 = chart.owned_chart_index_plus_1. DZx.entries_by_type(SCOB)
// covers every SCOB chunk (SCOB and the layer chunks SCO0-SCOb), in file order.
void pc_rando_charts_scob_patch(PcRandoResFile* file) {
    if (!pc_rando() || file == nullptr || file->type != PCR_RES_DZR || file->stage == nullptr ||
        strcmp(file->stage, "sea") != 0 || file->room < 1 || file->room > kIslands) {
        return;
    }
    const int room = file->room;
    sRoomWritten[room] = 0;
    sRoomIndex[room] = 0;
    const char* charts[kIslands + 1];
    if (!seedIslandCharts(charts)) {
        sRoomStatus[room] = PCR_CHARTS_NO_SEED;
        return;
    }
    const int want = pc_rando_chart_owned_index(charts[room]);
    const int vanilla = pc_rando_chart_owned_index(pcr::default_chart_for_island(room));
    if (want < 0 || vanilla < 0) {
        sRoomStatus[room] = PCR_CHARTS_BAD_SEED;
        logf("[cos] rando: charts sea/Room%d: no chart index for \"%s\"; salvage left vanilla\n", room,
             charts[room]);
        return;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK) {
        sRoomStatus[room] = PCR_CHARTS_MISMATCH;
        logf("[cos] rando: charts sea/Room%d: room.dzr does not parse; salvage left vanilla\n", room);
        return;
    }
    int written = 0, bad = 0;
    for (uint32_t c = 0; c < dzx.chunkCount; c++) {
        const uint8_t* row = dzx.data + 4 + c * 12;
        char tag[5];
        memcpy(tag, row, 4);
        tag[4] = '\0';
        if (strncmp(tag, "SCO", 3) != 0) {
            continue;
        }
        const uint32_t count = pc_rando_dzx_count(&dzx, tag);
        for (uint32_t i = 0; i < count; i++) {
            uint8_t* e = pc_rando_dzx_entry(&dzx, tag, i);
            if (e == nullptr || !isSalvageName(e)) {
                continue;
            }
            uint32_t params = pc_rando_dzx_be32(e + 0x08);
            if ((params >> 28) != 0) {
                continue;
            }
            const int have = (int)((params >> 20) & 0xFF);
            if (have != vanilla && have != want) {
                // D9: not the disc's chart index for this island (the second test lets a reloaded,
                // already patched buffer pass).
                bad++;
                logf("[cos] rando: charts sea/Room%d %s[%u]: chart index %d, expected %d; left vanilla\n", room,
                     tag, (unsigned)i, have, vanilla);
                continue;
            }
            params = (params & ~0x0FF00000u) | ((uint32_t)want << 20);
            e[0x08] = (uint8_t)(params >> 24);
            e[0x09] = (uint8_t)(params >> 16);
            e[0x0A] = (uint8_t)(params >> 8);
            e[0x0B] = (uint8_t)params;
            written++;
        }
    }
    sRoomWritten[room] = written;
    sRoomIndex[room] = written > 0 ? want : 0;
    sRoomStatus[room] = bad > 0 || written == 0 ? PCR_CHARTS_MISMATCH : PCR_CHARTS_PATCHED;
    logf("[cos] rando: charts sea/Room%d: %s (index %d -> %d), %d salvage point(s), %d left vanilla\n", room,
         charts[room], vanilla, want, written, bad);
}

void pc_rando_charts_init(void) {
    pc_rando_register_dzx_patcher(pc_rando_charts_scob_patch);
}

int pc_rando_charts_room_status(int room) {
    return room >= 1 && room <= kIslands ? sRoomStatus[room] : PCR_CHARTS_NOT_RUN;
}

int pc_rando_charts_room_written(int room) {
    return room >= 1 && room <= kIslands ? sRoomWritten[room] : 0;
}

int pc_rando_charts_room_index(int room) {
    return room >= 1 && room <= kIslands ? sRoomIndex[room] : 0;
}

void pc_rando_charts_reset(void) {
    sCmapStatus = PCR_CHARTS_NOT_RUN;
    sCmapBuffer = nullptr;
    sHaveVanilla = false;
    sPatchedSize = 0;
    for (int i = 0; i <= kIslands; i++) {
        sRoomStatus[i] = PCR_CHARTS_NOT_RUN;
        sRoomWritten[i] = 0;
        sRoomIndex[i] = 0;
    }
}

} // extern "C"
