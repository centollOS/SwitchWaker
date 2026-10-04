// Opt-in GPU-side reductions for A/B runs (native/include/pc/pc_gpu_opts.h).
#include "pc/pc_gpu_opts.h"

#include "pc_internal.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>
#if defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#endif

#include <dolphin/gx/GXAurora.h>

#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "JSystem/JKernel/JKRHeap.h"

// native/patches/aurora/0008 (lib/gfx/census.hpp).
extern "C" void aurora_draw_census_request(const char* pathPrefix);

#include <lib/dolphin/vi/vi_internal.hpp>
#include <lib/window.hpp>

namespace {

// -1 until the first call reads the variable.
int sShadowOffscreen = -1;
int sDof = -1;
int sMistLowres = -1;

bool envIs(const char* name, const char* value) {
    const char* v = getenv(name);
    return v != nullptr && strcmp(v, value) == 0;
}

// COS_GPU_GROUPS / COS_DRAW_CENSUS.
int sGroupsEnv = -1;                       // the COS_GPU_GROUPS level, -1 before the first frame
std::vector<unsigned int> sCensusFrames;   // sorted
const char* sBucket = "";                  // the last bucket marker
char sLastLabel[128];                      // the last packet marker

void groupsInit() {
    const char* v = getenv("COS_GPU_GROUPS");
    sGroupsEnv = v == nullptr ? 0 : atoi(v);
    sGroupsEnv = sGroupsEnv < 0 ? 0 : sGroupsEnv > 2 ? 2 : sGroupsEnv;
    const char* list = getenv("COS_DRAW_CENSUS");
    if (list != nullptr && list[0] != '\0') {
        const char* p = list;
        while (*p != '\0') {
            char* end = nullptr;
            errno = 0;
            unsigned long n = strtoul(p, &end, 10);
            if (errno != 0 || end == p || n == 0 || (*end != ',' && *end != '\0')) {
                pc::writef(STDERR_FILENO, "[cos] COS_DRAW_CENSUS=\"%s\" is not a list of frames\n", list);
                break;
            }
            sCensusFrames.push_back((unsigned int)n);
            p = *end == ',' ? end + 1 : end;
        }
        std::sort(sCensusFrames.begin(), sCensusFrames.end());
    }
    if (sGroupsEnv != 0 || !sCensusFrames.empty()) {
        pc::writef(STDERR_FILENO,
                   "[cos] gpu groups: COS_GPU_GROUPS=%d, %zu draw census frame(s); vtable labels are "
                   "offsets from pc_gpu_group=%p\n",
                   sGroupsEnv, sCensusFrames.size(), (const void*)&pc_gpu_group);
    }
}

// What a J3D packet is: "<material name>" for a model material packet, its class otherwise.
void packetLabel(const void* packet, char* out, size_t size) {
    static J3DMatPacket sMatProbe;
    const void* vtable = *static_cast<void* const*>(packet);
    if (vtable == *reinterpret_cast<void* const*>(&sMatProbe)) {
        J3DMatPacket* mat = const_cast<J3DMatPacket*>(static_cast<const J3DMatPacket*>(packet));
        J3DMaterial* material = mat->getMaterial();
        J3DShapePacket* shape = mat->getShapePacket();
        J3DModel* model = shape != nullptr ? shape->getModel() : nullptr;
        J3DModelData* data = model != nullptr ? model->getModelData() : nullptr;
        JUTNameTab* names = data != nullptr ? data->getMaterialName() : nullptr;
        const char* name = material != nullptr && names != nullptr ? names->getName(material->getIndex()) : nullptr;
        if (name != nullptr) {
            snprintf(out, size, "%s|mat:%s", sBucket, name);
            return;
        }
        snprintf(out, size, "%s|mat:?", sBucket);
        return;
    }
#if defined(__APPLE__) || defined(__linux__)
    Dl_info info{};
    if (dladdr(vtable, &info) != 0 && info.dli_sname != nullptr) {
        const char* name = info.dli_sname;
        if (strncmp(name, "_ZTV", 4) == 0) {
            name += 4;
            while (*name >= '0' && *name <= '9') {
                name++;
            }
        }
        snprintf(out, size, "%s|%s", sBucket, name);
        return;
    }
#endif
    snprintf(out, size, "%s|vt%+ld", sBucket,
             (long)((const char*)vtable - (const char*)(const void*)&pc_gpu_group));
}

} // namespace

