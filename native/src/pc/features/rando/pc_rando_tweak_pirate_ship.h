/*
 * pc_rando_tweak_pirate_ship.h - the pirate ship at Windfall (docs/RANDOMIZER_PLAN.md step R3.17,
 * tweaks report #9): wwrando 1.10.0 tweaks.py add_pirate_ship_to_windfall, made in memory through the
 * archive-load hook (pc_rando_resources.h, step R0.10) while pc_rando() is 1.
 *
 *   sea/Room11 room.dzr      the first ACT2 "Pirates" (the ship, layer 2) copied to the default
 *                            layer (ACTR) with pirate_ship_door_type (params & 0xFF00) 0, so its door
 *                            needs no password from Niko
 *   Asoko/Room0 room.dzr     the first "P2b" (Niko) of ACT2 and of ACT3 removed (his events go); ACTR
 *                            "Ls1" (Aryll, which_aryll 0, at (600, -550, -200), y_rot 0xC000); SCOB
 *                            "SW_C00" (switch 0xC3 while the player is in the chest room, behaviour 0,
 *                            prerequisite 0xFF, at (0, -550, -3900), scale 64/36/64); three ACTR
 *                            "SwOp" (step R3.7's switch operator, all continuous):
 *                              NOR  0xC0          -> 0xC2  (no countdown runs)
 *                              AND  0xC2, 0xC3    -> 0xC1  after 150 frames, starting EVNT
 *                                                          "AryllOpensDoor" (the player is trapped)
 *                              OR   0xC0, 0xC1    -> 0xC4  (the door should be open)
 *                            and the first "Ashut" (the chest room's door) of ACT2 and of ACT3 checks
 *                            switch 0xC4 instead of 0xC0 (params & 0xFF)
 *   Asoko/Stage stage.dzs    EVNT "AryllOpensDoor" appended (its index is the AND SwOp's evnt_index,
 *                            x_rot & 0xFF)
 *   Asoko/Stage event_list.dat  the new event "AryllOpensDoor" through the serializer (step R3.14):
 *                            CAMERA (staff type 2) FIXEDFRM Eye/Center/Fovy 60/Timer 30; Ls1 LOK_PLYER
 *                            prm_0 8, ANM_CHG 8, WAIT 30, TALK_MSG 849, ANM_CHG 4, TALK_MSG 850,
 *                            ANM_CHG 5; Link 001wait; ending flag 0 = Ls1's last action's flag
 *
 * Messages (the overlay of step R0.12, pc_rando_tweak_pirate_ship_messages): 3008 (Aryll's talk:
 * initial sound 106 "Haiiii~!", two parts built by construct_string_from_parts with the message
 * font), new 849 and 850 (dialog box, normal draw, text alignment 4).
 *
 * The game side (game_hooks/pc_rando_hooks_pirate_ship.cpp) has the rest: the message font and the
 * BMG's entry of 3008 for the messages, the stage BGM table entry (wwrando's main.dol writes at
 * 0x8039C30C + 0xC*4 + 3 and 0x8039C2E4 + 0xE*2: JAIZelBasic::m_scene_info[0xC] (Asoko) takes the
 * free second dynamic scene wave slot 0x0E, which loads IsleLink_0.aw (0x19), Aryll's voice), and
 * d_a_npc_ls1's facial animation of animation 4 (d_a_npc_ls1.rel 0x5D18 + 4*0x10 + 1 = 5).
 *
 * Plan D9: each file is checked first (the named actors and chunks are there, the EVNT name is free,
 * the event name is free). On a mismatch the whole file stays vanilla and it is logged; nothing is
 * written before every check of the file held.
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_PIRATE_SHIP_H
#define PC_RANDO_TWEAK_PIRATE_SHIP_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_msg.h"
#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The switches of the chest-room logic (tweaks.py). */
#define PCR_PS_SW_COUNTDOWN 0xC0        /* set by the rope game's countdown (ObjTime, vanilla) */
#define PCR_PS_SW_ARYLL_OPENED 0xC1     /* set by the AND SwOp once AryllOpensDoor ran */
#define PCR_PS_SW_NO_COUNTDOWN 0xC2     /* NOR of 0xC0 */
#define PCR_PS_SW_IN_CHEST_ROOM 0xC3    /* SW_C00 */
#define PCR_PS_SW_DOOR_OPEN 0xC4        /* OR of 0xC0, 0xC1: Ashut's switch */
#define PCR_PS_EVENT_NAME "AryllOpensDoor"
#define PCR_PS_ARYLL_DELAY 150          /* frames the player must be trapped before the event */
#define PCR_PS_MSG_ARYLL 3008
#define PCR_PS_MSG_STUCK 849
#define PCR_PS_MSG_OPEN 850
#define PCR_PS_ARYLL_SOUND 106
/* The audio table entries (JAIZelBasic::m_scene_info / m_dy_wave_set_2nd). */
#define PCR_PS_ASOKO_SCENE 0x0C
#define PCR_PS_SCENE_WAVE2 0x0E
#define PCR_PS_ISLE_LINK_WAVE 0x19
/* d_a_npc_ls1: animation 4 (ANMNUM_WAIT05_e) shows facial animation 5 (BTPNUM_NGWARAI_e). */
#define PCR_PS_LS1_ANIM 4
#define PCR_PS_LS1_FACE 5

