// cos_sdk: the PowerPC special-purpose register accessors of PPCArch (docs/NATIVE_PORT_PHASE2_3.md,
// step 2.6b).
//
// There is no Gekko to program on the host. The registers are emulated as plain values, so what
// the game writes reads back, with these exceptions that keep the values honest:
// - MSR[EE] is the calling thread's interrupt state (cos_sdk's OS lock, see os_internal.h):
//   PPCMfmsr reports it and PPCMtmsr/PPCOrMsr/PPCAndMsr/PPCAndCMsr change it through
//   OSDisableInterrupts/OSEnableInterrupts. MSR and FPSCR are per thread, as each thread's context
//   holds them on the console;
// - the decrementer counts down at the timer clock (OS_TIMER_CLOCK) from the last PPCMtdec;
// - HID2[LCE] reads as set: Aurora's locked cache (LCGetBase, LCStoreData, ...) is plain memory and
//   always usable, and the game's THP decoder (d_a_movie_player.cpp) refuses to run without it;
// - setting what the host cannot honour is logged once: FP exception enables (MSR[FE0/FE1],
//   FPSCR[VE..XE]), non-IEEE mode (FPSCR[NI]), performance-monitor counting (MMCR0/MMCR1).
//   Starting a locked-cache DMA by hand (DMA_L[T]) aborts: Aurora's LCLoadBlocks/LCStoreBlocks
//   copy directly, and a DMA that silently never happens would corrupt data;
// - PPCHalt aborts (the console stops there), and PPCSync/PPCEieio/ICSync are memory fences.
//
// Provenance: adapted from Dusklight src/dusk/stubs.cpp "PPC Arch" (CC0, ref/dusklight): PPCHalt
// aborts and PPCSync does nothing there; PPCMfhid2 and PPCMfmsr return 0 and PPCMtmsr is ignored
// (each with a stub log). Changed as described above; the rest of PPCArch is added.
#include "os_internal.h"

#include <dolphin/base/PPCArch.h>

#include <atomic>

using namespace cos_sdk::os;

namespace {

// MSR bits used here (the PowerPC 750CL manual's).
constexpr u32 kMsrEE = 0x00008000; // external interrupts enabled
constexpr u32 kMsrFP = 0x00002000;
constexpr u32 kMsrME = 0x00001000;
constexpr u32 kMsrFE0 = 0x00000800;
constexpr u32 kMsrFE1 = 0x00000100;
constexpr u32 kMsrIR = 0x00000020;
constexpr u32 kMsrDR = 0x00000010;
constexpr u32 kMsrRI = 0x00000002;
// HID0 bits: instruction and data caches enabled, speculative accesses disabled (SPD).
constexpr u32 kHid0ICE = 0x00008000;
constexpr u32 kHid0DCE = 0x00004000;
constexpr u32 kHid0SPD = 0x00000200;
// HID2 bits: paired-single load/store quantization, write-gather pipe, paired singles, locked
// cache.
constexpr u32 kHid2LSQE = 0x80000000;
constexpr u32 kHid2WPE = 0x40000000;
constexpr u32 kHid2PSE = 0x20000000;
constexpr u32 kHid2LCE = 0x10000000;
constexpr u32 kL2crL2E = 0x80000000;
constexpr u32 kDmaLTrigger = 0x00000002;
constexpr u32 kFpscrEnables = FPSCR_VE | FPSCR_OE | FPSCR_UE | FPSCR_ZE | FPSCR_XE;
// The Gekko's processor version register.
constexpr u32 kGekkoPvr = 0x00083214;

// MSR of a running game thread, without EE (which comes from the interrupt state).
constexpr u32 kMsrDefault = kMsrFP | kMsrME | kMsrIR | kMsrDR | kMsrRI;

thread_local u32 tMsr = kMsrDefault;
thread_local u32 tFpscr = 0;

std::atomic<u32> sHid0{kHid0ICE | kHid0DCE};
std::atomic<u32> sHid1{0};
std::atomic<u32> sHid2{kHid2LSQE | kHid2WPE | kHid2PSE | kHid2LCE};
std::atomic<u32> sL2cr{kL2crL2E};
std::atomic<u32> sWpar{0};
std::atomic<u32> sDmaU{0};
std::atomic<u32> sDmaL{0};
std::atomic<u32> sMmcr0{0};
std::atomic<u32> sMmcr1{0};
std::atomic<u32> sPmc[4] = {};
std::atomic<u32> sSia{0};

// The decrementer: its value at sDecSetTick.
std::atomic<u32> sDecValue{0xFFFFFFFF};
std::atomic<OSTick> sDecSetTick{0};

u32 ReadMsr() {
    return (tMsr & ~kMsrEE) | (tInterruptsDisabled ? 0 : kMsrEE);
}

void WriteMsr(u32 msr) {
    if ((msr & (kMsrFE0 | kMsrFE1)) != 0) {
        COS_SDK_LOG_ONCE("PPCMtmsr(0x%08x): floating-point exceptions cannot be enabled on the "
                         "host; they never trap", static_cast<unsigned>(msr));
    }
    tMsr = msr & ~kMsrEE;
    const bool enable = (msr & kMsrEE) != 0;
    if (enable && tInterruptsDisabled) {
        OSEnableInterrupts();
    } else if (!enable && !tInterruptsDisabled) {
        OSDisableInterrupts();
    }
}

void Store(std::atomic<u32>& reg, u32 value) {
    reg.store(value, std::memory_order_relaxed);
}

} // namespace

