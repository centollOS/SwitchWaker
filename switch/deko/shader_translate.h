#pragma once
// WGSL -> GLSL for uam (deko3d's shader compiler), docs/DEKO3D_MIGRATION_PLAN.md section 3.3:
//
//   WGSL (Aurora's generator, or a fixed shader) --Tint's GLSL writer--> GLSL 4.60
//     --post_pass--> GLSL that uam accepts --uam::compile--> DKSH
//
// Tint options: desktop GLSL 4.60, depth_zero_to_one (the device clips z to [0, w]),
// disable_position_y_negation (dawn-switch-tint-position-y-up.patch: deko3d's clip space with an
// upper-left origin is WebGPU's), no uniform window, real uniform blocks, robustness off (as the GL
// path), names kept, and a fixed binding table (BindingScheme) because uam has no linking: every
// binding must be explicit and is the deko3d slot itself.
//
// The post-pass rewrites the constructs uam refuses (verified on the host uam 1.1.0):
//   uniform highp sampler2D cos_tex3;           -> layout(binding = 3) uniform highp sampler2D cos_tex3;
//   layout(location = 0) uniform uint tint_immediates[16];
//                                               -> layout(binding = 15, std140) uniform
//                                                  tint_immediates_ubo { uvec4 tint_immediates_v[4]; };
//   tint_immediates[5u]                         -> tint_immediates_v[1].y
//   #extension GL_EXT_blend_func_extended: require   -> removed (dual-source output with
//                                                  layout(index = 1) is core in desktop GLSL)
// (an array of uint in a std140 block would have a 16-byte stride; Tint only indexes the immediates
// with constants, a dynamic index is refused because uam reads .x for it),
// so that the 64-byte per-draw immediates become a uniform buffer at slot 15
// (dkCmdBufPushConstants on the console).
//
// Used by native/tools/dksh_cache on the host (the offline cache) and, later, by the deko3d NRO for
// the pipelines the cache lacks. Tint is not reentrant per call but holds no global state between
// calls besides tint::Initialize(); keep calls on one thread anyway.
#include <cstdint>
#include <string>
#include <vector>

namespace swdk {

// uam::Stage's numbering (uam_api.h), stored in the DKSH cache records.
enum class ShaderStage : uint8_t { Vertex = 0, Fragment = 4 };

// How WGSL bindings map to deko3d slots.
enum class BindingScheme : uint8_t {
    // Aurora's GX shaders (gx/shader.cpp): @group(0) @binding(0, 1, 2) storage -> SSBO 0, 1, 2
    // (vbuf, abuf, census); @group(1) @binding(0) uniform -> UBO 0 (ubuf); @group(2) @binding(2i)
    // texture + @binding(2i + 1) sampler -> combined sampler i (texture map i). The renderer binds by
    // these numbers without reading a table.
    GX,
    // Fixed shaders (clear, copy conversions, present...): uniform buffers, storage buffers and
    // texture/sampler pairs each numbered from 0 in (group, binding) order of the texture. The
    // result's bindings say which slot each WGSL binding got.
    Sequential,
};

// Slot 15 of each stage holds the immediates block (Aurora's DrawImmediateData, 64 bytes).
constexpr uint32_t kImmediatesUboSlot = 15;
// Changes whenever this file's output for the same input changes (Tint options, post-pass rules):
// part of the DKSH cache's compiler id.
constexpr uint32_t kTranslateRevision = 1;

struct SlotBinding {
    enum class Kind : uint8_t { Uniform, Storage, Sampler, Immediates };
    Kind kind = Kind::Uniform;
    uint8_t slot = 0;
    // WGSL binding point (for a combined sampler: the texture's); Immediates: 0, 0
    uint32_t group = 0;
    uint32_t binding = 0;
};

struct TranslateResult {
    bool ok = false;
    std::string glsl;   // after the post-pass
    std::string error;  // Tint's or the post-pass's reason when !ok
    std::vector<SlotBinding> bindings;
    uint32_t immediateSize = 0;  // bytes the entry point uses of the immediates, 0 if none
};

// once per process, before the first translate (tint::Initialize)
void translate_init();

// One entry point of a WGSL module. Never throws; failures come back in error.
TranslateResult wgsl_to_glsl(const std::string& wgsl, const std::string& entryPoint, ShaderStage stage,
                             BindingScheme scheme);

// The post-pass alone (exposed for tests): rewrites Tint's GLSL as described above. samplerSlots maps
// the combined sampler names Tint emitted to their slots. Returns false (why in *error) when a sampler
// has no slot or an immediates declaration has an unexpected shape.
struct SamplerSlot {
    std::string name;
    uint32_t slot = 0;
};
bool post_pass(std::string* glsl, const std::vector<SamplerSlot>& samplerSlots, std::string* error);

}  // namespace swdk
