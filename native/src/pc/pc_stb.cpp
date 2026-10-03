// JStudio demos on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.17): the COS_SMOKE=stb-sweep
// test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc of the
// disc under /res (the FST walked with DVDOpenDir/DVDReadDir) is mounted in main RAM, and every
// STB in it (recognised by its "STB\0" signature; a Yaz0 entry is expanded first) is played as
// dDemo_manager_c plays a demo, with null adaptors in place of the game's:
// - a JStudio::TControl (1/30 s per frame) with a JStudio::TFactory whose only TCreateObject
//   makes, for every object block (JACT, JCMR, JABL, JLIT, JFOG, JPTC, JSND, JMSG), the JStudio
//   object of that kind over an adaptor that only records its calls; an STB with another object
//   kind fails the parse, as it would in the game;
// - JStudio::TParse::parse_next with flags 0 (dDemo_manager_c::create), which also reads the JFVB
//   block's function values; then forward(0), and forward(1) once per frame until it returns
//   false (dDemo_manager_c::update). A suspend of the control object (the game's message
//   window holds the demo) is released on the frame it is seen, as d_mesg does once the message
//   closes.
// Every frame every variable value of every adaptor (immediate, time and function values) must
// be finite and below 1e7 in magnitude; every object must end, within 100000 frames; destroying
// the objects must give the sweep heap back all it lent.
// <COS_RUN_DIR>/stb_sweep.txt gets per STB an STB line (version, block, function-value and
// object counts, the suspends released, the frames run) and per object an OBJ line (type, ID,
// flag, and the do_paragraph, do_data and waited-frame counts the objects saw); native/tools/
// run.sh compares it with the manifest (disc_manifest.py --check-stb), which walks the same
// sequences independently from the file bytes.
// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDecomp.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JStudio/JStudio/jstudio-control.h"
#include "JSystem/JStudio/JStudio/jstudio-object.h"

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