extern "C" {

// ---- MSR --------------------------------------------------------------------------------------

u32 PPCMfmsr() {
    return ReadMsr();
}

void PPCMtmsr(u32 newMSR) {
    WriteMsr(newMSR);
}

u32 PPCOrMsr(u32 value) {
    const u32 old = ReadMsr();
    WriteMsr(old | value);
    return old;
}

u32 PPCAndMsr(u32 value) {
    const u32 old = ReadMsr();
    WriteMsr(old & value);
    return old;
}

u32 PPCAndCMsr(u32 value) {
    const u32 old = ReadMsr();
    WriteMsr(old & ~value);
    return old;
}

// ---- HID0/1/2, L2CR, WPAR, PVR ----------------------------------------------------------------

u32 PPCMfhid0() {
    return sHid0.load(std::memory_order_relaxed);
}

void PPCMthid0(u32 newHID0) {
    Store(sHid0, newHID0);
}

u32 PPCMfhid1() {
    return sHid1.load(std::memory_order_relaxed);
}

u32 PPCMfhid2() {
    return sHid2.load(std::memory_order_relaxed);
}

void PPCMthid2(u32 newhid2) {
    Store(sHid2, newhid2);
}

u32 PPCMfl2cr() {
    return sL2cr.load(std::memory_order_relaxed);
}

void PPCMtl2cr(u32 newL2cr) {
    Store(sL2cr, newL2cr);
}

u32 PPCMfwpar() {
    return sWpar.load(std::memory_order_relaxed);
}

void PPCMtwpar(u32 newwpar) {
    Store(sWpar, newwpar);
}

u32 PPCMfpvr() {
    return kGekkoPvr;
}

void PPCEnableSpeculation() {
    sHid0.fetch_and(~kHid0SPD, std::memory_order_relaxed);
}

void PPCDisableSpeculation() {
    sHid0.fetch_or(kHid0SPD, std::memory_order_relaxed);
}

// ---- Decrementer ------------------------------------------------------------------------------

void PPCMtdec(u32 newDec) {
    sDecSetTick.store(OSGetTick(), std::memory_order_relaxed);
    sDecValue.store(newDec, std::memory_order_relaxed);
}

u32 PPCMfdec(void) {
    const OSTick elapsed = OSGetTick() - sDecSetTick.load(std::memory_order_relaxed);
    return sDecValue.load(std::memory_order_relaxed) - static_cast<u32>(elapsed);
}

// ---- FPSCR ------------------------------------------------------------------------------------

u32 PPCMffpscr() {
    return tFpscr;
}

void PPCMtfpscr(u32 newFPSCR) {
    if ((newFPSCR & kFpscrEnables) != 0) {
        COS_SDK_LOG_ONCE("PPCMtfpscr(0x%08x): floating-point exception enables have no effect on "
                         "the host", static_cast<unsigned>(newFPSCR));
    }
    if ((newFPSCR & FPSCR_NI) != 0) {
        COS_SDK_LOG_ONCE("PPCMtfpscr(0x%08x): non-IEEE mode has no effect on the host (denormals "
                         "are kept)", static_cast<unsigned>(newFPSCR));
    }
    tFpscr = newFPSCR;
}

void PPCSetFpIEEEMode() {
    tFpscr &= ~FPSCR_NI;
}

void PPCSetFpNonIEEEMode() {
    PPCMtfpscr(tFpscr | FPSCR_NI);
}

// ---- Performance monitor ----------------------------------------------------------------------

u32 PPCMfmmcr0() {
    return sMmcr0.load(std::memory_order_relaxed);
}

void PPCMtmmcr0(u32 newMmcr0) {
    if (newMmcr0 != 0) {
        COS_SDK_LOG_ONCE("PPCMtmmcr0(0x%08x): the performance counters do not count on the host",
                         static_cast<unsigned>(newMmcr0));
    }
    Store(sMmcr0, newMmcr0);
}

u32 PPCMfmmcr1() {
    return sMmcr1.load(std::memory_order_relaxed);
}

void PPCMtmmcr1(u32 newMmcr1) {
    if (newMmcr1 != 0) {
        COS_SDK_LOG_ONCE("PPCMtmmcr1(0x%08x): the performance counters do not count on the host",
                         static_cast<unsigned>(newMmcr1));
    }
    Store(sMmcr1, newMmcr1);
}

u32 PPCMfpmc1() {
    return sPmc[0].load(std::memory_order_relaxed);
}

u32 PPCMfpmc2(void) {
    return sPmc[1].load(std::memory_order_relaxed);
}

u32 PPCMfpmc3() {
    return sPmc[2].load(std::memory_order_relaxed);
}

u32 PPCMfpmc4() {
    return sPmc[3].load(std::memory_order_relaxed);
}

void PPCMtpmc1(u32 value) {
    Store(sPmc[0], value);
}

void PPCMtpmc2(u32 value) {
    Store(sPmc[1], value);
}

void PPCMtpmc3(u32 value) {
    Store(sPmc[2], value);
}

void PPCMtpmc4(u32 value) {
    Store(sPmc[3], value);
}

u32 PPCMfsia() {
    return sSia.load(std::memory_order_relaxed);
}

void PPCMtsia(u32 newSia) {
    Store(sSia, newSia);
}

// ---- Locked-cache DMA registers ---------------------------------------------------------------

u32 PPCMfdmaU() {
    return sDmaU.load(std::memory_order_relaxed);
}

u32 PPCMfdmaL() {
    // A DMA "completes" at once: the trigger bit always reads as clear.
    return sDmaL.load(std::memory_order_relaxed) & ~kDmaLTrigger;
}

void PPCMtdmaU(u32 newdmau) {
    Store(sDmaU, newdmau);
}

void PPCMtdmaL(u32 newdmal) {
    if ((newdmal & kDmaLTrigger) != 0) {
        Fatal("PPCMtdmaL(0x%08x): a locked-cache DMA started by hand cannot run on the host "
              "(use LCLoadBlocks/LCStoreBlocks)", static_cast<unsigned>(newdmal));
    }
    Store(sDmaL, newdmal);
}

// ---- Synchronisation and halt -----------------------------------------------------------------

void PPCSync() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void PPCEieio() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void PPCHalt() {
    Fatal("PPCHalt: the game halted the CPU");
}

} // extern "C"
