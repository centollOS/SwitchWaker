// SPDX-License-Identifier: GPL-2.0-or-later
// cos_dsp_hle: Dolphin's high-level DSP (DSPHLE, the JAudio ucode for the game) as the
// GameCube DSP behind cos_sdk's DSP library (native/sdk/src/audio/DSP.cpp). Step 5.A of
// docs/NATIVE_PORT_PHASE4_6.md, decision H6 (B).
//
// The interface is the DSP's register file as the CPU sees it: the control register and the two
// mailboxes. Dolphin's HLE answers synchronously: a mail written to the DSP is handled before
// cos_dsp_hle_write_cpu_mail returns, and any mails it sends back are queued for the CPU at once.
// When the DSP raises its interrupt, `interrupt` is called from inside the call that raised it.
//
// Not thread-safe: the caller serialises every call (cos_sdk holds one mutex around them). The
// callbacks run inside those calls and must not call back into this interface.
//
// Dolphin is GPLv2+; this repository is GPLv3 (decision H6).
#ifndef COS_DSP_HLE_H
#define COS_DSP_HLE_H

#include <cstdint>

namespace cos_dsp_hle {

struct Host {
    // Host pointer of `size` bytes of main memory at the physical address `address`, or null.
    std::uint8_t* (*guestPointer)(std::uint32_t address, std::uint32_t size) = nullptr;
    // ARAM: a host buffer whose size is a power of two.
    std::uint8_t* aram = nullptr;
    std::uint32_t aramSize = 0;
    // The CPU timebase (OSGetTime), for the control register's init-code bit.
    std::uint64_t (*timebase)() = nullptr;
    // The DSP raised its interrupt (DSPCR bit DSPINT).
    void (*interrupt)() = nullptr;
};

// Creates the DSP in its reset state: halted, running its boot ROM, which queues 0x8071FEED for
// the CPU. False if `host` is incomplete. May be called again to reset everything.
bool Initialize(const Host& host);
bool IsInitialized();

// Lets the ucode do its periodic work (the JAudio ucode's resume mail after a yield).
void Update();

// The control register (DSPCR) bits the DSP itself keeps: reset, halt, init, init code.
std::uint16_t ReadControl();
void WriteControl(std::uint16_t value);

// CPU -> DSP mailbox. Writing hands the mail to the ucode; ReadCpuMail reads it back, with the
// top bit clear once the DSP has taken it.
void WriteCpuMail(std::uint32_t mail);
std::uint32_t ReadCpuMail();

// DSP -> CPU mailbox, high half first (its top bit says a mail is there), then the low half,
// which pops the mail.
std::uint16_t ReadDspMailHigh();
std::uint16_t ReadDspMailLow();

} // namespace cos_dsp_hle

#endif // COS_DSP_HLE_H
