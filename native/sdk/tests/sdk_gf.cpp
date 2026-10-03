// cos_sdk_smoke: the "gf" test of step 2.6d (the game's GF sources, game/src/dolphin/gf, compiled
// into cos_sdk over Aurora).
//
// GF is the immediate-mode twin of GD: the same BP/XF/CP register commands, written through
// GXCmd1u* instead of into a GDLObj. Each GF call is recorded into a GX display list (Aurora's
// FIFO writes go to the list's buffer, no GPU needed) and compared byte for byte with the GD
// function of the same name from Aurora's aurora_gd, an independent encoder. The GF functions
// Aurora's GD lacks are checked against hand-encoded commands.
//
// Includes the game's dolphin/gf/GF.h through its forwarder (native/include/sdk), as the game will, so
// the GF names this file references are mangled as the game's calls will be.
#include "smoke.h"

#include <dolphin/gd.h>
#include <dolphin/gf/GF.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr u32 kBufSize = 1024;

alignas(32) u8 sGfBuf[kBufSize];
alignas(32) u8 sGdBuf[kBufSize];

// Records what `fn` writes through GXCmd1u* (the GF path) into sGfBuf. GXEndDisplayList pads to
// 32 bytes with zeros and returns only the padded size, so SameAs takes the length from the
// expected bytes and checks that the rest up to the padding is zero.
template <typename Fn>
void RecordGF(Fn fn) {
    std::memset(sGfBuf, 0xCD, sizeof(sGfBuf));
    GXBeginDisplayList(sGfBuf, kBufSize);
    fn();
    (void)GXEndDisplayList();
}

// Records what `fn` writes through Aurora's GD (into a GDLObj); returns the byte count.
template <typename Fn>
u32 RecordGD(Fn fn) {
    std::memset(sGdBuf, 0xCD, sizeof(sGdBuf));
    GDLObj dl;
    GDInitGDLObj(&dl, sGdBuf, kBufSize);
    GDLObj* const old = __GDCurrentDL;
    GDSetCurrent(&dl);
    fn();
    const u32 size = GDGetCurrOffset();
    GDSetCurrent(old);
    return size;
}

// The GF recording equals `expected` (size bytes) and is followed by the display list's padding.
bool SameAs(const u8* expected, u32 size, const char* what) {
    if (size == 0 || size > kBufSize - 32) {
        std::fprintf(stderr, "gf: %s: bad expected size %u\n", what, size);
        return false;
    }
    if (std::memcmp(sGfBuf, expected, size) != 0) {
        std::fprintf(stderr, "gf: %s: GF bytes differ:\n  got ", what);
        for (u32 i = 0; i < size; i++) std::fprintf(stderr, "%02x", sGfBuf[i]);
        std::fprintf(stderr, "\n  exp ");
        for (u32 i = 0; i < size; i++) std::fprintf(stderr, "%02x", expected[i]);
        std::fprintf(stderr, "\n");
        return false;
    }
    const u32 padded = (size + 31) & ~31u;
    for (u32 i = size; i < padded; i++) {
        if (sGfBuf[i] != 0) {
            std::fprintf(stderr, "gf: %s: GF wrote more than %u bytes\n", what, size);
            return false;
        }
    }
    return true;
}

