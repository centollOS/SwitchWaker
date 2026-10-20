// The game side of the randomizer's per-dungeon item IDs (docs/RANDOMIZER_PLAN.md step R3.4, asm
// report group G3; the table and the decisions are features/rando/pc_rando_dungeon_items.{h,cpp}):
//   - generic_on_dungeon_bit and generic_small_key_item_get_func (custom_funcs.asm:974-1071) and the
//     22 wrappers, registered as item-get overrides (pc_rando_items.h) for the 22 IDs of tweaks.py
//     allow_dungeon_items_to_appear_anywhere; execItemGet calls them under pc_rando() (R0.8), so no
//     hunk in game/ is needed;
//   - the item_resource / field_item_res / item_info entries of those IDs, copied from the vanilla
//     small key, Big Key, map and compass (table edits, applied at every activation, restored after);
//   - the 22 item-get messages (101 + id), added to the message overlay at every activation (the
//     overlay is cleared when pc_rando() turns 0).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_dungeon_items.h"
#include "features/rando/pc_rando_items.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "d/d_stage.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace {

// The save stage the player is in: the byte at 0x803C53A4 that custom_funcs.asm reads.
int currentStage() {
    return g_dComIfG_gameInfo.save.getDan().mStageNo;
}

// generic_on_dungeon_bit(stage, bit): onDungeonItem on the loaded stage's memory when the player is
// in that dungeon, else on the dungeon's saved stage info.
void onDungeonBit(int stage, int bit) {
    dSv_memBit_c& mem = pc_rando_dungeon_bit_target(currentStage(), stage) == PCR_DITEM_TARGET_CURRENT
                            ? g_dComIfG_gameInfo.save.getMemory().getBit()
                            : g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
    mem.onDungeonItem(bit);
}

// generic_small_key_item_get_func(stage).
void smallKey(int stage) {
    stage_stag_info_class* stag = dComIfGp_getStageStagInfo();
    // custom_funcs.asm reads the STAG without a NULL test; without one, the key goes straight into the
    // current memory as in a boss room.
    const int keyDisp = stag != nullptr && dStage_stagInfo_ChkKeyDisp(stag) != 0;
    switch (pc_rando_dungeon_key_target(currentStage(), stage, keyDisp)) {
    case PCR_DITEM_KEY_ITEM_FUNC:
        item_func_small_key();
        break;
    case PCR_DITEM_TARGET_CURRENT: {
        dSv_memBit_c& mem = g_dComIfG_gameInfo.save.getMemory().getBit();
        mem.setKeyNum(mem.getKeyNum() + 1);
        break;
    }
    default: {
        dSv_memBit_c& mem = g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
        mem.setKeyNum(mem.getKeyNum() + 1);
        break;
    }
    }
}

void ensureTableEdits();

// The 22 wrappers (drc_small_key_item_get_func, ...): one instantiation per table entry.
template <int I> void dungeonItemFunc() {
    ensureTableEdits();
    const PcRandoDungeonItem* e = pc_rando_dungeon_item(I);
    if (e->kind == PCR_DITEM_SMALL_KEY) {
        smallKey(e->stage);
    } else {
        onDungeonBit(e->stage, e->kind);
    }
}

template <int... I> struct FuncList {
    static constexpr PcRandoItemFunc kFuncs[sizeof...(I)] = {&dungeonItemFunc<I>...};
};
using AllFuncs = FuncList<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21>;
static_assert(sizeof(AllFuncs::kFuncs) / sizeof(AllFuncs::kFuncs[0]) == PCR_DITEM_COUNT);

// Registers `entry` of `table`, index `id`, as edits of at most PC_RANDO_TABLE_EDIT_MAX bytes each.
bool editEntry(int table, int id, const void* entry, size_t size) {
    bool ok = true;
    for (size_t off = 0; off < size; off += PC_RANDO_TABLE_EDIT_MAX) {
        const size_t len = size - off < PC_RANDO_TABLE_EDIT_MAX ? size - off : PC_RANDO_TABLE_EDIT_MAX;
        ok = pc_rando_table_edit(table, id, off, (const u8*)entry + off, len) && ok;
    }
    return ok;
}

