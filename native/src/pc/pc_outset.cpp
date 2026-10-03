// Milestone M12 outset-debug (docs/NATIVE_PORT_PHASE4_6.md, milestone table; boot loop):
// COS_BOOT_STAGE put Link in the requested stage (Outset, sea room 44 point 206): the PLAY scene
// runs, the start room is up and the player actor exists, then the game ran 300 more frames
// without a fault.
//
// pc_stage_created arms the probe when dStage_Create finishes for the COS_BOOT_STAGE stage, with
// its start room. From then on outsetFrame, called by pc_frame_end once per game frame, only reads
// game state:
// - PLAY scene: a PLAY_SCENE process is executing (it finished creating) and its start stage is
//   the requested one;
// - start room up: stageRoomReady (pc_title_stage.cpp): ROOM_SCENE executing, Room<n> mounted
//   with its dStage_roomDt_c, BG collision registered, the room's actors created;
// - player actor: dComIfGp_getPlayer(0) is a PLAYER actor that finished creating.
// When all three hold it logs the player's position; 300 frames later it reports the milestone.
// Until then it logs the first unmet condition every 600 frames. The probe changes no game state.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kOutsetFrames = 300;
constexpr unsigned int kStatusEvery = 600;

bool sArmed = false;
bool sReady = false;
bool sDone = false;
int sRoomNo = -1;
unsigned int sArmFrame = 0;
unsigned int sReadyFrame = 0;
const char* sLastReason = nullptr;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

const char* unmet(const PcBootStage* boot) {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return "PLAY scene not executing";
    }
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || strcmp(stage, boot->stage) != 0) {
        return "start stage is not the requested one";
    }
    if (const char* room = stageRoomReady(sRoomNo, nullptr)) {
        return room;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return "no player actor";
    }
    if (fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        return "player 0 is not a PLAYER actor";
    }
    if (fpcM_IsCreating(fopAcM_GetID(player)) != FALSE) {
        return "player actor still creating";
    }
    return nullptr;
}

} // namespace

void outsetArm(const char* stageName, int roomNo) {
    const PcBootStage* boot = pc_boot_stage();
    if (sArmed || boot == nullptr || stageName == nullptr || strcmp(stageName, boot->stage) != 0 ||
        roomNo < 0 || roomNo >= 64) {
        return;
    }
    sArmed = true;
    sRoomNo = roomNo;
    sArmFrame = pc_frame_count();
}

void outsetFrame(unsigned int frames) {
    if (!sArmed || sDone) {
        return;
    }
    if (sReady) {
        if (frames - sReadyFrame >= kOutsetFrames) {
            sDone = true;
            writef(STDERR_FILENO, "[cos] outset-debug: %u frames since Link was in room %d\n",
                   frames - sReadyFrame, sRoomNo);
            pc_milestone("outset-debug");
        }
        return;
    }

    const char* reason = unmet(pc_boot_stage());
    if (reason != nullptr) {
        if (reason != sLastReason || (frames - sArmFrame) % kStatusEvery == 0) {
            writef(STDERR_FILENO, "[cos] outset-debug: frame %u: waiting: %s\n", frames, reason);
            sLastReason = reason;
        }
        return;
    }

    sReady = true;
    sReadyFrame = frames;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    writef(STDERR_FILENO, "[cos] outset-debug: Link in %s room %d at frame %u: PLAY scene "
                          "executing, room up, player at (%.1f, %.1f, %.1f) room %d\n",
           pc_boot_stage()->stage, sRoomNo, frames, (double)player->current.pos.x,
           (double)player->current.pos.y, (double)player->current.pos.z,
           (int)fopAcM_GetRoomNo(player));
}

} // namespace pc
