# cos_sdk: the GameCube SDK for the native port, over Aurora

Phase 2 of the native port (`docs/NATIVE_PORT_PHASE2_3.md`). The model is Dusklight's: Aurora's
SDK libraries (`aurora::core gx gd si vi pad mtx os dvd thp card`, listed in `COS_AURORA_LIBS`)
plus a layer of glue for what Aurora does not provide (threads, messages, alarms, VI retrace, GF,
AI/DSP, EXI, GBA, DVD extras, ...). That glue lives here.

Built with Aurora, which every configure brings in since step 2.8 (see `native/README.md`,
"Aurora (phase 2)").

## Layout

- `src/<library>/*.{c,cpp}`: the SDK functions, one directory per SDK library (`os`, `vi`, `gx`,
  `dvd`, ...). `native/cmake/sdk.cmake` globs `src/**/*.{c,cpp}` with `CONFIGURE_DEPENDS`, so adding
  a file needs no CMake change (re-running `ninja` picks it up).
- `include/cos_sdk/`: the library's own headers (hooks the game glue sets, information functions).
  This directory is on `cos_sdk`'s public include path.
- `tests/`: the headless test program `cos_sdk_smoke` (no window, no GPU). Every `tests/*.cpp` is
  globbed; each file registers its tests by name with `COS_SMOKE_TEST(name)` from `tests/smoke.h`.

## Rules

- Compiled against **Aurora's headers only**: never `game/include` and never the game's flags
  (`cos_game_headers`). The SDK's own declarations come from `<dolphin/...>` as Aurora ships them;
  game-only declarations go into the forwarders of `native/include/sdk` (steps 2.3 and 2.4).
- `MTX_USE_PS=1` is public on `cos_sdk`, as in Dusklight's game flags.
- Code adapted from Dusklight (`ref/dusklight/src/dusk`, CC0) starts with a provenance comment
  naming the original file and what changed (decision D7). Code adapted from Aurora (MIT) keeps
  its notice.
- Hardware-only functions may be stubs only where the plan says so. A stub whose silent result
  would hide a bug logs it, and `OSPanic`/`OSFatal` abort.
- The SDK layer itself is 64-bit correct (pointers are `uintptr_t`, never `u32`).

## Building and running the test

```sh
cmake -S native -B build/native-mac -G Ninja
ninja -C build/native-mac cos_sdk cos_sdk_smoke
build/native-mac/cos_sdk_smoke            # every test; prints "ok"
build/native-mac/cos_sdk_smoke basic      # only the named tests
build/native-mac/cos_sdk_smoke --list
```

`basic` (step 2.2) checks `OSInit`, `OSGetTime` and `PSMTXConcat`. Aurora's `OSInit` does not need
`aurora_initialize`. Since step 2.6e the smoke program sets `AuroraConfig.mem1Size` (24 MiB) and
`mem2Size` (16 MiB) at static initialisation (`tests/sdk_devices.cpp`), so whichever test calls
`OSInit` first allocates MEM1 and the arena, and `ARInit` gets ARAM; the `ar` test needs both.

`threads` and `alarms` (step 2.6a, `tests/sdk_threads.cpp`): two OS threads exchange 1000
messages each way through blocking message queues and are joined (one ends through
`OSExitThread` with a 64-bit value); four threads count under a recursive `OSMutex` while the main
thread waits on an `OSCond`; `OSSleepThread`/`OSWakeupThread` with interrupts disabled,
self-suspension, thread-specific storage, `OSCancelThread` on a blocked thread; one-shot, ordered,
cancelled, tagged, periodic and self-re-arming alarms, and JFWDisplay's "set an alarm that resumes
me, then suspend myself with interrupts disabled".

### ThreadSanitizer run

`cos_sdk_smoke_tsan` (not in `all`) builds the same tests and `src/` with `-fsanitize=thread` and
must print `ok` with no TSan report. Xcode 26's Apple clang 17 TSan runtime crashes at start-up on
macOS 26.6 (even for an empty program), so it is built in its own build directory with the
Command Line Tools clang (21), reusing the fetched sources of the main build directory:

```sh
D=$PWD/build/native-mac/_deps
cmake -S native -B build/native-mac-tsan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/Library/Developer/CommandLineTools/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/Library/Developer/CommandLineTools/usr/bin/clang++ \
  -DFETCHCONTENT_SOURCE_DIR_AURORA=$PWD/build/aurora-3227d76 \
  -DFETCHCONTENT_SOURCE_DIR_ABSEIL-CPP=$D/abseil-cpp-src -DFETCHCONTENT_SOURCE_DIR_FMT=$D/fmt-src \
  -DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=$D/dawn_prebuilt-src \
  -DFETCHCONTENT_SOURCE_DIR_NOD_PREBUILT=$D/nod_prebuilt-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/imgui-src -DFETCHCONTENT_SOURCE_DIR_SDL=$D/sdl-src \
  -DFETCHCONTENT_SOURCE_DIR_TRACY=$D/tracy-src -DFETCHCONTENT_SOURCE_DIR_XXHASH=$D/xxhash-src
ninja -C build/native-mac-tsan cos_sdk_smoke_tsan
build/native-mac-tsan/cos_sdk_smoke_tsan
```

