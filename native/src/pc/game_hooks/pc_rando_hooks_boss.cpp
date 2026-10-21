// pc_rando_hooks_boss.cpp - the game side of the boss Heart Container locations (plan step R2.5;
// table in features/rando/pc_rando_boss_items.h). Called by the pc_rando(R2.5) hunk in
// fopAcM_createItemForBoss (game/src/f_op/f_op_actor_mng.cpp), only under pc_rando().
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_boss_items.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/d_stage.h"

extern "C" uint8_t pc_rando_boss_item(int key) {
    int stage = key;
    if (pc_rando_boss_loc(stage) < 0) {
        // The disappear cloud (Gohma, Kalle Demos, Helmaroc King, Jalhalla) and Gohdan/Molgera pass 0:
        // the boss of the loaded stage. The sea stage stands for Forsaken Fortress, as in
        // item_func_utuwa_heart (the Helmaroc King item outside the fortress).
        stage = dStage_stagInfo_GetSaveTbl(dComIfGp_getStageStagInfo());
        if (stage == dSv_save_c::STAGE_SEA) {
            stage = dSv_save_c::STAGE_FF;
        }
    }
    return pc_rando_boss_item_for_stage(stage);
}
