// Optional HD texture replacement from a Dolphin-format pack (native/include/pc/pc_hd_textures.h,
// docs/HD_TEXTURES.md). Off by default (COS_HD_TEXTURES).
//
// Aurora does the replacement (lib/gfx/texture_replacement.cpp, Dolphin's key and name rules,
// background loaders, LRU cache; tuned by native/patches/aurora/0010). This file picks the pack,
// registers it, applies the runtime toggle at frame boundaries and logs the counters.
//
// Pack formats:
//   - a converted pack (native/tools/hd_pack, cos_hd_pack): <dir>/index.bin plus data00.bin,
//     data01.bin, ... Every entry is one DDS file (with its mips) at an offset of a data file and
//     registers as a virtual replacement named by its Dolphin file name. Reads are positioned reads
//     of that file on Aurora's loader threads; the game thread never touches the SD card.
//   - a Dolphin texture directory of loose tex1_*.dds/.png files (Aurora's own directory loader).
//
// Index layout (little-endian; cos_hd_pack.cpp writes it, kIndexMagic):
//   header (40 bytes): char magic[8] "COSHDIX1"; u32 version (1); u32 entryCount; u32 dataFileCount;
//                      u32 maxDim; u64 dataBytes; u32 namesBytes; u32 reserved
//   entries (32 bytes each): u32 nameOffset; u16 nameLength; u16 dataFile; u64 offset; u32 size;
//                      u16 width; u16 height; u8 mips; u8 dxgiFormat; u8 flags; u8 reserved;
//                      u32 reserved
//   names: the Dolphin file names (tex1_..._<fmt>[_arb].dds), not terminated.

#include "pc/pc_hd_textures.h"
#include "pc_internal.h"

#include <dolphin/gx.h>

#include "JSystem/JKernel/JKRHeap.h"

