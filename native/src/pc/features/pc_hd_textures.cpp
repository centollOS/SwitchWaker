// Optional HD texture replacement from a Dolphin-format pack (native/include/pc/pc_hd_textures.h,
// docs/HD_TEXTURES.md). Off by default (COS_HD_TEXTURES).
//
// Aurora does the replacement (lib/gfx/texture_replacement.cpp, Dolphin's key and name rules,
// background loaders, LRU cache; tuned by native/patches/aurora/0010 and 0011). This file picks the
// pack, registers it, applies the runtime toggle and the size cap and logs the counters.
//
// Nothing here may stall a frame (lane hd-smooth): turning the pack on reads and parses its index
// on a helper thread, registers the names a slice per frame (COS_HD_REGISTER_US of the game
// thread) and has the texture-object cache look the scene's textures up again a budget of bytes
// per frame (COS_HD_RESCAN_KB); turning it off unregisters in slices the same way; finished loads
// become GPU textures within a time budget per frame (COS_HD_PUBLISH_US, estimated GPU-side cost
// of creation and upload; on the Switch the estimate is scaled by what Dawn measured).
//
// Pack formats:
//   - a converted pack (native/tools/hd_pack, cos_hd_pack): <dir>/index.bin plus data00.bin,
//     data01.bin, ... Every entry is one DDS file (with its mips) at an offset of a data file and
//     registers as a virtual replacement named by its Dolphin file name. Reads are positioned reads
//     of that file on Aurora's loader threads; the game thread never touches the SD card.
//   - a Dolphin texture directory of loose tex1_*.dds/.png files (Aurora's own directory loader,
//     registered in one go).
//
// Index layout (little-endian; cos_hd_pack.cpp writes it, kIndexMagic):
//   header (40 bytes): char magic[8] "COSHDIX1"; u32 version (1); u32 entryCount; u32 dataFileCount;
//                      u32 maxDim; u64 dataBytes; u32 namesBytes; u32 reserved
//   entries (32 bytes each): u32 nameOffset; u16 nameLength; u16 dataFile; u64 offset; u32 size;
//                      u16 width; u16 height; u8 mips; u8 dxgiFormat; u8 flags; u8 reserved;
//                      u32 reserved
//   names: the Dolphin file names (tex1_..._<fmt>[_arb].dds), not terminated.
// Each DDS is a 148-byte DX10 header followed by its levels, largest first, tightly packed (BC1
// 8 bytes per 4x4 block, BC3/BC7 16). COS_HD_MAX_SIZE skips the levels above the cap: the read
// starts at the first allowed level and the header is rewritten to that size.

#include "pc/pc_hd_textures.h"
#include "pc/pc_settings.h"
#include "pc_internal.h"

#include <dolphin/gx.h>

#include "JSystem/JKernel/JKRHeap.h"

#include <aurora/texture.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

