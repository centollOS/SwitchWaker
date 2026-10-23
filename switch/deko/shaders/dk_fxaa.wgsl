// FXAA of the present pass (switch/deko/aurora/dk_present.cpp, options menu: Gráficos > Suavizado de
// bordes, COS_FXAA). Replaces xfb_copy when on: it reads the resampled picture and writes the swapchain
// image in the picture's viewport, one full-screen triangle. Timothy Lottes' FXAA 3.11 (quality), as
// in his reference: pixels under a contrast threshold are left alone; for the rest the edge's
// orientation and its end on both sides are found by walking along it (eight taps at most, steps of
// 1 to 12 texels), and the pixel is moved across the edge by how far it lies from the nearer end
// (long diagonals and curves come out smooth, short features hardly blur), with a sub-pixel blend for
// isolated bright pixels. Compiled into initial_dksh_cache.bin like dk_test_pattern, as the
// named records dk_fxaa.vs_main and dk_fxaa.fs_main. Same bindings as present_resample.wgsl:
// uniforms 0, sampler 1, texture 2. The uniforms carry the options menu's strength (Suave, Normal, Fuerte).

struct Uniforms {
    inv_size: vec2<f32>,  // 1 / the picture's width and height in texels
    thresholds: vec2<f32>,  // the contrast an edge needs: an absolute minimum, and a share of the local maximum luma
    subpix: vec2<f32>,      // x: the sub-pixel blend's strength (0 to 1); y unused
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

fn tapl(uv: vec2<f32>) -> f32 {
    return luma(tap(uv));
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let px = uniforms.inv_size;
    let uv = in.uv;
    let rgbM = tap(uv);
    let lM = luma(rgbM);
    let lN = tapl(uv + vec2<f32>(0.0, -px.y));
    let lS = tapl(uv + vec2<f32>(0.0, px.y));
    let lW = tapl(uv + vec2<f32>(-px.x, 0.0));
    let lE = tapl(uv + vec2<f32>(px.x, 0.0));
    let lMin = min(lM, min(min(lN, lS), min(lW, lE)));
    let lMax = max(lM, max(max(lN, lS), max(lW, lE)));
    let range = lMax - lMin;
    // flat or low-contrast areas (text on flat panels, soft gradients) stay as they are
    if (range < max(uniforms.thresholds.x, lMax * uniforms.thresholds.y)) {
        return vec4<f32>(rgbM, 1.0);
    }
    let lNW = tapl(uv + vec2<f32>(-px.x, -px.y));
    let lNE = tapl(uv + vec2<f32>(px.x, -px.y));
    let lSW = tapl(uv + vec2<f32>(-px.x, px.y));
    let lSE = tapl(uv + vec2<f32>(px.x, px.y));

    let lNS = lN + lS;
    let lWE = lW + lE;
    let lNWSW = lNW + lSW;
    let lNESE = lNE + lSE;
    let edgeHorz = abs(-2.0 * lW + lNWSW) + abs(-2.0 * lM + lNS) * 2.0 + abs(-2.0 * lE + lNESE);
    let edgeVert = abs(-2.0 * lS + (lSW + lSE)) + abs(-2.0 * lM + lWE) * 2.0 + abs(-2.0 * lN + (lNW + lNE));
    let horzSpan = edgeHorz >= edgeVert;

    // across the edge: N/S for a horizontal edge, W/E for a vertical one
    let lA = select(lW, lN, horzSpan);
    let lB = select(lE, lS, horzSpan);
    let gradA = lA - lM;
    let gradB = lB - lM;
    let towardA = abs(gradA) >= abs(gradB);
    let gradient = max(abs(gradA), abs(gradB));
    var stepLen = select(px.x, px.y, horzSpan);
    if (towardA) { stepLen = -stepLen; }
    let lEdge = select(lB + lM, lA + lM, towardA);  // the pair's sum: the edge's mean is half of it

    // sub-pixel blend: how far the pixel stands out from its 3x3 average
    let blur = (2.0 * (lNS + lWE) + lNWSW + lNESE) / 12.0;
    let subpix = clamp(abs(blur - lM) / range, 0.0, 1.0);
    let subpixAmount = (-2.0 * subpix + 3.0) * subpix * subpix * uniforms.subpix.x;

    // walk along the edge both ways until its luma leaves the edge's mean by a quarter of the gradient
    var posB = uv;
    if (horzSpan) { posB.y = posB.y + stepLen * 0.5; } else { posB.x = posB.x + stepLen * 0.5; }
    let along = select(vec2<f32>(0.0, px.y), vec2<f32>(px.x, 0.0), horzSpan);
    let mean = lEdge * 0.5;
    let limit = gradient * 0.25;
    var posN = posB - along;
    var posP = posB + along;
    var endN = tapl(posN) - mean;
    var endP = tapl(posP) - mean;
    var doneN = abs(endN) >= limit;
    var doneP = abs(endP) >= limit;
    for (var i = 0; i < 7; i = i + 1) {
        if (doneN && doneP) { break; }
        let adv = select(select(select(2.0, 4.0, i == 5), 12.0, i == 6), 1.5, i == 0);
        if (!doneN) { posN = posN - along * adv; endN = tapl(posN) - mean; doneN = abs(endN) >= limit; }
        if (!doneP) { posP = posP + along * adv; endP = tapl(posP) - mean; doneP = abs(endP) >= limit; }
    }
    // distances along the edge: x for a horizontal edge, y for a vertical one
    let dstN = select(uv.y - posN.y, uv.x - posN.x, horzSpan);
    let dstP = select(posP.y - uv.y, posP.x - uv.x, horzSpan);
    let nearN = dstN < dstP;
    let endNear = select(endP, endN, nearN);
    let dstNear = select(dstP, dstN, nearN);
    // the end must lie on the side of the edge the pixel is not on, else the span does not cross
    let centreBelow = (lM - mean) < 0.0;
    let crosses = (endNear < 0.0) != centreBelow;
    let offsetEdge = select(0.0, 0.5 - dstNear / (dstN + dstP), crosses);
    let offset = max(offsetEdge, subpixAmount);
    var posM = uv;
    if (horzSpan) { posM.y = posM.y + offset * stepLen; } else { posM.x = posM.x + offset * stepLen; }
    return vec4<f32>(tap(posM), 1.0);
}
