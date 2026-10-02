#ifndef OSALLOC_H
#define OSALLOC_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSHeapDescriptor {
    /* 0x0 */ s32 size;
    /* 0x4 */ struct OSHeapCell* free;
    /* 0x8 */ struct OSHeapCell* allocated;
} OSHeapDescriptor;

typedef struct OSHeapCell {
    /* 0x00 */ struct OSHeapCell* prev;
    /* 0x04 */ struct OSHeapCell* next;
    /* 0x08 */ u32 size;
    /* 0x0C */ struct OSHeapDescriptor* hd;
    /* 0x10 */ u32 usedSize;
    /* 0x14 */ char field_0x14[0x20 - 0x14];
} OSHeapCell;

typedef s32 OSHeapHandle;

extern volatile s32 __OSCurrHeap;

#if TARGET_PC
// Pointers are 64-bit on the host: round them through uintptr_t (as Dusklight's dolphin/os.h).
#define OSRoundUp32B(x) (((uintptr_t)(x) + 0x1F) & ~(0x1F))
#define OSRoundDown32B(x) (((uintptr_t)(x)) & ~(0x1F))
#else
#define OSRoundUp32B(x) (((u32)(x) + 0x1F) & ~(0x1F))
#define OSRoundDown32B(x) (((u32)(x)) & ~(0x1F))
#endif

#define OSRoundUp(x, align) (((x) + (align)-1) & (-(align)))
#if TARGET_PC
#define OSRoundUpPtr(x, align) ((void*)((((uintptr_t)(x)) + (align)-1) & (~((align)-1))))
#else
#define OSRoundUpPtr(x, align) ((void*)((((u32)(x)) + (align)-1) & (~((align)-1))))
#endif

#define OSRoundDown(x, align) ((x) & (-(align)))
#if TARGET_PC
#define OSRoundDownPtr(x, align) ((void*)(((uintptr_t)(x)) & (~((align)-1))))
#else
#define OSRoundDownPtr(x, align) ((void*)(((u32)(x)) & (~((align)-1))))
#endif

static OSHeapCell* DLInsert(OSHeapCell* list, OSHeapCell* child);
void* OSAllocFromHeap(OSHeapHandle handle, u32 size);
void OSFreeToHeap(OSHeapHandle handle, void* ptr);
s32 OSSetCurrentHeap(OSHeapHandle handle);
void* OSInitAlloc(void* lo, void* hi, s32 maxHeaps);
OSHeapHandle OSCreateHeap(void* start, void* end);
void OSDestroyHeap(OSHeapHandle handle);
s32 OSCheckHeap(OSHeapHandle handle);
s32 OSReferentSize(void* ptr);
void OSDumpHeap(OSHeapHandle handle);

#ifdef __cplusplus
};
#endif

#endif /* OSALLOC_H */