namespace pc {

namespace {

constexpr char kIndexMagic[8] = {'T', 'W', 'W', 'H', 'D', 'I', 'X', '1'};
constexpr uint32_t kIndexVersion = 1;
constexpr size_t kHeaderSize = 40;
constexpr size_t kEntrySize = 32;
constexpr size_t kDdsHeaderSize = 148; // magic + DDS_HEADER + DDS_HEADER_DXT10
constexpr size_t kRegisterSlice = 128; // names per registration call

#if defined(__SWITCH__)
constexpr uint64_t kDefaultBudgetMb = 512;
constexpr uint64_t kDefaultPublishMb = 4;
constexpr uint32_t kDefaultWorkers = 1;
// Game-thread time per frame for (un)registering names, and the publish budget in estimated GPU
// work. The cost model is the Switch's after dawn-switch-gl-compressed-upload.patch (one
// glCompressedTexSubImage2D per level): about 0.3 ms to create a texture (glTexStorage, a Mesa bo),
// 0.1 ms per level, 2 ms per MiB (a staging copy and the GPU copy); scaled at run time by Dawn's
// measured upload and creation times.
constexpr uint64_t kDefaultRegisterUs = 2000;
constexpr uint64_t kDefaultPublishUs = 2500;
constexpr uint64_t kDefaultCostTextureUs = 300;
constexpr uint64_t kDefaultCostLevelUs = 100;
constexpr uint64_t kDefaultCostMiBUs = 2000;
constexpr uint64_t kDefaultRescanKb = 2048;
#else
constexpr uint64_t kDefaultBudgetMb = 1024;
constexpr uint64_t kDefaultPublishMb = 12;
constexpr uint32_t kDefaultWorkers = 0;
constexpr uint64_t kDefaultRegisterUs = 2000;
constexpr uint64_t kDefaultPublishUs = 3000;
constexpr uint64_t kDefaultCostTextureUs = 50;
constexpr uint64_t kDefaultCostLevelUs = 5;
constexpr uint64_t kDefaultCostMiBUs = 300;
constexpr uint64_t kDefaultRescanKb = 8192;
#endif

uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint64_t rd64(const uint8_t* p) { return rd32(p) | (uint64_t)rd32(p + 4) << 32; }
void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

struct PackEntry {
    uint16_t file;
    uint64_t offset;
    uint32_t size;
    uint16_t width;
    uint16_t height;
    uint8_t mips;
    uint8_t dxgi;
};

// Bytes per 4x4 block of the pack's formats (BC1 71/72, BC3 77/78, BC7 98/99); 0: not skippable.
uint32_t blockBytes(uint8_t dxgi) {
    switch (dxgi) {
    case 71:
    case 72:
        return 8;
    case 77:
    case 78:
    case 98:
    case 99:
        return 16;
    default:
        return 0;
    }
}

uint64_t levelBytes(uint32_t block, uint32_t w, uint32_t h) {
    return (uint64_t)std::max(1u, (w + 3) / 4) * std::max(1u, (h + 3) / 4) * block;
}

// The levels of `e` to skip so that its larger side fits `cap` (0: no cap); at least one level stays.
uint32_t levelsToSkip(const PackEntry& e, uint32_t cap) {
    if (cap == 0 || blockBytes(e.dxgi) == 0) {
        return 0;
    }
    uint32_t skip = 0;
    uint32_t w = e.width, h = e.height;
    while (std::max(w, h) > cap && skip + 1 < e.mips) {
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
        ++skip;
    }
    return skip;
}

// One opened converted pack. Its read callback runs on Aurora's loader threads.
struct Pack {
    std::string dir;
    std::vector<FILE*> files;
    std::vector<std::unique_ptr<std::mutex>> fileLocks;
    std::unordered_map<std::string, PackEntry> entries;
    std::vector<std::string_view> names; // keys of `entries`, for the registration slices
    uint64_t dataBytes = 0;
    uint32_t maxDim = 0;
    std::mutex statsLock;
    uint64_t readBytes = 0;
    uint64_t readNs = 0;
    uint64_t reads = 0;
    uint64_t cappedReads = 0;
    uint64_t skippedBytes = 0;

