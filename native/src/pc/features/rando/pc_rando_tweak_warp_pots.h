/*
 * pc_rando_tweak_warp_pots.h - the inter-dungeon warp pots (docs/RANDOMIZER_PLAN.md step R7.1c):
 * wwrando 1.10.0 tweaks.py add_inter_dungeon_warp_pots and INTER_DUNGEON_WARP_DATA, applied while
 * pc_rando() is 1 and the seed has add_shortcut_warps_between_dungeons (PcRandoConfig
 * inter_dungeon_warps).
 *
 * Two cycles of three pots: DRC M_NewD2/Room2, FW kindan/Room1, TotG Siren/Room6 (event register
 * index 2), and FF ma2room/Room2, ET M_Dai/Room1, WT kaze/Room3 (event register index 5). For the
 * pot at index k of its cycle, as wwrando's DZx.add_entity appends them:
 *   - a PLYR "Link" spawn 69, spawn type 7 (flying out of a warp pot), room_num, the pot's position
 *     and y_rot (PLYR class defaults: unknown_param_4, evnt_index and ship_id 0xFF). In stage.dzs for
 *     M_Dai and kaze ("or the game will crash"), in the room's room.dzr otherwise. wwrando asserts the
 *     file then holds a single spawn 69; here a file that already has one is left vanilla (D9);
 *   - in room.dzr, 3 SCLS {destination stage, spawn 69, destination room, fade 4, padding 0xFF}, one
 *     per pot of the cycle (itself included), in cycle order;
 *   - in room.dzr, an ACTR "Warpts<k+1>": params = type (k+2) | event register << 4 | the 3 new SCLS
 *     indexes << 8/16/24 (actor_parameters.txt d_a_obj_warpt), the pot's position, x_rot and z_rot
 *     0xFFFF, y_rot, enemy_number 0xFFFF.
 * Entries go at the end of their chunk (pc_rando_res_grow_dzx), so vanilla and earlier patchers' SCLS
 * indexes (R6.12 rewrites SCLS in place) stay valid; the new SCLS indexes are the chunk's count before
 * the grow, + 0, 1, 2 (wwrando's len(entries_by_type(SCLS)) - 1 after each add).
 *
 * The particles 0x8161, 0x8162, 0x8165, 0x8166 and 0x8112 are imported from Pscene035 into Pscene050
 * (TotG) and Pscene043 (FF) when missing, through R7.1b's registry (pc_rando_jpc_scene.h), from an
 * activation listener. The pots' custom smoke colours (misc_rando_features.asm, event register 2 or
 * 5: prm 0xE5101B80, env 0x3C379D80) are the d_a_obj_warpt.cpp createInit hunk, under pc_rando().
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_tweak_warp_pots_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_WARP_POTS_H
#define PC_RANDO_TWEAK_WARP_POTS_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCR_WARP_POT_COUNT 6
#define PCR_WARP_POT_CYCLE 3
#define PCR_WARP_POT_SPAWN_ID 69
#define PCR_WARP_POT_SPAWN_TYPE 7
#define PCR_WARP_POT_FADE 4
/* misc_rando_features.asm custom_warp_pot_prm_color / custom_warp_pot_env_color (RGBA). */
#define PCR_WARP_POT_PRM_COLOR 0xE5101B80u
#define PCR_WARP_POT_ENV_COLOR 0x3C379D80u
/* tweaks.py: the particles copied from Pscene035 into Pscene050 and Pscene043. */
#define PCR_WARP_POT_PARTICLE_COUNT 5
#define PCR_WARP_POT_PARTICLE_SOURCE 35

/* One entry of INTER_DUNGEON_WARP_DATA (pots 0-2: cycle 0, pots 3-5: cycle 1). */
typedef struct PcRandoWarpPot {
    const char* stage;
    int room;
    float x, y, z;
    uint16_t yRot;
    uint8_t eventReg;   /* cyclic_event_reg_index: 2 or 5 */
    uint8_t cycle;      /* 0 or 1 */
    uint8_t index;      /* 0-2 in the cycle */
    uint8_t spawnInStage; /* 1: the spawn goes in stage.dzs (M_Dai, kaze) */
} PcRandoWarpPot;

