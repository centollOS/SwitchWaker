// Control options (native/include/pc/pc_controls.h): the camera's inverted axes.
#include "pc_internal.h"

#include "pc/pc_controls.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

int sInvertX = -1; // -1: not read yet
int sInvertY = -1;

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

} // extern "C"
