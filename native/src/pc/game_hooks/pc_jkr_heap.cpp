// The host side of JKRHeap.cpp, moved out of game/ (step G3 of docs/GAME_CODE_ORGANIZATION.md): the
// global operator new and delete, the host allocation scopes and the allocation failure report
// (declared in native/include/pc/game_hooks/jkr_heap.h, included by JKRHeap.h). The GameCube's
// operators stay in game/src/JSystem/JKernel/JKRHeap.cpp.
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTAssert.h"
#include <stddef.h>
#include <stdlib.h>

// Adapted from Dusklight (CC0, ref/dusklight/libs/JSystem/src/JKernel/JKRHeap.cpp, fallback_alloc
// and its operator new/delete). On PC the REL units are linked into the executable (phase 3.5),
// so their static constructors, and those of Aurora and the C++ runtime, run before main and
// before any JKRHeap exists, where JKRHeap::alloc returns NULL. With no heap to take the
// allocation (none given and no current heap) the global forms fall back to the host allocator,
// and operator delete hands a pointer that no JKRHeap owns back to it. Once main has made the
// root heap, everything behaves as on the GameCube.
// Host bookkeeping must not come here: freeing a JKRHeap block takes the heap's OSMutex, which the
// alarm thread may not, and game heaps are freed wholesale. cos_sdk's containers and records and
// the PC harness use host memory explicitly (native/sdk/include/cos_sdk/host_alloc.h).
// Aurora's frame work (aurora_update, aurora_begin_frame, aurora_end_frame; native/src/pc/
// runtime/pc_frame.cpp) runs inside a JKRPcHostAllocScope: the GameCube has no such code, and its
// allocations (the frame's pass list, render-worker jobs) would otherwise take blocks from whatever
// heap the game left current, failing when that heap is full and dangling when the game frees it.
// TODO(native phase 6): Aurora/SDL/libc++ allocations made inside the game's own SDK calls (GX,
// VI, PAD) still land in the current JKRHeap through these global forms; Dusklight moves the game
// to JKR_NEW instead.
static thread_local int sPcHostAllocDepth = 0;

void JKRPcBeginHostAlloc() {
    sPcHostAllocDepth++;
}

void JKRPcEndHostAlloc() {
    JUT_ASSERT(0, sPcHostAllocDepth > 0);
    sPcHostAllocDepth--;
}

static JKRPcAllocFailureFn sPcAllocFailureReporter = NULL;

void JKRPcSetAllocFailureReporter(JKRPcAllocFailureFn fn) {
    sPcAllocFailureReporter = fn;
}

void JKRPcReportAllocFailure(JKRHeap* heap, u32 size, int alignment) {
    if (sPcAllocFailureReporter != NULL) {
        sPcAllocFailureReporter(heap, size, alignment);
    }
}

static void* pc_heapless_alloc(size_t size, int alignment) {
    size_t align = alignment < 0 ? (size_t)-alignment : (size_t)alignment;
    if (align < alignof(max_align_t)) {
        align = alignof(max_align_t);
    }
    // aligned_alloc needs a power-of-two alignment and a size that is a multiple of it.
    JUT_ASSERT(0, (align & (align - 1)) == 0);
    size = (size + align - 1) & ~(align - 1);
    return aligned_alloc(align, size != 0 ? size : align);
}

static void* pc_new(size_t size, int alignment, JKRHeap* heap) {
    if (heap == NULL && (JKRHeap::getCurrentHeap() == NULL || sPcHostAllocDepth > 0)) {
        return pc_heapless_alloc(size, alignment);
    }
    return JKRHeap::alloc(size, alignment, heap);
}

static void pc_delete(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    JKRHeap* heap = JKRHeap::findFromRoot(ptr);
    if (heap == NULL) {
        ::free(ptr);
        return;
    }
    JKRHeap::free(ptr, heap);
}

// The forms without an alignment use 4 on the GameCube, enough for any type there. The host's
// operator new must return memory aligned for any type, __STDCPP_DEFAULT_NEW_ALIGNMENT__ (16 on
// arm64 and x86-64), and the compiler may assume it; as in Dusklight's operator new
// (alignof(max_align_t)). The forms with an alignment keep the one the game asks for.
static const int kPcNewAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

/* 802B0C38-802B0C60       .text __nw__FUl */
void* operator new(size_t size) {
    return pc_new(size, kPcNewAlignment, NULL);
}

/* 802B0C60-802B0C84       .text __nw__FUli */
void* operator new(size_t size, int alignment) {
    return pc_new(size, alignment, NULL);
}

/* 802B0C84-802B0CB0       .text __nw__FUlP7JKRHeapi */
void* operator new(size_t size, JKRHeap* heap, int alignment) {
    return pc_new(size, alignment, heap);
}

/* 802B0CB0-802B0CD8       .text __nwa__FUl */
void* operator new[](size_t size) {
    return pc_new(size, kPcNewAlignment, NULL);
}

/* 802B0CD8-802B0CFC       .text __nwa__FUli */
void* operator new[](size_t size, int alignment) {
    return pc_new(size, alignment, NULL);
}

/* 802B0CFC-802B0D28       .text __nwa__FUlP7JKRHeapi */
void* operator new[](size_t size, JKRHeap* heap, int alignment) {
    return pc_new(size, alignment, heap);
}

/* 802B0D28-802B0D4C       .text __dl__FPv */
void operator delete(void* ptr) noexcept {
    pc_delete(ptr);
}

/* 802B0D4C-802B0D70       .text __dla__FPv */
void operator delete[](void* ptr) noexcept {
    pc_delete(ptr);
}
