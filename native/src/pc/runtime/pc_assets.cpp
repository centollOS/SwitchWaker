// The game's embedded data arrays read from the player's disc at start-up (docs/RUNTIME_ASSETS.md).
//
// A COS_RUNTIME_ASSETS=ON build compiles the game against the stub assets/ headers of
// native/tools/gen_assets.sh --stubs: the same 163 arrays (display lists, textures, vertex data the
// decompilation cuts out of main.dol and the RELs) with no contents, each followed by
// COS_ASSET_FILL (assets/cos_assets.h). Here:
// - cos_asset_fill records an array until the disc has been read (namespace-scope arrays, during
//   static initialisation), and copies its bytes in at once after that (arrays inside function
//   bodies register on the function's first call);
// - loadAssets (pc_main.cpp, right after aurora_dvd_open, before OSInit and the game's code) reads
//   main.dol (DVDGetDOLLocation) and the RELs the table names (files/rels/*.rel and RELS.arc, Yaz0
//   expanded), checks each against the SHA-1 the decompilation records for it, cuts every asset
//   out (main.dol: by address through the DOL header; REL: section table + offset) and fills the
//   arrays recorded so far. Vec and cXy arrays get their floats byte-swapped, as the decomp's
//   converters wrote them as numbers; raw arrays (textures, display lists) stay big-endian, as the
//   generated headers have them.
// - a disc whose main.dol or RELs do not match exits with PC_EXIT_DISC, as the disc check does.
// COS_SMOKE=assets writes every asset's bytes (after conversion) to <COS_RUN_DIR>/assets/ and
// exits: native/tools/check_runtime_assets.py compares them with the decomp's extracted .bin files.
#include "pc_internal.h"

#include "cos_sdk/host_alloc.h"

#include <dolphin/dvd.h>

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

#if COS_RUNTIME_ASSETS

namespace {

enum CosAssetKind { COS_ASSET_RAW = 0, COS_ASSET_F32 = 1 };

struct Row {
    const char* name;
    const char* file;    // disc path ("" = main.dol)
    const char* arcFile; // path inside that RARC ("" = the file itself)
    uint32_t section;    // REL section (0: offset is a main.dol address)
    uint32_t offset;
    uint32_t size;
    int kind;
    const char* sha1; // of the decompressed main.dol or REL
};

const Row kRows[] = {
#define COS_ASSET_ROW(name, file, arc, section, offset, size, kind, sha) {name, file, arc, section, offset, size, kind, sha},
#include "assets/cos_asset_table.inc"
#undef COS_ASSET_ROW
};
constexpr uint32_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);

struct Pending {
    void* array;
    uint32_t size;
    uint32_t id;
};

