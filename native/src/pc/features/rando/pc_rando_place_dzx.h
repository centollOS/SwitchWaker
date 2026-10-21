/*
 * pc_rando_place_dzx.h - the placement written into stage.dzs / room.dzr as they load
 * (docs/RANDOMIZER_PLAN.md phase R2: R2.1 chests; R2.2 ACTR and R2.3 SCOB extend the same patcher).
 *
 * wwrando's randomizers/items.py change_item edits the item field of the chunk entries named by the
 * location paths (logic/item_locations.txt) on the disc. The port does it in memory: one dzx patcher
 * (pc_rando_resources.h, registered from pc_rando_patchers.def) looks up every Dzx key of R0.5's
 * table (pc_rando_lockeys.h) whose stage directory, archive and file match the file being loaded and
 * writes pc_rando_loc_item(loc, vanilla) into the key's field, after the D9 fingerprint check (the
 * entry's 8-byte name and the field's disc value). On a mismatch (replaced content files) the entry
 * is left as it is, the location stays vanilla, and the mismatch is logged.
 *
 * R2.1 handles the chests (DzxField::TboxItem: TRES and the layer chunks TRE0-TREb, high byte of the
 * entry's angle z at +0x1C), in Stage.arc's stage.dzs and Room<N>.arc's room.dzr, including the
 * three Forsaken Fortress stages (majroom, ma2room, ma3room) and the multi-path locations (Ghost
 * Ship, Pirate Ship, Windfall's transparent chest), whose every path gets the same item.
 * dStage_KeepTresureInfoProc copies the same (patched) TRES entries, so the dungeon map's chest info
 * matches.
 *
 * The status table remembers, per key (pcr::location_keys() index), what the last patch of that
 * key's file did, for the location smoke (smokes/pc_rando_smoke_loc.cpp) and the unit test.
 *
 * Pure: no game header or game symbol (cos_rando_core links it into the unit tests).
 */
#ifndef PC_RANDO_PLACE_DZX_H
#define PC_RANDO_PLACE_DZX_H

#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What the last patch of a key's file did. */
enum PcRandoPlaceResult {
    PCR_PLACE_NONE = 0, /* the key's file has not been patched (not loaded, or no active rando) */
    PCR_PLACE_WRITTEN,  /* the placed item was written */
    PCR_PLACE_SAME,     /* the placed item is the disc's value: nothing written */
    PCR_PLACE_MISMATCH, /* D9 fingerprint mismatch: left vanilla */
    PCR_PLACE_MISSING,  /* no such chunk or entry in the file: left vanilla */
};

typedef struct PcRandoPlaceStatus {
    uint8_t result;     /* PCR_PLACE_* */
    uint8_t item;       /* the item written (WRITTEN, SAME), else the placed item */
    uint8_t found;      /* the field's value before the patch (MISMATCH: what the file holds) */
    uint32_t entry_off; /* the entry's byte offset in the file (WRITTEN, SAME, MISMATCH) */
    uint32_t patches;   /* how many times the key's file was patched since boot */
} PcRandoPlaceStatus;

/* Registers the dzx patcher (pc_rando_patchers.def). */
void pc_rando_place_dzx_init(void);
/* The patcher itself (also for tests). Does nothing unless pc_rando() is 1. */
void pc_rando_place_dzx_patch(PcRandoResFile* file);
/* 1 when this step's patcher writes keys of that DzxField (pcr::DzxField as an int). */
int pc_rando_place_dzx_handles(int field);

/* The status of key `key_index` (pcr::location_keys() index), NULL when out of range. */
const PcRandoPlaceStatus* pc_rando_place_dzx_status(int key_index);
/* Forgets every status (tests). */
void pc_rando_place_dzx_reset(void);

/* Counters since boot (or the last reset). */
typedef struct PcRandoPlaceStats {
    uint32_t files;    /* files that had at least one handled key */
    uint32_t written;  /* keys written */
    uint32_t same;     /* keys whose placed item equals the disc's value */
    uint32_t mismatch; /* D9 mismatches */
    uint32_t missing;  /* keys whose chunk or entry is missing */
} PcRandoPlaceStats;
void pc_rando_place_dzx_stats(PcRandoPlaceStats* out);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_PLACE_DZX_H */
