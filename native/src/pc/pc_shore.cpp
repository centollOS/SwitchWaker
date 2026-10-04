// Fixed debug camera (COS_CAMERA) and COS_SMOKE=shore-foam (bug B7, docs/NATIVE_PORT_PLAN.md
// "Known bugs").
//
// COS_CAMERA=<eye x>,<eye y>,<eye z>,<center x>,<center y>,<center z> replaces the eye and the
// centre of the game camera's look-at in camera_draw (d_camera.cpp, TARGET_PC) every frame, after
// the camera ran: the view, the projection and the clipping use the fixed camera; the game itself
// (the camera's logic, the player, the sea that follows the player) is not changed. Malformed: exit 2.
//
// COS_SMOKE=shore-foam (with COS_BOOT_STAGE in Outset, e.g. sea:44:8): B7 (Switch) was the white
// foam lines and the dark sea shadow around Outset's cliffs and rocks flickering from frame to
// frame, solid, dotted or gone. They are translucent decals of the room's model1.bdl (materials
// SC_01_mizu_v..., Txa_nami/Txa_umi_kiwa/Txa_umi_kage textures; z compare LEQUAL, no z update)
// lying about one unit above the island's opaque water (SC_01_mizu, z update on), drawn after it.
// Seen from the bridge, one unit is a sliver of the depth range (near plane 1, reversed Z), and
// Dawn's OpenGL backend lost that precision: Tint rewrote every vertex's z as 2z - w for GL's
// [-1, 1] clip range, which rounds z to about 2^-24 of w. The decals then lost the depth test in
// patches that moved with the camera. On Metal (and Vulkan, D3D) z keeps its float precision.
// Once the player is in the room, the test holds a fixed camera over the cliffs and rocks under the
// rope bridge (kSettleFrames), then moves it sideways kStep units a frame for kPathFrames frames,
// reading each frame back (captureFrame) and counting the near-white pixels in a region of the
// picture that holds the cliff and rock foam (foamMask). It fails when the median frame has less
// foam than kMinFoamPercent of the region (the decals hidden) or when a frame has less than
// kMinFoamRatio of the foam of both its neighbours (a flicker; the camera moves slowly enough
// that the foam changes by a few per cent a frame). The path's first and last frames are saved
// as shot-<frame>.png in the run directory. Exit 0 or 1. Reproduced on the Mac by putting Tint's
// GL arithmetic into Aurora's vertex shader (z' = 2z - w, then z'/w * 0.5 + 0.5, times w): three
// flicker frames (as low as 34 % of their neighbours) where Metal itself stays above 98 %.
//
// COS_ACTOR_LIST=<frame>[,<frame>...] logs every actor at those game frames (process name, its
// dStage name, room, position, angle, parameters and argument), to aim COS_CAMERA at an NPC.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

bool sCameraRead = false;
bool sCameraOn = false;
float sEye[3];
float sCenter[3];

void readCamera() {
    sCameraRead = true;
    const char* v = getenv("COS_CAMERA");
    if (v == nullptr || v[0] == '\0') {
        return;
    }
    float f[6];
    char tail = 0;
    if (sscanf(v, "%f,%f,%f,%f,%f,%f%c", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &tail) != 6) {
        writef(STDERR_FILENO, "[cos] COS_CAMERA=\"%s\" is not <eye x>,<eye y>,<eye z>,<center x>,"
                              "<center y>,<center z>\n", v);
        pc_exit(PC_EXIT_USAGE);
    }
    setFixedCamera(f, f + 3);
    writef(STDERR_FILENO, "[cos] camera: fixed, eye (%g, %g, %g) center (%g, %g, %g)\n",
           (double)f[0], (double)f[1], (double)f[2], (double)f[3], (double)f[4], (double)f[5]);
}

} // namespace

void setFixedCamera(const float eye[3], const float center[3]) {
    sCameraRead = true;
    sCameraOn = true;
    memcpy(sEye, eye, sizeof(sEye));
    memcpy(sCenter, center, sizeof(sCenter));
}

