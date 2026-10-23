// FXAA of the present pass (switch/deko/aurora/dk_present.cpp, options menu: Gráficos > Suavizado de
// bordes, COS_FXAA). Replaces xfb_copy when on: it reads the resampled picture and writes the swapchain
// image in the picture's viewport, one full-screen triangle, nine bilinear taps per pixel (Timothy
// Lottes' FXAA 1.0 edge blur: the luma gradient across the pixel's four corners gives the edge
// direction, two taps along it and two wider ones are averaged, the wide result is dropped when it
// leaves the local luma range). Tuned against blur: edges under a contrast threshold are skipped and the
// blur reaches at most three texels (the original's 8) along the edge. Compiled into initial_dksh_cache.bin like dk_test_pattern, as the
// named records dk_fxaa.vs_main and dk_fxaa.fs_main. Same bindings as present_resample.wgsl:
// uniforms 0, sampler 1, texture 2.

struct Uniforms {
    inv_size: vec2<f32>,  // 1 / the picture's width and height in texels
    pad: vec2<f32>,
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

fn luma(c: vec3<f32>) -> f32 {
    return dot(c, vec3<f32>(0.299, 0.587, 0.114));
}

fn tap(uv: vec2<f32>) -> vec3<f32> {
    return textureSampleLevel(t, s, uv, 0.0).rgb;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let px = uniforms.inv_size;
    let uv = in.uv;
    let rgbM = tap(uv);
    let rgbNW = tap(uv + vec2<f32>(-1.0, -1.0) * px);
    let rgbNE = tap(uv + vec2<f32>(1.0, -1.0) * px);
    let rgbSW = tap(uv + vec2<f32>(-1.0, 1.0) * px);
    let rgbSE = tap(uv + vec2<f32>(1.0, 1.0) * px);
    let lM = luma(rgbM);
    let lNW = luma(rgbNW);
    let lNE = luma(rgbNE);
    let lSW = luma(rgbSW);
    let lSE = luma(rgbSE);
    let lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    let lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    // flat or low-contrast areas (text on flat panels, soft gradients) are left as they are: only edges
    // with a clear step in luma are blurred
    if (lMax - lMin < max(0.0625, lMax * 0.166)) {
        return vec4<f32>(rgbM, 1.0);
    }

    var dir = vec2<f32>(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));
    let dirReduce = max((lNW + lNE + lSW + lSE) * (0.25 * (1.0 / 8.0)), 1.0 / 128.0);
    let rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2<f32>(-3.0, -3.0), vec2<f32>(3.0, 3.0)) * px;

    let rgbA = 0.5 * (tap(uv + dir * (1.0 / 3.0 - 0.5)) + tap(uv + dir * (2.0 / 3.0 - 0.5)));
    let rgbB = rgbA * 0.5 + 0.25 * (tap(uv + dir * -0.5) + tap(uv + dir * 0.5));
    let lB = luma(rgbB);
    let rgb = select(rgbB, rgbA, lB < lMin || lB > lMax);
    return vec4<f32>(rgb, 1.0);
}
