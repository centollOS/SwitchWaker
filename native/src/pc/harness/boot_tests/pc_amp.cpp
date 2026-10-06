// AGB floor maps on the host (docs/NATIVE_PORT_PLAN.md, fix F2-agb-map): the COS_SMOKE=
// amp-sweep test.
//
// The dungeon floor maps m<N>.amp in the room archives are AGB (Game Boy Advance) data, stored
// little-endian; the game reads their 16- and 32-bit fields through mDoLib_cnvind16/32 (map_dt_c in
// d_map.cpp), and writes the GBA link buffers (dMap_c's mAgbSendBuf) through the same functions.
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc under
// /res/Stage (the FST walked with DVDOpenDir/DVDReadDir) is mounted in main RAM, as dRes_info_c
// mounts a room archive; each m<N>.amp and its m<N>.bti are read
// - independently: plain little-endian loads of the .amp's bytes at the format's offsets (0x00/0x02
//   the map's pixel width and height, 0x0C the size of the tile graphics after the 0x3C-byte
//   header, 0x30/0x31 the map's width and height in 8x8 tiles, 0x34 the offset of the tile map,
//   0x38 its size) and big-endian loads of the .bti's width and height;
// - through the game: dMap_2DAGBScrDsp_c::init (as dMap_RoomInfo_c sets up a floor map) with the
//   two resources, then the values its callers and its draw read: getMapDtSize, the pixel size
//   (mDoLib_cnvind16 of field_0x0/0x2, as dMap_RoomInfo_c::init), the tile map offset
//   (mDoLib_cnvind32 of field_0x34) and every tile's info word (mDoLib_cnvind16 of each entry, as
//   draw reads it).
// Both readings must agree, and the values must be sane: the file is header + graphics + tile map
// exactly, the pixel size fits the tile count, and every tile's texture cell (info bits 0-3 and
// 4-9) lies inside m<N>.bti. Then a value written into a buffer as dMap_c writes the GBA buffers
// must give its little-endian bytes. Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "d/d_map.h"
#include "m_Do/m_Do_lib.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 16 * 1024 * 1024;
constexpr uint32_t kAmpHeader = 0x3C;

int sErrors = 0;
const char* sWhere = "";

struct Totals {
    unsigned int archives;
    unsigned int maps;
    unsigned int tiles;
} sTotals;

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] amp-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

void checkMap(uint8_t* amp, uint32_t size, ResTIMG* bti, uint32_t btiSize) {
    if (size < kAmpHeader || btiSize < 0x20) {
        fail("%u bytes, m*.bti %u bytes: too short", size, btiSize);
        return;
    }
    if (memcmp(amp, "Yaz0", 4) == 0) {
        fail("compressed in the archive");
        return;
    }
    // Independent reading, before the game touches the bytes.
    const uint32_t pixW = le16(amp + 0x00), pixH = le16(amp + 0x02);
    const uint32_t gfxSize = le32(amp + 0x0C), mapSize = le32(amp + 0x38);
    const uint32_t mapW = amp[0x30], mapH = amp[0x31], mapOff = le32(amp + 0x34);
    const uint32_t texW = be16((const uint8_t*)bti + 2), texH = be16((const uint8_t*)bti + 4);
    if ((uint64_t)kAmpHeader + gfxSize + mapSize != size || mapOff != kAmpHeader + gfxSize ||
        mapSize != mapW * mapH * 2) {
        fail("header 0x3c + graphics %u + tile map %u (%u x %u tiles at 0x%x) is not the %u-byte "
             "file", gfxSize, mapSize, mapW, mapH, mapOff, size);
        return;
    }
    if (!(pixW > (mapW - 1) * 8 && pixW <= mapW * 8 && pixH > (mapH - 1) * 8 && pixH <= mapH * 8)) {
        fail("%u x %u pixels do not fit %u x %u tiles", pixW, pixH, mapW, mapH);
    }

    // Through the game.
    map_dt_c* dt = (map_dt_c*)amp;
    dMap_2DAGBScrDsp_c scr;
    scr.init(dt, bti, 0.0f, 0.0f, 0, 0, 640, 480, 1.0f, 1.0f, 0xFF);
    if (scr.getMapDt() != dt || (uint32_t)scr.getMapDtSize() != size) {
        fail("getMapDtSize %d, the file has %u bytes", scr.getMapDtSize(), size);
    }
    uint32_t gamePixW = mDoLib_cnvind16(dt->field_0x0), gamePixH = mDoLib_cnvind16(dt->field_0x2);
    if (gamePixW != pixW || gamePixH != pixH) {
        fail("map size %u x %u, the file has %u x %u", gamePixW, gamePixH, pixW, pixH);
    }
    uint32_t gameOff = mDoLib_cnvind32(dt->field_0x34);
    if (gameOff != mapOff || dt->field_0x30 != mapW || dt->field_0x31 != mapH) {
        fail("tile map %u x %u at 0x%x, the file has %u x %u at 0x%x", dt->field_0x30,
             dt->field_0x31, gameOff, mapW, mapH, mapOff);
        return;
    }
    if (scr.mImg->width != texW || scr.mImg->height != texH) {
        fail("texture %u x %u, m*.bti has %u x %u", (unsigned)scr.mImg->width,
             (unsigned)scr.mImg->height, texW, texH);
    }
    const uint32_t cellsX = (texW + 7) / 8, cellsY = (texH + 7) / 8;
    // draw's walk: row = mapBase + tileY * mapW, info = mDoLib_cnvind16(row[tileX]).
    u16* mapBase = (u16*)((u8*)scr.getMapDt() + gameOff);
    int bad = 0;
    for (uint32_t y = 0; y < mapH; y++) {
        u16* row = mapBase + y * dt->field_0x30;
        for (uint32_t x = 0; x < mapW; x++) {
            u16 info = mDoLib_cnvind16(row[x]);
            uint16_t want = le16(amp + mapOff + (y * mapW + x) * 2);
            uint32_t cellS = info & 0xF, cellT = (info >> 4) & 0x3F;
            if (info != want || cellS >= cellsX || cellT >= cellsY) {
                if (bad++ < 3) {
                    fail("tile (%u, %u): info 0x%04x (cell %u, %u), the file has 0x%04x; the "
                         "texture has %u x %u cells", x, y, info, cellS, cellT, want, cellsX,
                         cellsY);
                }
            }
            sTotals.tiles++;
        }
    }
    sTotals.maps++;
}

