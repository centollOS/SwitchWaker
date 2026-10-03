// cos_sdk_smoke: the tests of step 2.6e, the devices Aurora lacks or covers only partly:
// "ar" (ARStartDMA round trip through ARAM), "gba", "exi", "si" and "db".
//
// The game-only names come through the forwarders of native/include/sdk, as the game will see them.
//
// ARStartDMA's main-memory address is a MEM1 physical address, so the "ar" test needs the MEM1
// block Aurora's OSInit allocates from AuroraConfig.mem1Size, and ARAM from mem2Size. The smoke
// program never calls aurora_initialize, so this file sets both sizes (the console's 24 MiB and
// 16 MiB) at static initialisation, before any test calls OSInit. OSInit then also sets up the
// arena in MEM1; no other test depends on there being no MEM1.
#include "smoke.h"

#include <dolphin/amcstubs/AmcExi2Stubs.h>
#include <dolphin/ar/ar.h>
#include <dolphin/ar/arq.h>
#include <dolphin/db/db.h>
#include <dolphin/exi/EXIBios.h>
#include <dolphin/gba/GBA.h>
#include <dolphin/os.h>
#include <dolphin/si/SIBios.h>

#include <aurora/aurora.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

namespace aurora {
extern AuroraConfig g_config;
} // namespace aurora

namespace {

[[maybe_unused]] const bool sMemoryConfigured = [] {
    if (aurora::g_config.mem1Size == 0) {
        aurora::g_config.mem1Size = 24 * 1024 * 1024;
    }
    if (aurora::g_config.mem2Size == 0) {
        aurora::g_config.mem2Size = 16 * 1024 * 1024;
    }
    return true;
}();

// Runs `fn` in a forked child with stderr captured; true if the child aborted (SIGABRT) and its
// stderr contains `text`.
bool ChildAborts(void (*fn)(), const char* text) {
    int fds[2];
    if (pipe(fds) != 0) {
        return false;
    }
    std::fflush(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        alarm(10);
        fn();
        std::fflush(nullptr);
        _exit(0);
    }
    close(fds[1]);
    std::string err;
    char buf[512];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        err.append(buf, static_cast<std::size_t>(n));
    }
    close(fds[0]);
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) != pid) {
        return false;
    }
    const bool ok = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT &&
                    err.find(text) != std::string::npos;
    if (!ok) {
        std::fprintf(stderr, "child: status 0x%x, stderr:\n%s\n", status, err.c_str());
    }
    return ok;
}

// ---- ar ---------------------------------------------------------------------------------------

std::atomic<int> sDmaCallbacks{0};

void DmaDone() {
    sDmaCallbacks++;
}

u32 sArStack[16];

void ConsoleAddressDma() {
    ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, 0x80001000u, ARGetBaseAddress(), 32);
}

void AramOverrunDma() {
    void* buf = OSAllocFromArenaLo(64, 32);
    ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, OSCachedToPhysical(buf), ARGetSize() - 32, 64);
}

} // namespace

