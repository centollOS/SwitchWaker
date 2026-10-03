// cos_sdk_smoke: the test runner, plus the "basic" test of step 2.2 (OSInit, OSGetTime,
// PSMTXConcat), which needs no window and no GPU: Aurora's OSInit does not depend on
// aurora_initialize. (sdk_devices.cpp sets AuroraConfig.mem1Size/mem2Size at static
// initialisation, so OSInit allocates MEM1 and the arena; nothing here depends on that.)
#include "smoke.h"

#include <dolphin/mtx.h>
#include <dolphin/os.h>

#include "cos_sdk/sdk.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace cos_smoke {
namespace {

struct Test {
    const char* name;
    TestFn fn;
};

std::vector<Test>& Registry() {
    static std::vector<Test> tests;
    return tests;
}

} // namespace

int Register(const char* name, TestFn fn) {
    Registry().push_back({name, fn});
    return 0;
}

} // namespace cos_smoke

COS_SMOKE_TEST(basic) {
    OSInit();
    OSInit(); // a second call must be harmless

    // OSGetTime: monotonic, and in units of the GameCube timer (OS_TIMER_CLOCK ticks per second).
    const OSTime t0 = OSGetTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const OSTime t1 = OSGetTime();
    COS_SMOKE_CHECK(t1 > t0);
    const OSTime elapsedMs = OSTicksToMilliseconds(t1 - t0);
    COS_SMOKE_CHECK(elapsedMs >= 15 && elapsedMs < 5000);

    // PSMTXConcat (C_MTXConcat in aurora_mtx): translate(1, 2, 3) * scale(2, 3, 4).
    Mtx translate, scale, out;
    PSMTXIdentity(translate);
    translate[0][3] = 1.0f;
    translate[1][3] = 2.0f;
    translate[2][3] = 3.0f;
    PSMTXIdentity(scale);
    scale[0][0] = 2.0f;
    scale[1][1] = 3.0f;
    scale[2][2] = 4.0f;
    PSMTXConcat(translate, scale, out);
    const float expected[3][4] = {
        {2.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 3.0f, 0.0f, 2.0f},
        {0.0f, 0.0f, 4.0f, 3.0f},
    };
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            COS_SMOKE_CHECK(std::fabs(out[r][c] - expected[r][c]) < 1e-6f);
        }
    }

    COS_SMOKE_CHECK(std::strlen(COSSdkAuroraCommit()) == 40);
    return true;
}

int main(int argc, char** argv) {
    // Headless: SDL's audio (the AI's output, step 5.3) goes to its dummy driver unless the
    // caller chose one.
    setenv("SDL_AUDIO_DRIVER", "dummy", 0);
    auto tests = cos_smoke::Registry();
    std::sort(tests.begin(), tests.end(), [](const cos_smoke::Test& a, const cos_smoke::Test& b) {
        return std::strcmp(a.name, b.name) < 0;
    });

    std::vector<const cos_smoke::Test*> selected;
    if (argc <= 1) {
        for (const auto& test : tests) {
            selected.push_back(&test);
        }
    } else if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& test : tests) {
            std::printf("%s\n", test.name);
        }
        return 0;
    } else {
        for (int i = 1; i < argc; i++) {
            auto it = std::find_if(tests.begin(), tests.end(), [&](const cos_smoke::Test& t) {
                return std::strcmp(t.name, argv[i]) == 0;
            });
            if (it == tests.end()) {
                std::fprintf(stderr, "cos_sdk_smoke: unknown test '%s' (--list lists them)\n",
                             argv[i]);
                return 2;
            }
            selected.push_back(&*it);
        }
    }

    int failed = 0;
    for (const auto* test : selected) {
        if (!test->fn()) {
            std::fprintf(stderr, "cos_sdk_smoke: FAIL %s\n", test->name);
            failed++;
        }
    }
    if (failed != 0) {
        std::fprintf(stderr, "cos_sdk_smoke: %d of %zu tests failed\n", failed, selected.size());
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
