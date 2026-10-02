#include "usb_log.h"

#include <string.h>
#include <switch.h>

// An NRO returns to hbloader inside the same process, so the sender must never
// block indefinitely: it uses the asynchronous usbComms API and bounded waits,
// and stop always joins it before usbCommsExit.

#define USB_LOG_QUEUE_SIZE (64 * 1024)
#define USB_LOG_WAIT_NS 100000000ULL

static u8 g_queue[USB_LOG_QUEUE_SIZE];
static size_t g_head;   // next byte to write
static size_t g_tail;   // next byte to send
static bool g_stop;
static bool g_in_flight;
static bool g_running;
static Mutex g_mutex;
static CondVar g_changed;
static Thread g_thread;
static u8 g_page[0x1000] __attribute__((aligned(0x1000)));

static size_t queued_locked(void) {
    return (g_head + USB_LOG_QUEUE_SIZE - g_tail) % USB_LOG_QUEUE_SIZE;
}

// Sends one page; returns false if stop was requested before it completed.
static bool send_page(size_t size) {
    while (R_FAILED(usbDsWaitReady(USB_LOG_WAIT_NS))) {
        if (__atomic_load_n(&g_stop, __ATOMIC_ACQUIRE))
            return false;
    }
    u32 urb_id = 0;
    if (R_FAILED(usbCommsWriteAsync(g_page, size, &urb_id, 0))) {
        svcSleepThread(USB_LOG_WAIT_NS);
        return true;
    }
    Event* done = usbCommsGetWriteCompletionEvent(0);
    while (R_FAILED(eventWait(done, USB_LOG_WAIT_NS))) {
        // A pending transfer is torn down by usbCommsExit after the join.
        if (__atomic_load_n(&g_stop, __ATOMIC_ACQUIRE))
            return false;
    }
    u32 transferred = 0;
    usbCommsGetWriteResult(urb_id, &transferred, 0);
    return true;
}

static void sender(void* arg) {
    (void)arg;
    for (;;) {
        mutexLock(&g_mutex);
        while (!g_stop && queued_locked() == 0)
            condvarWaitTimeout(&g_changed, &g_mutex, USB_LOG_WAIT_NS);
        if (g_stop) {
            mutexUnlock(&g_mutex);
            return;
        }
        size_t size = queued_locked();
        if (size > sizeof(g_page))
            size = sizeof(g_page);
        const size_t first = USB_LOG_QUEUE_SIZE - g_tail < size ? USB_LOG_QUEUE_SIZE - g_tail : size;
        memcpy(g_page, g_queue + g_tail, first);
        memcpy(g_page + first, g_queue, size - first);
        g_in_flight = true;
        mutexUnlock(&g_mutex);

        const bool sent = send_page(size);

        mutexLock(&g_mutex);
        g_in_flight = false;
        if (sent)
            g_tail = (g_tail + size) % USB_LOG_QUEUE_SIZE;
        condvarWakeAll(&g_changed);
        mutexUnlock(&g_mutex);
        if (!sent)
            return;
    }
}

bool usb_log_start(void) {
    if (g_running)
        return true;
    mutexInit(&g_mutex);
    condvarInit(&g_changed);
    g_head = g_tail = 0;
    g_stop = g_in_flight = false;
    if (R_FAILED(usbCommsInitialize()))
        return false;
    if (R_FAILED(threadCreate(&g_thread, sender, NULL, NULL, 0x4000, 0x2C, -2))) {
        usbCommsExit();
        return false;
    }
    if (R_FAILED(threadStart(&g_thread))) {
        threadClose(&g_thread);
        usbCommsExit();
        return false;
    }
    g_running = true;
    return true;
}

void usb_log_write(const void* data, size_t size) {
    if (!g_running || size == 0)
        return;
    mutexLock(&g_mutex);
    const size_t space = USB_LOG_QUEUE_SIZE - 1 - queued_locked();
    if (size > space)
        size = space;
    const u8* bytes = (const u8*)data;
    for (size_t i = 0; i < size; ++i) {
        g_queue[g_head] = bytes[i];
        g_head = (g_head + 1) % USB_LOG_QUEUE_SIZE;
    }
    condvarWakeAll(&g_changed);
    mutexUnlock(&g_mutex);
}

void usb_log_stop(unsigned timeout_ms) {
    if (!g_running)
        return;
    const u64 deadline = armGetSystemTick() + armNsToTicks((u64)timeout_ms * 1000000ULL);
    mutexLock(&g_mutex);
    while ((queued_locked() != 0 || g_in_flight) && armGetSystemTick() < deadline)
        condvarWaitTimeout(&g_changed, &g_mutex, USB_LOG_WAIT_NS);
    __atomic_store_n(&g_stop, true, __ATOMIC_RELEASE);
    condvarWakeAll(&g_changed);
    mutexUnlock(&g_mutex);
    threadWaitForExit(&g_thread);
    threadClose(&g_thread);
    usbCommsExit();
    g_running = false;
}