COS_SMOKE_TEST(ar) {
    OSInit();
    COS_SMOKE_CHECK(OSGetArenaLo() != nullptr);
    const u32 base = ARInit(sArStack, 16);
    COS_SMOKE_CHECK(base == ARGetBaseAddress());
    COS_SMOKE_CHECK(ARGetInternalSize() == ARGetSize());
    COS_SMOKE_CHECK(ARGetSize() == 16u * 1024 * 1024);

    constexpr u32 kSize = 4096;
    const u32 aram = ARAlloc(kSize);
    auto* src = static_cast<u8*>(OSAllocFromArenaLo(kSize, 32));
    auto* dst = static_cast<u8*>(OSAllocFromArenaLo(kSize, 32));
    COS_SMOKE_CHECK(src != nullptr && dst != nullptr);
    for (u32 i = 0; i < kSize; i++) {
        src[i] = static_cast<u8>(i * 7 + 3);
    }
    std::memset(dst, 0, kSize);

    COS_SMOKE_CHECK(ARRegisterDMACallback(DmaDone) == nullptr);
    sDmaCallbacks = 0;

    // Main RAM -> ARAM: the bytes land in Aurora's ARAM buffer at the ARAM offset.
    ARStartDMAWrite(OSCachedToPhysical(src), aram, kSize);
    COS_SMOKE_CHECK(sDmaCallbacks == 1);
    COS_SMOKE_CHECK(ARGetDMAStatus() == 0);
    COS_SMOKE_CHECK(std::memcmp(static_cast<u8*>(ARGetStorageAddress()) + aram, src, kSize) == 0);

    // ARAM -> main RAM.
    ARStartDMARead(OSCachedToPhysical(dst), aram, kSize);
    COS_SMOKE_CHECK(sDmaCallbacks == 2);
    COS_SMOKE_CHECK(std::memcmp(dst, src, kSize) == 0);

    // ARQ (Aurora's ARQPostRequest) sees the same ARAM.
    std::memset(dst, 0, kSize);
    ARQRequest req;
    ARQPostRequest(&req, 0, ARQ_TYPE_ARAM_TO_MRAM, ARQ_PRIORITY_LOW, aram,
                   reinterpret_cast<uintptr_t>(dst), kSize, nullptr);
    COS_SMOKE_CHECK(std::memcmp(dst, src, kSize) == 0);
    ARQRemoveOwnerRequest(0);
    ARQFlushQueue();
    ARQSetChunkSize(100);
    COS_SMOKE_CHECK(ARQGetChunkSize() == 128);
    ARQSetChunkSize(ARQ_CHUNK_SIZE_DEFAULT);
    COS_SMOKE_CHECK(ARQGetChunkSize() == ARQ_CHUNK_SIZE_DEFAULT);

    // ARClear(AR_CLEAR_INTERNAL_USER) clears the user area, which holds the block.
    ARClear(AR_CLEAR_INTERNAL_USER);
    ARStartDMARead(OSCachedToPhysical(dst), aram, kSize);
    bool zero = true;
    for (u32 i = 0; i < kSize; i++) {
        zero = zero && dst[i] == 0;
    }
    COS_SMOKE_CHECK(zero);

    COS_SMOKE_CHECK(ARRegisterDMACallback(nullptr) == DmaDone);
    ARFree(nullptr);

    // A console address in place of a MEM1 physical one, and a range past the end of ARAM, abort.
    COS_SMOKE_CHECK(ChildAborts(ConsoleAddressDma, "not inside MEM1"));
    COS_SMOKE_CHECK(ChildAborts(AramOverrunDma, "not inside ARAM"));
    return true;
}

// ---- gba --------------------------------------------------------------------------------------

namespace {

int sGbaCallbacks = 0;

void GbaCallback(s32, s32) {
    sGbaCallbacks++;
}

void BadGbaChannel() {
    u8 status;
    GBAGetStatus(4, &status);
}

} // namespace

COS_SMOKE_TEST(gba) {
    GBAInit();
    u8 status = 0xAB;
    u8 data[4] = {1, 2, 3, 4};
    for (s32 chan = 0; chan < GBA_MAX_CHAN; chan++) {
        COS_SMOKE_CHECK(GBAGetStatus(chan, &status) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAReset(chan, &status) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBARead(chan, data, &status) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAWrite(chan, data, &status) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAJoyBoot(chan, 0, 0, data, sizeof(data), &status) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAGetStatusAsync(chan, &status, GbaCallback) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAResetAsync(chan, &status, GbaCallback) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAReadAsync(chan, data, &status, GbaCallback) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAWriteAsync(chan, data, &status, GbaCallback) == GBA_NOT_READY);
        COS_SMOKE_CHECK(GBAJoyBootAsync(chan, 0, 0, data, sizeof(data), &status, GbaCallback) ==
                        GBA_NOT_READY);
        u8 percent = 0xCD;
        COS_SMOKE_CHECK(GBAGetProcessStatus(chan, &percent) == GBA_READY); // nothing in progress
        COS_SMOKE_CHECK(percent == 0xCD);
    }
    COS_SMOKE_CHECK(status == 0xAB); // never written
    COS_SMOKE_CHECK(data[0] == 1 && data[3] == 4);
    COS_SMOKE_CHECK(sGbaCallbacks == 0); // refused requests never call back
    COS_SMOKE_CHECK(ChildAborts(BadGbaChannel, "invalid GBA channel 4"));
    return true;
}

// ---- exi --------------------------------------------------------------------------------------

namespace {

int sUnlockCalls = 0;
s32 sUnlockChan = -1;

void Unlocked(s32 chan, OSContext*) {
    sUnlockCalls++;
    sUnlockChan = chan;
    // On the console the unlock callback usually takes the lock it waited for.
    EXILock(chan, 2, nullptr);
}

} // namespace