// Constructed on first use: cos_asset_fill runs during other units' static initialisation.
struct State {
    std::mutex lock;
    bool loaded = false;
    cos_sdk::HostVector<cos_sdk::HostVector<uint8_t>> data; // per row, after conversion
    cos_sdk::HostVector<Pending> pending;
    uint32_t filled = 0;
};
State& state() {
    static State* s = cos_sdk::HostNew<State>(); // never destroyed
    return *s;
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

[[noreturn]] void discError(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void discError(const char* fmt, ...) {
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    pc::writef(STDERR_FILENO, "[cos] DISC: %s (the game's data arrays come from the GZLE01 revision 0 disc)\n", text);
    pc_exit(PC_EXIT_DISC);
    __builtin_unreachable();
}

// ---- SHA-1 (FIPS 180-1), to check main.dol and the RELs against the decompilation's hashes
struct Sha1 {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buf[64];
    uint64_t len = 0;
    static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
    void block(const uint8_t* p) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = rd32(p + 4 * i);
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            const uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rol(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    void update(const uint8_t* p, size_t n) {
        size_t used = len % 64;
        len += n;
        while (n > 0) {
            const size_t take = n < 64 - used ? n : 64 - used;
            memcpy(buf + used, p, take);
            used += take, p += take, n -= take;
            if (used == 64) block(buf), used = 0;
        }
    }
    void hex(char out[41]) {
        const uint64_t bits = len * 8;
        const uint8_t pad = 0x80, zero = 0;
        update(&pad, 1);
        while (len % 64 != 56) update(&zero, 1);
        uint8_t l[8];
        for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i));
        update(l, 8);
        for (int i = 0; i < 5; i++) snprintf(out + 8 * i, 9, "%08x", h[i]);
    }
};

bool sha1Is(const uint8_t* p, size_t n, const char* want) {
    Sha1 s;
    s.update(p, n);
    char got[41];
    s.hex(got);
    return strcmp(got, want) == 0;
}

// ---- Yaz0 and RARC (the game's JKR code cannot run this early)
bool yaz0Expand(const uint8_t* src, size_t srcLen, cos_sdk::HostVector<uint8_t>& out) {
    if (srcLen < 16 || memcmp(src, "Yaz0", 4) != 0) return false;
    const uint32_t size = rd32(src + 4);
    out.assign(size, 0);
    size_t sp = 16;
    uint32_t dp = 0;
    while (dp < size) {
        if (sp >= srcLen) return false;
        uint8_t code = src[sp++];
        for (int bit = 0; bit < 8 && dp < size; bit++, code <<= 1) {
            if (code & 0x80) {
                if (sp >= srcLen) return false;
                out[dp++] = src[sp++];
                continue;
            }
            if (sp + 1 >= srcLen) return false;
            const uint32_t dist = (((uint32_t)(src[sp] & 0x0F) << 8) | src[sp + 1]) + 1;
            uint32_t n = src[sp] >> 4;
            sp += 2;
            if (n == 0) {
                if (sp >= srcLen) return false;
                n = src[sp++] + 0x12;
            } else {
                n += 2;
            }
            if (dist > dp || dp + n > size) return false;
            for (uint32_t i = 0; i < n; i++, dp++) out[dp] = out[dp - dist];
        }
    }
    return true;
}

bool readDiscFile(const char* path, cos_sdk::HostVector<uint8_t>& out) {
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) return false;
    cos_sdk::HostVector<uint8_t> file(info.length);
    const s32 got = DVDReadPrio(&info, file.data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    if (got != (s32)info.length) return false;
    if (file.size() >= 4 && memcmp(file.data(), "Yaz0", 4) == 0) return yaz0Expand(file.data(), file.size(), out);
    out = std::move(file);
    return true;
}

// The file at `want` ("rels/mmem/x.rel", relative to the root node) inside a RARC, Yaz0 expanded.
bool rarcFind(const cos_sdk::HostVector<uint8_t>& arc, const char* want, cos_sdk::HostVector<uint8_t>& out) {
    const uint8_t* b = arc.data();
    const size_t len = arc.size();
    if (len < 0x40 || memcmp(b, "RARC", 4) != 0) return false;
    const uint32_t headerLength = rd32(b + 0x08), fileData = rd32(b + 0x0C);
    const uint8_t* info = b + headerLength;
    const uint32_t numNodes = rd32(info), nodeOff = rd32(info + 4), numEntries = rd32(info + 8),
                   entryOff = rd32(info + 0x0C), stringOff = rd32(info + 0x14);
    if (headerLength + (uint64_t)nodeOff + numNodes * 0x10ull > len ||
        headerLength + (uint64_t)entryOff + numEntries * 0x14ull > len || headerLength + (uint64_t)stringOff > len)
        return false;
    const char* strings = (const char*)info + stringOff;
    const size_t stringsLen = len - headerLength - stringOff;
    const uint8_t* data = b + headerLength + fileData;
    const size_t dataLen = len - headerLength - fileData;
    // walk from the root node, one path component at a time; dtk's paths start with the root's name
    uint32_t node = 0;
    const char* p = want;
    if (numNodes > 0) {
        const uint32_t rootName = rd32(info + nodeOff + 4);
        const char* slash = strchr(p, '/');
        if (slash && rootName < stringsLen && strnlen(strings + rootName, stringsLen - rootName) == (size_t)(slash - p) &&
            memcmp(strings + rootName, p, (size_t)(slash - p)) == 0)
            p = slash + 1;
    }
    for (int depth = 0; depth < 16 && node < numNodes; depth++) {
        const char* slash = strchr(p, '/');
        const size_t partLen = slash ? (size_t)(slash - p) : strlen(p);
        const uint8_t* n = info + nodeOff + node * 0x10;
        const uint16_t count = rd16(n + 0x0A);
        const uint32_t first = rd32(n + 0x0C);
        bool found = false;
        for (uint32_t k = first; k < first + count && k < numEntries; k++) {
            const uint8_t* e = info + entryOff + k * 0x14;
            const uint32_t tf = rd32(e + 4);
            const uint32_t nameOff = tf & 0xFFFFFF;
            if (nameOff >= stringsLen) continue;
            const char* name = strings + nameOff;
            if (strnlen(name, stringsLen - nameOff) != partLen || memcmp(name, p, partLen) != 0) continue;
            const uint8_t flags = (uint8_t)(tf >> 24);
            const uint32_t off = rd32(e + 8), size = rd32(e + 0x0C);
            if (slash) {
                if (!(flags & 0x02)) return false;
                node = off;
                p = slash + 1;
                found = true;
                break;
            }
            if ((flags & 0x02) || (uint64_t)off + size > dataLen) return false;
            if (size >= 4 && memcmp(data + off, "Yaz0", 4) == 0) return yaz0Expand(data + off, size, out);
            out.assign(data + off, data + off + size);
            return true;
        }
        if (!found) return false;
    }
    return false;
}

// main.dol: the file bytes holding [addr, addr + size), through the DOL header's sections
const uint8_t* dolBytes(const uint8_t* dol, size_t dolLen, uint32_t addr, uint32_t size) {
    if (dolLen < 0x100) return nullptr;
    for (int i = 0; i < 18; i++) {
        const uint32_t off = rd32(dol + 4 * i), base = rd32(dol + 0x48 + 4 * i), len = rd32(dol + 0x90 + 4 * i);
        if (len == 0 || addr < base || (uint64_t)addr + size > (uint64_t)base + len) continue;
        if ((uint64_t)off + (addr - base) + size > dolLen) return nullptr;
        return dol + off + (addr - base);
    }
    return nullptr;
}

// a REL: section `section`'s bytes at `offset`
const uint8_t* relBytes(const cos_sdk::HostVector<uint8_t>& rel, uint32_t section, uint32_t offset, uint32_t size) {
    if (rel.size() < 0x40) return nullptr;
    const uint32_t numSections = rd32(rel.data() + 0x0C), infoOff = rd32(rel.data() + 0x10);
    if (section >= numSections || (uint64_t)infoOff + 8ull * numSections > rel.size()) return nullptr;
    const uint8_t* s = rel.data() + infoOff + 8 * section;
    const uint32_t secOff = rd32(s) & ~1u, secLen = rd32(s + 4);
    if (secOff == 0 || (uint64_t)offset + size > secLen || (uint64_t)secOff + offset + size > rel.size()) return nullptr;
    return rel.data() + secOff + offset;
}

void fillLocked(State& st, void* array, uint32_t size, uint32_t id) {
    if (id >= kRowCount || size != kRows[id].size || st.data[id].size() != size) {
        pc::writef(STDERR_FILENO, "[cos] assets: array %u (%s) is %u bytes, the table says %u\n", id,
                   id < kRowCount ? kRows[id].name : "?", size, id < kRowCount ? kRows[id].size : 0);
        pc_exit(PC_EXIT_DISC);
    }
    memcpy(array, st.data[id].data(), size);
    st.filled++;
}

} // namespace

