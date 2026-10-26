// SPDX-License-Identifier: MIT
// Light screen-space ambient occlusion of the present pass (switch/deko/aurora/dk_present.cpp; options menu:
// Gráficos > Oclusión ambiental, COS_SSAO). Three fixed shaders of this file, all one full-screen triangle:
//
//   fs_ao    reduced resolution (half or quarter, by the level): four to eight depth taps in a disc round the pixel, rotated per pixel (interleaved
//            gradient noise), on the EFB depth buffer (Depth32Float, reversed Z: 1 at the near plane, 0 at
//            the far plane and for the sky). No normals: with the perspective projection the depth value is
//            an affine function of 1/z, so it is an affine function of the screen position on a plane. The
//            plane through the pixel is taken from its depth gradient (the smaller one-sided difference of
//            each axis, so an edge does not give a false slope); a tap whose depth is above the plane (nearer
//            to the camera) occludes by how much, relative to the pixel's own depth, and no longer counts when
//            it is much nearer (a foreground object: no halo). The disc's radius is a world size
//            (proportional to the depth value, about the pixel size of 1 / z), clamped in pixels.
//   fs_blur  reduced resolution: 3x3 blur of that, each texel weighted by how near its depth is to the pixel's
//            (the sky and other objects do not bleed into it).
//   fs_apply the present resolution: the occlusion upsampled (bilinear) and drawn over the resampled picture
//            with the standard blend (colour 0, alpha = strength * occlusion): picture * (1 - alpha).
//
// Compiled into initial_dksh_cache.bin like dk_fxaa, as the named records dk_ssao.vs_main, dk_ssao.fs_ao,
// dk_ssao.fs_blur and dk_ssao.fs_apply (scripts/switch/build_native.sh checks them).

struct Uniforms {
    depth_dims: vec2<f32>,  // the depth buffer's size in texels
    out_dims: vec2<f32>,    // the target's size in texels
    // x: tap radius in depth-buffer pixels per unit of depth value (the depth value is about near / z),
    // y, z: the radius' minimum and maximum in depth-buffer pixels, w: occlusion bias (relative depth)
    radius: vec4<f32>,
    // x: strength of the final darkening (fs_apply), y: range where the occlusion fades out (relative depth),
    // z: the number of taps of fs_ao (4 to 8)
    apply: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var depth_tex: texture_depth_2d;
@group(0) @binding(2) var ao_tex: texture_2d<f32>;
@group(0) @binding(3) var ao_sampler: sampler;

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

const SKY: f32 = 0.0005;  // depth values below it (the far plane, the sky) take no occlusion

fn load_depth(c: vec2<i32>) -> f32 {
    let m = vec2<i32>(u.depth_dims) - vec2<i32>(1, 1);
    return textureLoad(depth_tex, clamp(c, vec2<i32>(0, 0), m), 0);
}

@fragment
fn fs_ao(in: VertexOutput) -> @location(0) vec4<f32> {
    let c = vec2<i32>(in.uv * u.depth_dims);
    let dc = load_depth(c);
    if (dc < SKY) {
        return vec4<f32>(0.0, 0.0, 0.0, 1.0);
    }
    // the plane's slope per depth-buffer pixel: the smaller of the two one-sided differences
    let dxp = load_depth(c + vec2<i32>(2, 0)) - dc;
    let dxm = dc - load_depth(c - vec2<i32>(2, 0));
    let dyp = load_depth(c + vec2<i32>(0, 2)) - dc;
    let dym = dc - load_depth(c - vec2<i32>(0, 2));
    let gx = select(dxp, dxm, abs(dxm) < abs(dxp)) * 0.5;
    let gy = select(dyp, dym, abs(dym) < abs(dyp)) * 0.5;

    let r = clamp(u.radius.x * dc, u.radius.y, u.radius.z);
    let noise = fract(52.9829189 * fract(dot(in.position.xy, vec2<f32>(0.06711056, 0.00583715))));
    let bias = u.radius.w;
    var occ = 0.0;
    let taps = i32(u.apply.z);
    let inv_taps = 1.0 / u.apply.z;
    for (var i = 0; i < taps; i = i + 1) {
        let a = f32(i) * 2.39996323 + noise * 6.2831853;
        let rad = sqrt((f32(i) + 0.5) * inv_taps) * r;
        let o = vec2<i32>(round(vec2<f32>(cos(a), sin(a)) * rad));
        let ds = load_depth(c + o);
        let dp = dc + gx * f32(o.x) + gy * f32(o.y);
        let dz = (ds - dp) / dc;
        occ = occ + smoothstep(bias, bias * 4.0, dz) * (1.0 - smoothstep(u.apply.y, u.apply.y * 2.4, dz));
    }
    return vec4<f32>(occ * inv_taps, 0.0, 0.0, 1.0);
}

@fragment
fn fs_blur(in: VertexOutput) -> @location(0) vec4<f32> {
    let h = vec2<i32>(in.uv * u.out_dims);
    let sd = u.depth_dims / u.out_dims;  // depth-buffer pixels per target texel
    let hm = vec2<i32>(u.out_dims) - vec2<i32>(1, 1);
    let dc = load_depth(vec2<i32>((vec2<f32>(h) + vec2<f32>(0.5, 0.5)) * sd));
    var sum = 0.0;
    var wsum = 0.0;
    for (var y = -1; y <= 1; y = y + 1) {
        for (var x = -1; x <= 1; x = x + 1) {
            let t = clamp(h + vec2<i32>(x, y), vec2<i32>(0, 0), hm);
            let d = load_depth(vec2<i32>((vec2<f32>(t) + vec2<f32>(0.5, 0.5)) * sd));
            let w = (select(1.0, 2.0, x == 0 && y == 0) + select(0.0, 0.5, x == 0 || y == 0)) *
                    max(0.0, 1.0 - abs(d - dc) / (max(dc, SKY) * 0.06));
            sum = sum + textureLoad(ao_tex, t, 0).r * w;
            wsum = wsum + w;
        }
    }
    return vec4<f32>(sum / max(wsum, 0.0001), 0.0, 0.0, 1.0);
}

@fragment
fn fs_apply(in: VertexOutput) -> @location(0) vec4<f32> {
    let occ = textureSampleLevel(ao_tex, ao_sampler, in.uv, 0.0).r;
    return vec4<f32>(0.0, 0.0, 0.0, clamp(occ * u.apply.x, 0.0, 1.0));
}
