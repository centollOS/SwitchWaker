// COS_SMOKE=camera-invert: the options' inverted camera axes (pc_controls.h). In play (e.g.
// COS_BOOT_STAGE=sea:11:9, Windfall's town square), for each axis: the C stick held kHoldFrames
// one way with the axis normal, then (after kRestFrames) the same with the axis inverted; the
// camera's yaw (dCam_getControledAngleY) for the horizontal axis, its pitch (dCam_getAngleX) for the
// vertical one, must move at least kMinTurn and the opposite way the second time. Then, with both
// axes inverted, the C stick held left must still read as left for the wind baton's left hand
// (mDoAud_getTactDirection(0, 0) == 4): the inversion is the camera's only. Exit 0 when all held,
// else 1.
#include "pc_internal.h"

#include "pc/pc_controls.h"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_audio.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 90;
constexpr unsigned int kHoldFrames = 30;
constexpr unsigned int kRestFrames = 30;
constexpr int kMinTurn = 0x200;

bool sChecked = false;
bool sOn = false;
unsigned int sSince = 0;
int sStep = 0; // 0 settle, then per axis: hold normal, rest, hold inverted, rest
s16 sStart = 0;
int sDelta[2][2] = {}; // [axis][normal/inverted]
unsigned int sErrors = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

s16 cameraAngle(int axis) {
    camera_class* cam = (camera_class*)dComIfGp_getCamera(0);
    return axis == 0 ? dCam_getControledAngleY(cam) : dCam_getAngleX(cam);
}

void setInvert(int axis, int on) {
    if (axis == 0) {
        pc_camera_invert_x_set(on);
    } else {
        pc_camera_invert_y_set(on);
    }
}

} // namespace

void cameraInvertFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        sOn = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "camera-invert") == 0;
        if (sOn) {
            setDrivenPad(true, 0, 0, 0);
        }
    }
    if (!sOn || dComIfGp_getPlayer(0) == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return;
    }
    ++sSince;
    if (sStep == 0) {
        if (sSince >= kSettleFrames) {
            sStep = 1;
            sSince = 0;
        }
        return;
    }
    // Steps 1..8: axis = (step - 1) / 4; phase = (step - 1) % 4: 0 hold normal, 1 rest,
    // 2 hold inverted, 3 rest.
    const int axis = (sStep - 1) / 4;
    const int phase = (sStep - 1) % 4;
    if (axis >= 2 && sSince < kHoldFrames) {
        // The baton's left hand with both axes inverted: C stick left must stay left (4).
        pc_camera_invert_x_set(1);
        pc_camera_invert_y_set(1);
        setDrivenPad(true, 0, 0, 0, -100, 0);
        return;
    }
    if (axis >= 2) {
        const int dir = mDoAud_getTactDirection(0, 0);
        writef(STDERR_FILENO, "[cos] camera-invert: frame %u: both axes inverted, C stick left: baton direction %d "
                              "(4 = left)\n", frames, dir);
        if (dir != 4) {
            sErrors++;
            writef(STDERR_FILENO, "[cos] camera-invert: FAIL the inversion reached the baton\n");
        }
        setDrivenPad(true, 0, 0, 0);
        pc_camera_invert_x_set(0);
        pc_camera_invert_y_set(0);
        writef(STDERR_FILENO, "[cos] camera-invert: %s\n", sErrors == 0 ? "PASS" : "FAIL");
        sOn = false;
        pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
        return;
    }
    const int inverted = phase == 2;
    if (phase == 0 || phase == 2) {
        if (sSince == 1) {
            setInvert(axis, inverted);
            sStart = cameraAngle(axis);
        }
        setDrivenPad(true, 0, 0, 0, axis == 0 ? 100 : 0, axis == 1 ? 100 : 0);
        if (sSince >= kHoldFrames) {
            const int d = (s16)(cameraAngle(axis) - sStart);
            sDelta[axis][inverted] = d;
            writef(STDERR_FILENO, "[cos] camera-invert: frame %u: %s axis %s, C stick held: camera %s %+d\n", frames,
                   axis == 0 ? "horizontal" : "vertical", inverted ? "inverted" : "normal",
                   axis == 0 ? "yaw" : "pitch", d);
            setDrivenPad(true, 0, 0, 0);
            if (inverted) {
                const int a = sDelta[axis][0], b = sDelta[axis][1];
                if (std::abs(a) < kMinTurn || std::abs(b) < kMinTurn || (a > 0) == (b > 0)) {
                    sErrors++;
                    writef(STDERR_FILENO, "[cos] camera-invert: FAIL the %s axis: %+d normal, %+d inverted\n",
                           axis == 0 ? "horizontal" : "vertical", a, b);
                }
                setInvert(axis, 0);
            }
            sStep++;
            sSince = 0;
        }
    } else if (sSince >= kRestFrames) {
        sStep++;
        sSince = 0;
    }
}

} // namespace pc