bool cos_asset_fill(void* array, unsigned int size, unsigned int id) {
    State& st = state();
    std::lock_guard<std::mutex> guard(st.lock);
    if (st.loaded) {
        fillLocked(st, array, size, id);
    } else {
        st.pending.push_back({array, size, id});
    }
    return true;
}

namespace pc {

void loadAssets() {
    State& st = state();
    s32 dolLen = 0;
    const uint8_t* dol = DVDGetDOLLocation(&dolLen);
    if (dol == nullptr || dolLen <= 0) discError("main.dol cannot be read from the disc");
    st.data.resize(kRowCount);
    bool dolChecked = false;
    const char* curFile = nullptr;
    const char* curArc = nullptr;
    cos_sdk::HostVector<uint8_t> container, rel;
    for (uint32_t i = 0; i < kRowCount; i++) {
        const Row& r = kRows[i];
        const uint8_t* src;
        if (r.file[0] == '\0') {
            if (!dolChecked) {
                if (!sha1Is(dol, (size_t)dolLen, r.sha1)) discError("main.dol is not GZLE01 revision 0's (SHA-1)");
                dolChecked = true;
            }
            src = dolBytes(dol, (size_t)dolLen, r.offset, r.size);
        } else {
            // the table lists a module's assets together: read each REL once
            if (curFile == nullptr || strcmp(curFile, r.file) != 0 || strcmp(curArc, r.arcFile) != 0) {
                if (curFile == nullptr || strcmp(curFile, r.file) != 0) {
                    if (!readDiscFile(r.file, container)) discError("%s cannot be read", r.file);
                }
                if (r.arcFile[0] != '\0') {
                    if (!rarcFind(container, r.arcFile, rel)) discError("%s has no %s", r.file, r.arcFile);
                } else {
                    rel = container;
                }
                if (!sha1Is(rel.data(), rel.size(), r.sha1))
                    discError("%s%s%s is not GZLE01 revision 0's (SHA-1)", r.file, r.arcFile[0] ? ":" : "", r.arcFile);
                curFile = r.file;
                curArc = r.arcFile;
            }
            src = relBytes(rel, r.section, r.offset, r.size);
        }
        if (src == nullptr) discError("%s: no %u bytes at %#x of %s", r.name, r.size, r.offset, r.file[0] ? r.file : "main.dol");
        auto& d = st.data[i];
        d.assign(src, src + r.size);
        if (r.kind == COS_ASSET_F32) {
            for (uint32_t k = 0; k + 4 <= r.size; k += 4) {
                const uint32_t v = rd32(&d[k]);
                memcpy(&d[k], &v, 4);
            }
        }
    }
    std::lock_guard<std::mutex> guard(st.lock);
    st.loaded = true;
    for (const Pending& p : st.pending) fillLocked(st, p.array, p.size, p.id);
    st.pending.clear();
    writef(STDERR_FILENO, "[cos] assets: %u arrays read from the disc (main.dol and RELs checked), %u filled at "
                          "start-up, the rest on their function's first call\n", kRowCount, st.filled);

    if (gConfig.smoke != nullptr && strcmp(gConfig.smoke, "assets") == 0) {
        const char* dir = getenv("COS_RUN_DIR");
        char path[1024];
        snprintf(path, sizeof path, "%s/assets", dir ? dir : ".");
        mkdir(path, 0755);
        for (uint32_t i = 0; i < kRowCount; i++) {
            snprintf(path, sizeof path, "%s/assets/%03u_%s.bin", dir ? dir : ".", i, kRows[i].name);
            if (FILE* f = fopen(path, "wb")) {
                fwrite(st.data[i].data(), 1, st.data[i].size(), f);
                fclose(f);
            }
        }
        writef(STDERR_FILENO, "[cos] SMOKE assets: PASS (%u arrays written to %s/assets)\n", kRowCount, dir ? dir : ".");
        pc_exit(0);
    }
}

} // namespace pc

#else

namespace pc {
void loadAssets() {} // the arrays are compiled in (COS_RUNTIME_ASSETS=OFF)
} // namespace pc

#endif