extern "C" {

// Frames from a comma-separated list (for A/B checks that must render both variants of one frame:
// the game's timing, and with it the scene, drifts as soon as a frame costs more or less).
std::vector<unsigned int> parseFrames(const char* list) {
    std::vector<unsigned int> frames;
    while (list != nullptr && *list != '\0') {
        char* end = nullptr;
        const unsigned long n = strtoul(list, &end, 10);
        if (end == list) {
            break;
        }
        frames.push_back((unsigned int)n);
        list = *end == ',' ? end + 1 : end;
    }
    std::sort(frames.begin(), frames.end());
    return frames;
}

int pc_mist_lowres(void) {
    static std::vector<unsigned int> sOnlyFrames;
    if (sMistLowres < 0) {
        // Default 4 (docs/SWITCH_PERF_STUDY.md, section 8: Mac A/B of the same frame within 7/255,
        // mean 1.5/255, at 2, 3 and 4 alike); COS_MIST_LOWRES=0 draws the mist as before.
        const char* v = getenv("COS_MIST_LOWRES");
        sMistLowres = v == nullptr || v[0] == '\0' ? 4 : atoi(v);
        sMistLowres = sMistLowres < 2 ? 0 : sMistLowres > 4 ? 4 : sMistLowres;
        // COS_MIST_LOWRES_FRAMES=<frame>[,...]: only in those game frames (Mac pixel checks).
        sOnlyFrames = parseFrames(getenv("COS_MIST_LOWRES_FRAMES"));
        if (sMistLowres != 0) {
            pc::writef(STDERR_FILENO, "[cos] COS_MIST_LOWRES=%d: forest mist drawn at 1/%d resolution%s\n",
                       sMistLowres, sMistLowres, sOnlyFrames.empty() ? "" : " in the listed frames only");
        }
    }
    if (!sOnlyFrames.empty() &&
        !std::binary_search(sOnlyFrames.begin(), sOnlyFrames.end(), pc_frame_count() + 1)) {
        return 0;
    }
    return sMistLowres;
}

void* pc_mist_lowres_target(unsigned int* w, unsigned int* h) {
    static std::vector<unsigned char> sBuffer;
    static unsigned int sLastW = 0, sLastH = 0;
    const int div = pc_mist_lowres();
    if (div < 2) {
        return nullptr;
    }
    unsigned int efbW = 0, efbH = 0;
    pc_efb_pixel_size(640, 480, &efbW, &efbH);
    const unsigned int tw = (efbW + div - 1) / div;
    const unsigned int th = (efbH + div - 1) / div;
    if (tw < 16 || th < 16) {
        return nullptr;
    }
    // GXCopyTex names the copy texture by its destination; RGBA8 tiles are 4x4 texels of 4 bytes.
    const size_t bytes = (size_t)((tw + 3) & ~3u) * ((th + 3) & ~3u) * 4;
    if (sBuffer.size() < bytes) {
        JKRPcHostAllocScope hostAlloc; // not the game's heaps
        sBuffer.resize(bytes);
    }
    if (tw != sLastW || th != sLastH) {
        sLastW = tw;
        sLastH = th;
        pc::writef(STDERR_FILENO, "[cos] mist target %ux%u for a %ux%u EFB (frame %u)\n", tw, th, efbW, efbH,
                   pc_frame_count());
    }
    *w = tw;
    *h = th;
    return sBuffer.data();
}

static unsigned int mistAbFrame() {
    static int sFrame = -1;
    if (sFrame < 0) {
        const char* v = getenv("COS_MIST_AB");
        sFrame = v != nullptr ? atoi(v) : 0;
    }
    return (unsigned int)sFrame;
}

int pc_mist_ab_frame(void) {
    return mistAbFrame() != 0 && pc_frame_count() + 1 == mistAbFrame();
}

void* pc_mist_ab_buffer(int index) {
    static std::vector<unsigned char> sBuffers[3];
    auto& b = sBuffers[index < 0 ? 0 : index > 2 ? 2 : index];
    if (b.empty()) {
        JKRPcHostAllocScope hostAlloc; // not the game's heaps
        b.resize(2560 * 1440 * 4);
    }
    return b.data();
}

void* pc_mist_ab_show(void) {
    const unsigned int ab = mistAbFrame();
    if (ab == 0) {
        return nullptr;
    }
    const unsigned int frame = pc_frame_count() + 1;
    if (frame == ab + 1) {
        return pc_mist_ab_buffer(1);
    }
    if (frame == ab + 2) {
        return pc_mist_ab_buffer(2);
    }
    return nullptr;
}

int pc_gpu_groups_level = 0;

void pc_gpu_groups_frame_begin(unsigned int frame) {
    if (sGroupsEnv < 0) {
        groupsInit();
    }
    pc_gpu_groups_level = sGroupsEnv;
    sBucket = "";
    sLastLabel[0] = '\0';
    if (!sCensusFrames.empty() && std::binary_search(sCensusFrames.begin(), sCensusFrames.end(), frame)) {
        char path[1024];
        const char* dir = pc::gConfig.runDir != nullptr ? pc::gConfig.runDir : ".";
        snprintf(path, sizeof(path), "%s/census-%06u", dir, frame);
        aurora_draw_census_request(path);
        pc_gpu_groups_level = 2;
        pc::writef(STDERR_FILENO, "[cos] draw census: frame %u -> %s-*.csv\n", frame, path);
    }
}

void pc_gpu_group(const char* name) {
    if (pc_gpu_groups_level < 1) {
        return;
    }
    sBucket = name;
    sLastLabel[0] = '\0';
    GXInsertDebugMarker(name);
}

void pc_gpu_group_packet(const void* packet) {
    if (pc_gpu_groups_level < 2 || packet == nullptr) {
        return;
    }
    char label[sizeof(sLastLabel)];
    packetLabel(packet, label, sizeof(label));
    if (strcmp(label, sLastLabel) == 0) {
        return;
    }
    memcpy(sLastLabel, label, sizeof(label));
    GXInsertDebugMarker(label);
}

int pc_shadow_offscreen(void) {
    if (sShadowOffscreen < 0) {
        sShadowOffscreen = envIs("COS_SHADOW_OFFSCREEN", "1")    ? PC_SHADOW_OFFSCREEN_SAME
                           : envIs("COS_SHADOW_OFFSCREEN", "gc") ? PC_SHADOW_OFFSCREEN_GC
                                                                 : PC_SHADOW_OFFSCREEN_OFF;
        if (sShadowOffscreen == PC_SHADOW_OFFSCREEN_SAME) {
            pc::writef(STDERR_FILENO, "[cos] COS_SHADOW_OFFSCREEN=1: real-time shadows drawn offscreen\n");
        } else if (sShadowOffscreen == PC_SHADOW_OFFSCREEN_GC) {
            pc::writef(STDERR_FILENO,
                       "[cos] COS_SHADOW_OFFSCREEN=gc: real-time shadows drawn offscreen at the GameCube's "
                       "256x256 (128x128 textures)\n");
        }
    }
    return sShadowOffscreen;
}

void pc_efb_pixel_size(unsigned int logicalW, unsigned int logicalH, unsigned int* outW,
                       unsigned int* outH) {
    const auto [logicalFbW, logicalFbH] = aurora::vi::configured_fb_size();
    const AuroraWindowSize window = aurora::window::get_window_size();
    unsigned int w = logicalW;
    unsigned int h = logicalH;
    if (logicalFbW != 0 && logicalFbH != 0 && window.fb_width != 0 && window.fb_height != 0) {
        const float sx = static_cast<float>(window.fb_width) / static_cast<float>(logicalFbW);
        const float sy = static_cast<float>(window.fb_height) / static_cast<float>(logicalFbH);
        w = static_cast<unsigned int>(std::lround(static_cast<float>(logicalW) * sx));
        h = static_cast<unsigned int>(std::lround(static_cast<float>(logicalH) * sy));
    }
    *outW = w != 0 ? w : 1;
    *outH = h != 0 ? h : 1;
}

void pc_shadow_offscreen_opened(unsigned int w, unsigned int h, unsigned int copyW, unsigned int copyH) {
    static unsigned int sLastW = 0, sLastH = 0;
    if (w != sLastW || h != sLastH) {
        sLastW = w;
        sLastH = h;
        pc::writef(STDERR_FILENO, "[cos] shadow offscreen target %ux%u, I4 copies %ux%u (frame %u)\n", w, h,
                   copyW, copyH, pc_frame_count());
    }
}

int pc_dof_enabled(void) {
    if (sDof < 0) {
        sDof = envIs("COS_DOF", "0") ? 0 : 1;
        if (!sDof) {
            pc::writef(STDERR_FILENO, "[cos] COS_DOF=0: depth-of-field composite skipped\n");
        }
    }
    return sDof;
}

} // extern "C"