If a race is found and the report hangs while symbolizing (`atos`), rerun with
`TSAN_OPTIONS=symbolize=0`. With a toolchain whose TSan works, `ninja cos_sdk_smoke_tsan` in the main build directory is enough.

## OS threads, interrupts and alarms (step 2.6a)

`src/os/` holds the host implementation; `src/os/os_internal.h` describes the model. In short:
"interrupts disabled" is one process-wide lock held per thread with the GameCube's boolean
semantics, every OS function runs under it, blocked threads wait on their own condition variable
(which releases the lock), `OSCreateThread` threads are detached pthreads launched by the first
`OSResumeThread`, and alarms fire on a real host timer thread that calls the handlers with the lock
held (decision D6). Differences from the console that game code can notice: threads run in
parallel and priorities order nothing (phase 6), and `OSSuspendThread`/`OSCancelThread` on
another thread that is running game code act at its next OS call (both are logged).
`OSInitContext`, `OSSaveContext`, `OSLoadContext`, `OSSwitchStack` and `OSSwitchFiber` abort.

`include/cos_sdk/hooks.h` replaces Dusklight's couplings to the game: `COSSdkRequestShutdown`
(blocking message-queue calls return FALSE, alarms stop) and `COSSdkSetThreadStartHook` (runs on
each new OS thread before its entry function; the game glue sets the thread's current `JKRHeap`
there, since the switch-thread callback is never called on host threads).

## OS misc (step 2.6b)

`src/os/OSReport.cpp`, `OSMisc.cpp`, `OSReset.cpp`, `OSSram.cpp`, `PPC.cpp` and `LC.cpp`; test
`misc` (`tests/sdk_misc.cpp`). Each file header says what it emulates and how it differs from the
console. The parts game glue needs to know:

- `OSReport`, `OSVReport` and `OSPanic` here are weak defaults for programs without the game; the
  game's `m_Do_printf.cpp` defines the real ones. `OSFatal`, `OSPanic`, `PPCHalt` abort.
- `OSResetSystem` never returns. It calls the hook set with `COSSdkSetResetHook` (`hooks.h`), which
  must end the process or the calling OS thread; without one it ends the process (exit code 0).
  `OSGetResetCode` and `OSGetSavedRegion` then describe that reset; `COSSdkSetResetCode` seeds the
  code for a game restarted in a new process.
- The SRAM (sound mode, progressive scan, EuRGB60, language, ...) lives in memory and starts from
  defaults in every process; `cos_sdk/sram.h` declares `__OSLockSram`, the RTC functions and the
  rest of the SDK-internal API.
- PPC special registers are emulated values (`MSR[EE]` is the interrupt state, the decrementer
  counts). Error handlers set with `OSSetErrorHandler` are recorded but no host fault reaches them;
  `COSSdkGetErrorHandler` returns them.

## VI retrace and GX gaps (step 2.6c)

`src/vi/VIRetrace.cpp`, `src/gx/GXExtras.cpp`; tests `vi` and `gx` (`tests/sdk_vi.cpp`). What game
glue needs to know:

- There is no VI interrupt: each `VIWaitForRetrace` call is one retrace (count, pre-retrace
  callback, next frame buffer becomes current, post-retrace callback, with interrupts disabled).
  It returns at once; pacing is the main loop's job (phase 6).
- `VISetBlack` does not blank Aurora's output; `COSSdkVIIsBlack` (`hooks.h`) returns the value.
- `GXSetDrawSync` calls the draw sync callback before it returns. `GXPeekARGB` reads white (no EFB
  colour read-back yet). `GXSetMisc(GX_MT_DL_SAVE_CONTEXT, 0)` does not reach Aurora's private
  display-list state. Each is logged once. `GXGetFifoBase`/`GXGetFifoSize` abort.
- `cos_sdk/gx.h` declares the GX functions Aurora's headers lack (`GXSetDrawSync`,
  `GXSetGPMetric`, `GXClearGPMetric`).

## GF (step 2.6d)

The game's own `game/src/dolphin/gf/*.cpp`, built by `native/cmake/sdk_gf.cmake` (OBJECT library
`cos_sdk_gf`, objects added to `cos_sdk`) against Aurora's headers and the forwarders in
`native/include/sdk`; test `gf` (`tests/sdk_gf.cpp`). What game glue needs to know:

- GF writes raw BP/XF/CP commands through `GXCmd1u*`; Aurora parses them from its FIFO in
  immediate mode as in display lists. The test checks each GF function against Aurora's GD twin.
