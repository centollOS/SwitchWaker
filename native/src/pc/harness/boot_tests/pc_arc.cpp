// RARC archives on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.4): the COS_SMOKE=arc-sweep test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create: the heaps, JKRAram with its ARAM
// heap and the JKRAram/JKRDecomp threads exist), then exits. For every .arc file on the disc (the
// FST walked with DVDOpenDir/DVDReadDir: the 1319 under /res, /RELS.arc and the one under
// /Audiores):
// - an independent reading: the file read with DVDReadPrio, its Yaz0 expanded by a decoder of
//   this file, the RARC tables read with big-endian loads at the format's offsets (no JKR struct),
//   every compressed entry expanded the same way;
// - the archive mounted with JKRArchive::mount in each of the four modes the game uses (MEM, ARAM,
//   DVD, COMP), into a heap of this test, and compared with that reading:
//   - the node and file-entry counts, the directory tree walked through mNodes/mFiles/mStringTable
//     (every path, file ID, flag byte, size and data offset), SDIFileEntry::index (the side table's
//     index) and getDirEntry for every entry;
//   - every file: readResource into a buffer before it is fetched (fetchResource(buffer, ...): the
//     DVD, ARAM and decompression paths), getResource by path (fetchResource: the pointer kept in
//     the side table, JKAR_DATA), its bytes (the stored bytes where the mode keeps them, MEM and
//     COMP's main-memory part, else the expanded ones), getResSize, getExpandedResSize,
//     findIdResource, getResource(0, name) for a name that is unique in the archive, and
//     readResource again (now a copy of the fetched data);
//   - for each directory whose type is unique in the archive, the game's lookup of d_resorce:
//     getFirstResource(type), the finder's entries and getResource(type, name);
//   - after unmount: the test heap, the system heap and the ARAM heap are back to their free size
//     before the mount (resources, side table and file table all freed);
// - an archive stored inside an archive (the disc has one) is mounted in place with
//   JKRMemArchive::mountFixed, as d_s_name does, and checked the same way;
// - <COS_RUN_DIR>/arc_sweep.txt: what the MEM mount read (counts, nodes, every file), in the
//   manifest's names; native/tools/run.sh compares it with build/native-mac/disc_manifest.json
//   (disc_manifest.py --check-arc).
// The frame counter ticks once per archive, so the stall watchdog sees the sweep advance.
// The test's own strings, vectors and maps are in host memory (cos_sdk/host_alloc.h), not in the
// game's operator new: they must not share the heaps whose free sizes the sweep measures.
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRAram.h"
#include "JSystem/JKernel/JKRAramHeap.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRFileFinder.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#include "cos_sdk/host_alloc.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;
template <class K, class V>
using Map = cos_sdk::HostMap<K, V>;

constexpr uint32_t kSweepHeapSize = 48 * 1024 * 1024;

int sErrors = 0;
int sArchiveErrors = 0; // errors of the archive being checked

void fail(const String& where, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void fail(const String& where, const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] arc-sweep: %s: %s\n", where.c_str(), text);
    }
    sErrors++;
    sArchiveErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

uint32_t alignUp32(uint32_t n) {
    return (n + 31) & ~31u;
}

// ---- the independent reading ------------------------------------------------------------------

// Expands a Yaz0 stream; false if it is not one or it is malformed.
bool yaz0Expand(const uint8_t* src, size_t srcLen, Vector<uint8_t>& out) {
    if (srcLen < 16 || memcmp(src, "Yaz0", 4) != 0) {
        return false;
    }
    uint32_t size = rd32(src + 4);
    out.assign(size, 0);
    size_t sp = 16;
    uint32_t dp = 0;
    while (dp < size) {
        if (sp >= srcLen) {
            return false;
        }
        uint8_t code = src[sp++];
        for (int bit = 0; bit < 8 && dp < size; bit++, code <<= 1) {
            if (code & 0x80) {
                if (sp >= srcLen) {
                    return false;
                }
                out[dp++] = src[sp++];
                continue;
            }
            if (sp + 1 >= srcLen) {
                return false;
            }
            uint32_t dist = (((uint32_t)(src[sp] & 0x0F) << 8) | src[sp + 1]) + 1;
            uint32_t n = src[sp] >> 4;
            sp += 2;
            if (n == 0) {
                if (sp >= srcLen) {
                    return false;
                }
                n = src[sp++] + 0x12;
            } else {
                n += 2;
            }
            if (dist > dp || dp + n > size) {
                return false;
            }
            for (uint32_t i = 0; i < n; i++, dp++) {
                out[dp] = out[dp - dist];
            }
        }
    }
    return true;
}

