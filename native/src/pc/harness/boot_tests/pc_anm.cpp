// J3D animations on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.12): the COS_SMOKE=anm-sweep
// test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc of the
// disc under /res (the FST walked with DVDOpenDir/DVDReadDir) is mounted in main RAM, as
// dRes_info_c mounts an object or stage archive, and every J3D1 animation in it (recognised by its
// magic: BCK, BTK, BRK, BPK, BTP, BVA are the kinds on the disc) is loaded the way the game loads
// it, in a solid heap made as dRes_info_c::setRes makes one:
// - a BCK as dRes_info_c::loadResource does: an mDoExt_transAnmBas (with the file's BAS data when
//   the header has one) bound to the file by J3DAnmLoaderDataBase::setResource;
// - any other kind through J3DAnmLoaderDataBase::load.
// Each animation is then evaluated through the game's getters at the frames 0, frame_max / 2 and
// frame_max (first, middle and last frame): getTransform (BCK, BTK), getTevColorReg and
// getTevKonstReg (BRK), getColor (BPK), getTexNo (BTP), getVisibility (BVA), plus the update
// material IDs, tex-matrix IDs and SRT centres the material code reads. Every value must be
// finite, every |scale| < 1e3 and every |translation| < 1e6. The same file is then loaded a second
// time through J3DAnmLoaderDataBase::load (the player's path for BCK) and must give the same
// values bit for bit: the loaders do not modify the file data.
// A compressed entry (Yaz0) is expanded first, as JKRArchive::readResource does for the player's
// animation archives.
// <COS_RUN_DIR>/anm_sweep.txt gets, per file, an ANM line (block tag, attribute, frame_max, track
// count, name table sizes) and per family of values a VAL line (count, sum, sum of |v|, max |v|);
// native/tools/run.sh compares it with the manifest (disc_manifest.py --check-anm), which
// evaluates the same animations independently from the file bytes.
// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "JSystem/J3DGraphBase/J3DStruct.h"
#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "JSystem/J3DGraphLoader/J3DAnmLoader.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDecomp.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "JSystem/JUtility/JUTDataHeader.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "m_Do/m_Do_ext.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <unistd.h>

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 48 * 1024 * 1024;