COS_SMOKE_TEST(exi) {
    EXIInit();
    COS_SMOKE_CHECK(EXIGetState(0) == EXI_STATE_IDLE);

    // The lock is real state: a second locker queues its callback, which runs at unlock.
    COS_SMOKE_CHECK(EXILock(0, 1, nullptr));
    COS_SMOKE_CHECK(EXIGetState(0) == EXI_STATE_LOCKED);
    COS_SMOKE_CHECK(!EXILock(0, 2, Unlocked));
    COS_SMOKE_CHECK(!EXILock(0, 2, Unlocked)); // same device: queued once
    COS_SMOKE_CHECK(EXIUnlock(0));
    COS_SMOKE_CHECK(sUnlockCalls == 1 && sUnlockChan == 0);
    COS_SMOKE_CHECK(EXIGetState(0) == EXI_STATE_LOCKED); // taken by the callback
    COS_SMOKE_CHECK(EXIUnlock(0));
    COS_SMOKE_CHECK(sUnlockCalls == 1); // the queue is empty
    COS_SMOKE_CHECK(!EXIUnlock(0));     // not locked

    // Nothing is attached: device access fails.
    COS_SMOKE_CHECK(!EXIProbe(0) && !EXIProbe(1) && EXIProbe(2));
    COS_SMOKE_CHECK(EXIProbeEx(0) == -1 && EXIProbeEx(1) == -1);
    COS_SMOKE_CHECK(!EXIAttach(0, nullptr));
    COS_SMOKE_CHECK(EXIDetach(0));
    COS_SMOKE_CHECK(EXILock(0, 0, nullptr));
    COS_SMOKE_CHECK(!EXISelect(0, 0, EXI_FREQ_16M));
    u32 word = 0x12345678;
    COS_SMOKE_CHECK(!EXIImm(0, &word, 4, EXI_WRITE, nullptr));
    COS_SMOKE_CHECK(!EXIImmEx(0, &word, 4, EXI_WRITE));
    COS_SMOKE_CHECK(!EXIDma(0, &word, 4, EXI_READ, nullptr));
    COS_SMOKE_CHECK(!EXISync(0));
    COS_SMOKE_CHECK(!EXIDeselect(0));
    COS_SMOKE_CHECK(EXIUnlock(0));
    COS_SMOKE_CHECK(word == 0x12345678);
    u32 id = 0xFFFF;
    COS_SMOKE_CHECK(EXIGetID(0, 0, &id) == 0 && id == 0xFFFF);
    COS_SMOKE_CHECK(EXIGetType(1, 0, &id) == 0);
    COS_SMOKE_CHECK(std::strcmp(EXIGetTypeString(EXI_MEMORY_CARD_59), "Memory Card 59") == 0);
    COS_SMOKE_CHECK(std::strcmp(EXIGetTypeString(0xDEAD), "Unknown") == 0);

    COS_SMOKE_CHECK(EXISetExiCallback(1, Unlocked) == nullptr);
    COS_SMOKE_CHECK(EXISetExiCallback(1, nullptr) == Unlocked);

    // The debugger's AMC stubs: AMC_IsStub says so, everything else does nothing.
    COS_SMOKE_CHECK(AMC_IsStub());
    COS_SMOKE_CHECK(EXI2_Poll() == 0);
    COS_SMOKE_CHECK(EXI2_ReadN(&word, 4) == AMC_EXI_NO_ERROR);
    COS_SMOKE_CHECK(EXI2_WriteN(&word, 4) == AMC_EXI_NO_ERROR);
    return true;
}

// ---- si ---------------------------------------------------------------------------------------

namespace {

std::atomic<int> sTransferDone{0};
std::atomic<u32> sTransferStatus{0};
std::atomic<s32> sTransferChan{-1};
std::atomic<bool> sTransferContextOk{false};
u32 sTypeSeen = 0xFFFFFFFF;

void TransferCallback(s32 chan, u32 sr, OSContext* context) {
    sTransferChan = chan;
    sTransferStatus = sr;
    sTransferContextOk = context != nullptr;
    sTransferDone++;
}

void TypeCallback(s32, u32 type) {
    sTypeSeen = type;
}

void PollingHandler(__OSInterrupt, OSContext*) {}

} // namespace