- `GFSetArray` stops with `OSPanic` on Aurora: a 32-bit `CP_REG_ARRAYBASE` cannot hold a host
  pointer and Aurora ignores it. Callers use `GFSetArraySized(attr, ptr, sizeBytes, stride, le)`
  (declared in `native/include/sdk/cos_gf_extras.h`), `le` true for arrays the host builds.

## Devices: AR, GBA, EXI, SI, DB, amcstubs (step 2.6e)

`src/ar/AR.cpp`, `src/gba/GBA.cpp`, `src/exi/EXI.cpp`, `src/si/SIExtras.cpp`, `src/db/DB.cpp`,
`src/db/AmcExi2Stubs.cpp`; tests `ar`, `gba`, `exi`, `si` and `db` (`tests/sdk_devices.cpp`). DVD
and CARD need nothing here: every DVD name the plan lists (`DVDLow*`, streaming, `DVDChangeDir`,
`DVDCancel*`, `DVDGetDriveStatus`, `DVDCheckDisk`, `DVDSetAutoFatalMessaging`) is in
`aurora_dvd`, and the CARD icon/banner accessors are macros in Aurora's `card.h`, with
`CARDGetSerialNo` in `aurora_card`. What game glue needs to know:

- `ARStartDMA` copies at once and calls the `ARRegisterDMACallback` callback with interrupts
  disabled; `ARGetDMAStatus` is always 0. Its main-memory address is a **MEM1 physical address**
  (`OSCachedToPhysical`, so the game must set `AuroraConfig.mem1Size` and allocate from the
  arena); anything outside MEM1 or ARAM aborts with the arguments. `ARGetBaseAddress` is 0x4000,
  what Aurora's `ARInit` returns. `ARReset`, `ARSetSize`, `ARQReset`, `ARQCheckInit` and
  `__AR*Interrupt*` are left undefined (they need Aurora's private AR state; nothing calls them).
- GBA: every port is empty. Requests return `GBA_NOT_READY` (async ones never call back);
  `GBAGetProcessStatus` returns `GBA_READY` ("nothing in progress").
- EXI: the lock and its unlock queue are real; every device selection fails (logged once).
- SI: `SIGetType`/`SIGetStatus` follow Aurora's `SIProbe`; raw `SITransfer`s complete with
  `SI_ERROR_NO_RESPONSE` on the alarm thread; polling words are kept but no polling interrupt or
  response exists (PAD goes through Aurora).
- DB: no debugger (`DBIsDebuggerPresent` FALSE); the debugger link refuses reads and writes;
  `__DBExceptionDestination` aborts. amcstubs are the SDK's own no-op stubs (`AMC_IsStub` 1).
- Aurora's `<dolphin/gba.h>` includes a bare `<types.h>` that only `native/include/sdk` has, and
  cos_sdk compiles against Aurora's headers only, so `GBA.cpp` (and the game-only SI, DB and
  amcstubs names) repeat their declarations; the tests call them through the forwarders.

## Audio hardware, silent, and MSL extras (step 2.6f)

`src/audio/AIStubs.cpp`, `src/audio/DSPStubs.cpp`, `src/audio/DTK.cpp`, `src/runtime/extras.c`;
tests `audio` and `msl` (`tests/sdk_audio.cpp`). Real audio is phase 5. What game glue and the
JAudio steps (3.7, phase 5) need to know:

- AI: a register model that plays nothing. Every setting reads back (`AIGetDSPSampleRate` uses
  the SDK's encoding, 0 = 32 kHz, 1 = 48 kHz; 1 before `AIInit`, 0 after it), but no interrupt is
  ever raised: the DMA and stream callbacks are never called, so JAudio's audio thread waits for
  a DMA tick that never comes (it blocks; it does not spin). `AIInitDMA` keeps the 64-bit
  address; `AIGetDMAStartAddr` aborts if it does not fit the SDK's u32.
- DSP: no DSP runs. Mail to the DSP is taken at once (`DSPCheckMailToDSP` is 0), no mail ever
  comes back, `__DSP_boot_task`/`__DSP_exec_task` log once and return without waiting. The task
  list (`__DSP_*_task`) is the SDK's. `DSPAddTask` is weak: JAudio's `osdsp.c` replaces it, and
  JAudio's own `__DSPHandler` is never called. JAudio's DSP handshakes (`DspHandShake`,
  `DSPSendCommands2`, `DsetupTable`, `DsetDolbyDelay`) busy-wait for DSP replies in JAudio code;
  phase 5 replaces them.
- DTK: the SDK's state machine (from Dusklight's `libs/dolphin`) over the silent AI and Aurora's
  DVD stream commands, which complete at once: tracks queue and change state, nothing plays and
  no track ends. The game does not call DTK. `cos_sdk_smoke_tsan` leaves `DTK.cpp` out (it links no
  `aurora::dvd`) and skips the DTK checks.
- `stricmp`, `strnicmp`: MSL's, with characters compared as unsigned (as on PowerPC). No other
  MSL-only name is unresolved across all 840 game units, and JAudio/JAZelAudio need none.
