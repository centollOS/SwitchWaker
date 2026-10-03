// cos_sdk: the AI (audio interface) library, with its DMA played over SDL3 (step 5.3 of
// docs/NATIVE_PORT_PHASE4_6.md; it replaces the silent AIStubs.cpp of step 2.6f). Aurora has none.
//
// The console's audio DMA, as Dolphin models it (Source/Core/Core/HW/DSP.cpp, UpdateAudioDMA):
// - AIInitDMA writes the start address (a MEM1 physical address: JAudio passes
//   JASystem::Kernel::toPhysical, step 5.2) and the length (a multiple of 32 bytes) into
//   registers. AIStartDMA latches them into the DMA engine (no interrupt). The engine reads the
//   block 32 bytes at a time at the DSP sample rate (32 or 48 kHz, 4 bytes a sample frame); when
//   the block is done it latches the registers again and raises the AI DMA interrupt, whose
//   handler (here: the callback AIRegisterDMACallback set) sets the registers for the block after
//   the one that just started. JAudio's syncAudio only posts a message to its audio thread.
// - The samples are big-endian s16 stereo, right channel first (Dolphin's Mixer::PushSamples).
//   They are swapped to host order and reordered left/right for SDL.
// - AIGetDMABytesLeft reads the engine's remaining count as the SDK does (Dolphin's register
//   read: the 32-byte blocks left after the one in transfer).
//
// On the host a clock thread (the "AI DMA" thread) plays the engine: every millisecond it reads
// the sample frames that real time says are due, in 32-byte steps, from MEM1 (silence while the
// DMA is off, or for a range outside MEM1, logged once), and raises the interrupt at each block
// end by calling the DMA callback with the OS lock held and interrupts disabled, as the console's
// interrupt dispatcher does (the interrupt is not routed through DSPCR's AIDINT bit, which
// nothing on the host reads). Like the alarm and DSP interrupt threads it is "in interrupt
// context": the callback must not block. The lock is released between 32-byte steps, so the
// audio thread can answer the interrupt (AIInitDMA of the next block) before the next block end.
// The frames are then queued on an SDL3 audio stream (s16 stereo at the DSP rate; SDL converts
// to the device) and appended to the WAV dump.
// Why a clock and not an SDL pull callback: SDL asks for several blocks' worth at once, so a
// callback that ran the engine for each request would raise several interrupts back to back
// before the audio thread could set the next block, and would replay stale blocks. The engine is
// paced by host time, as Dolphin's 4 kHz event is by emulated time; SDL only plays what it gets
// (the queue is capped at 200 ms, extra frames are dropped and logged once).
//
// Environment:
// - SDL_AUDIO_DRIVER: SDL's driver; `dummy` for headless runs (cos_sdk_smoke and run.sh
//   default to it). If SDL's audio cannot start, the DMA still runs (and the dump is written);
//   this is logged once.
// - COS_AUDIO_DUMP=x.wav: every frame the DMA engine plays (silence included, from AIInit on) is
//   written to x.wav, 16-bit stereo at the DSP rate of AIInit. AIInit reads the variable; an
//   AIInit after AIReset starts a new dump (or ends it, if the variable is gone). The header's sizes are rewritten
//   after each write, so the file is valid while the process runs and after it is killed.
//
// COSAIGetOutputStats (cos_sdk/audio.h, step 5.5) reports the frames the engine played and the
// sum of their squares, for the harness's COS_SMOKE=title-audio level check.
//
// Other registers (stream sample rate, play state, volumes, trigger, sample count) are a register
// model as before: disc streaming (DTK, AIStreamSampleCount) is step 5.6. AIGetDMAStartAddr
// returns a u32 as on the console, so it aborts when the address does not fit instead of handing
// back a truncated pointer. The SDK's ASSERT that the DMA length is a multiple of 32 bytes
// aborts here too. AISetDSPSampleRate, AISetStreamSampleRate and __AI_set_stream_sample_rate
// follow the SDK's rules (decomp src/dolphin/ai/ai.c): only rate 1 (48 kHz) is accepted for
// streams, AIInit sets the stream to 48 kHz and the DSP to 32 kHz, and before AIInit
// AIGetDSPSampleRate reads 1 (48 kHz), the register's reset value. The game's d_a_movie_player reads
// it to pick 32 or 48 kHz. All register state is guarded by the OS lock (interrupts disabled), as
// the SDK disables interrupts.
//
// Provenance: the register model is adapted from Dusklight src/dusk/stubs.cpp:866-900 (CC0,
// ref/dusklight), where AIGetDSPSampleRate returns 48000, AIRegisterDMACallback returns its
// argument, and the other AI functions only log "is a stub" (changed: the register model above,
// the AI_SAMPLERATE_* encoding, AIRegisterDMACallback returns the previous callback). The SDL3
// output stream follows Dusklight's src/dusk/audio/DuskAudioSystem.cpp (CC0: SDL_Init of the
// audio subsystem and SDL_OpenAudioDeviceStream on the default playback device; changed: s16
// stereo at the DSP rate, frames pushed with SDL_PutAudioStreamData instead of rendered in a
// pull callback, see above). The DMA engine's latch/interrupt order, the 32-byte steps, the
// bytes-left read and the right/left order follow Dolphin (GPLv2+, ref/recompcore,
// Source/Core/Core/HW/DSP.cpp and AudioCommon/Mixer.cpp), read as a reference only.
#include "../os/os_internal.h"