    ~Pack() {
        for (FILE* f : files) {
            if (f != nullptr) {
                fclose(f);
            }
        }
    }
};

enum class Phase {
    Off,
    Opening,       // the helper thread reads the index
    Registering,   // names registered a slice per frame
    On,
    Unregistering, // registrations removed a slice per frame
};

const char* phaseName(Phase p) {
    switch (p) {
    case Phase::Off: return "off";
    case Phase::Opening: return "opening";
    case Phase::Registering: return "registering";
    case Phase::On: return "on";
    case Phase::Unregistering: return "unregistering";
    }
    return "?";
}

struct State {
    char packDir[1024] = {};
    Phase phase = Phase::Off;
    bool requested = false;     // what the setting asks for
    uint64_t budgetMb = kDefaultBudgetMb;
    uint64_t publishMb = kDefaultPublishMb;
    uint32_t workers = kDefaultWorkers;
    uint64_t registerUs = kDefaultRegisterUs;
    uint64_t publishUs = kDefaultPublishUs;
    uint64_t costTextureUs = kDefaultCostTextureUs;
    uint64_t costLevelUs = kDefaultCostLevelUs;
    uint64_t costMiBUs = kDefaultCostMiBUs;
    uint64_t rescanKb = kDefaultRescanKb;
    unsigned int statsEvery = 300;
    std::vector<unsigned int> toggleFrames;
    std::vector<std::pair<unsigned int, uint32_t>> capFrames; // COS_HD_MAX_SIZE_FRAMES
    std::unique_ptr<Pack> pack;
    aurora::texture::ReplacementGroup group;
    bool directory = false; // group came from a loose Dolphin directory
    // Opening: the helper thread and its result.
    std::thread opener;
    std::atomic<bool> openDone{false};
    std::unique_ptr<Pack> opened;
    uint64_t openNs = 0;
    // Registering / unregistering progress.
    size_t sliceNext = 0;
    unsigned int sliceFrames = 0;
    uint64_t sliceNsTotal = 0;
    uint64_t sliceNsMax = 0;
    uint64_t phaseStartNs = 0;
    // Size cap (COS_HD_MAX_SIZE): px of the larger side, 0 = the pack's own size.
    uint32_t cap = 0;
    PcOperationMode mode = PC_MODE_HANDHELD; // to notice a dock/undock with "auto" in both modes
    unsigned int recheckFrames = 0; // frames left re-queueing loads published at an older cap
    // The adaptive publish cost scale (Switch: measured / estimated GPU-side cost).
    double costScale = 1.0;
    uint64_t prevEstNs = 0;
    uint64_t prevMeasuredNs = 0;
    // Hitch counters since the last stats line: frame intervals above 40 / 100 ms, and how many of
    // them had HD work in the frame (a toggle step, publishing).
    uint64_t lastFrameEndNs = 0;
    unsigned int hitches40 = 0;
    unsigned int hitches100 = 0;
    unsigned int hitchesHd = 0;
    uint64_t hitchMaxNs = 0;
    uint64_t prevPublishes = 0;
    bool workThisFrame = false;
    // census
    FILE* census = nullptr;
    std::unordered_set<std::string> censusSeen;
};

State sState;
std::atomic<uint32_t> sCap{0}; // what readPackRead applies (loader threads)

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
    const uint32_t skip = levelsToSkip(e, sCap.load(std::memory_order_relaxed));
    uint64_t skipBytes = 0;
    uint32_t w = e.width, h = e.height;
    for (uint32_t level = 0; level < skip; ++level) {
        skipBytes += levelBytes(blockBytes(e.dxgi), w, h);
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }
    if (skip != 0 && (e.size < kDdsHeaderSize || kDdsHeaderSize + skipBytes >= e.size)) {
        skipBytes = 0; // not the layout we expect: read it whole
    }
    bool ok;
    {
        std::lock_guard lock{*pack->fileLocks[e.file]};
        FILE* f = pack->files[e.file];
        if (skipBytes == 0) {
            out.resize(e.size);
            ok = fseeko(f, (off_t)e.offset, SEEK_SET) == 0 && fread(out.data(), 1, e.size, f) == e.size;
        } else {
            // The header, then the levels from the first one that fits the cap.
            out.resize(e.size - skipBytes);
            ok = fseeko(f, (off_t)e.offset, SEEK_SET) == 0 &&
                 fread(out.data(), 1, kDdsHeaderSize, f) == kDdsHeaderSize &&
                 fseeko(f, (off_t)(e.offset + kDdsHeaderSize + skipBytes), SEEK_SET) == 0 &&
                 fread(out.data() + kDdsHeaderSize, 1, out.size() - kDdsHeaderSize, f) ==
                     out.size() - kDdsHeaderSize;
        }
    }
    if (ok && skipBytes != 0) {
        wr32(&out[12], h);
        wr32(&out[16], w);
        wr32(&out[20], (uint32_t)levelBytes(blockBytes(e.dxgi), w, h));
        wr32(&out[28], e.mips - skip);
    }
    const uint64_t ns = monotonicNs() - start;
    {
        std::lock_guard lock{pack->statsLock};
        pack->reads++;
        pack->readNs += ns;
        pack->readBytes += ok ? out.size() : 0;
        if (skipBytes != 0) {
            pack->cappedReads++;
            pack->skippedBytes += skipBytes;
        }
    }
    return ok;
}

bool isDir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Opens <dir>/index.bin and its data files; nullptr (and a log line) if anything is wrong. Runs on
// the opener thread: no game heap, no Aurora.
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
        pack->entries.emplace(std::move(name), PackEntry{rd16(e + 6), rd64(e + 8), rd32(e + 16), rd16(e + 20),
                                                         rd16(e + 22), e[24], e[25]});
    }
    pack->names.reserve(pack->entries.size());
    for (const auto& [name, entry] : pack->entries) {
        pack->names.emplace_back(name);
    }
    // Registration order: the names sorted, so a run registers them in the same order.
    std::sort(pack->names.begin(), pack->names.end());
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
    // The estimate is scaled down (costScale > 1: the GPU side costs more than estimated) by
    // shrinking the budget.
    config.publishBudgetNsPerFrame =
        sState.publishUs == 0 ? 0 : (uint64_t)(sState.publishUs * 1000.0 / std::max(sState.costScale, 0.01));
    config.publishCostNsPerTexture = sState.costTextureUs * 1000;
    config.publishCostNsPerLevel = sState.costLevelUs * 1000;
    config.publishCostNsPerMiB = sState.costMiBUs * 1000;
    config.rescanBudgetBytesPerFrame = sState.rescanKb * 1024;
    aurora::texture::set_runtime_config(config);
}

