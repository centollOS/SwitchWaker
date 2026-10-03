// cos_sdk: OSAlarm on a real host timer thread (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a,
// decision D6; the model is in os_internal.h).
//
// Provenance: written for cos_sdk. Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight) makes
// OSCreateAlarm, OSSetAlarm, OSSetPeriodicAlarm and OSCancelAlarm no-ops; decision D6 replaces
// that with alarms that fire. The semantics follow the GameCube SDK's OSAlarm.c:
// - one alarm queue ordered by fire time; OSSetPeriodicAlarm fires at start + n * period, the
//   first such time after now;
// - a fired one-shot alarm has its handler cleared before the handler runs, a periodic one is
//   queued again first, so the handler may set or cancel its own alarm;
// - handlers run with interrupts disabled (here: on the timer thread, holding the OS lock), so
//   they may call OSResumeThread, OSWakeupThread, OSSendMessage(..., OS_MESSAGE_NOBLOCK), ... but
//   must not block (cos_sdk aborts if one does);
// - OSCancelAlarm/OSCancelAlarms and OSSetAlarm hold the same lock, so once OSCancelAlarm returns
//   the handler is not running and will not run, and the alarm's memory may be reused.
// Differences: the queue is a host container keyed by the OSAlarm pointer, not the alarm's
// prev/next fields (left null), so an alarm that is set again while pending is moved instead of
// corrupting the list (logged, as the console asserts there). The handler's OSContext argument
// points at the timer thread's (empty) context.
//
// The timer thread starts with the first OSSetAlarm* call (or OSInitAlarm) and is detached.
// After COSSdkRequestShutdown it fires nothing more.
//
// Memory: the queue, the pending table and the state live in host memory (cos_sdk/host_alloc.h),
// never in the game's operator new/delete. On PC those allocate from the current JKRHeap and free
// under its OSMutex, which the timer thread may not take: an erase while the game held a heap's
// mutex was fatal ("a blocking OS call was made from an alarm handler"). For the same reason the
// timer thread is a pthread: libc++'s std::thread allocates its start state with global new.
#include "os_internal.h"

#include "cos_sdk/host_alloc.h"

#include <pthread.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <utility>

using namespace cos_sdk::os;

namespace {

// Ordered by (fire time, insertion sequence): alarms with the same fire time fire in set order.
using AlarmKey = std::pair<OSTime, std::uint64_t>;
using AlarmQueue = cos_sdk::HostMap<AlarmKey, OSAlarm*>;

struct AlarmState {
    AlarmQueue queue;
    cos_sdk::HostUnorderedMap<OSAlarm*, AlarmQueue::iterator> pending;
    std::uint64_t sequence = 0;
    std::condition_variable cv; // the timer thread waits on it with the OS lock
    bool threadStarted = false;
    OSThread threadRecord = {};
    HostThread threadHost;
};

AlarmState& State() {
    // Never destroyed (detached timer thread).
    static AlarmState* const sState = cos_sdk::HostNew<AlarmState>();
    return *sState;
}

// The longest single wait of the timer thread: OSGetTime follows Aurora's game clock, so the
// thread re-reads it at least this often instead of trusting one long host-clock wait.
constexpr std::chrono::milliseconds kMaxWait{50};

std::chrono::nanoseconds TicksToNanoseconds(OSTime ticks) {
    const OSTime seconds = ticks / OS_TIMER_CLOCK;
    const OSTime rest = ticks % OS_TIMER_CLOCK;
    return std::chrono::seconds(seconds) +
           std::chrono::nanoseconds(rest * 1000000000LL / OS_TIMER_CLOCK);
}

bool RemoveLocked(OSAlarm* alarm) {
    AlarmState& state = State();
    auto it = state.pending.find(alarm);
    if (it == state.pending.end()) {
        return false;
    }
    state.queue.erase(it->second);
    state.pending.erase(it);
    return true;
}

void* TimerThreadMain(void*) {
    AlarmState& state = State();
    tCurrent = &state.threadRecord;
    tHost = &state.threadHost;

    std::unique_lock<std::mutex> lock(Lock());
    tInterruptsDisabled = true; // handlers run "in interrupt context"
    for (;;) {
        if (ShuttingDownLocked() || state.queue.empty()) {
            state.cv.wait(lock);
            continue;
        }
        const auto head = state.queue.begin();
        const OSTime now = OSGetTime();
        if (head->first.first > now) {
            auto wait = TicksToNanoseconds(head->first.first - now);
            if (wait > kMaxWait) {
                wait = kMaxWait;
            }
            state.cv.wait_for(lock, wait);
            continue;
        }

        OSAlarm* alarm = head->second;
        state.pending.erase(alarm);
        state.queue.erase(head);
        const OSAlarmHandler handler = alarm->handler;
        if (alarm->period > 0) {
            // Queued again before the handler runs, as on the GameCube.
            alarm->fire += alarm->period;
            auto it = state.queue.emplace(AlarmKey{alarm->fire, state.sequence++}, alarm).first;
            state.pending.emplace(alarm, it);
        } else {
            alarm->handler = nullptr;
        }
        if (handler != nullptr) {
            handler(alarm, &state.threadRecord.context);
        }
    }
}

void InsertLocked(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler, const char* caller) {
    AlarmState& state = State();
    if (RemoveLocked(alarm)) {
        Log("%s(%p): the alarm was still pending; it is moved to the new time", caller,
            static_cast<void*>(alarm));
    }
    if (alarm->period > 0) {
        const OSTime now = OSGetTime();
        fire = alarm->start;
        if (alarm->start < now) {
            fire += alarm->period * ((now - alarm->start) / alarm->period + 1);
        }
    }
    alarm->handler = handler;
    alarm->fire = fire;
    alarm->prev = alarm->next = nullptr;
    auto it = state.queue.emplace(AlarmKey{fire, state.sequence++}, alarm).first;
    state.pending.emplace(alarm, it);

    if (!state.threadStarted) {
        state.threadStarted = true;
        state.threadHost.alarmThread = true;
        state.threadRecord.state = OS_THREAD_STATE_RUNNING;
        state.threadRecord.priority = state.threadRecord.base = OS_PRIORITY_MIN;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_t handle;
        const int err = pthread_create(&handle, &attr, &TimerThreadMain, nullptr);
        pthread_attr_destroy(&attr);
        if (err != 0) {
            Fatal("%s: pthread_create of the alarm thread failed (%d: %s)", caller, err,
                  std::strerror(err));
        }
    }
    if (it == state.queue.begin()) {
        state.cv.notify_all();
    }
}

} // namespace

