#ifndef JASSYSTEMHEAP_H
#define JASSYSTEMHEAP_H

#include "dolphin/types.h"

class JKRSolidHeap;

extern JKRSolidHeap* JASDram;

namespace JASystem {
    namespace Kernel {
        class TSolidHeap;

        void sysDramSetup(JKRSolidHeap*);
        void* allocFromSysDram(u32);
        void sysAramSetup(u32);
        void* allocFromSysAramFull(u32*);
#if TARGET_PC
        // The MEM1 physical address of `ptr`, for an address the DSP or the AI reads (they take
        // 32-bit physical addresses; a host pointer does not fit). Panics if `ptr` is not inside
        // Aurora's MEM1, where every JKR heap (and so JASDram) lives.
        u32 toPhysical(const void* ptr);
#endif

        extern TSolidHeap audioAramHeap;
        extern u32 audioDramSize;
        extern u32 audioAramSize;
        extern int audioAramTop;
        extern int CARD_SECURITY_BUFFER;
    }
}

#endif /* JASSYSTEMHEAP_H */
