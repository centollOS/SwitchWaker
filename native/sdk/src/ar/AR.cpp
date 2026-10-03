// cos_sdk: the AR and ARQ functions Aurora's aurora_os lacks (docs/NATIVE_PORT_PHASE2_3.md,
// step 2.6e).
//
// Aurora's lib/dolphin/AR.cpp emulates ARAM as one host buffer of AuroraConfig.mem2Size bytes
// (allocated by ARInit; ARAM "addresses" are offsets into it, ARInit returns 0x4000) and has
// ARInit, ARAlloc, ARFree, ARCheckInit, ARGetSize, ARGetStorageAddress, ARQInit and
// ARQPostRequest (a memcpy whose callback runs before it returns). What is left is here:
//
// - ARStartDMA copies between main memory and ARAM at once, then calls the callback registered
//   with ARRegisterDMACallback with interrupts disabled (the OS lock of src/os), as the AR
//   interrupt handler does on the console when the DMA ends. ARGetDMAStatus is therefore always 0
//   ("no DMA in progress"), which is what code that polls it after ARStartDMA waits for.
//   - The ARAM side goes through ARGetStorageAddress(), the buffer Aurora's private aramToHost()
//     indexes, with the whole range checked against ARGetSize().
//   - The main-memory side is a u32 in the SDK's signature, so it cannot hold a 64-bit host
//     pointer. It is read as a MEM1 physical address (an offset into the MEM1 block Aurora's
//     OSInit allocates from AuroraConfig.mem1Size), the same convention as Aurora's
//     OSPhysicalToCached/OSCachedToPhysical. Callers convert with OSCachedToPhysical (phase 4
//     for game code that still passes `(u32)pointer`).
//   - Any address that does not fit (no MEM1, no ARAM, a range past the end, or a console address
//     such as 0x80xxxxxx) is a fatal error with the arguments in the message: on the host a
//     silently dropped or misdirected DMA would only show up much later as corrupt data.
//   - The console needs 32-byte aligned addresses and a length that is a multiple of 32 and
//     ignores the low bits. The copy here uses the exact values; a misaligned request is logged
//     once.
// - ARGetBaseAddress returns 0x4000, the start of the user area (what Aurora's ARInit returns
//   and what the SDK's ARGetBaseAddress returns), so JASSystemHeap's `ARGetBaseAddress()` and
//   JKRAram's `ARInit()` agree.
// - ARGetInternalSize is ARGetSize(): the host buffer is all "internal" ARAM, with no expansion.
// - ARClear clears the buffer as the SDK does: the whole internal ARAM, the user area from
//   0x4000, or (AR_CLEAR_EXPANSION) nothing, since there is no expansion ARAM.
// - ARQRemoveRequest, ARQRemoveOwnerRequest and ARQFlushQueue have nothing to remove: Aurora's
//   ARQPostRequest finishes every request before it returns, so the queue is always empty.
//   ARQSetChunkSize/ARQGetChunkSize keep the chunk size (rounded up to 32 as the SDK does) though
//   no request is ever split.
//
// Left out on purpose (not defined, so a caller fails to link instead of running on wrong state):
// ARReset, ARSetSize, ARQReset, ARQCheckInit, __ARGetInterruptStatus and __ARClearInterrupt.
// They read or reset Aurora's private AR state (its init flags), which cos_sdk cannot reach, and
// no game unit calls them.
//
// Provenance: not in Dusklight (Dusklight's game uses only ARQ, which Aurora covers). The SDK
// behaviour follows the decomp's src/dolphin/ar/ar.c and arq.c.
#include "../os/os_internal.h"

#include <dolphin/ar.h>
#include <dolphin/arq.h>
#include <dolphin/os.h>

#include <aurora/aurora.h>

#include <cstdint>
#include <cstring>

namespace aurora {
// Aurora's configuration (lib/aurora.cpp); mem1Size is the size of the MEM1 block.
extern AuroraConfig g_config;
} // namespace aurora

using namespace cos_sdk::os;

