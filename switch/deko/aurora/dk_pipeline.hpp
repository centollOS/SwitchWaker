#pragma once
// A deko3d pipeline of the Aurora side (dk_gfx.hpp build_gx_pipeline, build_clear_pipeline): the two
// stages' registry entries (shaders.cpp) and the fixed state that dkCmdBufBind*State takes, made on
// Aurora's compile thread and bound by the encoder (dk_encode.cpp) through its state cache. Pipes
// live as long as the process (Aurora's pipeline cache keeps every pipeline of the session).
#include <deko3d.h>

#include <cstdint>

#include "dk.h"

namespace aurora::gfx::dk {

struct Pipe {
    swdk::ShaderEntry* vs = nullptr;
    swdk::ShaderEntry* fs = nullptr;
    DkRasterizerState rasterizer{};
    DkColorState color{};
    DkColorWriteState colorWrite{};
    DkBlendState blend{};
    DkDepthStencilState depthStencil{};
    float depthBias[3] = {};  // dkCmdBufSetDepthBias: constant, clamp, slope
    uint64_t moduleHash = 0;  // Aurora's shader hash (0: a fixed shader)
    // deko3d 0.5.0 blends alpha with the colour's destination factor (dkCmdBufBindBlendStates, devkitPro/deko3d#29):
    // a state whose two destination factors differ draws RGB here and its alpha in this second pass (split_alpha)
    const Pipe* alphaPass = nullptr;
};

// Render worker: both stages are loaded (a compiled one is loaded now if the frame's budget allows);
// false: the draw is skipped
bool pipe_ready(const Pipe* p);

// The fixed shaders of the cache's named records: "<module>.<entry point>"
struct FixedShader {
    const swdk::NamedShader* vs = nullptr;
    const swdk::NamedShader* fs = nullptr;
    explicit operator bool() const { return vs && fs; }
};
FixedShader fixed_shader(const char* module, const char* vsEntry = "vs_main", const char* fsEntry = "fs_main");

// whether GX pipelines use dual-source blending for a destination alpha (COS_DK_DUAL_SOURCE=1 and the
// device feature); otherwise the alpha prepass (plan section 3.3: the console check comes first)
bool dual_source_blending();

struct PipelineStats {
    uint64_t gx = 0, alphaSplits = 0, clear = 0, cacheHits = 0, misses = 0, translateFailures = 0, compileFailures = 0;
    uint64_t translateNs = 0, waitNs = 0;
};
PipelineStats pipeline_stats();

}  // namespace aurora::gfx::dk
