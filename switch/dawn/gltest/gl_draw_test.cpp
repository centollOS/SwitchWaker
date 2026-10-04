// A replay-correctness test for the Switch's Dawn GL patches (switch/dawn/gltest/README.md), run on
// Linux's Mesa (llvmpipe) through Dawn's OpenGL ES backend: draws shaped like Aurora's GX draws
// (a storage buffer of vertices, one uniform buffer at a dynamic offset per draw, 64 bytes of
// immediates per draw, pipeline switches between different blend/depth/cull/mask states and
// uniform layouts, two render passes) and compares every pixel with a CPU reference. The Switch
// toggles (COS_SWITCH_GL_*) are read by Dawn from the environment, so the same binary runs each
// variant: `run.sh` runs it with each toggle off and on.
#include <dawn/webgpu_cpp.h>

#include "gltest_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using gltest::Context;
using gltest::Init;

constexpr uint32_t kWidth = 256;
constexpr uint32_t kHeight = 128;
constexpr uint32_t kCell = 8;
constexpr uint32_t kCols = kWidth / kCell;
constexpr uint32_t kRows = kHeight / kCell;
constexpr uint32_t kCells = kCols * kRows;
constexpr uint64_t kUniformBufferSize = 4u << 20;
constexpr uint32_t kBindingSize = 3840; // Aurora's MaxUniformSize
constexpr float kClear[4] = {0.1f, 0.2f, 0.3f, 1.0f};

const char* kShaderBig = R"(
struct U0 {
  vp: vec4f,
  proj: mat4x4f,
  mtx: array<mat3x4f, 10>,
  colors: array<vec4f, 4>,
  sel: u32,
  flags: u32,
  bias: f32,
  pad: f32,
};
struct Imm {
  vtx_start: u32,
  pnmtx: u32,
  extra: u32,
  _pad: u32,
  a0: vec4u,
  a1: vec4u,
  a2: vec4u,
};
@group(0) @binding(0) var<storage, read> vbuf: array<vec4f>;
@group(1) @binding(0) var<uniform> ubuf: U0;
var<immediate> imm: Imm;
struct VOut {
  @builtin(position) pos: vec4f,
  @location(0) @interpolate(flat, either) c: vec4f,
};
@vertex fn vs(@builtin(vertex_index) vi: u32) -> VOut {
  let p = vbuf[imm.vtx_start + vi];
  let mv = p * ubuf.mtx[imm.pnmtx];
  var o: VOut;
  o.pos = vec4f(mv, 1.0) * ubuf.proj;
  o.c = vec4f(f32(imm.a1.z) / 255.0, ubuf.vp.y, 0.0, 0.0);
  return o;
}
@fragment fn fs(i: VOut) -> @location(0) vec4f {
  var c = ubuf.colors[ubuf.sel & 3u];
  c = vec4f(c.r + i.c.x, c.g * i.c.y, c.b + ubuf.bias, c.a);
  if ((ubuf.flags & 1u) != 0u) { c = c * 0.5; }
  return c;
}
)";

const char* kShaderSmall = R"(
struct U1 {
  color: vec4f,
  xform: vec4f,
  depth: f32,
  sel: u32,
  pad0: f32,
  pad1: f32,
};
struct Imm {
  vtx_start: u32,
  pnmtx: u32,
  extra: u32,
  _pad: u32,
  a0: vec4u,
  a1: vec4u,
  a2: vec4u,
};
@group(0) @binding(0) var<storage, read> vbuf: array<vec4f>;
@group(1) @binding(0) var<uniform> ubuf: U1;
var<immediate> imm: Imm;
struct VOut {
  @builtin(position) pos: vec4f,
  @location(0) @interpolate(flat, either) k: f32,
};
@vertex fn vs(@builtin(vertex_index) vi: u32) -> VOut {
  let p = vbuf[imm.vtx_start + vi];
  var o: VOut;
  o.pos = vec4f(p.x * ubuf.xform.x + ubuf.xform.z, p.y * ubuf.xform.y + ubuf.xform.w, ubuf.depth, 1.0);
  o.k = f32(imm.extra) / 255.0;
  return o;
}
@fragment fn fs(i: VOut) -> @location(0) vec4f {
  var c = ubuf.color;
  if (ubuf.sel == 7u) { c = c.bgra; }
  return vec4f(c.r, c.g, c.b + i.k, c.a);
}
)";

