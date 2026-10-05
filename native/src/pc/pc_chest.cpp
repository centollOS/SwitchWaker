// COS_SMOKE=chest: a treasure chest opened end to end. The chest at the end of Niko's rope lesson on
// Tetra's ship (stage Asoko, Stage.arc TRE0: takara3, parameters 0xFF200200, angle z 0x24FF: the
// Spoils Bag; layers 2 and 3 have a bomb bag there) froze
// half open on the console with the game still running (an event waiting forever; the stuck-event
// watch, pc_event_watch.cpp, logs which cut it waits on). Boot with COS_BOOT_STAGE=Asoko:0:0:2:
//   1. A through Niko's opening talk until no event runs, then kSettleFrames;
//   2. the same chest is created kSpawn in front of the player, facing them, inside the play scene's
//      layer (the real one is behind the lesson's shutter, out of reach from the start);
//   3. the player walks at it until the A button says "Open" (dActStts_OPEN_e) and presses A; its
//      event (DEFAULT_TREASURE) must start within kStartFrames and end within kEventSeconds of wall
//      time (the item's message waits for the fanfare, which plays in real time: uncapped runs go
//      through many more frames), A every kTalkFrames for the item's message. Shots every kShotEvery frames while it runs.
// Exit 0 when the chest's event ends, else 1.
#include "pc_internal.h"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_node.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;
constexpr unsigned int kStartFrames = 240;
constexpr double kEventSeconds = 90.0; // wall time: the item fanfare plays in real time
constexpr unsigned int kTalkFrames = 45;
constexpr unsigned int kShotEvery = 30;
constexpr float kSpawn = 200.0f;
cXyz sChestPos;

enum State { kOff, kWait, kSettle, kPlace, kOpen, kEvent, kDone };
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
uint64_t sEventStartNs = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

void finish(unsigned int frames, bool ok) {
    setDrivenPad(true, 0, 0, 0);
    sState = kDone;
    captureFrame(frames, shotSink, nullptr);
    writef(STDERR_FILENO, "[cos] chest: %s\n", ok ? "PASS" : "FAIL");
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void logEvent(unsigned int frames) {
    dEvt_control_c* ev = g_dComIfG_gameInfo.play.getEvent();
    const char* name = "(none)";
    if (ev->mEventId != -1) {
        dEvDtEvent_c* data = dComIfGp_getPEvtManager()->getEventData(ev->mEventId);
        if (data != nullptr) {
            name = data->getName();
        }
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    writef(STDERR_FILENO, "[cos] chest: frame %u: event %d \"%s\" mode %u; player at (%.0f, %.0f, %.0f)\n", frames,
           ev->mEventId, name, ev->getMode(), player->current.pos.x, player->current.pos.y, player->current.pos.z);
}

} // namespace

void chestFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke != nullptr && strcmp(gConfig.smoke, "chest") == 0) {
            setDrivenPad(true, 0, 0, 0);
            sState = kWait;
        }
    }
    if (sState == kOff || sState == kDone) {
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return;
    }
    ++sSince;
    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWait:
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= kSettleFrames) {
            sState = kPlace;
            sSince = 0;
        }
        return;
    case kPlace: {
        if (sSince == 1) {
            // Bring the ship's chest to the player: the same chest (takara3, parameters 0xFF200200,
            // angle z 0x24FF: the Spoils Bag), kSpawn in front of the player, facing them, in their room.
            const s16 yaw = player->shape_angle.y;
            cXyz pos(player->current.pos.x + kSpawn * cM_ssin(yaw), player->current.pos.y,
                     player->current.pos.z + kSpawn * cM_scos(yaw));
            csXyz angle((s16)(fopAcM_GetRoomNo(player) & 0x3F), (s16)(yaw + 0x8000), (s16)0x24FF); // x: its room (daTbox mRoomNo); z: the item (Spoils Bag)
            sChestPos = pos;
            // Inside the play scene's layer, as the game creates actors (from the root layer an actor
            // is drawn twice a frame; see pc_ky_procs.cpp).
            layer_class* saved = fpcLy_CurrentLayer();
            fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
            const fpc_ProcID id =
                fopAcM_create(fpcNm_TBOX_e, 0xFF200200, &pos, fopAcM_GetRoomNo(player), &angle);
            fpcLy_SetCurrentLayer(saved);
            writef(STDERR_FILENO, "[cos] chest: frame %u: chest created in front of the player, process %u\n",
                   frames, (unsigned)id);
        }
        if (sSince >= 30) {
            writef(STDERR_FILENO, "[cos] chest: frame %u: the player at (%.0f, %.0f, %.0f); to the chest and A\n",
                   frames, player->current.pos.x, player->current.pos.y, player->current.pos.z);
            captureFrame(frames, shotSink, nullptr);
            sState = kOpen;
            sSince = 0;
        }
        return;
    }
    case kOpen: {
        // Walk at the chest until the A button says "Open" (dActStts_OPEN_e), then A.
        const bool canOpen = dComIfGp_getDoStatus() == dActStts_OPEN_e;
        if (!canOpen) {
            const s16 a = cM_atan2s(sChestPos.x - player->current.pos.x, sChestPos.z - player->current.pos.z);
            const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
            setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 70.0f), (int8_t)(-cM_scos(st) * 70.0f));
        } else {
            setDrivenPad(true, sSince % 15 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        }
        if (sSince % 30 == 0) {
            writef(STDERR_FILENO, "[cos] chest: frame %u: player at (%.0f, %.0f, %.0f), do status %d\n", frames,
                   player->current.pos.x, player->current.pos.y, player->current.pos.z, (int)dComIfGp_getDoStatus());
        }
        if (dComIfGp_event_runCheck()) {
            writef(STDERR_FILENO, "[cos] chest: frame %u: an event started after %u frames\n", frames, sSince);
            logEvent(frames);
            sState = kEvent;
            sSince = 0;
            sEventStartNs = monotonicNs();
        } else if (sSince >= kStartFrames) {
            logEvent(frames);
            writef(STDERR_FILENO, "[cos] chest: FAIL A did not open the chest\n");
            finish(frames, false);
        }
        return;
    }
    case kEvent:
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (sSince % kShotEvery == 0) {
            logEvent(frames);
            captureFrame(frames, shotSink, nullptr);
        }
        if (!dComIfGp_event_runCheck()) {
            writef(STDERR_FILENO, "[cos] chest: frame %u: the chest's event ended after %u frames\n", frames, sSince);
            finish(frames, true);
        } else if ((monotonicNs() - sEventStartNs) / 1e9 >= kEventSeconds) {
            logEvent(frames);
            writef(STDERR_FILENO, "[cos] chest: FAIL the chest's event did not end in %.0f s\n", kEventSeconds);
            finish(frames, false);
        }
        return;
    }
}

} // namespace pc