constexpr uint32_t kSweepHeapSize = 32 * 1024 * 1024;
constexpr uint32_t kMaxFrames = 100000;
constexpr double kValueBound = 1e7;

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
        writef(STDERR_FILENO, "[cos] stb-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Text for the report: printable ASCII but space and '%' kept, the rest as %XX.
String enc(const char* text, size_t len) {
    String out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c > 0x20 && c < 0x7F && c != '%') {
            out += (char)c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

String enc(const String& text) {
    return enc(text.data(), text.size());
}

struct Totals {
    unsigned int archives = 0;
    unsigned int files = 0;
    unsigned int objects = 0;
    unsigned long long frames = 0;
    unsigned long long adaptorCalls = 0;
    unsigned long long values = 0;
    double maxAbs = 0.0;
};
Totals sTotals;

// What one object of the STB saw (the OBJ line).
struct ObjStats {
    uint32_t type = 0;
    String id;
    uint16_t flag = 0;
    uint32_t paragraphs = 0;
    uint32_t data = 0;
    uint64_t wait = 0;
    uint64_t suspended = 0;
    const JStudio::stb::TObject* object = nullptr;
};

// The STB being played.
struct Run {
    Vector<ObjStats> objects;
    Vector<JStudio::TAdaptor*> adaptors;
    unsigned long long calls = 0;
};
Run* sRun = nullptr;

// Null adaptors: every operation is recorded, nothing else happens. Their variable values start
// at 0 (TVariableValue leaves mValue to its owner).
void zeroValues(JStudio::TAdaptor* a) {
    for (u32 i = 0; i < a->mCount; i++) {
        a->mVariableValues[i].mValue = 0.0f;
    }
}

void note(JStudio::data::TEOperationData op, const void* data, u32 size) {
    sRun->calls++;
    if (size != 0 && data == nullptr) {
        fail("operation 0x%x with %u bytes of operand and no data", (unsigned)op, size);
    }
}

#define NULL_OP(name)                                                                       \
    void adaptor_do_##name(JStudio::data::TEOperationData op, const void* data, u32 size)  \
        override {                                                                          \
        note(op, data, size);                                                               \
    }

struct NullActor : JStudio::TAdaptor_actor {
    NullActor() { zeroValues(this); }
    NULL_OP(PARENT)
    NULL_OP(PARENT_NODE)
    NULL_OP(PARENT_ENABLE)
    NULL_OP(RELATION)
    NULL_OP(RELATION_NODE)
    NULL_OP(RELATION_ENABLE)
    NULL_OP(SHAPE)
    NULL_OP(ANIMATION)
    NULL_OP(ANIMATION_MODE)
    NULL_OP(TEXTURE_ANIMATION)
    NULL_OP(TEXTURE_ANIMATION_MODE)
};

struct NullCamera : JStudio::TAdaptor_camera {
    NullCamera() { zeroValues(this); }
    NULL_OP(PARENT)
    NULL_OP(PARENT_NODE)
    NULL_OP(PARENT_ENABLE)
};

struct NullAmbientLight : JStudio::TAdaptor_ambientLight {
    NullAmbientLight() { zeroValues(this); }
};

struct NullLight : JStudio::TAdaptor_light {
    NullLight() { zeroValues(this); }
    NULL_OP(ENABLE)
    NULL_OP(FACULTY)
};

struct NullFog : JStudio::TAdaptor_fog {
    NullFog() { zeroValues(this); }
};

struct NullParticle : JStudio::TAdaptor_particle {
    NullParticle() { zeroValues(this); }
    NULL_OP(PARTICLE)
    NULL_OP(PARENT)
    NULL_OP(PARENT_NODE)
    NULL_OP(PARENT_ENABLE)
};

struct NullSound : JStudio::TAdaptor_sound {
    NullSound() { zeroValues(this); }
    NULL_OP(SOUND)
    NULL_OP(LOCATED)
};

struct NullMessage : JStudio::TAdaptor_message {
    NULL_OP(MESSAGE)
};

#undef NULL_OP

// The JStudio object of a kind, counting what the sequence hands it before the object acts on it.
template <class Base>
struct Counted : Base {
    template <class Adaptor>
    Counted(const JStudio::stb::data::TParse_TBlock_object& block, Adaptor* adaptor)
        : Base(block, adaptor) {}

    ObjStats& stats() { return sRun->objects[index]; }

    void do_paragraph(u32 type, const void* content, u32 size) override {
        stats().paragraphs++;
        Base::do_paragraph(type, content, size);
    }
    void do_data(const void* id, u32 idSize, const void* content, u32 size) override {
        stats().data++;
        Base::do_data(id, idSize, content, size);
    }
    void do_wait(u32 frames) override {
        if (this->getStatus() == JStudio::stb::TObject::STATUS_SUSPEND) {
            stats().suspended += frames;
        } else {
            stats().wait += frames;
        }
        Base::do_wait(frames);
    }

    size_t index = 0;
};

template <class Object, class Adaptor>
JStudio::TObject* make(const JStudio::stb::data::TParse_TBlock_object& block) {
    Adaptor* adaptor = new Adaptor();
    if (adaptor == nullptr) {
        fail("no memory for an adaptor");
        return nullptr;
    }
    Counted<Object>* object =
        JStudio::TObject::createFromAdaptor<Counted<Object>, Adaptor>(block, adaptor);
    if (object == nullptr) {
        fail("no memory for an object");
        return nullptr;
    }
    Run& run = *sRun;
    if (run.objects.size() == run.objects.capacity()) {
        fail("more objects than blocks");
        return object;
    }
    ObjStats st;
    st.type = block.get_type();
    const char* id = (const char*)block.get_ID();
    st.id = String(id, strnlen(id, block.get_IDSize()));
    st.flag = block.get_flag();
    st.object = object;
    object->index = run.objects.size();
    run.objects.push_back(st);
    run.adaptors.push_back(adaptor);
    return object;
}

struct NullCreate : JStudio::TCreateObject {
    ~NullCreate() override {}
    bool create(JStudio::TObject** out, const JStudio::stb::data::TParse_TBlock_object& block)
        override {
        using namespace JStudio;
        switch (block.get_type()) {
        case stb::data::BLOCK_ACTOR:
            *out = make<TObject_actor, NullActor>(block);
            return true;
        case stb::data::BLOCK_CAMERA:
            *out = make<TObject_camera, NullCamera>(block);
            return true;
        case stb::data::BLOCK_AMBIENTLIGHT:
            *out = make<TObject_ambientLight, NullAmbientLight>(block);
            return true;
        case stb::data::BLOCK_LIGHT:
            *out = make<TObject_light, NullLight>(block);
            return true;
        case stb::data::BLOCK_FOG:
            *out = make<TObject_fog, NullFog>(block);
            return true;
        case stb::data::BLOCK_PARTICLE:
            *out = make<TObject_particle, NullParticle>(block);
            return true;
        case stb::data::BLOCK_SOUND:
            *out = make<TObject_sound, NullSound>(block);
            return true;
        case stb::data::BLOCK_MESSAGE:
            *out = make<TObject_message, NullMessage>(block);
            return true;
        default:
            fail("object block of an unknown kind 0x%08x", (unsigned)block.get_type());
            return false;
        }
    }
};

// Every variable value of every adaptor, after a frame.
void checkValues(Run& run, uint32_t frame) {
    for (JStudio::TAdaptor* a : run.adaptors) {
        for (u32 i = 0; i < a->mCount; i++) {
            double v = a->mVariableValues[i].getValue();
            sTotals.values++;
            if (!std::isfinite(v) || std::fabs(v) >= kValueBound) {
                fail("frame %u: variable value %u of an adaptor is %g", frame, i, v);
                return;
            }
            sTotals.maxAbs = std::fmax(sTotals.maxAbs, std::fabs(v));
        }
    }
}

uint32_t fvbCount(JStudio::TControl& control) {
    uint32_t n = 0;
    while (control.fvb_getObject_index(n) != nullptr) {
        n++;
    }
    return n;
}

// Releases a suspend of the control object; returns its count.
int32_t release(JStudio::TControl& control) {
    s32 held = control.referObject_control().getSuspend();
    if (held > 0) {
        control.unsuspend(held);
        return held;
    }
    return 0;
}

void sweepStb(const String& path, const uint8_t* bytes, uint32_t size, JKRHeap* heap, int fd) {
    sWhere = path.c_str();
    if (size < 0x20) {
        fail("short STB (%u bytes)", size);
        return;
    }
    uint32_t blocks = rd32(bytes + 0x0C);
    if (blocks > 4096) {
        fail("%u blocks", blocks);
        return;
    }
    s32 heapFree = heap->getTotalFreeSize();
    JKRHeap* previous = heap->becomeCurrentHeap();
    Run run;
    run.objects.reserve(blocks);
    sRun = &run;
    int32_t suspends = 0;
    uint32_t frames = 0;
    uint32_t fvb = 0;
    bool parsed = false;
    {
        NullCreate creator;
        JStudio::TFactory factory;
        JStudio::TControl control;
        control.setSecondPerFrame(1 / 30.0f);
        control.setFactory(&factory);
        factory.appendCreateObject(&creator);
        {
            JStudio::TParse parse(&control);
            const void* p = bytes;
            parsed = parse.parse_next(&p, 0);
        }
        if (!parsed) {
            fail("JStudio::TParse::parse_next rejected the file");
        } else {
            fvb = fvbCount(control);
            control.forward(0);
            suspends += release(control);
            checkValues(run, 0);
            while (true) {
                if (frames >= kMaxFrames) {
                    fail("still running after %u frames", frames);
                    break;
                }
                bool more = control.forward(1);
                frames++;
                checkValues(run, frames);
                suspends += release(control);
                if (!more) {
                    break;
                }
            }
            for (const ObjStats& o : run.objects) {
                if (o.object->getStatus() != JStudio::stb::TObject::STATUS_END) {
                    fail("object '%s' ends with status %d", o.id.c_str(), (int)o.object->getStatus());
                }
            }
        }
        control.destroyObject_all();
        control.setFactory(nullptr);
    }
    previous->becomeCurrentHeap();
    sRun = nullptr;
    if (heap->getTotalFreeSize() != heapFree) {
        fail("the objects kept %d bytes of the sweep heap", (int)(heapFree - heap->getTotalFreeSize()));
    }
    if (!parsed) {
        return;
    }
    sTotals.files++;
    sTotals.objects += run.objects.size();
    sTotals.frames += frames;
    sTotals.adaptorCalls += run.calls;
    if (fd < 0) {
        return;
    }
    String name = enc(path);
    writef(fd, "STB %s version=%u target_version=%u blocks=%u fvb=%u objects=%zu suspend=%d "
               "frames=%u\n",
           name.c_str(), rd16(bytes + 6), rd16(bytes + 0x1E), blocks, fvb, run.objects.size(),
           suspends, frames);
    for (size_t i = 0; i < run.objects.size(); i++) {
        const ObjStats& o = run.objects[i];
        char tag[4] = {(char)(o.type >> 24), (char)(o.type >> 16), (char)(o.type >> 8),
                       (char)o.type};
        writef(fd, "OBJ %s %zu type=%s id=%s flag=%u paragraphs=%u data=%u wait=%llu\n",
               name.c_str(), i, enc(tag, 4).c_str(), enc(o.id).c_str(), o.flag, o.paragraphs,
               o.data, (unsigned long long)o.wait);
    }
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
            // Expanded as JKRArchive::readResource expands a compressed entry.
            uint32_t full = rd32(bytes + 4);
            expanded = (uint8_t*)JKRAllocFromHeap(heap, (full + 0x1F) & ~0x1Fu, 0x20);
            if (expanded == nullptr) {
                sWhere = path.c_str();
                fail("no room to expand 0x%x bytes", (unsigned)full);
                continue;
            }
            JKRDecomp::decode(bytes, expanded, full, 0);
            bytes = expanded;
            size = full;
        }
        if (size >= 4 && memcmp(bytes, "STB\0", 4) == 0) {
            sweepStb(path, bytes, size, heap, fd);
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

[[noreturn]] void smokeStbSweep() {
    uint64_t start = elapsedMs();
    sWhere = "stb-sweep";
    Vector<String> archives;
    findArchives("/res", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] stb-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("stb_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# stb-sweep (COS_SMOKE=stb-sweep): every STB as JStudio parses and plays it "
                   "with null adaptors, in disc_manifest.py's names\n");
    }
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sWhere = "stb-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    writef(STDERR_FILENO,
           "[cos] stb-sweep: %u archives, %u STB files, %u objects; %llu frames played, %llu "
           "adaptor operations, %llu variable values checked (max |v| %g); %llu ms; %d error(s)%s\n",
           sTotals.archives, sTotals.files, sTotals.objects, sTotals.frames, sTotals.adaptorCalls,
           sTotals.values, sTotals.maxAbs, (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in stb_sweep.txt)" : "");
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && sTotals.files > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
