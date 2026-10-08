// The deko3d test pattern (switch/deko/pattern.cpp, COS_DK_TEST_PATTERN=1). Written in WGSL with
// WebGPU's conventions (clip-space y up, z from 0 to 1, texture row 0 at v = 0) and compiled the way
// Aurora's shaders are: native/tools/dksh_cache (Tint's GLSL writer with the deko3d options and the
// post-pass, then uam) into initial_dksh_cache.bin as the named records dk_test_pattern.vs_main and
// dk_test_pattern.fs_main. Vertices are pulled from a storage buffer by vertex_index, as Aurora's GX
// vertex shaders pull theirs, so one picture checks the whole path phase 3 uses.

struct Vertex {
    pos: vec4f,    // clip space (w = 1)
    color: vec4f,
    uv: vec4f,     // xy: texture coordinates; z: 1 = the texture's colour, 0 = the vertex colour
};

@group(0) @binding(0) var<storage, read> verts: array<Vertex>;
@group(0) @binding(1) var tex: texture_2d<f32>;
@group(0) @binding(2) var samp: sampler;

struct VertexOutput {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
    @location(1) uv: vec4f,
};

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> VertexOutput {
    let v = verts[index];
    var out: VertexOutput;
    out.pos = v.pos;
    out.color = v.color;
    out.uv = v.uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let t = textureSample(tex, samp, in.uv.xy);
    return mix(in.color, t, in.uv.z);
}