COS_SMOKE_TEST(si) {
    SIInit();

    // With no controller layer started, every port is empty.
    const u32 type = SIGetType(0);
    COS_SMOKE_CHECK(type == SIProbe(0));
    COS_SMOKE_CHECK(SIGetTypeAsync(0, TypeCallback) == type);
    COS_SMOKE_CHECK(sTypeSeen == type);
    COS_SMOKE_CHECK(SIGetStatus(0) == (type == SI_ERROR_NO_RESPONSE ? SI_ERROR_NO_RESPONSE : 0u));

    // A raw transfer completes later with no response, from "interrupt" context.
    u8 out[1] = {0};
    u8 in[3] = {0xAA, 0xAA, 0xAA};
    COS_SMOKE_CHECK(!SIBusy());
    COS_SMOKE_CHECK(SITransfer(1, out, 1, in, 3, TransferCallback, OSMillisecondsToTicks(5)));
    COS_SMOKE_CHECK(SIIsChanBusy(1) && SIBusy() && !SIIsChanBusy(0));
    COS_SMOKE_CHECK(!SITransfer(1, out, 1, in, 3, TransferCallback, 0)); // one per channel
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (sTransferDone == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    COS_SMOKE_CHECK(sTransferDone == 1);
    COS_SMOKE_CHECK(sTransferChan == 1);
    COS_SMOKE_CHECK(sTransferStatus == SI_ERROR_NO_RESPONSE);
    COS_SMOKE_CHECK(sTransferContextOk);
    COS_SMOKE_CHECK(!SIIsChanBusy(1) && !SIBusy());
    COS_SMOKE_CHECK(in[0] == 0xAA);

    // Polling words, computed as the SDK does.
    COS_SMOKE_CHECK(SISetXY(0x80, 0x10) == ((0x80u << 16) | (0x10u << 8)));
    const u32 enabled = SIEnablePolling(SI_CHAN0_BIT | SI_CHAN2_BIT);
    COS_SMOKE_CHECK(enabled == ((0x80u << 16) | (0x10u << 8) | 0xA0u));
    COS_SMOKE_CHECK(SIEnablePolling(0) == enabled);
    COS_SMOKE_CHECK(SIDisablePolling(SI_CHAN0_BIT) == ((0x80u << 16) | (0x10u << 8) | 0x20u));
    SISetCommand(0, 0x00400300);
    SITransferCommands();
    u32 response[2] = {7, 7};
    COS_SMOKE_CHECK(!SIGetResponse(0, response));
    COS_SMOKE_CHECK(response[0] == 7);

    // The polling handler table: four entries, duplicates accepted once.
    COS_SMOKE_CHECK(SIRegisterPollingHandler(PollingHandler));
    COS_SMOKE_CHECK(SIRegisterPollingHandler(PollingHandler));
    COS_SMOKE_CHECK(SIUnregisterPollingHandler(PollingHandler));
    COS_SMOKE_CHECK(!SIUnregisterPollingHandler(PollingHandler));
    return true;
}

// ---- db ---------------------------------------------------------------------------------------

namespace {

void ExceptionToDebugger() {
    __DBExceptionDestination();
}

} // namespace

COS_SMOKE_TEST(db) {
    COS_SMOKE_CHECK(!DBIsDebuggerPresent());
    DBInit();
    COS_SMOKE_CHECK(__DBInterface != nullptr);
    COS_SMOKE_CHECK(!DBIsDebuggerPresent());
    COS_SMOKE_CHECK(!__DBIsExceptionMarked(OS_ERROR_DSI));
    __DBMarkException(OS_ERROR_DSI, 1);
    COS_SMOKE_CHECK(__DBIsExceptionMarked(OS_ERROR_DSI));
    COS_SMOKE_CHECK(!__DBIsExceptionMarked(OS_ERROR_ISI));
    __DBMarkException(OS_ERROR_DSI, 0);
    COS_SMOKE_CHECK(!__DBIsExceptionMarked(OS_ERROR_DSI));
    __DBSetPresent(1);
    COS_SMOKE_CHECK(DBIsDebuggerPresent());
    __DBSetPresent(0);
    COS_SMOKE_CHECK(!DBIsDebuggerPresent());
    DBPrintf(const_cast<char*>("not printed %d\n"), 1);

    // The debugger link reports no data and refuses transfers.
    COS_SMOKE_CHECK(DBQueryData() == 0);
    u8 buf[8] = {};
    COS_SMOKE_CHECK(DBRead(buf, sizeof(buf)) != 0);
    COS_SMOKE_CHECK(DBWrite(buf, sizeof(buf)) != 0);
    DBOpen();
    DBClose();

    COS_SMOKE_CHECK(ChildAborts(ExceptionToDebugger, "routed to the debugger"));
    return true;
}