// The registry, the pack's index and the census are host bookkeeping, never the game's current
// JKRHeap (JKRHeap.cpp, pc_new): every entry point below that allocates opens a host scope.

void startOpening() {
    JKRPcHostAllocScope hostAlloc;
    applyRuntimeConfig();
    aurora::texture::reset_stats();
    aurora::texture::reset_resolve_stats();
    sState.phase = Phase::Opening;
    sState.phaseStartNs = monotonicNs();
    sState.openDone.store(false, std::memory_order_relaxed);
    sState.opened.reset();
    sState.workThisFrame = true;
    std::string dir = sState.packDir;
    sState.opener = std::thread([dir = std::move(dir)] {
        JKRPcHostAllocScope threadHostAlloc;
        const uint64_t start = monotonicNs();
        auto pack = openPack(dir.c_str());
        sState.openNs = monotonicNs() - start;
        sState.opened = std::move(pack);
        sState.openDone.store(true, std::memory_order_release);
    });
}

void finishOff() {
    sState.pack.reset();
    sState.group.registrations.clear();
    sState.directory = false;
    sState.phase = Phase::Off;
    writef(STDERR_FILENO,
           "[cos] hd-textures: off; unregistered in %u frames (%.2f ms total, %.2f ms max per frame), "
           "%.0f ms after the request\n",
           sState.sliceFrames, sState.sliceNsTotal / 1e6, sState.sliceNsMax / 1e6,
           (monotonicNs() - sState.phaseStartNs) / 1e6);
}

void startUnregistering() {
    sState.phase = Phase::Unregistering;
    sState.phaseStartNs = monotonicNs();
    sState.sliceNext = 0;
    sState.sliceFrames = 0;
    sState.sliceNsTotal = 0;
    sState.sliceNsMax = 0;
    sState.workThisFrame = true;
    if (sState.group.registrations.empty()) {
        finishOff();
    }
}

