#!/usr/bin/env bash
# CMake compiler launcher for the native port's game units on the Switch
# (switch/native/CMakeLists.txt sets it on every target that compiles like a
# game unit). CMake runs it as
#   clang-launcher.sh <devkitA64 gcc or g++> <arguments...>
# and it compiles with clang 19 for the Switch's Cortex-A57 instead: the Mac
# build (Apple clang 17, LLVM 19) is what the decompiled game was brought up
# with, and devkitA64's GCC rejects some of its accepted constructs (template
# arguments that narrow, -Wno-c++11-narrowing and friends).
#
# - The C and C++ headers stay devkitA64's (newlib, libstdc++ 15), in GCC's
#   search order, so the objects link with the rest of the NRO (built by GCC).
#   clang's own resource directory replaces GCC's (intrinsics, stddef.h...).
# - GCC's thread pointer model (-mtp=soft: libnx's __aarch64_read_tp) has no
#   clang equivalent, so thread_local variables use emulated TLS
#   (__emutls_get_address from devkitA64's libgcc). JKRHeap::sCurrentHeap is
#   the game's only one.
set -euo pipefail

compiler=$1
shift
case $compiler in
    *g++) clang=clang++-19 cxx=1 ;;
    *gcc) clang=clang-19 cxx=0 ;;
    *) exec "$compiler" "$@" ;;
esac

dkp=${DEVKITPRO:-/opt/devkitpro}
gcc_root=$dkp/devkitA64/aarch64-none-elf
cxx_include=$(echo "$gcc_root"/include/c++/*)
resource=$("$clang" -print-resource-dir)

args=()
for arg in "$@"; do
    case $arg in
        # GCC-only code generation options; the target is set below.
        -mtp=*|-ftls-model=*|-march=*|-mtune=*|-fmax-errors=*) ;;
        *) args+=("$arg") ;;
    esac
done

includes=(-nostdinc)
if [[ $cxx == 1 ]]; then
    includes+=(-isystem "$cxx_include" -isystem "$cxx_include/aarch64-none-elf"
               -isystem "$cxx_include/backward")
fi
includes+=(-isystem "$resource/include" -isystem "$gcc_root/include")

exec "$clang" --target=aarch64-none-elf -mcpu=cortex-a57 -femulated-tls \
    "${includes[@]}" "${args[@]}"