#include <dolphin/ai.h>
#include <dolphin/os.h>
#include "cos_sdk/audio.h"

#include <aurora/aurora.h>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include <pthread.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace aurora {
// Aurora's configuration (lib/aurora.cpp); mem1Size is the size of the MEM1 block.
extern AuroraConfig g_config;
} // namespace aurora

using namespace cos_sdk::os;

namespace {

constexpr u32 kDmaStep = 32;      // bytes the engine reads at a time
constexpr u32 kFrameBytes = 4;    // one s16 stereo sample frame
constexpr u32 kStepFrames = kDmaStep / kFrameBytes;

// All guarded by Lock().
struct AIState {
    bool init = false;
    AIDCallback dmaCallback = nullptr;
    AISCallback streamCallback = nullptr;
    // The DMA registers (AIInitDMA) and the engine's latched copy (AIStartDMA, block ends).
    uintptr_t dmaStart = 0;
    u32 dmaLength = 0;
    bool dmaEnabled = false;
    uintptr_t curAddr = 0;
    u32 curBlocksLeft = 0; // 32-byte steps left in the latched block, the one in transfer included
    u32 dspRate = AI_SAMPLERATE_48KHZ; // reset value of AICR bit 6 (0) reads as 48 kHz
    u32 streamRate = AI_SAMPLERATE_32KHZ;
    u32 playState = AI_STREAM_STOP;
    u32 trigger = 0;
    u8 volLeft = 0;
    u8 volRight = 0;
    bool threadStarted = false;
    // COS_AUDIO_DUMP as AIInit read it; the thread (re)opens the dump when the count changes. A
    // plain array: the detached thread may still read it while exit runs static destructors.
    char dumpPath[1024] = {};
    u32 dumpInitCount = 0;
    // What the engine played since the process started (COSAIGetOutputStats): sample frames
    // (silence included) and the sum of the squares of both channels' samples.
    u64 outFrames = 0;
    u64 outSumSquares = 0;
};

AIState sAI;

u32 RateHz(u32 rate) {
    return rate == AI_SAMPLERATE_32KHZ ? 32000 : 48000;
}

// ---- the AI DMA thread (only it touches what follows) -----------------------------------------

OSThread sDmaThreadRecord;
HostThread sDmaThreadHost;

SDL_AudioStream* sStream = nullptr;
u32 sStreamHz = 0;
FILE* sDump = nullptr;
u32 sDumpHz = 0; // the rate in the dump's header (the DSP rate when the dump was opened)
u32 sDumpFrames = 0;

void PutLE32(u8* p, u32 v) {
    p[0] = u8(v);
    p[1] = u8(v >> 8);
    p[2] = u8(v >> 16);
    p[3] = u8(v >> 24);
}

void PutLE16(u8* p, u16 v) {
    p[0] = u8(v);
    p[1] = u8(v >> 8);
}

// Writes the 44-byte canonical WAV header for `frames` s16 stereo frames at `hz`.
void WriteWavHeader(FILE* f, u32 hz, u32 frames) {
    u8 h[44];
    const u32 dataBytes = frames * kFrameBytes;
    std::memcpy(h, "RIFF", 4);
    PutLE32(h + 4, 36 + dataBytes);
    std::memcpy(h + 8, "WAVEfmt ", 8);
    PutLE32(h + 16, 16);
    PutLE16(h + 20, 1); // PCM
    PutLE16(h + 22, 2); // stereo
    PutLE32(h + 24, hz);
    PutLE32(h + 28, hz * kFrameBytes);
    PutLE16(h + 32, kFrameBytes);
    PutLE16(h + 34, 16);
    std::memcpy(h + 36, "data", 4);
    PutLE32(h + 40, dataBytes);
    std::fseek(f, 0, SEEK_SET);
    std::fwrite(h, 1, sizeof(h), f);
}

// Closes the dump and opens `path` (none if empty) for the frames from now on.
void OpenDump(const std::string& path, u32 hz) {
    if (sDump != nullptr) {
        std::fclose(sDump);
        sDump = nullptr;
    }
    sDumpFrames = 0;
    if (path.empty()) {
        return;
    }
    sDump = std::fopen(path.c_str(), "wb");
    if (sDump == nullptr) {
        Log("AI: COS_AUDIO_DUMP: cannot open %s for writing; no dump", path.c_str());
        return;
    }
    sDumpHz = hz;
    WriteWavHeader(sDump, hz, 0);
    std::fflush(sDump);
    Log("AI: writing the audio DMA output to %s (16-bit stereo, %u Hz)", path.c_str(), hz);
}

void AppendDump(const std::vector<s16>& frames) {
    if (sDump == nullptr || frames.empty()) {
        return;
    }
    // WAV data is little-endian.
    std::vector<u8> bytes(frames.size() * 2);
    for (size_t i = 0; i < frames.size(); i++) {
        PutLE16(&bytes[i * 2], u16(frames[i]));
    }
    std::fseek(sDump, 0, SEEK_END);
    std::fwrite(bytes.data(), 1, bytes.size(), sDump);
    sDumpFrames += u32(frames.size() / 2);
    WriteWavHeader(sDump, sDumpHz, sDumpFrames);
    std::fflush(sDump);
}

void OpenOutput(u32 hz) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        Log("AI: SDL audio did not start (%s); the DMA runs without output", SDL_GetError());
        return;
    }
    const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, int(hz)};
    sStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (sStream == nullptr) {
        Log("AI: no SDL playback stream (%s); the DMA runs without output", SDL_GetError());
        return;
    }
    SDL_ResumeAudioStreamDevice(sStream);
    Log("AI: audio output over SDL3 (driver %s), 16-bit stereo, %u Hz",
        SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?", hz);
}

