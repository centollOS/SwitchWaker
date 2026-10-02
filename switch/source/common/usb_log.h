// Live diagnostic log over USB for the Switch probes. Bytes queued here are
// sent through libnx usbComms (057e:3000) by a background thread, so a probe
// never blocks on USB; scripts/switch/usb_log.py prints them on the host.
// With no host reading, the queue fills and further bytes are dropped.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Starts usbComms and the sender thread. Returns false if either fails; the
// other usb_log calls are then no-ops.
bool usb_log_start(void);

// Queues bytes for the host. Never blocks on USB.
void usb_log_write(const void* data, size_t size);

// Waits up to timeout_ms for the host to take the queued bytes, then stops.
void usb_log_stop(unsigned timeout_ms);

#ifdef __cplusplus
}
#endif
