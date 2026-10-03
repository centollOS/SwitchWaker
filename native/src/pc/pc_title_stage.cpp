// Milestone M8 title-stage (docs/NATIVE_PORT_PHASE4_6.md, milestone table; boot loop): the title
// opening's sea_T room has loaded, its collision is registered and its actors were created, then
// the game ran 300 more frames without a fault.
//
// pc_stage_created arms the probe when dStage_Create finishes for sea_T (M7) with the start room.
// From then on titleStageFrame, called by pc_frame_end once per game frame, only reads game state:
// - room loaded: the room's ROOM_SCENE process (dStage_roomControl_c's status proc ID) exists and
//   is executing (it finished creating), its "Room<n>" archive is mounted with room.dzr, and dScnRoom's phase_2 gave
//   the room status its dStage_roomDt_c;
// - collision registered: the room's BG actor (fpcNm_BG_e, parameter = room) finished creating,
//   set the room's status flag 0x10 (daBg_c::create, after dBgS::Regist) and its dBgW sits in a
//   used element of dComIfG_Bgsp()'s table;
// - actors created: at least one other actor of that room finished creating, and none of the
//   room's actors is still creating.
// When all three hold, it logs the room and the actor counts; 300 frames later it reports the
// milestone. The probe changes no game state.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_pc/f_pc_name.h"
#include "d/d_stage.h"
#include "d/actor/d_a_bg.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"

#include <cstdio>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kTitleStageFrames = 300;

bool sArmed = false;
bool sReady = false;
bool sDone = false;
int sRoomNo = -1;
unsigned int sReadyFrame = 0;

struct RoomActors {
    int roomNo;
    daBg_c* bg;      // the room's BG actor, once it finished creating
    bool bgCreating; // the room's BG actor exists but is still creating
    int created;     // other actors of the room that finished creating
    int creating;    // other actors of the room still creating
};

int countRoomActor(void* proc, void* data) {
    fopAc_ac_c* actor = (fopAc_ac_c*)proc;
    RoomActors* room = (RoomActors*)data;
    const bool creating = fpcM_IsCreating(fopAcM_GetID(actor)) != FALSE;
    if (fopAcM_GetName(actor) == fpcNm_BG_e) {
        if ((int)fopAcM_GetParam(actor) == room->roomNo) {
            if (creating) {
                room->bgCreating = true;
            } else {
                room->bg = (daBg_c*)actor;
            }
        }
        return TRUE;
    }
    if (fopAcM_GetRoomNo(actor) == room->roomNo) {
        (creating ? room->creating : room->created)++;
    }
    return TRUE;
}

bool bgwRegistered(const dBgW* bgw) {
    if (bgw == nullptr) {
        return false;
    }
    const cBgS* bgs = dComIfG_Bgsp();
    for (const cBgS_ChkElm& elm : bgs->m_chk_element) {
        if (elm.ChkUsed() && elm.m_bgw_base_ptr == (const cBgW*)bgw) {
            return true;
        }
    }
    return false;
}

} // namespace

void titleStageArm(int roomNo) {
    if (sArmed || roomNo < 0 || roomNo >= 64) {
        return;
    }
    sArmed = true;
    sRoomNo = roomNo;
}

void titleStageFrame(unsigned int frames) {
    if (!sArmed || sDone) {
        return;
    }
    if (sReady) {
        if (frames - sReadyFrame >= kTitleStageFrames) {
            sDone = true;
            writef(STDERR_FILENO, "[cos] title-stage: %u frames since room %d was ready\n",
                   frames - sReadyFrame, sRoomNo);
            pc_milestone("title-stage");
        }
        return;
    }

    // Room loaded.
    const fpc_ProcID roomProc = dStage_roomControl_c::getStatusProcID(sRoomNo);
    if (fpcM_IsErrorID(roomProc) || !fpcM_IsExecuting(roomProc)) {
        return;
    }
    char arcName[16];
    snprintf(arcName, sizeof(arcName), "Room%d", sRoomNo);
    if (dComIfG_getStageRes(arcName, "room.dzr") == nullptr ||
        dComIfGp_roomControl_getStatusRoomDt(sRoomNo) == nullptr) {
        return;
    }

    // Collision registered, actors created.
    RoomActors room = {sRoomNo, nullptr, false, 0, 0};
    fopAcIt_Executor(countRoomActor, &room);
    if (room.bg == nullptr || room.bgCreating ||
        !dComIfGp_roomControl_checkStatusFlag(sRoomNo, 0x10) || !bgwRegistered(room.bg->bgw)) {
        return;
    }
    if (room.created == 0 || room.creating != 0) {
        return;
    }

    sReady = true;
    sReadyFrame = frames;
    writef(STDERR_FILENO, "[cos] title-stage: room %d ready at frame %u: %s mounted, BG collision "
                          "registered, %d actor(s) created\n",
           sRoomNo, frames, arcName, room.created);
}

} // namespace pc
