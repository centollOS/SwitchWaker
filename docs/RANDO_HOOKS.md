# Randomizer hook log

Every hunk the built-in randomizer adds to `game/` (docs/RANDOMIZER_PLAN.md, "Conventions every step
follows"). A randomizer step appends its own section `## R<phase>.<n>` here, in plan order, in the same
commit as its hunks; on a merge keep both sides.

Rules for a hunk (checked by `python3 -I native/tools/rando_hook_census.py`, which `native/tools/regress.sh`
runs):

- It is on the host side of a `#if TARGET_PC` guard, with the original code in the `#else` where the
  hunk replaces something, and takes the original branch when `pc_rando()` is 0. Guard forms:
  `?:` (`pc_rando() ? new : original`), `||` (`pc_rando() || original`), `if/else`
  (`if (pc_rando()) {new} else {original}`), `add` (new code under `if (pc_rando())` with nothing
  replaced), `include` (`#include "pc/pc_rando.h"`; needs no row).
- The guarded branch holds a marker comment
  `// pc_rando(<step>): <wwrando ref, e.g. flexible_item_locations.asm d_a_bigelf 0x217C>; original: <value>`.
- Each file with a marker of a step has a row in that step's section. One row per function; the
  census matches the first cell (the file path from the repository root, an optional `:line` is
  ignored) and lists stale rows. A step without hunks in `game/` writes a single row whose first cell
  is `-`.
- The event bits wwrando reuses (0x6901-0x6980, 0x6A01-0x6A40; plan D17) appear only in these hunks
  and in the randomizer's own files.

Columns: file, function, guard form, wwrando reference.

## R0.1

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: the switch (`native/include/pc/pc_rando.h`), the `rando` smoke and the test plumbing | - | - |

## R0.8

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_item.cpp | `execItemGet`: `pc_rando_exec_item_get` (override registry `pc_rando_item_override`, else `item_func_ptr`) | add | tweaks.py `make_items_progressive`, `allow_dungeon_items_to_appear_anywhere`, `update_tingle_statue_item_get_funcs` (`item_func_ptr` writes) |
| game/src/d/d_item.cpp | `checkItemGet`: `pc_rando_check_item_get` (override registry `pc_rando_item_getcheck_override`, else `item_getcheck_func_ptr`) | add | `item_getcheck_func_ptr` counterpart of the same redirects |

## R0.12

The message overlay (`native/src/pc/features/rando/pc_rando_msg.h`, plan D10). Hook bodies:
`pc_rando_msg_hook_text` and `pc_rando_msg_hook_entry` (`native/include/pc/pc_rando_hooks.h`), in the
pure `pc_rando_msg.cpp`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/f_op/f_op_msg_mng.cpp:1509 | `fopMsgM_msgGet_c::getMesgEntry` (1509), `fopMsgM_itemMsgGet_c::getMesgEntry` (1601): the overlay's INF1 entry fields (or the new-message defaults) replace the BMG entry before the instant-text QoL | add | gclib `bmg.py` `add_new_message` / `Message` fields, set in `tweaks.py` and `randomizers/hints.py` |
| game/src/f_op/f_op_msg_mng.cpp:1530 | `fopMsgM_msgGet_c::getMessage` (1530), `fopMsgM_itemMsgGet_c::getMessage` (1621): the overlay's encoded text replaces the BMG text (and serves numbers the BMG lacks; `mResMsgNo` = the asked number) | add | `msg.string = ...` / `add_new_message` (`tweaks.py`, `randomizers/hints.py`, `randomizers/boss_reqs.py`) |

## R3.7

The custom actors SwOp (0x1F6) and DngSw (0x1F7). New port-only units and registrations, each inside
`#if TARGET_PC` and unconditional: vanilla stage data never names `SwOp` or `DngSw`, so they are only
created from randomizer data, and a game that is not randomized never creates one. SwOp is written
from the disassembly of wwrando's binary REL (`ref/wwrando/asm/d_a_switch_op.rel`, read with capstone
and the GZLE01 symbol map) and keeps its quirks. The profile list entries are in
`native/include/pc/game_hooks/f_pc_profile_lst.{h,inc}` (port code, no marker).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_switch_op.cpp | new unit: `daSwitchOp_c`, `g_profile_SWITCH_OP` | add (whole file under `#if TARGET_PC`) | `asm/d_a_switch_op.rel` (binary; tweaks.py `add_custom_actor_rels`) |
| game/src/d/actor/d_a_dungeon_flag_sw.cpp | new unit: `daDungeonFlagSw_c`, `g_profile_DUNGEON_FLAG_SW` | add (whole file under `#if TARGET_PC`) | `asm/actors/d_a_dungeon_flag_sw.cpp` |
| game/include/f_pc/f_pc_name.h | `fpcNm_SWITCH_OP_e`, `fpcNm_DUNGEON_FLAG_SW_e` after `fpcNm_GAMEOVER_e` | add | `asm/patches/custom_data.asm` `custom_DynamicNameTable` |
| game/src/d/d_stage.cpp | `l_objectName`: `SwOp`, `DngSw` before `EndCode` | add | `asm/patches/custom_data.asm` `custom_l_objectName` |

