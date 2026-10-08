// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The 3x5 text (glyph table, TextUbo, draw_text) is adapted from SwitchWakerHD
// (https://github.com/centollOS/SwitchWakerHD) at df8fbde: runtime/src/gfx/deko/backend.cpp.
// The deko3d test pattern (COS_DK_TEST_PATTERN=1; docs/DEKO3D_MIGRATION_PLAN.md phase 2): one
// picture that shows whether deko3d's conventions are WebGPU's, as Aurora's shaders need (plan
// section 3.1): orientation, depth range and test, texture origin and front-face winding. Its shaders
// are WGSL (shaders/dk_test_pattern.wgsl) compiled offline like Aurora's (Tint with the deko3d
// options, the post-pass, uam) and loaded from initial_dksh_cache.bin; the vertices are written in
// WebGPU's clip space (y up) and pulled from a storage buffer by vertex index, as GX vertices are.
// The legend is the 3x5 pixel font (text_fsh.glsl, gl_FragCoord from the top left): it says what the
// photo must show.
#include "dk.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace swdk {
namespace {

// ---- text: the 3x5 pixel font, one draw per text box
uint32_t glyph_bits(char c) {
    static const struct {
        char c;
        const char* rows;
    } kFont[] = {
        {'0', "111101101101111"}, {'1', "010110010010111"}, {'2', "111001111100111"}, {'3', "111001111001111"},
        {'4', "101101111001001"}, {'5', "111100111001111"}, {'6', "111100111101111"}, {'7', "111001001001001"},
        {'8', "111101111101111"}, {'9', "111101111001111"}, {'A', "010101111101101"}, {'B', "110101110101110"},
        {'C', "011100100100011"}, {'D', "110101101101110"}, {'E', "111100110100111"}, {'F', "111100110100100"},
        {'G', "011100101101011"}, {'H', "101101111101101"}, {'I', "111010010010111"}, {'J', "001001001101010"},
        {'K', "101101110101101"}, {'L', "100100100100111"}, {'M', "101111111101101"}, {'N', "110101101101101"},
        {'O', "010101101101010"}, {'P', "110101110100100"}, {'Q', "010101101110011"}, {'R', "110101110101101"},
        {'S', "011100010001110"}, {'T', "111010010010010"}, {'U', "101101101101111"}, {'V', "101101101101010"},
        {'W', "101101111111101"}, {'X', "101101010101101"}, {'Y', "101101010010010"}, {'Z', "111001010100111"},
        {'%', "101001010100101"}, {':', "000010000010000"}, {'/', "001001010100100"}, {'-', "000000111000000"},
        {'.', "000000000000010"}, {',', "000000000010100"}, {'=', "000111000111000"}, {'(', "010100100100010"},
        {')', "010001001001010"}, {'>', "100010001010100"}, {'<', "001010100010001"}, {'+', "000010111010000"},
        {'_', "000000000000111"},
    };
    for (auto& g : kFont) {
        if (g.c == c) {
            uint32_t bits = 0;
            for (int i = 0; i < 15; i++) bits = bits << 1 | uint32_t(g.rows[i] == '1');
            return bits;
        }
    }
    return 0;  // space and anything else
}

struct TextUbo {  // text_fsh.glsl, std140
    float box[4];
    int32_t grid[4];
    float fg[4], bg[4];
    uint32_t glyphs[768];  // uvec4 glyphs[192]: character i in component i & 3 of entry i >> 2
};
static_assert(offsetof(TextUbo, box) == 0 && offsetof(TextUbo, grid) == 16 && offsetof(TextUbo, fg) == 32 &&
                  offsetof(TextUbo, bg) == 48 && offsetof(TextUbo, glyphs) == 64 && sizeof(TextUbo) == 64 + 768 * 4,
              "TextUbo must match text_fsh.glsl's std140 block");

// ---- the pattern
struct Vertex {  // dk_test_pattern.wgsl's Vertex (std430: three vec4f)
    float pos[4];
    float color[4];
    float uv[4];
};
static_assert(sizeof(Vertex) == 48, "dk_test_pattern.wgsl's Vertex is three vec4f");

constexpr uint32_t kTexSize = 8;
bool g_texReady = false;
DkImage g_tex;
ImageAlloc g_texMem;

// window pixels (from the top left) to WebGPU's clip space (y up): what Aurora's shaders produce
float clip_x(float px) { return px / float(R.width) * 2.0f - 1.0f; }
float clip_y(float py) { return 1.0f - py / float(R.height) * 2.0f; }

void vertex(std::vector<Vertex>& v, float px, float py, float z, const float c[4], float u = 0, float w = 0,
            float textured = 0) {
    v.push_back({{clip_x(px), clip_y(py), z, 1}, {c[0], c[1], c[2], c[3]}, {u, w, textured, 0}});
}
// a rectangle in window pixels, two triangles wound counter-clockwise on the screen
void rect(std::vector<Vertex>& v, float x0, float y0, float x1, float y1, float z, const float c[4],
          bool textured = false) {
    const float t = textured ? 1.0f : 0.0f;
    vertex(v, x0, y0, z, c, 0, 0, t), vertex(v, x0, y1, z, c, 0, 1, t), vertex(v, x1, y1, z, c, 1, 1, t);
    vertex(v, x0, y0, z, c, 0, 0, t), vertex(v, x1, y1, z, c, 1, 1, t), vertex(v, x1, y0, z, c, 1, 0, t);
}

struct Draw {
    const DkShader* vs = nullptr;
    const DkShader* fs = nullptr;
    uint32_t ssbo = 0, sampler = 0;
};

// one draw of v (a stream slice of its own: the storage buffer starts at vertex 0, as gl_VertexID
// equals WebGPU's vertex_index only with first vertex 0)
void draw_vertices(const Draw& d, const std::vector<Vertex>& v, bool cullBack) {
    const uint32_t bytes = uint32_t(v.size() * sizeof(Vertex));
    StreamAlloc s = stream_alloc(bytes, 256);
    if (!s) return;
    memcpy(s.cpu, v.data(), bytes);
    DkRasterizerState rs;
    dkRasterizerStateDefaults(&rs);
    rs.cullMode = cullBack ? DkFace_Back : DkFace_None;
    rs.frontFace = DkFrontFace_CCW;  // what Aurora's FrontFace::CCW maps to (plan section 3.1)
    dkCmdBufBindRasterizerState(R.cmd, &rs);
    dkCmdBufBindStorageBuffer(R.cmd, DkStage_Vertex, d.ssbo, s.gpu, bytes);
    dkCmdBufDraw(R.cmd, DkPrimitive_Triangles, uint32_t(v.size()), 1, 0, 0);
}

}  // namespace

