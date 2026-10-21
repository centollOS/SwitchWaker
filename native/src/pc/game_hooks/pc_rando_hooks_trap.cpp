// pc_rando_hooks_trap.cpp - the game side of trap chests (docs/RANDOMIZER_PLAN.md step R6.5): the
// SPRING_TRAP action of daTbox_c::demoProc, called by its pc_rando(R6.5) hunks
// (game/src/d/actor/d_a_tbox.cpp) only under pc_rando(). A port of wwrando 1.10.0_9775811's
// asm/actors/d_a_tbox.cpp demoProc (DEMO_PROC_SPRING_TRAP init) and demoProcSpringTrap. The chest
// gets there only from DEFAULT_TREASURE_TRAP, which features/rando/pc_rando_trap.cpp adds to the
// stages that hold a trap chest.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "JSystem/JParticle/JPAEmitter.h"
#include "JAZelAudio/JAZelAudio_SE.h"
#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_tbox.h"
#include "d/d_com_inf_game.h"
#include "d/d_particle_name.h"
#include "f_op/f_op_actor_mng.h"

namespace {

// daPy_lk_c::mAcch is private; explicit instantiation (which access checking does not apply to)
// hands out its member pointer.
template <typename Tag, typename Tag::type M> struct Rob {
    friend typename Tag::type get(Tag) { return M; }
};
struct LinkAcch {
    using type = dBgS_LinkAcch daPy_lk_c::*;
    friend type get(LinkAcch);
};
template struct Rob<LinkAcch, &daPy_lk_c::mAcch>;

// asm/actors/d_a_tbox.cpp TRAP_TIME_*
constexpr u16 kTrapFreeze = 0x05;
constexpr u16 kTrapMistStop = 0x30;
constexpr u16 kTrapUnfreeze = 0x60;
constexpr u16 kTrapEnd = 0x64;

} // namespace

extern "C" void pc_rando_tbox_spring_trap_init(void* p) {
    daTbox_c* tbox = static_cast<daTbox_c*>(p);
    tbox->mOpenTimer = 0; // wwrando re-uses this timer for the trap
    cXyz ptcPos = tbox->current.pos + cXyz(0.0f, 5.0f, 0.0f);
    cXyz ptcScale(1.5f, 2.0f, 1.5f);
    csXyz ptcAngle(0x4000, 0, 0);
    tbox->mSmokeEmitter = dComIfGp_particle_set(dPa_name::ID_IT_JN_ARWI_REIKI00, &ptcPos, &ptcAngle, &ptcScale);
}

extern "C" void pc_rando_tbox_spring_trap(void* p) {
    daTbox_c* tbox = static_cast<daTbox_c*>(p);
    if (tbox->mOpenTimer < kTrapEnd) {
        tbox->mOpenTimer++;
    }
    daPy_lk_c* player = (daPy_lk_c*)dComIfGp_getPlayer(0);
    if (player == NULL) {
        if (tbox->mOpenTimer == kTrapEnd) {
            dComIfGp_evmng_cutEnd(tbox->mStaffId);
        }
        return;
    }
    switch (tbox->mOpenTimer) {
    case kTrapFreeze:
        player->onNoResetFlg1(daPy_py_c::daPyFlg1_FREEZE_STATE); // mNoResetFlg1 |= 0x800
        player->setDamagePoint(-2.0f);
        fopAcM_seStart(tbox, JA_SE_LK_FREEZE, 0);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_ENEMYFREEZE01, &player->current.pos);
        break;
    case kTrapMistStop:
        if (tbox->mSmokeEmitter != NULL) {
            tbox->mSmokeEmitter->becomeInvalidEmitter();
            tbox->mSmokeEmitter = NULL;
        }
        break;
    case kTrapUnfreeze:
        player->offNoResetFlg1(daPy_py_c::daPyFlg1_FREEZE_STATE);
        fopAcM_seStart(tbox, JA_SE_CM_ICE_BREAK, 0);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_ENEMYFREEZE02, &player->current.pos);
        break;
    case kTrapEnd: {
        // dProcOpenTreasure_init turned these off/on (SetWallNone, OffLineCheck); no item-get proc
        // follows a trap to turn them back.
        dBgS_LinkAcch& acch = player->*get(LinkAcch());
        acch.m_flags &= ~0x04u;  // WALL_NONE
        acch.m_flags |= 0x2000u; // LINE_CHECK
        dComIfGp_evmng_cutEnd(tbox->mStaffId);
        break;
    }
    default:
        break;
    }
}