namespace cos_sdk::os {

void NotifyAlarmThreadLocked() {
    State().cv.notify_all();
}

} // namespace cos_sdk::os

extern "C" {

void OSInitAlarm(void) {
    // Nothing to set up before the first alarm: the timer thread starts with it.
}

BOOL OSCheckAlarmQueue(void) {
    Guard guard;
    AlarmState& state = State();
    return state.queue.size() == state.pending.size() ? TRUE : FALSE;
}

void OSCreateAlarm(OSAlarm* alarm) {
    Guard guard;
    if (RemoveLocked(alarm)) {
        Log("OSCreateAlarm(%p): the alarm was still pending; it is cancelled",
            static_cast<void*>(alarm));
    }
    alarm->handler = nullptr;
    alarm->tag = 0;
    alarm->fire = 0;
    alarm->prev = alarm->next = nullptr;
    alarm->period = 0;
    alarm->start = 0;
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
    Guard guard;
    alarm->period = 0;
    InsertLocked(alarm, OSGetTime() + tick, handler, "OSSetAlarm");
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler) {
    Guard guard;
    alarm->period = 0;
    InsertLocked(alarm, time, handler, "OSSetAbsAlarm");
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    if (period <= 0) {
        Fatal("OSSetPeriodicAlarm(%p): period %lld is not positive", static_cast<void*>(alarm),
              static_cast<long long>(period));
    }
    Guard guard;
    alarm->period = period;
    alarm->start = start;
    InsertLocked(alarm, start, handler, "OSSetPeriodicAlarm");
}

void OSCancelAlarm(OSAlarm* alarm) {
    Guard guard;
    RemoveLocked(alarm);
    alarm->handler = nullptr;
}

void OSSetAlarmTag(OSAlarm* alarm, u32 tag) {
    Guard guard;
    alarm->tag = tag;
}

void OSCancelAlarms(u32 tag) {
    if (tag == 0) {
        return; // as on the GameCube: tag 0 means "no tag"
    }
    Guard guard;
    AlarmState& state = State();
    for (auto it = state.queue.begin(); it != state.queue.end();) {
        OSAlarm* alarm = it->second;
        if (alarm->tag == tag) {
            state.pending.erase(alarm);
            it = state.queue.erase(it);
            alarm->handler = nullptr;
        } else {
            ++it;
        }
    }
}

} // extern "C"