## R3.8

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/include/d/actor/d_a_tbox.h | `daTbox_c::getIsTrap` (new inline: param 0x40 while pc_rando(); unused until R6.5) | add | asm/actors/d_a_tbox.cpp `getIsTrap` |
| game/src/d/actor/d_a_tbox.cpp | `commonShapeSet` (open bck; stands for `l_modelInfo[1..3].openBckId` 0x9) | `?:` | asm/actors/d_a_tbox.cpp `l_modelInfo` |
| game/src/d/actor/d_a_tbox.cpp | `getFuncType` (mask 0x3F) | `?:` | asm/actors/d_a_tbox.cpp `getFuncType`/`getIsTrap` |
| game/src/d/actor/d_a_tbox.cpp | `actionOpenWait` (no big chest BGM/lights; event `DEFAULT_TREASURE_A` for every chest) | `if/else`, `\|\|` | asm/actors/d_a_tbox.cpp `actionOpenWait` |

## R3.9

Nonlinear A: `make_game_nonlinear.asm` items 1-11 (asm report G6). REL offsets are file offsets
(`.text` starts at 0xDC-0xF4 in these RELs); each was checked against the disc's code.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_ship.cpp | `daShip_c::checkOutRange` | `\|\|` (with `pc_qol(PC_QOL_FREE_BOAT)`) | d_a_ship 0x29EC/0x2A08/0x2A24 |
| game/src/d/actor/d_a_ship.cpp | `daShip_c::execute` | `?:` (boarding: `isGetItem(1, 0)` for event 0x0908), `\|\|` (0xB2D8 with the QoL) | d_a_ship 0xB29C, 0xB2D8 |
| game/src/d/actor/d_a_npc_so.cpp | `daNpc_So_c::_create` | `\|\|` (`!pc_rando() &&`) | d_a_npc_so 0x3FD8 |
| game/src/d/actor/d_a_npc_md.cpp | `daNpc_Md_c::create` | `\|\|` (sword check), `add` (`pc_rando_reset_partner_position(2)`, then setRestartOption always) | d_a_npc_md 0xA24, 0xD94/0xDB4 `reset_medli_position` |
| game/src/d/actor/d_a_npc_cb1.cpp | `daNpc_Cb1_c::create` | `\|\|` (sword check), `add` (`pc_rando_reset_partner_position(1)`, then setRestartOption always) | d_a_npc_cb1 0x640, 0x7B4/0x7D4 `reset_makar_position` |
| game/src/d/d_menu_dmap.cpp | `dMenu_Dmap_c::treasureSet` | `\|\|` | 0x801A9A6C/0x801A9AA8 |
| game/src/d/actor/d_a_obj_ftree.cpp | `daObjFtree::Act_c::set_first_stat` | `\|\|` | d_a_obj_ftree 0xA4C |
| game/src/d/actor/d_a_npc_bj1.cpp | `init_BJ4_0`, `init_BJ6_0`, `init_BJX_0`, `init_BJX_1`, `getMsg_BJ7_0` | `\|\|` (Farore's Pearl counts as owned) | d_a_npc_bj1 0x784/0x830/0x984/0xA30/0x2200 |
| game/src/d/actor/d_a_warpdm20.cpp | `daWarpdm20_c::CreateInit` | `\|\|` (`!pc_rando() &&`: no FF3 bit 0x1820) | d_a_warpdm20 0x68C |
| game/src/d/d_item.cpp | `check_itemno` | `\|\|` (bait, spoils) | 0x800C7E58/0x800C7E84 |
| game/src/d/actor/d_a_obj_mknjd.cpp | `daObjMknjD::Act_c::Create`, `Execute` | `\|\|` (song counts as owned) | d_a_obj_mknjd 0x96C/0x205C/0x20D4 |
| game/src/d/d_com_inf_game.cpp | `dComIfGs_copy/set/rev/exchangePlayerRecollectionData` | `add` (return at once) | 0x80054CC0/0x80054E9C/0x80055318/0x80055580 |
| game/src/d/actor/d_a_tag_ret.cpp | `daTagRet::Act_c::_create` | `\|\|` (`!pc_rando() &&`) | d_a_tag_ret 0x22C |
| game/src/d/actor/d_a_obj_toripost.cpp | `daObjTpost_c::_execute` | `\|\|` | d_a_obj_toripost 0x187C |

## R3.10

Nonlinear B: `make_game_nonlinear.asm` items 12-22 (asm report G6). Smoke `nonlinB`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_mo2.cpp | `daMo2_Create`, `daMo2_Execute` (sword checks, `rouya_mode`) | `\|\|` | make_game_nonlinear.asm d_a_mo2 0xBF2C, 0xAD70 |
| game/src/d/actor/d_a_obj_akabe.cpp | `daObjAkabe::Act_c::chk_appear` | `?:` | make_game_nonlinear.asm d_a_obj_akabe 0x650 |
| game/src/d/actor/d_a_kb.cpp | `pl_attack_hit_check`, `carry_move` | `if/else` (`!pc_rando() &&`), `\|\|` | make_game_nonlinear.asm d_a_kb 0x1460, 0x3C1C |
| game/src/d/d_menu_fmap.cpp | `dMenu_Fmap_c::checkMarkCheck1`, `checkMarkCheck2` | `add` (return) | make_game_nonlinear.asm 0x801B14C4, 0x801B1684 |
| game/src/d/actor/d_a_obj_ajav.cpp | `daObjAjav::Act_c::_create` | `\|\|` | make_game_nonlinear.asm d_a_obj_ajav 0x1568 |
| game/src/d/actor/d_a_obj_auzu.cpp | `daObjAuzu::Act_c::is_exist` | `\|\|` | make_game_nonlinear.asm d_a_obj_auzu 0x580 |
| game/src/d/actor/d_a_npc_co1.cpp | `daNpc_Co1_c::init_CO1_0` | `?:` | make_game_nonlinear.asm d_a_npc_co1 0x498 |
| game/src/d/actor/d_a_obj_eayogn.cpp | `daObjEayogn_c::check_ev_bit` | `?:` | make_game_nonlinear.asm d_a_obj_eayogn 0x40C |
| game/src/d/d_com_inf_game.cpp | `dComIfG_play_c::getLayerNo` (Hyrule) | `add` (return) | make_game_nonlinear.asm 0x80052668 |
| game/src/d/actor/d_a_npc_bms1.cpp | `shop_talk`, `CreateInit` (2), `_create` (2), `CreateHeap`: the six `checkItemGet(0x69, 1)` through a unit-local macro | `?:` | make_game_nonlinear.asm d_a_npc_bms1 0x1B9C, 0x2064, 0x20B4, 0x36A0, 0x36EC, 0x3E3C |
| game/src/d/actor/d_a_npc_de1.cpp | `daNpc_De1_c::getMsg` | `add` (return) | make_game_nonlinear.asm d_a_npc_de1 0xDB0 |
| game/src/d/actor/d_a_npc_bm1.cpp | `daNpc_Bm1_c::init_BMD_1` | `if/else` (`!pc_rando() &&`) | make_game_nonlinear.asm d_a_npc_bm1 0x1150 |
| game/src/d/actor/d_a_npc_os.cpp | `eventOrderCheck`, `finish02NpcAction`, `execute` | `if/else` (`!pc_rando() &&`), `\|\|`, `add` | make_game_nonlinear.asm d_a_npc_os 0x12E8, 0x1F74, 0x6000 |
| game/src/d/actor/d_a_obj_hsehi1.cpp | `daObj_hsh_c::actionDeleteEvent` | `add` | make_game_nonlinear.asm d_a_obj_hsehi1 0x1E60 |
| game/src/d/d_door.cpp | `dDoor_hkyo_c::proc` (west and north doors) | `if/else` (`!pc_rando() &&`) | make_game_nonlinear.asm 0x8006D618, 0x8006D674 |

## R3.11

Cutscene removal (wwrando `remove_cutscenes.asm`, plus the Killer Bees' Picto Box hint and the
item-get music of `flexible_item_locations.asm`). Hook bodies: `native/src/pc/game_hooks/pc_rando_hooks_cutscenes.cpp`.
Smoke: `COS_RANDO_SMOKE=cutscenes`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_warpf.cpp | `daWarpf_c::checkEndDemo` | add (`return TRUE`) | remove_cutscenes.asm d_a_warpf 0xC3C |
| game/src/d/actor/d_a_obj_doguu.cpp | `daObjDoguu_c::_execute` (case 9) | if/else | remove_cutscenes.asm d_a_obj_doguu 0x267C |
| game/src/d/actor/d_a_warpdm20.cpp | `daWarpdm20_c::CreateInit`, `normal_execute` | `?:` (`pc_rando_hyrule_warp_unlocked`) | remove_cutscenes.asm d_a_warpdm20 0x634, 0xB50 |
| game/src/d/actor/d_a_obj_toripost.cpp | `daObjTpost_c::createInit` (Orca's letter) | `?:` | remove_cutscenes.asm d_a_obj_toripost 0x1B0C/0x1B10 |
| game/src/d/d_kankyo_dayproc.inc | `dKankyo_DayProc` (Aryll's and Tingle's letters) | `?:` | remove_cutscenes.asm 0x80197ADC/0x80197AFC |
| game/src/d/d_item.cpp | `item_func_pearl1/2/3` | add (`pc_rando_place_pearl`) | remove_cutscenes.asm 0x800C43F4/4424/4454 |
| game/src/d/actor/d_a_npc_cb1.cpp | `daNpc_Cb1_c::init` | `\|\|` | remove_cutscenes.asm d_a_npc_cb1 0x80B8 |
| game/src/d/actor/d_a_bgn.cpp | `daBgn_Execute` | `?:` (layer -1) | remove_cutscenes.asm d_a_bgn 0xB1E0 |
| game/src/d/actor/d_a_npc_tt.cpp | `daNpc_Tt_c::demoInitPatten` | `?:` | remove_cutscenes.asm d_a_npc_tt 0xC68 |
| game/src/d/actor/d_a_npc_mk.cpp | `daNpc_Mk_c::visitTalkInit`, `visitSetEvent` (0x1208), `visit01` | `?:`, `!pc_rando() &&` | flexible_item_locations.asm d_a_npc_mk 0x2C2C/0x2D0C; remove_cutscenes.asm 0x2F80 |
| game/src/d/actor/d_a_npc_uk.cpp | `daNpc_Uk_c::visitTalkInit`, `visitSetEvent` (0x1208) | `?:` | flexible_item_locations.asm d_a_npc_uk 0x29B4/0x2AC8 |
| game/src/d/actor/d_a_npc_ho.cpp | `daNpc_Ho_c::getMsg`, `wait01` (0x1F80) | `?:` | remove_cutscenes.asm d_a_npc_ho 0xDB4/0x15FC |
| game/src/d/actor/d_a_player_dproc.inc | `daPy_lk_c::setGetItemSound` | `?:` (`pc_rando_item_get_bgm`) | remove_cutscenes.asm 0x8012E3A4; flexible_item_locations.asm 0x8012E3E8 |
| game/src/d/actor/d_a_player_tact.inc | `daPy_lk_c::procTactPlay` (QoL song replay) | `\|\|` | remove_cutscenes.asm 0x8014ECE0/0x8014EF28 |
| game/src/d/actor/d_a_ship.cpp | `daShip_c::procTactWarp_init`, `procTactWarp` (QoL Ballad) | `\|\|` | remove_cutscenes.asm d_a_ship 0x7680/0x7A10 |
| game/src/d/d_s_name.cpp | `dScnName_c::changeGameScene` (QoL intro) | `\|\|` | remove_cutscenes.asm 0x80232C78/0x80232C88 |

## R3.12

Vanilla bug fixes (`fix_vanilla_bugs.asm`) and the enemy death switches (`flexible_enemies.asm`).
The 11 fixes QoL item 15 already had are forced on under rando (`pc_rando() ||` before
`pc_qol(PC_QOL_FIX_BUGS)`; the hookshot sight and Orca fixes were always on and need no hunk). The 8
newer fixes are under `pc_rando() || pc_qol(PC_QOL_FIX_BUGS)` too, so vanilla players get them with
QoL item 15 (off by default). The `flexible_enemies.asm` parts are rando only. Guard `||+qol` below
means `pc_rando() || pc_qol(PC_QOL_FIX_BUGS)`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_item.cpp | `item_func_camera2` | `\|\|+qol` | fix_vanilla_bugs.asm deluxe_picto_box_item_func_fix_equipped_picto_box (main.dol 0x800C3420) |
| game/src/d/actor/d_a_pt.cpp | `daPt_Create` | `\|\|+qol` | fix_vanilla_bugs.asm miniblin_set_death_switch_when_light_arrowed (d_a_pt 0x4B44) |
| game/src/d/actor/d_a_pw.cpp | `Big_pow_down_check` | `\|\|+qol` | fix_vanilla_bugs.asm poe_fix_light_arrows_bug (d_a_pw 0x8CC/0x900) |
| game/src/d/actor/d_a_npc_bs1.cpp | `daNpc_Bs1_c::getMsg` | `\|\|+qol` | fix_vanilla_bugs.asm d_a_npc_bs1 0x214C |
| game/src/d/actor/d_a_st.cpp | `daSt_Execute` (lower body, 0x85CC: `\|\|+qol`; fall death, 0x845C) | `\|\|+qol`, `if/else` | fix_vanilla_bugs.asm stalfos_kill_lower_body_when_upper_body_light_arrowed; flexible_enemies.asm stalfos_set_death_switch (d_a_st 0x845C) |
| game/src/d/actor/d_a_npc_ba1.cpp | `daNpc_Ba1_c::next_msgStatus` | `\|\|+qol` | fix_vanilla_bugs.asm d_a_npc_ba1 0x16DC |
| game/src/d/actor/d_a_fganon.cpp | `start` | `\|\|+qol` | fix_vanilla_bugs.asm phantom_ganon_check_link_within_y_diff (d_a_fganon 0x4F50) |
| game/src/d/actor/d_a_mt.cpp | `damage_check` | `\|\|+qol` | fix_vanilla_bugs.asm magtail_respawn_when_head_light_arrowed (d_a_mt 0x6000) |
| game/src/d/actor/d_a_ks.cpp | `naraku_check` | `\|\|+qol` | fix_vanilla_bugs.asm d_a_ks 0x678 |
| game/src/d/d_s_play.cpp | `dScnPly_Delete` | `add` (`\|\|+qol`) | fix_vanilla_bugs.asm stop_sub_bgm_when_unloading_stage (main.dol 0x80235340) |
| game/src/d/actor/d_a_arrow.cpp | `daArrow_c::procMove` (callback), `daArrow_c::ShieldReflect` (phase 3) | `add`, `\|\|+qol` | fix_vanilla_bugs.asm zero_out_arrow_on_hit_callback (main.dol 0x800D6194), check_ganondorf_in_phase_3 (main.dol 0x800D4EBC) |
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::processTime` | `?:` (`\|\|+qol`) | fix_vanilla_bugs.asm main.dol 0x802ABEF8 |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::checkNextActionFromButton`, `daPy_lk_c::setShieldGuard` | `\|\|+qol` | fix_vanilla_bugs.asm check_can_defend (main.dol 0x8010E288, 0x8010E504) |
| game/src/d/actor/d_a_bdk.cpp | `landing` | `add` (`\|\|+qol`) | fix_vanilla_bugs.asm initialize_helmaroc_king_landing_timer, check_helmaroc_king_landing_timeout (d_a_bdk 0x24D4, 0x2AD0) |
| game/src/d/actor/d_a_boko.cpp | `daBoko_c::procCarry` | `if/else` (`\|\|+qol`) | fix_vanilla_bugs.asm d_a_boko 0x26A8 |
| game/src/d/actor/d_a_npc_people.cpp | `daNpcPeople_c::isUo1FdaiAll` | `if/else` (`\|\|+qol`) | fix_vanilla_bugs.asm sam_decorate_town_require_pedestals_to_exist (d_a_npc_people 0x9260) |
| game/src/d/actor/d_a_mo2.cpp | `daMo2_Execute` | `add` | flexible_enemies.asm moblin_set_death_switch (d_a_mo2 0xAB28) |
| game/src/d/actor/d_a_tn.cpp | `daTn_Execute` | `add` | flexible_enemies.asm darknut_set_death_switch (d_a_tn 0xB17C) |
| game/src/d/actor/d_a_rd.cpp | `daRd_c::_create`, `daRd_c::modeDeath` | `add` | flexible_enemies.asm redead_check_disable_spawn_switch, redead_set_death_switch (d_a_rd 0x4874, 0x1FBC) |
| game/src/d/actor/d_a_ph.cpp | `daPH_Create`, `dead_item`, `daPH_Execute`, `daPH_Draw` | `add` | flexible_enemies.asm peahat_check_disable_spawn_switch, peahat_set_death_switch, peahat_check_enable_spawn_switch(_for_draw) (d_a_ph 0x68B0, 0x4118, 0x5B2C, 0x358) |
| game/src/d/actor/d_a_nzg.cpp | `daNZG_Create` (group), `nzg_00_move` | `add`, `if/else` | flexible_enemies.asm g_profile_NZG 0xD08, rat_hole_all_rats_dead (d_a_nzg 0x1FC) |

## R4.2

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: the text input is port code (`features/rando/pc_rando_text.{h,cpp}`, `game_hooks/pc_rando_hooks_text.cpp`, `cos_switch_text_input*` in `switch/native/source/cos_switch.{h,cpp}`, small hooks in `native/src/pc/features/pc_menu.cpp`) | - | `wwr_ui` seed and permalink fields; `randomizer.py` `sanitize_seed`, `decode_permalink` |

## R6.15

Small modes (`native/include/pc/pc_rando_modes.h`). Hero mode is on while `pc_rando_cfg()->damage_multiplier`
is not 1.0; `remove_music` and `invert_sea_compass_x_axis` are `PcRandoModes` (`pc_rando_modes_set`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::setDamagePoint` | `?:` | `hero_mode.asm` 0x801102D0 `multiply_damage_amount` |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::checkRestHPAnime` | `add` | `remove_low_health_beep_anim.asm` 0x80111EE0 `remove_low_health_anim_at_full_health` |
| game/src/d/d_item.cpp | `getItemNoByLife` | `?:` | `hero_mode.asm` 0x800C7D4C nop |
| game/src/d/actor/d_a_disappear.cpp | `daDisappear_Execute` | `add` (+ `#else` original) | `hero_mode.asm` 0x8038B0C0 `ki_item_d` |
| game/src/d/actor/d_a_ki.cpp | `ki_fail_move` | `?:` | `hero_mode.asm` d_a_ki.rel 0x326C |
| game/src/d/actor/d_a_bl.cpp | `action_normal_skull` | `?:` | `hero_mode.asm` d_a_bl.rel 0x50C4 |
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::bgmStreamPlay`, `bgmStart`, `subBgmStart`, `bgmNowBattle` | `add` (return at entry) | `remove_music.asm` 0x802A33D0, 0x802A34A4, 0x802A47B8, 0x802A4EB8 |
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::processHeartGaugeSound` | `add` (skips the 1-heart `seStart`) | `remove_low_health_beep_anim.asm` 0x802A2FA0 |
| game/src/d/d_meter.cpp | `dMeter_compassGetOnProc` | `?:` | `invert_sea_compass_x_axis.asm` 0x801FD37C |
| game/src/d/d_save.cpp | `dSv_player_config_c::init` | `?:` | `custom_funcs.asm` `option_targeting_mode` (`set_default_targeting_mode_to_switch`) |