struct RawNode {
    uint32_t type;
    String name;
    uint16_t entries;
    uint32_t first;
};

struct RawEntry {
    uint16_t id;
    uint8_t flags;
    String name;
    uint32_t dataOffset;
    uint32_t size;
};

struct RawFile {
    String path;     // relative to the root node, "dir/name"
    uint32_t index;       // in the file table
    const uint8_t* data;  // the stored bytes (size of the entry)
    bool compressed;      // flags & 4
    Vector<uint8_t> expanded; // the expanded bytes of a compressed entry
};

struct RawArc {
    Vector<uint8_t> bytes; // the archive (expanded if the file is Yaz0)
    bool yaz0 = false;
    Vector<RawNode> nodes;
    Vector<RawEntry> entries;
    Vector<RawFile> files; // walk order: a node's entries in order, subdirectories in place
    int dirs = 0;
};

bool rawWalk(RawArc& raw, uint32_t node, const String& prefix, int depth,
             const uint8_t* dataBase, size_t dataLen, const String& where) {
    if (depth > 32 || node >= raw.nodes.size()) {
        fail(where, "independent reading: bad node %u", node);
        return false;
    }
    const RawNode& n = raw.nodes[node];
    for (uint32_t k = n.first; k < n.first + n.entries; k++) {
        if (k >= raw.entries.size()) {
            fail(where, "independent reading: entry %u out of range", k);
            return false;
        }
        const RawEntry& e = raw.entries[k];
        if (e.flags & 0x02) {
            if (e.name == "." || e.name == "..") {
                continue;
            }
            raw.dirs++;
            if (!rawWalk(raw, e.dataOffset, prefix + e.name + "/", depth + 1, dataBase, dataLen,
                         where)) {
                return false;
            }
            continue;
        }
        RawFile f;
        f.path = prefix + e.name;
        f.index = k;
        if ((uint64_t)e.dataOffset + e.size > dataLen) {
            fail(where, "independent reading: %s lies past the end", f.path.c_str());
            return false;
        }
        f.data = dataBase + e.dataOffset;
        f.compressed = (e.flags & 0x04) != 0;
        if (f.compressed && !yaz0Expand(f.data, e.size, f.expanded)) {
            fail(where, "independent reading: %s is flagged compressed but is not Yaz0",
                 f.path.c_str());
            return false;
        }
        raw.files.push_back(std::move(f));
    }
    return true;
}

// Parses raw.bytes as a RARC archive.
bool rawParse(RawArc& raw, const String& where) {
    const uint8_t* b = raw.bytes.data();
    size_t len = raw.bytes.size();
    if (len < 0x40 || memcmp(b, "RARC", 4) != 0) {
        fail(where, "independent reading: not a RARC archive");
        return false;
    }
    uint32_t headerLength = rd32(b + 0x08);
    uint32_t fileDataOffset = rd32(b + 0x0C);
    const uint8_t* info = b + headerLength;
    uint32_t numNodes = rd32(info + 0x00);
    uint32_t nodeOffset = rd32(info + 0x04);
    uint32_t numEntries = rd32(info + 0x08);
    uint32_t entryOffset = rd32(info + 0x0C);
    uint32_t stringOffset = rd32(info + 0x14);
    if (headerLength + (uint64_t)nodeOffset + numNodes * 0x10ull > len ||
        headerLength + (uint64_t)entryOffset + numEntries * 0x14ull > len ||
        headerLength + (uint64_t)stringOffset > len) {
        fail(where, "independent reading: tables past the end");
        return false;
    }
    const char* strings = (const char*)info + stringOffset;
    size_t stringsLen = len - headerLength - stringOffset;
    auto str = [&](uint32_t off) {
        if (off >= stringsLen) {
            return String("?");
        }
        return String(strings + off, strnlen(strings + off, stringsLen - off));
    };
    for (uint32_t i = 0; i < numNodes; i++) {
        const uint8_t* n = info + nodeOffset + i * 0x10;
        raw.nodes.push_back({rd32(n), str(rd32(n + 4)), rd16(n + 0x0A), rd32(n + 0x0C)});
    }
    for (uint32_t i = 0; i < numEntries; i++) {
        const uint8_t* e = info + entryOffset + i * 0x14;
        uint32_t tf = rd32(e + 4);
        raw.entries.push_back(
            {rd16(e), (uint8_t)(tf >> 24), str(tf & 0xFFFFFF), rd32(e + 8), rd32(e + 0x0C)});
    }
    size_t dataStart = (size_t)headerLength + fileDataOffset;
    if (dataStart > len) {
        fail(where, "independent reading: file data past the end");
        return false;
    }
    if (numNodes == 0) {
        return true;
    }
    return rawWalk(raw, 0, "", 0, b + dataStart, len - dataStart, where);
}

