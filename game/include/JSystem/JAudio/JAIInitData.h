#ifndef JAIINITDATA_H
#define JAIINITDATA_H

#include "dolphin/types.h"
#include "helpers/endian.h"

namespace JAInter {
    namespace InitData {
        BOOL checkInitDataFile();
        void checkInitDataOnMemory();

#if TARGET_PC
        // JaiInit.aaf in memory: big-endian words.
        extern BE(u32)* aafPointer;
#else
        extern u32* aafPointer;
#endif
    };
}

#endif /* JAIINITDATA_H */
