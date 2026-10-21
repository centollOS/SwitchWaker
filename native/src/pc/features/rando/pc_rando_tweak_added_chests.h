/*
 * pc_rando_tweak_added_chests.h - the 4 chests wwrando adds (docs/RANDOMIZER_PLAN.md step R2.10),
 * made in memory through the archive-load hook (pc_rando_resources.h, step R0.10) while pc_rando()
 * is 1. wwrando 1.10.0 tweaks.py:
 *
 *   add_chest_in_place_of_jabun_cutscene         Pjavdou/Room0 room.dzr: ACTR "Ikada" (the raft,
 *                                                y_rot 0x8000), FILI[0] wind_type 0, TRES takara3
 *                                                (behaviour 5 so it bobs with the raft, opened flag 6)
 *                                                with Jabun's Cave's placed item;
 *                                                sea/Room44 room.dzr: ACT5 "Ajav" (the stone door) and
 *                                                "Auzu" (the whirlpool) copied to ACTR and removed from
 *                                                ACT5 (they stand by day too);
 *                                                sea/Stage event_list.dat "ajav_uzu": DIRECTOR keeps 1
 *                                                action, CAMERA and Ship 2, ending flags = [DIRECTOR's
 *                                                last flag, CAMERA's last flag, -1] (KoRL no longer
 *                                                sails into the cave by himself)
 *   add_chest_in_place_medli_grappling_hook_gift M_Dra09/Stage stage.dzs: TRES takara3 (opened flag
 *                                                0x11, room 9) with Dragon Roost Cavern - Miniboss's
 *                                                placed item; M_NewD2/Stage stage.dzs: the same chest
 *                                                as a dummy with item 0xFF
 *   add_chest_in_place_of_master_sword           kenroom/Room0 room.dzr: ACTR "VmsMS"/"VmsDZ" removed,
 *                                                ACT5 "Yswdr00"/"ALLdie"/"Tn" copied to ACTR, every
 *                                                ACT5 actor removed, TRES takara3 (appear switch 5, set
 *                                                by the Mighty Darknuts, behaviour 4) with the Master
 *                                                Sword Chamber's placed item, PLYR spawn 10 moved back
 *                                                (y -2949.39, z -4240.7)
 *   add_chest_in_place_queen_fairy_cutscene      sea/Room9 room.dzr: TRES takara3 (opened flag 0x1C,
 *                                                room 9) with Inside Mother Isle's placed item
 *
 * wwlib semantics as in R3.16 (pc_rando_tweak_stages_b.h): remove_entity clears the 8-byte name
 * (pc_rando_dzx_suppress, indexes do not move), add_entity appends at the end of the chunk
 * (pc_rando_res_grow_dzx; a chunk the file lacks, e.g. TRES, gets a new row) with the ACTR class
 * defaults (rotation 0, enemy_number 0xFFFF). The added chest is Chest000 of its file (no file had a
 * TRES chunk), which is the TRES index of R0.5's Added keys (pc_rando_lockeys.h). The chest's item
 * is pc_rando_loc_item(loc, vanilla) of that key.
 *
 * Plan D9: each file is checked first (no TRES chunk where the chest becomes Chest000, the named
 * actors and spawn 10 present, the event's actors and actions present). On a mismatch the whole file
 * stays vanilla and it is logged; nothing is written before every check of the file held.
 *
 * The cutscene sites of the same locations (d_a_demo00.cpp l_itemNo[0, 5, 8] and d_a_npc_md.cpp
 * initialLetterEvent) read pc_rando_added_chests_demo_item / pc_rando_loc_item (pc_rando_hooks.h).
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_tweak_added_chests_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_ADDED_CHESTS_H
#define PC_RANDO_TWEAK_ADDED_CHESTS_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the dzx patcher and the event_list.dat patcher. Idempotent. */
void pc_rando_tweak_added_chests_init(void);
/* The patchers (registered by init; exposed for tests). Both do nothing unless pc_rando() is 1. */
void pc_rando_tweak_added_chests_patch_dzx(PcRandoResFile* file);
void pc_rando_tweak_added_chests_patch_event(PcRandoResFile* file);