/* Registers the dzx patcher and the event_list.dat patcher. Idempotent. */
void pc_rando_tweak_pirate_ship_init(void);
/* The patchers (registered by init; exposed for tests). Both do nothing unless pc_rando() is 1. */
void pc_rando_tweak_pirate_ship_patch_dzx(PcRandoResFile* file);
void pc_rando_tweak_pirate_ship_patch_event(PcRandoResFile* file);

/* The edit sites, one per file. */
enum PcRandoPirateShipSite {
    PCR_PS_WINDFALL = 0, /* sea/Room11: the ship on the default layer */
    PCR_PS_SHIP_ROOM,    /* Asoko/Room0: Niko out, Aryll, SW_C00, 3 SwOps, Ashut */
    PCR_PS_SHIP_STAGE,   /* Asoko/Stage stage.dzs: EVNT AryllOpensDoor */
    PCR_PS_SHIP_EVENTS,  /* Asoko/Stage event_list.dat: the event */
    PCR_PS_SITE_COUNT
};

typedef struct PcRandoPirateShipSiteInfo {
    const char* stage; /* "sea", "Asoko" */
    const char* arc;   /* "Room11", "Room0", "Stage" */
    const char* name;  /* "room.dzr", "stage.dzs", "event_list.dat" */
    const char* what;  /* for logs */
    int is_event;      /* the event_list.dat site (results are PCR_EVL_*, else PCR_DZX_*) */
    int runs;          /* times the patcher saw the file since the last reset */
    int result;        /* the last run's result: 0 applied, else why the file was left vanilla */
} PcRandoPirateShipSiteInfo;

const PcRandoPirateShipSiteInfo* pc_rando_tweak_pirate_ship_site(int site);
/* The last result's name ("ok", "mismatch", ...). */
const char* pc_rando_tweak_pirate_ship_result_name(int site);
/* Clears runs/result (tests, smokes). Keeps the EVNT index. */
void pc_rando_tweak_pirate_ship_reset(void);
/* The index of EVNT "AryllOpensDoor" in Asoko's stage.dzs: the one the stage patch appended, else
   the USA disc's (2, the vanilla EVNT count), which the room patch writes into the AND SwOp. */
int pc_rando_tweak_pirate_ship_evnt_index(void);

/* The 0x20-byte ACTR / 0x24-byte SCOB / 0x18-byte EVNT entries the tweak adds (disc format), for the
   patcher, the test and the smoke. `evntIndex` is the AND SwOp's event. */
void pc_rando_tweak_pirate_ship_aryll(uint8_t out[0x20]);
void pc_rando_tweak_pirate_ship_swc00(uint8_t out[0x24]);
/* which: 0 NOR, 1 AND (with the event and the delay), 2 OR. */
void pc_rando_tweak_pirate_ship_swop(int which, int evntIndex, uint8_t out[0x20]);
void pc_rando_tweak_pirate_ship_evnt(uint8_t out[0x18]);

/* Checks a patched file (the smoke, on the game's stored copy). 1 when every edit is there; else 0
   with the reason in `why` (may be NULL). */
int pc_rando_tweak_pirate_ship_verify(int site, const uint8_t* data, size_t size, char* why, size_t whyCap);

/* The text parts of message 3008 (wwrando syntax), for construct_string_from_parts. */
int pc_rando_tweak_pirate_ship_aryll_part_count(void);
const char* pc_rando_tweak_pirate_ship_aryll_part(int i);
/* The texts of 849 and 850 (msg.string, no wrap). */
const char* pc_rando_tweak_pirate_ship_new_text(uint16_t msg_no);

#ifdef __cplusplus
}

namespace pcr {
struct GlyphWidths;
}

/* Puts 3008, 849 and 850 into the message overlay. `vanilla3008` is the BMG's INF1 entry of 3008
   (its text box type sizes the wrap; only initial_sound changes); `widths` the message font's. Returns
   the number of messages set (3), or less when one failed (logged). */
int pc_rando_tweak_pirate_ship_messages(const pcr::GlyphWidths& widths, const PcRandoMsgEntry& vanilla3008);
#endif

#endif /* PC_RANDO_TWEAK_PIRATE_SHIP_H */
