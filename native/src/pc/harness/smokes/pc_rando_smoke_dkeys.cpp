// COS_SMOKE=rando COS_RANDO_SMOKE=dkeys (docs/RANDOMIZER_PLAN.md step R3.4): the 22 per-dungeon item
// IDs (features/rando/pc_rando_dungeon_items.h, game_hooks/pc_rando_hooks_dungeon_items.cpp), e.g.
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=dkeys
//   run.sh rando --stage M_NewD2:0 --env COS_RANDO_SMOKE=dkeys
// Once the play scene has run 10 frames:
//   1. a plain boot (not forced) is vanilla: execItemGet(0x13) changes no key count and the 22 IDs'
//      item_resource / field_item_res / item_info entries hold their boot bytes;
//   2. activation: each ID's item_resource entry equals its base item's (small key, Big Key, map,
//      compass), field_item_res takes the base's arc, model indexes and heap size, item_info holds
//      wwrando's values, and message 101 + id is in the overlay as an ITEM_GET box;
//   3. outside the dungeons (sea:44): each small key raises its dungeon's saved key count by 1 and
//      nothing else; each map, compass and Big Key sets its bit (0, 1, 2) in its dungeon's saved
//      stage info; the loaded stage's memory and the key counter stay as they were;
//   4. inside DRC (M_NewD2, stage 3): the DRC small key goes through item_func_small_key (the key
//      counter rises by 1; in a room without the key display the memory's key count instead), the
//      DRC map sets the loaded memory's map bit, and an FW key and map still go to FW's saved info;
//   5. a field DRC small key and an FF map (daItem, fopAcM_createItem) are spawned in front of the
//      player and still exist with a model 30 frames later;
//   6. key-lunacy (R6.2), outside the dungeons only: once no event runs (A through any opening talk),
//      a field FW small key (0x1D, action 2: it pops up and falls back on the player) is picked up by
//      touch; the item-get demo must carry 0x1D and end, and afterwards FW's saved key count is +1
//      while the loaded memory, the key counter and every other stage's saved info are unchanged
//      (generic_small_key_item_get_func's "other stage" branch, reached through a real pickup), and the
//      field key deletes itself;
//   7. deactivation (not forced): the tables are back to their boot bytes, the messages are gone and
//      execItemGet(0x13) is vanilla again.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "features/rando/pc_rando_dungeon_items.h"
#include "features/rando/pc_rando_msg.h"
#include "d/actor/d_a_item.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "d/d_item.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "dkeys";
unsigned int sEndFrame = 0;
fpc_ProcID sKeyId = fpcM_ERROR_PROCESS_ID_e;
fpc_ProcID sMapId = fpcM_ERROR_PROCESS_ID_e;

// R6.2: the FW small key picked up on the sea stage.
constexpr u8 kFwKey = 0x1D;                 // FW Small Key (tweaks.py dungeon_items)
constexpr int kFwStage = 4;                 // FW's save stage number
constexpr unsigned int kSettleFrames = 60;  // no event for this long before the key is spawned
constexpr unsigned int kStartFrames = 300;  // the touch must start the get demo within this
constexpr unsigned int kEventFrames = 3600; // the get demo must end within this
constexpr unsigned int kTalkFrames = 45;
constexpr unsigned int kGoneFrames = 120;  // the field key deletes itself within this after its demo
enum Pickup { kPickOff, kPickWait, kPickStart, kPickEvent, kPickGone, kPickDone };
Pickup sPick = kPickOff;
unsigned int sPickSince = 0;
fpc_ProcID sFwKeyId = fpcM_ERROR_PROCESS_ID_e;

dItem_data_item_resource sBootRes[0x100];
dItem_data_field_item_res sBootField[0x100];
dItem_data_item_info sBootInfo[0x100];

bool check(const char* what, bool ok) {
    return randoSmokeCheck(kName, what, ok);
}

bool checkf(bool ok, const char* fmt, int a, int b = 0) {
    char what[160];
    snprintf(what, sizeof what, fmt, a, b);
    return check(what, ok);
}

dSv_memBit_c& saved(int stage) {
    return g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
}
dSv_memBit_c& memory() {
    return g_dComIfG_gameInfo.save.getMemory().getBit();
}
int curStage() {
    return g_dComIfG_gameInfo.save.getDan().mStageNo;
}

bool tablesAtBoot() {
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const int id = pc_rando_dungeon_item(i)->id;
        if (memcmp(&sBootRes[id], &dItem_data::item_resource[id], sizeof sBootRes[id]) != 0 ||
            memcmp(&sBootField[id], &dItem_data::field_item_res[id], sizeof sBootField[id]) != 0 ||
            memcmp(&sBootInfo[id], &dItem_data::item_info[id], sizeof sBootInfo[id]) != 0) {
            return false;
        }
    }
    return true;
}

