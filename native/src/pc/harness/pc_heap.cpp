// JKR heaps on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.2): the COS_SMOKE=heap test and the
// check behind milestone M2 (heaps).
//
// - heap (smoke, runs right after pc_aurora_init, before the game's main code): before any heap
//   exists the global operator new hands out host memory (the fallback of JKRHeap.cpp); then the
//   root heap is made by JKRExpHeap::createRoot (JKRHeap::initArena over Aurora's MEM1, which must
//   hold it whole), and below it the heaps mDoMch_Create makes, with the PC sizes of decision H5:
//   system 32 MiB, main (inside system), command, archive and game. In each exp heap 10,000
//   random allocations (sizes, head and tail alignments), frees and resizes; every block must lie
//   inside its heap, be aligned as asked, keep its contents and leave check() true; once all is
//   freed the heap must be back to its first free size, and freeAll must leave one free block
//   over the whole heap, all of which getMaxAllocatableSize(0x10) offers. Then freeTail, which
//   must dispose the objects in tail blocks and only those; a solid heap in the game heap (head
//   and tail, every alignment); operator new in the current heap (aligned to
//   __STDCPP_DEFAULT_NEW_ALIGNMENT__, owned by it). Exit 0 when every check holds, 1 otherwise.
// - M2: pc_heaps_created, called by main01 right after mDoMch_Create returned, runs check() on the
//   root, system, main, game, archive and command heaps and logs the milestone if all hold;
//   then a COS_SMOKE test that needs the game's heaps (font, step 4.3) runs.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"

#include <aurora/aurora.h>
#include <dolphin/os.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#if defined(__APPLE__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#endif
#include <cstring>
#include <new>
#include <unistd.h>

#if defined(__SWITCH__)
#include <malloc.h>
#endif