/* The edit sites, one per file. */
enum PcRandoAddedSite {
    PCR_AC_JABUN_ROOM = 0,  /* Pjavdou/Room0: raft, wind, chest */
    PCR_AC_JABUN_DOOR,      /* sea/Room44: stone door and whirlpool on the default layer */
    PCR_AC_JABUN_EVENT,     /* sea/Stage event_list.dat: ajav_uzu shortened */
    PCR_AC_MEDLI_CHEST,     /* M_Dra09/Stage: chest */
    PCR_AC_MEDLI_DUMMY,     /* M_NewD2/Stage: dummy chest (item 0xFF) */
    PCR_AC_MASTER_SWORD,    /* kenroom/Room0: Master Sword removed, Darknuts on layer 0, chest, spawn 10 */
    PCR_AC_QUEEN_FAIRY,     /* sea/Room9: chest */
    PCR_AC_SITE_COUNT
};

typedef struct PcRandoAddedSiteInfo {
    const char* stage;  /* "Pjavdou" */
    const char* arc;    /* "Room0", "Stage" */
    const char* name;   /* "room.dzr", "stage.dzs", "event_list.dat" */
    const char* what;   /* a short description for logs */
    int added;          /* PCR_ADDED_* of the chest this file gets, or PCR_ADDED_NONE */
    int loc;            /* PCR_LOC_* of that chest, or -1 */
    int is_event;       /* 1: result is a PCR_EVL_* code, else a PCR_DZX_* code */
    /* state */
    int runs;           /* how often the site's file was patched since boot (or the last reset) */
    int result;         /* the last result (0 = OK: applied) */
    uint8_t item;       /* the item written into the chest (0xFF for the dummy) */
    uint64_t in_hash;   /* dzx sites: the content hash of the file as the patcher got it (the disc's) */
} PcRandoAddedSiteInfo;

/* Site i (PcRandoAddedSite), or NULL. */
const PcRandoAddedSiteInfo* pc_rando_tweak_added_chests_site(int i);
/* Forgets the runs and results (tests). */
void pc_rando_tweak_added_chests_reset(void);
/* The result as text: pc_rando_dzx_result_name or pc_rando_evl_result_name. */
const char* pc_rando_tweak_added_chests_result_name(int site);

/* The built TRES entry of an added chest (PCR_ADDED_*) holding `item`, as tweaks.py makes it
   (disc format, 0x20 bytes). Returns 0 for PCR_ADDED_NONE or an unknown id. The dummy M_NewD2
   chest is the Medli chest with item 0xFF. */
int pc_rando_added_chest_entry(int added, uint8_t item, uint8_t out[0x20]);

/* The item a cutscene gives in place of daDemo00_c's l_itemNo[argID] (d_a_demo00.cpp, the demo
   "item" parameter): argID 0 Master Sword Chamber, 5 Jabun's Cave, 8 Inside Mother Isle (the
   main.dol bytes 0x8035BD34/39/3C wwrando writes the placed items to); every other argID, and
   pc_rando() 0, gives `vanilla`. */
uint8_t pc_rando_added_chests_demo_item(int argID, uint8_t vanilla);

/* The record checks of the smoke and the tests: 1 when the raw (disc-format) final bytes of the
   site's file hold every edit of the site (the chest with `item`, raft, wind, layer moves, removed
   actors, spawn 10; for the event site the shortened chains and the ending flags). Writes the first
   failed check to `why` (may be NULL). */
int pc_rando_tweak_added_chests_verify(int site, const uint8_t* data, size_t size, uint8_t item, char* why,
                                       size_t whyCap);

/* The content hash of a dzx: FNV-1a 64 over, for each chunk row in file order whose tag is one this
   step edits (TRES, ACTR, ACT0-ACTb, PLYR, FILI; other rows are skipped), the 4-byte tag, the
   big-endian entry count and the entries. It does not depend on the chunk offsets or on chunks the
   game may convert in place, so the game's stored copy (relocated by dStage_dt_c_offsetToPtr)
   hashes the same as the raw file: start with h = PCR_AC_HASH_SEED and feed every chunk with
   ..._hash_chunk (which skips the other tags itself). */
#define PCR_AC_HASH_SEED 0xCBF29CE484222325ull
uint64_t pc_rando_tweak_added_chests_hash_chunk(uint64_t h, const char tag[4], uint32_t count, const uint8_t* entries);
/* The content hash of a raw dzx file (0 when it is not a dzx). */
uint64_t pc_rando_tweak_added_chests_content_hash(const uint8_t* data, size_t size);
/* FNV-1a 64 of a whole buffer (seed PCR_AC_HASH_SEED). */
uint64_t pc_rando_tweak_added_chests_hash(const void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_ADDED_CHESTS_H */