int sErrors = 0;
const char* sWhere = "";

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] anm-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// A path for the report: printable ASCII but space and '%' kept, the rest as %XX (two BRK of
// Os.arc have a space in their name).
String enc(const String& text) {
    String out;
    for (char ch : text) {
        unsigned char c = (unsigned char)ch;
        if (c > 0x20 && c < 0x7F && c != '%') {
            out += ch;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

struct Totals {
    unsigned int archives = 0;
    unsigned int files = 0;
    unsigned int byTag[6] = {};
    unsigned long long values = 0;
};
Totals sTotals;

const char* const kTags[6] = {"ANK1", "TTK1", "TRK1", "PAK1", "TPT1", "VAF1"};
const char* const kTypes[6] = {"bck1", "btk1", "brk1", "bpk1", "btp1", "bva1"};

// The values one family of getters returned, in order.
struct Family {
    const char* name;
    Vector<double> values;
};

struct Eval {
    Vector<Family> families;

    Vector<double>& operator[](const char* name) {
        for (Family& f : families) {
            if (strcmp(f.name, name) == 0) {
                return f.values;
            }
        }
        families.push_back(Family{name, {}});
        return families.back().values;
    }
};

// What the sweep requires of a family: finite values, and a bound on |v| when nonzero.
void checkRange(const Family& f) {
    double bound = 0.0;
    if (strcmp(f.name, "scale") == 0) {
        bound = 1e3;
    } else if (strcmp(f.name, "trans") == 0) {
        bound = 1e6;
    }
    for (size_t i = 0; i < f.values.size(); i++) {
        double v = f.values[i];
        if (!std::isfinite(v)) {
            fail("%s value %zu is not finite", f.name, i);
            return;
        }
        if (bound != 0.0 && std::fabs(v) >= bound) {
            fail("%s value %zu is %g (|v| must stay below %g)", f.name, i, v, bound);
            return;
        }
    }
}

uint16_t nameCount(JUTNameTab* tab) {
    const ResNTAB* res = tab->getResNameTable();
    return res != nullptr ? (uint16_t)res->mEntryNum : 0;
}

int tagIndex(const uint8_t* bytes) {
    for (int i = 0; i < 6; i++) {
        if (memcmp(bytes + 4, kTypes[i], 4) == 0) {
            return i;
        }
    }
    return -1;
}

// The first, middle and last frame of an animation of frameMax frames.
void framesOf(J3DAnmBase* anm, f32 frames[3]) {
    f32 frameMax = (f32)anm->getFrameMax();
    frames[0] = 0.0f;
    frames[1] = frameMax * 0.5f;
    frames[2] = frameMax;
}

// Count of the block's tracks the evaluation walks, from the file (J3DAnmTransformKey and
// J3DAnmVisibilityFull keep theirs private).
uint16_t rawCount(const uint8_t* bytes) {
    return rd16(bytes + 0x20 + 0x0C);
}

// Evaluates anm (of the kind tag) and fills ev; writes the ANM line's counts into head.
void evaluate(J3DAnmBase* anm, int tag, const uint8_t* bytes, Eval& ev, char* head,
              size_t headSize) {
    f32 frames[3];
    framesOf(anm, frames);
    switch (tag) {
    case 0: { // BCK
        J3DAnmTransform* t = (J3DAnmTransform*)anm;
        uint16_t joints = rawCount(bytes);
        snprintf(head, headSize, "count=%u", joints);
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t j = 0; j < joints; j++) {
                J3DTransformInfo info;
                t->getTransform(j, &info);
                ev["scale"].push_back(info.mScale.x);
                ev["rot"].push_back(info.mRotation.x);
                ev["trans"].push_back(info.mTranslate.x);
                ev["scale"].push_back(info.mScale.y);
                ev["rot"].push_back(info.mRotation.y);
                ev["trans"].push_back(info.mTranslate.y);
                ev["scale"].push_back(info.mScale.z);
                ev["rot"].push_back(info.mRotation.z);
                ev["trans"].push_back(info.mTranslate.z);
            }
        }
        break;
    }
    case 1: { // BTK
        J3DAnmTextureSRTKey* t = (J3DAnmTextureSRTKey*)anm;
        uint16_t mats = t->getUpdateMaterialNum();
        snprintf(head, headSize, "count=%u names=%u", mats, nameCount(t->getUpdateMaterialName()));
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t m = 0; m < mats; m++) {
                J3DTextureSRTInfo info;
                t->getTransform(m, &info);
                ev["scale"].push_back(info.mScaleX);
                ev["scale"].push_back(info.mScaleY);
                ev["rot"].push_back(info.mRotation);
                ev["trans"].push_back(info.mTranslationX);
                ev["trans"].push_back(info.mTranslationY);
            }
        }
        for (uint16_t m = 0; m < mats; m++) {
            ev["ids"].push_back(t->getUpdateMaterialID(m));
        }
        for (uint16_t m = 0; m < mats; m++) {
            ev["texmtx"].push_back(t->getUpdateTexMtxID(m));
        }
        // The SRT centres are optional (offset 0: no table).
        if (rd32(bytes + 0x20 + 0x24) != 0) {
            for (uint16_t m = 0; m < mats; m++) {
                Vec c = t->getSRTCenter(m);
                ev["center"].push_back(c.x);
                ev["center"].push_back(c.y);
                ev["center"].push_back(c.z);
            }
        }
        break;
    }
    case 2: { // BRK
        J3DAnmTevRegKey* t = (J3DAnmTevRegKey*)anm;
        uint16_t c = t->getCRegUpdateMaterialNum();
        uint16_t k = t->getKRegUpdateMaterialNum();
        snprintf(head, headSize, "count=%u,%u cnames=%u knames=%u", c, k,
                 nameCount(t->getCRegUpdateMaterialName()),
                 nameCount(t->getKRegUpdateMaterialName()));
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t m = 0; m < c; m++) {
                GXColorS10 col = {};
                t->getTevColorReg(m, &col);
                ev["creg"].push_back(col.r);
                ev["creg"].push_back(col.g);
                ev["creg"].push_back(col.b);
                ev["creg"].push_back(col.a);
            }
        }
        for (uint16_t m = 0; m < c; m++) {
            ev["cids"].push_back(t->getCRegUpdateMaterialID(m));
        }
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t m = 0; m < k; m++) {
                GXColor col = {};
                t->getTevKonstReg(m, &col);
                ev["kreg"].push_back(col.r);
                ev["kreg"].push_back(col.g);
                ev["kreg"].push_back(col.b);
                ev["kreg"].push_back(col.a);
            }
        }
        for (uint16_t m = 0; m < k; m++) {
            ev["kids"].push_back(t->getKRegUpdateMaterialID(m));
        }
        break;
    }
    case 3: { // BPK
        J3DAnmColor* t = (J3DAnmColor*)anm;
        uint16_t mats = t->getUpdateMaterialNum();
        snprintf(head, headSize, "count=%u names=%u", mats, nameCount(t->getUpdateMaterialName()));
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t m = 0; m < mats; m++) {
                GXColor col = {};
                t->getColor(m, &col);
                ev["color"].push_back(col.r);
                ev["color"].push_back(col.g);
                ev["color"].push_back(col.b);
                ev["color"].push_back(col.a);
            }
        }
        for (uint16_t m = 0; m < mats; m++) {
            ev["ids"].push_back(t->getUpdateMaterialID(m));
        }
        break;
    }
    case 4: { // BTP
        J3DAnmTexPattern* t = (J3DAnmTexPattern*)anm;
        uint16_t mats = t->getUpdateMaterialNum();
        snprintf(head, headSize, "count=%u names=%u", mats, nameCount(t->getUpdateMaterialName()));
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t m = 0; m < mats; m++) {
                u16 texNo = 0;
                t->getTexNo(m, &texNo);
                ev["texno"].push_back(texNo);
            }
        }
        for (uint16_t m = 0; m < mats; m++) {
            ev["ids"].push_back(t->getUpdateMaterialID(m));
        }
        break;
    }
    case 5: { // BVA
        J3DAnmVisibilityFull* t = (J3DAnmVisibilityFull*)anm;
        uint16_t num = rawCount(bytes);
        snprintf(head, headSize, "count=%u", num);
        for (f32 frame : frames) {
            t->setFrame(frame);
            for (uint16_t m = 0; m < num; m++) {
                u8 vis = 0;
                t->getVisibility(m, &vis);
                ev["vis"].push_back(vis);
            }
        }
        break;
    }
    }
}

