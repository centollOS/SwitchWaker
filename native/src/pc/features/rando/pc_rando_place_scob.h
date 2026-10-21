// pc_rando_place_scob.h - placement of the SCOB item locations at archive load (docs/RANDOMIZER_PLAN.md
// step R2.3): the 57 locations (204 key paths) whose item sits in a room.dzr SCOB entry:
//   - 49 Sunken Treasures, each 4 `Salvage` entries (d_a_salvage, params & 0x00000FF0);
//   - 5 Big Octos and 2 Gunboats (`SwSlvg`, also d_a_salvage);
//   - the black-soil dig spot on Outset (`TagKb`, d_a_tag_kb_item, params & 0x000000FF).
// Plus wwrando's tweak fix_needle_rock_island_salvage_flags (sea/Room29: the first two d_a_salvage
// SCOBs of salvage type 2-4 with salvage flag 0 or 1 get flags 8 and 9, so the light-ring salvages no
// longer share flags with the gunboats).
//
// A dzx patcher (pc_rando_resources.h) registered by pc_rando_place_scob_init (pc_rando_patchers.def).
// For every SCOB key of the file's stage and archive (pcr::location_keys(), kind Dzx, field
// SalvageItem or TagKbItem) it checks the D9 fingerprint (8-byte name and the field's disc value) and
// writes pc_rando_loc_item(loc, vanilla) into the field. On a mismatch the entry is left vanilla and
// the mismatch is logged.
//
// Pure (cos_rando_core): no game headers; the results below are for the smokes and unit tests.
#ifndef PC_RANDO_PLACE_SCOB_H
#define PC_RANDO_PLACE_SCOB_H

#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the SCOB patcher and the Needle Rock salvage-flag patcher (pc_rando_patchers.def). */
void pc_rando_place_scob_init(void);

/* The patchers themselves (registered by the init; tests may call them directly). */
void pc_rando_place_scob_patch(PcRandoResFile* file);
void pc_rando_place_scob_needle_rock_patch(PcRandoResFile* file);

/* Per key (index into pcr::location_keys()), the result of the last load of its archive. */
#define PCR_SCOB_NOT_SEEN 0 /* not a SCOB key, or its archive was not dispatched yet */
#define PCR_SCOB_PLACED 1   /* the field now holds pc_rando_place_scob_item(key) */
#define PCR_SCOB_MISMATCH 2 /* D9 fingerprint failed: left vanilla */
#define PCR_SCOB_MISSING 3  /* no such chunk entry in the file: left vanilla */
int pc_rando_place_scob_result(int keyIndex);
/* The item written for a PLACED key (else 0xFF). */
uint8_t pc_rando_place_scob_item(int keyIndex);
/* The number of SCOB keys (204). */
int pc_rando_place_scob_key_count(void);
/* Needle Rock (sea/Room29): flags rewritten at its last load (0..2); -1 when not loaded since the
   reset, -2 when fewer than two candidate entries were found (nothing written). */
int pc_rando_place_scob_needle_rock(void);
/* Forgets every result (smokes, tests). */
void pc_rando_place_scob_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_PLACE_SCOB_H */
