# Windows: status and feasibility (short study)

Status, 2026-10-05: **no native Windows build.** It is low priority; this note records what is
known so the work can be picked up later. Windows users can already build the Linux binary and
the Switch NRO with Docker (README, "Build with Docker (any OS)": WSL 2 with Docker Desktop or
Podman), and run the Linux binary under WSLg.

No Windows compile was attempted for this note (the decision to deprioritise came first); the
points below come from reading the code and from the Linux port, which needed only small changes
(compiler and libraries were the same family).

## What is already in place

- **Aurora** supports Windows: `AuroraDawnProvider.cmake` picks D3D12, Vulkan and D3D11 there,
  and prebuilt packages exist for Dawn (`dawn-windows-amd64`, `-arm64`), nod (`libnod-windows-*`)
  and SDL 3 (Aurora's `package` provider). These are MSVC builds.
- **Dolphin's DSP HLE** (`native/dsp_hle`) builds with MSVC upstream.
- **The game** is clang-clean on macOS and Linux (840 units, LP64).

## Blockers

1. **Toolchain.** The prebuilt Dawn/nod/SDL packages use the MSVC ABI, so the port should use
   **clang-cl** (not MinGW: MinGW cannot link MSVC C++ static libraries; it would need Dawn and
   nod built from source). The game's flags (`native/cmake/GameConfig.cmake`) are GCC-style
   (`-include`, `-fno-strict-aliasing`, `-ftrivial-auto-var-init=zero`, `-fcheck-new`,
   `-fsigned-char`, the `-Wno-*` list); clang-cl takes them through `/clang:` or has equivalents
   (`/FI`, `/J` inverted). `__attribute__((weak))` (Aurora's `DECL_WEAK`, cos_sdk defaults) has no
   COFF equivalent with the same semantics: those defaults need `/alternatename` or plain
   (non-weak) definitions selected at build time.
2. **LLP64.** On Windows `long` is 32 bits. The phase 4 fixes made the game correct for LP64
   (macOS, Linux, the Switch); any remaining `long`/`unsigned long` that holds a pointer or a
   64-bit value would truncate silently. A census (`-Wpointer-to-int-cast`,
   `-Wshorten-64-to-32` under clang-cl) is the first step; the code has ~390 `intptr_t`/`uintptr_t`
   uses that are fine, and about a dozen `(long)` casts near pointers to review.
3. **POSIX in our own code.**
   - `native/sdk/src/os` (OS threads, alarms, interrupts) is written on pthreads
     (`pthread_create` with a stack size, `pthread_exit`): the thread and alarm code needs
     a Win32 port (or winpthreads).
   - `native/src/pc` (the run harness): `sigaction`/`sigaltstack` crash handler (→ a vectored
     exception handler), `execinfo`/`dladdr` backtraces (→ `CaptureStackBackTrace`/DbgHelp),
     `pause`, `realpath`, `mkdir(path, mode)`, `clock_gettime`, `nanosleep`, `/proc/self/exe`
     (→ `GetModuleFileNameW`), `pthread_setname_np`. Each already sits behind a small `#if` per
     host, so this is additive.
   - `native/sdk/tests` fork children to test panics and exits (→ `CreateProcess` of the same test
     binary with an argument, or skip those cases on Windows).
4. **Scripts.** `native/tools/*.sh` (gen_assets, run, regress) are bash: run them from Git Bash or
   WSL; `run.sh` symbolises crashes with `atos`/`addr2line` (→ `llvm-symbolizer` with the PDB).
5. **Paths.** The harness and Aurora pass UTF-8 paths; Aurora already converts with
   `aurora::io::fs_path_from_string`. Our `user/` and `settings.ini` code builds paths with `/`,
   which Windows accepts.

## Estimate

| Part | Effort |
|---|---|
| CMake for clang-cl, flags, weak symbols, first full compile and link | 2–3 days |
| LLP64 census and fixes in the game | 1–3 days (unknown until the census) |
| cos_sdk OS layer on Win32 (threads, alarms), harness (crash handler, paths, timing) | 2–3 days |
| Smoke tests and regression on Windows (scripts under Git Bash/WSL), CI job | 1–2 days |
| **Total** | **about 1.5–2 weeks** |

Recommended order when it is picked up: a clang-cl configure in CI (no disc needed for
`cos_sdk_smoke`/`cos_pc_tests`, see `.github/workflows/ci.yml`), then the LLP64 census, then the
OS layer.
