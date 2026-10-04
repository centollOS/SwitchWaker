// Input injection (docs/NATIVE_PORT_PHASE4_6.md, step 6.3).
//
// COS_INPUT names a script that drives controller port 0. One line per change of state:
//     <frame> <buttons> <stickX> <stickY> [<substickX> <substickY>]
// - frame: the game frame (pc_frame_count() when mDoCPd_Read runs; the first frame is 0) from
//   which the line applies; it holds until the next line, and the last line holds to the end.
//   Frames are strictly increasing.
// - buttons: '-' or 0 for none, a number (0x1100), or names joined with '+': A B X Y Z L R START
//   UP DOWN LEFT RIGHT (PAD_BUTTON_* / PAD_TRIGGER_*). L and R also set the analog trigger to its
//   full value (180), as Aurora does for a digital trigger.
// - stickX, stickY: the raw main stick, -128..127 (positive is right / up), before PADClamp.
// - substickX, substickY (optional, 0 when left out): the C stick, the same way (bug B6: the
//   telescope zooms with the C stick).
// '#' starts a comment; blank lines are ignored. Before the first line the pad is neutral.
//
// With a script, port 0 is Aurora's virtual pad from the first frame on: mDoCPd_Read (TARGET_PC)
// calls pc_pad_feed before JUTGamePad::read, which hands the state to PADSetVirtualStatus, so the
// game reads it through PADRead, PADClamp, JUTGamePad and mDoCPd_Convert like a real controller
// (a real controller on port 0 is merged with it by Aurora).
//
// COS_SMOKE=pad-echo boots the game with a script and, after each mDoCPd_Read
// (pc_pad_read_done), checks g_mDoCPd_cpadInfo[0] against the script: the 12 buttons the game maps
// held exactly as scripted and triggered on the frame they go down; the main stick zero inside the
// dead zone, of the scripted sign on each axis, at full value past the clamp; the C stick at rest;
// the analog L/R full with L/R; no pad error. Each change is logged as a "[cos] pad-echo:" line;
// the test ends 8 frames after the last line with exit 0, or 1 on any difference.
#include "pc_internal.h"

#include "m_Do/m_Do_controller_pad.h"

#include <dolphin/pad.h>

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

struct InputLine {
    unsigned int frame;
    unsigned int line; // in the script file
    uint16_t buttons;
    int8_t stickX;
    int8_t stickY;
    int8_t substickX;
    int8_t substickY;
};

std::vector<InputLine> sScript;
bool sLoaded = false;
size_t sNext = 0;              // first line not applied yet
const InputLine* sCurrent = nullptr; // the line in force (nullptr: neutral)
uint16_t sPrevButtons = 0;     // buttons applied on the previous frame

// pad-echo
bool sEcho = false;
unsigned int sEchoErrors = 0;
unsigned int sEchoFrames = 0;
bool sEchoHaveLast = false;
uint32_t sEchoLastHold = 0;
uint32_t sEchoLastTrig = 0;
float sEchoLastX = 0;
float sEchoLastY = 0;
const unsigned int kEchoTail = 8;
const unsigned int kEchoMaxErrorLines = 40;

// The buttons mDoCPd_Convert maps into interface_of_controller_pad.
const uint16_t kMappedButtons = PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT |
                                PAD_BUTTON_RIGHT | PAD_TRIGGER_Z | PAD_TRIGGER_R | PAD_TRIGGER_L |
                                PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y |
                                PAD_BUTTON_START;

// Aurora's PADClamp region for the main stick (aurora lib/dolphin/pad/pad.cpp, ClampRegion):
// values up to the dead zone read 0; past dead zone + max every direction reads full.
const int kStickDeadZone = 15;
const int kStickMax = 72;
const uint8_t kTriggerFull = 180;

struct ButtonName {
    const char* name;
    uint16_t bit;
};

const ButtonName kButtonNames[] = {
    {"A", PAD_BUTTON_A},       {"B", PAD_BUTTON_B},         {"X", PAD_BUTTON_X},
    {"Y", PAD_BUTTON_Y},       {"Z", PAD_TRIGGER_Z},        {"L", PAD_TRIGGER_L},
    {"R", PAD_TRIGGER_R},      {"START", PAD_BUTTON_START}, {"UP", PAD_BUTTON_UP},
    {"DOWN", PAD_BUTTON_DOWN}, {"LEFT", PAD_BUTTON_LEFT},   {"RIGHT", PAD_BUTTON_RIGHT},
};

