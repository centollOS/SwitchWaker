#pragma once
// dksh_cache's view of Aurora (aurora_host.cpp): kept free of Aurora's headers so that the tool's
// other units do not see WebGPU or Aurora types.
#include <cstdint>
#include <string>
#include <vector>

namespace dksh {

struct GxConfigRow {
    uint64_t rowHash = 0;  // pipeline_cache.hash: xxh3(PipelineConfig, ShaderType::GX)
    uint32_t firstFrameUsed = 0;
    std::vector<uint8_t> config;  // a gx::PipelineConfig
};

struct GxModule {
    uint64_t moduleHash = 0;  // Aurora's shader hash (build_shader_source)
    uint8_t dstAlphaMode = 0;
    std::string wgsl;
};

uint32_t gx_config_version();
// every GX row of a pipeline cache database with the current config version, in first-use order;
// *error describes skipped rows (still true then), or why the database could not be read (false)
bool read_gx_configs(const std::string& dbPath, std::vector<GxConfigRow>* out, std::string* error);
// the shader modules create_pipeline builds for a config (main, alpha prepass, dual source)
std::vector<GxModule> gx_modules(const GxConfigRow& row, uint32_t normalAttachment = UINT32_MAX);
const char* dst_alpha_mode_name(uint8_t mode);
// one module of a config in a given gx::DstAlphaMode (0 none, 1 replace, 2 dual source), needed or not
GxModule gx_module(const GxConfigRow& row, uint8_t dstAlphaMode, uint32_t normalAttachment = UINT32_MAX);

}  // namespace dksh
