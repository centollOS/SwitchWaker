// Shared helpers of the randomizer's unit tests (native/check/rando/README.md). Header only.
//
//   #include "rando_test.h"
//   int main() {
//       RT_CHECK(x == 1);
//       RT_CHECK_EQ(pc_rando(), 0);                       // prints both values on a failure
//       std::string json;
//       RT_CHECK(rt::readFixture("TestSeed1.json", json)); // native/check/rando/fixtures/
//       return rt::finish();                              // "ok" + 0, or the count + 1
//   }
//
// Compiled like a game unit (cos_game_headers: C++ without exceptions or RTTI). Tests never read the
// disc: fixtures come from the oracle (native/tools/rando/) and wwrando's MIT data only.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>

namespace rt {

inline int& failures() {
    static int n = 0;
    return n;
}

inline bool check(bool ok, const char* expr, const char* file, int line) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
        failures()++;
    }
    return ok;
}

// A value as text for a failed RT_CHECK_EQ: integers (and enums) in decimal and hex, floats, strings,
// pointers; anything else as "?".
template <class T> std::string showAny(const T& v) {
    char b[64];
    if constexpr (std::is_same_v<T, std::string>) {
        return '"' + v + '"';
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>) {
        return v == nullptr ? std::string("(null)") : '"' + std::string(v) + '"';
    } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
        std::snprintf(b, sizeof b, "%lld (0x%llx)", (long long)v, (unsigned long long)v);
        return b;
    } else if constexpr (std::is_floating_point_v<T>) {
        std::snprintf(b, sizeof b, "%.9g", (double)v);
        return b;
    } else if constexpr (std::is_pointer_v<T>) {
        std::snprintf(b, sizeof b, "%p", (const void*)v);
        return b;
    } else {
        return "?";
    }
}

template <class A, class B>
bool checkEq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (a == b) {
        return true;
    }
    std::fprintf(stderr, "FAIL %s:%d: %s == %s: %s vs %s\n", file, line, ea, eb, showAny(a).c_str(),
                 showAny(b).c_str());
    failures()++;
    return false;
}

// The fixture directory: COS_RANDO_FIXTURES, else the source tree's (set by CMake).
inline std::string fixturePath(const char* name) {
    const char* dir = std::getenv("COS_RANDO_FIXTURES");
#ifdef COS_RANDO_FIXTURE_DIR
    if (dir == nullptr || dir[0] == '\0') {
        dir = COS_RANDO_FIXTURE_DIR;
    }
#endif
    return std::string(dir != nullptr ? dir : ".") + "/" + name;
}

// Reads a whole file (binary). False, with a FAIL line, if it cannot be read.
inline bool readFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        std::fprintf(stderr, "FAIL cannot read %s\n", path.c_str());
        failures()++;
        return false;
    }
    out.clear();
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return true;
}
inline bool readFixture(const char* name, std::string& out) { return readFile(fixturePath(name), out); }

// Prints "ok" and returns 0 when no check failed, else the count and 1. main returns it.
inline int finish() {
    if (failures() == 0) {
        std::printf("ok\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", failures());
    return 1;
}

} // namespace rt

#define RT_CHECK(cond) rt::check(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
#define RT_CHECK_EQ(a, b) rt::checkEq((a), (b), #a, #b, __FILE__, __LINE__)
