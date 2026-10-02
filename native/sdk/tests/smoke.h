// cos_sdk_smoke: a headless test program for cos_sdk (no window, no GPU).
//
// Each test file in native/sdk/tests registers its tests by name, so the phase 2 steps add test
// files without editing a shared one:
//
//   COS_SMOKE_TEST(threads) {
//       COS_SMOKE_CHECK(OSIsThreadTerminated(&t));
//       return true;
//   }
//
// `cos_sdk_smoke` runs every test; `cos_sdk_smoke <name>...` runs the named ones; `--list` lists
// them. It prints "ok" and exits 0 when all pass.
#ifndef COS_SDK_TESTS_SMOKE_H
#define COS_SDK_TESTS_SMOKE_H

#include <cstdio>

namespace cos_smoke {

using TestFn = bool (*)();

// Adds a test to the registry; used by COS_SMOKE_TEST at static initialisation.
int Register(const char* name, TestFn fn);

} // namespace cos_smoke

#define COS_SMOKE_TEST(name)                                                                       \
    static bool cos_smoke_test_##name();                                                           \
    [[maybe_unused]] static const int cos_smoke_reg_##name =                                       \
        ::cos_smoke::Register(#name, &cos_smoke_test_##name);                                      \
    static bool cos_smoke_test_##name()

// Fails the current test with the file, line and expression.
#define COS_SMOKE_CHECK(expr)                                                                      \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#endif // COS_SDK_TESTS_SMOKE_H
