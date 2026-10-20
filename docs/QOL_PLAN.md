# Quality-of-life options (BetterWW features)

Plan written 2026-10-12 from `main` ba8d0a3 for issue #4 ("Quality-of-life options (BetterWW
features)"; the scope is the checklist in the maintainer's comment). Line numbers are from that commit.
Items 13-16 (unrestricted boat, skip the intro, the original game's bug fixes, the wind-only sail) and
the BetterWW preset rows were added the same day on `feature/qol` after the merge with the European
build (`VERSION` 3): every hunk is the same in VERSION 2 and 3, or takes both branches (the hookshot).

BetterWW and wwrando patch `main.dol` and the RELs at fixed addresses, so a patched disc cannot run
here (SwitchWaker compiles the game from the decompilation and refuses a changed `main.dol`). Each
patch is redone in the game's C++ instead, behind a toggle of a new options-menu tab
**"Calidad de vida"** ("Quality of life").

## References

- BetterWW, [github.com/WideBoner/betterww](https://github.com/WideBoner/betterww) at `4501481`
  (2025-07-21): `randomizer.py` (which toggle applies what), `tweaks.py`, `asm/patches/*.asm`,
  `asm/patches/custom_funcs.asm`.
- wwrando, [github.com/LagoLunatic/wwrando](https://github.com/LagoLunatic/wwrando) at `9775811`
  (2026-07-20), the randomizer BetterWW is built on: the same tweaks, some newer.
- Both MIT, copyright 2018 LagoLunatic (BetterWW's additions by WideBoner). The code here is derived
  from their patches: each asm patch was read to find what it changes, and the same change, with its
  constants, is written in the decompiled function it patches. No asset of theirs is used. The MIT
  notice and the people they credit are in `THIRD_PARTY.md`.

**Credits:** BetterWW by WideBoner, built on LagoLunatic's wwrando (with brainfubar's fork, the Brisk
Sail designed by MelonSpeedruns, releases by JarheadHME); wwrando's helpers are listed in
`THIRD_PARTY.md`.

Addresses were mapped to functions with the decomp's `config/GZLE01/symbols.txt` (main.dol) and, for
REL offsets (file offsets in BetterWW), the REL's section table plus `config/GZLE01/rels/<rel>/
symbols.txt`; the split assembly (`build/GZLE01/asm`, `build/GZLE01/<rel>/asm`) shows the patched
instruction, matched to the source line below.

## Rules

- Every option is a `settings.ini` key (`COS_QOL_*`), **off by default**: a fresh install plays the
  original game. Off, every changed line takes its original branch (the original code stays in the
  `#else` of a `#if TARGET_PC` hunk, or the new condition is `pc_qol(...) || <original>` /
  `pc_qol(...) ? <new> : <original>`), so the off state is the original behaviour.
- All of them switch live (the menu's apply function sets the value the game reads); none needs a
  restart. A change applies from the next time the game reads it (e.g. the grappling hook's next
  throw, the next text box).
- `native/include/pc/pc_qol.h` / `native/src/pc/features/pc_qol.cpp`: `pc_qol(PcQol)` returns the
  option's value (0 = off). Hunks in `game/` are one-line calls or conditions, marked with a comment
  naming the option, the BetterWW/wwrando patch and the original value.
- Nothing here writes the save beyond what the game itself writes (see "Saves").

## The items

| # | Item (checklist) | Key | BetterWW / wwrando | Decomp change |
|---|---|---|---|---|
| 1 | Instant text boxes + hold B to skip text | `COS_QOL_INSTANT_TEXT` | `tweaks.make_all_text_instant` (every BMG message: initial draw type 1; wait commands `1A 07 00 00 07`, `..04` and `..03` removed, `..04` kept in messages 7726 and 2488) + `b_button_skips_text.asm` (7 B-trigger reads become B-held) | `fopMsgM_msgGet_c::getMesgEntry` / `fopMsgM_itemMsgGet_c::getMesgEntry` (`f_op_msg_mng.cpp:1505`, `1571`): the entry's `mDrawType` is 1. `fopMsgM_msgDataProc_c::stringSet` (`f_op_msg_mng.cpp:3873-3920`, the only parser of those codes): tags UNK03/UNK04/WAIT are skipped as if removed (UNK04 kept for 7726, 2488). B held instead of triggered in `dMsg_CHECK_TRIG_AB()` at the five patched uses (`d_msg.cpp`: `dMsg_stopProc` 2374, `dMsg_selectProc` 2431, `dMsg_continueProc` 2747, `dMsg_closewaitProc` 2923, `dMsg_finishProc` 2962) and `dMesg_outwaitProc` 2532 / `dMesg_closewaitProc` 2575 (`d_mesg.cpp`) |
| 2 | Faster rolling / movement speed | `COS_QOL_FAST_ROLL` | `tweaks.increase_player_movement_speeds`: roll speed `walk * 6/17 + 20` instead of `walk * 1.5 + 0.5` (`daPy_HIO_roll_c0::m` +0x18, +0x1C); its crawl part is item 5 | `daPy_lk_c::procFrontRoll_init` (`d_a_player_main.cpp:6837`, `6844`): the two roll constants |
| 3 | Faster block moving | `COS_QOL_FAST_BLOCKS` | `tweaks.increase_block_moving_animation`: push/pull animation 1.0 -> 1.4 (`daPy_HIO_pushpull_c0::m` +0x8, +0x10); every block type's push/pull time 20 -> 12 frames (`daObjMovebox::Act_c::M_attr` m04, m0A) | `procPushMove_init` / `procPullMove_init` (`d_a_player_pushpull.inc:129`, `172`); `daObjMovebox::Act_c::mode_wait` (`d_a_obj_movebox.cpp:1602-1606`). **Not** `dProcPushMove_init` (`d_a_player_dproc.inc:1507`, the event push of doors): the shared constant made Forsaken Fortress's big door softlock in wwrando (`fix_forsaken_fortress_door_softlock` patches the room's collision); here the door push keeps the original speed and no stage data is touched |
| 4 | Faster grappling hook | `COS_QOL_FAST_GRAPPLE` | `tweaks.increase_grapple_animation_speed` | `new_himo2_move` (`d_a_himo2.cpp`): throw speed 20 -> 40 (1094), first-person extension 40 -> 20 frames (982), third-person 20 -> 10 (1051), fall onto the target 10 -> 20 (1364), wrap speed constant 17 -> 25 (1367), wrap counter +1 -> +6 (1371) |
| 5 | Faster crawling, climbing and NPC chat zoom | `COS_QOL_FAST_CLIMB` | `tweaks.increase_misc_animations` (climb start 0.8 -> 1.6, ladder/vine climb 1.2 -> 1.6, start 1.0 -> 1.6, end 0.9 -> 1.4, sidle 1.6 -> 2.0, talk camera 20 -> 10 frames) + the crawl part of `increase_player_movement_speeds` (crawl 3.0 -> 6.0) | `d_a_player_hang.inc:604` (`mWallCatch` +0x1C), `d_a_player_ladder.inc:19` (+0x24), `:93` and `d_a_player_climb.inc:213` (+0x4), `d_a_player_ladder.inc:175` (+0xC), `d_a_player_whide.inc:27` (`mWall` +0x14), `d_a_player_crawl.inc:34` (`mCrouch` +0x3C); `dCamera_c::talktoCamera` (`d_camera.cpp:4012`) |
| 6 | Faster Ballad of Gales (no warp cutscene) | `COS_QOL_FAST_BALLAD` | `ballad.asm` (BetterWW): no wait for the boat to rise 5000 units before the warp, no warp music | `daShip_c::procTactWarp` (`d_a_ship.cpp`, the `m03F4 + 5000.0f` check), `daShip_c::procTactWarp_init` (`fopAcM_seStartCurrent(this, 0x186E, 0)`). The arrival (falling out of the cyclone) is kept, as in BetterWW (wwrando's `remove_ballad_of_gales_warp_in_cutscene` changes 49 rooms' spawn data and is commented out there) |
| 7 | ~~Remove the title / ending videos~~ (removed) | ~~`COS_QOL_SKIP_VIDEOS`~~ | `tweaks.remove_title_and_ending_videos` | Removed: the port plays no THP video yet, so the option changed nothing (see "Not implemented"). Its BetterWW toggle's other parts are items 14 and 15 |
| 8 | Reveal the full sea chart | `COS_QOL_REVEAL_CHART` | `reveal_sea_chart.asm`: `dSv_player_map_c::init` sets bit 0 of all 49 sectors on a new game | See "Saves": the chart menus (`d_menu_fmap.cpp`, `d_menu_fmap2.cpp`) read every sector as charted while the option is on; the save is not written |
| 9 | Tingle chests without the Tingle Tuner | `COS_QOL_TINGLE_CHESTS` | `tingle_chests_without_tuner.asm` (BetterWW: no GBA link checks, any bomb, Tingle Bomb or not; dungeon map and minimap show the chests with the compass) | `daAgbsw0_c::ExeSubT` (`d_a_agbsw0.cpp:1153-1157`); `dMenu_Dmap_c::treasureSet` (`d_menu_dmap.cpp`, the current floor's `tboxNo != 0xF && != 0x10`, the one check the patch removes), `dMap_c::drawPointGc` (`d_map.cpp:3319`) |
| 10 | Swift Sail / Brisk Sail (and item 16) | `COS_QOL_SAIL` (`0`, `swift`, `brisk`, `wind`) | `swift_sail.asm`, `brisk_sail.asm`, `tweaks.make_sail_behave_like_swift_sail` / `_brisk_sail`: wind always behind the boat (rounded to 45 degrees) while sailing; every 55.0 and 80.0 of `d_a_ship.rel` doubled (sailing and initial speed); A-stop deceleration 1.0/0.1 -> 2.0/0.2 (brisk 3.0/0.2); idle deceleration 1.0/0.05 -> 2.0/0.1 (brisk 4.0/0.2); R cruise 10 -> 30; `decrementShipSpeed` 0.1/0.015 -> 0.2/0.03; `firstDecrementShipSpeed` 5.0/1.0 -> 10.0/2.0 | `daShip_c` (`d_a_ship.cpp`): `execute` (4353, after `mDoAud_setShipSailState(1)`: `dKyw_tact_wind_set`), `setSelfMove`, `procWait`, `decrementShipSpeed`, `firstDecrementShipSpeed`, and the `55.0f` / `80.0f` literals (one pooled constant each in the REL: 945, 1148, 1657, 1661-1662, 2274, 2605, 2646, 2648, 3808, 4101, 4275). The new sail textures, icon and the item name "Swift Sail" are BetterWW assets: not done (the sail looks and is named as in the original) |
| 11 | Turn while swinging or grappling | `COS_QOL_SWING_TURN` | `swing_turn.asm` + `custom_funcs.asm` `turn_while_swinging` (BetterWW): in `procRopeSwing`, while swinging, the main stick's X turns the player by `64 * x` units a frame (none under 25 %) | `daPy_lk_c::procRopeSwing` (`d_a_player_rope.inc`, the last `else` before `f30_3 > 0.6f`); the grappling hook's swing is the same procedure |
| 12 | No song replays | `COS_QOL_NO_SONG_REPLAY` | `song_no_replay.asm` (BetterWW): `procTactPlay` stops waiting for the "You conducted..." box to close and for the conducting animation to end | `daPy_lk_c::procTactPlay` (`d_a_player_tact.inc:605`, `666`) |
| 13 | Unrestricted boat | `COS_QOL_FREE_BOAT` | `KORL_control.asm` (d_a_ship.rel 0x29EC, 0x2A08, 0x2A24: the three story checks of `checkOutRange` made unconditional; 0xB2D8: the boarding check) | `daShip_c::checkOutRange` (`d_a_ship.cpp`): paths 0-2 of the sea stage count as passed (`pc_qol(...) && pathIndex <= 2`), path 3 and later (the edge) stay. `daShip_c::execute`: the boat offers "get in" (`fopAc_Attn_ACTION_SHIP_e`) whatever 0x0902 / 0x1980 / 0x0A80 and the wind say |
| 14 | Skip the intro | `COS_QOL_SKIP_INTRO` | `skipintro.asm` (main.dol 0x80232C78, 0x80232C88: `dScnOpen_c::execute` no longer runs the legend and changes to the game scene at once); part of BetterWW's "remove intro video" toggle | `dScnName_c::changeGameScene` (`d_s_name.cpp`): a new file requests the PLAY scene, as a loaded one does, instead of the OPEN scene. The legend scene sets no event bit and writes nothing (its proc only draws and starts its music), so nothing is to be set in its place. BetterWW's patch also cut the title's attract legend (OPEN2, the same scene class); here that one stays |
| 15 | Fixes of the original game's bugs | `COS_QOL_FIX_BUGS` (two fixes always on) | `fix_vanilla_bugs.asm` (part of BetterWW's "remove intro video" toggle) and the vanilla parts of `necessary_fixes.asm` / `misc_rando_features.asm` | Fix by fix in "Bugs of the original game" below |
| 16 | Normal sail, wind at your back | `COS_QOL_SAIL=wind` | `normal_sail_wind.asm` (BetterWW's `normal_sail2`: the swift/brisk sails' `set_wind_dir_to_ship_dir` at d_a_ship.rel 0xB9FC, without their speed changes). BetterWW's `randomizer.py` calls `tweaks.normal_sail_wind`, which its `tweaks.py` at `4501481` does not define; the asm patch is taken as the intent | Item 10's `dKyw_tact_wind_set` in `daShip_c::execute` for every non-zero value; the speed changes now ask `pc_qol_fast_sail()` (swift or brisk only) |
| - | BetterWW preset | (action rows) | BetterWW's UI defaults (`wwr_ui/randomizer_window.ui`): Swift Sail, instant text, faster roll / grapple / blocks / crawling, Tingle chests, no song replays, swing turn, fast Ballad, remove intro video (here items 14 and 15) on; invert camera, full sea chart, unrestricted boat, random enemy colours off | `pc_menu.cpp`: "Experiencia BetterWW" sets every QoL row to those values, "Valores originales" sets every row to 0, both through `pc_settings_set_mode` (saved, each row still editable; a row locked by the environment is kept). Invert camera stays with the menu's existing camera rows |

## Saves

- Item 8: BetterWW sets the 49 "charted" bits (`mFmapBits[i]` bit 0, `isSaveArriveGrid`) in
  `dSv_player_map_c::init`, i.e. only in new games, and they are then saved for good. Here the option
  does not write anything: while it is on, the two sea chart menus read every sector as charted;
  off, they show the save's own bits again. Sectors the player really reaches are still recorded as
  in the original. The other reader of those bits, the fishmen (`d_a_npc_so.cpp:625`, `1195`: a
  fishman offers to draw the sector only if it is not charted yet), keeps reading the save, so they
  still draw their sectors (with BetterWW they would not, since the bits are set).
- Item 10: the wind direction follows the boat through `dKyw_tact_wind_set`, the function the Wind's
  Requiem uses: like the song, it stores the direction in the save's wind fields (`dComIfGs_setWindX/Y`),
  which every save keeps anyway. Turning the option off leaves the last wind until the song changes it.
- Item 14 writes nothing either: the skipped legend scene never wrote the save (checked: the save
  data in memory on arriving at Outset is the same with and without it, see the results).
- Item 15's fixes change what the game itself saves only in the bugged cases (a Miniblin's switch set
  when a Light Arrow kills it, as any other death sets it).
- No other item touches save data.

## Notes and choices

- Item 1's B: BetterWW turns 7 reads of "B triggered" into "B held"; the other B checks (number
  input, `dMsg_outnowProc`, the instant-finish check with `mDrawType == 0`) are unchanged, as there.
- Item 5's crawl constant belongs to wwrando's "movement speeds" tweak; the checklist (and BetterWW's
  label) names crawling with climbing, so it is under item 5.
- Item 9: wwrando's newer version also lets any explosion hit (not only a bomb actor); BetterWW's
  (bombs, Tingle Bombs too) is followed. Only the current floor's check in `treasureSet` is removed,
  as in both patches; the floors above and below still hide Tingle chests.
- Item 10 doubles every use of the two pooled constants, as the REL patch does (the tornado pull,
  the turn rate's and the effects' speed scale, a demo's target speed), so the boat handles as with
  BetterWW. One use is left out: `setWaveAngle`'s `static cXyz local_left(80.0f, ...)`, initialised
  once on first use (a live option cannot change it) and paired with `local_right(-80.0f, ...)`, a
  different constant the patch leaves at -80: doubling only one side there is a side effect of the
  patch, not a sail behaviour.

## Unrestricted boat (item 13): what it can break

BetterWW (and wwrando, where it is always on) accept these; they are the player's choice here too, and
the row's help says that story places can be reached early.

- The three story paths keep the player in the sea's middle row until Dragon Roost's arrival (0x0902),
  out of the east until Farore's Pearl, and out of the Forsaken Fortress corner until the Master
  Sword. With the option the whole sea is open from the first boarding: islands can be reached before
  the story leads there, and their arrival events (`d_a_tag_island`) and the boat's story talks
  (`checkForceMessage`: the sail talk 0x5E0, the wind talk 0x5ED, the pearl talks) play out of their
  usual order, but each still plays when its own bits say so.
- Before the Wind's Requiem the wind cannot be turned: sailing upwind is slow (no softlock, the boat
  can always be paddled with the sail down).
- Places gated by items or story bits stay gated (e.g. the Tower of the Gods needs the three pearls,
  Hyrule the tower); only the boat's own limits go. The edge of the sea (path 3) still turns the boat
  back.
- Boarding: between Dragon Roost's arrival and the first boarding after the boat's wind talk, the
  original only lets the player in while the wind blows north (the talk sends them to get the song).
  With the option the boat can be boarded at once: leaving Dragon Roost before the Wind's Requiem is
  possible (the song can still be learnt later on the island).
- Nothing is written to the save by the option itself; turning it off restores the limits from the
  boat's next check (a boat already past a story limit is turned back toward it).

## Bugs of the original game (item 15)

Every fix in BetterWW's `fix_vanilla_bugs.asm`, and what `necessary_fixes.asm` and
`misc_rando_features.asm` apply that is not randomizer-only. None of them was already fixed by the
port (`docs/GAME_DIVERGENCE.md`, bugs B1-B39: those are host bugs, not the original's). The rule: a fix
is **always on** only if the bug crashes the port and the fix changes nothing in normal play; every
other one changes what the original does in some case, so it is behind the toggle (off by default).
In a randomized file every toggled fix is on whatever the toggle says (`pc_rando() ||`, randomizer
step R3.12, `docs/RANDO_HOOKS.md`), as wwrando always applies them; the rows marked R3.12 are
wwrando's newer fixes, added to the same toggle.

| Fix (BetterWW) | Decomp | Decision |
|---|---|---|
| Hookshot sight on the shards of Helmaroc King's mask (`hookshot_sight_failsafe_check`) | `daHookshot_rockLineCallback` (`d_a_hookshot.cpp`): `collided_actor` can be NULL; the GameCube read low memory (BetterWW: a crash), the host faults | **Always on**: no actor, no hook target. Both the USA and the PAL condition |
| Orca with no sword equipped | `daNpc_Ji1_c::battleAction`: the counter's sword icon comes from `getSelectEquip(0)` = 0xFF, an item with no texture; BetterWW: a crash | **Always on**: no counter without a sword (as BetterWW). Not reachable in normal play (the sword cannot be unequipped), but the debug travel and the sailing preset can bring a swordless file to Orca |
| Grandmother's soup without an empty bottle (message 2041) | `daNpc_Ba1_c::next_msgStatus`: 0x7F9 is not in the game's text; the port shows a stand-in (message 1) where the GameCube crashed (checked on the Mac: message 2041 shows message 1, 2037 shows) | Toggle: 0x7F5 (2037) either way |
| Beedle buys Blue Chu Jelly | `daNpc_Bs1_c::getMsg`: selling it can leave too little for Doctor Bandam's blue potion | Toggle: the "I don't buy that" answer (0xF75) |
| Deluxe Picto Box on a button | `item_func_camera2` (`d_item.cpp`): an X/Y/Z button keeps the Picto Box (0x23) until the item menu sets it again | Toggle: such a button now holds 0x26 |
| Morths falling out of the map | `naraku_check` (`d_a_ks.cpp`): only void ground (code 4) deletes them; with no ground under them they fall for ever (a room waiting for its enemies stays shut) | Toggle: no ground counts as void ground |
| Stalfos lower body after a Light Arrow | `daSt_Execute` (`d_a_st.cpp`): the lower body only dies with an upper body that died normally (`mCountdownToDeath`), so it becomes unkillable | Toggle: it starts its own Light Arrow death. (`necessary_fixes.asm`'s other answer, Stalfos immune to Light Arrows, is superseded by this one) |
| Miniblin switch after a Light Arrow | `daPt_Create`: `mEnemyIce.mDeathSwitch` is left 0, so a non-respawning Miniblin killed by a Light Arrow does not set its switch | Toggle: its switch is given to the enemy-ice death (as `d_a_tn`, `d_a_mt` do) |
| Jalhalla's Poes and Light Arrows | `Big_pow_down_check` (`d_a_pw.cpp`): a Poe dying to a Light Arrow keeps health > 0 and never counts for Jalhalla | Toggle: such a Poe counts; one that rejoins Jalhalla stops dying |
| Respawning Magtails and Light Arrows | `damage_check` (`d_a_mt.cpp`): a Light Arrow in the head does not set `m1CBC`, so the Magtail does not come back | Toggle |
| Phantom Ganon's trigger in the Forsaken Fortress | `start` (`d_a_fganon.cpp`): an XZ circle with no height limit | Toggle: not while the player is 1000 or more above him. Mostly a randomizer case (entrances), harmless in normal play |
| Sub-BGM after leaving a stage (wwrando, newer than BetterWW; randomizer step R3.12) | `dScnPly_Delete` (`d_s_play.cpp`): Mothula's and the Stalfos' miniboss music, or the Outset whirlpool's, outlive the stage | Toggle (forced on under rando): `mDoAud_subBgmStop()` when the stage unloads |
| Arrow hit callback (R3.12) | `daArrow_c::procMove`: an arrow stopped in an actor still takes hits; a second actor (Big Octo's eye) desyncs the hit actor from the joint index: NaN position, an assert | Toggle (forced on under rando): the callback is cleared when it stops in an actor |
| Sunrise tune (R3.12) | `JAIZelBasic::processTime`: 5:59 AM's tune cuts the playing BGM (a Big Octo's battle music) | Toggle (forced on under rando): started without stopping the BGM, as `processMorningToNormal` does |
| Skull Hammer guard (R3.12) | `daPy_lk_c::checkNextActionFromButton`, `setShieldGuard`: no defence without a shield | Toggle (forced on under rando): the hammer in hand defends too |
| Ganondorf's reflect event (R3.12) | `daArrow_c::ShieldReflect`: a Light Arrow reflected in phase 1 can clash with the event that knocks Zelda out | Toggle (forced on under rando): the reflect event (aim, sparkle) only at 25 health or less; the arrow still hurts him |
| Helmaroc King's landing (R3.12) | `landing` (`d_a_bdk.cpp`): stuck on the tower geometry while landing (after jumping back in as it closes), a softlock | Toggle (forced on under rando): after 10 seconds in landing substates 1-2 he flies up and tries again (the timer is in the upper half of his parameters, unused) |
| Enemy weapon after a chest (R3.12) | `daBoko_c::procCarry`: with no floor found, the weapon's Y was set to 50 (absolute; a typo for "+ 50") | Toggle (forced on under rando): it stays at the player's feet |
| Sam's "Decorate the Town" (R3.12) | `daNpcPeople_c::isUo1FdaiAll`: given the Delivery Bag outdoors, no pedestal has spawned and 0 == 0 counts as decorated | Toggle (forced on under rando): at least one pedestal is needed |
| Island LoD models' heap estimate, JKRHeap warnings (`d_a_lod_bg`) | A GameCube heap-fragmentation measure and debug-console noise | Not done: the port's heaps are host-sized (`docs/GAME_DIVERGENCE.md`), nothing to fix |
| `necessary_fixes.asm`, active parts | the new-game save init (`init_save_with_tweaks`: with BetterWW's settings it sets nothing), the void out zone before the Forbidden Woods kept after Farore's Pearl, the custom models' arc pointers, Stalfos immune to Light Arrows, the rainbow 500-rupee, custom text commands, cannons dying to one boomerang hit | Not done: randomizer design or cosmetic changes, not bugs of the original (the rainbow rupee and the one-hit cannons are listed under "Not implemented") |
| `misc_rando_features.asm`, active parts | warp message colours for a custom message, a ChuChu float moved for recolouring, the missing-particle warning text | Not done: only serve randomizer features |

## Verification (Mac)

Per item, a targeted run with the option off and on (`--env COS_QOL_...=1`), measured from the log
(`COS_TRACE=qol`: `native/src/pc/harness/smokes/pc_qol_probe.cpp`, one line per frame with the
player's position, speed and angle, the event, the message process's number and status, the camera
eye, the boat's speed, the A button's action, a switch and a chosen actor; it can also spawn actors,
show a message, grab a rope and order the Ballad of Gales warp) or shots; the regression
(`native/tools/regress.sh`) with every option off and with every option on; the Switch NRO built.

| # | Mac check | Off | On |
|---|---|---|---|
| 1 | event-sweep LinkRM (A presses): message 186's typing frames; event Ba1_Get_Itm | 654; 615 frames | 14; 395 frames |
| 1 | B held through the grandmother's talks (d_mesg: her tunic event; d_msg: talking to her after it) | both stop at the first wait | the event ends; the talk closes in 17 frames |
| 2 | roll from speedF 3.62 (A two frames into a run, Windfall) | 5.93 | 21.28 |
| 3 | push a spawned black box with R | a step every 24 frames | every 16 |
| 4 | grappling hook thrown into the air (tool sweep case) | ~19 units/frame, turns back at 486 | ~38, at 466 |
| 5 | crawl (R + stick); talk camera to the grandmother | ~5 units/frame; settles in 20 frames | ~10; 10 frames |
| 6 | Ballad of Gales warp ordered at frame 600 (sailing preset) | new PLAY scene at 885 | at 631 |
| 7 | (removed) title attract video | ends after 61 frames (no THP playback in the port yet) | the same |
| 8 | sea chart (D-pad up, sailing preset) | 3 sectors drawn | all 49 |
| 9 | a spawned Tingle trigger (type T, switch 0x20) and a normal bomb | switch stays off | set 2 frames after the bomb |
| 10 | sailing smoke; A held at full speed | 54.84 units/frame; -1/frame | 109.71 (swift, brisk); -2 (swift), -3 (brisk) |
| 11 | rope swing (Asoko), stick forward-right | angle stays 0x0000 | turns ~45 units/frame |
| 12 | Wind's Requiem played (C stick, 30 frames a beat) | message 1453 up 814-960, wind window at 962 | message closed at 815, window at 816 |
| 13 | sailing smoke with 0x0902 cleared (before Dragon Roost), sailing north | KoRL's talk at the path 0 limit, stopped after 22913 units: FAIL | into room 18, 96721 units: PASS |
| 13 | sailing smoke with 0x1980 and 0x0A80 cleared: off the boat, A next to it | no "get in" (attention flags 0): FAIL | flags 0x80, back on in 132 frames: PASS |
| 13 | `checkOutRange` at fixed points (probe, 0x0902 cleared, no Master Sword): Windfall's corner / middle of the sea / fortress corner / past the north edge / past the west edge | 0 / 1 / 1 / 1 / 1 | 0 / 0 / 0 / 1 / 1 |
| 14 | M14's new-game flow from a clean card (uncapped): the first frame with the player on Outset | 43623 (after the legend) | 1474 |
| 14 | the save data in memory 40 frames after that (1912 bytes) | - | the same but one byte: `mRandomSalvagePoint`, a `cM_rndF` roll at file creation (2 / 1) |
| 14 | M14 with the input retimed (`new-game-skip-intro.txt`) | - | Outset's intro event and its STB demo play, the player is free at frame 4449, M14 passes |
| 15 | Picto Box on X (`COS_BOOT_ITEMS=23`), Deluxe Picto Box given at frame 400 | X holds 0x23 | 0x26 |
| 15 | a Miniblin (type 1, switch 5) spawned on Outset, a Light Arrow hit (its enemy-ice timer started, as its damage check does) | switch 5 stays off | set as it vanishes (frame ~600) |
| 15 | a Morth spawned 4000 in front of the player in LinkRM (no ground under it) | still falling at y -9508 after 500 frames | deleted within 60 frames |
| 15 | message 2041 / 2037 shown | 2041 is not in the text: the port shows message 1 | (the toggle asks for 2037, which shows) |
| 16 | sailing smoke heading west with the wind set north (crosswind) | 41.2 units/frame | `wind` 54.8 (the wind turned behind, normal speed); `swift` 109.6 |
| - | `menu-qol-preset.txt`: "Experiencia BetterWW", then Voltereta off by hand | - | all 15 values as BetterWW's, saved; the hand change kept |
| - | `menu-qol-original.txt`: BetterWW, then "Valores originales" | - | every row 0, saved |

European build (`-DCOS_VERSION=3`, `build/native-mac-eur`, GZLP01): the item 13 sailing smoke with
0x0902 cleared fails off and passes on, M14 with item 14 passes (the player free at frame 4449),
`menu-qol-preset.txt` passes, and the Deluxe Picto Box check gives 0x26 on X with item 15 on.

Not run on the Mac (the cases need a whole game's progress): Beedle's Blue Chu Jelly, the Stalfos,
Jalhalla's and Magtails' Light Arrow fixes, Phantom Ganon's trigger, the hookshot on the mask shards
and Orca without a sword; each is a one-line change to the condition BetterWW patches, reviewed
against its assembly.

Live: `native/check/input/menu-qol.txt` (a regression target) turns Voltereta más rápida on from the
menu; the next roll, from speedF 3.45, starts at 21.22.

Regression targets added with items 13-16: the two sailing smokes of item 13 with the option on, M14
with item 14 on, and the two preset scripts.

Checkpoint after items 13-16 and the presets (`feature/qol`, 2026-10-12): `native/tools/regress.sh`
with every option off passes, 88 checks (the menu smokes options-menu, menu-qol, menu-qol-preset,
menu-qol-original, menu-sailing and menu-travel-save among them); the all-on run below was for items
1-12 and was not repeated (item 14 on changes the new-game flow M11/M14 check by design). The Switch NRO
(`scripts/switch/build_native.sh --runtime-assets`) builds. The probe's
call of `daShip_c::checkOutRange` (a REL symbol) is listed in `native/check/expected_unresolved_phase2.txt`.

Regression for items 1-12: `native/tools/regress.sh` passes with every option off (the default). With every option on
(`COS_QOL_*` in the environment, `COS_QOL_SAIL=swift`) all targets pass but two that the instant text
breaks by design: `msg-sweep` compares `getMesgEntry` with the file (the draw type is now 1), and
`telescope-demo`'s input script is timed to the original text speed. Both pass with the instant text
off and the other options on.

## Left for the console

- Every option once on the Switch (the menu tab's layout at 720p, a live switch of each row).
- Not reached by a Mac run: ladders, vines, ledges and sidling (item 5); the grappling hook wrapping
  around a target (item 4); a whole trip with the unrestricted boat (item 13); the bug fixes of item
  15 that need a game's progress;
  Tingle chests on the dungeon map and minimap with the compass (item 9); a Tingle statue chest in a
  real dungeon with a normal bomb (item 9); Forsaken Fortress's big door with item 3 on (it keeps the
  original door push, so no softlock is expected); the Brisk Sail's idle stop (item 10).

## Not implemented

What remains of BetterWW (`4501481`) after this branch, for a follow-up issue:

- **Remove the title and ending videos.** The option existed (`COS_QOL_SKIP_VIDEOS`) and was removed:
  the port plays no THP video yet. `daMP` (`d_a_movie_player.cpp`) is created for `title_loop.thp` (the
  title's attract video, after a long idle on the title) and `end_st_epilogue.thp` (the ending), but it
  carries its own copy of the SDK's THP decoder, whose Huffman and IDCT paths are PowerPC assembly
  (the host build keeps only `TODO(native phase 4)` stand-ins around them), so it appears nothing is
  decoded; the movie reports its frames as done after about 2 s of black
  and the scene goes on (`docs/NATIVE_PORT_PHASE4_6.md` step 4.18, "d_a_movie_player going through
  Aurora's THP", deferred). Aurora has a THP decoder (`lib/dolphin/thp/THPDec.cpp`, `THPAudio.cpp`,
  linked as `aurora::thp`), so playback looks feasible. Once videos play, the option comes back (BetterWW
  and wwrando replace both files with a blank video; here `dScnTitle_Execute` would take the movie as
  finished, as the removed hunk did).
- **Random enemy colours** (`randomize_enemy_palettes`): recolours enemy textures and particles in the
  disc's files; needs texture and particle colour changes at load time.
- **Custom player models** (`custom_player_model`, the casual-clothes choice, custom items and voice):
  replaces Link's model archives with community models; needs loading replacement archives from the SD.
- **Swift Sail looks**: its sail textures, item icon and the item name "Swift Sail" (BetterWW assets and
  text). Here the sail keeps its look and name; only the behaviour is done (item 10). Also BetterWW's
  "for HD texture pack" sail variants.
- **The rainbow 500-rupee** (`necessary_fixes.asm`, `check_animate_rainbow_rupee_color`) and **cannons
  dying to one boomerang hit** (`obj_canon` 0x7D0): small changes BetterWW applies always; could be rows.
- **The optional Gecko codes** BetterWW ships (by Ralf and wiiztec): a second set of equipped items
  (D-pad down to switch), Hero Mode (double damage, no heart drops), items don't disappear, unrestricted
  camera, no heart beeping, blur off.
- **Title screen logo and memory card banner** (`titlelogo`, `memorylogo`): BetterWW's own branding,
  not wanted here.
- **Invert camera** is not part of the preset: the menu's existing camera rows ("Invertir cámara
  horizontal / vertical") cover it.

