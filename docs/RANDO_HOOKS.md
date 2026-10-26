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

## R0.7

Slot binding (plan D3, D4, D7). These hooks are not under `pc_rando()`: they decide it. With no rando
record in `mReserve` and no pending seed each one is a no-op, so a vanilla file is unchanged. Bodies in
`native/src/pc/game_hooks/pc_rando_hooks_slot.cpp`; the debug boot calls them from `pcBootStage`
(`native/src/pc/game_hooks/pc_logo_hooks.cpp`, outside `game/`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_s_name.cpp | `NameInMain`: `pc_rando_slot_new_game_begin` before `dComIfGs_init` (activates a pending seed, writes its sidecar), `pc_rando_slot_new_game_end` after it (writes the record, or clears a stale one) | add | none (wwrando patches the disc; plan D3/D4/D7) |
| game/src/d/d_s_name.cpp | `FileSelectMainNormal`: `pc_rando_slot_card_loaded` after `dComIfGs_setCardToMemory` | add | none (plan D7) |
| game/src/d/d_s_name.cpp | `dScnName_Create`: `pc_rando_slot_scene_entry` for the NAME scene (not NAMEEX) | add | none (plan D7) |
| game/src/d/d_s_title.cpp | `dScnTitle_Create`: `pc_rando_slot_scene_entry` for the TITLE scene (not ENDING) | add | none (plan D7) |

PAL: the file select load and the name entry are the same functions in `d_s_name.cpp` for every
version, so `game/src/pal/d/d_file_select.inc` needs no hunk.

## R0.8

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_item.cpp | `execItemGet`: `pc_rando_exec_item_get` (override registry `pc_rando_item_override`, else `item_func_ptr`) | add | tweaks.py `make_items_progressive`, `allow_dungeon_items_to_appear_anywhere`, `update_tingle_statue_item_get_funcs` (`item_func_ptr` writes) |
| game/src/d/d_item.cpp | `checkItemGet`: `pc_rando_check_item_get` (override registry `pc_rando_item_getcheck_override`, else `item_getcheck_func_ptr`) | add | `item_getcheck_func_ptr` counterpart of the same redirects |

## R0.10

The archive-load hook (`native/src/pc/features/rando/pc_rando_resources.h`, plan D8). Hook bodies:
`pc_rando_res_hook_set` (pure `pc_rando_resources.cpp`) and `pc_rando_res_hook_loaded`
(`native/src/pc/game_hooks/pc_rando_hooks_resources.cpp`). Patchers register through
`pc_rando_register_dzx_patcher` / `pc_rando_register_res_patcher`, from the init functions listed in
`native/src/pc/features/rando/pc_rando_patchers.def`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_resorce.cpp | `dRes_info_c::set` (records the archive directory, so `ma2room`/`ma3room` and every stage are told apart) and `dRes_info_c::loadResource` (each file stored in `mRes[]` goes through the patchers, which edit the raw bytes or replace them with a copy on the archive heap) | add | `randomizer.py` `get_arc` + `arc.get_file(..., DZx / EventList)`, written back by `save_randomized_iso` |

## R0.12

The message overlay (`native/src/pc/features/rando/pc_rando_msg.h`, plan D10). Hook bodies:
`pc_rando_msg_hook_text` and `pc_rando_msg_hook_entry` (`native/include/pc/pc_rando_hooks.h`), in the
pure `pc_rando_msg.cpp`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/f_op/f_op_msg_mng.cpp:1509 | `fopMsgM_msgGet_c::getMesgEntry` (1509), `fopMsgM_itemMsgGet_c::getMesgEntry` (1601): the overlay's INF1 entry fields (or the new-message defaults) replace the BMG entry before the instant-text QoL | add | gclib `bmg.py` `add_new_message` / `Message` fields, set in `tweaks.py` and `randomizers/hints.py` |
| game/src/f_op/f_op_msg_mng.cpp:1530 | `fopMsgM_msgGet_c::getMessage` (1530), `fopMsgM_itemMsgGet_c::getMessage` (1621): the overlay's encoded text replaces the BMG text (and serves numbers the BMG lacks; `mResMsgNo` = the asked number) | add | `msg.string = ...` / `add_new_message` (`tweaks.py`, `randomizers/hints.py`, `randomizers/boss_reqs.py`) |

## R2.6

Code locations A: the 15 locations whose item is a constant in an actor's code (Code keys of
`native/include/pc/pc_rando_locs.h`). Each site reads `pc_rando_loc_item(PCR_LOC_<name>, vanilla)`.
Checked by the `codeA` smoke (`native/src/pc/harness/smokes/pc_rando_smoke_codeA.cpp`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_bigelf.cpp | `daBigelf_c::next_msgStatus` (0x2EE8): the placed item of the fairy's `getType()` 0-5 (Northern, Outset, Eastern, Southern, Western, Thorned), red rupee for any other type, instead of the next wallet/bomb bag/quiver tier | if/else | flexible_item_locations.asm d_a_bigelf 0x217C |
| game/src/d/actor/d_a_bigelf.cpp | `daBigelf_c::demoProcFlDelete`: the Big Octo fairy (type 6) no longer adds 32 max and current magic | `if/else` (`!pc_rando() &&`) | flexible_item_locations.asm d_a_bigelf 0x7C4/0x7D0 (nop) |
| game/src/d/d_item.cpp | `item_func_max_mp_up1`: pending max and current magic both set to 32 (a refill) instead of max += 32 | if/else | flexible_item_locations.asm main.dol 0x800C4D14 |
| game/src/d/actor/d_a_npc_ji1.cpp | `daNpc_Ji1_c::createItem`: Orca - Hit 500 Times | `if/else` (`#if`/`#else`, `pc_rando_loc_item`) | item_locations.txt d_a_npc_ji1.rel@42F7 |
| game/src/d/actor/d_a_npc_tc_cut.inc | `daNpc_Tc_c::cutPresentProc`: ItemType 0 Tingle First Gift, 1 Tingle Second Gift, 2 (all statues paid) Ankle's reward | `if/else` (`#if`/`#else`, `pc_rando_loc_item`) | item_locations.txt d_a_npc_tc.rel@6117/611F/6143 |
| game/src/d/actor/d_a_npc_mk.cpp | `daNpc_Mk_c::demoProc` case 13: Ivan - Catch Killer Bees | `if/else` (`#if`/`#else`, `pc_rando_loc_item`) | item_locations.txt d_a_npc_mk.rel@20CB |
| game/src/d/actor/d_a_npc_ho.cpp | `daNpc_Ho_c::next_msgStatus` (0x271E, 0x275C, 0x2743, 0x2751): Mrs. Marie - Catch Killer Bees, Give 1, 21 and 40 Joy Pendants | `if/else` (`#if`/`#else`, `pc_rando_loc_item`) | item_locations.txt d_a_npc_ho.rel@0A07/0B4B/0B77/0C87 |

`game/src/d/actor/d_a_npc_tc.cpp` only gains the `pc/pc_rando.h` and `pc/pc_rando_locs.h` includes
for its `.inc`. `pc_rando_loc_item` returns the vanilla constant when `pc_rando()` is 0, so the
`#if`/`#else` sites are vanilla-equivalent; the `#else` keeps the original line for the census.

## R2.7

Code locations B, the Windfall NPCs (`flexible_item_locations.asm` and the CODE paths of
`item_locations.txt`). Item sites read `pc_rando_loc_item` (directly, or through
`pc_rando_people_item` / `pc_rando_kane_item` for d_a_npc_people's two tables); "does the player own the
vanilla item" checks become wwrando's unused event bits (0x6904 Pompie and Vera, 0x6908 Minenco, 0x6910
Kamo, 0x6920 Lenzo's Deluxe Picto Box slot, 0x6980 Salvage Corp, 0x6A01 Maggie, 0x6A02 the cafe
postman; `PC_RANDO_BIT_*` in `native/include/pc/pc_rando_hooks.h`). Hook bodies in
`native/src/pc/game_hooks/pc_rando_hooks_windfall.cpp`. The cafe postman's item itself is an event
location (R2.4); this step only tracks it with 0x6A02.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_npc_people.cpp:4281 | `phase_1` (UB4 in Ocmera): 0x6904 instead of owning Treasure Chart 24 (`COLLECT_MAP_20`) | ?: | d_a_npc_people 0x8D4/0x8D8 |
| game/src/d/actor/d_a_npc_people.cpp:4605 | `daNpcPeople_c::createInit` (Kamo, result unused): 0x6910 instead of `COLLECT_MAP_15` | ?: | d_a_npc_people 0x14CC/0x14D0 |
| game/src/d/actor/d_a_npc_people.cpp:4845 | `daNpcPeople_c::_execute` (Kamo's idle animation): 0x6910 | ?: | d_a_npc_people 0x1C34/0x1C38 |
| game/src/d/actor/d_a_npc_people.cpp:6075 | `daNpcPeople_c::eventGetItemInit`: `l_get_item_no[i]` is the placed item of Sam, Pompie and Vera, Minenco, Linda and Anton, Kreeb, Kamo; after the gift, indices 1/2/5 set 0x6904/0x6908/0x6910 | ?:, add | item_locations.txt d_a_npc_people.rel@C54B-C55F; 0xC54C/0xC550/0xC55C, 0x4BEC `create_item_and_set_event_bit_for_townsperson` |
| game/src/d/actor/d_a_npc_people.cpp:6585 | `daNpcPeople_c::next_msgStatus`: `l_item_id_sa3[i]` is the placed item of Kane's four gate rewards | ?: | item_locations.txt d_a_npc_people.rel@C21B/C21F/C223/C227 |
| game/src/d/actor/d_a_npc_people.cpp:6820 | `daNpcPeople_c::getMsg`: Minenco 0x6908 (`COLLECT_MAP_16`), Kamo 0x6910 twice (day and night) | ?: | d_a_npc_people 0x6170/0x6174, 0x6C50/0x6C54, 0x6CC4/0x6CC8 |
| game/src/d/actor/d_a_npc_people.cpp:8000 | `daNpcPeople_c::setWaitAnm` (Kamo): 0x6910 | ?: | d_a_npc_people 0x88A4/0x88A8 |
| game/src/d/actor/d_a_npc_people.cpp:8050 | `daNpcPeople_c::XyCheckCB` (Kamo takes a photo): 0x6910 | ?: | d_a_npc_people 0x8A5C/0x8A60 |
| game/src/d/actor/d_a_npc_people.cpp:8300 | `daNpcPeople_c::is1GetMap20`: 0x6904 | ?: | d_a_npc_people 0x91C0/0x91C4 |
| game/src/d/actor/d_a_npc_photo.cpp:650 | `phase_1` (Lenzo, both branches): Pompie and Vera's 0x6904 | ?: | d_a_npc_photo 0x9C4/0x9C8, 0x9F4/0x9F8 |
| game/src/d/actor/d_a_npc_photo.cpp:1065 | `daNpcPhoto_c::executeTalk`: `mItemNo` 31 / 38 are the placed items of Become Lenzo's Assistant / Bring Forest Firefly | ?: | item_locations.txt d_a_npc_photo.rel@1C13, @1C37 |
| game/src/d/actor/d_a_npc_photo.cpp:1795 | `daNpcPhoto_c::getMsg`: the Deluxe Picto Box slot is 0x6920 (two checks), set when the firefly is handed over; `mItemNo` is the placed item | ?:, add | d_a_npc_photo 0x3BB0/0x3BB4, 0x3BDC `lenzo_set_deluxe_picto_box_event_bit`, 0x3C68/0x3C6C; item_locations.txt @3BE3 |
| game/src/d/actor/d_a_npc_photo.cpp:2205 | `daNpcPhoto_c::XyEventCB` (firefly event): 0x6920 | ?: | d_a_npc_photo 0x4AF8/0x4AFC |
| game/src/d/actor/d_a_npc_kp1.cpp:398 | `daNpc_Kp1_c::getMsg`: the "quest complete" branch is skipped; the free item check is 0x6A01 | `&&` (!pc_rando()), ?: | d_a_npc_kp1 0x11D8 (`b 0x1210`), 0x1214/0x1218 |
| game/src/d/actor/d_a_npc_kp1.cpp:530 | `daNpc_Kp1_c::event_action`: the placed items of Maggie's Free Item / Delivery Reward; the free item sets 0x6A01 | ?:, add | item_locations.txt d_a_npc_kp1.rel@17D7, @1817; 0x17EC `maggie_give_item_and_set_event_bit` |
| game/src/d/actor/d_a_npc_sv.cpp:113 | `daNpcSv_c::daNpcSv_c`: 0x6980 instead of owning Treasure Chart 34 (`COLLECT_MAP_26`) | ?: | d_a_npc_sv 0x2C8/0x2CC |
| game/src/d/actor/d_a_npc_sv.cpp:580 | `daNpcSv_c::eventGetItemInit`: `l_get_item_no[0]` is the placed item of Salvage Corp Gift; every gift sets 0x6980 | ?:, add | item_locations.txt d_a_npc_sv.rel@3327; 0x19A8 `salvage_corp_give_item_and_set_event_bit` |
| game/src/d/actor/d_a_npc_bm1.cpp:641 | `daNpc_Bm1_c::init_BMC_2`: 0x6A02 instead of Moblin's Letter's reserve slot (0xF) | ?: | d_a_npc_bm1 0x1020/0x1024 |
| game/src/d/actor/d_a_npc_bm1.cpp:1205 | `daNpc_Bm1_c::eventOrder`: ordering event index 0 (the cafe postman's gift) sets 0x6A02 | add | d_a_npc_bm1 0x225C `rito_cafe_postman_start_event_and_set_event_bit` |
| game/src/d/actor/d_a_npc_bm1.cpp:1700 | `daNpc_Bm1_c::getMsg_BMC_2`: 0x6A02 | ?: | d_a_npc_bm1 0x3178/0x317C |

## R2.8

Code locations C: the auction, Battlesquid, Barrel Shooting, the Bird-Man Contest, the Goron trader,
the 13 mailbox letters, the Withered Trees, Zunari and Doc Bandam (27 locations, 31 location keys). The
REL bytes wwrando's items.py `change_item` writes become `pc_rando_code_c_item(site, index, vanilla)`
calls (site table in `native/src/pc/features/rando/pc_rando_code_c.cpp`); the Withered Trees'
`custom_createItem` and momentum are in `native/src/pc/game_hooks/pc_rando_hooks_code_c.cpp`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_auction.cpp | `daAuction_c` item display: `fopAcM_create(ShopItem)` takes the placed item of `l_item_dat[i]` | ?: | items.py change_item `rels/d_a_auction.rel@4489/4491/4499/44A1` |
| game/src/d/actor/d_a_auction.cpp | item given: `createItemForPresentDemo` takes the placed item of `l_item_dat[i]` | ?: | same bytes |
| game/src/d/actor/d_a_npc_kg1.cpp | `daNpc_Kg1_c::wait_action`: `items[0..1]` and `rec_items[0]` | ?: | `rels/d_a_npc_kg1.rel@2C28/2C29/2C2C` |
| game/src/d/actor/d_a_npc_kg2.cpp | `daNpc_Kg2_c::evn_createItem_init`: the item for register 0xB703 = 0/1, 2 | add | `rels/d_a_npc_kg2.rel@20FF/2107` |
| game/src/d/actor/d_a_npc_bmcon1.cpp | `daNpcBmcon_c::eventMesSet`: the first prize | ?: | `rels/d_a_npc_bmcon1.rel@2B7B` |
| game/src/d/actor/d_a_npc_roten.cpp | `daNpcRoten_c::eventGetItemInit`: `l_get_item_no[0]` | ?: | `rels/d_a_npc_roten.rel@533B` |
| game/src/d/actor/d_a_obj_toripost.cpp | `daObjTpost_c::cutPresentProc`: `m_letter[i].mItemNo` | ?: | `rels/d_a_obj_toripost.rel@2564-25F4` |
| game/src/d/actor/d_a_obj_ftree.cpp | `daObjFtree::Act_c::search_heart_part`: `pc_rando_withered_tree_item_momentum` on the frame the item exists (flag in the padding byte `m64A[0]`) | add | flexible_item_locations.asm d_a_obj_ftree 0x184, `withered_tree_item_try_give_momentum` |
| game/src/d/actor/d_a_obj_ftree.cpp | `daObjFtree::Act_c::launch_heart_part`: `pc_rando_custom_create_item` with the placed item, -1 check, no `actor_status` write | if/else | `rels/d_a_obj_ftree.rel@0237`; d_a_obj_ftree 0x25C/0x260/0x2A4, `custom_createItem` |
| game/src/d/actor/d_a_obj_ftree.cpp | `daObjFtree::Act_c::place_heart_part`: the same, and the momentum flag set | if/else | `rels/d_a_obj_ftree.rel@03F3`; d_a_obj_ftree 0x418/0x41C, `create_item_for_withered_trees_without_setting_speeds` |
| game/src/d/actor/d_a_npc_rsh1.cpp | `daNpc_Rsh1_c::getMsg`: event bit 0x6940 instead of `checkItemGet(Magic Armor)` | ?: | d_a_npc_rsh1 0x177C/0x1784 |
| game/src/d/actor/d_a_npc_rsh1.cpp | `daNpc_Rsh1_c::talk01`: the placed item instead of the Magic Armor | ?: | `rels/d_a_npc_rsh1.rel@3117` |
| game/src/d/actor/d_a_npc_rsh1.cpp | `daNpc_Rsh1_c::getdemo_action`: sets 0x6940 when the item given is the slot's (`zunari_magic_armor_slot_item_id`); the reply compare uses the placed item | add, ?: | d_a_npc_rsh1 0x32E8 `zunari_give_item_and_set_magic_armor_event_bit`; `rels/d_a_npc_rsh1.rel@338F` |
| game/src/d/actor/d_a_npc_ds1.cpp | `daNpc_Ds1_c::next_msgStatus`: message 0x1DCB (7627) gives the potion without an empty bottle | \|\| | d_a_npc_ds1 0x1550 (nop) |
| game/src/d/actor/d_a_npc_ds1.cpp | `daNpc_Ds1_c::getdemo_action`: on message 7627 the green/blue potion slot's item | add | d_a_npc_ds1 0x2940 `doc_bandam_check_new_potion_and_give_free_item` (`doc_bandam_green/blue_potion_slot_item_id`) |

## R2.9

Beedle's shops: the 4 shop locations and the shop data tweaks. The placed items reach
`shopItemData_Feedbag`/`_emptybottle`/`_kakera_heart`/`_map` (`d_shop.cpp`) as R0.8 table edits
(`mItemNo`), registered at every activation by a listener in
`native/src/pc/game_hooks/pc_rando_hooks_shop.cpp`, together with `remove_shop_item_forced_uniqueness_bit`
(`mBuyConditions` bit 0x02 cleared for indexes 0, 0xB, 0xC, 0xD); no hunk in `d_shop.cpp`. The Bait Bag
slot's sold-out state is event bit 0x6902 (`pc_rando_shop_bait_slot_set_sold_out` /
`pc_rando_shop_bait_slot_sold_out`). wwrando's nops at d_a_npc_bs1 0x1ED8/0x1EE4 (`>=` tests) have no C
counterpart: the decompiled code tests equality, and the hunk's `if` chain replaces the `switch`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_npc_bs1.cpp | `daNpc_Bs1_c::next_msgStatus`: the 20 Rupee slot's sold-out test uses the placed item and sets event bit 0x6902; the Rock Spire slots' sold-out tests and their 0x2020/0x2010/0x2008 bits use the placed items | ?: (loc item), add (0x6902), if/else (Rock Spire) | `flexible_item_locations.asm` d_a_npc_bs1 0x1CDB, 0x1CE8 `set_shop_item_in_bait_bag_slot_sold_out`; 0x1E9B/0x1EA3/0x1EAB, 0x1ED3/0x1EDF/0x1EEB; 0x1ED8/0x1EE4 nops |
| game/src/d/actor/d_a_npc_bs1.cpp | `daNpc_Bs1_c::createShopList`: the 20 Rupee slot shows its replacement once event bit 0x6902 is set (not once the Bait Bag is owned) | ?: | `flexible_item_locations.asm` d_a_npc_bs1 0x2DBB, 0x2DC4 `check_shop_item_in_bait_bag_slot_sold_out` |
| game/src/d/actor/d_a_shop_item.cpp | `daShopItem_c::set_mtx`: a Y offset of 0.0 in `daShopItem_c::mData` (const) reads as 20.0, except ids 0x10-0x12 | add | `tweaks.py` `fix_shop_item_y_offsets` (0x8034FD10 + id*0x20 + 0x10) |

## R2.11

Any item anywhere (tweaks.py `allow_all_items_to_be_field_items`, always applied by wwrando). The table
part (field models from `item_resource`, the fixed get models of the Master Swords, songs, magic meters
and Hurricane Spin, `item_info` height/radius 0 -> 0x28) is no hunk: edits of R0.8's registry, registered
by `native/src/pc/game_hooks/pc_rando_hooks_field_items.cpp` from `features/rando/pc_rando_field_items.cpp`.
The 7 song / Hurricane Spin arc-name initializations wwrando nops (0x800C1970-0x800C1BA8) need no hunk:
the port's tables are constant-initialized and the edits are applied later, at activation.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_item.cpp:55 | `daItem_c::getYOffset`: the Big Key (own case, split from the Small Key's) and the default case return 23.0 | ?: | getYOffset 0x800F4CD0 `lfs f1, -0x5E7C(rtoc)` (23.0) and jump table 0x8038C8B8 entry 0x4E |
| game/src/d/actor/d_a_item.cpp:789 | `daItem_c::itemGetExecute`: a `default:` case sets `STATUS_INIT_GET_DEMO` (the item-get demo, as the Small Key) | add | jump table 0x8038CA6C entries 0x800F6C8C -> 0x800F675C, `bgt 0x800F675C` at 0x800F6468 |
| game/src/d/actor/d_a_item.cpp:1289 | `daItem_c::mode_wait`: ids 0x20-0x44 and 0x4C+ use `itemActionForArrow` (`pc_rando_field_item_wait_arrow`) | if/else | jump table 0x8038CC7C ids 0x20-0x44 -> 0x800F8160, `bgt 0x800F8160` at 0x800F8138 |

## R3.3

Progressive items (asm group G2). Hook body: `pc_rando_convert_progressive_item_id`
(`native/include/pc/pc_rando_hooks.h`, `native/src/pc/game_hooks/pc_rando_hooks_progressive.cpp`; tier
tables in the pure `features/rando/pc_rando_progressive.cpp`). The 8 progressive item functions and
`normal_magic_meter_item_func` are registered on the 20 progressive ids through R0.8's override
registry (no hunk); message 101+0xB1 goes in R0.12's overlay at every activation.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/f_op/f_op_actor_mng.cpp | `fopAcM_createDemoItem`: the item id becomes the next tier before the params are built | add | `make_items_progressive.asm` 0x80026A24 `convert_progressive_item_id_for_createDemoItem` |
| game/src/d/actor/d_a_item.cpp | `daItem_c::_daItem_create`: the params' item byte becomes the next tier before `m_itemNo` is read | add | `make_items_progressive.asm` 0x800F5550 `convert_progressive_item_id_for_daItem_create` |
| game/src/d/actor/d_a_player_dproc.inc | `daPy_lk_c::dProcGetItem_init`: `item_no` (event param or `dComIfGp_event_getGtItm`) becomes the next tier; one hunk after both reads | add | `make_items_progressive.asm` 0x8012E7B8 / 0x8012E7DC `convert_progressive_item_id_for_dProcGetItem_init_1/2` |
| game/src/d/actor/d_a_shop_item.cpp | `daShopItem_c::_create`: `m_itemNo` is the next tier of the params' item | `?:` | `make_items_progressive.asm` d_a_shop_item 0x8B8 / 0x9C0 `convert_progressive_item_id_for_shop_item` |
| game/src/d/actor/d_a_npc_bs1.cpp | `daNpc_Bs1_c::next_msgStatus`: the two `checkItemGet` calls that decide the item-get animation use the next tier (bait bag slot; Rock Spire slots) | `?:` | `make_items_progressive.asm` d_a_npc_bs1 0x1D00 / 0x1F3C `custom_getSelectItemNo_progressive` |
| game/src/d/d_item.cpp | `item_func_bow`, `item_func_bomb_bag`: the 30/30 arrow and bomb writes; `item_func_deku_leaf`: the 16/16 magic adds (original code under `if (!pc_rando())`) | if/else | `tweaks.py` `make_items_progressive` nops 0x800C346C/70, 0x800C36C0/C4, 0x800C375C/68 |

## R3.6

The Tingle Statues (asm group G4). Owning a statue becomes its event bit 0x6A04-0x6A40 (set by the
item functions of 0xA3-0xA7, registered as R0.8 item-get overrides in
`native/src/pc/game_hooks/pc_rando_hooks_tingle.cpp`) instead of its dungeon chest's opened flag 0xF.
The statue sites pick `pc_rando_tingle_statue_owned` (check_tingle_statue_owned) or the original
`dComIfGs_isStageTbox` with one `pc_rando() ?` per function; the quest-screen texts 503/703 go in the
message overlay at activation (`features/rando/pc_rando_tingle.cpp`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_obj_vtil.cpp | `daObjVtil_c::check_ev_bit` (the Tingle Island statue appears) | ?: | flexible_item_locations.asm d_a_obj_vtil 0x820 |
| game/src/d/actor/d_a_npc_tc.cpp | `daNpc_Tc_HIO_c` ctor (×5), `existTcMonument` (×5), `next_msgStatusBlue` (×5) | ?: (function pointer) | flexible_item_locations.asm d_a_npc_tc 0x2F8-0x338, 0x1578-0x15C8, 0x193C-0x198C |
| game/src/d/actor/d_a_npc_tc_msg_red.inc | `getMsgRed` (×5: an `add` block before the version branches, returning the USA/PAL result with the owned check), `checkMonumentFee`, `checkMonumentPay` (?:) | add, ?: | flexible_item_locations.asm d_a_npc_tc 0x58CC-0x598C, 0x5A54, 0x5C50 |
| game/src/d/actor/d_a_agb.cpp | `daAgb_c::FlagsSend` (×5, statues told to the GBA) | ?: (function pointer) | flexible_item_locations.asm main.dol 0x800D14D8-0x800D1570 |
| game/src/d/actor/d_a_agbsw0.cpp | `daAgbsw0_c::MoveCheck` conditions 9-D | ?: (function pointer) | flexible_item_locations.asm d_a_agbsw0 0x359C-0x360C |
| game/src/d/d_menu_collect.cpp | `dMenu_Collect_c::screenSet`: chart counter = owned statues, chart icon = `tingle_figure.bti` | ?: | misc_rando_features.asm 0x8019CAC0/0x8019CAFC get_num_owned_tingle_statues; tweaks.py show_number_of_tingle_statues_on_quest_status_screen (DOL 0x8035F469) |

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

## R3.4

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: the 22 per-dungeon item IDs are item-get overrides through R0.8's `execItemGet` dispatch, table edits (`pc_rando_table_edit`) and overlay messages (`game_hooks/pc_rando_hooks_dungeon_items.cpp`, `features/rando/pc_rando_dungeon_items.{h,cpp}`) | - | `tweaks.py` `allow_dungeon_items_to_appear_anywhere`; `custom_funcs.asm` `generic_on_dungeon_bit`, `generic_small_key_item_get_func` and the 22 `*_item_get_func` wrappers |

## R3.13

Misc features (`misc_rando_features.asm`, plus `tweaks.py` `add_new_bog_warp` and
`fix_message_closing_sound_on_quest_status_screen`). The two new messages (848 for the FF warp's
confirmation, 704 for the quest screen) are overlay records added by a listener in
`native/src/pc/game_hooks/pc_rando_hooks_misc.cpp` at every activation; they need no hunk.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/include/d/d_menu_fmap.h | `dMenu_Fmap_c::mWrapSpotEmitters` (one more slot for the 10th warp; layout only) | `#if TARGET_PC` | tweaks.py add_new_bog_warp (emitter list has room for 12 on the GameCube) |
| game/src/d/d_menu_fmap.cpp | `l_pcRandoCursorTable`, `l_pcRandoWarpSpotPos*`, `pcRandoWarpCount`, `pcRandoCursorTable` (data and helpers) | `?:` | custom_data.asm ballad_of_gales_warp_table, ballad_of_gales_warp_float_bank |
| game/src/d/d_menu_fmap.cpp | `paneTranceZoomMap` (10th pass: `mLnk3Pane`, the member after the 9 sc1x panes) | `add` | tweaks.py add_new_bog_warp 0x801B6E6C |
| game/src/d/d_menu_fmap.cpp | `_open_warpMode` | `?:` | tweaks.py add_new_bog_warp 0x801B7988 |
| game/src/d/d_menu_fmap.cpp | `init_warpMode` | `?:` | tweaks.py add_new_bog_warp 0x801B7C80, 0x801B7C28/3C/44 |
| game/src/d/d_menu_fmap.cpp | `wrapMove` (message 848 for warp 9; 2 loops) | `?:` | misc_rando_features.asm 0x801B80EC; tweaks.py add_new_bog_warp 0x801B8414, 0x801B84D0 |
| game/src/d/d_menu_fmap.cpp | `wrapSelWinFadeOut` | `?:` | tweaks.py add_new_bog_warp 0x801B9020 |
| game/src/d/d_menu_fmap.cpp | `wrapSelWarp` | `?:` | tweaks.py add_new_bog_warp 0x801B9230 |
| game/src/d/d_menu_fmap.cpp | `warpAreaAnime0` | `?:` | tweaks.py add_new_bog_warp 0x801B936C, 0x801B9360/78/80/A4/C8 |
| game/src/d/d_menu_fmap.cpp | `getWarpAreaGridX`, `getWarpAreaGridY` | `?:` | tweaks.py add_new_bog_warp 0x801B96DC, 0x801B96F0 |
| game/src/d/d_menu_fmap.cpp | `getWarpAreaTablePtr` | `?:` | tweaks.py add_new_bog_warp 0x801B979C, 0x801B9790 |
| game/src/f_op/f_op_msg_mng.cpp | `fopMsgM_msgDataProc_c::stringSet` (warp colours for 848) | `\|\|` | misc_rando_features.asm 0x80032590 |
| game/src/JAZelAudio/JAIZelBasic.cpp | `zeldaGFrameWork` | `?:` | misc_rando_features.asm 0x802A2BE4 (custom_isTbox_for_unloaded_stage_save_info 13, 5) |
| game/src/JAZelAudio/JAIZelBasic.cpp | `startIsleBgm` | `?:` | misc_rando_features.asm 0x802AA2E4 |
| game/src/JAZelAudio/JAIZelBasic.cpp | `setScene` (2 checks) | `?:` | misc_rando_features.asm 0x802AA4CC, 0x802AA4E8 |
| game/src/d/actor/d_a_obj_canon.cpp | `daObj_Canon_c::checkTgHit` | `?:` | misc_rando_features.asm d_a_obj_canon 0x7D0 |
| game/src/d/actor/d_a_player_fan.inc | `daPy_lk_c::setShapeFanLeaf` | `\|\|` | misc_rando_features.asm 0x8014BDF8 |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::execute` (door drop of an enemy weapon) | `?:` | misc_rando_features.asm 0x80121B18 |
| game/src/d/actor/d_a_player_ladder.inc | `procLadderUpStart_init`, `procLadderDownStart_init` | `\|\|` | misc_rando_features.asm 0x8013344C, 0x8013389C |
| game/src/d/actor/d_a_npc_ah.cpp | `phase_1` (3 checks) | `?:` | misc_rando_features.asm d_a_npc_ah 0x85C, 0x8A8, 0x8D8 |
| game/src/d/actor/d_a_npc_ah.cpp | `daNpcAh_c::_execute` (3 checks) | `?:` | misc_rando_features.asm d_a_npc_ah 0x1018, 0x1044, 0x1078 |
| game/src/d/actor/d_a_npc_ls1.cpp | `daNpc_Ls1_c::wait_action1` | `?:` | misc_rando_features.asm d_a_npc_ls1 0x3E88 |
| game/src/d/actor/d_a_npc_ls1.cpp | `daNpc_Ls1_c::bodyCreateHeap` | `\|\|` | misc_rando_features.asm d_a_npc_ls1 0x4F04 |
| game/src/d/actor/d_a_player_rope.inc | `daPy_lk_c::procRopeSwing` (QoL swing turn forced on, base 100) | `\|\|` | misc_rando_features.asm 0x80145648 turn_while_swinging |

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

## R2.2

Field items, pots and stone heads. The ACTR entries themselves are patched by the dzx patcher
`features/rando/pc_rando_place_actr.{h,cpp}` (listed in `pc_rando_patchers.def`), which also applies
`fix_deku_leaf_model` and `make_dungeon_joy_pendant_locations_flexible`; the hunks below read the item
parameters wwrando adds to pots and stone heads.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_tsubo.cpp | `daTsubo::Act_c::damaged` | `if/else` (+ `#else` original) | `flexible_item_locations.asm` d_a_tsubo.rel 0x3E7C `create_pot_item` |
| game/src/d/actor/d_a_obj_homen.cpp | `daObjHomen::Act_c::make_item` | `add` (return; the original follows) | `flexible_item_locations.asm` d_a_obj_homen.rel 0x16DC nop, 0x1728 `create_stone_head_item` |
| game/src/d/actor/d_a_obj_homen.cpp | `daObjHomen::Act_c::_create` | `add` | `tweaks.py` `fix_stone_head_bugs` (d_a_obj_homen.rel 0x3450 profile status &= ~0x80) |

## R2.3

SCOB item locations (salvage points, Big Octos, Gunboats, the black-soil dig spot) are written into
room.dzr at archive load by `features/rando/pc_rando_place_scob.cpp` (a dzx patcher, not a hunk), which
also applies `fix_needle_rock_island_salvage_flags` (sea/Room29 light-ring salvage flags 8 and 9).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_salvage.cpp | `daSalvage_c::proc_wait` (ray of light: chart index + 0x61 instead of `getItemNo`) | `?:` (+ `#else` original) | flexible_item_locations.asm d_a_salvage 0x10C0 |
| game/src/d/d_com_static.cpp | `daTagKbItem_c::dig_main` (`fopAcM_createItem` type 3 action 5, no status 0x4000) | `if (pc_rando()) {new} else {original}` (+ `#else` original) | flexible_item_locations.asm 0x80056C0C, custom_funcs.asm `custom_createItem` |

## R2.5

The 6 boss Heart Container locations (asm group G5.3). One lookup instead of wwrando's per-boss
patches: under `pc_rando()`, `fopAcM_createItemForBoss` spawns `pc_rando_boss_item(unused)`
(`native/include/pc/pc_rando_hooks.h`, `native/src/pc/game_hooks/pc_rando_hooks_boss.cpp`; stage ->
location table in the pure `features/rando/pc_rando_boss_items.cpp`), keyed by the stage number 2-7
that `daBossItem` now passes, or by the loaded stage for the other callers (the disappear cloud, Gohdan,
Molgera). So `fopAcM_createDisappear`, `daDisappear_Execute` and the 6 boss files need no hunk.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/f_op/f_op_actor_mng.cpp | `fopAcM_createItemForBoss`: the boss location's item and item flag 0x15 | `add` (+ the original switch after it) | `flexible_item_locations.asm` 0x80026A90/0x80026AB0 (nop the `li r4, 8`), 0x80026A94/0x80026AB4 (`li r5, 0x15`) |
| game/src/d/actor/d_a_boss_item.cpp | `daBossItem_Create`: passes its stage number instead of 1 | `?:` (+ `#else` original) | `flexible_item_locations.asm` d_a_boss_item 0x1C4 (`lbz r4, 0xB2(r30)`) |
| game/src/d/d_item.cpp | `item_func_utuwa_heart`: the boss-item flag (`dComIfGs_onStageLife`) only when `!pc_rando()` | if/else (+ `#else` original) | `flexible_item_locations.asm` nops 0x800C2FAC/0x800C2FC4 |

## R3.1

New-game state (`custom_funcs.asm` `init_save_with_tweaks`, `reveal_sea_chart.asm`). No hunk in `game/`:
wwrando's call site (`misc_rando_features.asm` 0x8005D618 in `dSv_info_c::init`) is replaced by R0.7's
hook after `dComIfGs_init`, which calls `pc_rando_new_game_init`
(`native/src/pc/game_hooks/pc_rando_hooks_newgame.cpp`, tables in
`native/src/pc/features/rando/pc_rando_newgame.{h,cpp}`) for a randomized new file only.
`reveal_sea_chart.asm`'s change to `dSv_player_map_c::init` is a write to the new save there too.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: `pc_rando_new_game_init`, called from `pc_rando_slot_new_game_end` / `pc_rando_slot_debug_boot_end` (`native/src/pc/game_hooks/pc_rando_hooks_slot.cpp`) | - | `custom_funcs.asm` `init_save_with_tweaks`, `init_starting_gear`; `reveal_sea_chart.asm` 0x8005B2CC |

## R3.2

Health, magic, clothes and the start spawn (`misc_rando_features.asm`, `make_game_nonlinear.asm`
`reinit`, `tweaks.py` `skip_wakeup_intro_and_start_at_dock`). Hook bodies in
`native/src/pc/game_hooks/pc_rando_hooks_start.cpp`, pure part in
`native/src/pc/features/rando/pc_rando_start.{h,cpp}`. `start_ship_at_outset` and
`prevent_fire_mountain_lava_softlock` are not hunks: a dzx patcher on R0.10's dispatcher moves the
sea `stage.dzs` "Ship" ACTR to sea/Room44's SHIP 0 (read from the disc once) and sets its y to -500000.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_save.cpp | `dSv_player_status_a_c::init` (max HP `starting_quarter_hearts`, HP rounded down to a heart) | `if/else` | `misc_rando_features.asm` `set_starting_health` 0x800589A8 |
| game/src/d/d_save.cpp | `dSv_player_return_place_c::init` (spawn 0 instead of 206) | `?:` | `tweaks.py` `skip_wakeup_intro_and_start_at_dock` (DOL 0x80058BAF) |
| game/src/d/d_save.cpp | `dSv_info_c::reinit` (4 blocks: Lenzo's register, jail chest, Deluxe Picto Box, the two camera owned bits) | `if (!pc_rando())` | `make_game_nonlinear.asm` 0x8005D78C, 0x8005D7A4, 0x8005D7FC, 0x8005D81C, 0x8005D82C |
| game/src/d/d_file_select.cpp | `dFile_select_c::makeRecInfo` (a blank slot shows the starting HP; `pc_rando_start_file_select_life` returns the saved value unless the slot is blank and `pc_rando()` or a pending seed is on) | hook returning the vanilla value | `misc_rando_features.asm` `get_current_health_for_file_select_screen` 0x80182504, `get_max_health_for_file_select_screen` 0x80182544 |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::makeRecInfo` (R7.6a): the same hook in the PAL unit, which reads the card bytes big-endian (`mMaxLife` at +0, `mLife` at +2) and the name through `dSv_save_c_PACKED` | hook returning the vanilla value | as above |
| game/src/d/d_s_name.cpp | `dScnName_c::FileSelectMainNormal` (after R0.7's load hook: magic refilled, HP capped) | `add` | `misc_rando_features.asm` `fully_refill_magic_meter_and_cap_health_on_load_save` 0x80231B08 |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::playerInit` (casual clothes iff `heros_clothes != 1`) | `?:` | `misc_rando_features.asm` `check_player_in_casual_clothes` 0x80125AD8 |
| game/src/d/d_gameover.cpp | `dGameover_c::_execute` (continue HP min(12, max rounded down)) | `?:` | `misc_rando_features.asm` `gameover_continue_reset_life` 0x8018E7D0 |

## R3.5

Key bag text (`tweaks.py` `implement_key_bag`, `misc_rando_features.asm`
`check_run_new_text_commands` / `exec_curr_num_keys_text_command`). Messages 403 ("Key Bag") and 603
(the description with the five commands) are overlay records added by a listener in
`native/src/pc/game_hooks/pc_rando_hooks_keybag.cpp` at every activation; they need no hunk.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/f_op/f_op_msg_mng.cpp | `fopMsgM_msgDataProc_c::stringSet` (new tag cases 0x4B-0x4F through `pc_rando_text_command`) | `#if TARGET_PC` `add` (`else if (pc_rando() && ...)`) | `misc_rando_features.asm` 0x80033E74 |

## R3.15

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: the stage tweaks are a dzx patcher and a dzb patcher on R0.10's archive-load dispatcher (`features/rando/pc_rando_tweak_stages_a.{h,cpp}`, `game_hooks/pc_rando_hooks_stages_a.cpp`). The plan's hunk at `d_a_bg.cpp:206` is not needed: `room.dzb` passes through `pc_rando_res_hook_loaded` after `cBgS::ConvDzb`, before `dBgW::Set` reads it | - | `tweaks.py` `remove_forsaken_fortress_2_cutscenes`, `add_ganons_tower_warp_to_ff2`, `remove_makar_kidnapping_event`, `disable_invisible_walls`, `prevent_door_boulder_softlocks`, `fix_ghost_ship_chest_crash`, `fix_totg_warp_out_spawn_pos`, `remove_phantom_ganon_requirement_from_eye_reefs`, `fix_forsaken_fortress_door_softlock`, `fix_helmaroc_king_table_softlock`, `prevent_fairy_island_softlocks` |

## R2.4

The 11 event item locations and Hurricane Spin (asm group G5.6). The event actions are patched in
`event_list.dat` at archive load by the pure `features/rando/pc_rando_place_event.cpp` (wwrando's
`change_event_item`; no `game/` hunk). Hurricane Spin (0xAA) gets an item-get override that sets the
unused event bit 0x6901 (`native/src/pc/game_hooks/pc_rando_hooks_event.cpp`), and the spin attack
tests that bit. Its field and get models are R2.11's (plan D12).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_npc_ji1.cpp | `daNpc_Ji1_c::createItem`: the Hurricane Spin branch gives `pc_rando_loc_item(PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS, 0xAA)` | `pc_rando_loc_item` (`#if TARGET_PC`, `#else` original) | `item_locations.txt` path `rels/d_a_npc_ji1.rel@4267` |
| game/src/d/actor/d_a_player_sword.inc | `daPy_lk_c::procCutTurnMove_init`: Hurricane Spin is usable when `pc_rando_hurricane_spin_owned()` (bit 0x6901) instead of bit 0x0B20 | `?:` (+ `#else` original) | `flexible_item_locations.asm` 0x80158C08 (`li r4, 0x6901`), item_func_ptr[0xAA] = `hurricane_spin_item_func` |

## R2.10

The 4 added chests (`tweaks.py` `add_chest_in_place_of_jabun_cutscene`,
`add_chest_in_place_medli_grappling_hook_gift`, `add_chest_in_place_of_master_sword`,
`add_chest_in_place_queen_fairy_cutscene`). The chests, the raft, the wind, the layer moves, the removed
Master Sword actors, spawn 10 and the shortened `ajav_uzu` event are archive patchers on R0.10's
dispatcher (`features/rando/pc_rando_tweak_added_chests.{h,cpp}`, listed in `pc_rando_patchers.def`);
the two hunks below are the cutscene sites of the same locations (wwrando writes the placed item to
those code bytes too). Checked by the `addchest` smoke and `cos_rando_test_R2_10`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_demo00.cpp | `daDemo00_c::execute` (demo item parameter, `field_0x29c == 5`): `l_itemNo[0]` Master Sword Chamber, `[5]` Jabun's Cave, `[8]` Inside Mother Isle give their placed items through `pc_rando_added_chests_demo_item` | `?:` (`#if`/`#else`, original in `#else`) | item_locations.txt main.dol@8035BD34/8035BD39/8035BD3C |
| game/src/d/actor/d_a_npc_md.cpp | `daNpc_Md_c::initialLetterEvent` (`mCurEvent == 1`, item 0x25): Dragon Roost Cavern - Miniboss | `?:` (`#if`/`#else`, `pc_rando_loc_item`) | item_locations.txt d_a_npc_md.rel@AF2F |

## R3.19

Zephos, the auction intro and Zephos's double song. The two event edits (`tweaks.py`
`shorten_zephos_event`: sea `TACT_HT` Hr/Link actions[0:7], CAMERA actions[0:5], ending flags = the
last actions' flags; `shorten_auction_intro_event`: Orichh `AUCTION_START` CAMERA actions[3] and [4]
removed) are a `.dat` patcher on R0.10's archive-load dispatcher (`features/rando/pc_rando_tweak_zephos.{h,cpp}`)
and need no hunk.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_npc_hr.cpp | `daNpc_Hr_c::demoProcTact0` (the hard-coded `execItemGet(dItemNo_WINDS_REQUIEM_e)` skipped) | `#if TARGET_PC` `if/else` | `flexible_item_locations.asm` d_a_npc_hr 0x1164 |

## R3.17

The pirate ship at Windfall (`tweaks.py` `add_pirate_ship_to_windfall`). The stage edits (sea/Room11's
ship on the default layer with door type 0; Asoko/Room0: Niko removed from layers 2 and 3, Aryll
`Ls1`, `SW_C00` 0xC3, the three `SwOp`s and `Ashut` on switch 0xC4; Asoko `stage.dzs` EVNT
`AryllOpensDoor`) and the new event in Asoko's `event_list.dat` are archive patchers on R0.10's
dispatcher (`features/rando/pc_rando_tweak_pirate_ship.{h,cpp}`); messages 3008, 849 and 850 are
overlay records (R0.12) built by `game_hooks/pc_rando_hooks_pirate_ship.cpp`. The two hunks below are
wwrando's main.dol and REL writes. The wave slot itself (`JAIZelBasic::m_dy_wave_set_2nd[0x0E][0]` =
0x19) is written by the hook on first use: the slot is free (no stage or island entry points at it),
so a vanilla game never reads it. Checked by the `pirateship` smoke and `cos_rando_test_R3_17`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::setScene`: the stage entry's second dynamic scene wave through `pc_rando_pirate_ship_scene_wave2` (Asoko, entry 0x0C: slot 0x0E, IsleLink_0.aw) | `?:` (`#if`/`#else`, original in `#else`) | main.dol 0x8039C30C + 0xC*4 + 3 = 0x0E, 0x8039C2E4 + 0xE*2 = 0x19 |
| game/src/d/actor/d_a_npc_ls1.cpp | `daNpc_Ls1_c::setAnm_NUM`: animation 4 (`ANMNUM_WAIT05_e`) uses facial animation 5 (`BTPNUM_NGWARAI_e`) | `?:` (`#if`/`#else`, original in `#else`) | d_a_npc_ls1.rel 0x5D18 + 4*0x10 + 1 = 5 (`a_anm_prm_tbl$4744[4].btpNum`) |

## R6.5

Trap chests (`trap_chests`) and chest type matches contents. The file edits (params bit 0x40, the
chest type nibble, the `DEFAULT_TREASURE_TRAP` event per stage) are archive patchers in
`features/rando/pc_rando_trap.cpp`; the game side is below, its bodies in
`game_hooks/pc_rando_hooks_trap.cpp`. `getIsTrap` itself is R3.8's.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_tbox.cpp | `demoProc` (action table gains "SPRING_TRAP"; the count passed to `getMyActIdx` is 5 only under pc_rando()) | `#if`, `?:` | asm/actors/d_a_tbox.cpp `demoProc` |
| game/src/d/actor/d_a_tbox.cpp | `demoProc` (SPRING_TRAP init: timer 0, smoke 0x029D; `pc_rando_tbox_spring_trap_init`) | `if` | asm/actors/d_a_tbox.cpp `demoProc` DEMO_PROC_SPRING_TRAP |
| game/src/d/actor/d_a_tbox.cpp | `demoProc` (SPRING_TRAP each frame: `pc_rando_tbox_spring_trap` = demoProcSpringTrap; else the default cut end) | `if/else` | asm/actors/d_a_tbox.cpp `demoProcSpringTrap` |
| game/src/d/actor/d_a_tbox.cpp | `actionOpenWait` (a trap chest creates no item) | `if/else` (`#else` original) | asm/actors/d_a_tbox.cpp `actionOpenWait` |
| game/src/d/actor/d_a_tbox.cpp | `actionOpenWait` (event `DEFAULT_TREASURE_TRAP` for a trap chest, ahead of R3.8's `DEFAULT_TREASURE_A`) | `if` | asm/actors/d_a_tbox.cpp `actionOpenWait` |

## R4.3

Generate a seed in the menu: no hunk in `game/`. The rows (Seed..., Random seed, Paste permalink,
Generate) are R4.1 `RandoMenuAction`s registered by `features/rando/pc_rando_menu.cpp`, which
`pc_menu.cpp` calls from `menuInit` (`randoMenuInit`) and every frame from `menuFrame`
(`randoMenuFrame`); the worker is `features/rando/pc_rando_genjob.cpp`. Checked by the `gen` smoke,
`check_gen_run.py` and `cos_rando_test_R4_3`.

## R6.2

Key-lunacy needs no hunk. The generator already follows `options.keylunacy` (R1.3/R1.4: no dungeon
item prerandomization, `check_item_valid_in_location` drops the own-dungeon rule, and the logic's
`Option "keylunacy"` requirements). At runtime the dungeon items are R3.4's 22 item IDs, whose item
funcs already pick the stage by where the player is.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/` (smoke: `pc_rando_smoke_dkeys.cpp` step 6; golden: `check_seeds.py` keylunacy presets) | - | `randomizers/items.py` keylunacy branches; `custom_funcs.asm` `generic_small_key_item_get_func` |

## R6.3

Sword modes. The pool (No Starting Sword adds the 3 Progressive Swords; Swordless removes them and
Hurricane Spin) is the generator's (`pc_rando_options.cpp`, `pc_rando_logic.cpp`), and the new-game
sword and barrier bits 0x2C02/0x3B08 are R3.1's (`pc_rando_newgame.cpp`). The game side of
`swordless.asm` is below, its bodies in `game_hooks/pc_rando_hooks_sword.cpp`, which also puts
messages 1128 and 1590 (`tweaks.py` `update_text_for_swordless`) in the overlay at every activation of
a Swordless seed. The temporary sword is written as the asm writes it: the equip byte alone
(`dSv_player_status_a_c::setSelectEquip`), never `dComIfGs_setSelectEquip`, which would also mark it
as owned. Checked by the `swordless` smoke.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_fganon.cpp | `damage_check`: in a Swordless seed Phantom Ganon is hurt by the Skull Hammer (AT bit 0x00010000) with no Master Sword check | `?:` (`#if`/`#else`, original in `#else`) | swordless.asm d_a_fganon.rel 0x5EE0 (`rlwinm. r0, r0, 0, 15, 15`), 0x5EF0 (`b 0x5F0C`) |
| game/src/d/d_s_play.cpp | `phase_4`, GTower branch after the bow is taken: `pc_rando_sword_ganondorf_stage` (the Hero's Sword when none is equipped) | `if` | swordless.asm give_temporary_sword_during_ganondorf_fight_in_swordless (main.dol 0x80235F10) |
| game/src/d/d_s_play.cpp | `phase_4`, other stages before the bow check: `pc_rando_sword_stage_load` (Ojhous: the Hero's Sword and the bow check skipped; elsewhere: the equipped sword removed when none is owned) | `?:` (`#if`/`#else`, original in `#else`) | swordless.asm give_temporary_sword_in_orcas_house_in_swordless (0x8023607C), remove_temporary_sword_when_loading_stage_in_swordless (0x80236084) |

## R6.6

Randomized charts (`randomize_charts`; charts.py `ChartRandomizer._save`). The salvage half (every
`d_a_salvage` SCOB with salvage_type 0 in sea/Room1-49 gets the island's new chart index) is an
archive patcher in `features/rando/pc_rando_charts.cpp`; the sea chart half is the hunk below, its
body `pc_rando_charts_cmapdat_loaded` in the same file.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_menu_fmap.cpp | `dMenu_Fmap_c::aramCmapDatRead`: `pc_rando_charts_cmapdat_loaded` on the CmapDat.bin just read (texture id, sector and 4 random positions per chart) | add | randomizers/charts.py `ChartRandomizer._save` (wwlib/charts.py `Chart.save_changes`) |

## R6.7

Starting island (`randomizers/starting_island.py`). The generator draws `choice` of sea rooms 2-49
(`pc_rando_gen.cpp`, written as `starting_island_room`); at runtime `pc_rando_start_room()`
(`game_hooks/pc_rando_hooks_start.cpp`) returns the active seed's room, 44 without the option.
`_save`'s `change_ship_starting_island(room)` is not a hunk: R3.2's sea `stage.dzs` patcher now copies
the SHIP 0 entry of that room (read from the disc once per room) instead of always Room44's.
`set_new_game_starting_spawn_id(0)` is R3.2's hunk. Checked by the `island` smoke.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_save.cpp | `dSv_player_return_place_c::init` (the new game's room is `pc_rando_start_room()` instead of Outset) | `?:` (`#if`/`#else`, original in `#else`) | `tweaks.py` `set_new_game_starting_room` (DOL 0x80058BA7) |

## R4.4

New file binding and file-select display (plan D3, D15). The pending seed (R4.3's Generate) is consumed
by R0.7's `NameInMain` hook; R4.4 adds no hunk there, but R0.7's six hook bodies now run in a
`JKRPcHostAllocScope` (the active seed outlived the name scene's heap; the first load after a
randomized new file panicked with "Bad Block"). The file-select line is not drawn by the game: the
hook below only reads the card buffer, and `features/rando/pc_rando_fileselect.h`'s ImGui overlay
draws it at the bottom of the screen (the plan's "menu overlay" option: a J2D text pane in the info
panel would need several hunks). Like R0.7's hooks it is not under `pc_rando()` (the file select runs
with it off); a slot without a rando record and a blank slot with no pending seed show nothing.
"Clean unused seed files" is a Randomizer tab row (`game_hooks/pc_rando_hooks_fileselect.cpp`,
pure part `features/rando/pc_rando_sidecars.cpp`). Checked by the `newfile` smoke (target
"rando-newgame") and `cos_rando_test_R4_4`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_file_select.cpp | `dFile_select_c::_draw`: `pc_rando_file_select_draw(mSaveDataPtr, selectNum, field_0x392b)` after `dComIfGd_set2DOpa` (reads only) | add | none (plan D15) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::_draw` (R7.6a): the same call in the PAL unit (DataSelProc 0-6 are the same states in both) | add | none (plan D15) |

PAL: the PAL build compiles `game/src/pal/d/d_file_select.inc` instead (its own `dFile_select_c::_draw`);
R7.6a added the same call there.

## R4.5

Spoiler viewer: no hunk in `game/`. The row "Spoiler log..." is an R4.1 `RandoMenuAction` (order 140,
after Generate) registered by `game_hooks/pc_rando_hooks_spoiler.cpp` (`randoSpoilerInit`, called by
`pc_menu.cpp` `menuInit`); `pc_menu.cpp` hosts its page as it hosts R4.2's text input (keys in
`handleInput`, drawing in `drawMenu`, opened with the menu in `menuFrame`, closed in `setOpen`). The
model (spoiler log parser, locations by zone, pager) is `features/rando/pc_rando_spoilerview.cpp`.
Checked by the `spoiler` smoke (shots 530, 580, 630) and `cos_rando_test_R4_5`. wwrando reference:
`randomizer.py` `write_spoiler_log` (`randomizers/items.py` `write_progression_spheres_to_log`,
`write_item_location_spoilers_to_log`).

## R6.9

Required bosses at runtime (`required_bosses`; boss_reqs.py `RequiredBossesRandomizer._save`). The
Puppet Ganon door's condition is data, as wwrando writes it, not a hunk: an archive patcher in
`features/rando/pc_rando_bossreq.cpp` edits GanonL/Room0 `room.dzr` (ALLdie sets 0x26 instead of the
door's 0x19; appended DngSw with the required bosses' stage mask setting 0x25, SwOp AND 0x25+0x26 ->
0x19, SwOp NAND 0x25 -> 0xE5, TagMsg 851 at the door on spawn switch 0xE5, NBOX10 wall behind it
disabled by 0x25), with step R3.7's actors. Message 851 goes in the overlay from
`game_hooks/pc_rando_hooks_bossreq.cpp`. The sea chart's quest markers are the hunk below, its body
`pc_rando_bossreq_fmap_markers` (same file): the PIC1 panes of the required bosses are shown, moved to
their island's sector (through the seed's entrance map) and rotated as wwrando's f_map.blo bytes. A
seed without required bosses changes nothing. Checked by the `bossreq` smoke.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_menu_fmap.cpp | `dMenu_Fmap_c::_create`, right after `f_map.blo` is built: `pc_rando_bossreq_fmap_markers` (the required bosses' quest marker panes: visible, position, rotation) | add | randomizers/boss_reqs.py `show_quest_markers_on_sea_chart_for_dungeons` (f_map.blo 0x43B0 + i*0x40: +9, +0x10, +0x12, +0x19) |

## R5.3

Hint placement at runtime (hints.py `HintsRandomizer._save`). The hint texts (R5.2's `buildHintTexts`:
837, 12015-12017, 13026 + island, 14001-14010, 1502 and 3443-3448) go into the R0.12 message overlay
from `game_hooks/pc_rando_hooks_hints.cpp`, at the first archive load after each activation (after
R3.21's texts, so KoRL's hints replace R3.21's 3443 as in wwrando), wrapped with the message font and
each message's own BMG text box type. `rotate_hoho_to_face_hint` is data, as wwrando writes it, not
a hunk: an archive patcher in the same file sets the `y_rot` of the `Ah` actors of the Ho Ho sea rooms'
`room.dzr` (all ACTR layers) towards the MULT entry of the island his hints name first (embedded
table and angle in `features/rando/pc_rando_hint_place.cpp`). `flexible_hint_locations.asm` is the hunk
below, on only when the seed's hints have a Big Octo Great Fairy hint (wwrando applies the patch
exactly then). The spoiler log's Hints section is R5.1's. A seed without hints changes nothing.
Checked by the `hoho` smoke and `cos_rando_test_R5_3`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_bigelf.cpp | `daBigelf_c::getMsg`: message 0x2EEE goes on to the item hint 0x2EEF (12015) whatever the current bow, when `pc_rando_hints_flexible()` (the seed has an octo fairy hint) | `if/else` (`#if`/`#else`, `(pc_rando() && ...) \|\|`) | flexible_hint_locations.asm d_a_bigelf 0x22A4 (nop) |

## R6.12

Entrance randomizer, stage data (entrances.py `EntranceRandomizer._save`, first half:
`update_all_entrance_destinations` -> `update_entrance_to_lead_to_exit` for every exit, only when an
entrance option is on). All of it is data, as wwrando writes it, not a hunk: an archive patcher in
`features/rando/pc_rando_entr_stage.cpp` (registered after R3.16, whose spawns 27 it looks up) edits
`stage.dzs` / `room.dzr` as they load: the entrance's and the exit's SCLS, the PLYR spawn type 5 -> 1,
the (mini)boss arenas' `stage.dzs` SCLS[0] (the entrance, or the dungeon start: a table of the 7
entrance stages' SCLS[0], checked against each when it loads), Savage Labyrinth's `sea` SCLS in
Cave10/Room0, Cave10/Room20 and Cave11/Room0, the caves' and fountains' 2DMA sector, and the VolTag of
MiniKaz/MiniHyo Room0 (type 1/2 or the name cleared). A seed without entrance options changes nothing.
The boss warp-out half is R6.13. Checked by the `entr` smoke (every edited file of 4 entrance maps =
wwrando's `_save` on the disc) and `cos_rando_test_R6_12`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: an archive patcher (`features/rando/pc_rando_entr_stage.{h,cpp}`, `pc_rando_patchers.def`) | - | randomizers/entrances.py `update_entrance_to_lead_to_exit` |

## R6.13

Boss warp-out under entrance rando (entrances.py `update_all_boss_warp_out_destinations`). wwrando
rewrites the `Stage`, `RoomNo` and `StartCode` properties of the DIRECTOR's `NEXT` action in the
`WARP_WIND_AFTER` event of M_DragB, kinBOSS, SirenB, M_DaiB and kazeB to the warp-out spawn of the
arena's outermost entrance (`get_outermost_entrance_for_exit`, through the nesting). The port does it
in code: the hunk below asks `pc_rando_warpout_next_stage` (`game_hooks/pc_rando_hooks_warpout.cpp`),
which answers only for the DIRECTOR of the loaded `WARP_WIND_AFTER` event on those five stages; the
destination comes from the seed's entrance map (`features/rando/pc_rando_warpout.cpp`, R6.10's
tables). Helmaroc King's `update_helmaroc_king_arena_ganon_door` is data, as wwrando writes it, not a
hunk: an archive patcher in the same file sets M2tower/Room0 `room.dzr` `SCLS[1]` (after R3.15 turned
it into sea room 1 spawn 0). Like wwrando (`BaseRandomizer.save`: `made_any_changes`), nothing changes
unless the seed randomizes entrances. Checked by the `warpout` smoke and `cos_rando_test_R6_13`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_event_data.cpp | `dEvDt_Next_Stage`, after it read `RoomNo`: `pc_rando_warpout_next_stage` may replace the stage name, start code and room passed to `dComIfGp_setNextStage` | add | randomizers/entrances.py `update_boss_warp_out_destination` (event_list.dat `WARP_WIND_AFTER`, DIRECTOR `NEXT`: Stage, RoomNo, StartCode) |

## R6.14

Entrance sweep: no hunk in `game/`. The harness smoke `entrsweep`
(`harness/smokes/pc_rando_smoke_entrsweep.cpp`) drives the doors with the game's own calls
(`dLib_setNextStageBySclsNum`, `dComIfGp_setNextStage`, `fopAcM_orderOtherEventId` for
WARP_WIND_AFTER, `dComIfGs_putSave` + `dComIfGs_setGameStartStage` + `dComIfGs_gameStart` and R0.7's
slot hooks for save-and-quit); `native/tools/rando/entrance_sweep.py` is the structure checker and the
driver.

## R7.1b

The particle bank hook: wwrando edits particle files on the disc (`randomizer.py` `get_jpc`, saved by
gclib's `JPC.save()`); the port edits them as the game loads them, before the `JPAResourceManager` that
reads their bytes in place is built. Registry of imports and colour editors:
`features/rando/pc_rando_jpc_scene.{h,cpp}` (cleared when `pc_rando()` turns 0); hook bodies:
`game_hooks/pc_rando_hooks_particles.cpp`. For a scene with imports, the merged copy (R7.1a's
`pc_rando_jpc_copy_particle` + `pc_rando_jpc_save_into`, gclib's layout) is allocated on `mSceneHeap`
before the manager, so it is freed with the heap in `removeRoomScene`; `mpData` keeps pointing at the
loaded file, which `removeRoomScene` still frees. Sources are read synchronously from the disc into host
memory; "any" scans Pscene000-254 in wwrando's order. common.jpc is loaded at boot, before any randomized
file: the `createCommon` hook keeps its address in every game and only edits it under `pc_rando()`
(editors registered later run at once; the bytes are restored at deactivation). Checked by the
`jpcscene` smoke.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_particle.cpp | `dPa_control_c::createRoomScene`: the scene manager is built from `pc_rando_particles_scene` (merged copy or the loaded file, edited) | ?: | tweaks.py `add_inter_dungeon_warp_pots` (particle copy), randomizers/enemies.py `update_loaded_particles`, randomizers/palettes.py particle colours |
| game/src/d/d_particle.cpp | `dPa_control_c::createCommon`: `pc_rando_particles_common` keeps common.jpc's address and, under `pc_rando()`, runs its editors | hook (address kept; edits only under `pc_rando()`) | customizer.py `change_player_custom_colors` (common.jpc particle 0x48 `bsp1` prm colour) |

## R7.5a

Cosmetic tweaks wwrando applies on every seed. `make_tingle_statue_reward_rupee_rainbow_colored`:
`item_resource[0xB8].mTevFrm` becomes 7 through R0.8's `pc_rando_table_edit`
(`game_hooks/pc_rando_hooks_cosmetic.cpp`, at each activation), and the hunk below animates colour 7
(`features/rando/pc_rando_tweak_cosmetic.cpp`: the f32 key frame +0.15 per draw, -6.0 once not below
6.0, `fabs`). `give_fairy_fountains_distinct_colors` is data: an archive patcher in the same pure
file sets Fairy01-06 `stage.dzs` Pale `bg0_c0`/`bg0_k0` to hue k/6 (colorsys, int truncation).
Checked by the `cosmetic` smoke, `cos_rando_test_R7_5a` and `native/tools/rando/cosmetic_check.py`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_itembase.cpp | `daItemBase_c::animEntry`: the constant BRK frame comes from `pc_rando_cosmetic_brk_frame` (animated for colour 7) | ?: | misc_rando_features.asm main.dol 0x800F93F4 `check_animate_rainbow_rupee_color` |

## R7.2c

Raw J3D hook, palette textures, rat holes. `dRes_info_c::loadResource` hands every file to
`pc_rando_res_hook_raw` (`game_hooks/pc_rando_hooks_resources.cpp`) right after `JKRGetResource`,
before the J3D loaders; it dispatches BMD*, BDL*, BMT*, BSMD and BRK files to the raw patcher list
(`pc_rando_register_raw_patcher`, `features/rando/pc_rando_resources.h`), while the bytes are still the
disc's (the R0.10 hook after the loaders sees a `J3DModelData*` for them; BTI files are still raw there
and stay on its list). The patchers are `features/rando/pc_rando_palettes.cpp`: each palette group's
`/res/Object/<RARC>.arc` `.bdl .bmd .bmt .brk` (TEX1, `features/rando/pc_rando_j3d.cpp`) and `.bti`
files minus the excluded ones, shifted by the seed's `palettes` key with gclib's per-name semantics in
the disc's layout; and `make_rat_holes_visible_from_behind` (Nzg `kana_00.bdl`, every seed). Checked by
the `palettes` smoke, `cos_rando_test_R7_2c` and `native/tools/rando/palette_check.py`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_resorce.cpp | `dRes_info_c::loadResource`, after `JKRGetResource`: `pRes = pc_rando_res_hook_raw(...)` before the type's loader | add | randomizers/palettes.py `shift_colors_for_file_group` (`shift_all_colors_in_tex1` / `_bti`, `J3D.save`); tweaks.py `make_rat_holes_visible_from_behind` |

## R7.1c

Inter-dungeon warp pots (`add_shortcut_warps_between_dungeons`, `PcRandoConfig::inter_dungeon_warps`).
`tweaks.py add_inter_dungeon_warp_pots` is data: an archive patcher (`features/rando/pc_rando_tweak_warp_pots.cpp`,
registered after R6.12) appends to the 6 rooms' `room.dzr` 3 SCLS (the cycle's spawns 69, fade 4) and a `Warpts<k+1>`
(type k+2, event register 2 or 5, the new SCLS indexes), and the spawn 69 (type 7) to the room or, for M_Dai and
kaze, to `stage.dzs`; an activation listener registers the particle imports 0x8161, 0x8162, 0x8165, 0x8166, 0x8112
from Pscene035 into Pscene050 and Pscene043 with R7.1b's registry. The hunk below gives the pots of register 2 or 5
the custom smoke colours under `pc_rando()` (as wwrando's always-applied asm). Checked by the `warppots` smoke and
`cos_rando_test_R7_1c`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_obj_warpt.cpp | `daObj_Warpt_c::createInit`: `m86C`/`m870` are 0xE5101B80 / 0x3C379D80 when `m2B8` (event register index) is 2 or 5 | #if/else | misc_rando_features.asm d_a_obj_warpt.rel 0x2050/0x2080 `set_prm_color_for_warp_pot_particles`, `set_env_color_for_warp_pot_particles` |

## R7.2d

Material and animation colours. No new hunk in `game/`: the R7.2c raw J3D hook's palette patcher
(`features/rando/pc_rando_palettes.cpp`) now runs the whole J3D branch of `shift_colors_for_file_group`
(`pc_rando_j3d_shift_file`, `features/rando/pc_rando_j3d.cpp`): MAT3 tev and konst colours
(`shift_all_colors_in_mat3`, negative tev colours skipped, each used list entry shifted once in the
disc's layout); MDL3 TEV colour register commands rewritten from the shifted materials, which is what
gclib's `J3D.save` leaves (it regenerates MDL3 with `generate_from_mat3`, so wwrando's own
`shift_all_colors_in_mdl3` edit is overwritten; the duplicated high commands carry the new colour);
TRK1 register and konst animations (`shift_all_colors_in_trk1`: grouped by material name, `cc.brk`
animation 1 skipped, closest-time rule) written in gclib's TRK1 layout (never larger than the disc's
chunk on the group files; an in-place fallback counts conflicts). Checked by `cos_rando_test_R7_2d`
(byte-identical to gclib on `fixtures/R7_2d_j3d.txt`), the `palettes` smoke and
`native/tools/rando/palette_check.py`, which now compares every J3D file's material, MDL3 and TRK1
colours too.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: R7.2c's raw patcher (`features/rando/pc_rando_palettes.cpp`) calls `pc_rando_j3d_shift_file` (`features/rando/pc_rando_j3d.{h,cpp}`) | - | randomizers/palettes.py `shift_all_colors_in_mat3` / `_mdl3` / `_trk1`, gclib `J3D.save` |

## R7.5b

Player trail and eye colours, the part of `customizer.change_player_custom_colors` that needs no
wwrando asset (pure part `features/rando/pc_rando_player_colors.{h,cpp}`, hook bodies
`game_hooks/pc_rando_hooks_player_colors.cpp`, declared in `pc/pc_rando_player_colors.h`). The colours
come from the seed's `custom_colors` option (or `COS_RANDO_CUSTOM_COLORS`), read at the first hook call
after each activation; with none chosen every hook leaves the vanilla value and bytes. The trail keys
(`sword_slash_trail_color`, ...) are read from `custom_colors` (wwrando reads them from a custom model's
metadata, which the port does not have); the pupils (`Eyes`) are recoloured with `color_exchange`
without a mask (`ignore_bright`), every other body colour needs a mask and is skipped. The arrow trail
is a common.jpc editor of R7.1b's registry (registered at each activation; R7.1b restores the file). The
hitomi texture of the resident Link.arc cl.bdl is recoloured in place at the player's createHeap and put
back at deactivation. LkAnm.arc is an ARAM archive read with `JKRReadIdxResource` (not through
`loadResource`), so the cutf BRK hunk edits setSwordModel's buffer itself. Checked by the `colors` smoke,
`cos_rando_test_R7_5b` and `native/tools/rando/player_colors_check.py`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_player_particle.inc | `daPy_swBlur_c::draw`: the TEV colour of each slash type (`n_color0`, `y_color0`, `g_color0`) from `pc_rando_player_trail_color` | ?: | customizer.py `change_player_custom_colors` main.dol 0x803F62AC / 0x803F62B0 / 0x803F62B4 |
| game/src/d/actor/d_a_boomerang.cpp | `daBoomerang_blur_c::draw`: `color0` from `pc_rando_player_trail_color` | ?: | customizer.py `change_player_custom_colors` main.dol 0x803F6268 |
| game/src/d/actor/d_a_player_sword.inc | `daPy_lk_c::setSwordModel`: `pc_rando_player_colors_cutf_brk` on the cutfh/cutfm.brk buffer before `J3DAnmLoaderDataBase::load` | add | customizer.py `change_player_custom_colors` LkAnm.arc cutfh.brk / cutfm.brk key frames 0-2 |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::createHeap`: `pc_rando_player_colors_cl_model(mpCLModelData)` after cl.bdl's model is made | add | customizer.py `change_player_custom_colors` Link.arc cl.bdl "hitomi" (`color_exchange`, `replace_image`) |

## R7.2e

Particle and code colours; the `randomize_enemy_palettes` option is live (its refusal in
`pc_rando_gen.cpp` is gone). Particles need no hunk: an activation listener
(`features/rando/pc_rando_palette_sites.cpp`, `pc_rando_palette_code_init` in `pc_rando_patchers.def`)
registers a colour editor on every Pscene file with R7.1b's registry, which runs in `createRoomScene`
before the `JPAResourceManager` reads the bytes (`shift_all_colors_in_particle`: BSP1 prm / env colour and
their key tables, SSP1 prm / env colour, hue only, per group in file order). The hardcoded colours of
`shift_hardcoded_*_colors` are the hunks below: each takes the decompiled value through
`pc_rando_palette_rgb` (`native/include/pc/pc_rando_palette_code.h`), which shifts it by the site's group
(a value other than the decompiled one is logged and shifted as it is), or, for d_a_mant, a cached shifted
copy of the cape palette (`pc_rando_palette_mant_tlut`: RGB5A3 decode, `hsv_shift_palette`, encode).
Checked by the `palettes` smoke and `native/tools/rando/palette_check.py` (wwrando's own
`shift_colors_for_file_group` on the disc's Pscene files and RELs), `regress_rando/R7.2e.txt`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_tn.cpp | `yoroi_break`: `prim[i]` / `env[i]` (12 armour-break colours) through `pc_rando_palette_rgb(PCR_PALSITE_TN, ...)` | #if/else | palettes.py `shift_hardcoded_darknut_colors` d_a_tn.rel 0xE2AC |
| game/src/d/actor/d_a_mant.cpp | static registration of `pal_d` (smoke only); `daMant_packet_c::draw`: `palette = pc_rando_palette_mant_tlut(mTexNo, pal_d[mTexNo])` | #if/else | `shift_hardcoded_darknut_colors` d_a_mant.rel 0x4540, 0x6560, 0x8580, 0xA5A0, 0xC5C0 (RGB5A3 C4 palettes) |
| game/src/d/actor/d_a_mo2.cpp | `ke_disp`: spear thread colour of `m3Dline.update` | #if/else | `shift_hardcoded_moblin_colors` d_a_mo2.rel 0xD648 |
| game/src/d/actor/d_a_boko.cpp | `daBoko_c::keDraw`: thread colour of `lineMat.update` | #if/else | `shift_hardcoded_moblin_colors` d_a_boko.rel 0x4488 |
| game/src/d/actor/d_a_st.cpp | `ke_disp`: hat thread colour | #if/else | `shift_hardcoded_stalfos_colors` d_a_st.rel 0x9F30 |
| game/src/d/actor/d_a_nz.cpp | `tail_draw`: tail colour | #if/else | `shift_hardcoded_rat_colors` d_a_nz.rel 0x8FB8 |
| game/src/d/actor/d_a_npc_nz.cpp | `daNpc_Nz_c::_draw`: tail colour | #if/else | `shift_hardcoded_rat_colors` d_a_npc_nz.rel 0x48C0 |
| game/src/d/actor/d_a_cc.cpp | `cc_eff_set`: `eff_color_dt[mColorType]`; `action_damage_move` case 0x2D: the CCSPLIT00 multipliers 40/16/39 | #if/else | `shift_hardcoded_chuchu_colors` d_a_cc.rel 0x7F88 (5 colours) and floats 0x7E9C/0x7EBC/0x7EC0 (misc_rando_features.asm d_a_cc 0x33A4/0x3B14/0x7E9C) |
| game/src/d/actor/d_a_bgn.cpp | `daBgn2_Draw`, `daBgn3_Draw`: red rope colours of `mRedRopeMat.update` | #if/else | `shift_hardcoded_puppet_ganon_colors` d_a_bgn.rel 0xF0A8, 0xF0B0 |
| game/src/d/actor/d_a_bgn3.cpp | `daBgn3_Create`: `ke_color` from `l_HIO.m50/m52/m54` | #if/else | `shift_hardcoded_puppet_ganon_colors` d_a_bgn3.rel u8 0x2CF/0x2D7/0x2DF (li immediates of the HIO constructor) |
| game/src/d/actor/d_a_gnd.cpp | `daGnd_Draw`: thread colour of `mLineMat.update` | #if/else | `shift_hardcoded_ganondorf_colors` d_a_gnd.rel 0x8F1C |

## R7.4c

Enemy rando particles (`enemies.py` `particles_to_load_for_each_jpc_index` at the end of
`save_changed_enemies_and_randomize_their_params`, and `update_loaded_particles`). No hunk in `game/`: a dzx
patcher (`features/rando/pc_rando_enemy_particles.{h,cpp}`, registered last in `pc_rando_patchers.def` through
`game_hooks/pc_rando_hooks_enemy_particles.cpp`, which runs it in a host allocation scope) reads the place's
bank from the raw file as the game loads it (non-sea stage folders: `stage.dzs` STAG[0] `loaded_particle_bank`,
mask 0x07F8 of params_2; sea rooms: `room.dzr` FILI[0], mask 0x1FE00000) and registers each placed type's
"Required particle IDs" as an `any` import into that bank with R7.1b's registry. The game parses both files
before `dPa_control_c::readScene`, so the bank R7.1b builds in `createRoomScene` holds them; an id the bank
already has is skipped, as in wwrando. Imports accumulate for the activation, so a bank shared by several places
(Pscene043, 078, 080, 220, sea bank 200, ...) holds the ids of the places loaded so far rather than of all of
them, as wwrando's file does; the place being loaded always has its own. Checked by the `enemyptcl` smoke and
`native/tools/rando/enemy_particle_check.py` (wwrando's own code with gclib on the disc),
`regress_rando/R7.4c.txt`.

## R7.4d

Enemy randomizer option live (experimental, D18); no new hunk in `game/`. The menu row "Enemy randomizer
(experimental, can be unbeatable)" (`COS_RANDO_RANDOMIZE_ENEMIES`, `pc_rando_settings.cpp`; the menu
validates with `ensureValidCombination(o, allowUnbeatable = true)`), the `randomize_enemies` refusal in
`pc_rando_gen.cpp` removed. Heap fix (the R7.4b stage-change crash): R7.4a's results and parsed seed are
static storage read in place (`pc_rando_enemies.cpp`), and `game_hooks/pc_rando_hooks_resources.cpp` runs
both archive dispatches and the patchers' init functions in a `JKRPcHostAllocScope`, so no patcher's
lasting state lands on an archive heap (grown files still use `file.alloc` on the archive heap); the
palette shifts are static (`pc_rando_palettes.cpp`) and the R5.3 hints parse runs in a host scope
(`pc_rando_hooks_hints.cpp`). Checked by `native/tools/rando/enemy_sweep.py` (smoke `enemysweep`).

## R7.6a

European disc, build and region gates (plan R7.6a). D16's `kRegionOk` is true on both builds
(`pc_rando_settings.cpp`, `pc_rando_menu.cpp`, `game_hooks/pc_rando_hooks_fileselect_newgame.cpp`), so the
Randomizer tab, Generate and the file select's question work with the European disc. The sidecar folder already
followed the build's ID (`pc_rando_slot_card_dir`: `<userPath>/EUR/Card A`). Its hunks in `game/` are the PAL
copies of USA-only hunks, listed in their own steps' sections (R3.2, R4.4); `rando_hook_census.py` now reads every
file once per build (check 5, "Regions") and reports a step whose marker count differs between the USA and the
PAL view.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks of its own: the PAL copies carry their steps' markers (R3.2, R4.4) | - | - |

## R7.6b

European disc, stage data and location keys (plan R7.6b). No hunks: the EUR disc's stage data is the USA's
for every randomizer patch.

- `native/tools/rando/gen_location_keys.py --disc GZLP01` (compare mode, writes nothing) reads the EUR disc with
  R0.5's checks: the 391 dzx item fingerprints and the 11 event actions hold the original items, and its 425
  DZX/ADDED/EVENT keys (chunk, index, field, fingerprint name and byte, event and action indices, prm0) equal
  the checked-in table's, so the EUR build uses the same `generated/rando_location_keys.inc` (no
  `rando_location_keys_pal.inc`). The 86 CODE keys only record wwrando's USA REL/DOL offsets; the port patches
  decompiled code there, so their PAL bytes are not compared (60 of the 77 USA item offsets happen to hold the
  item on the EUR disc too).
- `native/tools/rando/location_sweep.py targets --check --disc ~/Documents/windwaker/GZLP01/GZLP01.iso` (`--disc`
  takes the .iso, not a game ID): the sweep's stops (rooms, spawn points, layers) are the same on the EUR disc
  (402 keys). It reads (and, for another disc, rewrites) the disc manifest of `$COS_BUILD_DIR` (default
  `build/native-mac`): run it with `COS_BUILD_DIR=build/native-mac-eur`, or again with the USA disc afterwards.
- Fingerprint audit: the `fpaudit` smoke (`harness/smokes/pc_rando_smoke_fpaudit.cpp`) runs every stage.dzs,
  room.dzr and event_list.dat of the disc (805 files) through the whole patcher chain of `pc_rando_patchers.def`
  with the active seed; `native/tools/rando/fp_audit.py` runs it with the rotation placement plus every
  stage-data option (`fixtures/R7_6b_all_options.json`, generated by `cos_rando_cli` with every option on) on
  both builds: no mismatch or refusal line on either disc, and the patchers' own lines are identical on both
  (only the smoke's size line of figureG/Room0, which no patcher touches, differs). room.dzb is not part of the
  audit (R3.15's dzb patcher runs after `cBgS::ConvDzb`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/` | - | - |

## R8.1

File select "Normal / Randomizer" question (Phase R8, revising D6). With the setting `COS_RANDO_ASK_NEWGAME`
on (row "Ask on New Game", order 90 of the Randomizer tab; default on, off in harness runs that do not set
it), A on a blank slot asks "Start a Randomizer file?" with the panes of the corrupted-slot prompt (message
pane + Yes/No, the existing `dataSelectPaneMove` -> `YesNoSelect` sequence with `field_0x392a == 4`), in
place of the info panel and the Start menu; No is preselected, so A, A still reaches the name entry. Yes
uses the pending seed or generates the menu settings' seed on R4.3's job (wait state in `YesNoSelect`); No
makes a normal file and R0.7's New Game hook (`pc_rando_hooks_slot.cpp` `newGameBegin`, one check of
`pc_rando_fs_newgame_take_choice`) leaves the pending seed pending; B and a generation error go back
through `CmdExecPaneMove0` / `backDatSelPaneMove` as the erase prompt's No. Like R0.7's and R4.4's hooks
they are not under `pc_rando()` (the file select runs with it off): the guard is the setting and a blank
slot (`pc_rando_fs_newgame_ask` returns 0, `pc_rando_fs_newgame_asking` stays 0, and every hunk takes the
vanilla branch). Texts: R3.20 overlay records 0xFF00-0xFF05 read by the hook only. Code in
`game_hooks/pc_rando_hooks_fileselect_newgame.cpp` (API `features/rando/pc_rando_fsnewgame.h`). Checked by
the `fsnewgame` smoke (`regress_rando/R8.1.txt`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_file_select.cpp | `dFile_select_c::dataSelect`: after A's vanilla code, `pc_rando_fs_newgame_ask(dataNew && !corrupted, ...)` writes the question title and the seed line | add | none (plan R8.1) |
| game/src/d/d_file_select.cpp | `dFile_select_c::dataSelectPaneMove`: message + Yes/No panes and proc 0xB also while `pc_rando_fs_newgame_asking()` (two conditions) | if/else | none (plan R8.1) |
| game/src/d/d_file_select.cpp | `dFile_select_c::YesNoSelect`: while asking, `pc_rando_fs_newgame_yesno` (START: selectEnd 1 as menuSelect's Start; BACK: as B; WAIT: generating or error shown) | add | none (plan R8.1) |
| game/src/d/d_file_select.cpp | `dFile_select_c::YesNoSelect` (R8.3, marker R8.1): ASK2, the second question: cursor to Yes (`field_0x3928 = 0`, `yesNoCursorMove`) | add | none (plan R8.3) |
| game/src/d/d_file_select.cpp | `dFile_select_c::CmdExecPaneMove0`: the message pane leaves (not the info panel) while asking | if/else | none (plan R8.1) |
| game/src/d/d_file_select.cpp | `dFile_select_c::closeEnd`: the message and Yes/No panes leave (as `closeCardErr`) while asking | if/else | none (plan R8.1) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::dataSelect`: the same `pc_rando_fs_newgame_ask` call in the save-slot branch (not the PAL language slot, `selectNum == 3`) | add | none (plan R8.1) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::dataSelectPaneMove`: as the USA hunk (two conditions) | if/else | none (plan R8.1) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::YesNoSelect`: as the USA hunk; START sets PAL's nextModeWait (`VERSION_SELECT(0x26, 0x26, 0x26, 0x29)`, the language states come first) | add | none (plan R8.1) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::YesNoSelect` (R8.3, marker R8.1): as the USA hunk | add | none (plan R8.3) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::CmdExecPaneMove0`: as the USA hunk | if/else | none (plan R8.1) |
| game/src/pal/d/d_file_select.inc | `dFile_select_c::closeEnd`: as the USA hunk | if/else | none (plan R8.1) |

Language: English on the USA build (the game's own language). On the PAL build the texts follow the game's
language (`dComIfGs_getPalLanguage`): Spanish for Spanish, English for the other three; wwrando's error line
stays in English. The PAL hunks are in place; R7.6a opened D16, so the PAL build asks too.

R8.3 extends this section (its one hunk per file is in the same `YesNoSelect` branch, marked `pc_rando(R8.1)`
with an R8.3 note). Yes on the question asks a second one, "Make a new seed?", when there is a seed (a pending
one, or a name in `COS_RANDO_SEED`): the hook writes the new title and "Current seed: <name>" (plus "Hash:
<words>" for a pending seed) into the same buffers and returns `PC_RANDO_FSNG_ASK2`, and the game moves the
cursor to Yes. Yes stores a new random name in `COS_RANDO_SEED` (as the menu's "Random seed" row; kept when the
environment sets it) and generates it; No uses the pending seed, else generates the setting's name; B goes back
as on the first question. With no seed at all the second question is skipped and a new random name is
generated. Texts: overlay records 0xFF06-0xFF08 (English; Spanish on the PAL build in Spanish). Checked by the
`fsnewgame` smoke (`regress_rando/R8.3.txt`; the R8.1 lines answer No through `rando-fsnewgame-yes.txt`).

## R7.6c

European disc, messages and text (plan R7.6c). Findings on GZLP01 (gclib over the five
`/res/Msg/data<N>/bmgres.arc`, N = 0 English, 1 German, 2 French, 3 Spanish, 4 Italian):
- **Message numbers:** every language's `zel_00.bmg` has USA's 4327 message numbers, no more and no less, so
  the overlay's numbers (R0.12) are the same on both discs. The INF1 fields are USA's too, apart from
  `unknown_1` of 27-34, 40 and 78-81, and the language menu's own entries 14500-14507, which no step edits.
  The English BMG differs from USA's in 7 texts (3308, 4454, 5289, 10016 and three language names), none of
  them a text wwrando reads or edits.
- **Font:** `fontres.arc`'s `rock_24_20_4i_usa.bfn` is the same file on both discs (same SHA-1), and the
  European game uses it in every language (`mDoExt_initFont0`). So R3.4's 22 pre-wrapped item-get texts
  (wrapped by `gen_dungeon_item_msgs.py` with the USA font) break where a run-time wrap with the PAL font
  breaks; the `eurtext` smoke checks this at run time, and no run-time wrap was added.
- **Language rule:** wwrando's texts stay English in all five languages (logged as a deviation of R7.6c).
  The overlay texts are built from wwrando's English strings and need no change; the R3.21 texts built from
  vanilla text (chart numbers in the chart item-get texts, line 3 of Beedle's refusal 3957, the Rock Spire
  letter 3325) read the English BMG in German, French, Spanish and Italian:
  `game_hooks/pc_rando_hooks_paltext.cpp` reads `/res/Msg/data0/bmgres.arc`'s `zel_00.bmg` from the disc
  into host memory (about 640 KB) the first time such a text is built in an activation, finds messages in it
  with `features/rando/pc_rando_bmg.cpp` (`pcr::bmgFind`), and frees it when `pc_rando()` turns 0. On the
  USA build and in English the loaded BMG is used, as before. Without this, those texts mixed German (etc.)
  vanilla lines with English ones, and the chart numbers were missing.
- **Display:** in German, French, Spanish and Italian the European game centres every line whose entry has
  text alignment 0 (`fopMsgM_msgDataProc_c::stringShift`), wwrando's new messages included, as it does for
  the game's own texts in those languages; the lines keep the widths wwrando wraps them to.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: `pc_rando_hooks_posttext.cpp`'s `bmgText` asks `pc_rando_en_bmg_message` first (native code, only under `pc_rando()`) | - | tweaks.py `add_chart_number_to_item_get_messages`, `update_beedle_spoil_selling_text`, `update_item_names_in_letter_advertising_rock_spire_shop` |

Checked by the `eurtext` smoke (`regress_rando/R7.6c.txt`) and `cos_rando_test_R7_6c`; on the EUR build, in
each of the five languages, the `eurtext`, `posttext`, `msg`, `hoho`, `dkeys`, `keybag` and `wrap` smokes pass
and the `eurtext` dump of every overlay record is byte-identical to USA's.

## R7.6d

European build's gate (plan R7.6d). No hunks; tooling and runs only.

- **regress.sh on EUR:** `COS_BUILD_DIR=build/native-mac-eur COS_DISC=~/Documents/windwaker/GZLP01/GZLP01.iso
  native/tools/regress.sh` (the build configured with `-DCOS_VERSION=3`, `COS_GAME_ID` GZLP01). regress.sh reads
  the build's game ID before it lists the run lines (`region_filter`): a target prefixed `eur:` runs only on the
  EUR build, `usa:` only on the USA build, the prefix is dropped before the line runs, and an unprefixed line
  runs on both (in English on EUR, run.sh's default language). The USA list is unchanged (`--list` identical to
  feature/randomizer's). `regress_rando/R7.6d.txt` holds the EUR-only lines: the EUR fingerprint audit and
  R7.6c's `eurtext`, `posttext`, `msg`, `hoho`, `dkeys`, `keybag` and `wrap` lines in German, French, Spanish
  and Italian. R7.6b's `fp_audit.py --region usa` line got the prefix `usa:` (on EUR `COS_DISC` is the EUR disc,
  which that line would boot the USA build with).
- **Location sweep on EUR** (by hand; long):
  `COS_BUILD_DIR=build/native-mac-eur python3 -I native/tools/rando/location_sweep.py run --tier both
  --placement <identity|rotation|fixture.json> --disc ~/Documents/windwaker/GZLP01/GZLP01.iso`. `--disc` takes
  the .iso. `targets --check` on EUR: `COS_BUILD_DIR=build/native-mac-eur ... targets --check --disc <EUR .iso>`
  (402 keys up to date; the manifest it reads is `$COS_BUILD_DIR`'s, so the USA one stays).
- **Results (2026-10-25, feature/randomizer 8a015342 + this step, no code change):**
  - T1 on EUR with `identity`, `rotation`, `R0_6_TestSeed1.json`, `R5_3_Heavy5.json` and
    `R1_6_CheckSeed11_dungeons_only.json`: 0 failures each (220 actr/scob rows, 182 chest keys record and live;
    the sea_1_0 run's last stop is retried in its own boot, `.boot0`, on USA too).
  - T2 is not at 0 failures on either region: the T2 driver of the `loc` smoke (R9.1's, in progress) fails about
    50 of the 182 chest keys on USA as well (the put of the player does not hold, the chest is gone or is not
    the chest's event at the stop, A does not open it). Region comparison, key by key: `identity` 182/182 rows
    the same result on both (127 ok, 53 FAIL, 2 no-target); `rotation` 181/182 the same, the one difference
    (Six Eye Reef TRES[1]) a put-retry case that passes on EUR and fails TRES[0] on USA when rerun alone;
    `R0_6_TestSeed1.json` 182/182 the same (128 ok, 52 FAIL, 2 no-target). Heavy5 and CheckSeed11 got T1 only (time).
  - The `jpcscene`, `warppots`, `palettes`, `cosmetic` and `enemies` lines (the R7.1c, R7.2e, R7.4d, R7.5a regress
    lines with their `enemy_check` / `palette_check` / `enemy_sweep` scripts, 46 lines) and every other game line
    of `regress_rando/` (197 lines) pass on EUR in English; the R7.6 lines (112, with English) pass in all five
    languages; `fp_audit.py` (both regions): 0 problem lines on either disc, the 1594 patcher lines identical.
  - Left for the user (console): Milestone R4's console list again with the EUR NRO.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/` | - | - |

## R9.1

Location sweep coverage (plan R9.1). No hunks in `game/`; the sweep tool, its targets and smokes only.

- **Kinds:** `native/tools/rando/location_sweep.py` covers every location wwrando has (320 on USA, none
  without a kind): `chest` (loc smoke, T1 record + live and T2 open), `actr` and `scob` (R2.2/R2.3 smokes),
  `boss` (`bossloc`: Bitem for the stage and for stage 2, `createItemForBoss`, the disappear cloud; T2 picks
  up a Bitem), `event` (`evloc`, T2 per event name), `added` (`addchest`, T2 opens it) and `code` (`codeloc`:
  every Code key's lookup, the code-C/people/Kane/boss/added-chest/shop tables and the storage-actor calls).
  Kinds live in `sweep_targets.json` "kinds"; a kind with hand-written per-stage `stops` and a `lines` parser
  reads per-location check lines.
- **loc smoke T2 drivers:** a type-0 Fire actor burning on the chest gets its switch set (DRC Big Key chest);
  the stage's enemies are deleted while T2 prepares (an enemy-kill chest, function 2, then appears by its own
  event), except on the sea and the Ghost Ship; another actor's event (a shutter, a camera) sends T2 back to
  its preparation, up to four times; a stop on the boat starts the game's get-off first; a switch-visible
  chest (function 3) gets its switch set. `bossloc` T2 accepts an item `daItem_c::itemGetExecute` gives without
  a get-item event (rupees, hearts, ammo, a spoil or bait already seen: wwrando marks them all seen) when the
  field item is picked up and the save changed.
- **xfails:** each listed in `sweep_targets.json` overrides with a one-line reason; `location_sweep.py` counts
  a listed xfail (now also on a key without a stop for that tier) apart from failures. T2 only: the Ghost
  Ship (5 keys), Dragon Roost's boulder chest, Fire Mountain's two lookout chests, Ice Ring Isle's frozen
  chest, the Ice Ring Isle and Fire Mountain caves, the Stone Watcher and Overlook caves (room 6, no spawn
  point: needs a stage driver), Orca's Hurricane Spin lesson and the Tower of the Gods' stone tablet (needs a
  Command Melody driver).
- **Results (2026-10-26, rando/R9.1 on feature/randomizer c1dfc8f8, USA):** T1 on `identity`, `rotation` and
  the ww_export seeds SweepSeedOne/Two/Three: 0 failures each (T1 ok 329, T1-record ok 182, T1-live ok 182,
  320/320 locations). T2 (every kind) on `rotation` and SweepSeedOne: ok 190, xfail 15, 0 failures each.
  T2 boss, event and added on `identity`, SweepSeedTwo and SweepSeedThree: 0 failures apart from the listed
  xfails (boss rerun with the bossloc fix: ok 8 on all five). Not run (time, about 45 min each): T2 chest on
  `identity`, SweepSeedTwo and SweepSeedThree; command:
  `python3 -I native/tools/rando/location_sweep.py run --tier t2 --kinds chest --placement <identity|seed json>`.
- `regress_rando/R9.1.txt` (prefix `usa:`): codeloc (rotation, none), bossloc T2 (Gohma rotation, Kalle Demos
  with R0_6_TestSeed1's Joy Pendant), bossloc in Helmaroc King's tower, and loc T2 samples of each driver.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/` | - | - |
