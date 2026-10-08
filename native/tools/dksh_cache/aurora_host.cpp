// The Aurora side of dksh_cache: GX pipeline configs from the pipeline cache database to WGSL
// modules, with what gx/shader.cpp and gx/shader_info.cpp need from the rest of Aurora replaced by
// host stand-ins (logging to stderr, no census, no device).
#include "aurora_host.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gfx/census.hpp"
#include "gfx/pipeline_cache.hpp"
#include "gfx/recording.hpp"
#include "gx/gx.hpp"
#include "gx/pipeline.hpp"
#include "webgpu/gpu.hpp"

// ---- what the two Aurora units reference outside themselves ----------------------------------------
namespace aurora {
AuroraConfig g_config = [] {
    AuroraConfig c{};
    c.logLevel = LOG_WARNING;
    return c;
}();
void log_internal(AuroraLogLevel level, const char* module, const char* message, unsigned int len) noexcept {
    fprintf(stderr, "[%s] %.*s\n", module, int(len), message);
}
namespace gfx::census {
bool counters_enabled() noexcept { return false; }
}  // namespace gfx::census
namespace webgpu {
wgpu::Device g_device;
bool g_dualSourceBlendingSupported = false;
}  // namespace webgpu
namespace gfx {
// shader_info.cpp's uniform packing for a draw (not used here; deko3d's alignment is WebGPU's 256)
uint32_t align_uniform(uint32_t value) { return (value + 255u) & ~255u; }
Range push_uniform(const uint8_t*, size_t) { return {}; }
}  // namespace gfx
namespace gx {
GXState g_gxState{};
const gfx::TextureBind& get_texture(GXTexMapID id) noexcept { return g_gxState.textures[static_cast<size_t>(id)]; }
}  // namespace gx
}  // namespace aurora

// build_shader (never called: dksh_cache uses build_shader_source) and the wgpu::Device above
extern "C" {
WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice, WGPUShaderModuleDescriptor const*) { abort(); }
void wgpuDeviceRelease(WGPUDevice) {}
void wgpuDeviceAddRef(WGPUDevice) {}
void wgpuShaderModuleRelease(WGPUShaderModule) {}
void wgpuShaderModuleAddRef(WGPUShaderModule) {}
}

namespace dksh {
using namespace aurora;

uint32_t gx_config_version() { return gx::GXPipelineConfigVersion; }

bool read_gx_configs(const std::string& dbPath, std::vector<GxConfigRow>* out, std::string* error) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        *error = dbPath + ": " + (db ? sqlite3_errmsg(db) : "cannot open");
        sqlite3_close(db);
        return false;
    }
    sqlite3_stmt* st = nullptr;
    // the order Aurora warms them up in (first use), then the hash: deterministic output
    const char* sql =
        "SELECT hash, config_version, config_size, config, first_frame_used FROM pipeline_cache "
        "WHERE type = ? ORDER BY first_frame_used, hash";
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        *error = dbPath + ": " + sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_int(st, 1, int(gfx::ShaderType::GX));
    out->clear();
    size_t wrongVersion = 0, wrongSize = 0, wrongHash = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const auto hash = uint64_t(sqlite3_column_int64(st, 0));
        const int version = sqlite3_column_int(st, 1);
        const int size = sqlite3_column_int(st, 2);
        const void* blob = sqlite3_column_blob(st, 3);
        const int blobSize = sqlite3_column_bytes(st, 3);
        if (version != int(gx::GXPipelineConfigVersion)) {
            wrongVersion++;
            continue;
        }
        if (size != int(sizeof(gx::PipelineConfig)) || blobSize != size || !blob) {
            wrongSize++;
            continue;
        }
        GxConfigRow row;
        row.rowHash = hash;
        row.firstFrameUsed = uint32_t(sqlite3_column_int64(st, 4));
        row.config.resize(sizeof(gx::PipelineConfig));
        memcpy(row.config.data(), blob, sizeof(gx::PipelineConfig));
        gx::PipelineConfig config;
        memcpy(&config, row.config.data(), sizeof config);
        // the row's key is xxh3(config, ShaderType::GX): a mismatch means this build's struct layout
        // differs from the one that wrote the row
        if (xxh3_hash(config, static_cast<HashType>(gfx::ShaderType::GX)) != hash) {
            wrongHash++;
            continue;
        }
        out->push_back(std::move(row));
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    if (wrongVersion || wrongSize || wrongHash) {
        char b[160];
        snprintf(b, sizeof b,
                 "%zu rows of another config version, %zu of another size (expected %zu), %zu whose hash differs",
                 wrongVersion, wrongSize, sizeof(gx::PipelineConfig), wrongHash);
        *error = b;
    }
    return true;
}

// gx/pipeline.cpp create_pipeline: which shader modules a config needs. With dual-source blending
// a config that blends with the source alpha while writing a destination alpha gets one
// DualSource module; without it, an alpha prepass (Replace) and the main pass (None). Both are
// built: the renderer decides at run time whether uam's dual-source output is used.
std::vector<GxModule> gx_modules(const GxConfigRow& row, uint32_t normalAttachment) {
    gx::PipelineConfig config;
    memcpy(&config, row.config.data(), sizeof config);
    std::vector<gx::DstAlphaMode> modes;
    if (config.alphaUpdate && config.dstAlpha != UINT32_MAX && config.dstAlpha != 0) {
        const auto usesSourceAlpha = [](GXBlendFactor f) { return f == GX_BL_SRCALPHA || f == GX_BL_INVSRCALPHA; };
        const bool needsSourceAlpha = config.colorUpdate && config.blendMode == GX_BM_BLEND &&
                                      (usesSourceAlpha(config.blendFacSrc) || usesSourceAlpha(config.blendFacDst));
        if (!needsSourceAlpha) {
            modes = {gx::DstAlphaMode::Replace};
        } else {
            modes = {gx::DstAlphaMode::DualSource, gx::DstAlphaMode::Replace, gx::DstAlphaMode::None};
        }
    } else {
        modes = {gx::DstAlphaMode::None};
    }
    std::vector<GxModule> out;
    for (const auto mode : modes) {
        GxModule m;
        m.dstAlphaMode = uint8_t(mode);
        m.moduleHash = xxh3_hash(mode, xxh3_hash(normalAttachment, xxh3_hash(config.shaderConfig)));
        m.wgsl = gx::build_shader_source(config.shaderConfig, mode, normalAttachment);
        out.push_back(std::move(m));
    }
    return out;
}

GxModule gx_module(const GxConfigRow& row, uint8_t dstAlphaMode, uint32_t normalAttachment) {
    gx::PipelineConfig config;
    memcpy(&config, row.config.data(), sizeof config);
    const auto mode = gx::DstAlphaMode(dstAlphaMode);
    GxModule m;
    m.dstAlphaMode = dstAlphaMode;
    m.moduleHash = xxh3_hash(mode, xxh3_hash(normalAttachment, xxh3_hash(config.shaderConfig)));
    m.wgsl = gx::build_shader_source(config.shaderConfig, mode, normalAttachment);
    return m;
}

const char* dst_alpha_mode_name(uint8_t mode) {
    switch (gx::DstAlphaMode(mode)) {
    case gx::DstAlphaMode::None:
        return "none";
    case gx::DstAlphaMode::Replace:
        return "replace";
    case gx::DstAlphaMode::DualSource:
        return "dual-source";
    }
    return "?";
}

}  // namespace dksh