// The whole save/memory state the item funcs may touch: 8 stage infos (0..7), the memory, the counter.
struct State {
    dSv_memBit_c save[8];
    dSv_memBit_c mem;
    s16 keyCount;
};
State capture() {
    State s;
    for (int i = 0; i < 8; i++) {
        s.save[i] = saved(i);
    }
    s.mem = memory();
    s.keyCount = dComIfGp_getItemKeyNumCount();
    return s;
}
bool sameBit(const dSv_memBit_c& a, const dSv_memBit_c& b) {
    return memcmp(&a, &b, sizeof a) == 0;
}
State sPickBefore;

void checkTables() {
    bool res = true, field = true, info = true;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* e = pc_rando_dungeon_item(i);
        const dItem_data_item_resource& r = dItem_data::item_resource[e->id];
        const dItem_data_item_resource& br = dItem_data::item_resource[e->base_id];
        res = res && r.mArcname == br.mArcname && r.mTexture == br.mTexture && r.mBmdIdx == br.mBmdIdx &&
              r.mSrtIdx == br.mSrtIdx && r.mSrtIdx2 == br.mSrtIdx2 && r.mTevIdx == br.mTevIdx &&
              r.mTevIdx2 == br.mTevIdx2 && r.mBckIdx == br.mBckIdx && r.mTevFrm == br.mTevFrm &&
              r.mItemMesgNum == br.mItemMesgNum && r.mUnknown == br.mUnknown && r.mHeapSize == br.mHeapSize;
        const dItem_data_field_item_res& f = dItem_data::field_item_res[e->id];
        field = field && f.mArc == br.mArcname && f.mBmdIdx == br.mBmdIdx && f.mBckIdx == br.mBckIdx &&
                f.mTevIdx == br.mTevIdx && f.mTevFrm == br.mTevFrm && f.mUnknown == br.mUnknown &&
                f.mHeapSize == dItem_data::field_item_res[e->base_id].mHeapSize;
        const dItem_data_item_info& n = dItem_data::item_info[e->id];
        const u32 want = e->kind == PCR_DITEM_SMALL_KEY ? 0x14281E05u
                         : e->kind == PCR_DITEM_BIG_KEY ? 0x00282805u
                                                        : 0x00282800u;
        const u32 got = ((u32)n.mShadowSize << 24) | ((u32)n.mCollisionH << 16) | ((u32)n.mCollisionR << 8) | n.mFlag;
        info = info && got == want;
    }
    check("active: item_resource of the 22 IDs = their base item's", res);
    check("active: field_item_res of the 22 IDs from the base item", field);
    check("active: item_info of the 22 IDs = wwrando's values", info);
    check("active: DRC small key's arc is set", dItem_data::item_resource[0x13].mArcname != nullptr);

    bool msgs = true;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* e = pc_rando_dungeon_item(i);
        const PcRandoMsg* m = pc_rando_msg_find((u16)(101 + e->id));
        msgs = msgs && m != nullptr && m->has_entry && m->entry.text_box_type == 9 &&
               m->entry.display_item_id == e->id && pc_rando_msg_hook_text((u16)(101 + e->id)) != nullptr;
    }
    check("active: the 22 item-get messages are in the overlay", msgs);
}

// Gives every dungeon item outside its dungeon and checks each one's effect alone.
void checkOutside() {
    const int cur = curStage();
    checkf(cur < 2 || cur > 7, "outside: current stage %d is not a dungeon", cur);
    bool allOk = true;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* e = pc_rando_dungeon_item(i);
        const State before = capture();
        execItemGet(e->id);
        const State after = capture();
        dSv_memBit_c expect = before.save[e->stage];
        if (e->kind == PCR_DITEM_SMALL_KEY) {
            expect.setKeyNum(expect.getKeyNum() + 1);
        } else {
            expect.onDungeonItem(e->kind);
        }
        bool ok = sameBit(after.save[e->stage], expect) && sameBit(after.mem, before.mem) &&
                  after.keyCount == before.keyCount;
        for (int s = 0; s < 8; s++) {
            ok = ok && (s == e->stage || sameBit(after.save[s], before.save[s]));
        }
        if (!ok) {
            checkf(false, "outside: item 0x%02X changes only stage %d's saved info", e->id, e->stage);
        }
        allOk = allOk && ok;
    }
    check("outside: each of the 22 items changes only its dungeon's saved info", allOk);
    check("outside: DRC has a key and its map/compass/Big Key bits", saved(3).getKeyNum() >= 1 &&
                                                                        saved(3).isDungeonItemMap() &&
                                                                        saved(3).isDungeonItemCompass() &&
                                                                        saved(3).isDungeonItemBossKey());
    check("outside: the FW map sets FW dungeon bit 0", saved(4).isDungeonItem(0));
}

