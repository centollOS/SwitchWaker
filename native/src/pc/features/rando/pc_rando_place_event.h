// pc_rando_place_event.h - placement of the 11 event item locations at archive load
// (docs/RANDOMIZER_PLAN.md step R2.4): the locations whose item is the prm0 of an action of a stage's
// event_list.dat (Stage.arc), wwrando's randomizers/items.py change_event_item:
//   Ojhous   Ji1_TeachSpRollCut  Orca, Give 10 Knight's Crests (046pget, Hurricane Spin 0xAA)
//   Orichh   Gp1_Get_Itm         Maggie's Father (011get_item)
//   Opub     Get_Mo3_Ltr         Cafe Bar Postman (011get_item)
//   sea      TACT_TT11           Tott (059get_dance, Song of Passing)
//   sea      get_empty_btl       Mila (011get_item)
//   sea      TACT_HT             Wind Shrine (059get_dance, Wind's Requiem)
//   sea      TACT1_RT            Cyclos (059get_dance, Ballad of Gales)
//   sea      BIGELF_ARRIVAL2     Big Octo Great Fairy (046pget, Magic Meter Upgrade)
//   Atorizk  Get_Rupee           Hoskit (011get_item)
//   Atorizk  GETMOTHERLETTER     Mail Sorting (011_get_item, as named on the disc)
//   Siren    hsehi1_tact         Command Melody tablet (059get_dance)
// Every one is rewritten, as wwrando does (the vanilla item too): the action becomes "059get_dance"
// with prm0 = item - 0x6D for a song (0x6D-0x72), else "011get_item" with prm0 = item. So the two
// 046pget actions (Orca, Big Octo fairy) always become 011get_item.
//
// A res patcher (pc_rando_resources.h, PCR_RES_DAT) registered by pc_rando_place_event_init
// (pc_rando_patchers.def). For the event_list.dat of a stage's Stage archive it runs
// pc_rando_evl_change_item (pc_rando_event.h) for each Event key of that stage with
// pc_rando_loc_item(loc, vanilla). D9: the action's vanilla name and prm0 are the fingerprint; on a
// mismatch the action is left vanilla and the mismatch is logged.
//
// Pure (cos_rando_core): no game headers; the results below are for the smokes and unit tests.
#ifndef PC_RANDO_PLACE_EVENT_H
#define PC_RANDO_PLACE_EVENT_H

#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the event patcher (pc_rando_patchers.def). */
void pc_rando_place_event_init(void);

/* The patcher itself (registered by the init; tests may call it directly). */
void pc_rando_place_event_patch(PcRandoResFile* file);

/* Per key (index into pcr::location_keys()), the result of the last load of its stage's event list. */
#define PCR_EVLOC_NOT_SEEN 0 /* not an Event key, or its event list was not dispatched yet */
#define PCR_EVLOC_PLACED 1   /* the action now gives pc_rando_place_event_item(key) */
#define PCR_EVLOC_FAILED 2   /* refused (fingerprint mismatch, event/actor/action missing): vanilla */
int pc_rando_place_event_result(int keyIndex);
/* For a FAILED key the pc_rando_event.h result (PCR_EVL_*), else PCR_EVL_OK. */
int pc_rando_place_event_error(int keyIndex);
/* The item written for a PLACED key (else 0xFF). */
uint8_t pc_rando_place_event_item(int keyIndex);
/* The global action index written for a PLACED key (else -1). */
int pc_rando_place_event_action(int keyIndex);
/* The number of Event keys (11). */
int pc_rando_place_event_key_count(void);
/* Forgets every result (smokes, tests). */
void pc_rando_place_event_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_PLACE_EVENT_H */
