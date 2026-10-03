// cos_sdk_smoke: the "ai-tone" test of step 5.3 (docs/NATIVE_PORT_PHASE4_6.md): a sine wave in
// MEM1 goes through the AI's DMA, block by block as JAudio feeds it (the DMA callback sets the
// next block), and the WAV dump (COS_AUDIO_DUMP) equals the input: the same frames in host order,
// left/right, at 32 kHz, after the silence the engine plays before AIStartDMA. SDL's output goes
// to the dummy driver (the runner sets SDL_AUDIO_DRIVER=dummy).
//
// With COS_AUDIO_DUMP unset the test dumps to a temporary file and removes it.
#include "smoke.h"

#include <dolphin/ai/ai.h>
#include <dolphin/os.h>

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr u32 kBlockBytes = 0x280; // JAudio's DAC block at 32 kHz: 160 frames, 5 ms
constexpr u32 kBlockFrames = kBlockBytes / 4;
constexpr u32 kBlocks = 24;
constexpr u32 kFrames = kBlocks * kBlockFrames;

u8* sTone = nullptr; // kBlocks blocks in MEM1
std::atomic<u32> sInterrupts{0};

u32 BlockAddress(u32 block) {
    return OSCachedToPhysical(sTone + block * kBlockBytes);
}

// The AI DMA interrupt: the block after the one that just started goes into the registers (the
// last block stays there once the tone has been fed).
void ToneCallback() {
    const u32 n = sInterrupts.fetch_add(1) + 1; // block n has just started
    AIInitDMA(BlockAddress(n + 1 < kBlocks ? n + 1 : kBlocks - 1), kBlockBytes);
}

s16 Left(u32 i) {
    return s16(std::lround(12000.0 * std::sin(2.0 * M_PI * 1000.0 * i / 32000.0)));
}
s16 Right(u32 i) {
    return s16(std::lround(9000.0 * std::cos(2.0 * M_PI * 440.0 * i / 32000.0)));
}

u32 LE32(const u8* p) {
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}
u16 LE16(const u8* p) {
    return u16(p[0] | p[1] << 8);
}

} // namespace

// Registered by hand: the test's name has a hyphen, as step 5.3 names it.
static bool AiToneTest();
[[maybe_unused]] static const int sAiToneReg = ::cos_smoke::Register("ai-tone", &AiToneTest);

static bool AiToneTest() {
    OSInit();

    std::string dumpPath;
    bool ownDump = false;
    if (const char* env = std::getenv("COS_AUDIO_DUMP"); env != nullptr && env[0] != '\0') {
        dumpPath = env;
    } else {
        const char* tmp = std::getenv("TMPDIR");
        dumpPath = std::string(tmp != nullptr ? tmp : "/tmp") + "/cos_ai_tone_" +
                   std::to_string(getpid()) + ".wav";
        ownDump = true;
        setenv("COS_AUDIO_DUMP", dumpPath.c_str(), 1);
    }

    // A fresh AIInit (any earlier test's AI state is reset) starts the dump.
    AIReset();
    AIInit(nullptr);
    if (ownDump) {
        unsetenv("COS_AUDIO_DUMP"); // AIInit has read it; later tests get no dump
    }
    COS_SMOKE_CHECK(AIGetDSPSampleRate() == AI_SAMPLERATE_32KHZ);

    // The tone in MEM1 as the console's DMA reads it: big-endian s16, right channel first. No
    // frame is (0, 0), so the dump's leading silence ends exactly where the tone starts.
    if (sTone == nullptr) {
        sTone = static_cast<u8*>(OSAllocFromArenaLo(kBlocks * kBlockBytes, 32));
    }
    COS_SMOKE_CHECK(sTone != nullptr);
    for (u32 i = 0; i < kFrames; i++) {
        const u16 r = u16(Right(i));
        const u16 l = u16(Left(i));
        COS_SMOKE_CHECK(r != 0 || l != 0);
        u8* f = sTone + i * 4;
        f[0] = u8(r >> 8);
        f[1] = u8(r);
        f[2] = u8(l >> 8);
        f[3] = u8(l);
    }

    // As JAudio: the first block, start, then the next block in the registers; each interrupt
    // sets the one after.
    sInterrupts = 0;
    AIRegisterDMACallback(ToneCallback);
    AIInitDMA(BlockAddress(0), kBlockBytes);
    const auto start = std::chrono::steady_clock::now();
    AIStartDMA();
    AIInitDMA(BlockAddress(1), kBlockBytes);
    // kBlocks interrupts: the last block has been played to its end.
    while (sInterrupts.load() < kBlocks) {
        COS_SMOKE_CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    AIStopDMA();
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    AIRegisterDMACallback(nullptr);
    // Paced by real time: 24 blocks of 5 ms take at least ~120 ms (a little slack for the clock
    // thread's 1 ms ticks).
    COS_SMOKE_CHECK(seconds > 0.1);
    // Let the clock thread write what it played up to the stop.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // The dump: a valid 16-bit stereo 32 kHz WAV whose data, after the leading silence, starts
    // with exactly the tone.
    FILE* f = std::fopen(dumpPath.c_str(), "rb");
    COS_SMOKE_CHECK(f != nullptr);
    std::vector<u8> wav;
    u8 buf[65536];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        wav.insert(wav.end(), buf, buf + got);
    }
    std::fclose(f);
    if (ownDump) {
        std::remove(dumpPath.c_str());
    }
    COS_SMOKE_CHECK(wav.size() >= 44);
    COS_SMOKE_CHECK(std::memcmp(wav.data(), "RIFF", 4) == 0);
    COS_SMOKE_CHECK(std::memcmp(wav.data() + 8, "WAVEfmt ", 8) == 0);
    COS_SMOKE_CHECK(LE16(wav.data() + 20) == 1 && LE16(wav.data() + 22) == 2);
    COS_SMOKE_CHECK(LE32(wav.data() + 24) == 32000 && LE16(wav.data() + 34) == 16);
    COS_SMOKE_CHECK(std::memcmp(wav.data() + 36, "data", 4) == 0);
    const u32 dataBytes = LE32(wav.data() + 40);
    COS_SMOKE_CHECK(LE32(wav.data() + 4) == 36 + dataBytes);
    COS_SMOKE_CHECK(44 + size_t(dataBytes) <= wav.size());
    const u32 dumpFrames = dataBytes / 4;
    const u8* data = wav.data() + 44;
    u32 first = 0;
    while (first < dumpFrames && LE32(data + first * 4) == 0) {
        first++;
    }
    COS_SMOKE_CHECK(first + kFrames <= dumpFrames);
    for (u32 i = 0; i < kFrames; i++) {
        const u8* p = data + (first + i) * 4;
        if (s16(LE16(p)) != Left(i) || s16(LE16(p + 2)) != Right(i)) {
            std::fprintf(stderr, "ai-tone: dump frame %u is (%d, %d), the tone's is (%d, %d)\n", i,
                         s16(LE16(p)), s16(LE16(p + 2)), Left(i), Right(i));
            return false;
        }
    }
    std::printf("ai-tone: %u frames (%u blocks) through the AI DMA in %.3f s, dump equals the "
                "input\n",
                kFrames, kBlocks, seconds);

    // Leave the AI as the next test expects it: before AIInit the DSP rate reads 48 kHz.
    AISetDSPSampleRate(AI_SAMPLERATE_48KHZ);
    AIReset();
    return true;
}