void checkInsideDrc() {
    stage_stag_info_class* stag = dComIfGp_getStageStagInfo();
    const bool keyDisp = stag != nullptr && dStage_stagInfo_ChkKeyDisp(stag) != 0;
    checkf(true, "inside DRC: room key display %d", keyDisp);

    State b = capture();
    execItemGet(0x13);
    State a = capture();
    if (keyDisp) {
        check("inside DRC: DRC small key runs item_func_small_key (counter +1)",
              a.keyCount == b.keyCount + 1 && sameBit(a.mem, b.mem) && sameBit(a.save[3], b.save[3]));
    } else {
        check("inside DRC (no key display): DRC small key raises the memory's key count",
              a.keyCount == b.keyCount && a.mem.getKeyNum() == b.mem.getKeyNum() + 1 && sameBit(a.save[3], b.save[3]));
    }

    b = capture();
    execItemGet(0x1B);
    a = capture();
    check("inside DRC: DRC map sets the loaded memory's map bit",
          a.mem.isDungeonItemMap() && sameBit(a.save[3], b.save[3]));

    b = capture();
    execItemGet(0x1D);
    a = capture();
    check("inside DRC: FW small key goes to FW's saved key count",
          a.save[4].getKeyNum() == b.save[4].getKeyNum() + 1 && a.keyCount == b.keyCount &&
              a.mem.getKeyNum() == b.mem.getKeyNum());

    b = capture();
    execItemGet(0x41);
    a = capture();
    check("inside DRC: FW map sets FW dungeon bit 0", a.save[4].isDungeonItem(0) && sameBit(a.mem, b.mem));
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

// A field item in the PLAY scene's layer, `dist` in front of the player.
fpc_ProcID spawnItem(int itemNo, float dist) {
    daPy_lk_c* p = daPy_getPlayerLinkActorClass();
    process_node_class* scene = (process_node_class*)fpcM_Search(isPlayScene, nullptr);
    if (p == nullptr || scene == nullptr) {
        return fpcM_ERROR_PROCESS_ID_e;
    }
    cXyz pos = p->current.pos;
    pos.x += dist * cM_ssin(p->shape_angle.y);
    pos.z += dist * cM_scos(p->shape_angle.y);
    layer_class* savedLayer = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&scene->mLayer);
    fpc_ProcID id = fopAcM_createItem(&pos, itemNo, -1, fopAcM_GetRoomNo(p), daItemType_0_e, nullptr, daItemAct_0_e);
    fpcLy_SetCurrentLayer(savedLayer);
    return id;
}

bool fieldItemAlive(fpc_ProcID id, int itemNo) {
    daItem_c* item = (daItem_c*)fopAcM_SearchByID(id);
    return item != nullptr && fopAcM_GetName(item) == fpcNm_ITEM_e && item->m_itemNo == itemNo &&
           item->mpModel != nullptr;
}

void deleteItem(fpc_ProcID id) {
    if (fopAc_ac_c* a = fopAcM_SearchByID(id)) {
        fpcM_Delete(a);
    }
}

