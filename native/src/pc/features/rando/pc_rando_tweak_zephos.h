/*
 * pc_rando_tweak_zephos.h - Zephos and the auction intro (docs/RANDOMIZER_PLAN.md step R3.19): the
 * event_list.dat edits of two tweaks.py functions that wwrando always applies, made in memory through
 * the archive-load hook (pc_rando_resources.h, step R0.10) while pc_rando() is 1:
 *
 *   shorten_zephos_event         sea Stage.arc, event TACT_HT (the Wind Shrine): actors Hr and Link
 *                                keep actions[0:7], CAMERA actions[0:5]; the event's ending flags
 *                                become the flags those three last actions set. The event ends when
 *                                Link gets the shrine's item (Link's 059get_dance, the location that
 *                                step R2.4 places an item in), before Zephos appears.
 *   shorten_auction_intro_event  Orichh Stage.arc, event AUCTION_START: CAMERA's actions[3] (the
 *                                200-frame UNITRANS pan) and actions[4] (the 30-frame PAUSE after it)
 *                                are removed. The 30-frame PAUSE before the pan stays: wwrando notes
 *                                that the auction's timer actor must finish its asynchronous creation
 *                                before the intro ends.
 *
 * The third part of the step, Zephos's hard-coded Wind's Requiem in daNpc_Hr_c::demoProcTact0
 * (flexible_item_locations.asm d_a_npc_hr 0x1164), is a hunk in game/src/d/actor/d_a_npc_hr.cpp
 * (docs/RANDO_HOOKS.md, R3.19).
 *
 * Each edit first checks the vanilla shape (D9): the actor's action count and the names of the
 * actions it keeps last / removes. On a mismatch (replaced content files) it logs and leaves the
 * event vanilla. The library's chain relinks leave the dropped actions in the file, unlinked.
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_tweak_zephos_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_ZEPHOS_H
#define PC_RANDO_TWEAK_ZEPHOS_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the .dat patcher. Idempotent. */
void pc_rando_tweak_zephos_init(void);
/* The patcher (registered by init; exposed for tests and smokes). */
void pc_rando_tweak_zephos_patch(PcRandoResFile* file);

/* The two edits, applied to an opened raw event_list.dat (pc_rando_event.h's PcRandoEvl, passed as
   void* so this header needs no C++). Return PCR_EVL_OK, or the PcRandoEvlResult that refused it
   (PCR_EVL_MISMATCH: not the vanilla shape); on an error nothing is written. */
int pc_rando_tweak_zephos_shorten_tact_ht(void* evl);
int pc_rando_tweak_zephos_shorten_auction(void* evl);

enum PcRandoZephosEdit {
    PCR_ZEPHOS_TACT_HT = 0, /* sea TACT_HT */
    PCR_ZEPHOS_AUCTION = 1, /* Orichh AUCTION_START */
    PCR_ZEPHOS_EDIT_COUNT = 2,
};

/* What the patcher did with each edit since boot (or the last reset), for smokes and logs. */
typedef struct PcRandoZephosStatus {
    uint32_t seen;   /* files of that stage's Stage.arc event_list.dat dispatched */
    uint32_t applied; /* edits that returned PCR_EVL_OK */
    int last;        /* the last result (PcRandoEvlResult), -1 before the first file */
} PcRandoZephosStatus;
void pc_rando_tweak_zephos_status(int edit, PcRandoZephosStatus* out);
void pc_rando_tweak_zephos_reset_status(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_ZEPHOS_H */