bool sameBits(double a, double b) {
    return memcmp(&a, &b, sizeof(double)) == 0;
}

void sweepAnimation(const String& path, uint8_t* bytes, uint32_t size, int tag, JKRHeap* parent,
                    int fd) {
    sWhere = path.c_str();
    const JUTDataFileHeader* header = (const JUTDataFileHeader*)bytes;
    if (size < 0x40 || header->mBlockNum != 1 || memcmp(bytes + 0x20, kTags[tag], 4) != 0) {
        fail("not a one-block %s file", kTags[tag]);
        return;
    }
    // A solid heap as dRes_info_c::setRes makes one, adjusted after the load.
    JKRSolidHeap* heap = mDoExt_createSolidHeapToCurrent(0, parent, 0x20);
    if (heap == nullptr) {
        fail("no solid heap (parent free %d)", (int)parent->getTotalFreeSize());
        return;
    }
    J3DAnmBase* anm;
    if (tag == 0) {
        // dRes_info_c::loadResource ('BCK '/'BCKS').
        void* bas = nullptr;
        if (header->mSeAnmOffset != 0xFFFFFFFF) {
            bas = bytes + (u32)header->mSeAnmOffset;
        }
        mDoExt_transAnmBas* t = new mDoExt_transAnmBas(bas);
        if (t != nullptr) {
            J3DAnmLoaderDataBase::setResource(t, bytes);
        }
        anm = t;
    } else {
        anm = J3DAnmLoaderDataBase::load(bytes);
    }
    J3DAnmBase* again = J3DAnmLoaderDataBase::load(bytes);
    mDoExt_restoreCurrentHeap();
    mDoExt_adjustSolidHeap(heap);
    if (anm == nullptr || again == nullptr) {
        fail("the loader returned NULL");
        mDoExt_destroySolidHeap(heap);
        return;
    }
    const uint8_t* block = bytes + 0x20;
    if (anm->getFrameMax() != (s16)rd16(block + (tag == 3 ? 0x0C : 0x0A))) {
        fail("frame_max %d", anm->getFrameMax());
    }
    if (anm->getAttribute() != block[0x08]) {
        fail("attribute %u", anm->getAttribute());
    }
    Eval ev;
    char head[128];
    evaluate(anm, tag, bytes, ev, head, sizeof(head));
    Eval ev2;
    char head2[128];
    evaluate(again, tag, bytes, ev2, head2, sizeof(head2));
    if (strcmp(head, head2) != 0 || ev.families.size() != ev2.families.size()) {
        fail("the second load reports %s", head2);
    } else {
        for (size_t i = 0; i < ev.families.size(); i++) {
            const Vector<double>& a = ev.families[i].values;
            const Vector<double>& b = ev2.families[i].values;
            bool same = a.size() == b.size();
            for (size_t k = 0; same && k < a.size(); k++) {
                same = sameBits(a[k], b[k]);
            }
            if (!same) {
                fail("%s: the second load gives other values", ev.families[i].name);
            }
        }
    }
    sTotals.files++;
    sTotals.byTag[tag]++;
    String name = enc(path);
    if (fd >= 0) {
        writef(fd, "ANM %s magic=J3D1%s tag=%s attribute=%u frame_max=%d %s\n", name.c_str(),
               kTypes[tag], kTags[tag], anm->getAttribute(), anm->getFrameMax(), head);
    }
    for (const Family& f : ev.families) {
        checkRange(f);
        double sum = 0.0, abssum = 0.0, maxabs = 0.0;
        for (double v : f.values) {
            sum += v;
            abssum += std::fabs(v);
            maxabs = std::fmax(maxabs, std::fabs(v));
        }
        sTotals.values += f.values.size();
        if (fd >= 0) {
            writef(fd, "VAL %s %s n=%zu sum=%.9g abs=%.9g max=%.9g\n", name.c_str(), f.name,
                   f.values.size(), sum, abssum, maxabs);
        }
    }
    mDoExt_destroySolidHeap(heap);
}