[[noreturn]] void scriptError(const char* path, unsigned int line, const char* what, const char* text) {
    writef(STDERR_FILENO, "[cos] COS_INPUT %s:%u: %s \"%s\"\n", path, line, what, text);
    pc_exit(PC_EXIT_USAGE);
}

bool parseButtons(const char* text, uint16_t& out) {
    if (strcmp(text, "-") == 0) {
        out = 0;
        return true;
    }
    if (text[0] >= '0' && text[0] <= '9') {
        char* end = nullptr;
        errno = 0;
        unsigned long v = strtoul(text, &end, 0);
        if (errno != 0 || *end != '\0' || v > 0xFFFF) {
            return false;
        }
        out = (uint16_t)v;
        return true;
    }
    uint16_t bits = 0;
    const char* p = text;
    while (*p != '\0') {
        const char* end = strchr(p, '+');
        size_t len = end != nullptr ? (size_t)(end - p) : strlen(p);
        bool found = false;
        for (const ButtonName& b : kButtonNames) {
            if (strlen(b.name) == len && strncasecmp(b.name, p, len) == 0) {
                bits |= b.bit;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
        if (end == nullptr) {
            break;
        }
        p = end + 1;
        if (*p == '\0') {
            return false;
        }
    }
    out = bits;
    return true;
}

bool parseStick(const char* text, int8_t& out) {
    char* end = nullptr;
    errno = 0;
    long v = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || v < -128 || v > 127) {
        return false;
    }
    out = (int8_t)v;
    return true;
}

void loadScript(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == nullptr) {
        writef(STDERR_FILENO, "[cos] COS_INPUT %s: cannot open (%s)\n", path, strerror(errno));
        pc_exit(PC_EXIT_USAGE);
    }
    char buf[256];
    unsigned int lineNo = 0;
    while (fgets(buf, sizeof(buf), f) != nullptr) {
        lineNo++;
        char* hash = strchr(buf, '#');
        if (hash != nullptr) {
            *hash = '\0';
        }
        char* fields[7];
        int n = 0;
        for (char* tok = strtok(buf, " \t\r\n"); tok != nullptr; tok = strtok(nullptr, " \t\r\n")) {
            if (n == 7) {
                break;
            }
            fields[n++] = tok;
        }
        if (n == 0) {
            continue;
        }
        if (n != 4 && n != 6) {
            scriptError(path, lineNo, "needs <frame> <buttons> <stickX> <stickY> [<substickX> <substickY>]", fields[0]);
        }
        InputLine line = {};
        line.line = lineNo;
        char* end = nullptr;
        errno = 0;
        unsigned long frame = strtoul(fields[0], &end, 10);
        if (errno != 0 || end == fields[0] || *end != '\0' || fields[0][0] == '-' || frame > 0x7FFFFFFFul) {
            scriptError(path, lineNo, "bad frame", fields[0]);
        }
        line.frame = (unsigned int)frame;
        if (!sScript.empty() && line.frame <= sScript.back().frame) {
            scriptError(path, lineNo, "frame not after the previous line's", fields[0]);
        }
        if (!parseButtons(fields[1], line.buttons)) {
            scriptError(path, lineNo, "bad buttons", fields[1]);
        }
        if (!parseStick(fields[2], line.stickX)) {
            scriptError(path, lineNo, "bad stickX", fields[2]);
        }
        if (!parseStick(fields[3], line.stickY)) {
            scriptError(path, lineNo, "bad stickY", fields[3]);
        }
        if (n == 6 && !parseStick(fields[4], line.substickX)) {
            scriptError(path, lineNo, "bad substickX", fields[4]);
        }
        if (n == 6 && !parseStick(fields[5], line.substickY)) {
            scriptError(path, lineNo, "bad substickY", fields[5]);
        }
        sScript.push_back(line);
    }
    fclose(f);
    if (sScript.empty()) {
        writef(STDERR_FILENO, "[cos] COS_INPUT %s: no input lines\n", path);
        pc_exit(PC_EXIT_USAGE);
    }
    writef(STDERR_FILENO, "[cos] input: %s, %zu line(s), frames %u..%u\n", path, sScript.size(),
           sScript.front().frame, sScript.back().frame);
}