void walkArchive(JKRArchive* arc, uint32_t node, const String& prefix, int depth) {
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
                walkArchive(arc, e->data_offset, prefix + name + "/", depth + 1);
            }
            continue;
        }
        size_t len = strlen(name);
        if (len < 5 || strcasecmp(name + len - 4, ".amp") != 0) {
            continue;
        }
        String inner = prefix + name;
        String where = String(sWhere) + ":" + inner;
        const char* outer = sWhere;
        sWhere = where.c_str();
        // dMap_RoomInfo_c::getRoomImage fetches m<N>.amp and m<N>.bti from the room archive by name.
        String btiName = String(name, len - 4) + ".bti";
        uint8_t* amp = (uint8_t*)arc->getResource(("/" + inner).c_str());
        ResTIMG* bti = (ResTIMG*)arc->getResource(0, btiName.c_str());
        if (amp == nullptr || bti == nullptr) {
            fail("getResource: amp %p, %s %p", (void*)amp, btiName.c_str(), (void*)bti);
        } else {
            checkMap(amp, arc->getResSize(amp), bti, arc->getResSize(bti));
        }
        sWhere = outer;
    }
}

void sweepArchive(const String& path, JKRHeap* heap) {
    sWhere = path.c_str();
    s32 heapFree = heap->getTotalFreeSize();
    JKRArchive* arc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_MEM, heap,
                                        JKRArchive::MOUNT_DIRECTION_HEAD);
    if (arc == nullptr) {
        fail("mount failed");
        return;
    }
    sTotals.archives++;
    walkArchive(arc, 0, "", 0);
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
    if (depth < 4) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

// The GBA buffers: dMap_c stores mDoLib_cnvind16/32 of a value as a u16/u32 in mAgbSendBuf, which
// goes to the GBA byte for byte; the bytes must be the value's little-endian ones.
void checkGbaBuffer() {
    sWhere = "GBA buffer";
    u8 buf[8];
    *(u16*)(buf + 0) = mDoLib_cnvind16((u16)0x1234);
    *(u32*)(buf + 4) = mDoLib_cnvind32(0x89ABCDEFu);
    static const u8 want[8] = {0x34, 0x12, 0, 0, 0xEF, 0xCD, 0xAB, 0x89};
    if (memcmp(buf, want, 2) != 0 || memcmp(buf + 4, want + 4, 4) != 0) {
        fail("0x1234 and 0x89abcdef stored as %02x %02x and %02x %02x %02x %02x", buf[0], buf[1],
             buf[4], buf[5], buf[6], buf[7]);
    }
}

} // namespace

[[noreturn]] void smokeAmpSweep() {
    uint64_t start = elapsedMs();
    sWhere = "amp-sweep";
    Vector<String> archives;
    findArchives("/res/Stage", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] amp-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    for (const String& path : archives) {
        sweepArchive(path, heap);
        pc_frame_tick();
    }
    checkGbaBuffer();
    sWhere = "amp-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    writef(STDERR_FILENO,
           "[cos] amp-sweep: %u archives, %u floor maps, %u tiles; %llu ms; %d error(s)\n",
           sTotals.archives, sTotals.maps, sTotals.tiles,
           (unsigned long long)(elapsedMs() - start), sErrors);
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && sTotals.maps > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
