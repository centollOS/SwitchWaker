// Forwarder (phase 2, step 2.4): the game's dolphin/gx/GX.h over Aurora's <dolphin/gx.h>, which brings
// in every GX sub-header, PPCWGPipe, GXFIFO_ADDR, GX_LOAD_BP_REG, GX_NOP and the GXCmd1u*,
// GXPosition*, GXNormal*, GXColor* and GXTexCoord* vertex functions (out-of-line under TARGET_PC,
// recorded by aurora_gx). The game-only GX sub-header names come from their own forwarders.
//
// Added here, the game-only names with a host meaning:
// - the register bit-field macros (GX_BITFIELD_SET, GX_BITGET, GX_GET_REG, GX_SET_REG,
//   GXCOLOR_AS_U32, INSERT_FIELD, GET_REG_FIELD, SET_REG_FIELD), same text as the decomp;
// - GXColor3x8 and GXColor4x8, the decomp's names for Aurora's GXColor3u8 and GXColor4u8 (the same
//   bytes, written through Aurora);
// - the decomp's GXBlendFactor spellings (GX_BL_SRC_ALPHA, ...) as macros for Aurora's enumerators of
//   the same value (GX_BL_SRCALPHA, ...), step 2.7;
// - the decomp's GXLogicOp spellings (GX_LO_REV_AND, GX_LO_INV_COPY, ...) as macros for Aurora's
//   enumerators of the same value (GX_LO_REVAND, GX_LO_INVCOPY, ...), step 2.7 (JParticle);
// - the BP/CP/XF register numbers and field locators (GX_BP_REG_*, GX_XF_*, ...), which the decomp's
//   GX.h exports through its GXEnum.h and Aurora keeps in <dolphin/gd/GDGeometry.h> (same values);
//   J3DGD/J3DTevs use them, step 2.7 (J3D);
// - GX_MAXCOORD and GX_TEXMAP_DISABLE, the decomp's names for Aurora's GX_MAX_TEXCOORD (8) and
//   GX_TEX_DISABLE (0x100), step 2.7 (J3D).
//
// Left out on purpose:
// - GXFIFO, the write-gather pipe the decomp defines in the header at 0xCC008000 (a definition in
//   every unit; on the host, writes that go nowhere), and the macros that write through it
//   (GX_CP_LOAD_REG, GX_XF_LOAD_REG*, GX_BP_LOAD_REG, GX_WRITE_*, GX_WRITE_SOME_REG*). Game code
//   that writes GXFIFO directly (J3DGD.h, J3DShape) moves to Aurora's GXCmd1u* under TARGET_PC in
//   step 2.7, so it fails to compile until then instead of writing nowhere;
// - GX_BITFIELD_TRUNC and GX_SET_TRUNC (Metrowerks' __rlwimi intrinsic), VERIF_*, and
//   __GXReadCPCounterU32 (reads the CP registers): SDK-internal, no host meaning.
#ifndef COS_SDK_DOLPHIN_GX_GX_H
#define COS_SDK_DOLPHIN_GX_GX_H

#include <dolphin/gx.h>
#include <dolphin/gd/GDGeometry.h>
#include <dolphin/gx/GXAttr.h>
#include <dolphin/gx/GXDisplayList.h>
#include <dolphin/gx/GXFrameBuf.h>
#include <dolphin/gx/GXInit.h>
#include <dolphin/gx/GXLight.h>
#include <dolphin/gx/GXMisc.h>
#include <dolphin/os/OSUtil.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pack value into bitfield
#define GX_BITFIELD_SET(field, pos, size, value)                                                   \
    (field) =                                                                                      \
    (field & ~(((1 << (size)) - 1) << (31 - (pos) - (size) + 1))) |                                \
    ((int)(value) << (31 - (pos) - (size) + 1))

#define GX_BITGET(field, pos, size)              ((field) >> (31 - (pos) - (size) + 1) & ((1 << (size)) - 1))
#define GX_GET_REG(reg, st, end)    GX_BITGET(reg, st, (end - st + 1))
#define GX_SET_REG(reg, x, st, end) GX_BITFIELD_SET(reg, st, (end - st + 1), x)

#define GXCOLOR_AS_U32(color) (*((u32*)&(color)))

#define INSERT_FIELD(reg, value, nbits, shift)                                 \
    (reg) = ((u32) (reg) & ~(((1 << (nbits)) - 1) << (shift))) |               \
            ((u32) (value) << (shift));

#define GET_REG_FIELD(reg, size, shift) ((int)((reg) >> (shift)) & ((1 << (size)) - 1))

#define SET_REG_FIELD(reg, size, shift, val) \
    (reg) = ((u32)(reg) & ~(((1 << (size)) - 1) << (shift))) | ((u32)(val) << (shift)); \

static inline void GXColor3x8(u8 r, u8 g, u8 b) {
    GXColor3u8(r, g, b);
}

static inline void GXColor4x8(u8 r, u8 g, u8 b, u8 a) {
    GXColor4u8(r, g, b, a);
}

// The decomp's GXBlendFactor names; the values are the same (0x2..0x7).
#define GX_BL_SRC_COLOR     GX_BL_SRCCLR
#define GX_BL_DST_COLOR     GX_BL_DSTCLR
#define GX_BL_INV_SRC_COLOR GX_BL_INVSRCCLR
#define GX_BL_INV_DST_COLOR GX_BL_INVDSTCLR
#define GX_BL_SRC_ALPHA     GX_BL_SRCALPHA
#define GX_BL_INV_SRC_ALPHA GX_BL_INVSRCALPHA
#define GX_BL_DST_ALPHA     GX_BL_DSTALPHA
#define GX_BL_INV_DST_ALPHA GX_BL_INVDSTALPHA

// The decomp's GXLogicOp spellings; same values (0x2, 0x4, 0xB, 0xC, 0xD).
#define GX_LO_REV_AND  GX_LO_REVAND
#define GX_LO_INV_AND  GX_LO_INVAND
#define GX_LO_REV_OR   GX_LO_REVOR
#define GX_LO_INV_COPY GX_LO_INVCOPY
#define GX_LO_INV_OR   GX_LO_INVOR

// The decomp's GXTexCoordID / GXTexMapID spellings; same values.
#define GX_MAXCOORD       GX_MAX_TEXCOORD
#define GX_TEXMAP_DISABLE GX_TEX_DISABLE

#ifdef __cplusplus
}
#endif

#endif
