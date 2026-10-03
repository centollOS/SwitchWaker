// Stage chunk tables on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.9a): the COS_SMOKE=
// stage-sweep test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc under
// /res/Stage (the FST walked with DVDOpenDir/DVDReadDir) is mounted in main RAM, as dRes_info_c
// mounts a stage or room archive, and its stage.dzs or room.dzr is read twice:
// - independently: plain big-endian loads at the format's offsets (the chunk count, each chunk's
//   tag, entry count and file offset; the RTBL entries and their room lists; the paths' point
//   offsets), taken before the game touches the bytes;
// - through the game, in place in the archive's resource as dStage_dt_c_stageInitLoader and
//   dStage_dt_c_roomLoader use it: dStage_dt_c_offsetToPtr relocates the chunk table, then the
//   table is read through dStage_fileHeader/dStage_nodeHeader (count, tags, entry counts, each
//   offset resolved to a pointer that must be the file's base plus the offset, 0 staying null);
//   dStage_dt_c_decode, with a FuncTable naming every tag of the file, must hand each recorder
//   the first chunk of its tag, its entry count and the file; every {num, pointer} chunk struct of
//   d_stage.h laid over each chunk (as the chunk loaders read it, at the node + 4) must give the
//   same count and pointer; then the game's own relocating loaders run through dStage_dt_c_decode
//   into a dStage_stageDt_c (stage.dzs: RTBL, PPNT, PATH, RPPN, RPAT, in dStage_dt_c_stageLoader's
//   order) or a dStage_roomDt_c (room.dzr: RTBL, RPPN, RPAT, as dStage_dt_c_roomLoader), and every
//   RTBL entry, room list and path point table must resolve where the file's offsets say.
// /res/Menu/Menu1.dat (the debug map select, step 4.9a's d_s_menu part) is loaded as d_s_menu's
// phase_2 loads it, relocated through menu_of_scene_class::menu_inf/stage_inf as phase_2 does,
// and every stage and room entry compared with the file's offsets.
// Step 4.9b: the actor records (ACTR/TGOB/PLYR/ACT0-b/TRE0-b through stage_actor_class, TRES
// through stage_tresure_class, SCOB/TGSC/DOOR/TGDR/SCO0-b through stage_tgsc_class) of the first
// chunk of each tag (the one dStage_dt_c_decode hands out) are read through the game's record
// structs and copied into an fopAcM_prm_class as dStage_actorInit (field by field) and
// dStage_tgscInfoInit (the whole base) copy them; both copies must equal the file's big-endian
// fields bit for bit, and the host values go to an ACTOR line.
// Step 4.9c: the records of the first RTBL, STAG, FILI, MULT, SCLS, PATH/PPNT, RPAT/RPPN,
// CAMR/RCAM, AROB/RARO, EVNT, 2DMA/2Dma and SOND chunk are read through the struct their chunk
// loader uses (RTBL after dStage_roomReadInit relocated it), each entry at the file's address,
// and every field goes to a REC line.
// <COS_RUN_DIR>/stage_sweep.txt gets what the game read (STG/CHUNK/ACTOR/REC lines in
// disc_manifest.py's names); native/tools/run.sh compares it with the manifest
// (disc_manifest.py --check-stage).
// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/d_path.h"
#include "d/d_s_menu.h"
#include "d/d_stage.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <utility>

// d_stage.cpp's chunk table code and relocating chunk loaders (external, not in d_stage.h).
void dStage_dt_c_offsetToPtr(void* i_data);
void dStage_dt_c_decode(void* i_data, dStage_dt_c* i_stage, FuncTable* i_funcTbl, int i_tblSize);
int dStage_roomReadInit(dStage_dt_c* i_stage, void* i_data, int i_num, void* i_file);
int dStage_ppntInfoInit(dStage_dt_c* i_stage, void* i_data, int i_num, void* i_file);
int dStage_pathInfoInit(dStage_dt_c* i_stage, void* i_data, int i_num, void* i_file);
int dStage_rppnInfoInit(dStage_dt_c* i_stage, void* i_data, int i_num, void* i_file);
int dStage_rpatInfoInit(dStage_dt_c* i_stage, void* i_data, int i_num, void* i_file);

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 16 * 1024 * 1024;

int sErrors = 0;
const char* sWhere = "";

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] stage-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

String tagText(const void* p) {
    char s[5];
    memcpy(s, p, 4);
    s[4] = 0;
    return s;
}

// ---- the independent reading -------------------------------------------------------------------

struct RawChunk {
    char tag[4];
    int32_t num;
    uint32_t offset;
};

struct RawFile {
    const uint8_t* base;
    uint32_t size;
    Vector<RawChunk> chunks;
    // The first chunk of a tag (the one dStage_dt_c_decode hands out), or -1.
    int find(const char* tag) const {
        for (size_t i = 0; i < chunks.size(); i++) {
            if (memcmp(chunks[i].tag, tag, 4) == 0) {
                return (int)i;
            }
        }
        return -1;
    }
};

