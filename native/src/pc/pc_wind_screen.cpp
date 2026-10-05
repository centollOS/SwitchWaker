// COS_SMOKE=wind-screen: the wind-direction screen of the wind song (d_operate_wind, created by
// d_meter's dMeter_windMove once dComIfGp_setOperateWindOn() is set, as the wind baton does after
// the song). Boot with the sailing preset (COS_BOOT_PRESET=sailing: the baton and the song). In
// play, kSettleFrames after the PLAY scene: OperateWind on; within kOpenFrames the screen process
// (fpcNm_OPERATE_WIND_e) must exist and no JKR allocation may have failed since the switch (its
// 20000-byte ExpHeap holds two J2DScreens); then OperateWind off. Exit 0 when both held, else 1.
// A shot of the screen.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 90;
constexpr unsigned int kOpenFrames = 30;
bool sChecked = false;
bool sOn = false;
unsigned int sSince = 0;
unsigned int sFailuresBefore = 0;
bool sSwitched = false;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void* isWindScreen(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_OPERATE_WIND_e ? proc : nullptr;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

} // namespace

void windScreenFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        sOn = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "wind-screen") == 0;
    }
    if (!sOn || dComIfGp_getPlayer(0) == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return;
    }
    ++sSince;
    if (!sSwitched) {
        if (sSince >= kSettleFrames) {
            sSwitched = true;
            sSince = 0;
            sFailuresBefore = heapAllocFailures();
            dComIfGp_setOperateWindOn();
            writef(STDERR_FILENO, "[cos] wind-screen: frame %u: OperateWind on\n", frames);
        }
        return;
    }
    const bool open = fpcM_Search(isWindScreen, nullptr) != nullptr;
    if (!open && sSince < kOpenFrames) {
        return;
    }
    if (open && sSince < 10) {
        return; // let it build and draw a few frames
    }
    const unsigned int failures = heapAllocFailures() - sFailuresBefore;
    writef(STDERR_FILENO, "[cos] wind-screen: frame %u: screen %s, %u failed JKR allocation(s) since the switch\n",
           frames, open ? "open" : "NOT open", failures);
    if (open) {
        captureFrame(frames, shotSink, nullptr);
    }
    dComIfGp_setOperateWindCancelOff();
    const bool ok = open && failures == 0;
    writef(STDERR_FILENO, "[cos] wind-screen: %s\n", ok ? "PASS" : "FAIL");
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
