#ifndef PC_GAME_HOOKS_JKR_EXP_HEAP_POISON_H
#define PC_GAME_HOOKS_JKR_EXP_HEAP_POISON_H

// Included by game/src/JSystem/JKernel/JKRExpHeap.cpp only, under TARGET_PC (step G3 of
// docs/GAME_CODE_ORGANIZATION.md moved this here). Defines nothing outside an ASan build; there it
// defines COS_JKR_POISON, which JKRExpHeap.cpp tests, and turns instrumentation off for the rest
// of that file.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
// COS_ASAN build (native/CMakeLists.txt): ASan sees host malloc only, and the game's JKR heaps are
// one malloc'd arena each, so a write through a pointer to a freed JKR block (an actor deleted, its
// solid heap given back) goes unseen and damages whatever is allocated there next. Here the
// payload of every block the heap frees is poisoned and every block it hands out unpoisoned for the
// size asked for, so ASan reports such an access where it happens (and an overrun past the size
// asked for). The heap's own code reads and writes block headers inside freed areas (splitting and
// joining blocks), so this file is not instrumented, and every header it builds is unpoisoned, so
// code elsewhere that walks the lists (pc_heap.cpp) reads them freely. COS_JKR_POISON=0 turns
// the poisoning off.
#include <sanitizer/asan_interface.h>
#include <stdlib.h>
#define COS_JKR_POISON 1
#pragma clang attribute push(__attribute__((no_sanitize("address"))), apply_to = function)
static bool jkrPoisonOn() {
    static int on = -1;
    if (on < 0) {
        const char* env = getenv("COS_JKR_POISON");
        on = (env != NULL && env[0] == '0') ? 0 : 1;
    }
    return on != 0;
}
static void jkrPoison(void* p, uintptr_t size) {
    // Whole 8-byte shadow granules inside the payload only: a partly poisoned granule at the end
    // would also cover the start of the next block's header.
    const uintptr_t begin = ((uintptr_t)p + 7) & ~(uintptr_t)7;
    const uintptr_t end = ((uintptr_t)p + size) & ~(uintptr_t)7;
    if (jkrPoisonOn() && end > begin) {
        ASAN_POISON_MEMORY_REGION((void*)begin, end - begin);
    }
}
static void jkrUnpoison(void* p, uintptr_t size) {
    if (size != 0) {
        ASAN_UNPOISON_MEMORY_REGION(p, size);
    }
}
#endif
#endif

#endif /* PC_GAME_HOOKS_JKR_EXP_HEAP_POISON_H */