uint32_t holdMask(const interface_of_controller_pad& pad) {
    uint32_t m = 0;
    m |= pad.mButtonHold.bits.up ? PAD_BUTTON_UP : 0;
    m |= pad.mButtonHold.bits.down ? PAD_BUTTON_DOWN : 0;
    m |= pad.mButtonHold.bits.left ? PAD_BUTTON_LEFT : 0;
    m |= pad.mButtonHold.bits.right ? PAD_BUTTON_RIGHT : 0;
    m |= pad.mButtonHold.bits.z ? PAD_TRIGGER_Z : 0;
    m |= pad.mButtonHold.bits.r ? PAD_TRIGGER_R : 0;
    m |= pad.mButtonHold.bits.l ? PAD_TRIGGER_L : 0;
    m |= pad.mButtonHold.bits.a ? PAD_BUTTON_A : 0;
    m |= pad.mButtonHold.bits.b ? PAD_BUTTON_B : 0;
    m |= pad.mButtonHold.bits.x ? PAD_BUTTON_X : 0;
    m |= pad.mButtonHold.bits.y ? PAD_BUTTON_Y : 0;
    m |= pad.mButtonHold.bits.start ? PAD_BUTTON_START : 0;
    return m;
}

uint32_t trigMask(const interface_of_controller_pad& pad) {
    uint32_t m = 0;
    m |= pad.mButtonTrig.bits.up ? PAD_BUTTON_UP : 0;
    m |= pad.mButtonTrig.bits.down ? PAD_BUTTON_DOWN : 0;
    m |= pad.mButtonTrig.bits.left ? PAD_BUTTON_LEFT : 0;
    m |= pad.mButtonTrig.bits.right ? PAD_BUTTON_RIGHT : 0;
    m |= pad.mButtonTrig.bits.z ? PAD_TRIGGER_Z : 0;
    m |= pad.mButtonTrig.bits.r ? PAD_TRIGGER_R : 0;
    m |= pad.mButtonTrig.bits.l ? PAD_TRIGGER_L : 0;
    m |= pad.mButtonTrig.bits.a ? PAD_BUTTON_A : 0;
    m |= pad.mButtonTrig.bits.b ? PAD_BUTTON_B : 0;
    m |= pad.mButtonTrig.bits.x ? PAD_BUTTON_X : 0;
    m |= pad.mButtonTrig.bits.y ? PAD_BUTTON_Y : 0;
    m |= pad.mButtonTrig.bits.start ? PAD_BUTTON_START : 0;
    return m;
}

// The scripted raw axis value after the dead zone: its sign, 0 inside the dead zone.
int axisSign(int raw) {
    if (raw > kStickDeadZone) {
        return 1;
    }
    if (raw < -kStickDeadZone) {
        return -1;
    }
    return 0;
}

int floatSign(float v) {
    return v > 0.0f ? 1 : (v < 0.0f ? -1 : 0);
}

void echoError(unsigned int frame, const char* what) {
    sEchoErrors++;
    if (sEchoErrors <= kEchoMaxErrorLines) {
        writef(STDERR_FILENO, "[cos] pad-echo: frame %u: %s\n", frame, what);
    }
}