namespace {

// COS_SMOKE=shore-foam: the camera path, the measured region and the limits.
constexpr unsigned int kSettleFrames = 90;  // after the player is in the room: room, sky and HUD up
constexpr unsigned int kPathFrames = 60;    // frames measured while the camera moves
constexpr float kEye0[3] = {-196800.0f, 2400.0f, 318300.0f};
constexpr float kCenter0[3] = {-197600.0f, 0.0f, 321800.0f};
constexpr float kStep[3] = {3.0f, 0.0f, 0.0f}; // units per frame, eye and centre together
constexpr double kMinFoamPercent = 0.5;  // median foam share of the region (Mac: about 1.6 %)
constexpr double kMinFoamRatio = 0.9;    // a frame's foam against its neighbours' (Mac: 0.98 or more)

enum State { kOff, kWaitLink, kSettle, kPath, kDone };
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
std::vector<uint8_t> sPrevMask;
std::vector<unsigned int> sFoam; // foam pixels of each measured frame
unsigned long long sSumXor = 0;
size_t sRegionPixels = 0;

void placeCamera(unsigned int step) {
    float eye[3], center[3];
    for (int i = 0; i < 3; i++) {
        eye[i] = kEye0[i] + kStep[i] * (float)step;
        center[i] = kCenter0[i] + kStep[i] * (float)step;
    }
    setFixedCamera(eye, center);
}

// Foam pixels: near white (every channel at least 200) inside the measured region.
unsigned int foamMask(const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, std::vector<uint8_t>& mask) {
    const uint32_t x0 = w * 45 / 100, x1 = w * 92 / 100, y0 = h * 36 / 100, y1 = h * 85 / 100;
    mask.assign((size_t)(x1 - x0) * (y1 - y0), 0);
    sRegionPixels = mask.size();
    unsigned int n = 0;
    size_t k = 0;
    for (uint32_t y = y0; y < y1; y++) {
        const uint8_t* p = rgb.data() + ((size_t)y * w + x0) * 3;
        for (uint32_t x = x0; x < x1; x++, p += 3, k++) {
            if (p[0] >= 200 && p[1] >= 200 && p[2] >= 200) {
                mask[k] = 1;
                n++;
            }
        }
    }
    return n;
}

// Render worker (captureFrame): counts the frame's foam, compares it with the previous frame and
// saves the path's first and last frames.
void measureFrame(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    std::vector<uint8_t> mask;
    const unsigned int foam = foamMask(rgb, w, h, mask);
    unsigned int changed = 0;
    if (!sPrevMask.empty() && sPrevMask.size() == mask.size()) {
        for (size_t i = 0; i < mask.size(); i++) {
            changed += mask[i] != sPrevMask[i];
        }
        sSumXor += changed;
    }
    sFoam.push_back(foam);
    writef(STDERR_FILENO, "[cos] shore-foam: frame %u step %u: foam %u px, changed %u px\n", frame,
           sSince, foam, changed);
    if (sSince == 0 || sSince + 1 == kPathFrames) {
        saveFramePng(frame, rgb, w, h);
    }
    sPrevMask.swap(mask);
}

} // namespace

namespace {
void* listActor(void* proc, void*) {
    fopAc_ac_c* ac = (fopAc_ac_c*)proc;
    const s16 name = fopAcM_GetName(ac);
    const char* stage = dStage_getName(name, -1);
    if (stage == nullptr || (unsigned char)stage[0] >= 0x80) {
        stage = dStage_getName(name, 0);
    }
    writef(STDERR_FILENO, "[cos] actor %d %s room %d pos %.0f %.0f %.0f angle %d params %08x argument %d\n", (int)name,
           stage != nullptr && (unsigned char)stage[0] < 0x80 ? stage : "-", (int)fopAcM_GetRoomNo(ac),
           (double)ac->current.pos.x, (double)ac->current.pos.y, (double)ac->current.pos.z,
           (int)ac->shape_angle.y, (unsigned)fopAcM_GetParam(ac), (int)ac->argument);
    return nullptr;
}

void actorListFrame(unsigned int frames) {
    static const char* list = getenv("COS_ACTOR_LIST");
    if (list == nullptr || list[0] == '\0') {
        return;
    }
    for (const char* p = list; *p != '\0';) {
        char* end = nullptr;
        const unsigned long f = strtoul(p, &end, 10);
        if (end == p) {
            return;
        }
        if (f == frames) {
            writef(STDERR_FILENO, "[cos] actor list frame %u\n", frames);
            fopAcIt_Judge(listActor, nullptr);
        }
        p = *end == ',' ? end + 1 : end;
    }
}
} // namespace

