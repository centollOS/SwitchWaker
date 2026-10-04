// The persistent shader cache on the Switch (docs/SWITCH_BUILD.md, "Shader cache").
//
// The NRO links the Mesa scripts/switch/build_mesa.sh builds (switch/mesa/patches): its disk
// shader cache keeps, in one file, the GLSL compiler's results (a program linked once is not
// compiled or linked again: glCompileShader is deferred, glLinkProgram loads the program) and
// nvc0's machine code; with it on, Mesa also offers program binaries, which Dawn keeps in its own
// blob cache (user/cache/dawn_cache.db) and loads instead of compiling.
//
// COS_SWITCH_SHADER_CACHE (env.txt): 1 (default) on; 0 off (MESA_SHADER_CACHE_DISABLE: no cache
// and no program binaries, as with devkitPro's Mesa); reset deletes the cache file first. The
// file is COS_SWITCH_ROOT/user/cache/mesa_shader_cache.bin unless MESA_SHADER_CACHE_DIR is set
// (env.txt); deleting user/cache/ (docs: "If something goes wrong") clears it with Aurora's caches.
//
// Log: "[switch] shader cache: <file>: N entries, ... MiB, opened in T ms" once Mesa has opened it
// (or why it is off), and "[switch] shader compile: ..." with Mesa's compile counters and timers
// and Dawn's program binary counts, every 15 s while they change and at exit.
#include "cos_switch_internal.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#if defined(COS_SWITCH_MESA_STATS)
#include <mesa_switch.h>
#endif

#include "cos_switch.h"

// switch/dawn/patches/dawn-switch-gl-program-binary-stats.patch
extern "C" int dawn_switch_gl_program_binary_stats(uint64_t* out, int count);

namespace {

constexpr const char* kDefaultDir = COS_SWITCH_ROOT "/user/cache";

bool gStatusShown = false;
uint64_t gLastSignature = 0;

double ms(uint64_t ns) { return ns / 1e6; }

} // namespace

int cos_switch_shader_cache_setup(char* note, size_t size) {
    const char* mode = getenv("COS_SWITCH_SHADER_CACHE");
    if (mode != nullptr && strcmp(mode, "0") == 0) {
        setenv("MESA_SHADER_CACHE_DISABLE", "true", 1);
        return snprintf(note, size, "[switch] shader cache: off (COS_SWITCH_SHADER_CACHE=0)\n");
    }
    if (getenv("MESA_SHADER_CACHE_DIR") == nullptr) {
        setenv("MESA_SHADER_CACHE_DIR", kDefaultDir, 0);
    }
    if (mode != nullptr && strcmp(mode, "reset") == 0) {
        char path[512];
        snprintf(path, sizeof(path), "%s/mesa_shader_cache.bin", getenv("MESA_SHADER_CACHE_DIR"));
        const int removed = unlink(path) == 0;
        return snprintf(note, size, "[switch] shader cache: COS_SWITCH_SHADER_CACHE=reset: %s %s\n",
                        removed ? "deleted" : "no", path);
    }
#if defined(COS_SWITCH_MESA_STATS)
    return snprintf(note, size, "[switch] shader cache: MESA_SHADER_CACHE_DIR=%s\n",
                    getenv("MESA_SHADER_CACHE_DIR"));
#else
    return snprintf(note, size, "[switch] shader cache: not in this build (devkitPro's Mesa)\n");
#endif
}

