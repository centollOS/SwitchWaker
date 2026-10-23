// The present pass's picture with the options menu's gamma (Gráficos > Gamma; dk_present.cpp): Aurora's
// xfb_copy (webgpu/gpu.cpp create_copy_pipeline, fs_opaque) plus colour^exponent, the one place the
// output is finalized. The game's colours reach the swapchain as they are (RGBA8 Unorm, no sRGB
// conversion anywhere), so they are encoded for a GameCube TV (CRT, about 2.4) and the Switch's panel
// (about 2.2) shows them too bright: an exponent above 1 (2.4 / 2.2) darkens them back. Exponent 1 is not drawn with this
// shader at all (xfb_copy is). ImGui is drawn after this pass, so the menus are not changed.
// Bindings as xfb_copy's (sampler 0, texture 1) plus the uniform at 2; compiled like the test pattern:
// native/tools/dksh_cache, records present_gamma.vs_main and present_gamma.fs_main.

struct Params {
    exponent: vec4<f32>,  // x: the exponent
};

@group(0) @binding(0)
var efb_sampler: sampler;
@group(0) @binding(1)
var efb_texture: texture_2d<f32>;
@group(0) @binding(2)
var<uniform> params: Params;

struct VertexOutput {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

var<private> pos: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -3.0),
    vec2(3.0, 1.0),
);
var<private> uvs: array<vec2<f32>, 3> = array<vec2<f32>, 3>(
    vec2(0.0, 0.0),
    vec2(0.0, 2.0),
    vec2(2.0, 0.0),
);

@vertex
fn vs_main(@builtin(vertex_index) vtxIdx: u32) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4<f32>(pos[vtxIdx], 0.0, 1.0);
    out.uv = uvs[vtxIdx];
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let color = textureSample(efb_texture, efb_sampler, in.uv);
    return vec4(pow(clamp(color.rgb, vec3(0.0), vec3(1.0)), vec3(params.exponent.x)), 1.0);
}
