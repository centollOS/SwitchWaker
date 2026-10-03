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

// The game's main (m_Do_main.cpp; <aurora/main.h> renames it).
int aurora_main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif
