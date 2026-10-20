// COS_SMOKE=rando COS_RANDO_SMOKE=bugfixes (docs/RANDOMIZER_PLAN.md step R3.12): the vanilla bug
// fixes wwrando always applies (fix_vanilla_bugs.asm: on under rando or with QoL item 15,
// COS_QOL_FIX_BUGS=1) and the ReDead's new switch (flexible_enemies.asm: rando only), driven in a
// booted game. Run on Outset with the Skull Hammer on X:
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=bugfixes --env COS_BOOT_ITEMS=33
//     [--env COS_RANDO_FORCE=1 | --env COS_QOL_FIX_BUGS=1]
// "fix" below is pc_rando() || pc_qol(PC_QOL_FIX_BUGS); each check expects the fixed behaviour with
// it and the original one without, so the same smoke passes plain, forced and with the QoL option.
// Once the play scene has run 30 frames:
//   1. Skull Hammer guard (check_can_defend): no shield equipped, X takes the hammer out (the pad is
//      driven), then R is held for kGuardFrames: with fix the player crouches behind the hammer
//      (daPyProc_CROUCH_DEFENSE_e), without it he only crouches (daPyProc_CROUCH_e, never the
//      defence).
//   2. Sam's pedestals (isUo1FdaiAll): no Joy Pedestal on Outset, so both counters are 0: "all
//      decorated" is false with fix, true without.
//   3. ReDead switch (params 0x00FF0000, kSwitch): a ReDead spawned with the switch off is there
//      kSpawnWait frames later; with the switch on, a second one does not spawn under rando (it
//      spawns without rando).
//   4. Arrow callback (zero_out_arrow_on_hit_callback): an arrow is created at the player, fired
//      (parameter 1) and, in flight, moved kArrowLead in front of the first ReDead's chest, flying
//      at it: it must stop in the ReDead's joints (procStop_Actor) within kArrowFrames, and its
//      on-hit callback is then NULL with fix and still set without.
//   5. the game runs kEndFrames more frames.
// Exit 0 when every check held, else 1. The smoke changes only the save in memory (the shield
// button, one switch) and creates its own actors in the PLAY scene's layer (as combat-sweep does).
#include "pc_rando_smokes.h"

#include "pc/pc_qol.h"
#include "pc/pc_rando.h"

#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_npc_people.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <dolphin/pad.h>

#include <cstdlib>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "bugfixes";
constexpr unsigned int kHammerFrames = 90; // to take the hammer out
constexpr unsigned int kGuardFrames = 40;  // R held
constexpr unsigned int kSpawnWait = 45;
constexpr unsigned int kArrowFrames = 60;
constexpr unsigned int kEndFrames = 30;
constexpr int kSwitch = 0x7D;        // a stage switch Outset's sea room does not use
constexpr float kRdDistance = 600.0f; // the ReDeads, in front of and behind the player
constexpr float kArrowLead = 300.0f;

enum State { kStart, kHammer, kGuard, kRdFirst, kRdSecond, kArrowWait, kArrowFly, kEnd };
State sState = kStart;
unsigned int sSince = 0; // frame the state began
bool sFix = false;
int sSawDefense = 0, sSawCrouch = 0;
fpc_ProcID sRdA = fpcM_ERROR_PROCESS_ID_e, sRdB = fpcM_ERROR_PROCESS_ID_e;
fpc_ProcID sArrow = fpcM_ERROR_PROCESS_ID_e;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

daPy_lk_c* player() {
    return daPy_getPlayerLinkActorClass();
}

// An actor of ours, created in the PLAY scene's layer (from the root layer a process is drawn
// twice a frame; pc_combat_sweep.cpp).
fpc_ProcID spawn(s16 proc, u32 params, cXyz pos, s16 angleY) {
    process_node_class* scene = (process_node_class*)fpcM_Search(isPlayScene, nullptr);
    if (scene == nullptr) {
        return fpcM_ERROR_PROCESS_ID_e;
    }
    csXyz angle(0, angleY, 0);
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&scene->mLayer);
    fpc_ProcID id = fopAcM_create(proc, params, &pos, fopAcM_GetRoomNo(player()), &angle);
    fpcLy_SetCurrentLayer(saved);
    return id;
}

cXyz inFront(float dist) {
    daPy_lk_c* p = player();
    cXyz pos = p->current.pos;
    pos.x += dist * cM_ssin(p->shape_angle.y);
    pos.z += dist * cM_scos(p->shape_angle.y);
    return pos;
}

// The ReDead's parameters: idle standing, area radius 0x7F (none), no wait-for-switch mode
// (0xFF), the new disable-spawn switch, no other switch.
u32 rdParams(int sw) {
    return 0xFF000000u | ((u32)sw << 16) | 0xFF00u | 0xFEu;
}

void next(State s, unsigned int frames) {
    sState = s;
    sSince = frames;
}

} // namespace

void randoSmokeBugfixes(unsigned int frames) {
    if (frames == 0) {
        setenv("COS_BOOT_ITEMS", "33", 0); // the Skull Hammer on X, if the run did not say
    }
    if (!randoSmokeInPlay(30)) {
        return;
    }
    daPy_lk_c* p = player();
    const unsigned int t = frames - sSince;
    switch (sState) {
    case kStart: {
        sFix = pc_rando() || pc_qol(PC_QOL_FIX_BUGS);
        writef(STDERR_FILENO, "[cos] rando-smoke %s: pc_rando %d, QoL fix bugs %d: fixes %s\n", kName, pc_rando(),
               pc_qol(PC_QOL_FIX_BUGS), sFix ? "on" : "off");
        g_dComIfG_gameInfo.save.getPlayer().getPlayerStatusA().setSelectEquip(1, dItemNo_NONE_e);
        const u8 slot = dComIfGs_getSelectItem(dItemBtn_X_e);
        randoSmokeCheck(kName, "setup: the Skull Hammer on X (COS_BOOT_ITEMS=33)",
                        slot != dInvSlot_NONE_e && dComIfGs_getItem(slot) == dItemNo_SKULL_HAMMER_e);
        next(kHammer, frames);
        break;
    }
    case kHammer:
        setDrivenPad(true, t < 3 ? PAD_BUTTON_X : 0, 0, 0);
        if (p->debugEquipItem() == dItemNo_SKULL_HAMMER_e && t > 20) {
            randoSmokeCheck(kName, "setup: the hammer in hand", true);
            sSawDefense = sSawCrouch = 0;
            next(kGuard, frames);
        } else if (t > kHammerFrames) {
            randoSmokeCheck(kName, "setup: the hammer in hand", false);
            next(kGuard, frames);
        }
        break;
    case kGuard:
        setDrivenPad(true, PAD_TRIGGER_R, 0, 0);
        if (p->debugCurProc() == daPy_lk_c::daPyProc_CROUCH_DEFENSE_e) {
            sSawDefense++;
        } else if (p->debugCurProc() == daPy_lk_c::daPyProc_CROUCH_e) {
            sSawCrouch++;
        }
        if (t >= kGuardFrames) {
            setDrivenPad(false, 0, 0, 0);
            writef(STDERR_FILENO, "[cos] rando-smoke %s: R held %u frames: %d defending, %d crouching\n", kName,
                   kGuardFrames, sSawDefense, sSawCrouch);
            randoSmokeCheck(kName,
                            sFix ? "Skull Hammer guard: defends with the hammer, no shield"
                                 : "Skull Hammer guard: only crouches without a shield (original)",
                            sFix ? sSawDefense > 0 : (sSawDefense == 0 && sSawCrouch > 0));

            // isUo1FdaiAll reads only daDai_c's two static counters (0 here: no pedestal on
            // Outset), so any instance will do; it is not constructed.
            alignas(daNpcPeople_c) static unsigned char sNpc[sizeof(daNpcPeople_c)];
            daNpcPeople_c* npc = reinterpret_cast<daNpcPeople_c*>(sNpc);
            randoSmokeCheck(kName,
                            sFix ? "Sam: no pedestal spawned is not 'all decorated'"
                                 : "Sam: no pedestal spawned counts as 'all decorated' (original)",
                            npc->isUo1FdaiAll() == (sFix ? FALSE : TRUE));

            dComIfGs_offSwitch(kSwitch, fopAcM_GetRoomNo(p));
            sRdA = spawn(fpcNm_RD_e, rdParams(kSwitch), inFront(kRdDistance), p->shape_angle.y + 0x8000);
            randoSmokeCheck(kName, "ReDead 1 requested", sRdA != fpcM_ERROR_PROCESS_ID_e);
            next(kRdFirst, frames);
        }
        break;
    case kRdFirst:
        if (t >= kSpawnWait) {
            randoSmokeCheck(kName, "ReDead switch off: it spawns", fopAcM_SearchByID(sRdA) != nullptr);
            dComIfGs_onSwitch(kSwitch, fopAcM_GetRoomNo(p));
            sRdB = spawn(fpcNm_RD_e, rdParams(kSwitch), inFront(-kRdDistance), p->shape_angle.y);
            next(kRdSecond, frames);
        }
        break;
    case kRdSecond:
        if (t >= kSpawnWait) {
            const bool there = sRdB != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(sRdB) != nullptr;
            randoSmokeCheck(kName,
                            pc_rando() ? "ReDead switch on: it does not spawn"
                                       : "ReDead switch on: it spawns (the switch is rando only)",
                            pc_rando() ? !there : there);
            if (there) {
                fopAcM_delete(fopAcM_SearchByID(sRdB));
            }
            sArrow = spawn(fpcNm_ARROW_e, 0, p->current.pos, p->shape_angle.y);
            randoSmokeCheck(kName, "arrow requested", sArrow != fpcM_ERROR_PROCESS_ID_e);
            next(kArrowWait, frames);
        }
        break;
    case kArrowWait: {
        daArrow_c* arrow = (daArrow_c*)fopAcM_SearchByID(sArrow);
        fopAc_ac_c* rd = fopAcM_SearchByID(sRdA);
        if (arrow != nullptr && rd != nullptr) {
            if (arrow->mCurrProcFunc == &daArrow_c::procWait) {
                fopAcM_SetParam(arrow, 1); // fired: procWait shoots it next frame
            } else if (arrow->mCurrProcFunc == &daArrow_c::procMove) {
                cXyz target = rd->current.pos;
                target.y += 100.0f;
                cXyz dir = target - p->current.pos;
                dir.y = 0.0f;
                dir.normalizeZP();
                arrow->current.pos = target - dir * kArrowLead;
                arrow->old.pos = arrow->current.pos;
                arrow->speed = dir * 100.0f;
                arrow->shape_angle.x = 0;
                arrow->shape_angle.y = cM_atan2s(-dir.x, -dir.z);
                writef(STDERR_FILENO, "[cos] rando-smoke %s: arrow flying at ReDead 1 (callback %s)\n", kName,
                       arrow->mAtCps.GetAtHitCallback() != NULL ? "set" : "NULL");
                next(kArrowFly, frames);
                break;
            }
        }
        if (t > kArrowFrames) {
            randoSmokeCheck(kName, "arrow fired with ReDead 1 there", false);
            next(kEnd, frames);
        }
        break;
    }
    case kArrowFly: {
        daArrow_c* arrow = (daArrow_c*)fopAcM_SearchByID(sArrow);
        if (arrow != nullptr && arrow->mCurrProcFunc == &daArrow_c::procStop_Actor) {
            randoSmokeCheck(kName, "arrow stopped in ReDead 1's joints", true);
            randoSmokeCheck(kName,
                            sFix ? "arrow callback: cleared when it stops in an actor"
                                 : "arrow callback: still set when it stops in an actor (original)",
                            sFix ? arrow->mAtCps.GetAtHitCallback() == NULL
                                 : arrow->mAtCps.GetAtHitCallback() != NULL);
            next(kEnd, frames);
        } else if (arrow == nullptr || t > kArrowFrames) {
            writef(STDERR_FILENO, "[cos] rando-smoke %s: arrow %s\n", kName,
                   arrow == nullptr ? "gone" : "not stopped in an actor");
            randoSmokeCheck(kName, "arrow stopped in ReDead 1's joints", false);
            next(kEnd, frames);
        }
        break;
    }
    case kEnd:
        if (t >= kEndFrames) {
            randoSmokeEnd(kName);
        }
        break;
    }
}

} // namespace pc
