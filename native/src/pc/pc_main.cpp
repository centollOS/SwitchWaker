// Aurora bring-up of the native executable centollos (docs/NATIVE_PORT_PHASE4_6.md, step 6.1): called by
// m_Do_main.cpp's main (TARGET_PC) right after pc_harness_init, before any of the game's own code.
//
// - aurora_initialize: window and WebGPU device; MEM1 256 MiB (decision H5) and ARAM 16 MiB, which
//   OSInit allocates (so OSInit runs here, after the configuration exists: on the GameCube __start
//   called it before main); user and cache data under <directory of the executable>/user (for the
//   build, build/native-mac/user, which git ignores); vsync off when COS_UNCAPPED.
// - aurora_dvd_open(COS_DISC) before anything calls DVDInit, then DVDGetCurrentDiskID must be
//   GZLE01 version 0 (pc_disc.cpp checked the image header already; this checks what the game
//   will read), else exit 14.
// - cos_sdk thread hooks: a thread made by OSCreateThread starts with the current JKRHeap of the
//   thread whose OSResumeThread started it. On the GameCube that resume is where the new thread is
//   first switched to, and the heap current at that moment stays current in it (JKRThreadSwitch's
//   callback swaps heaps only for JKRThreads, which reach their run loop without allocating from
//   it). JKRHeap::sCurrentHeap is per host thread on PC (JKRHeap.h).
// - COS_AUDIO=off: mDoAud_zelAudio_c::onInitFlag(), so the game sees the audio system as ready
//   and never starts it (Dusklight's DUSK_AUDIO_DISABLED; audio is phase 5).
//
// From Dusklight (CC0, ref/dusklight/src/m_Do/m_Do_main.cpp, main: AuroraConfig set-up with
// mem1Size 256 MiB, aurora_initialize, aurora_dvd_open before OSInit; main01: onInitFlag when
// audio is disabled). Changed: no settings, ImGui, mods or prelaunch UI; paths and options come
// from the harness environment; failures exit through the harness codes.
#include "pc_internal.h"
#include "pc/pc_aspect.h"

#include "JSystem/JKernel/JKRHeap.h"
#include "m_Do/m_Do_audio.h"
#include "cos_sdk/hooks.h"

#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/os.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

namespace pc {

namespace {

constexpr uint32_t kMem1Size = 256u * 1024 * 1024; // decision H5
constexpr uint32_t kMem2Size = 16u * 1024 * 1024;  // ARAM, as on the GameCube

char sUserPath[PATH_MAX];
char sCachePath[PATH_MAX];

// Creates path (one level; its parent must exist). Exits 2 if that fails.
void makeDir(const char* path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        writef(STDERR_FILENO, "[cos] cannot create %s: %s\n", path, strerror(errno));
        pc_exit(PC_EXIT_USAGE);
    }
}

// <directory of the executable>/user and <directory of the executable>/user/cache; on the Switch
// <COS_SWITCH_ROOT>/user (the native port's directory on the SD card, switch/native/source).
void makeUserPaths(const char* argv0) {
#if defined(__SWITCH__)
    (void)argv0;
    snprintf(sUserPath, sizeof(sUserPath), "%s/user", COS_SWITCH_ROOT);
#else
    char exe[PATH_MAX] = {};
#if defined(__APPLE__)
    uint32_t size = sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) != 0) {
        exe[0] = '\0';
    }
#endif
    if (exe[0] == '\0' && argv0 != nullptr) {
        snprintf(exe, sizeof(exe), "%s", argv0);
    }
    char real[PATH_MAX];
    const char* path = realpath(exe, real) != nullptr ? real : exe;
    const char* slash = strrchr(path, '/');
    int dirLen = slash != nullptr ? (int)(slash - path) : 0;
    if (slash == nullptr) {
        path = ".";
        dirLen = 1;
    }
    snprintf(sUserPath, sizeof(sUserPath), "%.*s/user", dirLen, path);
#endif
    snprintf(sCachePath, sizeof(sCachePath), "%s/cache", sUserPath);
    makeDir(sUserPath);
    makeDir(sCachePath);
}

const char* backendName(AuroraBackend backend) {
    switch (backend) {
    case BACKEND_AUTO: return "auto";
    case BACKEND_D3D11: return "d3d11";
    case BACKEND_D3D12: return "d3d12";
    case BACKEND_METAL: return "metal";
    case BACKEND_VULKAN: return "vulkan";
    case BACKEND_OPENGL: return "opengl";
    case BACKEND_OPENGLES: return "opengles";
    case BACKEND_WEBGPU: return "webgpu";
    case BACKEND_NULL: return "null";
    }
    return "?";
}

// Launch hook: runs on the thread calling OSResumeThread; its current heap goes to the new thread.
void* threadLaunchHook(OSThread* thread) {
    (void)thread;
    return JKRHeap::getCurrentHeap();
}