// One frame's slice of registering (or unregistering) within COS_HD_REGISTER_US.
void sliceStep() {
    JKRPcHostAllocScope hostAlloc;
    const uint64_t start = monotonicNs();
    const uint64_t budgetNs = sState.registerUs * 1000;
    sState.workThisFrame = true;
    if (sState.phase == Phase::Registering) {
        const auto& names = sState.pack->names;
        const aurora::texture::VirtualFileSource source{.read = readPackRead, .userData = sState.pack.get()};
        do {
            const size_t count = std::min(kRegisterSlice, names.size() - sState.sliceNext);
            const bool last = sState.sliceNext + count == names.size();
            auto regs = aurora::texture::register_virtual_replacements(
                std::span<const std::string_view>(names.data() + sState.sliceNext, count), source, {}, last);
            for (const auto& reg : regs) {
                if (reg.id != 0) {
                    sState.group.registrations.push_back(reg);
                }
            }
            sState.sliceNext += count;
        } while (sState.sliceNext < names.size() && monotonicNs() - start < budgetNs);
    } else {
        auto& regs = sState.group.registrations;
        do {
            const size_t count = std::min(kRegisterSlice, regs.size() - sState.sliceNext);
            const bool last = sState.sliceNext + count == regs.size();
            aurora::texture::unregister_replacements_gradual(
                std::span<const aurora::texture::ReplacementRegistration>(regs.data() + sState.sliceNext, count),
                last);
            sState.sliceNext += count;
        } while (sState.sliceNext < regs.size() && monotonicNs() - start < budgetNs);
    }
    const uint64_t ns = monotonicNs() - start;
    sState.sliceFrames++;
    sState.sliceNsTotal += ns;
    sState.sliceNsMax = std::max(sState.sliceNsMax, ns);
    if (sState.phase == Phase::Registering && sState.sliceNext == sState.pack->names.size()) {
        sState.phase = Phase::On;
        const size_t rejected = sState.pack->names.size() - sState.group.registrations.size();
        writef(STDERR_FILENO,
               "[cos] hd-textures: on, pack %s: %zu textures (%zu names rejected), %.1f MiB in %zu data "
               "files, max %u px, cap %u px; budget %" PRIu64 " MiB, publish %" PRIu64 " MiB / %" PRIu64
               " us per frame; index read in %.1f ms (helper thread), registered in %u frames (%.2f ms "
               "total, %.2f ms max per frame), %.0f ms after the request\n",
               sState.packDir, sState.group.registrations.size(), rejected, sState.pack->dataBytes / 1048576.0,
               sState.pack->files.size(), sState.pack->maxDim, sState.cap, sState.budgetMb, sState.publishMb,
               sState.publishUs, sState.openNs / 1e6, sState.sliceFrames, sState.sliceNsTotal / 1e6,
               sState.sliceNsMax / 1e6, (monotonicNs() - sState.phaseStartNs) / 1e6);
    } else if (sState.phase == Phase::Unregistering && sState.sliceNext == sState.group.registrations.size()) {
        finishOff();
    }
}

// The opener thread finished: register its pack a slice per frame, or fall back to a loose
// Dolphin directory (registered at once by Aurora's directory loader).
void openingDone() {
    JKRPcHostAllocScope hostAlloc;
    sState.opener.join();
    sState.pack = std::move(sState.opened);
    if (!sState.requested) {
        // Turned off again while the index was read.
        sState.pack.reset();
        sState.phase = Phase::Off;
        writef(STDERR_FILENO, "[cos] hd-textures: off (turned off while opening)\n");
        return;
    }
    if (sState.pack) {
        sState.group.registrations.reserve(sState.pack->names.size());
        sState.phase = Phase::Registering;
        sState.sliceNext = 0;
        sState.sliceFrames = 0;
        sState.sliceNsTotal = 0;
        sState.sliceNsMax = 0;
        sliceStep();
        return;
    }
    if (isDir(sState.packDir)) {
        const uint64_t start = monotonicNs();
        sState.group = aurora::texture::load_replacement_directory(sState.packDir);
        sState.directory = true;
        writef(STDERR_FILENO,
               "[cos] hd-textures: on, Dolphin directory %s: %zu textures; budget %" PRIu64
               " MiB, publish %" PRIu64 " MiB/frame; registered in %.1f ms\n",
               sState.packDir, sState.group.registrations.size(), sState.budgetMb, sState.publishMb,
               (monotonicNs() - start) / 1e6);
    }
    if (sState.group.registrations.empty()) {
        writef(STDERR_FILENO, "[cos] hd-textures: no usable pack at %s (COS_HD_PACK); staying off\n",
               sState.packDir);
        sState.requested = false;
        sState.directory = false;
        sState.phase = Phase::Off;
        return;
    }
    sState.phase = Phase::On;
}

// The size cap reloads: every cached replacement whose loaded size differs from what the cap now
// gives is loaded again; the current texture stays until the new one is published.
bool reloadPredicate(void* user, std::string_view path, uint32_t width, uint32_t height, uint32_t) {
    Pack* pack = static_cast<Pack*>(user);
    const size_t slash = path.rfind('/');
    const auto it = pack->entries.find(std::string(slash == std::string_view::npos ? path : path.substr(slash + 1)));
    if (it == pack->entries.end()) {
        return false;
    }
    const PackEntry& e = it->second;
    const uint32_t skip = levelsToSkip(e, sCap.load(std::memory_order_relaxed));
    const uint32_t w = std::max(1u, (uint32_t)e.width >> skip);
    const uint32_t h = std::max(1u, (uint32_t)e.height >> skip);
    return w != width || h != height;
}