bool parseRaw(const uint8_t* b, uint32_t size, RawFile& raw) {
    raw.base = b;
    raw.size = size;
    if (size < 4) {
        fail("%u bytes: no chunk count", size);
        return false;
    }
    int32_t n = (int32_t)rd32(b);
    if (n < 0 || n >= 256 || 4 + (uint32_t)n * 12 > size) {
        fail("chunk count %d in %u bytes", n, size);
        return false;
    }
    for (int32_t i = 0; i < n; i++) {
        const uint8_t* p = b + 4 + i * 12;
        RawChunk c;
        memcpy(c.tag, p, 4);
        c.num = (int32_t)rd32(p + 4);
        c.offset = rd32(p + 8);
        if (c.num < 0 || c.offset > size) {
            fail("chunk %d %s: %d entries at 0x%x in %u bytes", i, tagText(c.tag).c_str(), c.num,
                 c.offset, size);
            return false;
        }
        raw.chunks.push_back(c);
    }
    return true;
}

// The parts of the file that the relocating loaders rewrite, read before they run.
struct RawRelocs {
    Vector<uint32_t> rtblEntries; // file offset of each RTBL entry
    Vector<uint32_t> rtblRooms;   // file offset of each entry's room list
    Vector<uint8_t> rtblNums;     // each entry's room count
    Vector<uint32_t> pathPoints;  // PATH: each path's point offset from the PPNT entries
    Vector<uint32_t> rpatPoints;  // RPAT: the same from the RPPN entries
};

bool inFile(const RawFile& raw, uint32_t off, uint32_t len) {
    return off <= raw.size && len <= raw.size - off;
}

// RTBL and PATH/PPNT are stage data: dStage_roomDt_c asserts on them (setRoom, setPathInfo).
void readRelocs(const RawFile& raw, RawRelocs& r, bool isStage) {
    if (!isStage && (raw.find("RTBL") >= 0 || raw.find("PATH") >= 0 || raw.find("PPNT") >= 0)) {
        fail("a room with stage chunks (RTBL/PATH/PPNT)");
        return;
    }
    int rtbl = raw.find("RTBL");
    if (rtbl >= 0 && raw.chunks[rtbl].offset != 0) {
        const RawChunk& c = raw.chunks[rtbl];
        if (!inFile(raw, c.offset, (uint32_t)c.num * 4)) {
            fail("RTBL: %d entries at 0x%x past the end", c.num, c.offset);
            return;
        }
        for (int32_t i = 0; i < c.num; i++) {
            uint32_t e = rd32(raw.base + c.offset + i * 4);
            if (!inFile(raw, e, 8)) {
                fail("RTBL entry %d at 0x%x past the end", i, e);
                return;
            }
            r.rtblEntries.push_back(e);
            r.rtblNums.push_back(raw.base[e]);
            r.rtblRooms.push_back(rd32(raw.base + e + 4));
        }
    }
    const char* kPaths[2][2] = {{"PATH", "PPNT"}, {"RPAT", "RPPN"}};
    for (int k = 0; k < 2; k++) {
        Vector<uint32_t>& out = k == 0 ? r.pathPoints : r.rpatPoints;
        int path = raw.find(kPaths[k][0]);
        if (path < 0 || raw.chunks[path].offset == 0) {
            continue;
        }
        const RawChunk& c = raw.chunks[path];
        if (!inFile(raw, c.offset, (uint32_t)c.num * 0xC)) {
            fail("%s: %d paths at 0x%x past the end", kPaths[k][0], c.num, c.offset);
            continue;
        }
        if (c.num > 0 && raw.find(kPaths[k][1]) < 0) {
            fail("%s without %s", kPaths[k][0], kPaths[k][1]);
            continue;
        }
        for (int32_t i = 0; i < c.num; i++) {
            out.push_back(rd32(raw.base + c.offset + i * 0xC + 8));
        }
    }
}

// ---- through the game --------------------------------------------------------------------------

struct Decoded {
    const void* node;
    int num;
    const void* file;
    int calls;
};
Vector<Decoded> sDecoded;
int sRecorderIndex = 0;

// One recorder per FuncTable slot: dStage_dt_c_decode calls the slot's function with the chunk.
template <int N>
int recorder(dStage_dt_c*, void* i_data, int i_num, void* i_file) {
    Decoded& d = sDecoded[N];
    d.node = i_data;
    d.num = i_num;
    d.file = i_file;
    d.calls++;
    return 1;
}

constexpr int kMaxTags = 64;

template <int... I>
constexpr dStage_Func recorderAt(int i, std::integer_sequence<int, I...>) {
    constexpr dStage_Func table[] = {recorder<I>...};
    return table[i];
}

dStage_Func recorderFor(int i) {
    return recorderAt(i, std::make_integer_sequence<int, kMaxTags>());
}

// A {num, pointer} chunk struct laid over a chunk as its loader reads it (at the node + 4).
template <class T, class Num, class Ptr>
void checkOverlay(const char* type, const dStage_nodeHeader* node, Num T::*num, Ptr T::*ptr,
                  int wantNum, const void* wantPtr) {
    const T* c = (const T*)((const int*)node + 1);
    int gotNum = c->*num;
    const void* gotPtr = (const void*)(c->*ptr);
    if (gotNum != wantNum || gotPtr != wantPtr) {
        fail("chunk %s through %s: num %d, pointer %p; the node has %d, %p",
             tagText(&node->m_tag).c_str(), type, gotNum, gotPtr, wantNum, wantPtr);
    }
}

#define OVERLAY(T, NUM, PTR) checkOverlay(#T, node, &T::NUM, &T::PTR, wantNum, wantPtr)

