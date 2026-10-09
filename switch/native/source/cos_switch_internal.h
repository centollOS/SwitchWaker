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

// cos_switch.cpp: logs (tee of stdout/stderr to the SD card and USB), the settings file and the defaults,
// the system report and the crash handler. Called by main before anything else.
void cos_switch_start(int argc, char** argv);

// cos_shader_cache.cpp: the deko3d shader registry's "[cos] shaders:" line (switch/deko/shaders.cpp)
// for the memory report and the exit (force: kept for the callers; the line is always built).
int cos_switch_shader_cache_report(char* out, size_t size, int force);

// cos_switch.cpp: the debug server's reload: the GPU profile restored and the logs written as at an exit, then
// the application restarts (appletRestartProgram: the forwarder loads the NRO again). Returns only on failure.
void cos_switch_restart(void);

// cos_debug.cpp: the debug server (debug_server.h), started when COS_DEBUG_SERVER is 1 (or a port; the menu row);
// otherwise the log text kept since start is let go. Called by cos_switch_start once the settings file is read.
void cos_switch_debug_start(void);
// cos_debug.cpp, for the gamepad shim (switch/aurora/sdl3_shim/sdl3_shim_gamepad.c): the debug server's
// presses added to the controller's buttons (HidNpadButton bits) and its sticks over the controller's
// (left x, y, right x, y; libnx units, up positive). Nothing while the server does not run.
// Returns 1 while it runs: the controller then counts as connected even with none attached
// (docked with the Joy-Cons off the console), so the game takes the server's presses.
int cos_switch_debug_input(uint64_t* buttons, int32_t sticks[4]);

// The game's main (m_Do_main.cpp; <aurora/main.h> renames it).
int aurora_main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif
