/*
 * pc_rando_modes.h - the randomizer's small modes (docs/RANDOMIZER_PLAN.md, step R6.15).
 *
 * Each mode is one wwrando option whose patch is a handful of instructions in main.dol or a REL. The
 * hunks in game/ ask the functions below, which all return the "off" answer while pc_rando() is 0,
 * so a file that is not randomized plays exactly as the original game.
 *
 *   hero mode            option hero_mode (permalink): tweaks.py enable_hero_mode applies
 *                        hero_mode.asm and writes damage_multiplier = 4.0. Here hero mode is on while
 *                        pc_rando() and pc_rando_cfg()->damage_multiplier != 1.0, so the config field is
 *                        the only switch (pc_rando_config_set_hero_mode writes it). Hunks:
 *                        daPy_lk_c::setDamagePoint (damage x multiplier), getItemNoByLife (hearts become
 *                        rupees at any health), daDisappear_Execute's ki_item_d (smoke clouds drop a
 *                        green rupee, not a heart), d_a_ki ki_fail_move (Puppet Ganon's Keese skip the
 *                        "2 hearts or less: heart" branch) and d_a_bl action_normal_skull (Jalhalla's
 *                        skulls drop a green rupee).
 *   remove_music         option remove_music (permalink): remove_music.asm makes JAIZelBasic::bgmStart,
 *                        bgmNowBattle, subBgmStart and bgmStreamPlay return at once.
 *   invert sea compass   option invert_sea_compass_x_axis (not in the permalink: a player setting):
 *                        invert_sea_compass_x_axis.asm drops the neg in dMeter_compassGetOnProc.
 *   switch targeting     option switch_targeting_mode (not in the permalink): the new file's attention
 *                        type (dSv_player_config_c::mAttentionType) is pc_rando_cfg()->targeting_mode
 *                        (custom_funcs.asm option_targeting_mode), 0 hold, 1 switch.
 *   low-health beep      tweaks.py set_starting_health applies remove_low_health_beep_anim.asm when the
 *                        starting health is under 8 quarter hearts: no 1-heart beep while the maximum
 *                        is under one full heart (JAIZelBasic::processHeartGaugeSound), and no
 *                        low-health animation unless at least half a heart is missing
 *                        (daPy_lk_c::checkRestHPAnime).
 *
 * remove_music and invert_sea_compass_x_axis have no field in PcRandoConfig (R0.1); they live here
 * (pc_rando_modes_set). Whoever activates a seed (R0.7/R4) sets them before pc_rando_set_active; they
 * are not reset on deactivate (they act only while pc_rando() is 1).
 *
 * Pure: no game headers, so cos_rando_core and the unit test link it. Game thread only.
 */
#ifndef PC_RANDO_MODES_H
#define PC_RANDO_MODES_H

#include <stdint.h>

#include "pc/pc_rando.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcRandoModes {
    uint8_t remove_music;              /* remove_music (0) */
    uint8_t invert_sea_compass_x_axis; /* invert_sea_compass_x_axis (0) */
} PcRandoModes;

/* hero_mode.asm's multiplier (tweaks.py enable_hero_mode). */
#define PC_RANDO_HERO_DAMAGE_MULTIPLIER 4.0f
/* tweaks.py set_starting_health: remove_low_health_beep_anim below this many quarter hearts. */
#define PC_RANDO_LOW_HEALTH_BEEP_MIN_START 8

void pc_rando_modes_defaults(PcRandoModes* modes);
/* Copies *modes (NULL: the defaults). */
void pc_rando_modes_set(const PcRandoModes* modes);
const PcRandoModes* pc_rando_modes(void);

/* Maps the hero_mode option onto a config: damage_multiplier 4.0 when on, 1.0 when off. */
void pc_rando_config_set_hero_mode(PcRandoConfig* cfg, int on);

/* 1 while pc_rando() and the seed's hero mode is on. */
int pc_rando_hero_mode(void);
/* setDamagePoint's amount: a negative amount (damage) times the multiplier in hero mode; else as is. */
float pc_rando_hero_damage(float amount);
/* An item a hero-mode drop gives: the heart (dItemNo_HEART_e 0x00) becomes a green rupee (0x01);
   any other item, or outside hero mode, is returned as is. */
uint8_t pc_rando_hero_drop(uint8_t item);

/* 1 while pc_rando() and remove_music. */
int pc_rando_remove_music(void);
/* 1 while pc_rando() and invert_sea_compass_x_axis. */
int pc_rando_invert_sea_compass(void);
/* The attention type a new file starts with: the seed's targeting mode under rando, else vanilla. */
uint8_t pc_rando_targeting_mode(uint8_t vanilla);

/* 1 while pc_rando() and the seed's starting health is under 8 quarter hearts. */
int pc_rando_low_health_patch(void);
/* processHeartGaugeSound: 1 when the beep for `life` quarter hearts is dropped (the 1-heart beep,
   life 3-4, while max_life is under 8). */
int pc_rando_mute_low_health_beep(int life, int max_life);
/* checkRestHPAnime: 1 when its "true" is turned to false (less than half a heart missing from the
   maximum rounded down to full hearts). */
int pc_rando_skip_low_health_anim(int life, int max_life);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_MODES_H */