void draw_text(int left, int top, int scale, const char* const* lines, int count, const float fg[4],
               const float bg[4]) {
    const DkShader* vs = builtin_shader(kTextVs);
    const DkShader* fs = builtin_shader(kTextFs);
    if (count <= 0 || !vs || !fs) return;
    int columns = 1;
    for (int r = 0; r < count; r++) columns = std::max(columns, int(strlen(lines[r])));
    const int rows = std::min(count, 768 / columns);
    constexpr uint32_t kUboSize = (sizeof(TextUbo) + 255) & ~255u;
    StreamAlloc u = stream_alloc(kUboSize, DK_UNIFORM_BUF_ALIGNMENT);
    if (!u) return;
    TextUbo t{};
    t.box[0] = float(left);
    t.box[1] = float(top);
    t.box[2] = float(scale);
    t.grid[0] = columns;
    t.grid[1] = rows;
    memcpy(t.fg, fg, sizeof t.fg);
    memcpy(t.bg, bg, sizeof t.bg);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; lines[r][c] != '\0'; c++) t.glyphs[r * columns + c] = glyph_bits(lines[r][c]);
    }
    memcpy(u.cpu, &t, sizeof t);
    const int w = (columns * 4 + 1) * scale, h = (rows * 6 + 1) * scale;
    if (left >= int(R.width) || top >= int(R.height)) return;
    const DkShader* sh[] = {vs, fs};
    dkCmdBufBindShaders(R.cmd, DkStageFlag_GraphicsMask, sh, 2);
    bind_2d_state(true);
    set_view(uint32_t(left), uint32_t(top), uint32_t(std::min(w, int(R.width) - left)),
             uint32_t(std::min(h, int(R.height) - top)));
    dkCmdBufBindUniformBuffer(R.cmd, DkStage_Fragment, 0, u.gpu, kUboSize);
    dkCmdBufBindVtxAttribState(R.cmd, nullptr, 0);
    dkCmdBufBindVtxBufferState(R.cmd, nullptr, 0);
    dkCmdBufDraw(R.cmd, DkPrimitive_Triangles, 3, 1, 0, 0);
}

bool pattern_enabled() {
    const char* e = getenv("COS_DK_TEST_PATTERN");
    return e && *e && *e != '0';
}

