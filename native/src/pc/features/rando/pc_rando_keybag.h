/*
 * pc_rando_keybag.h - the randomizer's key bag text (docs/RANDOMIZER_PLAN.md step R3.5, asm report
 * group G3; wwrando tweaks.py implement_key_bag and misc_rando_features.asm
 * check_run_new_text_commands / exec_curr_num_keys_text_command).
 *
 * wwrando renames the Pirate's Charm (message 403, its name) to "Key Bag" and replaces its
 * description (603) with a text that shows the small keys of the five dungeons that have them. The
 * numbers come from five new text commands, "\{1A 05 00 00 4B}" to "\{1A 05 00 00 4F}" (DRC, FW,
 * TotG, ET, WT), which fopMsgM_msgDataProc_c::stringSet expands under pc_rando() (one hunk, through
 * pc_rando_text_command in pc/pc_rando_hooks.h). Each expands to the dungeon's key count followed by
 * " +Big" when its Big Key is owned, or by five spaces so the columns do not move.
 *
 * This file holds the pure part: the command -> dungeon mapping, where the key count is read from,
 * the expansion and the two message texts. The game side (game_hooks/pc_rando_hooks_keybag.cpp)
 * reads the save state and adds the messages to the overlay at every activation. The amulet_00.bti
 * icon swap is not done (plan D12, until R7.5).
 *
 * No game headers, no game calls (it links into cos_rando_core and the unit tests).
 */
#ifndef PC_RANDO_KEYBAG_H
#define PC_RANDO_KEYBAG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The five new text commands (group 0, codes 0x4B-0x4F) and the two messages wwrando changes. */
#define PCR_KEYBAG_CMD_FIRST 0x4B /* DRC */
#define PCR_KEYBAG_CMD_LAST 0x4F  /* WT */
#define PCR_KEYBAG_MSG_NAME 403   /* the Pirate's Charm's name */
#define PCR_KEYBAG_MSG_DESC 603   /* its description (name + 200, dMenu_Item_c::itemnoteSet) */

/* The dungeon's save stage number (3 DRC .. 7 WT) for text command `code`, or -1 when `code` is not
   one of the five (exec_curr_num_keys_text_command: code - 0x48). */
int pc_rando_keybag_stage(int code);

/* Where the key count is read from: 1 when the player is in that dungeon (`cur_stage` == `stage`; the
   loaded stage memory at 0x803C5380), 0 for the dungeon's saved stage info (0x803C4F88 + stage * 0x24). */
int pc_rando_keybag_reads_current(int cur_stage, int stage);

/* The expansion: `keys` in decimal (fopMsgM_int_to_char, USA digits) and " +Big" when `has_big_key`,
   else five spaces. Writes it NUL-terminated and returns its length, or -1 (then out[0] = 0 when
   cap > 0) when it does not fit or keys is negative. */
int pc_rando_keybag_format(int keys, int has_big_key, char* out, size_t cap);

/* The two texts, in wwrando's string syntax (pc_rando_msg.h). */
const char* pc_rando_keybag_name_text(void);
const char* pc_rando_keybag_desc_text(void);

/* Puts both texts in the message overlay (pc_rando_msg_set_text: the BMG entries are kept). Returns 1
   when both were set, else 0. */
int pc_rando_keybag_set_messages(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_KEYBAG_H */
