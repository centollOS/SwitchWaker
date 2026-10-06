// The host side of JKRThread.cpp and JKRArchivePri.cpp, moved out of game/ (step G3 of
// docs/GAME_CODE_ORGANIZATION.md): the lock around JKRThread's thread list and JKRArchive's side
// table of resource pointers (JKAR_DATA). JKRHeap's host side is in pc_jkr_heap.cpp.
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRThread.h"
#include "pc/game_hooks/jkr_thread.h"
#include "dolphin/os/OS.h"
#include <mutex>
#include <string.h>

// ---- JKRThread.cpp ----------------------------------------------------------------------------


namespace {
// sThreadList is linked from every thread that builds a JKRThread. On the GameCube those threads
// never ran at the same time; here they are host threads: the four JUTGba threads each build and
// drop one at start-up (gbaThreadMain), together with the audio thread's and the main thread's,
// and the unguarded appends and removes raced. One wrote its link into another's JKRThread after
// that one had gone out of scope (ASan, stack-use-after-scope in JSUPtrList::append), and a corrupt
// list faulted in JSUPtrList::remove at boot (SIGSEGV, frame 0). Never destroyed: a JKRThread may
// still unlink itself during exit.
std::mutex& threadListMutex() {
    static std::mutex* mutex = new std::mutex;
    return *mutex;
}
} // namespace

void JKRPcThreadListAppend(JSULink<JKRThread>* link) {
    std::lock_guard<std::mutex> lock(threadListMutex());
    JKRThread::getList().append(link);
}

void JKRPcThreadListRemove(JSULink<JKRThread>* link) {
    std::lock_guard<std::mutex> lock(threadListMutex());
    JKRThread::getList().remove(link);
}

// ---- JKRArchivePri.cpp ------------------------------------------------------------------------

// The side table of the resource pointers (JKAR_DATA), after Dusklight's JKRArchivePri.cpp (CC0,
// ref/dusklight at 40457c6), which allocates it from the system heap. Here it comes from the
// archive's heap, where the GameCube kept these pointers (inside the file table it loaded there):
// its size follows the archive, and the 64 KiB the system heap keeps beside the zelda heap stay
// for what the game puts there. A failed allocation fails the mount like the other allocations of
// open().
void*& JKRArchive::getFileDataPointer(u32 index) const {
    if (mFileData == NULL || mArcInfoBlock == NULL || index >= mArcInfoBlock->num_file_entries) {
        OSPanic(__FILE__, __LINE__, "JKRArchive: file entry index %u out of range (%u entries)",
                index, mArcInfoBlock != NULL ? (u32)mArcInfoBlock->num_file_entries : 0);
    }
    return mFileData[index];
}

// Called by every open() once mArcInfoBlock and mFiles point at the loaded file table: numbers the
// entries and clears their resource pointers (the GameCube's `data` fields are 0 in the file).
bool JKRArchive::initFileDataPointers() {
    if (mFileData != NULL) {
        JKRFree(mFileData);
        mFileData = NULL;
    }

    u32 count = mArcInfoBlock->num_file_entries;
    int alignment = mMountDirection == MOUNT_DIRECTION_TAIL ? -(int)sizeof(void*) : (int)sizeof(void*);
    mFileData = (void**)JKRAllocFromHeap(mHeap, count != 0 ? count * sizeof(void*) : sizeof(void*),
                                         alignment);
    if (mFileData == NULL) {
        return false;
    }

    memset(mFileData, 0, count * sizeof(void*));
    for (u32 i = 0; i < count; i++) {
        mFiles[i].index = i;
    }
    return true;
}
