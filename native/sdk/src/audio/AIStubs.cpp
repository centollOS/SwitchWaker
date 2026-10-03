// cos_sdk: the AI (audio interface) library, silent (docs/NATIVE_PORT_PHASE2_3.md, step 2.6f).
// Aurora has none. Real audio output is phase 5 (JAudio over the host); until then the AI is a
// register model that plays nothing:
// - Every setting the game makes is kept and read back as the console's registers would give it:
//   DSP sample rate, stream sample rate, stream play state, stream volumes, stream trigger, the
//   DMA source address and length, the DMA enable bit, and both callbacks.
// - No interrupt is ever raised: the DMA callback (AIRegisterDMACallback) and the stream callback
//   (AIRegisterStreamCallback) are kept but never called, AIGetDMABytesLeft is always 0 (as when
//   a DMA block has finished) and AIGetStreamSampleCount stays 0 (no disc stream plays).
//   JAudio's audio thread (JASAudioThread) therefore waits on its message queue for a DMA tick
//   that never comes; it blocks, it does not spin. AIInit logs this once.
// - AIInitDMA keeps the full 64-bit address (Aurora declares it with a uintptr_t under
//   TARGET_PC). AIGetDMAStartAddr returns a u32 as on the console, so it aborts when the address
//   does not fit instead of handing back a truncated pointer.
// - The SDK's ASSERT that the DMA length is a multiple of 32 bytes aborts here too: on the
//   console the low 5 bits are dropped (the length register counts 32-byte blocks).
// - AISetDSPSampleRate, AISetStreamSampleRate and __AI_set_stream_sample_rate follow the SDK's
//   rules (decomp src/dolphin/ai/ai.c): only rate 1 (48 kHz) is accepted for streams, AIInit sets
//   the stream to 48 kHz and the DSP to 32 kHz, and before AIInit AIGetDSPSampleRate reads 1
//   (48 kHz), the register's reset value. The game's d_a_movie_player reads it to pick 32 or 48 kHz.
// All state is guarded by the OS lock (interrupts disabled), as the SDK disables interrupts.
//
// Provenance: adapted from Dusklight src/dusk/stubs.cpp:866-900 (CC0, ref/dusklight), where
// AIGetDSPSampleRate returns 48000, AIRegisterDMACallback returns its argument, and the other AI
// functions only log "is a stub". Changed: the register model above, the AI_SAMPLERATE_* encoding
// (0 = 32 kHz, 1 = 48 kHz) instead of 48000, AIRegisterDMACallback returns the previous callback
// as the SDK does, and the rest of Aurora's <dolphin/ai.h> plus the SDK-internal
// __AI_set_stream_sample_rate (declared by the dolphin/ai/ai.h forwarder) are defined.
#include "../os/os_internal.h"

#include <dolphin/ai.h>

#include <cstdint>

using namespace cos_sdk::os;

namespace {

// All guarded by Lock().
struct AIState {
    bool init = false;
    AIDCallback dmaCallback = nullptr;
    AISCallback streamCallback = nullptr;
    uintptr_t dmaStart = 0;
    u32 dmaLength = 0;
    bool dmaEnabled = false;
    u32 dspRate = AI_SAMPLERATE_48KHZ; // reset value of AICR bit 6 (0) reads as 48 kHz
    u32 streamRate = AI_SAMPLERATE_32KHZ;
    u32 playState = AI_STREAM_STOP;
    u32 trigger = 0;
    u8 volLeft = 0;
    u8 volRight = 0;
};

AIState sAI;

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
    sAI.dmaEnabled = true;
}

void AIStopDMA(void) {
    Guard guard;
    sAI.dmaEnabled = false;
}

u32 AIGetDMABytesLeft(void) {
    return 0; // no DMA ever runs: every block reads as finished
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
    return 0; // no disc stream ever plays
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
    (void)stack; // the SDK switches to this stack for the DMA callback; none is ever called
    Guard guard;
    if (sAI.init) {
        return;
    }
    COS_SDK_LOG_ONCE("AIInit: audio output is not emulated yet (phase 5): the AI plays nothing "
                     "and its DMA and stream callbacks are never called");
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
}

void AIReset(void) {
    Guard guard;
    sAI.init = false; // as the SDK: the next AIInit starts again
}

} // extern "C"