void PlayOutput(const std::vector<s16>& frames, u32 hz) {
    if (sStream == nullptr || frames.empty()) {
        return;
    }
    if (hz != sStreamHz) {
        const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, int(hz)};
        SDL_SetAudioStreamFormat(sStream, &spec, nullptr);
        sStreamHz = hz;
    }
    const int bytes = int(frames.size() * sizeof(s16));
    // At most 200 ms queued: past that the device plays slower than the clock (or not at all).
    if (SDL_GetAudioStreamQueued(sStream) + bytes > int(hz / 5 * kFrameBytes)) {
        COS_SDK_LOG_ONCE("AI: the SDL stream holds 200 ms already; frames are dropped (logged "
                         "once)");
        return;
    }
    SDL_PutAudioStreamData(sStream, frames.data(), bytes);
}

const u8* Mem1Range(uintptr_t address, u32 size) {
    const u64 mem1Size = aurora::g_config.mem1Size;
    if (OSBaseAddress != 0 && u64(address) + size <= mem1Size) {
        return static_cast<const u8*>(OSPhysicalToCached(u32(address)));
    }
    COS_SDK_LOG_ONCE("AI: DMA range 0x%llx+0x%x is outside MEM1 (0x%llx bytes); it plays as "
                     "silence (logged once)",
                     static_cast<unsigned long long>(address), size,
                     static_cast<unsigned long long>(mem1Size));
    return nullptr;
}

// One 32-byte step of the engine: appends 8 frames (left, right, host order) to `out`. Raises
// the interrupt at a block end. Needs Lock() (interrupts disabled).
void StepLocked(std::vector<s16>& out) {
    const size_t at = out.size();
    out.resize(at + kStepFrames * 2, 0);
    sAI.outFrames += kStepFrames;
    if (!sAI.dmaEnabled) {
        return;
    }
    if (const u8* p = Mem1Range(sAI.curAddr, kDmaStep)) {
        for (u32 i = 0; i < kStepFrames; i++) {
            const u8* f = p + i * kFrameBytes; // big-endian right, then left
            const s16 left = s16(u16(f[2] << 8 | f[3]));
            const s16 right = s16(u16(f[0] << 8 | f[1]));
            out[at + i * 2 + 0] = left;
            out[at + i * 2 + 1] = right;
            sAI.outSumSquares += u64(s32(left) * left) + u64(s32(right) * right);
        }
    }
    if (sAI.curBlocksLeft != 0) {
        sAI.curBlocksLeft--;
        sAI.curAddr += kDmaStep;
    }
    if (sAI.curBlocksLeft == 0) {
        sAI.curAddr = sAI.dmaStart;
        sAI.curBlocksLeft = sAI.dmaLength / kDmaStep;
        if (sAI.dmaCallback != nullptr) {
            sAI.dmaCallback();
        }
    }
}

