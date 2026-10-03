// nod's C API (nod.h here) for plain GameCube disc images: what Aurora's DVD library needs to read
// the player's GZLE01 .iso on the Switch, where nod (Rust) cannot be built.
//
// A GameCube disc has no partitions or encryption: the "data partition" is the disc itself.
// boot.bin (0x440 bytes at 0) holds the game ID and, big-endian, the offsets of main.dol (0x420)
// and fst.bin (0x424, size at 0x428); bi2.bin follows at 0x440 (0x2000 bytes) and the apploader
// at 0x2440. fst.bin is an array of 12-byte entries (type and 24-bit name offset, file offset or
// parent index, file size or the index past the directory's last child; entry 0 is the root and
// its size is the entry count) followed by the name table.
//
// All reads go through the caller's stream (Aurora's SDL_IOStream on the SD card), one at a time:
// the stream is a seek and a read, and Aurora reads from its DVD worker and the game threads.
#include "nod.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace {

thread_local std::string tError;

NodResult fail(NodResult result, const std::string& message) {
  tError = message;
  return result;
}

uint32_t be32(const uint8_t* p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

constexpr uint32_t kBootSize = 0x440;
constexpr uint32_t kBi2Offset = 0x440;
constexpr uint32_t kBi2Size = 0x2000;
constexpr uint32_t kApploaderOffset = 0x2440;
constexpr uint32_t kApploaderHeaderSize = 0x20;
constexpr uint32_t kDolHeaderSize = 0x100;
constexpr uint8_t kGcnMagic[4] = {0xC2, 0x33, 0x9F, 0x3D};

struct Disc {
  NodDiscStream stream{};
  uint64_t size = 0;
  std::mutex lock;
  std::vector<uint8_t> boot, bi2, apploader, dol, fst;

  ~Disc() {
    if (stream.close != nullptr) {
      stream.close(stream.user_data);
    }
  }

  // Reads exactly len bytes at offset; false on a short read or an error.
  bool readAt(uint64_t offset, void* out, size_t len) {
    std::lock_guard guard(lock);
    auto* dst = static_cast<uint8_t*>(out);
    while (len > 0) {
      const int64_t got = stream.read_at(stream.user_data, offset, dst, len);
      if (got <= 0) {
        return false;
      }
      dst += got;
      offset += (uint64_t)got;
      len -= (size_t)got;
    }
    return true;
  }

  // Reads up to len bytes at offset, stopping at the end of the disc; -1 on an error.
  int64_t readSome(uint64_t offset, void* out, size_t len) {
    if (offset >= size) {
      return 0;
    }
    len = (size_t)std::min<uint64_t>(len, size - offset);
    return readAt(offset, out, len) ? (int64_t)len : -1;
  }

  bool readBlob(uint64_t offset, uint64_t len, std::vector<uint8_t>& out) {
    if (offset > size || len > size - offset) {
      return false;
    }
    out.resize((size_t)len);
    return len == 0 || readAt(offset, out.data(), (size_t)len);
  }
};

enum class Kind { Disc, Partition, File };

} // namespace

struct NodHandle {
  Kind kind;
  std::shared_ptr<Disc> disc;
  uint64_t start = 0;  // File: its offset on the disc; Disc: 0
  uint64_t length = 0; // File: its size; Disc: the disc size
  uint64_t pos = 0;
};