namespace pc {

namespace {

constexpr int kOps = 10000;
constexpr int kMaxLive = 512;
// Per block, at most: its header and the gap an alignment of 0x80 can leave before it.
constexpr uint32_t kOverhead = sizeof(JKRExpHeap::CMemBlock) + 0x80;

int sErrors = 0;

void failMessage(const char* heap, const char* message) {
    if (sErrors < 50) {
        writef(STDERR_FILENO, "[cos] heap: %s: %s\n", heap, message);
    }
    sErrors++;
}

// FAIL(heap, printf format, arguments...): one error, reported (the first 50 of them).
#define FAIL(heap, ...)                                                                            \
    do {                                                                                           \
        char failText[256];                                                                        \
        snprintf(failText, sizeof(failText), __VA_ARGS__);                                         \
        failMessage(heap, failText);                                                               \
    } while (0)

// xorshift64*: the same sequence on every run.
uint64_t sRng = 0x9E3779B97F4A7C15ull;
uint32_t rnd() {
    sRng ^= sRng >> 12;
    sRng ^= sRng << 25;
    sRng ^= sRng >> 27;
    return (uint32_t)((sRng * 0x2545F4914F6CDD1Dull) >> 32);
}
uint32_t rnd(uint32_t n) {
    return rnd() % n;
}

struct Block {
    uint8_t* ptr;
    uint32_t size;
    uint8_t fill;
};

bool inside(JKRHeap* heap, const void* p, uint32_t size) {
    uintptr_t a = (uintptr_t)p;
    return a >= (uintptr_t)heap->getStartAddr() && a + size <= (uintptr_t)heap->getEndAddr();
}

void fillBlock(Block& b) {
    memset(b.ptr, b.fill, b.size);
}

bool blockIntact(const Block& b, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        if (b.ptr[i] != b.fill) {
            return false;
        }
    }
    return true;
}

// Alignments the game passes: 0 and 4 (none), powers of two up to 0x80, negative from the tail.
const int kAlignments[] = {0, 4, 8, 16, 32, 64, 128, -4, -8, -16, -32, -64};

// What JKRExpHeap::create(-1, heap) and JKRSolidHeap::create(-1, heap) take: the size
// getMaxAllocatableSize(0x10) promises (it assumes a block's content has the block's address
// modulo 16) must be allocatable with that alignment.
void checkMaxAllocatable(const char* name, JKRExpHeap* heap) {
    u32 maxAlloc = heap->getMaxAllocatableSize(0x10);
    if (maxAlloc == 0 || maxAlloc > 0x7FFFFFFF) {
        return;
    }
    // Without the error handler: a refusal is counted here instead of ending the run.
    bool errorFlag = heap->setErrorFlag(false);
    void* all = heap->alloc(maxAlloc, 0x10);
    heap->setErrorFlag(errorFlag);
    if (all == nullptr) {
        FAIL(name, "getMaxAllocatableSize(0x10) is 0x%x, which alloc refuses", (unsigned)maxAlloc);
    } else {
        heap->free(all);
    }
}

// 10,000 random alloc/free/resize operations in heap, then everything freed.
void exerciseExpHeap(const char* name, JKRExpHeap* heap) {
    const uint32_t heapSize = heap->getHeapSize();
    // Keep well below the free size, so a NULL is a heap error and not a full heap.
    const uint32_t budget = (uint32_t)heap->getTotalFreeSize() / 2;
    uint32_t maxSize = heapSize / 64;
    if (maxSize > 256 * 1024) {
        maxSize = 256 * 1024;
    }
    const s32 freeBefore = heap->getTotalFreeSize();
    const s32 maxFreeBefore = heap->getFreeSize();

    static Block live[kMaxLive];
    int liveCount = 0;
    uint32_t used = 0;
    int allocs = 0, frees = 0, resizes = 0, tails = 0;
    uint8_t nextFill = 1;

    for (int op = 0; op < kOps; op++) {
        uint32_t kind = rnd(100);
        if (liveCount > 0 && (kind < 40 || liveCount == kMaxLive)) {
            int i = (int)rnd((uint32_t)liveCount);
            Block b = live[i];
            if (!blockIntact(b, b.size)) {
                FAIL(name, "block %p (0x%x bytes) was overwritten", (void*)b.ptr, (unsigned)b.size);
            }
            heap->free(b.ptr);
            used -= b.size + kOverhead;
            live[i] = live[--liveCount];
            frees++;
        } else if (liveCount > 0 && kind < 45) {
            int i = (int)rnd((uint32_t)liveCount);
            Block& b = live[i];
            uint32_t newSize = 1 + rnd(b.size * 2 < maxSize ? b.size * 2 : maxSize);
            if (used + newSize + kOverhead > budget) {
                continue;
            }
            s32 got = heap->resize(b.ptr, newSize);
            resizes++;
            if (got == -1) {
                continue; // no free neighbour: allowed
            }
            if ((uint32_t)got < newSize) {
                FAIL(name, "resize to 0x%x returned 0x%x", (unsigned)newSize, (unsigned)got);
                continue;
            }
            uint32_t keep = b.size < newSize ? b.size : newSize;
            if (!blockIntact(b, keep)) {
                FAIL(name, "resize of %p lost the contents", (void*)b.ptr);
            }
            used = used - b.size + newSize;
            b.size = newSize;
            fillBlock(b);
        } else {
            uint32_t r = rnd(100);
            uint32_t size = r < 70 ? 1 + rnd(256) : r < 95 ? 257 + rnd(4096 - 256) : 1 + rnd(maxSize);
            if (size > maxSize) {
                size = 1 + rnd(maxSize);
            }
            if (used + size + kOverhead > budget) {
                continue;
            }
            int align = kAlignments[rnd(sizeof(kAlignments) / sizeof(kAlignments[0]))];
            void* p = heap->alloc(size, align);
            if (p == nullptr) {
                FAIL(name, "alloc(0x%x, %d) returned NULL", (unsigned)size, align);
                continue;
            }
            allocs++;
            if (align < 0) {
                tails++;
            }
            uint32_t mag = (uint32_t)(align < 0 ? -align : align);
            if (mag < 4) {
                mag = 4;
            }
            if ((uintptr_t)p % mag != 0) {
                FAIL(name, "alloc returned %p, not aligned to %u", p, (unsigned)mag);
            }
            if (!inside(heap, p, size)) {
                FAIL(name, "alloc returned %p (0x%x bytes) outside the heap", p, (unsigned)size);
                continue;
            }
            s32 blockSize = heap->getSize(p);
            if (blockSize < (s32)size) {
                FAIL(name, "getSize is 0x%x for 0x%x bytes", (unsigned)blockSize, (unsigned)size);
            }
            if (JKRHeap::findFromRoot(p) != heap) {
                FAIL(name, "findFromRoot(%p) is not the heap", p);
            }
            Block b = {(uint8_t*)p, size, nextFill};
            nextFill = (uint8_t)(nextFill == 0xFF ? 1 : nextFill + 1);
            fillBlock(b);
            live[liveCount++] = b;
            used += size + kOverhead;
        }
        if (op % 500 == 499 && !heap->check()) {
            FAIL(name, "check() false after %d operations", op + 1);
        }
        if (op % 50 == 49) {
            checkMaxAllocatable(name, heap);
        }
    }

    while (liveCount > 0) {
        Block b = live[--liveCount];
        if (!blockIntact(b, b.size)) {
            FAIL(name, "block %p (0x%x bytes) was overwritten", (void*)b.ptr, (unsigned)b.size);
        }
        heap->free(b.ptr);
    }
    if (!heap->check()) {
        FAIL(name, "check() false once everything was freed");
    }
    if (heap->getTotalFreeSize() != freeBefore || heap->getFreeSize() != maxFreeBefore) {
        FAIL(name, "free size 0x%x after the test, 0x%x before", (unsigned)heap->getTotalFreeSize(),
             (unsigned)freeBefore);
    }
    writef(STDERR_FILENO,
           "[cos] heap: %-8s %p..%p size 0x%08x: %d allocs (%d from the tail), %d frees, %d resizes\n",
           name, heap->getStartAddr(), heap->getEndAddr(), (unsigned)heapSize, allocs, tails, frees,
           resizes);
}

// freeAll on a heap with no child heap: one free block over the whole heap afterwards.
void checkFreeAll(const char* name, JKRExpHeap* heap) {
    uint32_t maxSize = heap->getHeapSize() / 128 < 1024 ? heap->getHeapSize() / 128 : 1024;
    for (int i = 0; i < 16; i++) {
        heap->alloc(16 + rnd(maxSize), kAlignments[rnd(sizeof(kAlignments) / sizeof(kAlignments[0]))]);
    }
    heap->freeAll();
    s32 expected = (s32)(heap->getHeapSize() - sizeof(JKRExpHeap::CMemBlock));
    if (!heap->check()) {
        FAIL(name, "check() false after freeAll");
    }
    if (heap->getTotalFreeSize() != expected || heap->getFreeSize() != expected) {
        FAIL(name, "free size 0x%x after freeAll, expected 0x%x", (unsigned)heap->getTotalFreeSize(),
             (unsigned)expected);
    }
    checkMaxAllocatable(name, heap);
}

void checkSolidHeap(JKRHeap* parent) {
    const char* name = "solid";
    JKRSolidHeap* heap = JKRSolidHeap::create(0x10000, parent, false);
    if (heap == nullptr) {
        FAIL(name, "JKRSolidHeap::create failed");
        return;
    }
    int allocs = 0;
    for (int round = 0; round < 2; round++) {
        for (;;) {
            int align = kAlignments[rnd(sizeof(kAlignments) / sizeof(kAlignments[0]))];
            uint32_t size = 1 + rnd(512);
            if ((uint32_t)heap->getFreeSize() < size + 0x100) {
                break;
            }
            void* p = heap->alloc(size, align);
            if (p == nullptr) {
                FAIL(name, "alloc(0x%x, %d) returned NULL", (unsigned)size, align);
                break;
            }
            allocs++;
            uint32_t mag = (uint32_t)(align < 0 ? -align : align);
            if (mag < 4) {
                mag = 4;
            }
            if ((uintptr_t)p % mag != 0) {
                FAIL(name, "alloc returned %p, not aligned to %u", p, (unsigned)mag);
            }
            if (!inside(heap, p, size)) {
                FAIL(name, "alloc returned %p (0x%x bytes) outside the heap", p, (unsigned)size);
                break;
            }
            memset(p, 0xA5, size);
        }
        if (!heap->check()) {
            FAIL(name, "check() false when full (round %d)", round);
        }
        heap->freeAll();
        if (!heap->check() || (u32)heap->getFreeSize() != heap->getHeapSize()) {
            FAIL(name, "freeAll left 0x%x of 0x%x free", (unsigned)heap->getFreeSize(),
                 (unsigned)heap->getHeapSize());
        }
    }
    heap->destroy();
    writef(STDERR_FILENO, "[cos] heap: solid heap in the game heap: %d allocs\n", allocs);
}

// A disposer that counts its destruction.
struct ProbeDisposer : public JKRDisposer {
    explicit ProbeDisposer(int* gone) : mGone(gone) {}
    virtual ~ProbeDisposer() { (*mGone)++; }
    int* mGone;
};

// freeTail disposes the objects in the blocks taken from the tail (JKRHeap::dispose over their
// address range), and only those.
void checkFreeTail(const char* name, JKRExpHeap* heap) {
    int headGone = 0, tailGone = 0;
    void* head = heap->alloc(sizeof(ProbeDisposer), 16);
    void* tail = heap->alloc(sizeof(ProbeDisposer), -16);
    if (head == nullptr || tail == nullptr) {
        FAIL(name, "no room for the freeTail probes");
        return;
    }
    ProbeDisposer* kept = new (head) ProbeDisposer(&headGone);
    new (tail) ProbeDisposer(&tailGone);
    heap->freeTail();
    if (tailGone != 1 || headGone != 0) {
        FAIL(name, "freeTail disposed %d tail and %d head object(s), expected 1 and 0", tailGone, headGone);
    }
    if (!heap->check()) {
        FAIL(name, "check() false after freeTail");
    }
    kept->~ProbeDisposer();
    heap->free(head);
}

// 24 bytes: with only 4-byte alignment, consecutive objects would sit 8 bytes off 16.
struct NewProbe {
    void* p;
    double d;
    u32 n;
};

void checkOperatorNew(JKRExpHeap* heap) {
    const char* name = "new";
    JKRHeap* old = JKRSetCurrentHeap(heap);
    s32 freeBefore = heap->getTotalFreeSize();
    NewProbe* objects[32];
    for (int i = 0; i < 32; i++) {
        objects[i] = new NewProbe();
        if ((uintptr_t)objects[i] % __STDCPP_DEFAULT_NEW_ALIGNMENT__ != 0) {
            FAIL(name, "operator new returned %p, not aligned to %u", (void*)objects[i],
                 (unsigned)__STDCPP_DEFAULT_NEW_ALIGNMENT__);
        }
        if (JKRHeap::findFromRoot(objects[i]) != heap) {
            FAIL(name, "operator new returned %p, not from the current heap", (void*)objects[i]);
        }
    }
    for (int i = 0; i < 32; i++) {
        delete objects[i];
    }
    if (heap->getTotalFreeSize() != freeBefore) {
        FAIL(name, "delete left 0x%x free, 0x%x before", (unsigned)heap->getTotalFreeSize(),
             (unsigned)freeBefore);
    }
    JKRSetCurrentHeap(old);
}

} // namespace