// Start hook: runs on the new thread before its entry function.
void threadStartHook(OSThread* thread, void* launchValue) {
    (void)thread;
    JKRHeap::sCurrentHeap = static_cast<JKRHeap*>(launchValue);
}

} // namespace

} // namespace pc

using namespace pc;

extern "C" {

OSThread* pc_main_thread(void) {
    return COSSdkGetDefaultThread();
}

void pc_aurora_init(int argc, char* argv[]) {
    makeUserPaths(argc > 0 ? argv[0] : nullptr);

    AuroraConfig config{};
    config.appName = "centollos";
    config.userPath = sUserPath;
    config.cachePath = sCachePath;
    config.desiredBackend = BACKEND_AUTO;
    config.vsync = !gConfig.uncapped;
    config.windowPosX = -1;
    config.windowPosY = -1;
    // 960x720, or as wide as COS_ASPECT at that height (1280x720 at 16:9, 1152x720 at 16:10).
    config.windowWidth = (int)(480 * 3 / 2 * pc_aspect_ratio() + 0.5f);
    config.windowHeight = 480 * 3 / 2;
    config.logLevel = LOG_INFO;
    config.allowTextureDumps = false;
    config.mem1Size = kMem1Size;
    config.mem2Size = kMem2Size;
    config.blockingPipelines = gConfig.syncPipelines;
    const AuroraInfo info = aurora_initialize(argc, argv, &config);
    if (info.window == nullptr) {
        writef(STDERR_FILENO, "[cos] aurora_initialize returned no window\n");
        pc_exit(PC_EXIT_USAGE);
    }
    if (pc_aspect_wide()) {
        // The game draws an anamorphic picture into the 640x480 EFB (pc_aspect.h): Aurora presents
        // it at the wider aspect, letterboxed or pillarboxed in a window of another shape (patch
        // 0006), as a widescreen TV stretches the console's output.
        AuroraSetFitAspect(pc_aspect_ratio());
    }
    writef(STDERR_FILENO,
           "[cos] aurora: backend=%s window=%ux%u framebuffer=%ux%u vsync=%d pipelines=%s "
           "user=%s\n",
           backendName(info.backend), (unsigned int)info.windowSize.width,
           (unsigned int)info.windowSize.height, (unsigned int)info.windowSize.fb_width,
           (unsigned int)info.windowSize.fb_height, config.vsync ? 1 : 0,
           config.blockingPipelines ? "sync" : "async", sUserPath);

    // Before DVDInit (Aurora's rule); pc_harness_init already checked COS_DISC is set and readable.
    if (!aurora_dvd_open(gConfig.disc)) {
        writef(STDERR_FILENO, "[cos] DISC: aurora_dvd_open(%s) failed\n", gConfig.disc);
        pc_exit(PC_EXIT_DISC);
    }
    const DVDDiskID* id = DVDGetCurrentDiskID();
    if (id == nullptr || memcmp(id->gameName, "GZLE", 4) != 0 || memcmp(id->company, "01", 2) != 0 ||
        id->gameVersion != 0) {
        writef(STDERR_FILENO, "[cos] DISC: DVDGetCurrentDiskID is %.4s%.2s version %u; the supported "
                              "disc is GZLE01 version 0\n",
               id != nullptr ? id->gameName : "????", id != nullptr ? id->company : "??",
               id != nullptr ? (unsigned int)id->gameVersion : 0u);
        pc_exit(PC_EXIT_DISC);
    }
    writef(STDERR_FILENO, "[cos] dvd: %.4s%.2s version %u disc %u\n", id->gameName, id->company,
           (unsigned int)id->gameVersion, (unsigned int)id->diskNumber);

    // On the GameCube __start ran OSInit before main; here MEM1 (mem1Size) must be configured first.
    OSInit();

    COSSdkSetThreadLaunchHook(threadLaunchHook);
    COSSdkSetThreadStartHook(threadStartHook);

    if (!gConfig.audio) {
        // Dusklight: "Pretend the audio engine initialized already. This is a lie, but needed to boot."
        mDoAud_zelAudio_c::onInitFlag();
    }

    // COS_SMOKE=save writes its own card: the path must be set before mDoMch_Create's CARDInit.
    if (gConfig.smoke != nullptr && strcmp(gConfig.smoke, "save") == 0) {
        prepareSaveSmoke();
    }
    // M11 new-game and M14 outset-real start from a clean memory card (pc_new_game.cpp): the
    // name scene finds no save file and offers to create one.
    if (newGameNeedsCleanCard()) {
        prepareRunCard(gConfig.milestone);
    }

    // Smoke tests that need Aurora and OSInit but none of the game's main code (heap) end here.
    runAuroraSmoke();
}

} // extern "C"