extern "C" {

const char* nod_error_message(void) { return tError.c_str(); }

NodResult nod_disc_open_stream(const NodDiscStream* stream, const NodDiscOptions* options, NodHandle** out) {
  (void)options;
  if (out == nullptr) {
    return fail(NOD_RESULT_ERR_OTHER, "null output handle");
  }
  *out = nullptr;
  if (stream == nullptr || stream->read_at == nullptr || stream->stream_len == nullptr) {
    return fail(NOD_RESULT_ERR_OTHER, "incomplete stream");
  }
  // The handle owns the stream from here on, also when opening fails (as nod's does).
  auto disc = std::make_shared<Disc>();
  disc->stream = *stream;
  const int64_t size = stream->stream_len(stream->user_data);
  if (size < (int64_t)(kApploaderOffset + kApploaderHeaderSize)) {
    return fail(NOD_RESULT_ERR_FORMAT, "too small for a GameCube disc");
  }
  disc->size = (uint64_t)size;

  if (!disc->readBlob(0, kBootSize, disc->boot) || !disc->readBlob(kBi2Offset, kBi2Size, disc->bi2)) {
    return fail(NOD_RESULT_ERR_IO, "cannot read the disc header");
  }
  const uint8_t* boot = disc->boot.data();
  if (std::memcmp(boot, "CISO", 4) == 0 || std::memcmp(boot + 0x1C, kGcnMagic, 4) != 0) {
    return fail(NOD_RESULT_ERR_FORMAT, "not a plain GameCube disc image");
  }

  // The apploader: header, code, trailer.
  uint8_t appHeader[kApploaderHeaderSize];
  if (!disc->readAt(kApploaderOffset, appHeader, sizeof appHeader)) {
    return fail(NOD_RESULT_ERR_IO, "cannot read the apploader header");
  }
  const uint64_t appSize = (uint64_t)kApploaderHeaderSize + be32(appHeader + 0x14) + be32(appHeader + 0x18);
  // main.dol: its size is the end of its furthest section.
  const uint32_t dolOffset = be32(boot + 0x420);
  uint8_t dolHeader[kDolHeaderSize];
  if (!disc->readBlob(kApploaderOffset, appSize, disc->apploader) ||
      !disc->readAt(dolOffset, dolHeader, sizeof dolHeader)) {
    return fail(NOD_RESULT_ERR_IO, "cannot read the apploader or main.dol header");
  }
  uint64_t dolSize = kDolHeaderSize;
  for (int i = 0; i < 18; i++) {
    const uint32_t offset = be32(dolHeader + 4 * i);
    const uint32_t sectionSize = be32(dolHeader + 0x90 + 4 * i);
    if (sectionSize != 0) {
      dolSize = std::max<uint64_t>(dolSize, (uint64_t)offset + sectionSize);
    }
  }
  const uint32_t fstOffset = be32(boot + 0x424);
  const uint32_t fstSize = be32(boot + 0x428);
  if (!disc->readBlob(dolOffset, dolSize, disc->dol) || !disc->readBlob(fstOffset, fstSize, disc->fst)) {
    return fail(NOD_RESULT_ERR_IO, "cannot read main.dol or fst.bin");
  }
  if (disc->fst.size() < 12 || (uint64_t)be32(disc->fst.data() + 8) * 12 > disc->fst.size()) {
    return fail(NOD_RESULT_ERR_FORMAT, "malformed fst.bin");
  }

  auto* handle = new (std::nothrow) NodHandle{Kind::Disc, disc, 0, disc->size, 0};
  if (handle == nullptr) {
    return fail(NOD_RESULT_ERR_OTHER, "out of memory");
  }
  *out = handle;
  return NOD_RESULT_OK;
}

NodResult nod_disc_open_partition_kind(NodHandle* disc, uint32_t kind, const NodPartitionOptions* options,
                                       NodHandle** out) {
  (void)options;
  if (out == nullptr) {
    return fail(NOD_RESULT_ERR_OTHER, "null output handle");
  }
  *out = nullptr;
  if (disc == nullptr || disc->kind != Kind::Disc) {
    return fail(NOD_RESULT_ERR_INVALID_HANDLE, "not a disc handle");
  }
  if (kind != NOD_PARTITION_KIND_DATA) {
    return fail(NOD_RESULT_ERR_NOT_FOUND, "a GameCube disc has only the data partition");
  }
  *out = new NodHandle{Kind::Partition, disc->disc, 0, disc->disc->size, 0};
  return NOD_RESULT_OK;
}

NodResult nod_partition_open_file(NodHandle* partition, uint32_t index, NodHandle** out) {
  if (out == nullptr) {
    return fail(NOD_RESULT_ERR_OTHER, "null output handle");
  }
  *out = nullptr;
  if (partition == nullptr || partition->kind != Kind::Partition) {
    return fail(NOD_RESULT_ERR_INVALID_HANDLE, "not a partition handle");
  }
  const auto& fst = partition->disc->fst;
  const uint32_t count = be32(fst.data() + 8);
  if (index == 0 || index >= count) {
    return fail(NOD_RESULT_ERR_NOT_FOUND, "no FST entry " + std::to_string(index));
  }
  const uint8_t* entry = fst.data() + 12 * (size_t)index;
  if (entry[0] != 0) {
    return fail(NOD_RESULT_ERR_NOT_FOUND, "FST entry " + std::to_string(index) + " is a directory");
  }
  const uint64_t start = be32(entry + 4);
  const uint64_t length = be32(entry + 8);
  if (start > partition->disc->size || length > partition->disc->size - start) {
    return fail(NOD_RESULT_ERR_FORMAT, "FST entry " + std::to_string(index) + " lies past the end of the disc");
  }
  *out = new NodHandle{Kind::File, partition->disc, start, length, 0};
  return NOD_RESULT_OK;
}

void nod_free(NodHandle* handle) { delete handle; }

// Disc reads for the perf-switch and hitch lines (cos_switch_nod_stats): calls, bytes, time.
static std::atomic<uint64_t> gNodReads{0};
static std::atomic<uint64_t> gNodBytes{0};
static std::atomic<uint64_t> gNodNs{0};

extern "C" void cos_switch_nod_stats(uint64_t out[3]) {
  out[0] = gNodReads.load(std::memory_order_relaxed);
  out[1] = gNodBytes.load(std::memory_order_relaxed);
  out[2] = gNodNs.load(std::memory_order_relaxed);
}

int64_t nod_read(NodHandle* handle, uint8_t* buf, size_t len) {
  if (handle == nullptr || handle->kind == Kind::Partition || (buf == nullptr && len != 0)) {
    return -1;
  }
  if (handle->pos >= handle->length) {
    return 0;
  }
  len = (size_t)std::min<uint64_t>(len, handle->length - handle->pos);
  const auto start = std::chrono::steady_clock::now();
  const int64_t got = handle->disc->readSome(handle->start + handle->pos, buf, len);
  gNodNs.fetch_add((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count(),
                   std::memory_order_relaxed);
  gNodReads.fetch_add(1, std::memory_order_relaxed);
  if (got > 0) {
    handle->pos += (uint64_t)got;
    gNodBytes.fetch_add((uint64_t)got, std::memory_order_relaxed);
  }
  return got;
}

int64_t nod_seek(NodHandle* handle, int64_t offset, int32_t whence) {
  if (handle == nullptr || handle->kind == Kind::Partition) {
    return -1;
  }
  int64_t base = 0;
  switch (whence) {
  case SEEK_SET: base = 0; break;
  case SEEK_CUR: base = (int64_t)handle->pos; break;
  case SEEK_END: base = (int64_t)handle->length; break;
  default: return -1;
  }
  const int64_t pos = base + offset;
  if (pos < 0) {
    return -1;
  }
  handle->pos = (uint64_t)pos;
  return pos;
}

NodResult nod_disc_header(const NodHandle* disc, NodDiscHeader* out) {
  if (disc == nullptr || disc->kind != Kind::Disc || out == nullptr) {
    return fail(NOD_RESULT_ERR_INVALID_HANDLE, "not a disc handle");
  }
  static_assert(sizeof(NodDiscHeader) <= kBootSize);
  std::memcpy(out, disc->disc->boot.data(), sizeof(NodDiscHeader));
  return NOD_RESULT_OK;
}

NodResult nod_partition_meta(const NodHandle* partition, NodPartitionMeta* out) {
  if (partition == nullptr || partition->kind != Kind::Partition || out == nullptr) {
    return fail(NOD_RESULT_ERR_INVALID_HANDLE, "not a partition handle");
  }
  const Disc& d = *partition->disc;
  *out = {};
  out->raw_boot = {d.boot.data(), d.boot.size()};
  out->raw_bi2 = {d.bi2.data(), d.bi2.size()};
  out->raw_apploader = {d.apploader.data(), d.apploader.size()};
  out->raw_dol = {d.dol.data(), d.dol.size()};
  out->raw_fst = {d.fst.data(), d.fst.size()};
  return NOD_RESULT_OK;
}

void nod_partition_iterate_fst(const NodHandle* partition, NodFstCallback callback, void* user_data) {
  if (partition == nullptr || partition->kind != Kind::Partition || callback == nullptr) {
    return;
  }
  const auto& fst = partition->disc->fst;
  const uint32_t count = be32(fst.data() + 8);
  const char* names = reinterpret_cast<const char*>(fst.data()) + 12 * (size_t)count;
  const size_t namesSize = fst.size() - 12 * (size_t)count;
  for (uint32_t i = 1; i < count && i != NOD_FST_STOP;) {
    const uint8_t* entry = fst.data() + 12 * (size_t)i;
    const uint32_t nameOffset = (uint32_t)entry[1] << 16 | (uint32_t)entry[2] << 8 | entry[3];
    const char* name = nameOffset < namesSize ? names + nameOffset : "";
    const NodNodeKind kind = entry[0] != 0 ? NOD_NODE_KIND_DIRECTORY : NOD_NODE_KIND_FILE;
    i = callback(i, kind, name, be32(entry + 8), user_data);
  }
}

} // extern "C"
