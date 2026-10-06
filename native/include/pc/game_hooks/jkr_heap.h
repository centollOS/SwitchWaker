#ifndef PC_GAME_HOOKS_JKR_HEAP_H
#define PC_GAME_HOOKS_JKR_HEAP_H

// The host additions to JKRHeap.h (step G3 of docs/GAME_CODE_ORGANIZATION.md moved them here);
// included by game/include/JSystem/JKernel/JKRHeap.h under TARGET_PC, after class JKRHeap.
// Defined in native/src/pc/game_hooks/pc_jkr_heap.cpp.

// Host allocation scope (pc_jkr_heap.cpp): while one is open on a thread, the global operator new
// forms that take no heap give that thread host memory instead of a block of the current heap.
// For host code with no GameCube counterpart (Aurora's frame work) that the game calls with a
// current heap set; operator delete already hands blocks no JKRHeap owns back to the host.
void JKRPcBeginHostAlloc();
void JKRPcEndHostAlloc();

// Allocation failure report (bug B8): JKRExpHeap::do_alloc and JKRSolidHeap::do_alloc call
// JKRPcReportAllocFailure when a block cannot be had, after the heap is unlocked; the PC layer
// installs the reporter (native/src/pc/harness/pc_heap.cpp: one log line with the heap's name, the size
// asked, its free total and largest free block). Many callers handle NULL themselves, so the report
// never stops the game.
typedef void (*JKRPcAllocFailureFn)(JKRHeap* heap, u32 size, int alignment);
void JKRPcSetAllocFailureReporter(JKRPcAllocFailureFn fn);
void JKRPcReportAllocFailure(JKRHeap* heap, u32 size, int alignment);

struct JKRPcHostAllocScope {
    JKRPcHostAllocScope() { JKRPcBeginHostAlloc(); }
    ~JKRPcHostAllocScope() { JKRPcEndHostAlloc(); }
    JKRPcHostAllocScope(const JKRPcHostAllocScope&) = delete;
    JKRPcHostAllocScope& operator=(const JKRPcHostAllocScope&) = delete;
};

#endif /* PC_GAME_HOOKS_JKR_HEAP_H */
