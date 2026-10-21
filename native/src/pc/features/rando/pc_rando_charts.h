/*
 * pc_rando_charts.h - randomized charts at run time (docs/RANDOMIZER_PLAN.md step R6.6): wwrando
 * 1.10.0_9775811's randomizers/charts.py ChartRandomizer._save, applied in memory. The generator side
 * (the island shuffle) is pc_rando_gen_charts.h; the seed holds its result as PcRandoSeed::charts
 * (island 1-49 -> the chart name found by salvaging there).
 *
 * Two edits, both only under pc_rando() and only when the active seed has a chart for every island:
 *
 *   1. CmapDat.bin (fmapres.arc, 'DATA'), the sea chart's table of charts, read by
 *      dMenu_Fmap_c::aramCmapDatRead each time the chart menu opens (the menu frees it again in
 *      _delete). For each island N with chart C, the record of chart C (type 0, 1, 2 or 6: the 8
 *      Triforce and 41 Treasure Charts) gets, from the vanilla record of the chart for island N:
 *      the texture id (byte 0), the sector x/y (bytes 4, 5) and the 4 random positions (texture
 *      x/y offsets, salvage x/y; bytes 6-0x25). Its owned index, number and type stay. The game
 *      hunk calls pc_rando_charts_cmapdat_loaded with the resource (big-endian disc bytes).
 *   2. sea/Room1-49 room.dzr (an archive patcher, pc_rando_patchers.def): every d_a_salvage SCOB
 *      entry with salvage_type 0 (params >> 28; the sunken treasure points) gets
 *      chart_index_plus_1 (params & 0x0FF00000) = the owned index of the island's new chart.
 *
 * A chart's owned index ("owned_chart_index_plus_1", CmapDat byte 1, the collect-map number the
 * salvage tests) is 0xFF minus its item id: the item table gives item 0xFF - k the function
 * item_func_collectmap<k> (d_item.cpp), which sets collect map k. Both edits check it against the
 * disc before writing (decision D9): a CmapDat whose 49 chart records do not hold the 49 charts at
 * the 49 islands with these indices, or a SCOB whose index is not the vanilla chart's, is left
 * vanilla and logged.
 *
 * Pure (cos_rando_core): no game headers. Game thread only.
 */
#ifndef PC_RANDO_CHARTS_H
#define PC_RANDO_CHARTS_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CmapDat.bin layout (wwlib/charts.py ChartList / Chart). */
#define PCR_CMAP_RECORD_SIZE 0x26
#define PCR_CMAP_CHARTS 49

/* Results (pc_rando_charts_cmapdat_status, pc_rando_charts_room_status). */
#define PCR_CHARTS_NOT_RUN 0    /* not called since the reset, or pc_rando() was 0 */
#define PCR_CHARTS_PATCHED 1    /* written */
#define PCR_CHARTS_ALREADY 2    /* CmapDat: the same buffer, already patched (left as it is) */
#define PCR_CHARTS_NO_SEED 3    /* pc_rando() on but the seed has no full island -> chart table */
#define PCR_CHARTS_MISMATCH 4   /* D9: the disc data is not what wwrando expects: left vanilla */
#define PCR_CHARTS_BAD_SEED 5   /* the seed's charts are not the 49 charts once each */

/* The owned index of a chart ("Treasure Chart 25" -> 41, "Triforce Chart 1" -> 1), -1 for any other
   name. */
int pc_rando_chart_owned_index(const char* chartName);
/* The CmapDat number of a chart (Triforce Chart k -> k, Treasure Chart k -> 8 + k), -1 otherwise. */
int pc_rando_chart_number(const char* chartName);
/* wwlib Chart.island_number from a sector (-3..3 each). */
int pc_rando_chart_island_of_sector(int sectorX, int sectorY);

/* ChartRandomizer._save on a CmapDat.bin image (big-endian, `size` bytes). islandCharts[1..49] are
   the chart names per island (index 0 unused). Returns PCR_CHARTS_PATCHED, PCR_CHARTS_MISMATCH or
   PCR_CHARTS_BAD_SEED (then nothing was written). */
int pc_rando_charts_apply_cmapdat(uint8_t* data, size_t size, const char* const islandCharts[50]);

/* The game hunk in dMenu_Fmap_c::aramCmapDatRead (under pc_rando()): patches the loaded CmapDat.bin
   for the active seed. Its size is taken from its record count (at most 0x400 records): the aram
   archive's getResSize gives the packed size, not the loaded one. Calling it again on the buffer it
   patched last (unchanged since) does nothing (PCR_CHARTS_ALREADY). */
void pc_rando_charts_cmapdat_loaded(void* data);
/* The result of the last pc_rando_charts_cmapdat_loaded and the buffer it saw (smokes). */
int pc_rando_charts_cmapdat_status(void);
const void* pc_rando_charts_cmapdat_buffer(void);
/* The vanilla record (PCR_CMAP_RECORD_SIZE bytes) of the chart drawn for `island` (1-49), as it was
   before the last patch; NULL when nothing was patched yet. */
const uint8_t* pc_rando_charts_vanilla_record(int island);

/* The SCOB patcher (registered by pc_rando_charts_init) and its init (pc_rando_patchers.def). */
void pc_rando_charts_scob_patch(PcRandoResFile* file);
void pc_rando_charts_init(void);
/* Per sea room 1-49, the result of its last load: status, number of SCOB entries written, and the
   chart index written (0 when none). */
int pc_rando_charts_room_status(int room);
int pc_rando_charts_room_written(int room);
int pc_rando_charts_room_index(int room);

/* Forgets every result (smokes, tests). */
void pc_rando_charts_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_CHARTS_H */