namespace {

constexpr u32 kAramUserBase = 0x4000; // ARAM_STACK_START in Aurora, the SDK's user-area start

ARCallback sDmaCallback = nullptr; // guarded by Lock()
u32 sArqChunkSize = ARQ_CHUNK_SIZE_DEFAULT; // guarded by Lock()

// Host address of `length` bytes of ARAM at `aramAddr`, or null if they are not all in the buffer.
u8* AramRange(u32 aramAddr, u32 length) {
    auto* base = static_cast<u8*>(ARGetStorageAddress());
    const u64 size = ARGetSize();
    if (base == nullptr || u64(aramAddr) + length > size) {
        return nullptr;
    }
    return base + aramAddr;
}

// Host address of `length` bytes of MEM1 at physical address `physAddr`, or null.
u8* Mem1Range(u32 physAddr, u32 length) {
    const u64 size = aurora::g_config.mem1Size;
    if (OSBaseAddress == 0 || u64(physAddr) + length > size) {
        return nullptr;
    }
    return static_cast<u8*>(OSPhysicalToCached(physAddr));
}

} // namespace

extern "C" {

// ---- AR DMA -----------------------------------------------------------------------------------

ARCallback ARRegisterDMACallback(ARCallback callback) {
    Guard guard;
    ARCallback old = sDmaCallback;
    sDmaCallback = callback;
    return old;
}

u32 ARGetDMAStatus(void) {
    // Every DMA has ended by the time ARStartDMA returns.
    return 0;
}

void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length) {
    if (((mainmem_addr | aram_addr | length) & 31) != 0) {
        COS_SDK_LOG_ONCE("ARStartDMA(type %u, main 0x%08x, aram 0x%08x, length 0x%x): not 32-byte "
                         "aligned (the console would drop the low bits; copying the exact range)",
                         type, mainmem_addr, aram_addr, length);
    }
    u8* main = Mem1Range(mainmem_addr, length);
    u8* aram = AramRange(aram_addr, length);
    if (main == nullptr) {
        Fatal("ARStartDMA(type %u, main 0x%08x, aram 0x%08x, length 0x%x): the main-memory range "
              "is not inside MEM1 (MEM1 size 0x%x; the address must be a MEM1 physical address, "
              "see OSCachedToPhysical, and AuroraConfig.mem1Size must be set before OSInit)",
              type, mainmem_addr, aram_addr, length, aurora::g_config.mem1Size);
    }
    if (aram == nullptr) {
        Fatal("ARStartDMA(type %u, main 0x%08x, aram 0x%08x, length 0x%x): the ARAM range is not "
              "inside ARAM (size 0x%x; ARInit not called or AuroraConfig.mem2Size is 0?)",
              type, mainmem_addr, aram_addr, length, ARGetSize());
    }
    if (type == ARAM_DIR_MRAM_TO_ARAM) {
        std::memmove(aram, main, length);
    } else if (type == ARAM_DIR_ARAM_TO_MRAM) {
        std::memmove(main, aram, length);
    } else {
        Fatal("ARStartDMA: unknown direction %u", type);
    }

    // The DMA-complete interrupt: the handler runs with interrupts disabled.
    Guard guard;
    if (sDmaCallback != nullptr) {
        sDmaCallback();
    }
}

// ---- AR information and clearing --------------------------------------------------------------

u32 ARGetBaseAddress(void) {
    return kAramUserBase;
}

u32 ARGetInternalSize(void) {
    return ARGetSize();
}

void ARClear(u32 flag) {
    auto* base = static_cast<u8*>(ARGetStorageAddress());
    const u32 size = ARGetSize();
    if (base == nullptr) {
        Log("ARClear(%u) before ARInit: nothing to clear", flag);
        return;
    }
    switch (flag) {
    case AR_CLEAR_INTERNAL_ALL:
        std::memset(base, 0, size);
        break;
    case AR_CLEAR_INTERNAL_USER:
        if (size > kAramUserBase) {
            std::memset(base + kAramUserBase, 0, size - kAramUserBase);
        }
        break;
    case AR_CLEAR_EXPANSION:
        // No expansion ARAM on the host.
        break;
    default:
        Log("ARClear: unknown flag %u", flag);
        break;
    }
}

// ---- ARQ queue (always empty) -----------------------------------------------------------------

void ARQRemoveRequest(ARQRequest* task) {
    (void)task;
}

void ARQRemoveOwnerRequest(u32 owner) {
    (void)owner;
}

void ARQFlushQueue(void) {}

void ARQSetChunkSize(u32 size) {
    Guard guard;
    sArqChunkSize = (size & 31) != 0 ? (size & ~31u) + 32 : size;
}

u32 ARQGetChunkSize(void) {
    Guard guard;
    return sArqChunkSize;
}

} // extern "C"