bool rawRead(const char* path, RawArc& raw) {
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        fail(path, "DVDOpen failed");
        return false;
    }
    Vector<uint8_t> file(info.length);
    s32 got = DVDReadPrio(&info, file.data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    if (got != (s32)info.length) {
        fail(path, "DVDReadPrio read %d of %u bytes", (int)got, (unsigned int)info.length);
        return false;
    }
    if (file.size() >= 4 && memcmp(file.data(), "Yaz0", 4) == 0) {
        raw.yaz0 = true;
        if (!yaz0Expand(file.data(), file.size(), raw.bytes)) {
            fail(path, "independent reading: bad Yaz0 stream");
            return false;
        }
    } else {
        raw.bytes = std::move(file);
    }
    return rawParse(raw, path);
}

// ---- the JKR side -------------------------------------------------------------------------------

const char* modeName(int mode) {
    switch (mode) {
    case JKRArchive::MOUNT_MEM: return "MEM";
    case JKRArchive::MOUNT_ARAM: return "ARAM";
    case JKRArchive::MOUNT_DVD: return "DVD";
    case JKRArchive::MOUNT_COMP: return "COMP";
    default: return "?";
    }
}

struct JkrFile {
    String path;
    uint32_t index;
};

void jkrWalk(JKRArchive* arc, uint32_t node, const String& prefix, int depth,
             Vector<JkrFile>& out, int& dirs, const String& where) {
    uint32_t numNodes = arc->mArcInfoBlock->num_nodes;
    uint32_t numEntries = arc->mArcInfoBlock->num_file_entries;
    if (depth > 32 || node >= numNodes) {
        fail(where, "bad node %u", node);
        return;
    }
    JKRArchive::SDIDirEntry* n = arc->mNodes + node;
    for (uint32_t k = n->first_file_index; k < n->first_file_index + n->num_entries; k++) {
        if (k >= numEntries) {
            fail(where, "entry %u out of range", k);
            return;
        }
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        const char* name = arc->mStringTable + e->getNameOffset();
        if (e->isDirectory()) {
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }
            dirs++;
            jkrWalk(arc, e->data_offset, prefix + name + "/", depth + 1, out, dirs, where);
            continue;
        }
        out.push_back({prefix + name, k});
    }
}

// Whether `got` (n bytes reported) holds `want` (wantLen bytes): n between wantLen and wantLen
// rounded up to 32 (the DVD and ARAM paths report whole 32-byte blocks), the first wantLen bytes
// equal.
bool sameBytes(const uint8_t* got, uint32_t n, const uint8_t* want, uint32_t wantLen, bool exact) {
    if (exact ? n != wantLen : (n < wantLen || n > alignUp32(wantLen))) {
        return false;
    }
    return wantLen == 0 || memcmp(got, want, wantLen) == 0;
}

struct Totals {
    unsigned int archives = 0;
    unsigned int mounts = 0;
    unsigned int files = 0;
    unsigned int fetches = 0;
    unsigned int nested = 0;
    unsigned int finderLookups = 0;
    unsigned long long bytes = 0;
};

Totals sTotals;

// A name for arc_sweep.txt: a space, '%' and bytes outside printable ASCII as %XX (names can hold
// spaces; disc_manifest.py --check-arc decodes them).
String enc(const String& text) {
    String out;
    for (unsigned char c : text) {
        if (c <= 0x20 || c >= 0x7F || c == '%') {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        } else {
            out += (char)c;
        }
    }
    return out;
}

void reportArchive(int fd, const String& name, JKRArchive* arc, const RawArc& raw,
                   const Vector<JkrFile>& files, int dirs) {
    if (fd < 0) {
        return;
    }
    String en = enc(name);
    writef(fd, "ARC %s nodes=%u entries=%u files=%u dirs=%d\n", en.c_str(),
           (unsigned int)arc->mArcInfoBlock->num_nodes,
           (unsigned int)arc->mArcInfoBlock->num_file_entries, (unsigned int)files.size(), dirs);
    for (uint32_t i = 0; i < arc->mArcInfoBlock->num_nodes; i++) {
        JKRArchive::SDIDirEntry* n = arc->mNodes + i;
        writef(fd, "NODE %s %u type=%08x name=%s entries=%u first=%u\n", en.c_str(), i,
               (unsigned int)n->type, enc(arc->mStringTable + n->name_offset).c_str(),
               (unsigned int)n->num_entries, (unsigned int)n->first_file_index);
    }
    for (size_t i = 0; i < files.size(); i++) {
        JKRArchive::SDIFileEntry* e = arc->mFiles + files[i].index;
        writef(fd, "FILE %s %s id=%u flags=%u size=%u offset=%u", en.c_str(),
               enc(files[i].path).c_str(),
               (unsigned int)e->getFileID(), (unsigned int)e->getFlags(),
               (unsigned int)e->data_size, (unsigned int)e->data_offset);
        if (i < raw.files.size() && raw.files[i].compressed) {
            writef(fd, " expanded=%u", (unsigned int)arc->getExpandedResSize(
                                           arc->getResource(("/" + files[i].path).c_str())));
        }
        writef(fd, "\n");
    }
}

void checkArchive(JKRArchive* arc, int mode, const RawArc& raw, const String& where,
                  JKRHeap* heap, int reportFd, const String& reportName,
                  Vector<std::pair<String, Vector<uint8_t>>>* nested);

