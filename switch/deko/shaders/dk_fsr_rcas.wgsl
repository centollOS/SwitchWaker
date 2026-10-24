// FSR 1 RCAS (robust contrast-adaptive sharpening) of the present pass (switch/deko/aurora/dk_present.cpp,
// options menu: Gráficos > Reescalado > FSR 1). Replaces xfb_copy in the final draw when FSR 1 is on and
// FXAA is off: it reads the EASU picture (or the EFB itself when it already has the shown size) and writes
// the swapchain image in the picture's viewport. A port of AMD FidelityFX FSR 1.0's FsrRcasF (ffx_fsr1.h,
// MIT license, Copyright (c) 2021 Advanced Micro Devices, Inc.), without the optional noise removal and with
// exact divisions. Compiled into initial_dksh_cache.bin as the named records dk_fsr_rcas.vs_main and
// dk_fsr_rcas.fs_main. Same bindings as present_resample.wgsl: uniforms 0, sampler 1, texture 2.

struct Uniforms {
    inv_size: vec2<f32>,  // 1 / the picture's width and height in texels
    sharpness: vec2<f32>,  // x: the lobe's scale, 2^-stops (1 sharpest); y: the options menu's gamma exponent (present_gamma.wgsl), 1 = none
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var s: sampler;
@group(0) @binding(2) var t: texture_2d<f32>;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
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
    out.position = vec4<f32>(pos[vtxIdx], 0.0, 1.0);
    out.uv = uvs[vtxIdx];
    return out;
}


fn tap(uv: vec2<f32>) -> vec3<f32> {
    return textureSampleLevel(t, s, uv, 0.0).rgb;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let px = uniforms.inv_size;
    let uv = in.uv;
    //   b
    // d e f
    //   h
    let b = tap(uv + vec2<f32>(0.0, -px.y));
    let d = tap(uv + vec2<f32>(-px.x, 0.0));
    let e = tap(uv);
    let f = tap(uv + vec2<f32>(px.x, 0.0));
    let h = tap(uv + vec2<f32>(0.0, px.y));
    // the ring's extremes
    let mn4 = min(min(min(b, d), f), h);
    let mx4 = max(max(max(b, d), f), h);
    // the largest lobe that cannot clip: towards black, towards white
    let hitMin = min(mn4, e) / max(4.0 * mx4, vec3<f32>(1.0e-5));
    let hitMax = (vec3<f32>(1.0) - max(mx4, e)) / min(4.0 * mn4 - vec3<f32>(4.0), vec3<f32>(-1.0e-5));
    let lobeC = max(-hitMin, hitMax);
    let lobe = max(-0.1875, min(max(max(lobeC.r, lobeC.g), lobeC.b), 0.0)) * uniforms.sharpness.x;
    let rcpL = 1.0 / (4.0 * lobe + 1.0);
    return vec4<f32>(pow(clamp((lobe * (b + d + h + f) + e) * rcpL, vec3<f32>(0.0), vec3<f32>(1.0)), vec3<f32>(uniforms.sharpness.y)), 1.0);
}