int cos_switch_shader_cache_report(char* out, size_t size, int force) {
    int n = 0;
    out[0] = '\0';
#if defined(COS_SWITCH_MESA_STATS)
    const char* status = mesa_switch_shader_cache_status();
    // Mesa writes its status when it creates the cache (eglInitialize); before that the default.
    const bool opened = strncmp(status, "off (no shader cache", 20) != 0;
    if (opened && !gStatusShown) {
        gStatusShown = true;
        n += snprintf(out + n, size - n, "[switch] shader cache: %s\n", status);
    }
    uint64_t s[MESA_SWITCH_STAT_COUNT] = {};
    mesa_switch_get_stats(s, MESA_SWITCH_STAT_COUNT);
#endif
    uint64_t d[6] = {};
    dawn_switch_gl_program_binary_stats(d, 6);

#if defined(COS_SWITCH_MESA_STATS)
    uint64_t signature = d[1] + d[2] + d[3] + d[4];
    for (uint64_t v : s) {
        signature = signature * 31 + v;
    }
    if (!force && signature == gLastSignature) {
        return n;
    }
    gLastSignature = signature;
    if ((size_t)n >= size) {
        return n;
    }
    n += snprintf(out + n, size - n,
                  "[switch] shader compile: compiles %llu (%llu deferred) %.0f ms; links %llu (%llu from "
                  "cache) %.0f ms = glsl %.0f + st %.0f; nvc0 %llu (%llu from cache) %.0f ms; "
                  "binaries loaded %llu (%llu refused) %.0f ms, saved %llu %.0f ms; cache gets %llu "
                  "(%llu hits, %llu damaged) %.0f ms, puts %llu (%.1f MiB, %llu dropped) %.0f ms; "
                  "dawn binaries: formats %llu, hits %llu, misses %llu, refused %llu, stored %llu "
                  "(%.1f MiB)\n",
                  (unsigned long long)s[MESA_SWITCH_GLSL_COMPILES],
                  (unsigned long long)s[MESA_SWITCH_GLSL_COMPILES_SKIPPED], ms(s[MESA_SWITCH_GLSL_COMPILE_NS]),
                  (unsigned long long)s[MESA_SWITCH_LINKS], (unsigned long long)s[MESA_SWITCH_LINKS_FROM_CACHE],
                  ms(s[MESA_SWITCH_LINK_NS]), ms(s[MESA_SWITCH_LINK_GLSL_NS]), ms(s[MESA_SWITCH_LINK_ST_NS]),
                  (unsigned long long)s[MESA_SWITCH_NVC0_TRANSLATES],
                  (unsigned long long)s[MESA_SWITCH_NVC0_CACHE_HITS], ms(s[MESA_SWITCH_NVC0_TRANSLATE_NS]),
                  (unsigned long long)s[MESA_SWITCH_BINARY_LOADS],
                  (unsigned long long)s[MESA_SWITCH_BINARY_LOAD_FAILS], ms(s[MESA_SWITCH_BINARY_LOAD_NS]),
                  (unsigned long long)s[MESA_SWITCH_BINARY_SAVES], ms(s[MESA_SWITCH_BINARY_SAVE_NS]),
                  (unsigned long long)s[MESA_SWITCH_CACHE_GETS], (unsigned long long)s[MESA_SWITCH_CACHE_GET_HITS],
                  (unsigned long long)s[MESA_SWITCH_CACHE_CORRUPT], ms(s[MESA_SWITCH_CACHE_GET_NS]),
                  (unsigned long long)s[MESA_SWITCH_CACHE_PUTS], s[MESA_SWITCH_CACHE_PUT_BYTES] / 1048576.0,
                  (unsigned long long)s[MESA_SWITCH_CACHE_PUTS_DROPPED], ms(s[MESA_SWITCH_CACHE_PUT_NS]),
                  (unsigned long long)d[0], (unsigned long long)d[1], (unsigned long long)d[2],
                  (unsigned long long)d[3], (unsigned long long)d[4], d[5] / 1048576.0);
#else
    const uint64_t signature = d[1] + d[2] + d[3] + d[4];
    if (!force && signature == gLastSignature) {
        return n;
    }
    gLastSignature = signature;
    n += snprintf(out + n, size - n,
                  "[switch] shader compile: dawn binaries: formats %llu, hits %llu, misses %llu, "
                  "refused %llu, stored %llu\n",
                  (unsigned long long)d[0], (unsigned long long)d[1], (unsigned long long)d[2],
                  (unsigned long long)d[3], (unsigned long long)d[4]);
#endif
    return n;
}