size_t reloadForCap() {
    if (sState.pack == nullptr || (sState.phase != Phase::On && sState.phase != Phase::Registering)) {
        return 0;
    }
    JKRPcHostAllocScope hostAlloc;
    return aurora::texture::reload_cached_replacements(reloadPredicate, sState.pack.get());
}

uint32_t parseCap(const char* value) {
    if (value == nullptr || *value == '\0' || strcmp(value, "auto") == 0) {
        return pc_settings_mode() == PC_MODE_DOCKED ? 1024 : 512;
    }
    if (strcmp(value, "full") == 0 || strcmp(value, "0") == 0) {
        return 0;
    }
    char* end = nullptr;
    const unsigned long n = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || n < 16 || n > 16384) {
        writef(STDERR_FILENO, "[cos] COS_HD_MAX_SIZE=\"%s\" is not auto, full or 16..16384; auto\n", value);
        return pc_settings_mode() == PC_MODE_DOCKED ? 1024 : 512;
    }
    return (uint32_t)n;
}

void setCap(uint32_t cap, const char* why) {
    if (cap == sState.cap) {
        return;
    }
    const uint32_t old = sState.cap;
    sState.cap = cap;
    sCap.store(cap, std::memory_order_relaxed);
    const size_t queued = reloadForCap();
    // Loads in flight were read at the old cap: look again for a few seconds.
    sState.recheckFrames = 240;
    sState.workThisFrame = true;
    writef(STDERR_FILENO, "[cos] hd-textures: max size %u -> %u px (%s, mode %s); %zu cached textures reload\n",
           old, cap, why, pc_settings_mode() == PC_MODE_DOCKED ? "docked" : "handheld", queued);
}

#if defined(__SWITCH__)
// The adaptive cost scale: what Dawn measured for texture uploads and creation over what the
// published textures were estimated to cost, every 30 frames with enough work to compare.
void adaptCostScale(const aurora::texture::ReplacementStats& s) {
    CosSwitchGfxStats g{};
    cos_switch_gfx_stats(&g);
    const uint64_t measured = g.glTexUploadNs + g.glTexCreateNs;
    const uint64_t dEst = s.publishEstNsTotal - std::min(s.publishEstNsTotal, sState.prevEstNs);
    const uint64_t dMeasured = measured - std::min(measured, sState.prevMeasuredNs);
    if (dEst < 2000000) {
        // Not enough HD publishing in the window to tell; keep accumulating (only the baseline
        // of the Dawn counters moves on, so the originals' uploads alone do not count).
        if (dEst == 0) {
            sState.prevMeasuredNs = measured;
        }
        return;
    }
    const double ratio = std::clamp((double)dMeasured / (double)dEst, 0.25, 8.0);
    sState.costScale = std::clamp(0.5 * sState.costScale + 0.5 * ratio, 0.25, 8.0);
    sState.prevEstNs = s.publishEstNsTotal;
    sState.prevMeasuredNs = measured;
    applyRuntimeConfig();
}
#endif

