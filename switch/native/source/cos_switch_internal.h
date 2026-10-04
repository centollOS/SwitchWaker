// Between the parts of the Switch platform layer (switch/native/source).
#pragma once

#include "cos_switch.h"

#ifdef __cplusplus
extern "C" {
#endif

// thread_wrap.c: the next pthread created prefers this core (the game thread: core 0).
void cos_switch_next_thread_core(int core);
// thread_wrap.c: pthreads created so far.
unsigned cos_switch_threads_created(void);

// cos_switch.cpp: logs (tee of stdout/stderr to the SD card and USB), env.txt and the defaults,
// the system report and the crash handler. Called by main before anything else.
void cos_switch_start(int argc, char** argv);

// cos_shader_cache.cpp: the persistent shader cache (COS_SWITCH_SHADER_CACHE, MESA_SHADER_CACHE_DIR)
// set up before EGL starts; the note is a log line. The report is the "[switch] shader cache" and
// "[switch] shader compile" lines (empty when nothing changed since the last one, unless force).
int cos_switch_shader_cache_setup(char* note, size_t size);
int cos_switch_shader_cache_report(char* out, size_t size, int force);

// The game's main (m_Do_main.cpp; <aurora/main.h> renames it).
int aurora_main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif
