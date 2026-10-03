// SPDX-License-Identifier: GPL-2.0-or-later
// cos_dsp_hle (cos_dsp_hle.h): Dolphin's DSPHLE behind the DSP register interface cos_sdk uses.
//
// Provenance: adapted from this repository's runtime/host/src/dsp_hle_backend.cpp (the
// translated build's HLE backend, GPLv2+ like the Dolphin code it links) and
// runtime/host/src/dsp_adapter_donor_stubs.cpp (Core::System, CoreTiming and SystemTimers
// stubs). Dolphin's HLE reaches the rest of the emulator through Core::System; the few services
// it uses are implemented here over the Host callbacks: main-memory pointers (MEM1 by physical
// address), the ARAM buffer, the timebase and the DSP interrupt. Everything else it can reach
// (config, files, analytics) is inert. Changed from the original:
// - The interrupt is delivered at once (Host::interrupt) instead of being latched until the next
//   run_cycles: cos_sdk keeps the DSPCR interrupt bit itself and clears it when the handler
//   acknowledges, before the handler reads the mails, so a delivery during a mail read is kept.
// - No save states and no BLUEWAKE_* environment switches; an interface of plain functions.
// - The DSPHLE object is never destroyed at exit (host threads may still use it then).

#include "cos_dsp_hle.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSPEmulator.h"
#include "Core/DolphinAnalytics.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

namespace {

cos_dsp_hle::Host g_host;
// Never destroyed at exit: cos_sdk's DSP interrupt thread and the game's audio thread are detached
// and may still reach the DSP while the process runs its static destructors.
DSP::HLE::DSPHLE* g_hle = nullptr;

template <typename T>
T& inert_object() {
    alignas(T) static std::byte storage[sizeof(T)]{};
    return *reinterpret_cast<T*>(storage);
}

u8* guest_pointer(u32 address, size_t size) {
    if (g_host.guestPointer == nullptr || size > 0xFFFFFFFFu) {
        return nullptr;
    }
    return g_host.guestPointer(address, static_cast<u32>(size));
}

} // namespace

// ---- Core::System and the managers DSPHLE reaches through it ---------------------------------

namespace Core {
struct System::Impl {};

System::System() = default;
System::~System() = default;

DSP::DSPManager& System::GetDSP() const {
    return inert_object<DSP::DSPManager>();
}

Memory::MemoryManager& System::GetMemory() const {
    return inert_object<Memory::MemoryManager>();
}

CoreTiming::CoreTimingManager& System::GetCoreTiming() const {
    return inert_object<CoreTiming::CoreTimingManager>();
}

SystemTimers::SystemTimersManager& System::GetSystemTimers() const {
    return inert_object<SystemTimers::SystemTimersManager>();
}

void DisplayMessage(std::string message, int) {
    std::fprintf(stderr, "[dsp-hle] %s\n", message.c_str());
}
} // namespace Core

namespace CoreTiming {
void CoreTimingManager::ForceExceptionCheck(s64) {}
} // namespace CoreTiming

namespace SystemTimers {
u32 SystemTimersManager::GetTicksPerSecond() const {
    return 486000000u;
}

u64 SystemTimersManager::GetFakeTimeBase() const {
    return g_host.timebase != nullptr ? g_host.timebase() : 0;
}
} // namespace SystemTimers

namespace DSP {
void DSPManager::GenerateDSPInterruptFromDSPEmu(DSPInterruptType, int) {
    if (g_host.interrupt != nullptr) {
        g_host.interrupt();
    }
}

u8* DSPManager::GetARAMPtr() const {
    return g_host.aram;
}

u32 DSPManager::GetARAMSize() const {
    return g_host.aramSize;
}

u8 DSPManager::ReadARAM(u32 address) const {
    return g_host.aram != nullptr ? g_host.aram[address & (g_host.aramSize - 1)] : 0;
}

void DSPManager::WriteARAM(u8 value, u32 address) {
    if (g_host.aram != nullptr) {
        g_host.aram[address & (g_host.aramSize - 1)] = value;
    }
}
} // namespace DSP