void logStats(unsigned int frame) {
    const auto s = aurora::texture::stats();
    const auto r = aurora::texture::resolve_stats();
    uint64_t readBytes = 0, readNs = 0, reads = 0, capped = 0, skipped = 0;
    if (sState.pack) {
        std::lock_guard lock{sState.pack->statsLock};
        readBytes = sState.pack->readBytes;
        readNs = sState.pack->readNs;
        reads = sState.pack->reads;
        capped = sState.pack->cappedReads;
        skipped = sState.pack->skippedBytes;
    }
    const uint64_t loads = s.loadsCompleted + s.loadsFailed;
    writef(STDERR_FILENO,
           "[cos] hd-textures frame %u (%s, cap %u px): lookups %" PRIu64 " hit / %" PRIu64 " miss; loads %" PRIu64
           " (%" PRIu64 " failed), %.1f MiB decoded, load %.1f ms avg / %.1f max; read %.1f MiB in %" PRIu64
           " reads (%" PRIu64 " capped, %.1f MiB skipped), %.1f ms avg; published %" PRIu64
           " (%.1f MiB), %.2f ms max per frame on the game thread, est %.2f ms max (scale %.2f), %" PRIu64
           " max per frame, %" PRIu64 " frames left some for later, %" PRIu64 " reloads; cache %.1f MiB in %" PRIu64
           " textures, %" PRIu64 " evicted, %" PRIu64 " over budget; pending %" PRIu64
           "; hashing %.2f ms max per frame, %" PRIu64 " rescanned%s; hitches >40 ms %u (>100 ms %u, %u with HD "
           "work), max %.1f ms\n",
           frame, phaseName(sState.phase), sState.cap, s.lookupHits, s.lookupMisses, loads, s.loadsFailed,
           s.loadedBytes / 1048576.0, loads ? s.loadNsTotal / 1e6 / (double)loads : 0.0, s.loadNsMax / 1e6,
           readBytes / 1048576.0, reads, capped, skipped / 1048576.0, reads ? readNs / 1e6 / (double)reads : 0.0,
           s.publishes, s.publishedBytes / 1048576.0, s.publishNsMax / 1e6, s.publishEstNsMax / 1e6,
           sState.costScale, s.publishMaxPerFrame, s.publishFramesLimited, s.reloadsQueued, s.cacheBytes / 1048576.0,
           s.cacheEntries, s.evictions, s.budgetRejects, s.pendingLoads, r.hashNsMaxFrame / 1e6, r.rescanned,
           r.rescanActive ? " (rescan active)" : "", sState.hitches40, sState.hitches100, sState.hitchesHd,
           sState.hitchMaxNs / 1e6);
    sState.hitches40 = sState.hitches100 = sState.hitchesHd = 0;
    sState.hitchMaxNs = 0;
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

// The options menu's "Texturas HD" row (pc_menu.cpp, COS_HD_TEXTURES): a change of its value turns
// the replacement on or off over the next frames. Turning it on without a pack logs why and stays
// off.
void onSettingChanged(const char*, const char* value, void*) {
    cos_hd_textures_set_enabled(value != nullptr && strcmp(value, "1") == 0);
}

// "Tamaño máx. texturas HD" (COS_HD_MAX_SIZE, per mode): also called on a docked/handheld change.
void onMaxSizeChanged(const char*, const char* value, void*) {
    setCap(parseCap(value), "setting");
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
    sState.registerUs = envU64("COS_HD_REGISTER_US", kDefaultRegisterUs, 100, 1000000);
    sState.publishUs = envU64("COS_HD_PUBLISH_US", kDefaultPublishUs, 0, 1000000);
    sState.costTextureUs = envU64("COS_HD_COST_TEXTURE_US", kDefaultCostTextureUs, 0, 1000000);
    sState.costLevelUs = envU64("COS_HD_COST_LEVEL_US", kDefaultCostLevelUs, 0, 1000000);
    sState.costMiBUs = envU64("COS_HD_COST_MIB_US", kDefaultCostMiBUs, 0, 1000000);
    sState.rescanKb = envU64("COS_HD_RESCAN_KB", kDefaultRescanKb, 0, 1048576);
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
    // COS_HD_MAX_SIZE_FRAMES=frame:size,...: set the cap at those frames (checks of the reload).
    if (const char* list = getenv("COS_HD_MAX_SIZE_FRAMES"); list != nullptr) {
        for (const char* p = list; *p != '\0';) {
            char* end = nullptr;
            const unsigned long frame = strtoul(p, &end, 10);
            if (end == p || *end != ':') {
                break;
            }
            p = end + 1;
            const unsigned long size = strtoul(p, &end, 10);
            if (end == p) {
                break;
            }
            sState.capFrames.emplace_back((unsigned int)frame, (uint32_t)size);
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
    sState.mode = pc_settings_mode();
    sState.cap = parseCap(pc_settings_get("COS_HD_MAX_SIZE"));
    sCap.store(sState.cap, std::memory_order_relaxed);
    pc_settings_subscribe("COS_HD_TEXTURES", onSettingChanged, nullptr);
    pc_settings_subscribe("COS_HD_MAX_SIZE", onMaxSizeChanged, nullptr);
    // The settings file's value is in the environment already (pc_settings_load_early).
    const char* on = getenv("COS_HD_TEXTURES");
    if (on != nullptr && strcmp(on, "1") == 0) {
        // At start nothing is drawn yet: open and register at once.
        sState.requested = true;
        startOpening();
        while (!sState.openDone.load(std::memory_order_acquire)) {
            usleep(1000);
        }
        openingDone();
        while (sState.phase == Phase::Registering) {
            sliceStep();
        }
    } else {
        if (on != nullptr && strcmp(on, "0") != 0 && *on != '\0') {
            writef(STDERR_FILENO, "[cos] COS_HD_TEXTURES=\"%s\" is not 0 or 1; off\n", on);
        }
        writef(STDERR_FILENO, "[cos] hd-textures: off (COS_HD_TEXTURES=1 to use %s), max size %u px\n",
               sState.packDir, sState.cap);
    }
}

void pc_hd_textures_frame_end(unsigned int frame) {
    for (unsigned int f : sState.toggleFrames) {
        if (f == frame) {
            cos_hd_textures_set_enabled(!sState.requested);
        }
    }
    if (pc_settings_mode() != sState.mode) {
        // "auto" (the default) is a value of both modes: no setting change reports a dock/undock.
        sState.mode = pc_settings_mode();
        setCap(parseCap(pc_settings_get("COS_HD_MAX_SIZE")), "mode change");
    }
    for (const auto& [f, size] : sState.capFrames) {
        if (f == frame) {
            setCap(size, "COS_HD_MAX_SIZE_FRAMES");
        }
    }
    switch (sState.phase) {
    case Phase::Off:
        if (sState.requested) {
            startOpening();
        }
        break;
    case Phase::Opening:
        if (sState.openDone.load(std::memory_order_acquire)) {
            openingDone();
        }
        break;
    case Phase::Registering:
        if (!sState.requested) {
            startUnregistering();
        } else {
            sliceStep();
        }
        break;
    case Phase::On:
        if (!sState.requested) {
            if (sState.directory) {
                JKRPcHostAllocScope hostAlloc;
                aurora::texture::unregister_replacements(sState.group);
                sState.sliceFrames = 1;
                sState.sliceNsTotal = sState.sliceNsMax = 0;
                sState.phaseStartNs = monotonicNs();
                finishOff();
            } else {
                startUnregistering();
            }
        }
        break;
    case Phase::Unregistering:
        if (sState.sliceNext < sState.group.registrations.size()) {
            sliceStep();
        }
        break;
    }
    if (sState.recheckFrames != 0) {
        --sState.recheckFrames;
        if (sState.recheckFrames % 30 == 0) {
            reloadForCap();
        }
    }
    const bool active = sState.phase != Phase::Off;
    if (active) {
        const auto s = aurora::texture::stats();
        if (s.publishes != sState.prevPublishes) {
            sState.workThisFrame = true;
            sState.prevPublishes = s.publishes;
        }
#if defined(__SWITCH__)
        if (frame % 30 == 0 && sState.publishUs != 0) {
            adaptCostScale(s);
        }
#endif
    }
    // Hitch counters: the interval since the previous frame end (pace wait included).
    const uint64_t now = monotonicNs();
    if (active && sState.lastFrameEndNs != 0) {
        const uint64_t interval = now - sState.lastFrameEndNs;
        if (interval > 40000000) {
            sState.hitches40++;
            if (interval > 100000000) {
                sState.hitches100++;
            }
            if (sState.workThisFrame) {
                sState.hitchesHd++;
            }
            sState.hitchMaxNs = std::max(sState.hitchMaxNs, interval);
        }
    }
    sState.lastFrameEndNs = now;
    sState.workThisFrame = false;
    if (active && sState.statsEvery != 0 && frame % sState.statsEvery == 0) {
        logStats(frame);
    }
}

bool cos_hd_textures_set_enabled(bool enabled) {
    if (enabled && !sState.requested) {
        // Fail early if there is nothing to load; opening and registering happen over the next frames.
        const std::string index = std::string(sState.packDir) + "/index.bin";
        struct stat st;
        if (stat(index.c_str(), &st) != 0 && !isDir(sState.packDir)) {
            writef(STDERR_FILENO, "[cos] hd-textures: no pack at %s (COS_HD_PACK)\n", sState.packDir);
            return false;
        }
    }
    sState.requested = enabled;
    return true;
}

bool cos_hd_textures_enabled(void) {
    return sState.requested;
}

} // extern "C"