// The pattern's 8x8 texture: row 0 red, column 0 green (their corner yellow), the rest a white and
// gray checkerboard. Sampled nearest, so each texel is a block of the quad.
void pattern_init() {
    if (g_texReady) return;
    uint8_t px[kTexSize * kTexSize * 4];
    for (uint32_t y = 0; y < kTexSize; y++) {
        for (uint32_t x = 0; x < kTexSize; x++) {
            uint8_t* p = px + (y * kTexSize + x) * 4;
            const uint8_t check = ((x + y) & 1) ? 0xFF : 0x90;
            p[0] = y == 0 ? 0xFF : x == 0 ? 0x00 : check;
            p[1] = x == 0 ? 0xFF : y == 0 ? 0x00 : check;
            p[2] = (x == 0 || y == 0) ? 0x00 : check;
            p[3] = 0xFF;
        }
    }
    DkImageLayoutMaker m;
    dkImageLayoutMakerDefaults(&m, R.device);
    m.format = DkImageFormat_RGBA8_Unorm;
    m.dimensions[0] = kTexSize;
    m.dimensions[1] = kTexSize;
    image_tile_size_fix(m, kTexSize);
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &m);
    g_texMem = image_alloc(uint32_t(dkImageLayoutGetSize(&layout)), dkImageLayoutGetAlignment(&layout));
    dkImageInitialize(&g_tex, &layout, g_texMem.block, g_texMem.offset);
    StreamAlloc s = stream_alloc(sizeof px, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    if (!s) return;
    memcpy(s.cpu, px, sizeof px);
    write_image_descriptor(kPatternImage, g_tex);
    DkImageView view;
    dkImageViewDefaults(&view, &g_tex);
    const DkCopyBuf src = {s.gpu, 0, 0};
    const DkImageRect r = {0, 0, 0, kTexSize, kTexSize, 1};
    dkCmdBufCopyBufferToImage(R.cmd, &src, &view, &r, 0);
    dkCmdBufBarrier(R.cmd, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_Descriptors);
    g_texReady = true;
    dklog("test pattern: its %ux%u texture uploaded (image slot %u)", kTexSize, kTexSize, kPatternImage);
}

