/*
 * pc_qol.h - the options menu's "Calidad de vida" tab (docs/QOL_PLAN.md, issue #4): BetterWW's and
 * wwrando's quality-of-life patches redone in the decompiled game, each behind its own setting.
 *
 * COS_QOL_INSTANT_TEXT=1     text boxes appear whole, without their waits; holding B skips text
 * COS_QOL_FAST_ROLL=1        faster roll
 * COS_QOL_FAST_BLOCKS=1      faster block pushing and pulling
 * COS_QOL_FAST_GRAPPLE=1     faster grappling hook
 * COS_QOL_FAST_CLIMB=1       faster crawling, climbing, sidling and talk camera
 * COS_QOL_FAST_BALLAD=1      the Ballad of Gales warps without the boat's rise into the cyclone
 * COS_QOL_REVEAL_CHART=1     the sea chart shows every sector (the save is not written)
 * COS_QOL_TINGLE_CHESTS=1    Tingle chests appear with any bomb, no Tingle Tuner
 * COS_QOL_SAIL=swift|brisk|wind  the sail behaves like HD's Swift Sail (or BetterWW's Brisk Sail);
 *                            wind: the original sail with the wind always behind the boat
 * COS_QOL_SWING_TURN=1       the main stick turns the player while swinging on a rope or the hook
 * COS_QOL_NO_SONG_REPLAY=1   no conducting replay after a song
 * COS_QOL_FREE_BOAT=1        the boat does not turn back at the story's limits (only the sea's edge)
 *                            and can be boarded whatever the wind
 * COS_QOL_SKIP_INTRO=1       a new game starts on Outset without the opening legend
 * COS_QOL_FIX_BUGS=1         fixes for bugs of the original game (docs/QOL_PLAN.md, "Bugs of the
 *                            original game")
 * COS_QOL_GANON=1           mod: the player moves like the Ganondorf boss (slow stalk, charging sword
 *                            lunges, heavy leaps and falls); the model stays Link's
 *
 * All off by default (0: the original game). Each value is read from the environment (the settings
 * file's value, pc_settings.h) the first time the game asks; the menu's apply function sets it
 * afterwards, so every option switches live. The game thread calls all of them.
 */
#ifndef PC_QOL_H
#define PC_QOL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcQol {
    PC_QOL_INSTANT_TEXT = 0,
    PC_QOL_FAST_ROLL,
    PC_QOL_FAST_BLOCKS,
    PC_QOL_FAST_GRAPPLE,
    PC_QOL_FAST_CLIMB,
    PC_QOL_FAST_BALLAD,
    PC_QOL_REVEAL_CHART,
    PC_QOL_TINGLE_CHESTS,
    PC_QOL_SAIL,
    PC_QOL_SWING_TURN,
    PC_QOL_NO_SONG_REPLAY,
    PC_QOL_FREE_BOAT,
    PC_QOL_SKIP_INTRO,
    PC_QOL_FIX_BUGS,
    PC_QOL_GANON,
    PC_QOLS
} PcQol;

/* PC_QOL_SAIL's values. */
#define PC_QOL_SAIL_SWIFT 1
#define PC_QOL_SAIL_BRISK 2
#define PC_QOL_SAIL_WIND 3 /* BetterWW's "normal sail controls wind": the wind only, no speed change */

/* The option's value: 0 off (the original game), else on (PC_QOL_SAIL: PC_QOL_SAIL_SWIFT/BRISK). */
int pc_qol(PcQol which);
/* PC_QOL_SAIL's speed part: PC_QOL_SAIL_SWIFT or PC_QOL_SAIL_BRISK, else 0 (off or the wind only). */
int pc_qol_fast_sail(void);
/* The Ganon mod's factors (1.0 when it is off): the walk/run top speed, the sword swings' forward
   lunge, and the gravity while airborne. Ganondorf (d_a_gnd.cpp) stalks at 5 and charges at 15, and
   falls with a gravity of -2. */
float pc_qol_ganon_walk(void);
float pc_qol_ganon_lunge(void);
float pc_qol_ganon_gravity(void);
/* The settings key of an option (COS_QOL_...). */
const char* pc_qol_key(PcQol which);
/* The options menu: a new value of the setting `key` ("0", "1", "swift", "brisk"); 0 if the key is
   not a QoL option. */
int pc_qol_set(const char* key, const char* value);

#ifdef __cplusplus
}
#endif

#endif /* PC_QOL_H */