// tweaks.py allow_dungeon_items_to_appear_anywhere, table part. In GameCube offsets it copies, from
// the base item:
//   item_resource: the arc and icon name pointers, +8..+0x14 (the model/animation indexes and
//     mTevFrm), +0x15..+0x1B (mItemMesgNum, mUnused), +0x1C (mUnknown), +0x20 (mHeapSize): the whole
//     entry;
//   field_item_res: the arc pointer and, from item_resource[base], the indexes, mTevFrm and mUnknown,
//     plus mHeapSize "from field_item_res[base]";
//   item_info: 0x14281E05 for a small key, 0x00282805 for a Big Key, 0x00282800 for a map or compass
//     (the "does not fade out" flag included).
// wwrando reads that last field_item_res heap size with the item_resource stride (base * 0x24 instead
// of base * 0x1C), so it gets bytes of an unrelated entry; the port uses field_item_res[base].mHeapSize,
// the value the code means (the heap size is only an estimate here: fopAcM_entrySolidHeap retries with
// the largest free block when it is too small).
bool registerTableEdits() {
    bool ok = true;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* e = pc_rando_dungeon_item(i);
        const dItem_data_item_resource& baseRes = dItem_data::item_resource[e->base_id];
        ok = editEntry(PCR_TBL_ITEM_RESOURCE, e->id, &baseRes, sizeof baseRes) && ok;

        dItem_data_field_item_res field = dItem_data::field_item_res[e->id];
        field.mArc = baseRes.mArcname;
        field.mBmdIdx = baseRes.mBmdIdx;
        field.mSrtIdx = baseRes.mSrtIdx;
        field.mSrtIdx2 = baseRes.mSrtIdx2;
        field.mTevIdx = baseRes.mTevIdx;
        field.mTevIdx2 = baseRes.mTevIdx2;
        field.mBckIdx = baseRes.mBckIdx;
        field.mTevFrm = baseRes.mTevFrm;
        field.mUnknown = baseRes.mUnknown;
        field.mHeapSize = dItem_data::field_item_res[e->base_id].mHeapSize;
        ok = editEntry(PCR_TBL_FIELD_ITEM_RES, e->id, &field, sizeof field) && ok;

        dItem_data_item_info info;
        info.mShadowSize = e->kind == PCR_DITEM_SMALL_KEY ? 0x14 : 0x00;
        info.mCollisionH = 0x28;
        info.mCollisionR = e->kind == PCR_DITEM_SMALL_KEY ? 0x1E : 0x28;
        info.mFlag = e->kind == PCR_DITEM_SMALL_KEY || e->kind == PCR_DITEM_BIG_KEY ? 0x05 : 0x00;
        ok = editEntry(PCR_TBL_ITEM_INFO, e->id, &info, sizeof info) && ok;
    }
    return ok;
}

bool sEditsRegistered = false;

// Registers the table edits once. At the first activation, not at static init; and only once
// item_resource[] holds its data: it reads item_arcname_tbl[], so a compiler may initialize it
// dynamically, after a COS_RANDO_FORCE boot has already called the listener (clang folds it into
// constant data, so on the Mac this never waits). Until then it is retried at every activation and
// at every dungeon item get. The edits then stay registered, so every later activation applies them
// again (R0.8 restores the tables in between).
void ensureTableEdits() {
    if (sEditsRegistered || dItem_data::item_resource[PCR_DITEM_BASE_SMALL_KEY].mArcname == nullptr) {
        return;
    }
    sEditsRegistered = true;
    if (!registerTableEdits()) {
        fprintf(stderr, "[cos] rando: dungeon items: some table edits were refused\n");
    }
}

void onRando(int active) {
    if (!active) {
        return;
    }
    ensureTableEdits();
    const int added = pc_rando_dungeon_items_add_messages();
    if (added != PCR_DITEM_COUNT) {
        fprintf(stderr, "[cos] rando: dungeon items: %d of %d item-get messages added\n", added, PCR_DITEM_COUNT);
    }
}

} // namespace

// Overrides only act under pc_rando() (execItemGet's R0.8 hunk), so they are registered once, before
// main. Called from R0.8's table binding (game_hooks/pc_rando_hooks_items.cpp, static init): cos_pc is
// a static library, so a file that nothing references is not linked, and a static constructor of its
// own would never run.
extern "C" void pc_rando_dungeon_items_register(void) {
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        pc_rando_item_override(pc_rando_dungeon_item(i)->id, AllFuncs::kFuncs[i]);
    }
    pc_rando_add_listener(onRando);
}
