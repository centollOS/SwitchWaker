/*
 * pc_rando_bossreq.h - required bosses at runtime (docs/RANDOMIZER_PLAN.md step R6.9): wwrando
 * 1.10.0_9775811's RequiredBossesRandomizer._save (randomizers/boss_reqs.py), for a seed made with
 * required_bosses. The generator half (which bosses, the banned locations) is R6.8's
 * (pc_rando_gen_bosses.h); the seed carries the result as required_boss_locations.
 *
 * update_puppet_ganon_door_unlock_requirements, made in memory through the archive-load hook
 * (pc_rando_resources.h, step R0.10) while pc_rando() is 1 and the active seed has required bosses.
 * The plan allows the door condition in code or as DngSw/SwOp records; this takes the records,
 * exactly as wwrando writes them, since step R3.7's actors exist. GanonL/Room0 room.dzr:
 *   ACTR "ALLdie"     switch_to_set (params & 0xFF00) 0x19 -> 0x26 (all enemies dead no longer opens
 *                     the door directly)
 *   ACTR "DngSw"      appended: flag 3 (boss dead), stage mask of the required bosses' stages
 *                     (Gohma 3, Kalle Demos 4, Gohdan 5, Helmaroc King 2, Jalhalla 6, Molgera 7),
 *                     switch 0x25, at (1800, 2000, -19000)
 *   ACTR "SwOp"       appended: AND of 0x25, 0x26 -> 0x19 (the door's switch), one-shot, no event
 *   SCOB "TagMsg"     appended: message 851 at the KGBdor door (y + 200), spawn switch 0xE5, scale
 *                     80/255/10
 *   ACTR "SwOp"       appended: NAND of 0x25 -> 0xE5 (the bosses are not all dead), one-shot
 *   SCOB "NBOX10"     appended: an invisible wall behind the door (d_a_obj_akabe, collision 3, scale
 *                     mode 3, disable switch 0x25) at (-0.7944336, 0, -37797.72), scale 9/255/7
 * in wwrando's add_entity order (the ACTR adds and the SCOB adds each keep their order).
 * Plan D9: the file is checked first (ALLdie on 0x19 in ACTR, KGBdor in TGOB, no DngSw yet); on a
 * mismatch it stays vanilla and it is logged.
 *
 * show_quest_markers_on_sea_chart_for_dungeons: f_map.blo's quest marker PIC1 panes (marker index i
 * at 0x43B0 + i*0x40) are not patched as bytes; pc_rando_bossreq_markers gives what wwrando writes
 * there (visible, position, rotation) and the game side (game_hooks/pc_rando_hooks_bossreq.cpp)
 * applies it to the panes after dMenu_Fmap_c::_create builds the screen. The island of each boss is
 * EntranceRandomizer.get_entrance_zone_for_boss on the seed's entrance map: the outermost (island)
 * entrance of "<boss> Boss Arena", through nested entrances.
 *
 * Message 851 (bmg.add_new_message: special box 1, normal draw 0, alignment 4, word-wrapped with the
 * message font) is put in the overlay by the game side (pc_rando_bossreq_message gives its text).
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. Game thread only.
 */
#ifndef PC_RANDO_BOSSREQ_H
#define PC_RANDO_BOSSREQ_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PcRandoSeed;

/* boss_reqs.py update_puppet_ganon_door_unlock_requirements. */
#define PCR_BR_SW_DOOR 0x19          /* the KGBdor door's switch (vanilla: set by ALLdie) */
#define PCR_BR_SW_BOSSES_DEAD 0x25   /* set by DngSw */
#define PCR_BR_SW_ENEMIES_DEAD 0x26  /* ALLdie's new switch */
#define PCR_BR_SW_BOSSES_ALIVE 0xE5  /* NAND of 0x25: TagMsg's spawn switch (a zone bit) */
#define PCR_BR_MSG 851
#define PCR_BR_MSG_BOX 1             /* TextBoxType.SPECIAL */
#define PCR_BR_MSG_ALIGN 4           /* bottom text box */
#define PCR_BR_FLAG_BOSS_DEAD 3      /* DngSw flag_to_check: dSv_memBit_c STAGE_BOSS_ENEMY */
#define PCR_BR_MAX_BOSSES 6
#define PCR_BR_MARKERS 8             /* f_map.blo's quest marker panes */

