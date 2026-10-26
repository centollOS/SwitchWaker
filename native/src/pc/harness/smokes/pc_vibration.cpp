// COS_SMOKE=vibration: dVibration_c with rumble patterns of 0 bits or more than 32 (d_vibration.cpp's
// rollshift and makebits, JUTGamePad::CRumble::stopPatternedRumbleAtThePeriod). In play (the regress
// target boots sea:11:9), with the camera shake on, after kSettleFrames:
//   1. a camera shock (CS_patt[4], 7 frames) with a camera quake (CQ_patt[1]) started on its last
//      frame: Run's shock + quake case rolls a pattern of length 0 (rollshift);
//   2. a camera quake from CQ_patt[0], whose length is 0: Run's quake case rolls it (rollshift);
//   3. a camera shock with CQ_patt[0]'s quake: makebits extends a 0-bit pattern;
//   4. a camera quake from a bit pattern whose length is 40 (StartQuake(u8 const*, ...)): makebits
//      masks with a negative shift count;
//   5. the pad's motor wave with a 0-frame pattern, then stopped at its period: what Run's motor cases
//      do with MS_patt[0], MQ_patt[0] or a motor quake started on a motor shock's last frame (called
//      on the pad directly: without a rumble pad, Run drops the motor patterns first).
// Without the guards, cases 1, 2 and 5 divide by 0 (SIGFPE on x86-64; the console's divw/divwu and
// AArch64's sdiv/udiv do not trap), case 3 never leaves makebits' loop (every host) and case 4 gives
// the host's mask rather than the console's. Each case sets the state Run reads itself (it does not
// depend on how many Runs a frame gets), must go through its guard (pc_vibration_guard, counted
// here, so a missing guard fails on every host) and must leave the game running kRunFrames. Exit 0
// after the five, 1 when a case did not reach its guard, else the crash (13) or the stall watchdog.
#include "pc_internal.h"

#include "pc/pc_controls.h"
#include "pc/pc_harness.h"

#include "d/d_com_inf_game.h"
#include "d/d_vibration.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstring>
#include <unistd.h>

namespace {

unsigned int sGuardHits[PC_VIB_GUARD_COUNT];

} // namespace

extern "C" void pc_vibration_guard(int site) {
    if (site >= 0 && site < PC_VIB_GUARD_COUNT) {
        sGuardHits[site]++;
    }
}

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;
constexpr unsigned int kRunFrames = 30;
constexpr int kCameraFlags = 0x1E; // camera shake bits only (MASK_CAMERA_SHAKE without the motor's 0x1)
constexpr int kShock = 4;          // CS_patt[4]: 7 frames
constexpr int kQuake = 1;          // CQ_patt[1]: 32 frames
constexpr int kZeroPattern = 0;    // CS_patt[0] and CQ_patt[0]: 0 frames
constexpr int kCaseCount = 5;

struct Case {
    const char* name;
    int guard;  // PC_VIB_GUARD_*
};

const Case kCases[kCaseCount] = {
    {"camera quake started on a camera shock's last frame", PC_VIB_GUARD_ROLLSHIFT},
    {"0-frame camera quake", PC_VIB_GUARD_ROLLSHIFT},
    {"camera shock with a 0-frame camera quake", PC_VIB_GUARD_MAKEBITS},
    {"camera quake from a 40-bit pattern", PC_VIB_GUARD_MAKEBITS},
    {"0-frame motor wave", PC_VIB_GUARD_RUMBLE},
};

bool sChecked = false;
bool sOn = false;
int sCase = -1; // -1 settle, 0..kCaseCount-1 the cases above, kCaseCount done
bool sStarted = false;
unsigned int sSince = 0;
unsigned int sHitsBefore = 0;
int sShakeBefore = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void restore() {
    pc_camera_shake_set(sShakeBefore);
    sOn = false;
}

void startCase(dVibration_c& vib, int index) {
    const cXyz coord(0.0f, 1.0f, 0.0f);
    switch (index) {
    case 0:
        // As Run leaves it on the shock's last frame (mCurrentFrame counted up to mLength), then
        // the quake: the next Run's shock + quake case gets mLength - mCurrentFrame = 0 frames.
        vib.StartShock(kShock, kCameraFlags, coord);
        vib.mCamera.mShock.mCurrentFrame = vib.mCamera.mShock.mLength;
        vib.StartQuake(kQuake, kCameraFlags, coord);
        break;
    case 1:
        vib.StartQuake(kZeroPattern, kCameraFlags, coord);
        break;
    case 2:
        vib.StartShock(kShock, kCameraFlags, coord);
        vib.StartQuake(kZeroPattern, kCameraFlags, coord);
        break;
    case 3: {
        // The callers' layout on PC: native ints whose bytes were the big-endian pattern on the
        // console (a 16-bit length, then the bits; d_vibration.cpp's StartQuake).
        static const u32 pattern[2] = {0x0028FFFFu, 0xFFFF0000u};
        vib.StartQuake((const u8*)pattern, 1, kCameraFlags, coord);
        break;
    }
    default: {
        // makedata(data, pattern, 0) as Run builds it, then Run's motor default case.
        static u16 data[4] = {0, 0, 0, 0};
        g_mDoCPd_gamePad[0]->startMotorWave((u8*)data, JUTGamePad::CRumble::LOOP_ONCE, 60);
        g_mDoCPd_gamePad[0]->stopMotorWave();
        g_mDoCPd_gamePad[0]->stopMotor();
        break;
    }
    }
}

void endCase(dVibration_c& vib, unsigned int frames) {
    vib.StopQuake(kCameraFlags);
    writef(STDERR_FILENO, "[cos] vibration: frame %u: case %d (%s) ran %u frames\n", frames, sCase + 1,
           kCases[sCase].name, kRunFrames);
    sCase++;
    sStarted = false;
    sSince = 0;
}

} // namespace

void vibrationFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        sOn = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "vibration") == 0;
    }
    if (!sOn || dComIfGp_getPlayer(0) == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return;
    }
    dVibration_c& vib = dComIfGp_getVibration();
    ++sSince;
    if (sCase < 0) {
        if (sSince >= kSettleFrames) {
            sShakeBefore = pc_camera_shake();
            pc_camera_shake_set(1);
            sCase = 0;
            sSince = 0;
        }
        return;
    }
    if (sCase >= kCaseCount) {
        restore();
        writef(STDERR_FILENO, "[cos] vibration: PASS\n");
        pc_exit(PC_EXIT_REACHED);
        return;
    }
    const Case& c = kCases[sCase];
    if (!sStarted) {
        sHitsBefore = sGuardHits[c.guard];
        startCase(vib, sCase);
        sStarted = true;
        sSince = 0;
        return;
    }
    if (sSince >= kRunFrames) {
        if (sGuardHits[c.guard] == sHitsBefore) {
            writef(STDERR_FILENO, "[cos] vibration: FAIL case %d (%s) never reached its guard (%d)\n", sCase + 1,
                   c.name, c.guard);
            restore();
            pc_exit(PC_EXIT_CHECK_FAILED);
            return;
        }
        endCase(vib, frames);
    }
}

} // namespace pc