void echoCheck(unsigned int frame) {
    const interface_of_controller_pad& pad = g_mDoCPd_cpadInfo[0];
    const uint16_t buttons = sCurrent != nullptr ? sCurrent->buttons : 0;
    const int sx = sCurrent != nullptr ? sCurrent->stickX : 0;
    const int sy = sCurrent != nullptr ? sCurrent->stickY : 0;
    const uint32_t hold = holdMask(pad);
    const uint32_t trig = trigMask(pad);
    const uint32_t wantHold = buttons & kMappedButtons;
    const uint32_t wantTrig = buttons & ~sPrevButtons & kMappedButtons;
    char msg[160];

    sEchoFrames++;
    if (hold != wantHold) {
        snprintf(msg, sizeof(msg), "hold 0x%04x, scripted 0x%04x", hold, wantHold);
        echoError(frame, msg);
    }
    if (trig != wantTrig) {
        snprintf(msg, sizeof(msg), "trigger 0x%04x, expected 0x%04x", trig, wantTrig);
        echoError(frame, msg);
    }
    const float px = pad.mMainStickPosX;
    const float py = pad.mMainStickPosY;
    const float value = pad.mMainStickValue;
    if (floatSign(px) != axisSign(sx) || floatSign(py) != axisSign(sy)) {
        snprintf(msg, sizeof(msg), "main stick %.3f,%.3f, scripted %d,%d", px, py, sx, sy);
        echoError(frame, msg);
    }
    const bool full = std::abs(sx) >= kStickDeadZone + kStickMax || std::abs(sy) >= kStickDeadZone + kStickMax;
    const bool rest = axisSign(sx) == 0 && axisSign(sy) == 0;
    if ((full && value != 1.0f) || (rest && value != 0.0f) || value < 0.0f || value > 1.0f ||
        std::isnan(value)) {
        snprintf(msg, sizeof(msg), "main stick value %.3f, scripted %d,%d", value, sx, sy);
        echoError(frame, msg);
    }
    if (pad.mCStickPosX != 0.0f || pad.mCStickPosY != 0.0f || pad.mCStickValue != 0.0f) {
        snprintf(msg, sizeof(msg), "C stick %.3f,%.3f, not scripted", pad.mCStickPosX, pad.mCStickPosY);
        echoError(frame, msg);
    }
    const float wantL = (buttons & PAD_TRIGGER_L) ? 1.0f : 0.0f;
    const float wantR = (buttons & PAD_TRIGGER_R) ? 1.0f : 0.0f;
    if (pad.mTriggerLeft != wantL || pad.mTriggerRight != wantR) {
        snprintf(msg, sizeof(msg), "analog L/R %.3f/%.3f, expected %.0f/%.0f", pad.mTriggerLeft,
                 pad.mTriggerRight, wantL, wantR);
        echoError(frame, msg);
    }
    if (pad.mGamepadErrorFlags != 0) {
        snprintf(msg, sizeof(msg), "pad error %d", (int)pad.mGamepadErrorFlags);
        echoError(frame, msg);
    }

    if (!sEchoHaveLast || hold != sEchoLastHold || trig != sEchoLastTrig || px != sEchoLastX ||
        py != sEchoLastY) {
        const PADStatus& status = JUTGamePad::getPortStatus(JUTGamePad::EPort1);
        writef(STDERR_FILENO,
               "[cos] pad-echo: frame=%u line=%u script=0x%04x,%d,%d clamped=%d,%d hold=0x%04x "
               "trig=0x%04x stick=%.3f,%.3f value=%.3f angle=%d L=%.2f R=%.2f\n",
               frame, sCurrent != nullptr ? sCurrent->line : 0, (unsigned int)buttons, sx, sy,
               (int)status.stickX, (int)status.stickY, hold, trig, px, py, value,
               (int)pad.mMainStickAngle, pad.mTriggerLeft, pad.mTriggerRight);
        sEchoHaveLast = true;
        sEchoLastHold = hold;
        sEchoLastTrig = trig;
        sEchoLastX = px;
        sEchoLastY = py;
    }

    if (frame >= sScript.back().frame + kEchoTail) {
        writef(STDERR_FILENO, "[cos] pad-echo: %zu script line(s), %u frame(s) checked, %u error(s)\n",
               sScript.size(), sEchoFrames, sEchoErrors);
        pc_exit(sEchoErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
    }
}

} // namespace

void loadInput() {
    sEcho = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "pad-echo") == 0;
    if (gConfig.input == nullptr) {
        if (sEcho) {
            writef(STDERR_FILENO, "[cos] smoke pad-echo needs COS_INPUT\n");
            pc_exit(PC_EXIT_USAGE);
        }
        return;
    }
    loadScript(gConfig.input);
    sLoaded = true;
}

} // namespace pc

using namespace pc;

extern "C" {

void pc_pad_feed(void) {
    if (!sLoaded) {
        return;
    }
    const unsigned int frame = pc_frame_count();
    sPrevButtons = sCurrent != nullptr ? sCurrent->buttons : 0;
    while (sNext < sScript.size() && sScript[sNext].frame <= frame) {
        sCurrent = &sScript[sNext++];
    }
    PADStatus status;
    memset(&status, 0, sizeof(status));
    if (sCurrent != nullptr) {
        status.button = sCurrent->buttons;
        status.stickX = sCurrent->stickX;
        status.stickY = sCurrent->stickY;
        status.substickX = sCurrent->substickX;
        status.substickY = sCurrent->substickY;
        status.triggerLeft = (sCurrent->buttons & PAD_TRIGGER_L) ? kTriggerFull : 0;
        status.triggerRight = (sCurrent->buttons & PAD_TRIGGER_R) ? kTriggerFull : 0;
    }
    PADSetVirtualStatus(0, &status);
}

void pc_pad_read_done(void) {
    if (sEcho && sLoaded) {
        echoCheck(pc_frame_count());
    }
}

} // extern "C"
