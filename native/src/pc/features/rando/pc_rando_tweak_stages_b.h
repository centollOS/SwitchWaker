/*
 * pc_rando_tweak_stages_b.h - stage tweaks B (docs/RANDOMIZER_PLAN.md step R3.16): the stage.dzs /
 * room.dzr edits of six tweaks.py functions that wwrando always applies, made in memory through the
 * archive-load hook (pc_rando_resources.h, step R0.10) while pc_rando() is 1:
 *
 *   #19 add_failsafe_id_0_spawns       PLYR spawn 0 in 44 rooms: 32 copied from an existing spawn,
 *                                      12 (TF_01/TF_02 rooms 1-6) 200 units in front of a stage door
 *   #20 add_spawns_outside_boss_doors  PLYR spawn 27 in front of 6 boss doors
 *   #21 remove_minor_panning_cutscenes SCOB TagEv removed and PLYR evnt_index cleared (7 sites)
 *   #22 add_shortcut_warps_into_dungeons  sea/Room41: SCOB Ysdls00 + SCOB SW_C00 (switch 0x7F)
 *   #8  add_more_magic_jars            6 skull/pot drops and 2 grass patches
 *   #12 add_hint_signs                 M_NewD2/Room2 BFlower[1] -> Kanban (message 847, added to the
 *                                      message overlay of step R0.12)
 *
 * Every record is computed the way tweaks.py computes it, from the file's own disc bytes (copied
 * spawns, door positions), so no disc data is in the source. A spawn in front of a door that lives in
 * stage.dzs is computed when that stage.dzs is dispatched (the stage archive loads before its rooms)
 * and appended when the room's room.dzr is dispatched.
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_tweak_stages_b_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_STAGES_B_H
#define PC_RANDO_TWEAK_STAGES_B_H

#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the dzx patcher and the message listener (message 847). Idempotent. */
void pc_rando_tweak_stages_b_init(void);
/* The patcher (registered by init; exposed for tests). */
void pc_rando_tweak_stages_b_patch(PcRandoResFile* file);
/* Adds message 847 (the hint sign's right arrow) to the overlay. Returns 1 on success. */
int pc_rando_tweak_stages_b_add_messages(void);

/* The tweak of a site. */
enum PcRandoStagesBTweak {
    PCR_SB_FAILSAFE_SPAWN = 19,
    PCR_SB_BOSS_SPAWN = 20,
    PCR_SB_PANNING = 21,
    PCR_SB_FH_WARP = 22,
    PCR_SB_MAGIC_JAR = 8,
    PCR_SB_HINT_SIGN = 12,
};

/* One edit site: a record in one file, as the patcher saw it last. */
typedef struct PcRandoStagesBSite {
    int tweak;          /* PcRandoStagesBTweak */
    const char* stage;  /* "M_NewD2" */
    int room;           /* the file's room (-1: stage.dzs) */
    const char* what;   /* a short description for logs */
    int runs;           /* how often the site's file was patched since boot */
    int result;         /* the last PCR_DZX_* result (PCR_DZX_OK when applied) */
} PcRandoStagesBSite;

int pc_rando_tweak_stages_b_site_count(void);
/* Site i, or NULL. */
const PcRandoStagesBSite* pc_rando_tweak_stages_b_site(int i);
/* Forgets the runs and results (tests). */
void pc_rando_tweak_stages_b_reset(void);

/* The record checks of the smoke: 1 when the final bytes of a file (stage, room) hold every record
   of the sites of that file (spawn 0 / 27 present, TagEv gone, spawn events cleared, drops, sign,
   grass, warp). Writes the first failed check to `why` (may be NULL). Files without sites: 1. */
int pc_rando_tweak_stages_b_verify(const char* stage, int room, const uint8_t* data, uint32_t size,
                                   char* why, uint32_t whyCap);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_STAGES_B_H */