struct Imm {
    uint32_t vtxStart, pnmtx, extra, pad;
    uint32_t a[12];
};
static_assert(sizeof(Imm) == 64);

struct Vec4 {
    float x, y, z, w;
};

// The CPU side of one draw: what the GPU should do to its cell.
enum class Kind { Opaque, Additive, MaskRG, Alpha };
struct DrawRef {
    uint32_t cell;
    Kind kind;
    float depth;
    float rgba[4];
};

// Pixel-exact unorm8 conversion of the reference, with one step of tolerance at compare time.
uint8_t ToUnorm8(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(v * 255.0f));
}

wgpu::RenderPipeline MakePipeline(Context& ctx, const wgpu::PipelineLayout& layout, const char* code, Kind kind) {
    wgpu::ShaderSourceWGSL wgsl{};
    wgsl.code = code;
    wgpu::ShaderModuleDescriptor smDesc{};
    smDesc.nextInChain = &wgsl;
    wgpu::ShaderModule module = ctx.device.CreateShaderModule(&smDesc);

    wgpu::BlendState blend{};
    wgpu::ColorTargetState target{};
    target.format = wgpu::TextureFormat::RGBA8Unorm;
    target.writeMask = wgpu::ColorWriteMask::All;
    wgpu::DepthStencilState ds{};
    ds.format = wgpu::TextureFormat::Depth24Plus;
    ds.depthWriteEnabled = wgpu::OptionalBool::True;
    ds.depthCompare = wgpu::CompareFunction::Less;
    wgpu::PrimitiveState prim{};
    prim.topology = wgpu::PrimitiveTopology::TriangleList;
    prim.cullMode = wgpu::CullMode::None;
    switch (kind) {
    case Kind::Opaque:
        break;
    case Kind::Additive:
        blend.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
        blend.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
        target.blend = &blend;
        ds.depthWriteEnabled = wgpu::OptionalBool::False;
        ds.depthCompare = wgpu::CompareFunction::Always;
        prim.cullMode = wgpu::CullMode::Front; // quad 11 is front-facing under CCW, so back-facing
        prim.frontFace = wgpu::FrontFace::CW;  // here: drawn (quad 5 would be culled)
        break;
    case Kind::MaskRG:
        target.writeMask = wgpu::ColorWriteMask::Red | wgpu::ColorWriteMask::Green;
        break;
    case Kind::Alpha:
        blend.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::SrcAlpha, wgpu::BlendFactor::OneMinusSrcAlpha};
        blend.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
        target.blend = &blend;
        ds.depthWriteEnabled = wgpu::OptionalBool::False;
        ds.depthCompare = wgpu::CompareFunction::LessEqual;
        prim.cullMode = wgpu::CullMode::Back;
        break;
    }
    wgpu::FragmentState frag{};
    frag.module = module;
    frag.entryPoint = "fs";
    frag.targetCount = 1;
    frag.targets = &target;
    wgpu::RenderPipelineDescriptor desc{};
    desc.layout = layout;
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs";
    desc.primitive = prim;
    desc.depthStencil = &ds;
    desc.fragment = &frag;
    return ctx.device.CreateRenderPipeline(&desc);
}

void CellXform(uint32_t cell, float& sx, float& sy, float& tx, float& ty) {
    const uint32_t cx = cell % kCols, cy = cell / kCols;
    // Unit quad corners (0..1) to the cell's NDC rectangle; NDC +y is up, row 0 is the top.
    sx = 2.0f * kCell / kWidth;
    sy = -2.0f * kCell / kHeight;
    tx = -1.0f + 2.0f * (cx * kCell) / kWidth;
    ty = 1.0f - 2.0f * (cy * kCell) / kHeight;
}

} // namespace