void walkArchive(JKRArchive* arc, uint32_t node, const String& arcPath, const String& prefix,
                 int depth, JKRHeap* heap, int fd) {
    if (depth > 16 || node >= arc->mArcInfoBlock->num_nodes) {
        fail("bad directory node %u", node);
        return;
    }
    JKRArchive::SDIDirEntry* n = arc->mNodes + node;
    for (uint32_t k = n->first_file_index; k < n->first_file_index + n->num_entries; k++) {
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        const char* name = arc->mStringTable + e->getNameOffset();
        if (e->isDirectory()) {
            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                walkArchive(arc, e->data_offset, arcPath, prefix + name + "/", depth + 1, heap, fd);
            }
            continue;
        }
        String inner = prefix + name;
        uint8_t* bytes = (uint8_t*)arc->getResource(("/" + inner).c_str());
        if (bytes == nullptr) {
            continue;
        }
        uint32_t size = arc->getResSize(bytes);
        String path = arcPath + ":" + inner;
        uint8_t* expanded = nullptr;
        if (size >= 0x10 && memcmp(bytes, "Yaz0", 4) == 0) {
            // A compressed entry (the player's animation archives LkAnm, LkD00, LkD01, which the
            // game reads with JKRArchive::readResource): expanded as readResource expands it.
            uint32_t full = rd32(bytes + 4);
            expanded = (uint8_t*)JKRAllocFromHeap(heap, (full + 0x1F) & ~0x1Fu, 0x20);
            if (expanded == nullptr) {
                sWhere = path.c_str();
                fail("no room to expand 0x%x bytes", (unsigned)full);
                continue;
            }
            // decode's length is that of the output (fetchResource_subroutine passes the buffer's).
            JKRDecomp::decode(bytes, expanded, full, 0);
            bytes = expanded;
            size = full;
        }
        if (size >= 8 && memcmp(bytes, "J3D1", 4) == 0) {
            int tag = tagIndex(bytes);
            if (tag < 0) {
                sWhere = path.c_str();
                fail("an animation kind the sweep does not evaluate: %.4s",
                     (const char*)bytes + 4);
            } else {
                sweepAnimation(path, bytes, size, tag, heap, fd);
            }
        }
        if (expanded != nullptr) {
            JKRFreeToHeap(heap, expanded);
        }
        sWhere = arcPath.c_str();
    }
}

