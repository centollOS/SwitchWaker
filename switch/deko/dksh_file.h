// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/shader_files.h.
#pragma once
// The deko3d renderer's DKSH cache files (docs/DEKO3D_MIGRATION_PLAN.md section 6), readable and
// writable on the host (native/tools/dksh_cache writes initial_dksh_cache.bin) and on the Switch
// (reads it; appends what it compiles to dksh_local.bin). The layout follows SwitchWakerHD's WDK1
// files: a header with the compiler's identity, then records that can be appended; a record cut
// short ends the file. The records are new: Aurora's keys, a CRC per record, module and named
// records.
//
//   header   {char magic[4] = "SWDK", u32 version = kDkshFileVersion, u64 compilerId}
//   records  {u8 kind, u32 payloadSize, u32 crc32 (zlib's, of kind + payload), payload}
//     kind 1 shader:   {u8 stage (ShaderStage), u64 glslHash, DKSH bytes (payloadSize - 9;
//                       none = the compile failed with this compilerId: do not retry)}
//     kind 2 module:   {u64 moduleHash, u64 vertexGlslHash, u64 fragmentGlslHash}
//                      moduleHash = Aurora's shader hash of a GX module, as gx/shader.cpp computes it:
//                      xxh3(dstAlphaMode, xxh3(normalAttachment, xxh3(ShaderConfig))); a GLSL hash
//                      of 0 = that stage failed to translate
//     kind 3 named:    {u8 stage, u8 nameLength, name, u64 glslHash, u8 bindingCount,
//                       bindingCount x {u8 kind, u8 slot, u8 group, u8 binding}} (fixed shaders:
//                      what each WGSL binding became, shader_translate.h SlotBinding)
//   glslHash = XXH3-64 of the GLSL text uam compiled (after the post-pass), so stages that come out
//   the same from many modules are stored once.
//   All little-endian, packed. A record with a bad CRC is skipped (a miss), a cut one ends the file.
//   compilerId (dksh_compiler_id) changes with uam, its patches, the translation revision, the
//   GX config version and this format: a file with another id is stale as a whole.
#include <cstdint>
#include <string>
#include <vector>

#include "shader_translate.h"

namespace swdk {

constexpr uint32_t kDkshFileVersion = 1;
constexpr size_t kDkshFileHeaderSize = 16;
// uam's identity: version, upstream commit and the patch set of switch/uam/PATCHES.md
constexpr char kDkshCompilerName[] = "uam 1.1.0 (devkitPro/uam 5a5afc2) + SwitchWakerHD patches 1-7 + SwitchWaker patch 8";

// FNV-1a over the compiler name, kTranslateRevision, the GX pipeline config version and the format
uint64_t dksh_compiler_id(uint32_t gxConfigVersion);

struct DkshShaderRecord {
    ShaderStage stage = ShaderStage::Vertex;
    uint64_t glslHash = 0;
    std::vector<uint8_t> dksh;  // empty: failed
};
struct DkshModuleRecord {
    uint64_t moduleHash = 0;
    uint64_t vertexGlslHash = 0;
    uint64_t fragmentGlslHash = 0;
};
struct DkshNamedRecord {
    ShaderStage stage = ShaderStage::Vertex;
    std::string name;
    uint64_t glslHash = 0;
    std::vector<SlotBinding> bindings;
};

struct DkshFile {
    uint64_t compilerId = 0;
    std::vector<DkshShaderRecord> shaders;
    std::vector<DkshModuleRecord> modules;
    std::vector<DkshNamedRecord> named;
    size_t badCrc = 0;  // records skipped on read
};

std::vector<uint8_t> dksh_file_header(uint64_t compilerId);
void append_record(std::vector<uint8_t>* out, const DkshShaderRecord& r);
void append_record(std::vector<uint8_t>* out, const DkshModuleRecord& r);
void append_record(std::vector<uint8_t>* out, const DkshNamedRecord& r);
// false (why in *error) when data is not such a file or its version differs; the records are read
// whatever the compiler id (the caller compares it)
bool read_dksh_file(const std::vector<uint8_t>& data, DkshFile* out, std::string* error);

// What a file holds and whether its records resolve: every module's two stages and every named
// record found among the compiled shader records. The deko3d NRO logs it at start ("[dk] shader
// cache:"); `dksh_cache check` prints the same line on the host.
struct DkshFileStats {
    bool idMatches = false;
    size_t vertex = 0, fragment = 0, failed = 0;  // shader records (failed: compiled to nothing)
    uint64_t dkshBytes = 0;
    size_t modules = 0, modulesComplete = 0;      // both stages present and compiled
    size_t named = 0, namedComplete = 0;
    size_t badCrc = 0;
};
DkshFileStats dksh_file_stats(const DkshFile& file, uint64_t expectedCompilerId);
std::string dksh_file_summary(const DkshFileStats& s);

// XXH3-64 of a GLSL text (the shader records' key)
uint64_t glsl_hash(const std::string& glsl);

}  // namespace swdk