/* Pot i (0..5), or NULL. */
const PcRandoWarpPot* pc_rando_warp_pot(int i);
/* The pot of a stage and room, or -1. */
int pc_rando_warp_pot_find(const char* stage, int room);
/* The pot that the cycle's destination d (0-2) of pot i leads to. */
int pc_rando_warp_pot_dest(int i, int d);
/* The destination (0-2) daObj_Warpt_c::spWarp takes first: type 2 -> 1, 3 -> 2, 4 -> 0 (the next pot
   of the cycle). */
int pc_rando_warp_pot_next_dest(int i);
/* The Warpts params of pot i when its 3 SCLS start at index firstScls. */
uint32_t pc_rando_warp_pot_params(int i, uint32_t firstScls);
/* The 8-byte zero-padded actor name "Warpts<k+1>" of pot i into out[8]. */
void pc_rando_warp_pot_name(int i, char out[8]);
/* The particle ids imported (tweaks.py order). */
uint16_t pc_rando_warp_pot_particle(int k);

/* The entries pot i adds to a file: what = 0 the room.dzr (SCLS x3, ACTR, and PLYR unless
   spawnInStage), 1 the stage.dzs (PLYR, only when spawnInStage). The PLYR, SCLS and ACTR bytes are
   written to the given buffers (each may be NULL); firstScls as pc_rando_warp_pot_params. */
void pc_rando_warp_pot_entries(int i, uint32_t firstScls, uint8_t plyr[0x20], uint8_t scls[3][0xC],
                               uint8_t actr[0x20]);

/* The edit of one raw file of stage `stage`, room `room` (-1 for stage.dzs): result codes. */
enum {
    PCR_WARP_POTS_NONE = 0,    /* not a warp pot file (nothing to do) */
    PCR_WARP_POTS_PATCHED = 1, /* entries added */
    PCR_WARP_POTS_REFUSED = -1, /* D9: not a dzx file, or a spawn 69 already there; left vanilla */
    PCR_WARP_POTS_FAILED = -2,  /* the grown copy could not be built (allocation) */
};
/* Applies the edit to src into dst (a grown copy; cap >= pc_rando_warp_pots_grown_size). Returns a
   result code; *outSize gets the copy's size when PATCHED. Does not check pc_rando() or the option. */
size_t pc_rando_warp_pots_grown_size(const char* stage, int room, const uint8_t* src, size_t size);
int pc_rando_warp_pots_apply(const char* stage, int room, const uint8_t* src, size_t size, uint8_t* dst,
                             size_t cap, size_t* outSize);

/* The dzx patcher: does the edit when pc_rando() and pc_rando_cfg()->inter_dungeon_warps. */
void pc_rando_tweak_warp_pots_patch(PcRandoResFile* file);
/* Registers the patcher and the activation listener (particle imports). Idempotent. */
void pc_rando_tweak_warp_pots_init(void);
/* What the listener registered at the last activation: the number of imports (0 when the option is
   off). */
int pc_rando_warp_pots_imports_registered(void);

/* What the patcher did per pot since boot (or the last reset). */
typedef struct PcRandoWarpPotStatus {
    uint32_t roomSeen;     /* room.dzr files dispatched with the option on */
    uint32_t roomPatched;
    uint32_t stageSeen;    /* stage.dzs files (spawnInStage pots only) */
    uint32_t stagePatched;
    int lastRoom;          /* last result code for the room file (-3 before the first) */
    int lastStage;
    uint32_t firstScls;    /* the new SCLS indexes' first, at the last room patch */
} PcRandoWarpPotStatus;
void pc_rando_warp_pots_status(int i, PcRandoWarpPotStatus* out);
void pc_rando_warp_pots_reset_status(void);
/* Files of the 6 pots' stages and rooms the patcher saw with the option off (it left them alone). */
uint32_t pc_rando_warp_pots_skipped(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_WARP_POTS_H */
