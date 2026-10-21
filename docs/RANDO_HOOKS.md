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

## R6.2

Key-lunacy needs no hunk. The generator already follows `options.keylunacy` (R1.3/R1.4: no dungeon
item prerandomization, `check_item_valid_in_location` drops the own-dungeon rule, and the logic's
`Option "keylunacy"` requirements). At runtime the dungeon items are R3.4's 22 item IDs, whose item
funcs already pick the stage by where the player is.

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/` (smoke: `pc_rando_smoke_dkeys.cpp` step 6; golden: `check_seeds.py` keylunacy presets) | - | `randomizers/items.py` keylunacy branches; `custom_funcs.asm` `generic_small_key_item_get_func` |