#include <aurora/texture.hpp>

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pc {

namespace {

constexpr char kIndexMagic[8] = {'T', 'W', 'W', 'H', 'D', 'I', 'X', '1'};
constexpr uint32_t kIndexVersion = 1;
constexpr size_t kHeaderSize = 40;
constexpr size_t kEntrySize = 32;

#if defined(__SWITCH__)
constexpr uint64_t kDefaultBudgetMb = 512;
constexpr uint64_t kDefaultPublishMb = 4;
constexpr uint32_t kDefaultWorkers = 1;
#else
constexpr uint64_t kDefaultBudgetMb = 1024;
constexpr uint64_t kDefaultPublishMb = 12;
constexpr uint32_t kDefaultWorkers = 0;
#endif

uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint64_t rd64(const uint8_t* p) { return rd32(p) | (uint64_t)rd32(p + 4) << 32; }

struct PackEntry {
    uint16_t file;
    uint64_t offset;
    uint32_t size;
};

// One opened converted pack. Its read callback runs on Aurora's loader threads.
struct Pack {
    std::string dir;
    std::vector<FILE*> files;
    std::vector<std::unique_ptr<std::mutex>> fileLocks;
    std::unordered_map<std::string, PackEntry> entries;
    uint64_t dataBytes = 0;
    uint32_t maxDim = 0;
    std::mutex statsLock;
    uint64_t readBytes = 0;
    uint64_t readNs = 0;
    uint64_t reads = 0;

    ~Pack() {
        for (FILE* f : files) {
            if (f != nullptr) {
                fclose(f);
            }
        }
    }
};

struct State {
    char packDir[1024] = {};
    bool enabled = false;       // what is registered now
    bool requested = false;     // what the setting asks for (applied at the frame end)
    bool requestPending = false;
    uint64_t budgetMb = kDefaultBudgetMb;
    uint64_t publishMb = kDefaultPublishMb;
    uint32_t workers = kDefaultWorkers;
    unsigned int statsEvery = 300;
    std::vector<unsigned int> toggleFrames;
    std::unique_ptr<Pack> pack;
    aurora::texture::ReplacementGroup group;
    uint64_t enabledAtNs = 0;
    // census
    FILE* census = nullptr;
    std::unordered_set<std::string> censusSeen;
};

State sState;

bool readPackRead(void* userData, const char* path, std::vector<uint8_t>& out) {
    Pack* pack = static_cast<Pack*>(userData);
    const char* slash = strrchr(path, '/');
    const auto it = pack->entries.find(slash != nullptr ? slash + 1 : path);
    if (it == pack->entries.end()) {
        return false; // e.g. the _mipN sidecars Aurora probes: the mips are inside the DDS
    }
    const PackEntry& e = it->second;
    if (e.file >= pack->files.size() || pack->files[e.file] == nullptr) {
        return false;
    }
    const uint64_t start = monotonicNs();
    out.resize(e.size);
    bool ok;
    {
        std::lock_guard lock{*pack->fileLocks[e.file]};
        FILE* f = pack->files[e.file];
        ok = fseeko(f, (off_t)e.offset, SEEK_SET) == 0 && fread(out.data(), 1, e.size, f) == e.size;
    }
    const uint64_t ns = monotonicNs() - start;
    {
        std::lock_guard lock{pack->statsLock};
        pack->reads++;
        pack->readNs += ns;
        pack->readBytes += ok ? e.size : 0;
    }
    return ok;
}

bool isDir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Opens <dir>/index.bin and its data files; nullptr (and a log line) if anything is wrong.
std::unique_ptr<Pack> openPack(const char* dir) {
    const std::string indexPath = std::string(dir) + "/index.bin";
    FILE* f = fopen(indexPath.c_str(), "rb");
    if (f == nullptr) {
        return nullptr;
    }
    std::vector<uint8_t> bytes;
    fseeko(f, 0, SEEK_END);
    const off_t size = ftello(f);
    fseeko(f, 0, SEEK_SET);
    if (size > 0 && size < (off_t)256 * 1024 * 1024) {
        bytes.resize((size_t)size);
        if (fread(bytes.data(), 1, bytes.size(), f) != bytes.size()) {
            bytes.clear();
        }
    }
    fclose(f);
    if (bytes.size() < kHeaderSize || memcmp(bytes.data(), kIndexMagic, 8) != 0 ||
        rd32(&bytes[8]) != kIndexVersion) {
        writef(STDERR_FILENO, "[cos] hd-textures: %s is not a version %u pack index\n", indexPath.c_str(),
               kIndexVersion);
        return nullptr;
    }
    const uint32_t count = rd32(&bytes[12]);
    const uint32_t fileCount = rd32(&bytes[16]);
    const uint32_t namesBytes = rd32(&bytes[32]);
    const size_t namesStart = kHeaderSize + (size_t)count * kEntrySize;
    if (fileCount == 0 || fileCount > 64 || namesStart + namesBytes > bytes.size()) {
        writef(STDERR_FILENO, "[cos] hd-textures: %s is truncated\n", indexPath.c_str());
        return nullptr;
    }
    auto pack = std::make_unique<Pack>();
    pack->dir = dir;
    pack->maxDim = rd32(&bytes[20]);
    pack->dataBytes = rd64(&bytes[24]);
    for (uint32_t i = 0; i < fileCount; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "/data%02u.bin", i);
        const std::string path = std::string(dir) + name;
        FILE* data = fopen(path.c_str(), "rb");
        if (data == nullptr) {
            writef(STDERR_FILENO, "[cos] hd-textures: cannot open %s: %s\n", path.c_str(), strerror(errno));
            return nullptr;
        }
        // Loader reads are whole textures: a large stdio buffer only adds a copy.
        setvbuf(data, nullptr, _IONBF, 0);
        pack->files.push_back(data);
        pack->fileLocks.push_back(std::make_unique<std::mutex>());
    }
    pack->entries.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = &bytes[kHeaderSize + (size_t)i * kEntrySize];
        const uint32_t nameOffset = rd32(e);
        const uint16_t nameLength = rd16(e + 4);
        if ((size_t)nameOffset + nameLength > namesBytes) {
            continue;
        }
        std::string name(reinterpret_cast<const char*>(&bytes[namesStart + nameOffset]), nameLength);
        pack->entries.emplace(std::move(name), PackEntry{rd16(e + 6), rd64(e + 8), rd32(e + 16)});
    }
    return pack;
}

void applyRuntimeConfig() {
    aurora::texture::ReplacementRuntimeConfig config;
    config.cacheBudgetBytes = sState.budgetMb * 1024 * 1024;
    config.publishBudgetBytesPerFrame = sState.publishMb * 1024 * 1024;
    // Thumbnails would read every texture of the pack at registration: the original texture
    // stands in until the full one is loaded instead.
    config.eagerThumbnails = false;
    config.workerCount = sState.workers;
    aurora::texture::set_runtime_config(config);
}

// The registry, the pack's index and the census are host bookkeeping, never the game's current
// JKRHeap (JKRHeap.cpp, pc_new): every entry point below that allocates opens a host scope.

