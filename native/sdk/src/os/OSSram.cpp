// cos_sdk: SRAM, the settings stored in it, and the real-time clock
// (docs/NATIVE_PORT_PHASE2_3.md, step 2.6b).
//
// On the GameCube the RTC chip holds 64 bytes of battery-backed SRAM (OSSram + OSSramEx): sound
// mode, video mode, progressive scan, EuRGB60, language, wireless pad IDs, ... The SDK keeps a
// copy, reads it over EXI at boot and writes it back on commit. Here the copy is all there is:
// it starts from defaults in every process (stereo, NTSC, interlaced, English) and a commit only
// recomputes the checksums. Persisting it is game-glue work for later (settings, phase 6).
//
// Provenance: Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight) returns constants (OSGetSoundMode
// returns 2, OSSetSoundMode and OSSetProgressiveMode do nothing, OSGetProgressiveMode returns 0).
// Changed here: the SDK's own model (lock/unlock with interrupts disabled, the flag bits, the
// checksum loop) over an in-memory SRAM, so a setting the game writes reads back, and
// OSGetSoundMode returns OS_SOUND_MODE_MONO or OS_SOUND_MODE_STEREO as on the console (the game's
// d_save.cpp and m_Do_audio.cpp only know 0 and 1).
#include "os_internal.h"

#include "cos_sdk/sram.h"

#include <cstddef>
#include <cstring>

using namespace cos_sdk::os;

namespace {

// OSSram::flags bits (the SDK's).
constexpr u8 kFlagVideoMode = 0x03;   // OS_VIDEO_MODE_*
constexpr u8 kFlagStereo = 0x04;      // sound mode
constexpr u8 kFlagForceMenu = 0x40;   // OSResetSystem(OS_RESET_HOTRESET, ..., TRUE)
constexpr u8 kFlagProgressive = 0x80; // progressive scan
// OSSram::ntd bits.
constexpr u8 kNtdEuRgb60 = 0x40;

struct Sram {
    OSSram sram;
    OSSramEx ex;
};
static_assert(sizeof(OSSram) == 20, "OSSram is 20 bytes on the console");

// Guarded by "interrupts disabled" (Lock()): LockSram keeps them disabled while the SRAM is locked.
Sram sSram;
bool sInitialized = false;
bool sLocked = false;
BOOL sEnabled = FALSE; // the interrupt level to restore at unlock
BOOL sSync = TRUE;

s64 sRtcOffset = 0; // seconds added by __OSSetRTC; guarded by Lock()

void ComputeChecksums(OSSram* sram) {
    // As the SDK: the u16 words from counterBias to the end of OSSram.
    u16 sum = 0;
    u16 sumInv = 0;
    const auto* bytes = reinterpret_cast<const u8*>(sram);
    for (std::size_t off = offsetof(OSSram, counterBias); off + 2 <= sizeof(OSSram); off += 2) {
        u16 word;
        std::memcpy(&word, bytes + off, sizeof(word));
        sum = static_cast<u16>(sum + word);
        sumInv = static_cast<u16>(sumInv + static_cast<u16>(~word));
    }
    sram->checkSum = sum;
    sram->checkSumInv = sumInv;
}

void InitDefaultsLocked() {
    if (sInitialized) {
        return;
    }
    sInitialized = true;
    std::memset(&sSram, 0, sizeof(sSram));
    sSram.sram.language = OS_LANGUAGE_ENGLISH;
    sSram.sram.flags = kFlagStereo; // video mode NTSC (0), interlaced
    ComputeChecksums(&sSram.sram);
}

void* LockSram(std::size_t offset) {
    const BOOL enabled = OSDisableInterrupts();
    InitDefaultsLocked();
    if (sLocked) {
        OSRestoreInterrupts(enabled);
        return nullptr;
    }
    sEnabled = enabled;
    sLocked = true;
    return reinterpret_cast<u8*>(&sSram) + offset;
}

BOOL UnlockSram(BOOL commit, std::size_t offset) {
    if (!sLocked) {
        Fatal("__OSUnlockSram%s without a matching lock", offset == 0 ? "" : "Ex");
    }
    if (commit) {
        if (offset == 0) {
            OSSram* sram = &sSram.sram;
            if ((sram->flags & kFlagVideoMode) > 2) {
                sram->flags &= static_cast<u8>(~kFlagVideoMode);
            }
            ComputeChecksums(sram);
        }
        sSync = TRUE; // "written back": the in-memory copy is the SRAM
    }
    sLocked = false;
    const BOOL sync = sSync;
    OSRestoreInterrupts(sEnabled);
    return sync;
}

// The u8/u16/u32 field accessors below follow the SDK's OSRtc.c.
template <class T, class F>
T ReadSram(F read) {
    OSSram* sram = __OSLockSram();
    if (sram == nullptr) {
        Fatal("SRAM read while the calling thread holds the SRAM lock");
    }
    const T value = read(sram);
    __OSUnlockSram(FALSE);
    return value;
}

// Sets `mask` bits of the byte `field(sram)` to `value` (already shifted and masked).
template <class F>
void WriteSramBits(F field, u8 mask, u8 value) {
    OSSram* sram = __OSLockSram();
    if (sram == nullptr) {
        Fatal("SRAM write while the calling thread holds the SRAM lock");
    }
    u8& byte = field(sram);
    if ((byte & mask) == value) {
        __OSUnlockSram(FALSE);
        return;
    }
    byte = static_cast<u8>((byte & ~mask) | value);
    __OSUnlockSram(TRUE);
}

OSSramEx* LockSramExOrDie() {
    OSSramEx* ex = __OSLockSramEx();
    if (ex == nullptr) {
        Fatal("SRAM access while the calling thread holds the SRAM lock");
    }
    return ex;
}

} // namespace

