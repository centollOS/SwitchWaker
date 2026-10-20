/*
 * pc_rando_progressive.h - progressive items of the built-in randomizer (docs/RANDOMIZER_PLAN.md step
 * R3.3, asm group G2).
 *
 * wwrando (tweaks.py make_items_progressive, custom_funcs.asm convert_progressive_item_id and the 8
 * progressive_*_item_func) makes 8 item families progressive: whichever tier of a family the player
 * is given, the next tier from what the save already owns is the one that is given and shown.
 *
 *   family       ids registered            owned state read            state -> next tier
 *   sword        0x38 0x39 0x3A 0x3D 0x3E  collect[0] bits (0x803C4CBC) 0,1,3,7 -> 0x38,0x39,0x3A,0x3E
 *   shield       0x3B 0x3C                 collect[1] bits (0x803C4CBD) 0,1     -> 0x3B,0x3C
 *   bow          0x27 0x35 0x36            get-item bits of the bow slot 0,1,3 -> 0x27,0x35,0x36
 *   wallet       0xAB 0xAC                 wallet size (0x803C4C1A)     0,1     -> 0xAB,0xAC
 *   bomb bag     0xAD 0xAE                 max bombs                    30,60   -> 0xAD,0xAE
 *   quiver       0xAF 0xB0                 max arrows                   30,60   -> 0xAF,0xB0
 *   picto box    0x23 0x26                 get-item bits of the camera  0,1     -> 0x23,0x26
 *   magic meter  0xB1 0xB2                 max magic (0x803C4C1B)       0,16    -> 0xB1,0xB2
 *
 * Any other owned state (a full family, or a state the game cannot reach) gives the family's base id
 * from the conversion, and no tier from the item function: the asm does exactly this.
 *
 * This file is pure (no game header, no game call): the game side, which reads the save and calls
 * the vanilla item functions, is game_hooks/pc_rando_hooks_progressive.cpp.
 */
#ifndef PC_RANDO_PROGRESSIVE_H
#define PC_RANDO_PROGRESSIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcRandoProgFamily {
    PCR_PROG_SWORD = 0,
    PCR_PROG_SHIELD,
    PCR_PROG_BOW,
    PCR_PROG_WALLET,
    PCR_PROG_BOMB_BAG,
    PCR_PROG_QUIVER,
    PCR_PROG_PICTO_BOX,
    PCR_PROG_MAGIC_METER,
    PCR_PROG_COUNT
} PcRandoProgFamily;

/* The save bytes convert_progressive_item_id and the progressive item functions read. */
typedef struct PcRandoProgState {
    uint8_t swords;    /* dSv_player_collect_c::mCollect[0] */
    uint8_t shields;   /* dSv_player_collect_c::mCollect[1] */
    uint8_t bow;       /* dSv_player_get_item_c::mItemFlags[dInvSlot_BOW_e] */
    uint8_t wallet;    /* dSv_player_status_a_c::mWalletSize */
    uint8_t bomb_max;  /* dComIfGs_getBombMax() */
    uint8_t arrow_max; /* dComIfGs_getArrowMax() */
    uint8_t picto;     /* dSv_player_get_item_c::mItemFlags[dInvSlot_CAMERA_e] */
    uint8_t max_magic; /* dSv_player_status_a_c::mMaxMagic */
} PcRandoProgState;

/* Number of item ids that get a progressive item function (20: tweaks.py make_items_progressive). */
#define PCR_PROG_ID_COUNT 20

/* The family of `id`, or -1 when it is not a progressive id. */
int pc_rando_progressive_family(uint8_t id);
/* The `i`-th of the PCR_PROG_ID_COUNT progressive ids (tweaks.py order), or 0xFF out of range. */
uint8_t pc_rando_progressive_id(int i);
/* convert_progressive_item_id: the id of the tier the player gets next; `id` itself when it is not a
   progressive id. */
uint8_t pc_rando_progressive_convert(uint8_t id, const PcRandoProgState* st);
/* The tier (0-based) the family's progressive item function gives for `st`, or -1 when it gives
   nothing (the family is full or the state is not one of the table's). */
int pc_rando_progressive_tier(int family, const PcRandoProgState* st);
/* Number of tiers of a family (0 for a bad family). */
int pc_rando_progressive_tier_count(int family);
/* The item id of tier `tier` of a family, or 0xFF out of range. */
uint8_t pc_rando_progressive_tier_id(int family, int tier);

/* The message 101 + 0xB1 that tweaks.py adds for the normal magic meter (vanilla has none). */
#define PCR_PROG_MAGIC_MSG_NO (101 + 0xB1)
#define PCR_PROG_MAGIC_MSG_TEXT \
    "\\{1A 05 00 00 01}You got \\{1A 06 FF 00 00 01}magic power\\{1A 06 FF 00 00 00}!\nNow you can use magic items!"
/* Adds that message to the overlay (pc_rando_msg.h) as tweaks.py does: ITEM_GET box, slow initial
   draw, display item 0xB1. Returns PCR_MSG_OK or a PCR_MSG_ERR_* code. */
int pc_rando_progressive_add_messages(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_PROGRESSIVE_H */
