// The game-only GF declarations Aurora's GF headers lack.
//
// Phase 2, step 2.4 (docs/NATIVE_PORT_PHASE2_3.md, decision D2). The game's dolphin/gf headers declare
// more than Aurora's <dolphin/gf/*.h>: GFSetArray, GFBegin/GFEnd, the GFTransform functions... The
// names GF.h and GFTransform.h are game-only, and their forwarders include this header. The names
// both have (GFGeometry.h, GFLight.h, GFPixel.h, GFTev.h) resolve to Aurora's, so a game unit that
// includes only one of those and uses a name below includes GF.h instead under TARGET_PC (step
// 2.7). Step 2.6d compiles the game's GF sources into cos_sdk against Aurora's headers plus this one.
//
// The BP_*, XF_* and CP_MTX_* register macros of the game's GF headers are Aurora's GD macros (same
// text), so they come from <dolphin/gd/*.h> here rather than being defined again.
//
// Linkage: as in the game's and Aurora's GF headers, these have C++ linkage (the GF sources are C++).
// The inline helpers are `static inline`, as Aurora writes GFWrite_u8 and the like (the game's are plain
// `inline`); they write through Aurora's GXCmd1u*.
#ifndef COS_GF_EXTRAS_H
#define COS_GF_EXTRAS_H

#include <dolphin/gf.h>
#include <dolphin/gd/GDLight.h>
#include <dolphin/gd/GDPixel.h>
#include <dolphin/gd/GDTev.h>
#include <dolphin/gd/GDTransform.h>
#include <dolphin/mtx.h>

// The game's GF.h.
static inline void GFWriteCPCmd(u8 addr, u32 val) {
    GFWrite_u8(GX_LOAD_CP_REG);
    GFWrite_u8(addr);
    GFWrite_u32(val);
}

static inline void GFWriteXFCmdHdr(u16 addr, u8 len) {
    GFWrite_u8(GX_LOAD_XF_REG);
    GFWrite_u16(len - 1);
    GFWrite_u16(addr);
}

// The game's GFGeometry.h.
void GFSetVtxDescv(GXVtxDescList*);
void GFSetVtxAttrFmtv(GXVtxFmt, GXVtxAttrFmtList*);
void GFSetArray(GXAttr, void*, u8);
// Not in the game: the Aurora form of GFSetArray (step 2.6d), as Aurora's GDSetArraySized. Aurora
// ignores CP_REG_ARRAYBASE, so on Aurora GFSetArray stops with OSPanic and the game's callers pass
// the array's size in bytes and byte order (le: built by the host, not loaded from the disc).
void GFSetArraySized(GXAttr attr, void* base_ptr, u32 size, u8 stride, bool le);
void GFSetCullMode(GXCullMode);

static inline void GFBegin(GXPrimitive type, GXVtxFmt fmt, u16 vert_num) {
    GFWrite_u8(fmt | type);
    GFWrite_u16(vert_num);
}

static inline void GFEnd(void) {}

static inline void GFWrite_f32(f32 f) {
    union {
        f32 f;
        u32 u;
    } data;
    data.f = f;
    GXCmd1u32(data.u);
}

static inline void GFWrite_s16(s16 s) {
    GXCmd1u16(s);
}

static inline void GFPosition3f32(f32 x, f32 y, f32 z) {
    GFWrite_f32(x);
    GFWrite_f32(y);
    GFWrite_f32(z);
}

static inline void GFTexCoord2s16(s16 u, s16 v) {
    GFWrite_s16(u);
    GFWrite_s16(v);
}

// The game's GFLight.h.
void GFSetChanMatColor(GXChannelID, GXColor);

// The game's GFPixel.h.
void GFSetDstAlpha(u8, u8);

// The game's GFTev.h.
void GFSetTevColor(GXTevRegID, GXColor);
void GFSetAlphaCompare(GXCompare, u8, GXAlphaOp, GXCompare, u8);

// The game's GFTransform.h. The decomp's MtxP is Aurora's MtxPtr (both f32 (*)[4]); this header does
// not need the MTX forwarder for it.
void GFLoadPosMtxImm(MtxPtr, u32);
void GFLoadNrmMtxImm(MtxPtr, u32);
void GFSetCurrentMtx(u32, u32, u32, u32, u32, u32, u32, u32, u32);

#endif // COS_GF_EXTRAS_H
