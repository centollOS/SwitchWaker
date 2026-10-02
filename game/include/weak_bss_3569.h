#ifndef WEAK_BSS_3569_H
#define WEAK_BSS_3569_H

// Fake header.
// This is some kind of weak object that get included in several TUs.
// Goes in either .bss (for RELs) or .rodata (for main.dol: d_snap, J3DUClipper).
// Its true source is currently unknown, so include this header in TUs that need it to match for now.

#ifdef DECOMPCTX // Hack, see comment in dolzel.pch for details
#include "dolphin/types.h"
template <typename T> struct bss_3569 { u8 val[0xC]; };

static inline void dummy_bss_3569() {
    bss_3569<void*>(); // @3569
}
#else
#if TARGET_PC
// A quoted include looks next to this file first, so it would reach game/include/dolphin
// even when the SDK headers come from Aurora (COS_SDK_HEADERS=aurora); <> follows the -I order.
#include <dolphin/mtx/mtx.h>
#else
#include "dolphin/mtx/mtx.h"
#endif

static inline void dummy_bss_3569() {
    Vec vec = (Vec){0.0f, 0.0f, 0.0f}; // @3569 / @3599
}
#endif

#endif /* WEAK_BSS_3569_H */