void checkOverlays(const dStage_nodeHeader* node, int wantNum, const void* wantPtr) {
    OVERLAY(stage_tresure_class, num, m_entries);
    OVERLAY(stage_scls_info_dummy_class, num, m_entries);
    OVERLAY(stage_map_info_dummy_class, num, m_entries);
    OVERLAY(stage_camera_class, num, m_entries);
    OVERLAY(stage_arrow_class, num, m_entries);
    OVERLAY(stage_actor_class, num, m_entries);
    OVERLAY(stage_tgsc_class, num, m_entries);
    OVERLAY(roomRead_class, num, m_entries);
    OVERLAY(dStage_MemoryMap_c, num, m_entries);
    OVERLAY(dStage_MemoryConfig_c, num, m_entries);
    OVERLAY(dStage_dPath_c, num, m_path);
    OVERLAY(dStage_dPnt_c, num, m_pnt_offset);
    OVERLAY(dStage_Multi_c, num, m_entries);
    OVERLAY(dStage_SoundInfo_c, num, m_entries);
    OVERLAY(dStage_FloorInfo_c, num, m_entries);
    OVERLAY(dStage_Lbnk_c, m_num, m_entries);
    OVERLAY(dStage_DMap_c, num, entries);
    OVERLAY(dStage_EventInfo_c, num, events);
    OVERLAY(dStage_Ship_c, num, m_entries);
}

#undef OVERLAY

struct Totals {
    uint32_t archives = 0, files = 0, chunks = 0, rtbl = 0, paths = 0, actors = 0, records = 0;
};
Totals sTotals;