/* The active seed's required bosses (bosses() order, which is wwrando's required_bosses order: the
   boss item locations in item_locations.txt order). Returns the count (0 when no seed is active, the
   seed has none, or a location name is not a boss Heart Container) and fills names (may be NULL) with
   pointers into pc_rando_data's boss table. */
int pc_rando_bossreq_bosses_of(const struct PcRandoSeed* seed, const char* names[PCR_BR_MAX_BOSSES]);
/* DngSw's stage_no_bitmask for those bosses (0 for none). */
uint32_t pc_rando_bossreq_stage_mask(const struct PcRandoSeed* seed);

/* One quest marker as show_quest_markers_on_sea_chart_for_dungeons leaves it. */
typedef struct PcRandoBossMarker {
    uint8_t index;    /* the PIC1 pane (BOSS_NAME_TO_SEA_CHART_QUEST_MARKER_INDEX) */
    uint8_t rotation; /* +0x19: 55 plus the overlap angle */
    int16_t x, y;     /* +0x10/+0x12: the island's sector (x*0x37 - 0xFA, y*0x38 - 0xBC) */
    uint8_t island;   /* 1..49 */
    const char* boss;
} PcRandoBossMarker;
/* The markers to show, in required_bosses order. Returns their count; 0 when the seed has no
   required bosses. A boss whose island cannot be resolved from the entrance map is skipped (logged).
   */
int pc_rando_bossreq_markers(const struct PcRandoSeed* seed, PcRandoBossMarker out[PCR_BR_MAX_BOSSES]);
/* The island name of a boss's arena through the seed's entrances (get_entrance_zone_for_boss), or
   NULL when the map does not lead to an island. */
const char* pc_rando_bossreq_boss_island(const struct PcRandoSeed* seed, const char* boss);
/* The pane tag of marker index 0..7 in the USA f_map.blo ('CK32', 'CK31', 'CK23', 'CK22', 'CK21',
   'CK13', 'CK12', 'CK11'), 0 when out of range. */
uint32_t pc_rando_bossreq_marker_tag(int index);

/* The dzx patcher (registered by init; exposed for tests). Does nothing unless pc_rando() is 1 and
   the active seed has required bosses; the active seed is pc_rando_seed() unless a test set one with
   pc_rando_bossreq_set_seed_for_test. */
void pc_rando_bossreq_patch_dzx(PcRandoResFile* file);
/* Registers the patcher (pc_rando_patchers.def calls pc_rando_bossreq_hooks_init on the game side,
   which calls this). Idempotent. */
void pc_rando_bossreq_init(void);
/* Tests: a seed used instead of pc_rando_seed() (NULL: back to pc_rando_seed()). */
void pc_rando_bossreq_set_seed_for_test(const struct PcRandoSeed* seed);

/* The room patch on a raw GanonL/Room0 room.dzr: the appended entries (DngSw, the two SwOps,
   TagMsg, NBOX10) for `stageMask`, from the door's position. Exposed for tests. */
void pc_rando_bossreq_dngsw(uint32_t stageMask, uint8_t out[0x20]);
void pc_rando_bossreq_swop(int nand, uint8_t out[0x20]);
void pc_rando_bossreq_tagmsg(const uint8_t door[0x20], uint8_t out[0x24]);
void pc_rando_bossreq_akabe(uint8_t out[0x24]);

/* What the patcher did with the last GanonL/Room0 room.dzr (smokes and tests). */
typedef struct PcRandoBossreqStatus {
    int seen;        /* files the patcher was called for since the last reset */
    int applied;     /* of those, patched */
    int result;      /* PCR_DZX_* of the last one */
    uint32_t mask;   /* the stage mask written */
} PcRandoBossreqStatus;
void pc_rando_bossreq_status(PcRandoBossreqStatus* out);
void pc_rando_bossreq_reset(void);

/* Message 851's string before word wrap (wwrando's msg.string). */
const char* pc_rando_bossreq_message_text(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "pc_rando_msg.h"
#include "pc_rando_wrap.h"
#include <string>
/* Message 851's entry (box 1, draw 0, alignment 4) and its text wrapped with `widths`
   (Message.word_wrap_string). */
void pc_rando_bossreq_message(const pcr::GlyphWidths& widths, PcRandoMsgEntry* entry, std::string* text);
#endif

#endif /* PC_RANDO_BOSSREQ_H */
