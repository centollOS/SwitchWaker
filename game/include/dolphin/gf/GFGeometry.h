#ifndef GFGEOMETRY_H
#define GFGEOMETRY_H

#include "dolphin/gx/GX.h"

void GFSetVtxDescv(GXVtxDescList*);
void GFSetVtxAttrFmtv(GXVtxFmt, GXVtxAttrFmtList*);
void GFSetArray(GXAttr, void*, u8);
void GFSetCullMode(GXCullMode);

inline void GFWrite_u8(u8 data) {
    GXCmd1u8(data);
}

inline void GFWrite_u16(u16 data) {
    GXCmd1u16(data);
}

inline void GFBegin(GXPrimitive type, GXVtxFmt fmt, u16 vert_num) {
    GFWrite_u8(fmt | type);
    GFWrite_u16(vert_num);
}

inline void GFEnd() {}

// GFWrite_f32/GFWrite_s16 go through GXCmd1u32/GXCmd1u16 like GFWrite_u8/u16 (sizes 0x30/0x2C in the demo debug map).
inline void GFWrite_f32(f32 f) {
    union {
        f32 f;
        u32 u;
    } data;
    data.f = f;
    GXCmd1u32(data.u);
}

inline void GFWrite_s16(s16 s) {
    GXCmd1u16(s);
}

inline void GFPosition3f32(f32 x, f32 y, f32 z) {
    GFWrite_f32(x);
    GFWrite_f32(y);
    GFWrite_f32(z);
}

inline void GFTexCoord2s16(s16 u, s16 v) {
    GFWrite_s16(u);
    GFWrite_s16(v);
}

#endif /* GFGEOMETRY_H */