namespace cos_sdk::os {

void SramSetForceMenu() {
    WriteSramBits([](OSSram* s) -> u8& { return s->flags; }, kFlagForceMenu, kFlagForceMenu);
}

} // namespace cos_sdk::os

extern "C" {

OSSram* __OSLockSram(void) {
    return static_cast<OSSram*>(LockSram(0));
}

OSSramEx* __OSLockSramEx(void) {
    return static_cast<OSSramEx*>(LockSram(offsetof(Sram, ex)));
}

BOOL __OSUnlockSram(BOOL commit) {
    return UnlockSram(commit, 0);
}

BOOL __OSUnlockSramEx(BOOL commit) {
    return UnlockSram(commit, offsetof(Sram, ex));
}

BOOL __OSSyncSram(void) {
    Guard guard;
    return sSync;
}

BOOL __OSCheckSram(void) {
    OSSram* sram = __OSLockSram();
    if (sram == nullptr) {
        Fatal("__OSCheckSram while the calling thread holds the SRAM lock");
    }
    OSSram copy = *sram;
    ComputeChecksums(&copy);
    const BOOL ok = copy.checkSum == sram->checkSum && copy.checkSumInv == sram->checkSumInv;
    __OSUnlockSram(FALSE);
    return ok;
}

u32 OSGetSoundMode(void) {
    return ReadSram<u32>([](OSSram* s) {
        return (s->flags & kFlagStereo) ? OS_SOUND_MODE_STEREO : OS_SOUND_MODE_MONO;
    });
}

void OSSetSoundMode(u32 mode) {
    WriteSramBits([](OSSram* s) -> u8& { return s->flags; }, kFlagStereo,
                  static_cast<u8>((mode << 2) & kFlagStereo));
}

u32 OSGetVideoMode(void) {
    return ReadSram<u32>([](OSSram* s) { return static_cast<u32>(s->flags & kFlagVideoMode); });
}

void OSSetVideoMode(u32 mode) {
    WriteSramBits([](OSSram* s) -> u8& { return s->flags; }, kFlagVideoMode,
                  static_cast<u8>(mode & kFlagVideoMode));
}

u32 OSGetProgressiveMode(void) {
    return ReadSram<u32>([](OSSram* s) { return static_cast<u32>((s->flags & kFlagProgressive) >> 7); });
}

void OSSetProgressiveMode(u32 on) {
    WriteSramBits([](OSSram* s) -> u8& { return s->flags; }, kFlagProgressive,
                  static_cast<u8>((on << 7) & kFlagProgressive));
}

u32 OSGetEuRgb60Mode(void) {
    return ReadSram<u32>([](OSSram* s) { return static_cast<u32>((s->ntd >> 6) & 1); });
}

void OSSetEuRgb60Mode(u32 on) {
    WriteSramBits([](OSSram* s) -> u8& { return s->ntd; }, kNtdEuRgb60,
                  static_cast<u8>((on << 6) & kNtdEuRgb60));
}

u8 OSGetLanguage(void) {
    return ReadSram<u8>([](OSSram* s) { return s->language; });
}

void OSSetLanguage(u8 language) {
    OSSram* sram = __OSLockSram();
    if (sram == nullptr) {
        Fatal("OSSetLanguage while the calling thread holds the SRAM lock");
    }
    if (sram->language == language) {
        __OSUnlockSram(FALSE);
        return;
    }
    sram->language = language;
    __OSUnlockSram(TRUE);
}

u16 OSGetGbsMode(void) {
    OSSramEx* ex = LockSramExOrDie();
    const u16 mode = ex->gbs;
    __OSUnlockSramEx(FALSE);
    return mode;
}

void OSSetGbsMode(u16 mode) {
    OSSramEx* ex = LockSramExOrDie();
    if (ex->gbs == mode) {
        __OSUnlockSramEx(FALSE);
        return;
    }
    ex->gbs = mode;
    __OSUnlockSramEx(TRUE);
}

u16 OSGetWirelessID(s32 chan) {
    if (chan < 0 || chan >= 4) {
        Fatal("OSGetWirelessID: channel %d out of range", static_cast<int>(chan));
    }
    OSSramEx* ex = LockSramExOrDie();
    const u16 id = ex->wirelessPadID[chan];
    __OSUnlockSramEx(FALSE);
    return id;
}

void OSSetWirelessID(s32 chan, u16 id) {
    if (chan < 0 || chan >= 4) {
        Fatal("OSSetWirelessID: channel %d out of range", static_cast<int>(chan));
    }
    OSSramEx* ex = LockSramExOrDie();
    if (ex->wirelessPadID[chan] == id) {
        __OSUnlockSramEx(FALSE);
        return;
    }
    ex->wirelessPadID[chan] = id;
    __OSUnlockSramEx(TRUE);
}

BOOL __OSGetRTC(u32* rtc) {
    s64 offset;
    {
        Guard guard;
        offset = sRtcOffset;
    }
    const s64 seconds = static_cast<s64>(OSGetSystemTime() / OS_TIMER_CLOCK) + offset;
    if (rtc != nullptr) {
        *rtc = static_cast<u32>(seconds);
    }
    return TRUE;
}

BOOL __OSSetRTC(u32 rtc) {
    const s64 now = static_cast<s64>(OSGetSystemTime() / OS_TIMER_CLOCK);
    Guard guard;
    sRtcOffset = static_cast<s64>(rtc) - now;
    Log("__OSSetRTC(%u): the host clock is not changed; later __OSGetRTC calls add %lld s",
        static_cast<unsigned>(rtc), static_cast<long long>(sRtcOffset));
    return TRUE;
}

} // extern "C"