namespace Memory {
u8* MemoryManager::GetPointerForRange(u32 address, size_t size) const {
    return guest_pointer(address, size);
}

void MemoryManager::CopyFromEmu(void* data, u32 address, size_t size) const {
    if (const u8* p = guest_pointer(address, size)) {
        std::memcpy(data, p, size);
    }
}

void MemoryManager::CopyToEmu(u32 address, const void* data, size_t size) {
    if (u8* p = guest_pointer(address, size)) {
        std::memcpy(p, data, size);
    }
}

void MemoryManager::Memset(u32 address, u8 value, size_t size) {
    if (u8* p = guest_pointer(address, size)) {
        std::memset(p, value, size);
    }
}

u8 MemoryManager::Read_U8(u32 address) const {
    const u8* p = guest_pointer(address, 1);
    return p != nullptr ? p[0] : 0;
}

u16 MemoryManager::Read_U16(u32 address) const {
    const u8* p = guest_pointer(address, 2);
    return p != nullptr ? static_cast<u16>((p[0] << 8) | p[1]) : 0;
}

u32 MemoryManager::Read_U32(u32 address) const {
    const u8* p = guest_pointer(address, 4);
    return p != nullptr ? (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3] : 0;
}

u32 MemoryManager::Read_U32_Swap(u32 address) const {
    return Common::swap32(Read_U32(address));
}

void MemoryManager::Write_U8(u8 value, u32 address) {
    if (u8* p = guest_pointer(address, 1)) {
        p[0] = value;
    }
}

void MemoryManager::Write_U16(u16 value, u32 address) {
    if (u8* p = guest_pointer(address, 2)) {
        p[0] = static_cast<u8>(value >> 8);
        p[1] = static_cast<u8>(value);
    }
}

void MemoryManager::Write_U32(u32 value, u32 address) {
    if (u8* p = guest_pointer(address, 4)) {
        p[0] = static_cast<u8>(value >> 24);
        p[1] = static_cast<u8>(value >> 16);
        p[2] = static_cast<u8>(value >> 8);
        p[3] = static_cast<u8>(value);
    }
}
} // namespace Memory

DSPEmulator::~DSPEmulator() = default;

DolphinAnalytics& DolphinAnalytics::Instance() {
    return inert_object<DolphinAnalytics>();
}

void DolphinAnalytics::ReportGameQuirk(GameQuirk) {}

namespace Config {
const Info<bool> MAIN_DUMP_UCODE{{System::Main, "DSP", "DumpUCode"}, false};
} // namespace Config

namespace DSP {
// Ucode dumping (Config MAIN_DUMP_UCODE, always off here) without DSPCodeUtil's disassembler.
bool DumpDSPCode(const u8*, size_t, u32) {
    return false;
}
} // namespace DSP

// ---- cos_dsp_hle ------------------------------------------------------------------------------

namespace cos_dsp_hle {

bool Initialize(const Host& host) {
    if (host.guestPointer == nullptr || host.aram == nullptr || host.aramSize == 0 ||
        (host.aramSize & (host.aramSize - 1)) != 0 || host.interrupt == nullptr) {
        return false;
    }
    if (g_hle != nullptr) {
        g_hle->Shutdown();
        delete g_hle;
        g_hle = nullptr;
    }
    g_host = host;
    g_hle = new DSP::HLE::DSPHLE(Core::System::GetInstance());
    return g_hle->Initialize(false, false);
}

bool IsInitialized() {
    return g_hle != nullptr;
}

void Update() {
    g_hle->DSP_Update(0);
}

std::uint16_t ReadControl() {
    return g_hle->DSP_ReadControlRegister();
}

void WriteControl(std::uint16_t value) {
    g_hle->DSP_WriteControlRegister(value);
}

void WriteCpuMail(std::uint32_t mail) {
    g_hle->DSP_WriteMailBoxHigh(true, static_cast<u16>(mail >> 16));
    g_hle->DSP_WriteMailBoxLow(true, static_cast<u16>(mail));
}

std::uint32_t ReadCpuMail() {
    return (u32(g_hle->DSP_ReadMailBoxHigh(true)) << 16) | g_hle->DSP_ReadMailBoxLow(true);
}

std::uint16_t ReadDspMailHigh() {
    return g_hle->DSP_ReadMailBoxHigh(false);
}

std::uint16_t ReadDspMailLow() {
    return g_hle->DSP_ReadMailBoxLow(false);
}

} // namespace cos_dsp_hle