// Step 6 (R6.2): a FW small key picked up on the sea stage. Returns true once done (checks logged).
bool pickupFwKey() {
    ++sPickSince;
    switch (sPick) {
    case kPickOff:
        // The two field items of step 5 are out of the way first.
        deleteItem(sKeyId);
        deleteItem(sMapId);
        setDrivenPad(true, 0, 0, 0);
        sPick = kPickWait;
        sPickSince = 0;
        return false;
    case kPickWait:
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, sPickSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sPickSince = 0;
            return false;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sPickSince < kSettleFrames) {
            return false;
        }
        sPickBefore = capture();
        dComIfGp_event_setGtItm(dItemNo_NONE_e);
        {
            daPy_lk_c* p = daPy_getPlayerLinkActorClass();
            process_node_class* scene = (process_node_class*)fpcM_Search(isPlayScene, nullptr);
            if (p != nullptr && scene != nullptr) {
                cXyz pos = p->current.pos;
                csXyz angle = csXyz::Zero;
                layer_class* savedLayer = fpcLy_CurrentLayer();
                fpcLy_SetCurrentLayer(&scene->mLayer);
                sFwKeyId = fopAcM_createItem(&pos, kFwKey, -1, fopAcM_GetRoomNo(p), daItemType_0_e, &angle, 2);
                fpcLy_SetCurrentLayer(savedLayer);
            }
        }
        if (!check("key-lunacy: a field FW small key is created on the player", sFwKeyId != fpcM_ERROR_PROCESS_ID_e)) {
            sPick = kPickDone;
            return true;
        }
        sPick = kPickStart;
        sPickSince = 0;
        return false;
    case kPickStart:
        setDrivenPad(true, 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            sPick = kPickEvent;
            sPickSince = 0;
            return false;
        }
        if (sPickSince >= kStartFrames) {
            check("key-lunacy: touching the FW small key starts its get demo", false);
            deleteItem(sFwKeyId);
            setDrivenPad(false, 0, 0, 0);
            sPick = kPickDone;
            return true;
        }
        return false;
    case kPickEvent: {
        setDrivenPad(true, sPickSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            if (sPickSince >= kEventFrames) {
                check("key-lunacy: the FW small key's get demo ends", false);
                setDrivenPad(false, 0, 0, 0);
                sPick = kPickDone;
                return true;
            }
            return false;
        }
        setDrivenPad(false, 0, 0, 0);
        State a = capture();
        State& b = sPickBefore;
        checkf(dComIfGp_event_getGtItm() == kFwKey, "key-lunacy: the get demo carried item 0x%02X (want 0x%02X)",
               dComIfGp_event_getGtItm(), kFwKey);
        checkf(a.save[kFwStage].getKeyNum() == b.save[kFwStage].getKeyNum() + 1,
               "key-lunacy: picked up on the sea, FW's saved key count %d -> %d", b.save[kFwStage].getKeyNum(),
               a.save[kFwStage].getKeyNum());
        dSv_memBit_c expect = b.save[kFwStage];
        expect.setKeyNum(expect.getKeyNum() + 1);
        bool others = sameBit(a.save[kFwStage], expect) && sameBit(a.mem, b.mem) && a.keyCount == b.keyCount;
        for (int s = 0; s < 8; s++) {
            others = others && (s == kFwStage || sameBit(a.save[s], b.save[s]));
        }
        check("key-lunacy: only FW's key count changed (memory, key counter, other stages as before)", others);
        sPick = kPickGone;
        sPickSince = 0;
        return false;
    }
    case kPickGone:
        // A field item deletes itself at the end of its demo (execMainGetDemoDirection), a few frames on.
        if (fopAcM_SearchByID(sFwKeyId) != nullptr && sPickSince < kGoneFrames) {
            return false;
        }
        checkf(fopAcM_SearchByID(sFwKeyId) == nullptr, "key-lunacy: the field FW small key is gone %d frames after the demo",
               (int)sPickSince);
        sPick = kPickDone;
        return true;
    case kPickDone:
        return true;
    }
    return true;
}

} // namespace

void randoSmokeDkeys(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames < sEndFrame) {
            return;
        }
        if (sPick == kPickOff) {
            check("field DRC small key exists with a model", fieldItemAlive(sKeyId, 0x13));
            check("field FF map exists with a model", fieldItemAlive(sMapId, 0x5F));
        }
        // R6.2: key-lunacy, a FW small key picked up outside the dungeons.
        if (curStage() != 3 && !pickupFwKey()) {
            return;
        }
        if (!pc_rando_forced()) {
            pc_rando_deactivate();
            check("deactivated: the 22 IDs' table entries are back to boot", tablesAtBoot());
            check("deactivated: the item-get messages are gone", pc_rando_msg_find(101 + 0x13) == nullptr);
            const State b = capture();
            execItemGet(0x13);
            const State a = capture();
            check("deactivated: execItemGet(0x13) is vanilla again",
                  sameBit(a.save[3], b.save[3]) && sameBit(a.mem, b.mem) && a.keyCount == b.keyCount);
        }
        randoSmokeEnd(kName);
    }

    const int forced = pc_rando_forced();
    if (!forced) {
        memcpy(sBootRes, dItem_data::item_resource, sizeof sBootRes);
        memcpy(sBootField, dItem_data::field_item_res, sizeof sBootField);
        memcpy(sBootInfo, dItem_data::item_info, sizeof sBootInfo);
        check("plain: pc_rando() is 0", pc_rando() == 0);
        const State b = capture();
        execItemGet(0x13);
        const State a = capture();
        check("plain: execItemGet(0x13) changes no key or bit",
              sameBit(a.save[3], b.save[3]) && sameBit(a.mem, b.mem) && a.keyCount == b.keyCount);
        pc_rando_set_active(nullptr, nullptr);
    }
    check("active: pc_rando() is 1", pc_rando() == 1);
    checkTables();

    if (curStage() == 3) {
        checkInsideDrc();
    } else {
        checkOutside();
    }

    sKeyId = spawnItem(0x13, 250.0f);
    sMapId = spawnItem(0x5F, -250.0f);
    check("field items created", sKeyId != fpcM_ERROR_PROCESS_ID_e && sMapId != fpcM_ERROR_PROCESS_ID_e);
    sEndFrame = frames + 30;
}

} // namespace pc