int main(int argc, char** argv) {
    const int passes = 2;
    Context ctx;
    if (!Init(ctx)) {
        return 1;
    }
    wgpu::Device& device = ctx.device;
    const char* toggleNames[] = {"COS_SWITCH_GL_UBO_WINDOW", "COS_SWITCH_GL_STATE_CACHE"};
    for (const char* n : toggleNames) {
        const char* v = getenv(n);
        printf("%s=%s\n", n, v != nullptr ? v : "(unset)");
    }

    // Bind group layouts as Aurora's GX pipelines have them.
    wgpu::BindGroupLayoutEntry staticEntry{};
    staticEntry.binding = 0;
    staticEntry.visibility = wgpu::ShaderStage::Vertex;
    staticEntry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    wgpu::BindGroupLayoutDescriptor staticDesc{};
    staticDesc.entryCount = 1;
    staticDesc.entries = &staticEntry;
    wgpu::BindGroupLayout staticLayout = device.CreateBindGroupLayout(&staticDesc);
    wgpu::BindGroupLayoutEntry uniformEntry{};
    uniformEntry.binding = 0;
    uniformEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    uniformEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniformEntry.buffer.hasDynamicOffset = true;
    wgpu::BindGroupLayoutDescriptor uniformDesc{};
    uniformDesc.entryCount = 1;
    uniformDesc.entries = &uniformEntry;
    wgpu::BindGroupLayout uniformLayout = device.CreateBindGroupLayout(&uniformDesc);
    const std::array layouts{staticLayout, uniformLayout};
    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.bindGroupLayoutCount = layouts.size();
    plDesc.bindGroupLayouts = layouts.data();
    plDesc.immediateSize = 64;
    wgpu::PipelineLayout layout = device.CreatePipelineLayout(&plDesc);

    // Vertices: a few unused vec4s, then the unit quad as two triangles (vtx_start 5).
    std::vector<Vec4> verts(5, Vec4{9, 9, 9, 1});
    const Vec4 quad[6] = {{0, 0, 0, 1}, {1, 0, 0, 1}, {1, 1, 0, 1}, {0, 0, 0, 1}, {1, 1, 0, 1}, {0, 1, 0, 1}};
    verts.insert(verts.end(), std::begin(quad), std::end(quad));
    // The same quad with the other winding (vtx_start 11): after the cells' y flip it is the
    // front-facing one under the default CCW front face; quad 5 is back-facing.
    const Vec4 quadCw[6] = {{0, 0, 0, 1}, {1, 1, 0, 1}, {1, 0, 0, 1}, {0, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 0, 1}};
    verts.insert(verts.end(), std::begin(quadCw), std::end(quadCw));
    wgpu::BufferDescriptor vbDesc{};
    vbDesc.size = verts.size() * sizeof(Vec4);
    vbDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer vbuf = device.CreateBuffer(&vbDesc);
    device.GetQueue().WriteBuffer(vbuf, 0, verts.data(), vbDesc.size);

    wgpu::BufferDescriptor ubDesc{};
    ubDesc.size = kUniformBufferSize;
    ubDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer ubuf = device.CreateBuffer(&ubDesc);

    wgpu::BindGroupEntry sEntry{};
    sEntry.binding = 0;
    sEntry.buffer = vbuf;
    wgpu::BindGroupDescriptor sbg{};
    sbg.layout = staticLayout;
    sbg.entryCount = 1;
    sbg.entries = &sEntry;
    wgpu::BindGroup staticGroup = device.CreateBindGroup(&sbg);
    wgpu::BindGroupEntry uEntry{};
    uEntry.binding = 0;
    uEntry.buffer = ubuf;
    uEntry.size = kBindingSize;
    wgpu::BindGroupDescriptor ubg{};
    ubg.layout = uniformLayout;
    ubg.entryCount = 1;
    ubg.entries = &uEntry;
    wgpu::BindGroup uniformGroup = device.CreateBindGroup(&ubg);

    const Kind kinds[4] = {Kind::Opaque, Kind::Additive, Kind::MaskRG, Kind::Alpha};
    wgpu::RenderPipeline pipes[4];
    for (int k = 0; k < 4; k++) {
        pipes[k] = MakePipeline(ctx, layout, (k < 2) ? kShaderBig : kShaderSmall, kinds[k]);
    }

    // Uniform records and draws. Records go where Aurora would put them (256-aligned, packed),
    // with gaps, some repeats of the previous offset, and the second pass near the buffer's end.
    std::vector<uint8_t> uniforms(kUniformBufferSize, 0);
    struct Draw {
        int pipe;
        uint32_t offset;
        Imm imm;
        int pass;
    };
    std::vector<Draw> draws;
    std::vector<DrawRef> refs;
    uint32_t cursor = 0;
    uint32_t seed = 12345;
    auto rnd = [&] {
        seed = seed * 1103515245u + 12345u;
        return (seed >> 8) & 0xffff;
    };
    auto put = [&](uint32_t size) {
        cursor = (cursor + 255) & ~255u;
        cursor += (rnd() % 3) * 256;
        const uint32_t off = cursor;
        cursor += size;
        return off;
    };
    auto writeF = [&](uint32_t off, float v) { memcpy(&uniforms[off], &v, 4); };
    auto writeU = [&](uint32_t off, uint32_t v) { memcpy(&uniforms[off], &v, 4); };
    for (uint32_t i = 0; i < kCells * 2; i++) {
        const uint32_t cell = i % kCells;
        const int pass = i < kCells ? 0 : 1;
        if (pass == 1 && cursor < kUniformBufferSize - 1024 * 1024) {
            cursor = kUniformBufferSize - 1024 * 1024 + 512; // the window must clamp at the end
        }
        // Pass 0 draws every cell once; pass 1 draws over every cell again with another kind.
        const int pipe = int((cell * 7 + i / kCells * 3 + rnd() % 2) % 4);
        Kind kind = kinds[pipe];
        float sx, sy, tx, ty;
        CellXform(cell, sx, sy, tx, ty);
        Imm imm{};
        imm.vtxStart = 5;
        imm.a[6] = rnd() % 64; // a1.z
        imm.extra = rnd() % 64;
        DrawRef ref{cell, kind, 0, {}};
        const float base[4] = {(rnd() % 200) / 255.0f, (rnd() % 256) / 255.0f, (rnd() % 128) / 255.0f,
                               (64 + rnd() % 192) / 255.0f};
        bool repeat = false;
        uint32_t off = 0;
        if (pipe < 2) {
            // U0: vp(16) proj(64) mtx[10](480) colors[4](64) sel flags bias pad (16) = 640.
            const uint32_t pn = rnd() % 10;
            imm.pnmtx = pn;
            const float depth = pass == 0 ? 0.5f : (rnd() % 2 ? 0.25f : 0.75f);
            const uint32_t sel = rnd() % 4;
            const uint32_t flags = rnd() % 2;
            const float bias = (rnd() % 32) / 255.0f;
            const float vpY = 0.5f + (rnd() % 64) / 128.0f;
            off = put(640);
            writeF(off + 0, 1.0f);
            writeF(off + 4, vpY);
            for (int c = 0; c < 4; c++) { // proj: identity
                writeF(off + 16 + c * 16 + c * 4, 1.0f);
            }
            for (uint32_t m = 0; m < 10; m++) {
                const uint32_t mo = off + 80 + m * 48;
                const float s = m == pn ? 1.0f : 0.0f; // a wrong matrix index draws nothing
                // columns (x, y, z) of mat3x4: out = (p . col0, p . col1, p . col2)
                writeF(mo + 0, sx * s);
                writeF(mo + 12, tx * s);
                writeF(mo + 16 + 4, sy * s);
                writeF(mo + 16 + 12, ty * s);
                writeF(mo + 32 + 12, depth);
            }
            for (uint32_t c = 0; c < 4; c++) {
                const uint32_t co = off + 560 + c * 16;
                for (int k = 0; k < 4; k++) {
                    writeF(co + k * 4, c == sel ? base[k] : 1.0f - base[k]);
                }
            }
            writeU(off + 624, sel);
            writeU(off + 628, flags);
            writeF(off + 632, bias);
            float r = base[0] + imm.a[6] / 255.0f, g = base[1] * vpY, b = base[2] + bias, a = base[3];
            if (flags & 1) {
                r *= 0.5f, g *= 0.5f, b *= 0.5f, a *= 0.5f;
            }
            ref.rgba[0] = r, ref.rgba[1] = g, ref.rgba[2] = b, ref.rgba[3] = a;
            ref.depth = depth;
            if (pipe == 1) {
                imm.vtxStart = 11;
            }
        } else {
            // U1: color(16) xform(16) depth sel pad pad (16) = 48.
            const float depth = pass == 0 ? 0.5f : (rnd() % 2 ? 0.25f : 0.75f);
            const uint32_t sel = rnd() % 2 ? 7u : 0u;
            if (!draws.empty() && draws.back().pipe == pipe && (rnd() % 3) == 0) {
                // Repeat the previous record: same offset, only the immediates move the quad...
                // which they cannot (the transform is in the record), so give it the same cell.
                repeat = true;
            }
            if (repeat) {
                off = draws.back().offset;
                ref = refs.back();
                imm.extra = draws.back().imm.extra;
                imm.vtxStart = draws.back().imm.vtxStart;
                ref.cell = refs.back().cell;
            } else {
                off = put(48);
                for (int k = 0; k < 4; k++) {
                    writeF(off + k * 4, base[k]);
                }
                writeF(off + 16, sx);
                writeF(off + 20, sy);
                writeF(off + 24, tx);
                writeF(off + 28, ty);
                writeF(off + 32, depth);
                writeU(off + 36, sel);
                float c[4] = {base[0], base[1], base[2], base[3]};
                if (sel == 7) {
                    c[0] = base[2], c[1] = base[1], c[2] = base[0], c[3] = base[3];
                }
                ref.rgba[0] = c[0], ref.rgba[1] = c[1], ref.rgba[2] = c[2] + imm.extra / 255.0f, ref.rgba[3] = c[3];
                ref.depth = depth;
                if (pipe == 3) {
                    imm.vtxStart = 11; // front-facing in framebuffer space (default CCW), cull back
                }
            }
        }
        if (off + kBindingSize > kUniformBufferSize) {
            fprintf(stderr, "test layout overflow\n");
            return 1;
        }
        draws.push_back({pipe, off, imm, pass});
        refs.push_back(ref);
    }
    device.GetQueue().WriteBuffer(ubuf, 0, uniforms.data(), uniforms.size());

    wgpu::TextureDescriptor colorDesc{};
    colorDesc.size = {kWidth, kHeight, 1};
    colorDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    colorDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture color = device.CreateTexture(&colorDesc);
    wgpu::TextureDescriptor depthDesc = colorDesc;
    depthDesc.format = wgpu::TextureFormat::Depth24Plus;
    depthDesc.usage = wgpu::TextureUsage::RenderAttachment;
    wgpu::Texture depthTex = device.CreateTexture(&depthDesc);

    wgpu::CommandEncoder enc = device.CreateCommandEncoder();
    size_t di = 0;
    for (int p = 0; p < passes; p++) {
        wgpu::RenderPassColorAttachment ca{};
        ca.view = color.CreateView();
        ca.loadOp = p == 0 ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        ca.storeOp = wgpu::StoreOp::Store;
        ca.clearValue = {kClear[0], kClear[1], kClear[2], kClear[3]};
        wgpu::RenderPassDepthStencilAttachment da{};
        da.view = depthTex.CreateView();
        da.depthLoadOp = p == 0 ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        da.depthStoreOp = wgpu::StoreOp::Store;
        da.depthClearValue = 1.0f;
        wgpu::RenderPassDescriptor rp{};
        rp.colorAttachmentCount = 1;
        rp.colorAttachments = &ca;
        rp.depthStencilAttachment = &da;
        wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
        pass.SetBindGroup(0, staticGroup);
        int lastPipe = -1;
        for (; di < draws.size() && draws[di].pass == p; di++) {
            const Draw& d = draws[di];
            if (d.pipe != lastPipe) {
                pass.SetPipeline(pipes[d.pipe]);
                lastPipe = d.pipe;
            }
            pass.SetImmediates(0, &d.imm, sizeof(d.imm));
            const uint32_t dyn = d.offset;
            pass.SetBindGroup(1, uniformGroup, 1, &dyn);
            pass.Draw(6);
        }
        pass.End();
    }
    const uint32_t bytesPerRow = kWidth * 4;
    wgpu::BufferDescriptor rbDesc{};
    rbDesc.size = bytesPerRow * kHeight;
    rbDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readback = device.CreateBuffer(&rbDesc);
    wgpu::TexelCopyTextureInfo src{};
    src.texture = color;
    wgpu::TexelCopyBufferInfo dst{};
    dst.buffer = readback;
    dst.layout.bytesPerRow = bytesPerRow;
    dst.layout.rowsPerImage = kHeight;
    wgpu::Extent3D extent{kWidth, kHeight, 1};
    enc.CopyTextureToBuffer(&src, &dst, &extent);
    wgpu::CommandBuffer cb = enc.Finish();
    device.GetQueue().Submit(1, &cb);
    bool mapped = false;
    wgpu::Future mf = readback.MapAsync(wgpu::MapMode::Read, 0, rbDesc.size, wgpu::CallbackMode::WaitAnyOnly,
                                        [&](wgpu::MapAsyncStatus status, wgpu::StringView) {
                                            mapped = status == wgpu::MapAsyncStatus::Success;
                                        });
    ctx.instance.WaitAny(mf, UINT64_MAX);
    if (!mapped) {
        fprintf(stderr, "map failed\n");
        return 1;
    }
    const uint8_t* px = static_cast<const uint8_t*>(readback.GetConstMappedRange());

    // CPU reference: per cell, apply the draws in order with their pipeline's state.
    struct CellState {
        float c[4];
        float depth;
    };
    std::vector<CellState> cells(kCells);
    for (auto& c : cells) {
        memcpy(c.c, kClear, sizeof(kClear));
        c.depth = 1.0f;
    }
    // The GPU writes unorm8: quantise after every draw as the GPU does.
    auto q = [](float v) { return ToUnorm8(v) / 255.0f; };
    for (const DrawRef& r : refs) {
        CellState& s = cells[r.cell];
        switch (r.kind) {
        case Kind::Opaque:
            if (r.depth < s.depth) {
                for (int k = 0; k < 4; k++) s.c[k] = q(r.rgba[k]);
                s.depth = r.depth;
            }
            break;
        case Kind::Additive:
            for (int k = 0; k < 4; k++) s.c[k] = q(std::min(1.0f, q(r.rgba[k]) + s.c[k]));
            break;
        case Kind::MaskRG:
            if (r.depth < s.depth) {
                s.c[0] = q(r.rgba[0]);
                s.c[1] = q(r.rgba[1]);
                s.depth = r.depth;
            }
            break;
        case Kind::Alpha:
            if (r.depth <= s.depth) {
                const float a = std::clamp(r.rgba[3], 0.0f, 1.0f);
                for (int k = 0; k < 3; k++) s.c[k] = q(std::clamp(r.rgba[k], 0.0f, 1.0f) * a + s.c[k] * (1 - a));
            }
            break;
        }
    }
    int bad = 0;
    for (uint32_t y = 0; y < kHeight; y++) {
        for (uint32_t x = 0; x < kWidth; x++) {
            const CellState& s = cells[(y / kCell) * kCols + x / kCell];
            const uint8_t* p = px + y * bytesPerRow + x * 4;
            for (int k = 0; k < 4; k++) {
                const int want = ToUnorm8(s.c[k]);
                if (std::abs(int(p[k]) - want) > 2) {
                    if (bad < 10) {
                        printf("mismatch at %u,%u (cell %u) channel %d: got %d want %d\n", x, y,
                               (y / kCell) * kCols + x / kCell, k, p[k], want);
                    }
                    bad++;
                }
            }
        }
    }
    if (bad != 0 && getenv("GLTEST_DIAG") != nullptr) {
        // Mismatching cells by the kinds of their draws.
        int byKind[4][4] = {};
        for (uint32_t c = 0; c < kCells; c++) {
            const CellState& s = cells[c];
            const uint8_t* p = px + (c / kCols) * kCell * bytesPerRow + (c % kCols) * kCell * 4;
            bool ok = true;
            for (int k = 0; k < 4; k++) ok = ok && std::abs(int(p[k]) - int(ToUnorm8(s.c[k]))) <= 2;
            int k0 = -1, k1 = -1;
            for (const DrawRef& r : refs) {
                if (r.cell == c) {
                    (k0 < 0 ? k0 : k1) = int(r.kind);
                }
            }
            if (!ok && k0 >= 0 && k1 >= 0) byKind[k0][k1]++;
        }
        for (int a = 0; a < 4; a++)
            for (int b = 0; b < 4; b++)
                if (byKind[a][b]) printf("bad cells with kinds %d then %d: %d\n", a, b, byKind[a][b]);
    }
    printf("%zu draws, %d passes: %s (%d channel mismatches)\n", draws.size(), passes, bad == 0 ? "PASS" : "FAIL",
           bad);
    return bad == 0 ? 0 : 3;
}