void shoreFoamFrame(unsigned int frames) {
    actorListFrame(frames);
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "shore-foam") != 0) {
            return;
        }
        const PcBootStage* boot = pc_boot_stage();
        if (boot == nullptr || strcmp(boot->stage, "sea") != 0 || boot->room != 44) {
            writef(STDERR_FILENO, "[cos] shore-foam: needs COS_BOOT_STAGE in Outset, e.g. sea:44:8\n");
            pc_exit(PC_EXIT_USAGE);
        }
        sState = kWaitLink;
    }
    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWaitLink:
        if (outsetLinkReady()) {
            placeCamera(0);
            sState = kSettle;
            sSince = 0;
        }
        return;
    case kSettle:
        if (++sSince >= kSettleFrames) {
            sState = kPath;
            sSince = 0;
        }
        return;
    case kPath:
        break;
    }
    if (!captureFrame(frames, measureFrame, nullptr)) {
        writef(STDERR_FILENO, "[cos] shore-foam: FAIL: no image of frame %u\n", frames);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    if (++sSince < kPathFrames) {
        placeCamera(sSince);
        return;
    }
    sState = kDone;
    std::vector<unsigned int> sorted = sFoam;
    std::sort(sorted.begin(), sorted.end());
    const unsigned int minFoam = sorted.front(), maxFoam = sorted.back();
    const unsigned int median = sorted[sorted.size() / 2];
    const double share = sRegionPixels > 0 ? 100.0 * median / (double)sRegionPixels : 0.0;
    writef(STDERR_FILENO, "[cos] shore-foam: %zu frames: foam min %u / median %u / max %u px (median "
                          "%.2f %% of the region), %.0f px changed a frame\n",
           sFoam.size(), minFoam, median, maxFoam, share,
           (double)sSumXor / (double)(sFoam.size() - 1));
    // The foam lines and the dark sea shadow around the rocks are decals 1 unit above the
    // island's water: with too little depth precision they are hidden in whole or in part, and
    // which part changes as the camera moves (bug B7).
    if (share < kMinFoamPercent) {
        writef(STDERR_FILENO, "[cos] shore-foam: FAIL: the foam covers %.2f %% of the region "
                              "(expected at least %.2f %%): the shore decals are hidden\n",
               share, kMinFoamPercent);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    // A flicker: a frame with clearly less foam than both its neighbours (the camera moves
    // slowly, so the foam a frame shows changes by a few per cent at most).
    unsigned int flickers = 0;
    double worst = 1.0;
    for (size_t i = 1; i + 1 < sFoam.size(); i++) {
        const unsigned int neighbours = std::min(sFoam[i - 1], sFoam[i + 1]);
        if (neighbours == 0) {
            continue;
        }
        const double ratio = (double)sFoam[i] / (double)neighbours;
        worst = std::min(worst, ratio);
        if (ratio < kMinFoamRatio) {
            writef(STDERR_FILENO, "[cos] shore-foam: frame %zu of the path: %u foam pixels, its "
                                  "neighbours %u and %u\n",
                   i, sFoam[i], sFoam[i - 1], sFoam[i + 1]);
            flickers++;
        }
    }
    writef(STDERR_FILENO, "[cos] shore-foam: lowest frame against its neighbours %.3f\n", worst);
    if (flickers > 0) {
        writef(STDERR_FILENO, "[cos] shore-foam: FAIL: %u frame(s) under %.0f %% of the foam of both "
                              "neighbours: the shore decals flicker\n",
               flickers, 100.0 * kMinFoamRatio);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    writef(STDERR_FILENO, "[cos] shore-foam: ok\n");
    pc_exit(PC_EXIT_REACHED);
}

} // namespace pc

extern "C" int pc_camera_override(float* eye, float* center) {
    if (!pc::sCameraRead) {
        pc::readCamera();
    }
    if (!pc::sCameraOn) {
        return 0;
    }
    memcpy(eye, pc::sEye, sizeof(pc::sEye));
    memcpy(center, pc::sCenter, sizeof(pc::sCenter));
    return 1;
}
