/* The run harness's Switch platform layer (switch/native/source): what native/src/pc does with
 * macOS facilities (signals, backtrace(), the executable's directory, _Exit) done with libnx.
 * Plain C and no libnx types, so the harness (compiled like a game unit, with clang) includes it
 * without libnx's headers. */
#ifndef COS_SWITCH_H
#define COS_SWITCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The native port's directory on the SD card, without the "sdmc:" device: sqlite (Aurora's
 * caches) treats a path that does not start with '/' as relative. */
#define COS_SWITCH_ROOT "/switch/centollos/native"
/* The disc image, shared with the translated port (scripts/switch/push.sh --game). */
#define COS_SWITCH_DEFAULT_DISC "/switch/centollos/GZLE01.iso"

/* Ends the process with `code`, from any thread: flushes the logs to the SD card and the USB
 * host, then exits the process (svcExitProcess: the threads the game started cannot be stopped,
 * so the app does not return to the Homebrew Menu in the same process). */
__attribute__((noreturn)) void cos_switch_exit(int code);

/* Writes every queued log byte to the SD card (and gives the USB host up to a second). */
void cos_switch_flush_logs(void);

/* Load address of the NRO: an address minus this is the offset addr2line takes with centollos.elf. */
uintptr_t cos_switch_image_base(void);

/* Kernel thread ID of the calling thread. */
uint64_t cos_switch_thread_id(void);

/* Reads one 8-byte word if the address is mapped readable (svcQueryMemory); 0 otherwise. */
int cos_switch_read_word(uintptr_t addr, uintptr_t* out);

/* The crash report (libnx's exception handler) calls this to add the harness's state line. */
void cos_switch_set_crash_state_writer(void (*writer)(int fd));

#ifdef __cplusplus
}
#endif

#endif /* COS_SWITCH_H */