// A name for stage_sweep.txt: a space, '%' and bytes outside printable ASCII as %XX
// (disc_manifest.py --check-stage decodes them as cstr() does).
String enc(const char* text, size_t limit) {
    String out;
    for (size_t i = 0; i < limit && text[i] != 0; i++) {
        unsigned char c = (unsigned char)text[i];
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

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t floatBits(f32 v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    return u;
}

// Which record struct the game's chunk loader reads a tag's entries through (d_stage.cpp's
// FuncTables): 0 none, 1 stage_actor_class, 2 stage_tresure_class, 3 stage_tgsc_class.
int actorKind(const char* tag) {
    static const char* const kActor[] = {"ACTR", "TGOB", "PLYR"};
    static const char* const kTgsc[] = {"SCOB", "TGSC", "DOOR", "TGDR"};
    for (const char* t : kActor) {
        if (memcmp(tag, t, 4) == 0) {
            return 1;
        }
    }
    if (memcmp(tag, "TRES", 4) == 0) {
        return 2;
    }
    for (const char* t : kTgsc) {
        if (memcmp(tag, t, 4) == 0) {
            return 3;
        }
    }
    // The layer variants: ACT0-b and TRE0-b (dStage_actorInit), SCO0-b (dStage_tgscInfoInit).
    if (strchr("0123456789ab", tag[3]) != nullptr && tag[3] != 0) {
        if (memcmp(tag, "ACT", 3) == 0 || memcmp(tag, "TRE", 3) == 0) {
            return 1;
        }
        if (memcmp(tag, "SCO", 3) == 0) {
            return 3;
        }
    }
    return 0;
}

// One record: the game's struct against the file's bytes, then the two copies into
// fopAcM_prm_class the chunk loaders make.
template <class Data>
void checkRecord(const Data* d, const uint8_t* rawEntry, const char* tag, int k,
                 const String& name, int chunk, int fd) {
    if ((const uint8_t*)d != rawEntry) {
        fail("%s entry %d at %p, the file has it at %p", tag, k, (const void*)d,
             (const void*)rawEntry);
        return;
    }
    // The file's fields, read independently (big-endian at the format's offsets).
    uint32_t wantParams = rd32(rawEntry + 0x08);
    uint32_t wantPos[3] = {rd32(rawEntry + 0x0C), rd32(rawEntry + 0x10), rd32(rawEntry + 0x14)};
    int16_t wantAngle[3] = {(int16_t)rd16(rawEntry + 0x18), (int16_t)rd16(rawEntry + 0x1A),
                            (int16_t)rd16(rawEntry + 0x1C)};
    uint16_t wantSetId = rd16(rawEntry + 0x1E);

    // dStage_actorInit's copy (each field), and dStage_tgscInfoInit's (the whole base).
    fopAcM_prm_class byField;
    byField.base.parameters = d->base.parameters;
    byField.base.position = d->base.position;
    byField.base.angle = d->base.angle;
    byField.base.setID = d->base.setID;
    fopAcM_prm_class whole;
    whole.base = d->base;
    const fopAcM_prmBase_class* copies[2] = {&byField.base, &whole.base};
    for (int c = 0; c < 2; c++) {
        const fopAcM_prmBase_class& b = *copies[c];
        if (b.parameters != wantParams || floatBits(b.position.x) != wantPos[0] ||
            floatBits(b.position.y) != wantPos[1] || floatBits(b.position.z) != wantPos[2] ||
            b.angle.x != wantAngle[0] || b.angle.y != wantAngle[1] ||
            b.angle.z != wantAngle[2] || b.setID != wantSetId) {
            fail("%s entry %d (%.8s), %s copy: params 0x%08x pos %08x %08x %08x angle %d %d %d "
                 "set %u; the file has 0x%08x, %08x %08x %08x, %d %d %d, %u",
                 tag, k, d->name, c == 0 ? "field" : "whole", b.parameters,
                 floatBits(b.position.x), floatBits(b.position.y), floatBits(b.position.z),
                 b.angle.x, b.angle.y, b.angle.z, b.setID, wantParams, wantPos[0], wantPos[1],
                 wantPos[2], wantAngle[0], wantAngle[1], wantAngle[2], wantSetId);
        }
    }
    if (fd >= 0) {
        const fopAcM_prmBase_class& b = whole.base;
        writef(fd, "ACTOR %s %d %d name=%s params=%u pos=%.17g,%.17g,%.17g angle=%d,%d,%d "
                   "set_id=%u\n",
               name.c_str(), chunk, k, enc(d->name, sizeof(d->name)).c_str(), b.parameters,
               (double)b.position.x, (double)b.position.y, (double)b.position.z, b.angle.x,
               b.angle.y, b.angle.z, b.setID);
    }
    sTotals.actors++;
}

// The actor records of the first chunk of each tag, through the struct its loader uses.
void checkActors(const dStage_fileHeader* file, const RawFile& raw, const String& name, int fd) {
    for (size_t i = 0; i < raw.chunks.size(); i++) {
        const RawChunk& c = raw.chunks[i];
        int kind = actorKind(c.tag);
        if (kind == 0 || raw.find(String(tagText(c.tag)).c_str()) != (int)i) {
            continue;
        }
        String tag = tagText(c.tag);
        uint32_t esize = kind == 3 ? sizeof(stage_tgsc_data_class) : sizeof(stage_actor_data_class);
        if (c.num > 0 && (c.offset == 0 || !inFile(raw, c.offset, (uint32_t)c.num * esize))) {
            fail("%s: %d records of 0x%x bytes at 0x%x past the end", tag.c_str(), c.num, esize,
                 c.offset);
            continue;
        }
        const void* node = (const int*)&file->m_nodes[i] + 1;
        for (int32_t k = 0; k < c.num; k++) {
            const uint8_t* rawEntry = raw.base + c.offset + k * esize;
            if (kind == 1) {
                const stage_actor_class* a = (const stage_actor_class*)node;
                checkRecord(&a->m_entries[k], rawEntry, tag.c_str(), k, name, (int)i, fd);
            } else if (kind == 2) {
                const stage_tresure_class* a = (const stage_tresure_class*)node;
                checkRecord(&a->m_entries[k], rawEntry, tag.c_str(), k, name, (int)i, fd);
            } else {
                const stage_tgsc_class* a = (const stage_tgsc_class*)node;
                const stage_tgsc_data_class* d = &a->m_entries[k];
                checkRecord(d, rawEntry, tag.c_str(), k, name, (int)i, fd);
                // dStage_tgscInfoInit's scale copy (u8 each).
                fopAcM_prm_class prm;
                prm.scale = d->scale;
                if (prm.scale.x != rawEntry[0x20] || prm.scale.y != rawEntry[0x21] ||
                    prm.scale.z != rawEntry[0x22]) {
                    fail("%s entry %d: scale %u %u %u differs from the file", tag.c_str(), k,
                         prm.scale.x, prm.scale.y, prm.scale.z);
                }
            }
        }
    }
}

// ---- step 4.9c: the room, file and path records ------------------------------------------------

// One REC line of stage_sweep.txt: "REC <path> <chunk> <entry> tag=XXXX <field>=<value>..."
// (disc_manifest.py check_stage_record): a list comma-separated, an f32 as %.9g (it round-trips),
// a string percent-encoded.
struct RecLine {
    String text;
    RecLine(const String& name, int chunk, int k, const char* tag) {
        char head[64];
        snprintf(head, sizeof(head), " %d %d tag=%.4s", chunk, k, tag);
        text = String("REC ") + name + head;
    }
    void key(const char* name) {
        text += " ";
        text += name;
        text += "=";
    }
    template <class... T>
    void ints(const char* name, T... v) {
        key(name);
        long long vals[] = {(long long)v...};
        for (size_t i = 0; i < sizeof...(v); i++) {
            char n[24];
            snprintf(n, sizeof(n), i == 0 ? "%lld" : ",%lld", vals[i]);
            text += n;
        }
    }
    template <class... T>
    void floats(const char* name, T... v) {
        key(name);
        double vals[] = {(double)(f32)v...};
        for (size_t i = 0; i < sizeof...(v); i++) {
            char n[40];
            snprintf(n, sizeof(n), i == 0 ? "%.9g" : ",%.9g", vals[i]);
            text += n;
        }
    }
    void str(const char* name, const char* v, size_t limit) {
        key(name);
        text += enc(v, limit);
    }
    void write(int fd) {
        if (fd >= 0) {
            writef(fd, "%s\n", text.c_str());
        }
        sTotals.records++;
    }
};

// The entry the game reads must be the file's (its struct the format's size).
template <class Entry>
bool entryAt(const Entry* e, const RawFile& raw, const RawChunk& c, int k, uint32_t esize) {
    const uint8_t* want = raw.base + c.offset + (uint32_t)k * esize;
    if (sizeof(Entry) != esize || (const uint8_t*)e != want) {
        fail("%s entry %d at %p (%zu bytes); the file has it at %p (%u bytes)",
             tagText(c.tag).c_str(), k, (const void*)e, sizeof(Entry), (const void*)want, esize);
        return false;
    }
    return true;
}

// The records of the first chunk of each tag (the one dStage_dt_c_decode hands out), read through
// the struct its chunk loader (d_stage.cpp) uses: STAG and FILI through the node's offset as
// dStage_stagInfoInit and dStage_filiInfoInit, the others through their {num, pointer} struct at
// the node + 4. RTBL is written by checkRelocs (its entries are relocated there).
void checkRecords(const dStage_fileHeader* file, const RawFile& raw, const String& name, int fd) {
    for (size_t i = 0; i < raw.chunks.size(); i++) {
        const RawChunk& c = raw.chunks[i];
        String tag = tagText(c.tag);
        // An offset of 0 is "no data" (dStage_dt_c_offsetToPtr leaves the pointer null): four
        // stage.dzs files have a 2DMA chunk with 1 entry and offset 0.
        if (raw.find(tag.c_str()) != (int)i || c.num == 0 || c.offset == 0) {
            continue;
        }
        const dStage_nodeHeader* nodeHdr = &file->m_nodes[i];
        const void* node = (const int*)nodeHdr + 1;
        auto is = [&](const char* a, const char* b = nullptr) {
            return tag == a || (b != nullptr && tag == b);
        };
        uint32_t esize = is("STAG") ? 0x20 : is("FILI") ? 0x08 : is("MULT", "SCLS") ? 0x0C
                       : is("PATH", "RPAT") ? 0x0C : is("PPNT", "RPPN") ? 0x10
                       : is("CAMR", "RCAM") ? 0x14 : is("AROB", "RARO") ? 0x14
                       : is("EVNT") ? 0x18 : is("2DMA", "2Dma") ? 0x38 : is("SOND") ? 0x1C : 0;
        if (esize == 0) {
            continue;
        }
        if (!inFile(raw, c.offset, (uint32_t)c.num * esize)) {
            fail("%s: %d records of 0x%x bytes at 0x%x past the end", tag.c_str(), c.num, esize,
                 c.offset);
            continue;
        }
        for (int k = 0; k < c.num; k++) {
            RecLine l(name, (int)i, k, tag.c_str());
            if (is("STAG")) {
                const stage_stag_info_class* e =
                    (const stage_stag_info_class*)(const void*)nodeHdr->m_offset + k;
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.floats("near", e->mNearPlane);
                l.floats("far", e->mFarPlane);
                l.ints("camera_tool", e->mCameraMapToolID);
                l.ints("prop", e->mProp);
                l.ints("particle", (u16)e->mParticleSceneNo);
                l.ints("type_schbit", (u32)e->mStageTypeAndSchbit);
                l.ints("schbit_far", (u32)e->mSchbitEnableAndFarPlane);
                l.ints("f14", (u32)e->field_0x14);
                l.ints("f18", (u32)e->field_0x18);
                l.ints("f1c", (u32)e->field_0x1c);
            } else if (is("FILI")) {
                const dStage_FileList_dt_c* e =
                    (const dStage_FileList_dt_c*)(const void*)nodeHdr->m_offset + k;
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.ints("param", (u32)e->mParam);
                l.floats("sea_level", e->mSeaLevel);
            } else if (is("MULT")) {
                const dStage_Mult_info* e = &((const dStage_Multi_c*)node)->m_entries[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.floats("trans", e->mTransX, e->mTransY);
                l.ints("angle", (s16)e->mAngle);
                l.ints("room", e->mRoomNo);
                l.ints("wave_max", e->mWaveMax);
            } else if (is("SCLS")) {
                const stage_scls_info_class* e =
                    &((const stage_scls_info_dummy_class*)node)->m_entries[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.str("stage", e->mStage, sizeof(e->mStage));
                l.ints("start", e->mStart);
                l.ints("room", e->mRoom);
                l.ints("wipe", e->mWipe);
                l.ints("b0b", e->field_0xb);
            } else if (is("PATH", "RPAT")) {
                const dPath* e = &((const dStage_dPath_c*)node)->m_path[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.ints("num", (u16)e->m_num);
                l.ints("next", (u16)e->m_nextID);
                l.ints("args", e->mArg0, e->m_closed, e->field4_0x6, e->field5_0x7);
            } else if (is("PPNT", "RPPN")) {
                const dPnt* e =
                    (const dPnt*)(const void*)((const dStage_dPnt_c*)node)->m_pnt_offset + k;
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                cXyz pos = e->m_position;
                l.ints("args", e->mArg0, e->mArg1, e->mArg2, e->mArg3);
                l.floats("pos", pos.x, pos.y, pos.z);
            } else if (is("CAMR", "RCAM")) {
                const stage_camera2_data_class* e =
                    &((const stage_camera_class*)node)->m_entries[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.str("type", e->m_cam_type, sizeof(e->m_cam_type));
                l.ints("args", e->m_arrow_idx, e->field_0x11, e->field_0x12, e->field_0x13);
            } else if (is("AROB", "RARO")) {
                const stage_arrow_data_class* e = &((const stage_arrow_class*)node)->m_entries[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                cXyz pos = e->position;
                csXyz angle = e->angle;
                l.floats("pos", pos.x, pos.y, pos.z);
                l.ints("angle", angle.x, angle.y, angle.z);
                l.ints("f12", (s16)e->field_0x12);
            } else if (is("EVNT")) {
                const dStage_Event_dt_c* e = &((const dStage_EventInfo_c*)node)->events[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.ints("b00", e->field_0x0);
                l.str("name", e->mName, sizeof(e->mName));
                l.ints("args", e->field_0x10, e->field_0x11, e->field_0x12, e->mSpawnSwitchNo);
                l.ints("b14", e->field_0x14);
                l.ints("args2", e->field_0x15, e->field_0x16, e->field_0x17);
            } else if (is("2DMA", "2Dma")) {
                const stage_map_info_class* e =
                    &((const stage_map_info_dummy_class*)node)->m_entries[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                l.floats("f", e->field_0x00, e->field_0x04, e->field_0x08, e->field_0x0C,
                         e->field_0x10, e->field_0x14, e->field_0x18, e->field_0x1C, e->field_0x20,
                         e->field_0x24, e->field_0x28, e->field_0x2c, e->field_0x30);
                l.ints("bytes", e->field_0x34, e->field_0x35, e->mOceanXZ, e->field_0x37[0]);
            } else if (is("SOND")) {
                const stage_sound_data* e = &((const dStage_SoundInfo_c*)node)->m_entries[k];
                if (!entryAt(e, raw, c, k, esize)) {
                    break;
                }
                Vec pos = e->field_0x8;
                l.str("name", e->field_0x0, sizeof(e->field_0x0));
                l.floats("pos", pos.x, pos.y, pos.z);
                l.ints("bytes", e->field_0x14, e->field_0x15, e->field_0x16, e->field_0x17,
                       e->field_0x18, e->field_0x19, e->field_0x1a);
            }
            l.write(fd);
        }
    }
}

// The relocating loaders through dStage_dt_c_decode, then their results against the file.
void checkRelocs(uint8_t* data, const RawFile& raw, const RawRelocs& r, bool isStage,
                 const String& name, int fd) {
    dStage_stageDt_c stageDt;
    dStage_roomDt_c roomDt;
    dStage_dt_c* dt = isStage ? (dStage_dt_c*)&stageDt : (dStage_dt_c*)&roomDt;
    dt->init();
    static FuncTable sStageTable[] = {
        {"RTBL", dStage_roomReadInit}, {"PPNT", dStage_ppntInfoInit},
        {"PATH", dStage_pathInfoInit}, {"RPPN", dStage_rppnInfoInit},
        {"RPAT", dStage_rpatInfoInit},
    };
    static FuncTable sRoomTable[] = {
        {"RTBL", dStage_roomReadInit},
        {"RPPN", dStage_rppnInfoInit},
        {"RPAT", dStage_rpatInfoInit},
    };
    if (isStage) {
        dStage_dt_c_decode(data, dt, sStageTable, ARRAY_SIZE(sStageTable));
    } else {
        dStage_dt_c_decode(data, dt, sRoomTable, ARRAY_SIZE(sRoomTable));
    }

    roomRead_class* rtbl = isStage ? dt->getRoom() : nullptr;
    if (r.rtblEntries.empty() != (rtbl == nullptr || rtbl->num == 0)) {
        fail("RTBL: %zu entries in the file, the stage data has %p", r.rtblEntries.size(),
             (void*)rtbl);
    } else if (rtbl != nullptr) {
        if ((size_t)(int)rtbl->num != r.rtblEntries.size()) {
            fail("RTBL: %d entries, the file has %zu", (int)rtbl->num, r.rtblEntries.size());
        }
        for (size_t i = 0; i < r.rtblEntries.size() && i < (size_t)(int)rtbl->num; i++) {
            roomRead_data_class* e = rtbl->m_entries[i];
            if ((const uint8_t*)e != raw.base + r.rtblEntries[i]) {
                fail("RTBL entry %zu at %p, the file has 0x%x", i, (void*)e, r.rtblEntries[i]);
                continue;
            }
            if (e->num != r.rtblNums[i] || (const uint8_t*)(u8*)e->m_rooms !=
                                               raw.base + r.rtblRooms[i]) {
                fail("RTBL entry %zu: %u rooms at %p, the file has %u at 0x%x", i, e->num,
                     (void*)(u8*)e->m_rooms, r.rtblNums[i], r.rtblRooms[i]);
                continue;
            }
            // Step 4.9c: the entry's fields and room list, as the game reads them.
            RecLine l(name, raw.find("RTBL"), (int)i, "RTBL");
            l.ints("num", e->num);
            l.ints("b01", e->field_0x1);
            l.ints("b02", e->field_0x2);
            l.key("rooms");
            for (int k = 0; k < e->num; k++) {
                char n[8];
                snprintf(n, sizeof(n), k == 0 ? "%u" : ",%u", ((u8*)e->m_rooms)[k]);
                l.text += n;
            }
            l.write(fd);
        }
    }

    struct PathCase {
        const char* tag;
        const char* pntTag;
        dStage_dPath_c* path;
        dStage_dPnt_c* pnt;
        const Vector<uint32_t>* want;
    };
    PathCase cases[2] = {
        {"PATH", "PPNT", isStage ? dt->getPathInf() : nullptr,
         isStage ? dt->getPntInf() : nullptr, &r.pathPoints},
        {"RPAT", "RPPN", dt->getPath2Inf(), dt->getPnt2Inf(), &r.rpatPoints},
    };
    for (const PathCase& pc : cases) {
        int pntChunk = raw.find(pc.pntTag);
        if (pc.want->empty()) {
            continue;
        }
        if (pc.path == nullptr || pc.pnt == nullptr || pntChunk < 0) {
            fail("%s: %zu paths in the file, the stage data has %p / %s %p", pc.tag,
                 pc.want->size(), (void*)pc.path, pc.pntTag, (void*)pc.pnt);
            continue;
        }
        const uint8_t* pntBase = raw.base + raw.chunks[pntChunk].offset;
        dPath* paths = pc.path->m_path;
        for (size_t i = 0; i < pc.want->size(); i++) {
            const uint8_t* got = (const uint8_t*)(dPnt*)paths[i].m_points;
            if (got != pntBase + (*pc.want)[i]) {
                fail("%s path %zu: points at %p, the file has %s + 0x%x", pc.tag, i,
                     (const void*)got, pc.pntTag, (*pc.want)[i]);
            }
        }
    }
}

void sweepData(const String& name, uint8_t* data, uint32_t size, bool isStage, int fd) {
    RawFile raw;
    if (!parseRaw(data, size, raw)) {
        return;
    }
    RawRelocs relocs;
    readRelocs(raw, relocs, isStage);

    // The game's relocation of the chunk table, in place.
    dStage_dt_c_offsetToPtr(data);
    dStage_fileHeader* file = (dStage_fileHeader*)data;
    int count = file->m_chunkCount;
    if (count != (int)raw.chunks.size()) {
        fail("m_chunkCount %d, the file has %zu", count, raw.chunks.size());
        return;
    }
    if (fd >= 0) {
        writef(fd, "STG %s chunk_count=%d\n", name.c_str(), count);
    }
    for (int i = 0; i < count; i++) {
        const dStage_nodeHeader* node = &file->m_nodes[i];
        const RawChunk& c = raw.chunks[i];
        String tag = tagText(&node->m_tag);
        int num = node->m_entryNum;
        const void* ptr = (const void*)node->m_offset;
        const void* want = c.offset != 0 ? raw.base + c.offset : nullptr;
        if (memcmp(&node->m_tag, c.tag, 4) != 0 || num != c.num || ptr != want) {
            fail("chunk %d: %s, %d entries at %p; the file has %s, %d at 0x%x", i, tag.c_str(),
                 num, ptr, tagText(c.tag).c_str(), c.num, c.offset);
        }
        checkOverlays(node, c.num, want);
        // The offset the game resolved, back as a file offset (0 for a null offset).
        uint32_t offset = ptr != nullptr ? (uint32_t)((const uint8_t*)ptr - raw.base) : 0;
        if (fd >= 0) {
            writef(fd, "CHUNK %s %d tag=%s num=%d offset=%u\n", name.c_str(), i, tag.c_str(), num,
                   offset);
        }
        sTotals.chunks++;
    }

    // dStage_dt_c_decode: a FuncTable with every distinct tag of the file.
    Vector<String> tags;
    for (const RawChunk& c : raw.chunks) {
        String t = tagText(c.tag);
        bool seen = false;
        for (const String& s : tags) {
            seen = seen || s == t;
        }
        if (!seen) {
            tags.push_back(t);
        }
    }
    if (tags.size() > (size_t)kMaxTags) {
        fail("%zu distinct tags", tags.size());
        return;
    }
    Vector<FuncTable> table(tags.size());
    sDecoded.assign(tags.size(), Decoded{nullptr, 0, nullptr, 0});
    for (size_t i = 0; i < tags.size(); i++) {
        memcpy(table[i].identifier, tags[i].c_str(), 5);
        table[i].function = recorderFor((int)i);
    }
    dStage_dt_c_decode(data, nullptr, table.data(), (int)table.size());
    for (size_t i = 0; i < tags.size(); i++) {
        int first = raw.find(tags[i].c_str());
        const Decoded& d = sDecoded[i];
        if (d.calls != 1 || d.node != &file->m_nodes[first] || d.num != raw.chunks[first].num ||
            d.file != data) {
            fail("decode %s: %d call(s) with node %p, %d entries, file %p; chunk %d is at %p "
                 "with %d", tags[i].c_str(), d.calls, d.node, d.num, d.file, first,
                 (const void*)&file->m_nodes[first], raw.chunks[first].num);
        }
    }

    checkActors(file, raw, name, fd);
    checkRecords(file, raw, name, fd);
    checkRelocs(data, raw, relocs, isStage, name, fd);
    sTotals.rtbl += relocs.rtblEntries.size();
    sTotals.paths += relocs.pathPoints.size() + relocs.rpatPoints.size();
    sTotals.files++;
}

void sweepArchive(const String& path, JKRHeap* heap, int fd) {
    sWhere = path.c_str();
    s32 heapFree = heap->getTotalFreeSize();
    // A stage or room archive, mounted as dRes_info_c mounts it (main RAM).
    JKRArchive* arc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_MEM, heap,
                                        JKRArchive::MOUNT_DIRECTION_HEAD);
    if (arc == nullptr) {
        fail("mount failed");
        return;
    }
    sTotals.archives++;
    static const struct {
        const char* inner;
        bool isStage;
    } kFiles[] = {{"dzs/stage.dzs", true}, {"dzr/room.dzr", false}};
    for (const auto& f : kFiles) {
        void* res = arc->getResource((String("/") + f.inner).c_str());
        if (res == nullptr) {
            continue;
        }
        String name = path + ":" + f.inner;
        sWhere = name.c_str();
        sweepData(name, (uint8_t*)res, arc->getResSize(res), f.isStage, fd);
        sWhere = path.c_str();
    }
    arc->unmount();
    if (heap->getTotalFreeSize() != heapFree) {
        fail("after unmount the sweep heap lost %d bytes",
             (int)(heapFree - heap->getTotalFreeSize()));
    }
}

void findStageArchives(const String& dirPath, Vector<String>& out, int depth) {
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
    if (depth < 4) {
        for (const String& d : subdirs) {
            findStageArchives(d, out, depth + 1);
        }
    }
}

// /res/Menu/Menu1.dat through menu_of_scene_class's structs, relocated as d_s_menu's phase_2
// does, against its offsets read independently.
void sweepMenu(JKRHeap* heap) {
    const char* path = "/res/Menu/Menu1.dat";
    sWhere = path;
    // phase_1/phase_2 load it with mDoDvdThd_toMainRam_c (JKRDvdToMainRam into the heap).
    uint8_t* bytes = (uint8_t*)JKRDvdToMainRam(path, NULL, EXPAND_SWITCH_UNKNOWN1, 0, heap,
                                               JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, NULL);
    int size = bytes != nullptr ? heap->getSize(bytes) : 0;
    if (bytes == nullptr || size < 8) {
        fail("JKRDvdToMainRam failed");
        return;
    }
    uint32_t num = bytes[0];
    uint32_t stageOff = rd32(bytes + 4);
    constexpr uint32_t kStage = 0x28, kRoom = 0x2C;
    if (num == 0 || stageOff + num * kStage > (uint32_t)size) {
        fail("%u stages at 0x%x in %d bytes", num, stageOff, size);
        JKRFree(bytes);
        return;
    }
    Vector<uint32_t> roomOffs;
    for (uint32_t i = 0; i < num; i++) {
        roomOffs.push_back(rd32(bytes + stageOff + i * kStage + 0x24));
    }

    menu_of_scene_class::menu_inf* info = (menu_of_scene_class::menu_inf*)bytes;
    // phase_2's relocation (d_s_menu.cpp), on the same struct types.
    info->stage.setBase(info);
    for (int i = 0; i < info->num; i++) {
        info->stage[i].roomPtr.setBase(info);
    }

    if (info->num != num || (uint8_t*)(menu_of_scene_class::stage_inf*)info->stage !=
                                bytes + stageOff) {
        fail("%u stages at %p; the file has %u at 0x%x", info->num,
             (void*)(menu_of_scene_class::stage_inf*)info->stage, num, stageOff);
    }
    uint32_t rooms = 0;
    for (uint32_t i = 0; i < num; i++) {
        const menu_of_scene_class::stage_inf& st = info->stage[i];
        const uint8_t* rawStage = bytes + stageOff + i * kStage;
        const menu_of_scene_class::room_inf* room = st.roomPtr;
        if ((const uint8_t*)&st != rawStage || st.roomNum != rawStage[0x21] ||
            (const uint8_t*)room != bytes + roomOffs[i] || memchr(st.name, 0, sizeof(st.name)) ==
                                                             nullptr) {
            fail("stage %u: %u rooms at %p; the file has %u at 0x%x", i, st.roomNum,
                 (const void*)room, rawStage[0x21], roomOffs[i]);
            continue;
        }
        if (roomOffs[i] + st.roomNum * kRoom > (uint32_t)size) {
            fail("stage %u: %u rooms at 0x%x past the end", i, st.roomNum, roomOffs[i]);
            continue;
        }
        for (uint32_t k = 0; k < st.roomNum; k++) {
            const uint8_t* rawRoom = bytes + roomOffs[i] + k * kRoom;
            const menu_of_scene_class::room_inf& rm = room[k];
            if (memcmp(rm.stageName, rawRoom + 0x21, 8) != 0 || rm.roomNo != (s8)rawRoom[0x29] ||
                rm.startCode != rawRoom[0x2A] || rm.layerNo != (s8)rawRoom[0x2B]) {
                fail("stage %u room %u: %.8s room %d point %u layer %d differs from the file", i,
                     k, rm.stageName, rm.roomNo, rm.startCode, rm.layerNo);
            }
            rooms++;
        }
    }
    writef(STDERR_FILENO, "[cos] stage-sweep: %s: %u stages, %u rooms\n", path, num, rooms);
    JKRFree(bytes);
}

} // namespace

[[noreturn]] void smokeStageSweep() {
    uint64_t start = elapsedMs();
    sWhere = "stage-sweep";
    Vector<String> archives;
    findStageArchives("/res/Stage", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] stage-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("stage_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# stage-sweep (COS_SMOKE=stage-sweep): the dzs/dzr chunk tables as the game "
                   "read them (and the actor, room, file and path records), in "
                   "disc_manifest.py's names\n");
    }
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sweepMenu(heap);
    sWhere = "stage-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    writef(STDERR_FILENO,
           "[cos] stage-sweep: %u archives, %u dzs/dzr files, %u chunks, %u actor records, %u "
           "room/file/path records, %u RTBL entries, %u paths relocated; %llu ms; %d error(s)%s\n",
           sTotals.archives, sTotals.files, sTotals.chunks, sTotals.actors, sTotals.records,
           sTotals.rtbl, sTotals.paths,
           (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in stage_sweep.txt)" : "");
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && sTotals.files > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