// Mounts the archive inside `data` (a resource of a MEM mount) with mountFixed, as d_s_name does.
void checkNested(const String& where, const uint8_t* data, uint32_t size, JKRHeap* heap,
                 int reportFd, const String& reportName) {
    RawArc raw;
    raw.bytes.assign(data, data + size);
    if (!rawParse(raw, where)) {
        return;
    }
    s32 heapFree = heap->getTotalFreeSize();
    JKRMemArchive* arc = new (heap, 0) JKRMemArchive();
    if (arc == nullptr) {
        fail(where, "no memory for a JKRMemArchive");
        return;
    }
    s32 objectFree = heap->getTotalFreeSize();
    if (!arc->mountFixed((void*)data, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        fail(where, "mountFixed failed");
    } else {
        sTotals.nested++;
        checkArchive(arc, JKRArchive::MOUNT_MEM, raw, where + " (mountFixed)", heap, reportFd,
                     reportName, nullptr);
        arc->unmountFixed();
        // d_s_name keeps the object after unmountFixed: everything the mount took must be back.
        if (heap->getTotalFreeSize() != objectFree) {
            fail(where, "after unmountFixed the heap lost %d bytes",
                 (int)(objectFree - heap->getTotalFreeSize()));
        }
    }
    delete arc;
    if (heap->getTotalFreeSize() != heapFree) {
        fail(where, "mountFixed/unmountFixed: the heap lost %d bytes",
             (int)(heapFree - heap->getTotalFreeSize()));
    }
}

void checkArchive(JKRArchive* arc, int mode, const RawArc& raw, const String& where,
                  JKRHeap* heap, int reportFd, const String& reportName,
                  Vector<std::pair<String, Vector<uint8_t>>>* nested) {
    if (arc->getMountMode() != mode) {
        fail(where, "mount mode %d", arc->getMountMode());
    }
    if (arc->getVolumeType() != 'RARC') {
        fail(where, "volume type %08x", (unsigned int)arc->getVolumeType());
    }
    uint32_t numNodes = arc->mArcInfoBlock->num_nodes;
    uint32_t numEntries = arc->mArcInfoBlock->num_file_entries;
    if (numNodes != raw.nodes.size() || numEntries != raw.entries.size() ||
        arc->countFile() != raw.entries.size()) {
        fail(where, "%u nodes, %u entries (countFile %u); the file has %zu and %zu", numNodes,
             numEntries, (unsigned int)arc->countFile(), raw.nodes.size(), raw.entries.size());
        return;
    }
    for (uint32_t i = 0; i < numNodes; i++) {
        JKRArchive::SDIDirEntry* n = arc->mNodes + i;
        const RawNode& r = raw.nodes[i];
        if (n->type != r.type || r.name != arc->mStringTable + n->name_offset ||
            n->num_entries != r.entries || n->first_file_index != r.first) {
            fail(where, "node %u: type %08x name %s entries %u first %u; the file has %08x %s %u %u",
                 i, (unsigned int)n->type, arc->mStringTable + n->name_offset,
                 (unsigned int)n->num_entries, (unsigned int)n->first_file_index, r.type,
                 r.name.c_str(), r.entries, r.first);
        }
    }
    for (uint32_t k = 0; k < numEntries; k++) {
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        const RawEntry& r = raw.entries[k];
        if (e->index != k) {
            fail(where, "entry %u: side-table index %u", k, e->index);
        }
        JKRArchive::SDirEntry d;
        if (!arc->getDirEntry(&d, k) || d.flags != r.flags || d.id != r.id || r.name != d.name ||
            e->data_offset != r.dataOffset || e->data_size != r.size) {
            fail(where, "entry %u: getDirEntry flags %u id %u name %s, offset %u size %u; the file "
                        "has %u %u %s %u %u",
                 k, d.flags, d.id, d.name, (unsigned int)e->data_offset,
                 (unsigned int)e->data_size, r.flags, r.id, r.name.c_str(), r.dataOffset, r.size);
        }
    }
    Vector<JkrFile> files;
    int dirs = 0;
    jkrWalk(arc, 0, "", 0, files, dirs, where);
    if (files.size() != raw.files.size() || dirs != raw.dirs) {
        fail(where, "the tree has %zu files and %d directories; the file has %zu and %d",
             files.size(), dirs, raw.files.size(), raw.dirs);
        return;
    }
    Map<String, int> nameCount;
    for (const RawFile& f : raw.files) {
        nameCount[raw.entries[f.index].name]++;
    }
    Map<uint32_t, void*> fetched; // entry index -> the resource getResource returned
    // JKRCompArchive keeps expanded sizes (mExpandedSize) only when a compressed file lies outside
    // its main-memory part; without that table getExpandedResSize answers getResSize.
    bool compExpandTable = false;
    for (const RawEntry& r : raw.entries) {
        if ((r.flags & 0x01) && !(r.flags & 0x10) && (r.flags & 0x04)) {
            compExpandTable = true;
        }
    }

    for (size_t i = 0; i < files.size(); i++) {
        const RawFile& f = raw.files[i];
        const RawEntry& r = raw.entries[f.index];
        if (files[i].path != f.path || files[i].index != f.index) {
            fail(where, "file %zu: %s (entry %u); the file has %s (entry %u)", i,
                 files[i].path.c_str(), files[i].index, f.path.c_str(), f.index);
            continue;
        }
        String path = "/" + f.path;
        String fw = where + ":" + f.path;
        const uint8_t* expanded = f.compressed ? f.expanded.data() : f.data;
        uint32_t expandedLen = f.compressed ? (uint32_t)f.expanded.size() : r.size;
        // Where the mode keeps the stored bytes: a MEM archive returns its data in place, a COMP
        // archive the files of its main-memory part (flag 0x10); everything else is expanded.
        bool stored = mode == JKRArchive::MOUNT_MEM ||
                      (mode == JKRArchive::MOUNT_COMP && (r.flags & 0x10) != 0);
        const uint8_t* fetchWant = stored ? f.data : expanded;
        uint32_t fetchWantLen = stored ? r.size : expandedLen;

        // readResource before the fetch: the read paths of the mode.
        uint32_t bufSize = alignUp32(expandedLen) + 32;
        uint8_t* buf = (uint8_t*)JKRHeap::alloc(bufSize, 32, heap);
        if (buf == nullptr) {
            fail(fw, "no memory for a %u-byte buffer", bufSize);
            continue;
        }
        uint32_t n = arc->readResource(buf, bufSize, path.c_str());
        if (!sameBytes(buf, n, expanded, expandedLen, false)) {
            fail(fw, "readResource before the fetch: %u bytes, not the %u expected", n, expandedLen);
        }

        void* res = arc->getResource(path.c_str());
        sTotals.fetches++;
        if (res == nullptr) {
            fail(fw, "getResource returned NULL");
            JKRHeap::free(buf, heap);
            continue;
        }
        if (!sameBytes((const uint8_t*)res, fetchWantLen, fetchWant, fetchWantLen, true)) {
            fail(fw, "getResource: the %u bytes differ (%s)", fetchWantLen,
                 stored ? "stored" : "expanded");
        }
        sTotals.bytes += fetchWantLen;
        if (arc->getResSize(res) != r.size) {
            fail(fw, "getResSize %u, the entry has %u", (unsigned int)arc->getResSize(res), r.size);
        }
        u32 exp = arc->getExpandedResSize(res);
        u32 expWant = expandedLen;
        if (mode == JKRArchive::MOUNT_COMP && f.compressed && !compExpandTable) {
            expWant = r.size;
        }
        if (exp != expWant) {
            fail(fw, "getExpandedResSize %u, expected %u", (unsigned int)exp, expWant);
        }
        if (arc->getResource(path.c_str()) != res) {
            fail(fw, "a second getResource returned another pointer");
        }
        fetched[f.index] = res;
        if (r.id != 0xFFFF) {
            JKRArchive::SDIFileEntry* byId = arc->findIdResource(r.id);
            if (byId == nullptr || byId->getFileID() != r.id) {
                fail(fw, "findIdResource(%u) found no entry with that ID", r.id);
            }
        }
        if (nameCount[r.name] == 1 && arc->getResource(0, r.name.c_str()) != res) {
            fail(fw, "getResource(0, \"%s\") did not return the fetched resource", r.name.c_str());
        }
        // Once fetched, readResource copies the fetched data: the stored bytes where the mode
        // keeps them.
        memset(buf, 0, bufSize);
        n = arc->readResource(buf, bufSize, path.c_str());
        if (!sameBytes(buf, n, fetchWant, fetchWantLen, false)) {
            fail(fw, "readResource after the fetch: %u bytes, not the %u expected", n, fetchWantLen);
        }
        JKRHeap::free(buf, heap);
        if (nested != nullptr && expandedLen >= 4 && memcmp(expanded, "RARC", 4) == 0) {
            nested->push_back({f.path, Vector<uint8_t>(expanded, expanded + expandedLen)});
        }
        sTotals.files++;
    }

    // d_resorce's lookup: the files of a type's directory through a finder.
    Map<uint32_t, int> typeCount;
    for (const RawNode& n : raw.nodes) {
        typeCount[n.type]++;
    }
    for (uint32_t i = 0; i < numNodes; i++) {
        const RawNode& node = raw.nodes[i];
        if (typeCount[node.type] != 1) {
            continue;
        }
        JKRArcFinder* finder = arc->getFirstResource(node.type);
        for (; JKRIsFileFinderAvailable(finder); finder->findNextFile()) {
            uint32_t k = (uint32_t)finder->mEntryFileIndex;
            if (k < node.first || k >= node.first + node.entries) {
                fail(where, "the finder of %08x gave entry %u outside its directory", node.type, k);
                break;
            }
            const RawEntry& r = raw.entries[k];
            if (r.name != finder->mEntryName || finder->mEntryId != r.id) {
                fail(where, "the finder of %08x: entry %u is %s/%u, the file has %s/%u", node.type,
                     k, finder->mEntryName, finder->mEntryId, r.name.c_str(), r.id);
            }
            if (!(r.flags & 0x01)) {
                continue;
            }
            void* byType = arc->getResource(node.type, finder->mEntryName);
            void* byIndex = fetched.count(k) != 0 ? fetched[k] : nullptr;
            if (byType == nullptr || byType != byIndex) {
                fail(where, "getResource(%08x, \"%s\") %p, the entry's resource %p", node.type,
                     finder->mEntryName, byType, byIndex);
            }
            sTotals.finderLookups++;
        }
        delete finder;
    }

    if (reportFd >= 0 && !reportName.empty()) {
        reportArchive(reportFd, reportName, arc, raw, files, dirs);
    }
}

void sweepArchive(const String& path, JKRHeap* heap, int reportFd) {
    sArchiveErrors = 0;
    RawArc raw;
    if (!rawRead(path.c_str(), raw)) {
        return;
    }
    sTotals.archives++;
    static const int kModes[] = {JKRArchive::MOUNT_MEM, JKRArchive::MOUNT_ARAM,
                                 JKRArchive::MOUNT_DVD, JKRArchive::MOUNT_COMP};
    JKRHeap* sysHeap = JKRHeap::getSystemHeap();
    JKRAramHeap* aramHeap = JKRAram::getAramHeap();
    for (int mode : kModes) {
        String where = path + " " + modeName(mode);
        s32 heapFree = heap->getTotalFreeSize();
        s32 sysFree = sysHeap->getTotalFreeSize();
        u32 aramFree = aramHeap->getTotalFreeSize();
        JKRArchive* arc = JKRArchive::mount(path.c_str(), (JKRArchive::EMountMode)mode, heap,
                                            JKRArchive::MOUNT_DIRECTION_HEAD);
        if (arc == nullptr) {
            fail(where, "mount failed");
            continue;
        }
        sTotals.mounts++;
        Vector<std::pair<String, Vector<uint8_t>>> nested;
        bool report = mode == JKRArchive::MOUNT_MEM;
        checkArchive(arc, mode, raw, where, heap, report ? reportFd : -1, report ? path : "",
                     report ? &nested : nullptr);
        if (report) {
            for (auto& [inner, bytes] : nested) {
                void* res = arc->getResource(("/" + inner).c_str());
                if (res != nullptr && memcmp(res, "RARC", 4) == 0) {
                    checkNested(path + ":" + inner, (const uint8_t*)res, (uint32_t)bytes.size(),
                                heap, reportFd, path + ":" + inner);
                }
            }
        }
        arc->unmount();
        if (heap->getTotalFreeSize() != heapFree) {
            fail(where, "after unmount the test heap lost %d bytes",
                 (int)(heapFree - heap->getTotalFreeSize()));
        }
        if (sysHeap->getTotalFreeSize() != sysFree) {
            fail(where, "after unmount the system heap lost %d bytes",
                 (int)(sysFree - sysHeap->getTotalFreeSize()));
        }
        if (aramHeap->getTotalFreeSize() != aramFree) {
            fail(where, "after unmount the ARAM heap lost %d bytes",
                 (int)(aramFree - aramHeap->getTotalFreeSize()));
        }
    }
    if (sArchiveErrors != 0) {
        writef(STDERR_FILENO, "[cos] arc-sweep: %s: %d error(s)\n", path.c_str(), sArchiveErrors);
    }
}

void findArchives(const String& dirPath, Vector<String>& out, int depth) {
    DVDDir dir;
    if (!DVDOpenDir(dirPath.empty() ? "/" : dirPath.c_str(), &dir)) {
        fail(dirPath, "DVDOpenDir failed");
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
    if (depth < 32) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

} // namespace

[[noreturn]] void smokeArcSweep() {
    Vector<String> archives;
    findArchives("", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr || JKRAram::getAramHeap() == nullptr) {
        writef(STDERR_FILENO, "[cos] arc-sweep: no test heap (%p, root free %d) or no ARAM heap\n",
               (void*)heap, (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("arc_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# arc-sweep (COS_SMOKE=arc-sweep): what the MEM mount of each archive read\n");
    }
    uint64_t start = elapsedMs();
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    if (!heap->check()) {
        fail("test heap", "JKRExpHeap::check failed after the sweep");
    }
    heap->destroy();
    writef(STDERR_FILENO,
           "[cos] arc-sweep: %u archives (%zu found), %u mounts (MEM, ARAM, DVD, COMP), %u nested "
           "mountFixed, %u files checked, %u fetches, %u finder lookups, %llu MiB compared, "
           "%llu ms; %d error(s)%s\n",
           sTotals.archives, archives.size(), sTotals.mounts, sTotals.nested, sTotals.files,
           sTotals.fetches, sTotals.finderLookups, sTotals.bytes >> 20,
           (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (listing in arc_sweep.txt)" : "");
    bool ok = sErrors == 0 && !archives.empty() && sTotals.archives == archives.size();
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