void* DmaThreadMain(void*) {
    u32 hz;
    u32 dumpInitCount = 0; // AIInit's count the dump was opened for
    {
        std::lock_guard<std::mutex> os(Lock());
        tCurrent = &sDmaThreadRecord;
        tHost = &sDmaThreadHost;
        hz = RateHz(sAI.dspRate);
    }
    sStreamHz = hz;
    OpenOutput(hz);

    using Clock = std::chrono::steady_clock;
    auto base = Clock::now();
    u64 played = 0; // frames since `base`
    std::vector<s16> frames;
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        u32 rate;
        std::string newDump;
        bool reopenDump = false;
        {
            std::lock_guard<std::mutex> os(Lock());
            if (ShuttingDownLocked()) {
                return nullptr;
            }
            rate = RateHz(sAI.dspRate);
            if (sAI.dumpInitCount != dumpInitCount) {
                dumpInitCount = sAI.dumpInitCount;
                newDump = sAI.dumpPath;
                reopenDump = true;
            }
        }
        if (reopenDump) {
            OpenDump(newDump, rate);
        }
        const auto now = Clock::now();
        if (rate != hz) {
            hz = rate;
            base = now;
            played = 0;
        }
        const u64 due = u64(std::chrono::duration_cast<std::chrono::microseconds>(now - base)
                                .count()) * hz / 1000000;
        if (due - played > hz / 10) {
            // More than 100 ms behind (the process was stopped): skip ahead, as a console that
            // was paused would not replay what it missed.
            COS_SDK_LOG_ONCE("AI: the DMA clock fell more than 100 ms behind; it skips ahead "
                             "(logged once)");
            played = due - due % kStepFrames;
        }
        frames.clear();
        while (played + kStepFrames <= due) {
            std::unique_lock<std::mutex> os(Lock());
            tInterruptsDisabled = true; // the interrupt runs "in interrupt context"
            if (ShuttingDownLocked()) {
                tInterruptsDisabled = false;
                return nullptr;
            }
            StepLocked(frames);
            tInterruptsDisabled = false;
            played += kStepFrames;
        }
        PlayOutput(frames, hz);
        if (hz != sDumpHz && sDump != nullptr) {
            COS_SDK_LOG_ONCE("AI: the DSP rate changed to %u Hz; the dump keeps its header's %u "
                             "Hz (logged once)", hz, sDumpHz);
        }
        AppendDump(frames);
    }
}

// Starts the AI DMA thread. Needs Lock().
void StartDmaThreadLocked() {
    if (sAI.threadStarted) {
        return;
    }
    sAI.threadStarted = true;
    sDmaThreadHost.alarmThread = true; // interrupt context: must not block, Enable is ignored
    sDmaThreadRecord.state = OS_THREAD_STATE_RUNNING;
    sDmaThreadRecord.priority = sDmaThreadRecord.base = OS_PRIORITY_MIN;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t handle;
    const int err = pthread_create(&handle, &attr, &DmaThreadMain, nullptr);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        Fatal("AIInit: pthread_create of the AI DMA thread failed (%d: %s)", err,
              std::strerror(err));
    }
}

} // namespace

extern "C" {

AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    Guard guard;
    const AIDCallback old = sAI.dmaCallback;
    sAI.dmaCallback = callback;
    return old;
}

void AIInitDMA(uintptr_t start_addr, u32 length) {
    if ((length & 0x1F) != 0) {
        Fatal("AIInitDMA: length 0x%x is not a multiple of 32 bytes", length);
    }
    Guard guard;
    sAI.dmaStart = start_addr;
    sAI.dmaLength = length;
}

BOOL AIGetDMAEnableFlag(void) {
    Guard guard;
    return sAI.dmaEnabled ? TRUE : FALSE;
}

void AIStartDMA(void) {
    Guard guard;
    if (!sAI.dmaEnabled) {
        // The engine latches the registers; the first interrupt comes at the end of this block.
        sAI.curAddr = sAI.dmaStart;
        sAI.curBlocksLeft = sAI.dmaLength / kDmaStep;
    }
    sAI.dmaEnabled = true;
}

void AIStopDMA(void) {
    Guard guard;
    sAI.dmaEnabled = false;
}

u32 AIGetDMABytesLeft(void) {
    Guard guard;
    // As Dolphin's register read: the 32-byte steps after the one in transfer.
    return sAI.curBlocksLeft > 0 ? (sAI.curBlocksLeft - 1) * kDmaStep : 0;
}

