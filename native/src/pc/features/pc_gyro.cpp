// Aiming with the controller's gyroscope (native/include/pc/pc_controls.h, issue #25): the options,
// and the turn since the last call that dCamera_c::CalcSubjectAngle adds to the view.
#include "pc_internal.h"

#include "pc/pc_controls.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// Aurora's <dolphin/pad.h> (the game's own comes first on the include path); PAD_SENSOR_ACCEL and
// PAD_SENSOR_GYRO, SDL's units: m/s^2 and rad/s, x right, y out of the face, z towards the player.
extern "C" int PADSetSensorEnabled(unsigned int port, int sensor, int enabled);
extern "C" int PADGetSensorData(unsigned int port, int sensor, float* data, int nValues);

namespace {

constexpr int kSensorAccel = 1;
constexpr int kSensorGyro = 2;
constexpr float kMaxStep = 0.1f;  // seconds; a longer gap (a hitch, a pause) turns no further
constexpr float kRadToDeg = 57.2957795f;

int sGyro = -1;  // -1: not read yet
float sSens = -1.0f;
bool sFakeRead;
bool sFake;
float sFakeYaw, sFakePitch;  // degrees/s
int sFakeFrames = -1;        // frames the fake turn lasts after each reset; -1: for good
int sFakeLeft;
std::chrono::steady_clock::time_point sLast;
bool sHaveLast;
float sUp[3];  // the up of the world in the controller's axes (smoothed accelerometer); 0: unknown

void readFake() {
    if (sFakeRead) {
        return;
    }
    sFakeRead = true;
    const char* v = getenv("COS_GYRO_FAKE");
    if (v != nullptr && sscanf(v, "%f,%f,%d", &sFakeYaw, &sFakePitch, &sFakeFrames) >= 2) {
        sFake = true;
        sFakeLeft = sFakeFrames;
        pc::writef(STDERR_FILENO, "[cos] COS_GYRO_FAKE: %g deg/s left, %g up, %d frames\n", sFakeYaw,
                   sFakePitch, sFakeFrames);
    }
}

// The turn rate around the up of the world: the gyroscope projected on the accelerometer's
// direction, so a controller held flat and a console held upright both turn the view by turning
// left and right. Without a usable accelerometer, the controller's own up (held flat).
float worldYawRate(const float gyro[3], const float accel[3]) {
    const float len = std::sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
    if (len > 4.0f) {
        for (int i = 0; i < 3; ++i) {
            sUp[i] += (accel[i] / len - sUp[i]) * 0.2f;
        }
    }
    const float upLen = std::sqrt(sUp[0] * sUp[0] + sUp[1] * sUp[1] + sUp[2] * sUp[2]);
    if (upLen < 0.5f) {
        return gyro[1];
    }
    return (gyro[0] * sUp[0] + gyro[1] * sUp[1] + gyro[2] * sUp[2]) / upLen;
}

} // namespace

extern "C" {

int pc_aim_gyro(void) {
    if (sGyro < 0) {
        const char* v = getenv("COS_AIM_GYRO");
        sGyro = v != nullptr && strcmp(v, "1") == 0;
    }
    return sGyro;
}

void pc_aim_gyro_set(int on) {
    sGyro = on ? 1 : 0;
}

float pc_aim_gyro_sens(void) {
    if (sSens < 0.0f) {
        const char* v = getenv("COS_AIM_GYRO_SENS");
        sSens = v != nullptr && atof(v) > 0.0 ? (float)atof(v) : 1.0f;
    }
    return sSens;
}

void pc_aim_gyro_sens_set(float sens) {
    sSens = sens > 0.0f ? sens : 1.0f;
}

void pc_aim_gyro_reset(void) {
    sHaveLast = false;
    sFakeLeft = sFakeFrames;
}

int pc_aim_gyro_turn(int pad, float* yaw, float* pitch) {
    *yaw = 0.0f;
    *pitch = 0.0f;
    if (!pc_aim_gyro()) {
        sHaveLast = false;
        return 0;
    }
    const auto now = std::chrono::steady_clock::now();
    float dt = sHaveLast ? std::chrono::duration<float>(now - sLast).count() : 0.0f;
    sLast = now;
    sHaveLast = true;
    if (dt > kMaxStep) {
        dt = kMaxStep;
    }
    readFake();
    float yawRate, pitchRate;  // degrees/s
    if (sFake) {
        // The harness runs faster than real time: one 30 Hz frame per call.
        dt = 1.0f / 30.0f;
        if (sFakeLeft == 0) {
            return 0;
        }
        if (sFakeLeft > 0) {
            --sFakeLeft;
        }
        yawRate = sFakeYaw;
        pitchRate = sFakePitch;
    } else {
        float gyro[3] = {};
        float accel[3] = {};
        const unsigned port = (unsigned)pad & 3;
        PADSetSensorEnabled(port, kSensorGyro, 1);
        PADSetSensorEnabled(port, kSensorAccel, 1);
        if (!PADGetSensorData(port, kSensorGyro, gyro, 3)) {
            return 0;
        }
        PADGetSensorData(port, kSensorAccel, accel, 3);
        yawRate = worldYawRate(gyro, accel) * kRadToDeg;
        pitchRate = gyro[0] * kRadToDeg;
    }
    const float sens = pc_aim_gyro_sens();
    *yaw = yawRate * dt * sens;
    *pitch = pitchRate * dt * sens;
    return *yaw != 0.0f || *pitch != 0.0f;
}

} // extern "C"