[[noreturn]] void smokeHeap() {
    // Before any heap: the global operator new falls back to the host allocator.
    if (JKRHeap::getRootHeap() != nullptr || JKRHeap::getCurrentHeap() != nullptr) {
        FAIL("start", "a heap exists before the test made one");
    }
    NewProbe* early = new NewProbe();
    if ((uintptr_t)early % __STDCPP_DEFAULT_NEW_ALIGNMENT__ != 0) {
        FAIL("start", "operator new without a heap returned %p", (void*)early);
    }
    delete early;

    // The root heap over the arena, as JFWSystem::firstInit makes it (maxStdHeaps 1, set by
    // mDoMch_Create), and inside Aurora's MEM1 (finding 4: u32 physical addresses stay valid).
    JKRExpHeap* root = JKRExpHeap::createRoot(1, false);
    uintptr_t mem1 = (uintptr_t)OSPhysicalToCached(0);
    uintptr_t mem1End = mem1 + ((OSBootInfo*)OSPhysicalToCached(0))->memorySize;
    if (root == nullptr || !((uintptr_t)root >= mem1 && (uintptr_t)root->getEndAddr() <= mem1End)) {
        FAIL("root", "root heap %p is not inside MEM1 %p", (void*)root, (void*)mem1);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    if ((uintptr_t)root->getStartAddr() % 0x10 != 0) {
        FAIL("root", "root heap data %p is not 16-byte aligned", root->getStartAddr());
    }
    writef(STDERR_FILENO, "[cos] heap: MEM1 %p..%p, root heap %p..%p (0x%08x bytes)\n", (void*)mem1,
           (void*)mem1End, root->getStartAddr(), root->getEndAddr(), (unsigned)root->getHeapSize());

    // The heaps of mDoMch_Create with their PC sizes (decision H5; m_Do_machine.cpp, USA).
    JKRExpHeap* system = JKRExpHeap::create(32u * 1024 * 1024, root, false);
    JKRExpHeap* command = mDoExt_createCommandHeap(0x1000 * 2, root);
    JKRExpHeap* archive = mDoExt_createArchiveHeap(0xA51400 * 2, root);
    JKRExpHeap* game = mDoExt_createGameHeap(0x2CE800 * 20, root);
    JKRExpHeap* main =
        system != nullptr ? mDoExt_createZeldaHeap(system->getFreeSize() - 0x10000, system) : nullptr;
    if (system == nullptr || command == nullptr || archive == nullptr || game == nullptr || main == nullptr) {
        FAIL("create", "a heap could not be made in the root heap");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }

    struct {
        const char* name;
        JKRExpHeap* heap;
    } heaps[] = {{"main", main}, {"command", command}, {"archive", archive}, {"game", game},
                 {"system", system}};
    for (auto& h : heaps) {
        exerciseExpHeap(h.name, h.heap);
    }
    checkFreeTail("main", main);
    checkSolidHeap(game);
    checkOperatorNew(main);

    // freeAll on the leaves; the main heap goes first so the system heap has no child left.
    for (auto& h : heaps) {
        if (h.heap == system) {
            main->destroy();
            if (!system->check()) {
                FAIL("system", "check() false after the main heap was destroyed");
            }
        }
        if (h.heap != main) {
            checkFreeAll(h.name, h.heap);
        }
    }
    if (!root->check()) {
        FAIL("root", "check() false at the end");
    }

    writef(STDERR_FILENO, "[cos] heap: %d error(s)\n", sErrors);
    pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

// COS_HEAP_CHECK (bug B4): check() on every heap of the tree below `heap`; the first heap that fails
// is returned (nullptr: all hold).
static JKRHeap* checkHeapTree(JKRHeap* heap, unsigned int* count) {
    (*count)++;
    if (!heap->check()) {
        return heap;
    }
    JSUTree<JKRHeap>& tree = heap->getHeapTree();
    for (JSUTreeIterator<JKRHeap> it(tree.getFirstChild()); it != tree.getEndChild(); ++it) {
        if (JKRHeap* bad = checkHeapTree(it.getObject(), count)) {
            return bad;
        }
    }
    return nullptr;
}

void heapCheckFrame(unsigned int frame) {
    JKRHeap* root = JKRHeap::getRootHeap();
    if (root == nullptr) {
        return;
    }
    static bool sAnnounced = false;
    unsigned int count = 0;
    JKRHeap* bad = checkHeapTree(root, &count);
#if defined(__SWITCH__)
    // newlib's malloc (the host heap on the Switch: Aurora, Dawn, Mesa, the C++ runtime): mallinfo
    // walks every free list, so a free chunk whose links were overwritten faults here, on the game
    // thread with "heapCheckFrame" in the backtrace, at the first check after the damage, instead
    // of in some later malloc on any thread (bug B4).
    const struct mallinfo info = mallinfo();
#endif
    if (!sAnnounced) {
        sAnnounced = true;
#if defined(__SWITCH__)
        writef(STDERR_FILENO, "[cos] heap-check: COS_HEAP_CHECK=%u, first check at frame %u: %u JKR heaps, "
                              "newlib arena %zu bytes (%zu in use, %zu free chunks)\n",
               gConfig.heapCheckEvery, frame, count, (size_t)info.arena, (size_t)info.uordblks,
               (size_t)info.ordblks);
#else
        writef(STDERR_FILENO, "[cos] heap-check: COS_HEAP_CHECK=%u, first check at frame %u: %u JKR heaps\n",
               gConfig.heapCheckEvery, frame, count);
#endif
    }
    if (bad != nullptr) {
        writef(STDERR_FILENO, "[cos] heap-check: frame %u: heap %p (type 0x%08x, %p..%p) fails check() "
                              "(the JUTWarning lines above say where)\n",
               frame, (void*)bad, (unsigned)bad->getHeapType(), bad->getStartAddr(),
               bad->getEndAddr());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
}

// Bug B8: one line per failed JKR allocation (the first 64, then every 256th), so a later assert
// or a NULL dereference can be tied to the heap that ran out. The game's heaps by name; any other
// heap by its type and the nearest named ancestor.
static const char* heapName(JKRHeap* heap) {
    if (heap == nullptr) {
        return "none";
    }
    if (heap == JKRHeap::getRootHeap()) {
        return "root";
    }
    if (heap == JKRHeap::getSystemHeap()) {
        return "system";
    }
    if (heap == mDoExt_getZeldaHeap()) {
        return "main";
    }
    if (heap == mDoExt_getGameHeap()) {
        return "game";
    }
    if (heap == mDoExt_getArchiveHeap()) {
        return "archive";
    }
    if (heap == mDoExt_getCommandHeap()) {
        return "command";
    }
    if (heap == (JKRHeap*)dComIfGp_getExpHeap2D()) {
        return "2D (dComIfGp_getExpHeap2D)";
    }
    return nullptr;
}

static void describeHeap(JKRHeap* heap, char* out, size_t outSize) {
    if (const char* name = heapName(heap)) {
        snprintf(out, outSize, "%s heap %p", name, (void*)heap);
        return;
    }
    const u32 type = heap->getHeapType();
    const char t[5] = {(char)(type >> 24), (char)(type >> 16), (char)(type >> 8), (char)type, 0};
    const char* ancestor = "none";
    int depth = 0;
    for (JKRHeap* p = heap->getParent(); p != nullptr; p = p->getParent()) {
        depth++;
        if (const char* name = heapName(p)) {
            ancestor = name;
            break;
        }
    }
    snprintf(out, outSize, "%s heap %p (%d level(s) below %s)", t, (void*)heap, depth, ancestor);
}

static unsigned int sAllocFailures = 0;

static void* actorWithHeap(void* proc, void* heap) {
    return ((fopAc_ac_c*)proc)->heap == heap ? proc : nullptr;
}

// The actor whose own heap (fopAc_ac_c::heap, fopAcM_entrySolidHeap) heap or the current heap is:
// " (actor fpcNm 0x...)" or "". A failure the game survives in an actor's heap has no backtrace on
// the console; the process name says which actor's heap is too small.
static void describeOwner(JKRHeap* heap, char* out, size_t outSize) {
    out[0] = '\0';
    if (heapName(heap) != nullptr) {
        return;
    }
    fopAc_ac_c* actor = fopAcM_Search(actorWithHeap, heap);
    if (actor == nullptr && JKRHeap::getCurrentHeap() != heap) {
        actor = fopAcM_Search(actorWithHeap, JKRHeap::getCurrentHeap());
    }
    if (actor != nullptr) {
        snprintf(out, outSize, "; actor fpcNm 0x%04X (process %u, room %d)", (unsigned)fopAcM_GetName(actor),
                 (unsigned)fopAcM_GetID(actor), (int)fopAcM_GetRoomNo(actor));
    }
}

static void reportAllocFailure(JKRHeap* heap, u32 size, int alignment) {
    const unsigned int sCount = ++sAllocFailures;
    if (sCount > 64 && sCount % 256 != 0) {
        return;
    }
    char name[96];
    describeHeap(heap, name, sizeof(name));
    char current[96];
    describeHeap(JKRHeap::getCurrentHeap(), current, sizeof(current));
    char owner[96];
    describeOwner(heap, owner, sizeof(owner));
    writef(STDERR_FILENO,
           "[cos] heap: cannot alloc 0x%x bytes (align %d) in the %s: size 0x%x, free 0x%x, largest "
           "free block 0x%x; current %s%s; frame %u, failure #%u\n",
           (unsigned)size, alignment, name, (unsigned)heap->getHeapSize(),
           (unsigned)heap->getTotalFreeSize(), (unsigned)heap->getFreeSize(), current, owner,
           pc_frame_count(), sCount);
#if defined(__APPLE__)
    // The callers (room sweep: a failure the game survives has no crash backtrace to tell whose
    // fixed-size heap is too small for host-sized objects, bugs B8 and B12). The first 16 only.
    if (sCount <= 16) {
        void* frames[12];
        const int count = backtrace(frames, 12);
        char line[1024];
        int n = snprintf(line, sizeof(line), "[cos] heap:   from");
        for (int i = 2; i < count && n < (int)sizeof(line) - 2; i++) {
            Dl_info info;
            const char* sym = nullptr;
            char* demangled = nullptr;
            if (dladdr(frames[i], &info) != 0 && info.dli_sname != nullptr) {
                int status = 0;
                demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
                sym = demangled != nullptr ? demangled : info.dli_sname;
            }
            char shortName[160];
            snprintf(shortName, sizeof(shortName), "%s", sym != nullptr ? sym : "?");
            if (char* paren = strchr(shortName, '(')) {
                *paren = '\0';
            }
            n += snprintf(line + n, sizeof(line) - n, "%s %s", i == 2 ? "" : " <-", shortName);
            free(demangled);
        }
        if (n >= (int)sizeof(line)) {
            n = (int)sizeof(line) - 2;
        }
        line[n++] = '\n';
        write(STDERR_FILENO, line, n);
    }
#endif
}

unsigned int heapAllocFailures() {
    return sAllocFailures;
}

void heapReport(const char* why) {
    if (JKRHeap::getRootHeap() == nullptr) {
        return;
    }
    struct {
        const char* name;
        JKRHeap* heap;
    } heaps[] = {{"root", JKRHeap::getRootHeap()},       {"system", JKRHeap::getSystemHeap()},
                 {"main", mDoExt_getZeldaHeap()},        {"game", mDoExt_getGameHeap()},
                 {"archive", mDoExt_getArchiveHeap()},    {"command", mDoExt_getCommandHeap()}};
    char line[1024];
    int n = snprintf(line, sizeof(line), "[cos] heaps: frame %u (%s), free/largest KiB:", pc_frame_count(), why);
    for (auto& h : heaps) {
        if (h.heap == nullptr || n >= (int)sizeof(line)) {
            continue;
        }
        n += snprintf(line + n, sizeof(line) - n, " %s %u/%u", h.name, (unsigned)h.heap->getTotalFreeSize() / 1024,
                      (unsigned)h.heap->getFreeSize() / 1024);
    }
    // The exp heaps made inside the game heap (fopMsgM_createExpHeap: the 2D heap, the meter's,
    // the message window's...) and their exp children (the menu window's child heap): the actors'
    // solid heaps are left out.
    auto expChildren = [&](JKRHeap* parent, const char* label) {
        JSUTree<JKRHeap>& tree = parent->getHeapTree();
        for (JSUTreeIterator<JKRHeap> it(tree.getFirstChild()); it != tree.getEndChild() && n < (int)sizeof(line); ++it) {
            JKRHeap* child = it.getObject();
            if (child->getHeapType() != 'EXPH') {
                continue;
            }
            n += snprintf(line + n, sizeof(line) - n, " %s(%uK) %u/%u", child == (JKRHeap*)dComIfGp_getExpHeap2D() ? "2D" : label,
                          (unsigned)child->getHeapSize() / 1024, (unsigned)child->getTotalFreeSize() / 1024,
                          (unsigned)child->getFreeSize() / 1024);
        }
    };
    if (JKRHeap* game = mDoExt_getGameHeap()) {
        expChildren(game, "game-exp");
    }
    if (JKRHeap* heap2D = (JKRHeap*)dComIfGp_getExpHeap2D()) {
        expChildren(heap2D, "2D-child");
    }
    if (n < (int)sizeof(line)) {
        char current[96];
        describeHeap(JKRHeap::getCurrentHeap(), current, sizeof(current));
        snprintf(line + n, sizeof(line) - n, "; current %s\n", current);
    }
    writef(STDERR_FILENO, "%s", line);
}

} // namespace pc

using namespace pc;

extern "C" void pc_heaps_created(void) {
    JKRPcSetAllocFailureReporter(reportAllocFailure);
    struct {
        const char* name;
        JKRHeap* heap;
    } heaps[] = {{"root", JKRHeap::getRootHeap()},  {"system", JKRHeap::getSystemHeap()},
                 {"main", mDoExt_getZeldaHeap()},  {"game", mDoExt_getGameHeap()},
                 {"archive", mDoExt_getArchiveHeap()}, {"command", mDoExt_getCommandHeap()}};
    int bad = 0;
    for (auto& h : heaps) {
        if (h.heap == nullptr) {
            writef(STDERR_FILENO, "[cos] heaps: the %s heap does not exist\n", h.name);
            bad++;
            continue;
        }
        bool ok = h.heap->check();
        writef(STDERR_FILENO, "[cos] heaps: %-8s %p size 0x%08x free 0x%08x check %s\n", h.name,
               h.heap->getStartAddr(), (unsigned)h.heap->getHeapSize(),
               (unsigned)h.heap->getTotalFreeSize(), ok ? "ok" : "FAILED");
        if (!ok) {
            bad++;
        }
    }
    if (bad != 0) {
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    pc_milestone("heaps");
    runHeapsSmoke();
}