#define GF_SAME_AS_GD(gfCall, gdCall)                                                              \
    do {                                                                                           \
        RecordGF([&] { gfCall; });                                                                 \
        const u32 gdSize = RecordGD([&] { gdCall; });                                              \
        COS_SMOKE_CHECK(SameAs(sGdBuf, gdSize, #gfCall));                                          \
    } while (0)

// A big-endian byte writer for the hand-encoded commands.
struct Bytes {
    u8 data[256];
    u32 size = 0;
    Bytes& u8_(u32 v) { data[size++] = (u8)v; return *this; }
    Bytes& u16_(u32 v) { return u8_(v >> 8).u8_(v); }
    Bytes& u32_(u32 v) { return u16_(v >> 16).u16_(v); }
    Bytes& bp(u32 v) { return u8_(GX_LOAD_BP_REG).u32_(v); }
    Bytes& cp(u32 addr, u32 v) { return u8_(GX_LOAD_CP_REG).u8_(addr).u32_(v); }
};

u32 F32Bits(f32 f) {
    u32 u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

} // namespace

COS_SMOKE_TEST(gf) {
    const GXColor color = {0x12, 0x34, 0x56, 0x78};
    const GXColorS10 colorS10 = {-0x100, 0x1FF, -1, 0x3FF};

    // ---- the same command stream as Aurora's GD ----------------------------------------------
    GF_SAME_AS_GD(GFSetChanMatColor(GX_COLOR1A1, color), GDSetChanMatColor(GX_COLOR1A1, color));
    GF_SAME_AS_GD(GFSetTevColor(GX_TEVREG2, color), GDSetTevColor(GX_TEVREG2, color));
    GF_SAME_AS_GD(GFSetTevColorS10(GX_TEVREG1, colorS10), GDSetTevColorS10(GX_TEVREG1, colorS10));
    GF_SAME_AS_GD(GFSetAlphaCompare(GX_GEQUAL, 0x40, GX_AOP_OR, GX_LESS, 0xC0),
                  GDSetAlphaCompare(GX_GEQUAL, 0x40, GX_AOP_OR, GX_LESS, 0xC0));
    GF_SAME_AS_GD(GFSetDstAlpha(GX_TRUE, 0x80), GDSetDstAlpha(GX_TRUE, 0x80));
    GF_SAME_AS_GD(GFSetBlendModeEtc(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY,
                                    GX_TRUE, GX_FALSE, GX_TRUE),
                  GDSetBlendModeEtc(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_COPY,
                                    GX_TRUE, GX_FALSE, GX_TRUE));
    GF_SAME_AS_GD(GFSetFog(GX_FOG_PERSP_LIN, 100.0f, 5000.0f, 10.0f, 100000.0f, color),
                  GDSetFog(GX_FOG_PERSP_LIN, 100.0f, 5000.0f, 10.0f, 100000.0f, color));
    GF_SAME_AS_GD(GFSetFog(GX_FOG_NONE, 0.0f, 0.0f, 1.0f, 1.0f, color),
                  GDSetFog(GX_FOG_NONE, 0.0f, 0.0f, 1.0f, 1.0f, color));
    GF_SAME_AS_GD(GFSetCurrentMtx(GX_PNMTX3, GX_TEXMTX0, GX_TEXMTX1, GX_TEXMTX2, GX_TEXMTX3,
                                  GX_TEXMTX4, GX_TEXMTX5, GX_TEXMTX6, GX_TEXMTX7),
                  GDSetCurrentMtx(GX_PNMTX3, GX_TEXMTX0, GX_TEXMTX1, GX_TEXMTX2, GX_TEXMTX3,
                                  GX_TEXMTX4, GX_TEXMTX5, GX_TEXMTX6, GX_TEXMTX7));

    Mtx m;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            m[r][c] = 1.5f * (f32)(r * 4 + c) - 3.0f;
        }
    }
    GF_SAME_AS_GD(GFLoadPosMtxImm(m, GX_PNMTX2), GDLoadPosMtxImm(m, GX_PNMTX2));
    GF_SAME_AS_GD(GFLoadNrmMtxImm(m, GX_PNMTX2), GDLoadNrmMtxImm(m, GX_PNMTX2));

    GXVtxDescList desc[] = {
        {GX_VA_PNMTXIDX, GX_DIRECT}, {GX_VA_TEX1MTXIDX, GX_DIRECT}, {GX_VA_POS, GX_INDEX16},
        {GX_VA_NRM, GX_INDEX8},      {GX_VA_CLR0, GX_DIRECT},       {GX_VA_TEX0, GX_INDEX16},
        {GX_VA_TEX3, GX_DIRECT},     {GX_VA_NULL, GX_NONE},
    };
    GF_SAME_AS_GD(GFSetVtxDescv(desc), GDSetVtxDescv(desc));

    GXVtxAttrFmtList fmt[] = {
        {GX_VA_POS, GX_POS_XYZ, GX_S16, 7},  {GX_VA_NRM, GX_NRM_NBT3, GX_S8, 6},
        {GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0}, {GX_VA_TEX0, GX_TEX_ST, GX_U16, 8},
        {GX_VA_TEX5, GX_TEX_S, GX_S8, 3},    {GX_VA_NULL, GX_POS_XYZ, GX_F32, 0},
    };
    GF_SAME_AS_GD(GFSetVtxAttrFmtv(GX_VTXFMT5, fmt), GDSetVtxAttrFmtv(GX_VTXFMT5, fmt));

    // ---- hand-encoded --------------------------------------------------------------------------
    // GFSetCullMode: BP mask (only the cull bits), then gen mode with the hardware cull encoding
    // (GX_CULL_FRONT is 2 on the hardware).
    {
        RecordGF([] { GFSetCullMode(GX_CULL_FRONT); });
        Bytes exp;
        exp.bp(0xFE00C000).bp(2u << 14);
        COS_SMOKE_CHECK(SameAs(exp.data, exp.size, "GFSetCullMode"));
    }

    // GFBegin / GFPosition3f32 / GFTexCoord2s16 (inline in cos_gf_extras.h): the primitive header
    // and raw vertex data, big-endian.
    {
        RecordGF([] {
            GFBegin(GX_QUADS, GX_VTXFMT2, 4);
            GFPosition3f32(1.0f, -2.5f, 3.25f);
            GFTexCoord2s16(-2, 0x1234);
            GFEnd();
        });
        Bytes exp;
        exp.u8_(GX_QUADS | GX_VTXFMT2).u16_(4);
        exp.u32_(F32Bits(1.0f)).u32_(F32Bits(-2.5f)).u32_(F32Bits(3.25f));
        exp.u16_(0xFFFE).u16_(0x1234);
        COS_SMOKE_CHECK(SameAs(exp.data, exp.size, "GFBegin"));
    }

    // GFSetArraySized (TARGET_PC): the same Aurora array-base command and stride as Aurora's
    // GDSetArraySized, the full 64-bit pointer included; GX_VA_NBT uses the normal array.
    static f32 sArray[12];
    GF_SAME_AS_GD(GFSetArraySized(GX_VA_TEX2, sArray, sizeof(sArray), 8, true),
                  GDSetArraySized(GX_VA_TEX2, sArray, sizeof(sArray), 8, true));
    GF_SAME_AS_GD(GFSetArraySized(GX_VA_NBT, sArray, sizeof(sArray), 9, false),
                  GDSetArraySized(GX_VA_NBT, sArray, sizeof(sArray), 9, false));
    {
        RecordGF([] { GFSetArraySized(GX_VA_POS, sArray, sizeof(sArray), 12, true); });
        const u64 addr = (u64)(uintptr_t)sArray;
        Bytes exp;
        exp.u8_(GX_AURORA).u16_(GX_AURORA_LOAD_ARRAYBASE + 0);
        exp.u32_((u32)(addr >> 32)).u32_((u32)addr).u32_(sizeof(sArray)).u8_(1);
        exp.cp(CP_REG_ARRAYSTRIDE_ID + 0, 12);
        COS_SMOKE_CHECK(SameAs(exp.data, exp.size, "GFSetArraySized"));
    }

    // GFSetArray has no size, so on Aurora it stops loudly (OSPanic) instead of writing a
    // truncated CP_REG_ARRAYBASE that Aurora would ignore. Run it in a forked child.
    {
        int fds[2];
        COS_SMOKE_CHECK(pipe(fds) == 0);
        std::fflush(nullptr);
        const pid_t pid = fork();
        COS_SMOKE_CHECK(pid >= 0);
        if (pid == 0) {
            close(fds[0]);
            dup2(fds[1], STDERR_FILENO);
            dup2(fds[1], STDOUT_FILENO);
            GFSetArray(GX_VA_POS, sArray, 12);
            _exit(0); // not reached if GFSetArray stops
        }
        close(fds[1]);
        std::string out;
        char buf[256];
        ssize_t n;
        while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
            out.append(buf, (size_t)n);
        }
        close(fds[0]);
        int status = 0;
        COS_SMOKE_CHECK(waitpid(pid, &status, 0) == pid);
        COS_SMOKE_CHECK(!(WIFEXITED(status) && WEXITSTATUS(status) == 0));
        COS_SMOKE_CHECK(out.find("GFSetArraySized") != std::string::npos);
    }

    // Outside a display list the same calls go to Aurora's command FIFO (no GPU is needed to
    // write it; it is only parsed at drain): they must not write into the last list's buffer.
    std::memset(sGfBuf, 0xEE, sizeof(sGfBuf));
    GFSetTevColor(GX_TEVREG0, color);
    GFLoadPosMtxImm(m, GX_PNMTX0);
    for (u32 i = 0; i < kBufSize; i++) {
        COS_SMOKE_CHECK(sGfBuf[i] == 0xEE);
    }
    return true;
}
