// Control options (native/include/pc/pc_controls.h): the camera's inverted axes and its shake, and
// the heat haze.
#include "pc_internal.h"

#include "pc/pc_controls.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

int sInvertX = -1; // -1: not read yet
int sInvertY = -1;
int sShake = -1;
int sHeatHaze = -1;

int readFlag(const char* name) {
    const char* v = getenv(name);
    const int on = v != nullptr && strcmp(v, "1") == 0;
    if (on) {
        pc::writef(STDERR_FILENO, "[cos] %s=1: camera axis inverted\n", name);
    }
    return on;
}

} // namespace

extern "C" {

int pc_camera_invert_x(void) {
    if (sInvertX < 0) {
        sInvertX = readFlag("COS_CAMERA_INVERT_X");
    }
    return sInvertX;
}

int pc_camera_invert_y(void) {
    if (sInvertY < 0) {
        sInvertY = readFlag("COS_CAMERA_INVERT_Y");
    }
    return sInvertY;
}

void pc_camera_invert_x_set(int on) {
    sInvertX = on ? 1 : 0;
}

void pc_camera_invert_y_set(int on) {
    sInvertY = on ? 1 : 0;
}

int pc_camera_shake(void) {
    if (sShake < 0) {
        const char* v = getenv("COS_CAMERA_SHAKE");
        sShake = v != nullptr && strcmp(v, "1") == 0;
    }
    return sShake;
}

void pc_camera_shake_set(int on) {
    sShake = on ? 1 : 0;
}

int pc_heat_haze(void) {
    if (sHeatHaze < 0) {
        const char* v = getenv("COS_HEAT_HAZE");
        sHeatHaze = v != nullptr && strcmp(v, "1") == 0;
    }
    return sHeatHaze;
}

void pc_heat_haze_set(int on) {
    sHeatHaze = on ? 1 : 0;
}

int pc_heat_haze_hidden(unsigned short user_id) {
    if (pc_heat_haze()) {
        return 0;
    }
    // d_particle_name.h: the "kagerou" (heat haze) projection particles.
    switch (user_id) {
    case 0x4004: // ID_AK_JP_O_KAGEROU00
    case 0x445B: // ID_IT_JP_KAKOMI_KAGERO00
    case 0xC06B: // ID_AK_SP_O_FIREHOLEKAGEROU
    case 0xC06C: // ID_IT_SP_MAGT_KAGERO
    case 0xC06D: // ID_AK_SP_MTDRAGONKAGEROU (daYkgr: Dragon Roost)
    case 0xC06E: // ID_IT_SP_KAKOMI_KAGERO00
        return 1;
    default:
        return 0;
    }
}

} // extern "C"
