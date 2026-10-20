//
// DngSw (d_a_dungeon_flag_sw): a custom actor of the Wind Waker Randomizer (wwrando, MIT), taken
// from ref/wwrando/asm/actors/d_a_dungeon_flag_sw.cpp (wwrando 9775811; docs/RANDOMIZER_PLAN.md
// step R3.7). It sets a switch once a dungeon flag is set for every stage of a mask, e.g. when all
// required bosses are dead. Changes from that source: the includes, the profile written with this
// port's field names and fpcNm_DUNGEON_FLAG_SW_e for the process name.
//
// The randomizer adds these actors to stage data (required bosses); vanilla stage data never names
// "DngSw", so a game that is not randomized never creates one. Port code only: the GameCube build
// has no such unit.
//
#if TARGET_PC
// pc_rando(R3.7): new actor DngSw 0x1F7 from wwrando asm/actors/d_a_dungeon_flag_sw.cpp; original: none (wwrando-only REL)
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"

// This custom actor sets a switch once a dungeon-specific flag is set for all of the specified dungeons.
// e.g. This can be used to detect when all bosses in the game have been defeated.
class daDungeonFlagSw_c : public fopAc_ac_c {
public:
    BOOL execute();
    s32 create();
    BOOL checkConditionsMet();

    // This parameter is the switch to set once the checked condition is met.
    u32 prmGetSwitch() { return (fopAcM_GetParam(this) & 0x000000FF) >> 0x00; }

    // This parameter is which stage numbers to check. It's a bitmask where each bit is one stage number.
    // For example, to check DRC and ET (0x03 and 0x06) you would set this parameter to 0x0048.
    u32 prmGetStageNoMask() { return (fopAcM_GetParam(this) & 0x00FFFF00) >> 0x08; }

    // This parameter is which dungeon condition to check for each stage number.
    // 0 - Got dungeon map.
    // 1 - Got compass.
    // 2 - Got big key.
    // 3 - Boss is dead.
    // 4 - Heart container is taken.
    // 5 - Watched boss intro.
    u32 prmGetCondToCheck() { return (fopAcM_GetParam(this) & 0x07000000) >> 0x18; }
};

s32 daDungeonFlagSw_c::create() {
    fopAcM_SetupActor(this, daDungeonFlagSw_c);

    if (fopAcM_isSwitch(this, prmGetSwitch())) {
        return cPhs_ERROR_e;
    }

    return cPhs_COMPLEATE_e;
}

BOOL daDungeonFlagSw_c::checkConditionsMet() {
    u32 stageNoMask = prmGetStageNoMask();
    u32 prmToCheck = prmGetCondToCheck();
    for (int stageNo = 0; stageNo < 0x10; stageNo++) {
        if (!(stageNoMask & (1 << stageNo))) {
            continue;
        }
        BOOL condMet = g_dComIfG_gameInfo.save.getSavedata().getSave(stageNo).getBit().isDungeonItem(prmToCheck);
        if (!condMet) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL daDungeonFlagSw_c::execute() {
    if (checkConditionsMet()) {
        fopAcM_onSwitch(this, prmGetSwitch());
        fopAcM_delete(this);
    }

    return TRUE;
}

static BOOL daDungeonFlagSw_Draw(daDungeonFlagSw_c* i_this) {
    return TRUE;
}

static BOOL daDungeonFlagSw_Execute(daDungeonFlagSw_c* i_this) {
    return i_this->execute();
}

static BOOL daDungeonFlagSw_IsDelete(daDungeonFlagSw_c* i_this) {
    return TRUE;
}

static BOOL daDungeonFlagSw_Delete(daDungeonFlagSw_c* i_this) {
    i_this->~daDungeonFlagSw_c();
    return TRUE;
}

static s32 daDungeonFlagSw_Create(fopAc_ac_c* ac) {
    return ((daDungeonFlagSw_c*)ac)->create();
}

static actor_method_class l_daDungeonFlagSw_Method = {
    (process_method_func)daDungeonFlagSw_Create,
    (process_method_func)daDungeonFlagSw_Delete,
    (process_method_func)daDungeonFlagSw_Execute,
    (process_method_func)daDungeonFlagSw_IsDelete,
    (process_method_func)daDungeonFlagSw_Draw,
};

actor_process_profile_definition g_profile_DUNGEON_FLAG_SW = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_DUNGEON_FLAG_SW_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daDungeonFlagSw_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ 0x0136,
    /* Actor SubMtd */ &l_daDungeonFlagSw_Method,
    /* Status       */ 0x00044000,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_6_e,
};
#endif /* TARGET_PC */
