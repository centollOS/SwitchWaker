// cos_sdk: default OSReport, OSVReport and OSPanic (docs/NATIVE_PORT_PHASE2_3.md, step 2.6b).
//
// The game defines these itself (m_Do_printf.cpp: OSReport and OSVReport print with vprintf,
// OSPanic prints a back chain and halts), and Aurora declares them weak. The definitions here are
// weak too and only serve programs without the game, such as cos_sdk_smoke and the SDK code of
// later steps run on its own. This file defines nothing else on purpose: a static-library member
// is only loaded for a symbol nobody else defines, so with the game linked it is never loaded and
// the game's definitions are the ones used (even in the aurora header mode, where the game's are
// weak as well).
//
// Provenance: adapted from Dusklight src/dusk/OSReport.cpp (CC0, ref/dusklight). Its borealis log
// calls become fprintf(stderr); its OSReport_* variants, OSAttention and the OSReport switches
// (__OSReport_disable, ...) are left out because the game's m_Do_printf.cpp defines them. Changed:
// OSPanic aborts after printing (Dusklight's Log.fatal also ends the process), and the message
// keeps its own newlines, as on the console.
#include <dolphin/os.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

extern "C" {

__attribute__((weak)) void OSVReport(const char* msg, va_list list) {
    std::vfprintf(stderr, msg, list);
}

__attribute__((weak)) void OSReport(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    OSVReport(msg, args);
    va_end(args);
}

__attribute__((weak)) void OSPanic(const char* file, int line, const char* msg, ...) {
    std::fputs("[cos_sdk] PANIC: ", stderr);
    va_list args;
    va_start(args, msg);
    std::vfprintf(stderr, msg, args);
    va_end(args);
    std::fprintf(stderr, " in \"%s\" on line %d.\n", file, line);
    std::fflush(stderr);
    std::abort();
}

} // extern "C"