bool enableNow() {
    JKRPcHostAllocScope hostAlloc;
    const uint64_t start = monotonicNs();
    applyRuntimeConfig();
    sState.pack = openPack(sState.packDir);
    if (sState.pack) {
        sState.group.registrations.reserve(sState.pack->entries.size());
        const aurora::texture::VirtualFileSource source{.read = readPackRead, .userData = sState.pack.get()};
        size_t bad = 0;
        for (const auto& [name, entry] : sState.pack->entries) {
            const auto reg = aurora::texture::register_virtual_replacement(name, source);
            if (reg.id == 0) {
                ++bad;
                continue;
            }
            sState.group.registrations.push_back(reg);
        }
        writef(STDERR_FILENO,
               "[cos] hd-textures: on, pack %s: %zu textures (%zu names rejected), %.1f MiB in %zu data "
               "files, max %u px; budget %" PRIu64 " MiB, publish %" PRIu64 " MiB/frame; registered in "
               "%.1f ms\n",
               sState.packDir, sState.group.registrations.size(), bad, sState.pack->dataBytes / 1048576.0,
               sState.pack->files.size(), sState.pack->maxDim, sState.budgetMb, sState.publishMb,
               (monotonicNs() - start) / 1e6);
    } else if (isDir(sState.packDir)) {
        // A Dolphin texture directory (loose files): Aurora's directory loader.
        sState.group = aurora::texture::load_replacement_directory(sState.packDir);
        writef(STDERR_FILENO,
               "[cos] hd-textures: on, Dolphin directory %s: %zu textures; budget %" PRIu64
               " MiB, publish %" PRIu64 " MiB/frame; registered in %.1f ms\n",
               sState.packDir, sState.group.registrations.size(), sState.budgetMb, sState.publishMb,
               (monotonicNs() - start) / 1e6);
    }
    if (sState.group.registrations.empty()) {
        writef(STDERR_FILENO, "[cos] hd-textures: no usable pack at %s (COS_HD_PACK); staying off\n",
               sState.packDir);
        sState.pack.reset();
        return false;
    }
    aurora::texture::reset_stats();
    sState.enabledAtNs = monotonicNs();
    sState.enabled = true;
    return true;
}

void disableNow() {
    JKRPcHostAllocScope hostAlloc;
    // Unregistering cancels the queued loads, waits for the read in flight and drops the cached
    // replacements; the texture objects resolve to the original textures again.
    aurora::texture::unregister_replacements(sState.group);
    sState.group.registrations.clear();
    sState.pack.reset();
    sState.enabled = false;
    writef(STDERR_FILENO, "[cos] hd-textures: off\n");
}

void logStats(unsigned int frame) {
    const auto s = aurora::texture::stats();
    uint64_t readBytes = 0, readNs = 0, reads = 0;
    if (sState.pack) {
        std::lock_guard lock{sState.pack->statsLock};
        readBytes = sState.pack->readBytes;
        readNs = sState.pack->readNs;
        reads = sState.pack->reads;
    }
    const uint64_t loads = s.loadsCompleted + s.loadsFailed;
    writef(STDERR_FILENO,
           "[cos] hd-textures frame %u: lookups %" PRIu64 " hit / %" PRIu64 " miss; loads %" PRIu64
           " (%" PRIu64 " failed), %.1f MiB decoded, load %.1f ms avg / %.1f max; read %.1f MiB in %"
           PRIu64 " reads, %.1f ms avg; published %" PRIu64 " (%.1f MiB); cache %.1f MiB in %" PRIu64
           " textures, %" PRIu64 " evicted, %" PRIu64 " over budget; pending %" PRIu64 "\n",
           frame, s.lookupHits, s.lookupMisses, loads, s.loadsFailed, s.loadedBytes / 1048576.0,
           loads ? s.loadNsTotal / 1e6 / (double)loads : 0.0, s.loadNsMax / 1e6, readBytes / 1048576.0, reads,
           reads ? readNs / 1e6 / (double)reads : 0.0, s.publishes, s.publishedBytes / 1048576.0,
           s.cacheBytes / 1048576.0, s.cacheEntries, s.evictions, s.budgetRejects, s.pendingLoads);
}

// COS_HD_CENSUS: the Dolphin name of every distinct source key (without "_m", which depends on the
// sampler, not the texture data; native/tools/hd_census.py ignores it).
void censusObserver(const aurora::texture::TextureSourceKey& key, void*) {
    JKRPcHostAllocScope hostAlloc;
    char name[128];
    if (key.hasTlut) {
        snprintf(name, sizeof(name), "tex1_%ux%u_%016" PRIx64 "_%016" PRIx64 "_%u", key.width, key.height,
                 key.textureHash, key.tlutHash, key.format);
    } else {
        snprintf(name, sizeof(name), "tex1_%ux%u_%016" PRIx64 "_%u", key.width, key.height, key.textureHash,
                 key.format);
    }
    if (sState.censusSeen.insert(name).second && sState.census != nullptr) {
        fprintf(sState.census, "%s\n", name);
        fflush(sState.census);
    }
}

