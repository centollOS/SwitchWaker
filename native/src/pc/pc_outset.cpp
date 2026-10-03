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
//
// Milestone M13 outset-control (step 6.6) builds on the same probe: once Link is in the room, every
// run of game frames in which the converted main stick of pad 0 (g_mDoCPd_cpadInfo[0], what the
// game itself reads, so COS_INPUT's script) is held past kStickHeld is measured; when a hold
// reaches kHoldFrames, Link's horizontal displacement over it is logged and, if above kMoveUnits,
// Link counts as controllable. outset-control is reported once Link was controllable and
// kControlFrames frames passed since he was in the room (the game ran that long without a fault).
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cmath>

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kOutsetFrames = 300;
constexpr unsigned int kStatusEvery = 600;

// M13: a hold of the main stick past kStickHeld (mMainStickValue, 0..1) for kHoldFrames frames
// must move Link more than kMoveUnits horizontally, and the game must run kControlFrames frames
// after Link is in the room.
constexpr float kStickHeld = 0.5f;
constexpr unsigned int kHoldFrames = 120;
constexpr float kMoveUnits = 300.0f;
constexpr unsigned int kControlFrames = 3600;

bool sArmed = false;
bool sReady = false;
bool sDone = false;
int sRoomNo = -1;
unsigned int sArmFrame = 0;
unsigned int sReadyFrame = 0;
const char* sLastReason = nullptr;
bool sDebugDone = false;
bool sControlled = false;
unsigned int sHeld = 0;      // frames of the current stick hold
cXyz sHoldStart;             // Link's position on its first frame
int sEventRunning = -1;      // dComIfGp_event_runCheck on the last frame (-1: not read yet)

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

bool outsetLinkReady() {
    return sReady;
}

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

namespace {

// M13: measures the current stick hold (see the top of the file).
void controlFrame(unsigned int frames) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        sHeld = 0;
        return;
    }
    const cXyz& pos = player->current.pos;
    if ((float)g_mDoCPd_cpadInfo[0].mMainStickValue <= kStickHeld) {
        sHeld = 0;
        return;
    }
    if (sHeld++ == 0) {
        sHoldStart = pos;
        return;
    }
    if (sHeld != kHoldFrames) {
        return;
    }
    const float dx = pos.x - sHoldStart.x;
    const float dz = pos.z - sHoldStart.z;
    const float moved = std::sqrt(dx * dx + dz * dz);
    writef(STDERR_FILENO, "[cos] outset-control: frame %u: stick held %u frames, Link moved %.1f "
                          "units (%.1f, %.1f, %.1f) -> (%.1f, %.1f, %.1f)%s\n",
           frames, kHoldFrames, (double)moved, (double)sHoldStart.x, (double)sHoldStart.y,
           (double)sHoldStart.z, (double)pos.x, (double)pos.y, (double)pos.z,
           dComIfGp_event_runCheck() ? ", event running" : "");
    if (moved > kMoveUnits) {
        sControlled = true;
    }
}

} // namespace

void outsetFrame(unsigned int frames) {
    if (!sArmed || sDone) {
        return;
    }
    if (sReady) {
        const unsigned int since = frames - sReadyFrame;
        if (!sDebugDone && since >= kOutsetFrames) {
            sDebugDone = true;
            writef(STDERR_FILENO, "[cos] outset-debug: %u frames since Link was in room %d\n",
                   since, sRoomNo);
            pc_milestone("outset-debug");
        }
        const int running = dComIfGp_event_runCheck() ? 1 : 0;
        if (running != sEventRunning) {
            writef(STDERR_FILENO, "[cos] outset-control: frame %u: event %s\n", frames,
                   running ? "running" : "over (Link free)");
            sEventRunning = running;
        }
        controlFrame(frames);
        if (since % kStatusEvery == 0) {
            fopAc_ac_c* player = dComIfGp_getPlayer(0);
            if (player != nullptr) {
                writef(STDERR_FILENO, "[cos] outset-control: frame %u: Link at (%.1f, %.1f, %.1f)%s%s\n",
                       frames, (double)player->current.pos.x, (double)player->current.pos.y,
                       (double)player->current.pos.z,
                       dComIfGp_event_runCheck() ? ", event running" : "",
                       sControlled ? ", controllable" : "");
            }
        }
        if (sControlled && since >= kControlFrames) {
            sDone = true;
            writef(STDERR_FILENO, "[cos] outset-control: Link controllable, %u frames since he was "
                                  "in room %d\n", since, sRoomNo);
            pc_milestone("outset-control");
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
