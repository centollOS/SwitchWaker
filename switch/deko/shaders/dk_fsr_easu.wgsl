// FSR 1 EASU (edge-adaptive spatial upsampling) of the present pass (switch/deko/aurora/dk_present.cpp,
// options menu: Gráficos > Reescalado > FSR 1, COS_RESCALE 2). Replaces present_resample when on and the
// EFB is smaller than the size the picture is shown at: it reads the EFB and writes the resampled image
// at the shown size. A port of AMD FidelityFX FSR 1.0's FsrEasuF (ffx_fsr1.h, MIT license, Copyright (c)
// 2021 Advanced Micro Devices, Inc.), with 12 point-sampled taps instead of the gathers and exact
// divisions instead of the approximate reciprocals. Compiled into initial_dksh_cache.bin as the named
// records dk_fsr_easu.vs_main and dk_fsr_easu.fs_main. Same bindings as present_resample.wgsl: uniforms 0,
// sampler 1 (linear, clamp: only ever sampled at texel centres), texture 2.

struct Uniforms {
    in_size: vec2<f32>,  // the EFB's width and height in texels
    inv_in_size: vec2<f32>,
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


// the texel at integer offset `o` from the cell `fp`, as its colour
fn tap(fp: vec2<f32>, o: vec2<f32>) -> vec3<f32> {
    return textureSampleLevel(t, s, (fp + o + vec2<f32>(0.5, 0.5)) * uniforms.inv_in_size, 0.0).rgb;
}

fn luma(c: vec3<f32>) -> f32 {
    return c.b * 0.5 + (c.r * 0.5 + c.g);
}

// the direction and length of the edge, from one of the four bilinear cells' weight w and the lumas
// above, left, centre, right and below the pixel it is about (FsrEasuSetF)
fn easu_set(dir: ptr<function, vec2<f32>>, len: ptr<function, f32>, w: f32,
            lA: f32, lB: f32, lC: f32, lD: f32, lE: f32) {
    let dc = lD - lC;
    let cb = lC - lB;
    let lenX = 1.0 / max(max(abs(dc), abs(cb)), 1.0e-8);
    let dirX = lD - lB;
    (*dir).x = (*dir).x + dirX * w;
    let sx = clamp(abs(dirX) * lenX, 0.0, 1.0);
    *len = *len + sx * sx * w;
    let ec = lE - lC;
    let ca = lC - lA;
    let lenY = 1.0 / max(max(abs(ec), abs(ca)), 1.0e-8);
    let dirY = lE - lA;
    (*dir).y = (*dir).y + dirY * w;
    let sy = clamp(abs(dirY) * lenY, 0.0, 1.0);
    *len = *len + sy * sy * w;
}

// one tap's Lanczos-like weight (FsrEasuTapF); `off` is the tap's offset from the output pixel in input texels
fn easu_tap(acc: ptr<function, vec3<f32>>, wsum: ptr<function, f32>, off: vec2<f32>, dir: vec2<f32>,
            len2: vec2<f32>, lob: f32, clp: f32, c: vec3<f32>) {
    var v = vec2<f32>(off.x * dir.x + off.y * dir.y, off.x * (-dir.y) + off.y * dir.x);
    v = v * len2;
    let d2 = min(v.x * v.x + v.y * v.y, clp);
    var wB = 0.4 * d2 - 1.0;
    var wA = lob * d2 - 1.0;
    wB = wB * wB;
    wA = wA * wA;
    wB = 1.5625 * wB - 0.5625;
    let w = wB * wA;
    *acc = *acc + c * w;
    *wsum = *wsum + w;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // the output pixel's place in the input, in texels, relative to the centre of cell f
    let pp0 = in.uv * uniforms.in_size - vec2<f32>(0.5, 0.5);
    let fp = floor(pp0);
    let pp = pp0 - fp;

    //    b c
    //  e f g h
    //  i j k l
    //    n o
    let cb = tap(fp, vec2<f32>(0.0, -1.0));
    let cc = tap(fp, vec2<f32>(1.0, -1.0));
    let ce = tap(fp, vec2<f32>(-1.0, 0.0));
    let cf = tap(fp, vec2<f32>(0.0, 0.0));
    let cg = tap(fp, vec2<f32>(1.0, 0.0));
    let ch = tap(fp, vec2<f32>(2.0, 0.0));
    let ci = tap(fp, vec2<f32>(-1.0, 1.0));
    let cj = tap(fp, vec2<f32>(0.0, 1.0));
    let ck = tap(fp, vec2<f32>(1.0, 1.0));
    let cl = tap(fp, vec2<f32>(2.0, 1.0));
    let cn = tap(fp, vec2<f32>(0.0, 2.0));
    let co = tap(fp, vec2<f32>(1.0, 2.0));
    let lb = luma(cb);
    let lc = luma(cc);
    let le = luma(ce);
    let lf = luma(cf);
    let lg = luma(cg);
    let lh = luma(ch);
    let li = luma(ci);
    let lj = luma(cj);
    let lk = luma(ck);
    let ll = luma(cl);
    let ln = luma(cn);
    let lo = luma(co);

    // the edge's direction and length: the four bilinear cells around the pixel, weighted
    var dir = vec2<f32>(0.0, 0.0);
    var len = 0.0;
    easu_set(&dir, &len, (1.0 - pp.x) * (1.0 - pp.y), lb, le, lf, lg, lj);
    easu_set(&dir, &len, pp.x * (1.0 - pp.y), lc, lf, lg, lh, lk);
    easu_set(&dir, &len, (1.0 - pp.x) * pp.y, lf, li, lj, lk, ln);
    easu_set(&dir, &len, pp.x * pp.y, lg, lj, lk, ll, lo);

    // normalise, with the flat case pointing along x
    let dirR2 = dir.x * dir.x + dir.y * dir.y;
    let zro = dirR2 < (1.0 / 32768.0);
    let dirR = select(inverseSqrt(dirR2), 1.0, zro);
    dir = vec2<f32>(select(dir.x, 1.0, zro), dir.y) * dirR;
    // length {0 to 2} -> {0 to 1}, shaped with a square
    len = len * 0.5;
    len = len * len;
    // stretch the kernel: 1 along an axis, sqrt(2) on a diagonal
    let stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    // anisotropic lengths after the rotation: x along the edge, y across it
    let len2 = vec2<f32>(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    // the window shifts from +-sqrt(2) to just beyond 2 with the amount of edge
    let lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    let clp = 1.0 / lob;

    // the four nearest texels bound the result (no ringing beyond them)
    let min4 = min(min(cf, cg), min(cj, ck));
    let max4 = max(max(cf, cg), max(cj, ck));
    var acc = vec3<f32>(0.0, 0.0, 0.0);
    var wsum = 0.0;
    easu_tap(&acc, &wsum, vec2<f32>(0.0, -1.0) - pp, dir, len2, lob, clp, cb);
    easu_tap(&acc, &wsum, vec2<f32>(1.0, -1.0) - pp, dir, len2, lob, clp, cc);
    easu_tap(&acc, &wsum, vec2<f32>(-1.0, 0.0) - pp, dir, len2, lob, clp, ce);
    easu_tap(&acc, &wsum, vec2<f32>(0.0, 0.0) - pp, dir, len2, lob, clp, cf);
    easu_tap(&acc, &wsum, vec2<f32>(1.0, 0.0) - pp, dir, len2, lob, clp, cg);
    easu_tap(&acc, &wsum, vec2<f32>(2.0, 0.0) - pp, dir, len2, lob, clp, ch);
    easu_tap(&acc, &wsum, vec2<f32>(-1.0, 1.0) - pp, dir, len2, lob, clp, ci);
    easu_tap(&acc, &wsum, vec2<f32>(0.0, 1.0) - pp, dir, len2, lob, clp, cj);
    easu_tap(&acc, &wsum, vec2<f32>(1.0, 1.0) - pp, dir, len2, lob, clp, ck);
    easu_tap(&acc, &wsum, vec2<f32>(2.0, 1.0) - pp, dir, len2, lob, clp, cl);
    easu_tap(&acc, &wsum, vec2<f32>(0.0, 2.0) - pp, dir, len2, lob, clp, cn);
    easu_tap(&acc, &wsum, vec2<f32>(1.0, 2.0) - pp, dir, len2, lob, clp, co);
    return vec4<f32>(min(max4, max(min4, acc / wsum)), 1.0);
}