uint64_t envU64(const char* name, uint64_t fallback, uint64_t lo, uint64_t hi) {
    const char* v = getenv(name);
    if (v == nullptr || *v == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long long n = strtoull(v, &end, 10);
    if (end == v || *end != '\0' || n < lo || n > hi) {
        writef(STDERR_FILENO, "[cos] %s=\"%s\" is not a number in %" PRIu64 "..%" PRIu64 "; ignored\n", name, v,
               lo, hi);
        return fallback;
    }
    return n;
}

} // namespace

} // namespace pc

using namespace pc;

extern "C" {

void pc_hd_textures_init(const char* userPath) {
    const char* pack = getenv("COS_HD_PACK");
    if (pack != nullptr && *pack != '\0') {
        snprintf(sState.packDir, sizeof(sState.packDir), "%s", pack);
    } else {
        snprintf(sState.packDir, sizeof(sState.packDir), "%s/hd_textures", userPath);
    }
    sState.budgetMb = envU64("COS_HD_BUDGET_MB", kDefaultBudgetMb, 16, 65536);
    sState.publishMb = envU64("COS_HD_PUBLISH_MB", kDefaultPublishMb, 1, 1024);
    sState.workers = (uint32_t)envU64("COS_HD_WORKERS", kDefaultWorkers, 0, 4);
    sState.statsEvery = (unsigned int)envU64("COS_HD_STATS_EVERY", 300, 0, 1000000);
    if (const char* list = getenv("COS_HD_TOGGLE_FRAMES"); list != nullptr) {
        for (const char* p = list; *p != '\0';) {
            char* end = nullptr;
            const unsigned long n = strtoul(p, &end, 10);
            if (end == p) {
                break;
            }
            sState.toggleFrames.push_back((unsigned int)n);
            p = *end == ',' ? end + 1 : end;
        }
    }
    if (const char* census = getenv("COS_HD_CENSUS"); census != nullptr && *census != '\0') {
        sState.census = fopen(census, "a"); // appended: several runs share one census
        if (sState.census == nullptr) {
            writef(STDERR_FILENO, "[cos] hd-textures: cannot write the census to %s: %s\n", census,
                   strerror(errno));
        } else {
            aurora::texture::set_source_key_observer(censusObserver, nullptr);
            writef(STDERR_FILENO, "[cos] hd-textures: census of the static textures to %s\n", census);
        }
    }
    const char* on = getenv("COS_HD_TEXTURES");
    if (on != nullptr && strcmp(on, "1") == 0) {
        sState.requested = enableNow();
    } else {
        if (on != nullptr && strcmp(on, "0") != 0 && *on != '\0') {
            writef(STDERR_FILENO, "[cos] COS_HD_TEXTURES=\"%s\" is not 0 or 1; off\n", on);
        }
        writef(STDERR_FILENO, "[cos] hd-textures: off (COS_HD_TEXTURES=1 to use %s)\n", sState.packDir);
    }
}

void pc_hd_textures_frame_end(unsigned int frame) {
    for (unsigned int f : sState.toggleFrames) {
        if (f == frame) {
            cos_hd_textures_set_enabled(!sState.requested);
        }
    }
    if (sState.requestPending) {
        sState.requestPending = false;
        if (sState.requested && !sState.enabled) {
            sState.requested = enableNow();
        } else if (!sState.requested && sState.enabled) {
            disableNow();
        }
    }
    if (sState.enabled && sState.statsEvery != 0 && frame % sState.statsEvery == 0) {
        logStats(frame);
    }
}

bool cos_hd_textures_set_enabled(bool enabled) {
    if (enabled && !sState.enabled) {
        // Fail early if there is nothing to load; the registration itself waits for the frame end.
        const std::string index = std::string(sState.packDir) + "/index.bin";
        struct stat st;
        if (stat(index.c_str(), &st) != 0 && !isDir(sState.packDir)) {
            writef(STDERR_FILENO, "[cos] hd-textures: no pack at %s (COS_HD_PACK)\n", sState.packDir);
            return false;
        }
    }
    sState.requested = enabled;
    sState.requestPending = true;
    return true;
}

bool cos_hd_textures_enabled(void) {
    return sState.requested;
}

} // extern "C"