u32 AIGetDMAStartAddr(void) {
    Guard guard;
    if (sAI.dmaStart > UINT32_MAX) {
        Fatal("AIGetDMAStartAddr: the DMA address %p does not fit the SDK's u32",
              reinterpret_cast<void*>(sAI.dmaStart));
    }
    return static_cast<u32>(sAI.dmaStart);
}

u32 AIGetDMALength(void) {
    Guard guard;
    return sAI.dmaLength;
}

BOOL AICheckInit(void) {
    Guard guard;
    return sAI.init ? TRUE : FALSE;
}

AISCallback AIRegisterStreamCallback(AISCallback callback) {
    Guard guard;
    const AISCallback old = sAI.streamCallback;
    sAI.streamCallback = callback;
    return old;
}

u32 AIGetStreamSampleCount(void) {
    return 0; // no disc stream plays yet (step 5.6)
}

void AIResetStreamSampleCount(void) {}

void AISetStreamTrigger(u32 trigger) {
    Guard guard;
    sAI.trigger = trigger;
}

u32 AIGetStreamTrigger(void) {
    Guard guard;
    return sAI.trigger;
}

void AISetStreamPlayState(u32 state) {
    Guard guard;
    sAI.playState = state & 1; // AICR bit 0
}

u32 AIGetStreamPlayState(void) {
    Guard guard;
    return sAI.playState;
}

void AISetDSPSampleRate(u32 rate) {
    Guard guard;
    // The SDK clears AICR bit 6 for any non-zero rate (48 kHz) and sets it for 0 (32 kHz).
    sAI.dspRate = rate == AI_SAMPLERATE_32KHZ ? AI_SAMPLERATE_32KHZ : AI_SAMPLERATE_48KHZ;
}

u32 AIGetDSPSampleRate(void) {
    Guard guard;
    return sAI.dspRate;
}

void __AI_set_stream_sample_rate(u32 rate) {
    Guard guard;
    sAI.streamRate = rate & 1; // AICR bit 1
}

void AISetStreamSampleRate(u32 rate) {
    // As the SDK: only 48 kHz streaming from disc is supported; other rates are ignored.
    if (rate == AI_SAMPLERATE_48KHZ) {
        __AI_set_stream_sample_rate(rate);
    }
}

u32 AIGetStreamSampleRate(void) {
    Guard guard;
    return sAI.streamRate;
}

void AISetStreamVolLeft(u8 vol) {
    Guard guard;
    sAI.volLeft = vol;
}

u8 AIGetStreamVolLeft(void) {
    Guard guard;
    return sAI.volLeft;
}

void AISetStreamVolRight(u8 vol) {
    Guard guard;
    sAI.volRight = vol;
}

u8 AIGetStreamVolRight(void) {
    Guard guard;
    return sAI.volRight;
}

void AIInit(u8* stack) {
    (void)stack; // the SDK switches to this stack for the DMA callback; host threads have their own
    Guard guard;
    if (sAI.init) {
        return;
    }
    // The SDK's AIInit: volumes 0, trigger 0, sample count reset, stream 48 kHz, DSP 32 kHz,
    // no callbacks.
    sAI.volLeft = 0;
    sAI.volRight = 0;
    sAI.trigger = 0;
    sAI.streamRate = AI_SAMPLERATE_48KHZ;
    sAI.dspRate = AI_SAMPLERATE_32KHZ;
    sAI.dmaCallback = nullptr;
    sAI.streamCallback = nullptr;
    sAI.init = true;
    // The dump starts again at each AIInit (the DMA thread opens it at its next tick).
    const char* dump = std::getenv("COS_AUDIO_DUMP");
    if (dump != nullptr && std::strlen(dump) >= sizeof(sAI.dumpPath)) {
        Fatal("AIInit: COS_AUDIO_DUMP is longer than %zu bytes", sizeof(sAI.dumpPath) - 1);
    }
    std::snprintf(sAI.dumpPath, sizeof(sAI.dumpPath), "%s", dump != nullptr ? dump : "");
    sAI.dumpInitCount++;
    // The DMA engine runs from now on (silence until AIStartDMA), as the console's does.
    StartDmaThreadLocked();
}

void COSAIGetOutputStats(u64* frames, u64* sumSquares) {
    Guard guard;
    *frames = sAI.outFrames;
    *sumSquares = sAI.outSumSquares;
}

void AIReset(void) {
    Guard guard;
    sAI.init = false; // as the SDK: the next AIInit starts again
}

} // extern "C"
