// COS_SMOKE=blur-pos (bug B3, docs/NATIVE_PORT_PLAN.md "Known bugs"): the sword blur positions.
//
// Runs from pc_heaps_created, then exits. The player's sword trail (daPy_swBlur_c) is drawn from a
// *_POS resource of /res/Object/LkAnm.arc: a raw array of big-endian Vec pairs (sword root and
// tip in the player's model space, one pair per animation frame) that daPy_lk_c::setBlurPosResource
// copies into mSwBlur.mpPosBuffer. Read in host order, those floats are garbage and the trail's
// quads cover the screen in white (bug B3, seen as a "white polygon" when the sword hits Orca's
// spear). The test mounts LkAnm.arc in main RAM, as the game does, and reads every *_POS
// resource (dRes_INDEX_LKANM__BTJUMPCUT_POS_e .. dRes_INDEX_LKANM__WEAPONTURN_POS_e) through
// daPy_readBlurPosResource, the TARGET_PC reader setBlurPosResource uses. Each must be a
// non-empty whole number of Vec pairs whose every value is finite and within 1000 units of the player
// (the sword is about 100 units long), with some value beyond 1 unit (not all zero).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "res/Object/LkAnm.h"

#include <cmath>
#include <unistd.h>

u32 daPy_readBlurPosResource(Vec* buffer, u32 bufferSize, u16 index, JKRArchive* arc);

namespace pc {

namespace {

constexpr u32 kHeapSize = 0x200000;
constexpr u32 kBufferSize = sizeof(Vec) * 2 * 0x300; // as setBlurPosResource

} // namespace

[[noreturn]] void smokeBlurPos() {
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] blur-pos: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    JKRArchive* arc = JKRArchive::mount("/res/Object/LkAnm.arc", JKRArchive::MOUNT_MEM, heap,
                                        JKRArchive::MOUNT_DIRECTION_HEAD);
    if (arc == nullptr) {
        writef(STDERR_FILENO, "[cos] blur-pos: cannot mount /res/Object/LkAnm.arc\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    Vec* buffer = static_cast<Vec*>(JKRAllocFromHeap(heap, kBufferSize, 0x20));
    int errors = 0;
    unsigned int files = 0, pairs = 0;
    float maxAll = 0.0f;
    for (int index = dRes_INDEX_LKANM__BTJUMPCUT_POS_e; index <= dRes_INDEX_LKANM__WEAPONTURN_POS_e;
         index++) {
        u32 size = daPy_readBlurPosResource(buffer, kBufferSize, (u16)index, arc);
        files++;
        if (size == 0 || size % (2 * sizeof(Vec)) != 0) {
            writef(STDERR_FILENO, "[cos] blur-pos: resource 0x%X: size %u is not a whole number "
                                  "of Vec pairs\n", index, (unsigned int)size);
            errors++;
            continue;
        }
        const float* v = reinterpret_cast<const float*>(buffer);
        const u32 count = size / sizeof(float);
        float maxAbs = 0.0f;
        bool finite = true;
        for (u32 i = 0; i < count; i++) {
            if (!std::isfinite(v[i])) {
                finite = false;
                break;
            }
            maxAbs = std::fmax(maxAbs, std::fabs(v[i]));
        }
        pairs += size / (2 * sizeof(Vec));
        if (!finite || maxAbs >= 1000.0f || maxAbs <= 1.0f) {
            writef(STDERR_FILENO, "[cos] blur-pos: resource 0x%X: %u frames, values %s, max |v| "
                                  "%g (expected finite, 1 < max < 1000)\n",
                   index, (unsigned int)(size / (2 * sizeof(Vec))), finite ? "finite" : "not finite",
                   (double)maxAbs);
            errors++;
            continue;
        }
        maxAll = std::fmax(maxAll, maxAbs);
    }
    JKRFreeToHeap(heap, buffer);
    arc->unmount();
    writef(STDERR_FILENO, "[cos] blur-pos: %u *_POS resources, %u root/tip pairs, max |v| %.1f; "
                          "%d error(s)\n", files, pairs, (double)maxAll, errors);
    pc_exit(errors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