void sweepArchive(const String& path, JKRHeap* heap, int fd) {
    sWhere = path.c_str();
    s32 heapFree = heap->getTotalFreeSize();
    JKRArchive* arc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_MEM, heap,
                                        JKRArchive::MOUNT_DIRECTION_HEAD);
    if (arc == nullptr) {
        fail("mount failed");
        return;
    }
    sTotals.archives++;
    walkArchive(arc, 0, path, "", 0, heap, fd);
    sWhere = path.c_str();
    arc->unmount();
    if (heap->getTotalFreeSize() != heapFree) {
        fail("after unmount the sweep heap lost %d bytes", (int)(heapFree - heap->getTotalFreeSize()));
    }
}

void findArchives(const String& dirPath, Vector<String>& out, int depth) {
    DVDDir dir;
    if (!DVDOpenDir(dirPath.c_str(), &dir)) {
        sWhere = dirPath.c_str();
        fail("DVDOpenDir failed");
        return;
    }
    DVDDirEntry entry;
    Vector<String> subdirs;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr) {
            continue;
        }
        String child = dirPath + "/" + entry.name;
        if (entry.isDir) {
            subdirs.push_back(child);
            continue;
        }
        size_t len = child.size();
        if (len > 4 && strcasecmp(child.c_str() + len - 4, ".arc") == 0) {
            out.push_back(child);
        }
    }
    DVDCloseDir(&dir);
    if (depth < 6) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

} // namespace

[[noreturn]] void smokeAnmSweep() {
    uint64_t start = elapsedMs();
    sWhere = "anm-sweep";
    Vector<String> archives;
    findArchives("/res", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] anm-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("anm_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# anm-sweep (COS_SMOKE=anm-sweep): every J3D animation as the game's loaders "
                   "and getters read it, in disc_manifest.py's names\n");
    }
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sWhere = "anm-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    writef(STDERR_FILENO,
           "[cos] anm-sweep: %u archives, %u animations (%u BCK, %u BTK, %u BRK, %u BPK, %u BTP, "
           "%u BVA); %llu values at the first, middle and last frame; %llu ms; %d error(s)%s\n",
           sTotals.archives, sTotals.files, sTotals.byTag[0], sTotals.byTag[1], sTotals.byTag[2],
           sTotals.byTag[3], sTotals.byTag[4], sTotals.byTag[5], sTotals.values,
           (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in anm_sweep.txt)" : "");
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && sTotals.files > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