void pattern_draw() {
    static const float kRed[4] = {0.9f, 0.1f, 0.1f, 1}, kGreen[4] = {0.1f, 0.8f, 0.2f, 1},
                       kBlue[4] = {0.15f, 0.3f, 1.0f, 1}, kYellow[4] = {1.0f, 0.9f, 0.1f, 1},
                       kCyan[4] = {0.1f, 0.85f, 0.9f, 1}, kMagenta[4] = {0.85f, 0.15f, 0.8f, 1},
                       kOrange[4] = {1.0f, 0.55f, 0.0f, 1}, kGray[4] = {0.3f, 0.3f, 0.3f, 1},
                       kDark[4] = {0.12f, 0.12f, 0.16f, 1}, kWhite[4] = {1, 1, 1, 1}, kNone[4] = {0, 0, 0, 0};
    const NamedShader* vs = shader_cache_named("dk_test_pattern.vs_main");
    const NamedShader* fs = shader_cache_named("dk_test_pattern.fs_main");
    Draw d;
    if (vs && fs) {
        d.vs = &vs->shader;
        d.fs = &fs->shader;
        for (const SlotBinding& b : vs->bindings)
            if (b.kind == SlotBinding::Kind::Storage) d.ssbo = b.slot;
        for (const SlotBinding& b : fs->bindings)
            if (b.kind == SlotBinding::Kind::Sampler) d.sampler = b.slot;
    }
    if (d.vs && d.fs) {
        std::vector<Vertex> v;
        v.reserve(96);
        // the corners, 120 pixels square
        const float W = float(R.width), H = float(R.height);
        rect(v, 0, 0, 120, 120, 0.5f, kRed);
        rect(v, W - 120, 0, W, 120, 0.5f, kGreen);
        rect(v, 0, H - 120, 120, H, 0.5f, kBlue);
        rect(v, W - 120, H - 120, W, H, 0.5f, kYellow);
        // depth test: the near cyan square first, then the far magenta one over part of it
        rect(v, 180, 190, 420, 400, 0.25f, kCyan);
        rect(v, 300, 270, 540, 480, 0.75f, kMagenta);
        // depth range: an orange triangle at z = -0.5 (outside 0..1, clipped) over a gray square
        rect(v, 600, 190, 780, 370, 0.9f, kGray);
        vertex(v, 620, 210, -0.5f, kOrange), vertex(v, 690, 350, -0.5f, kOrange), vertex(v, 760, 210, -0.5f, kOrange);
        // the textured square: texture row 0 must be its top edge, column 0 its left edge
        rect(v, 840, 190, 1080, 430, 0.5f, kWhite, true);
        // the boxes behind the two culling triangles
        rect(v, 180, 500, 420, 680, 0.95f, kDark);
        rect(v, 560, 500, 800, 680, 0.95f, kDark);
        // culling: counter-clockwise on the screen (front with FrontFace::CCW) and clockwise (culled)
        std::vector<Vertex> tris;
        vertex(tris, 300, 515, 0.5f, kWhite), vertex(tris, 195, 665, 0.5f, kWhite), vertex(tris, 405, 665, 0.5f, kWhite);
        vertex(tris, 680, 515, 0.5f, kOrange), vertex(tris, 785, 665, 0.5f, kOrange), vertex(tris, 575, 665, 0.5f, kOrange);

        const DkShader* sh[] = {d.vs, d.fs};
        dkCmdBufBindShaders(R.cmd, DkStageFlag_GraphicsMask, sh, 2);
        DkColorState cs;
        dkColorStateDefaults(&cs);
        dkCmdBufBindColorState(R.cmd, &cs);
        DkColorWriteState cw;
        dkColorWriteStateDefaults(&cw);
        dkCmdBufBindColorWriteState(R.cmd, &cw);
        DkDepthStencilState ds;
        dkDepthStencilStateDefaults(&ds);
        ds.depthTestEnable = true;
        ds.depthWriteEnable = true;
        ds.depthCompareOp = DkCompareOp_Less;
        dkCmdBufBindDepthStencilState(R.cmd, &ds);
        set_view(0, 0, R.width, R.height);
        dkCmdBufBindVtxAttribState(R.cmd, nullptr, 0);
        dkCmdBufBindVtxBufferState(R.cmd, nullptr, 0);
        const DkResHandle h = dkMakeTextureHandle(kPatternImage, kSamplerNearestClamp);
        dkCmdBufBindTexture(R.cmd, DkStage_Fragment, d.sampler, h);
        draw_vertices(d, v, false);
        draw_vertices(d, tris, true);
    }

    // the legend: what the photo must show
    static const float fg[4] = {1, 1, 1, 1}, bg[4] = {0, 0, 0, 0.6f};
    char frame[96];
    snprintf(frame, sizeof frame, "DEKO3D TEST PATTERN (COS_DK_TEST_PATTERN=1), FRAME %llu",
             (unsigned long long)R.frame + 1);
    std::string status = std::string("SHADERS: ") + shader_cache_status();
    for (char& c : status) c = char(toupper(uint8_t(c)));
    const char* legend[] = {
        frame,
        status.c_str(),
        d.vs ? "THIS TEXT UPRIGHT: WINDOW ORIGIN TOP LEFT" : "NO DK_TEST_PATTERN SHADERS IN THE CACHE: TEXT ONLY",
        "CORNERS: RED TOP LEFT, GREEN TOP RIGHT, BLUE BOTTOM LEFT, YELLOW BOTTOM RIGHT",
        "MIDDLE ROW: CYAN IN FRONT OF MAGENTA, GRAY SQUARE EMPTY, RED TEXTURE ROW AT THE TOP",
        "BOTTOM ROW: WHITE TRIANGLE (CCW) SHOWN, ORANGE (CW) CULLED: ITS BOX EMPTY",
    };
    draw_text(140, 12, 2, legend, int(sizeof legend / sizeof legend[0]), fg, bg);
    static const char* const lDepth[] = {"DEPTH: CYAN NEAR (0.25)", "MAGENTA FAR (0.75)"};
    static const char* const lRange[] = {"DEPTH RANGE: ORANGE", "AT Z -0.5, CLIPPED"};
    static const char* const lTex[] = {"TEXTURE: ROW 0 RED (TOP)", "COLUMN 0 GREEN (LEFT)"};
    static const char* const lCcw[] = {"CCW: MUST SHOW"};
    static const char* const lCw[] = {"CW: MUST BE CULLED"};
    draw_text(180, 152, 2, lDepth, 2, fg, kNone);
    draw_text(600, 152, 2, lRange, 2, fg, kNone);
    draw_text(840, 152, 2, lTex, 2, fg, kNone);
    draw_text(180, 686, 2, lCcw, 1, fg, kNone);
    draw_text(560, 686, 2, lCw, 1, fg, kNone);
}

}  // namespace swdk
