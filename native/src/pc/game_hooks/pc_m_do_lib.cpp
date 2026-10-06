// mDoLib_loadDLTexImage (m_Do_lib.h), moved out of game/src/m_Do/m_Do_lib.cpp (step G3 of
// docs/GAME_CODE_ORGANIZATION.md): host code with no GameCube counterpart.
#include "m_Do/machine.h" // IWYU pragma: keep
#include "m_Do/m_Do_lib.h"
#include "dolphin/gx/GX.h"
#include "dolphin/os/OS.h"

// The packet materials (grass, trees, flowers, chains...) come as static display lists from the
// DOL whose texture address is a BP SETIMAGE3 write of the image's physical address >> 5
// (IMAGE_ADDR in the asset headers). A 32-bit physical address cannot hold a host pointer, and
// Aurora binds a texture's image only through GXLoadTexObj: its BP handler keeps SETIMAGE3 but
// never reads it, while the list's SETIMAGE0 overwrites the slot's size and format. Such a list
// therefore sampled whatever image was last loaded in that texture map, decoded with the list's
// size and format (the grass drew as quads of purple/blue garbage with no alpha cut-out).
// mDoLib_loadDLTexImage reads the list's own texture registers (SETMODE0/1, SETIMAGE0 and the
// maps SETIMAGE3 names) and loads one cached GXTexObj per image and register set before the
// list runs, as Dusklight loads a GXTexObj before TP's grass material lists (CC0,
// src/d/actor/d_grass.inc); the list then writes the same size, format and modes again.
namespace {
struct DLTexImage {
    const void* image;
    u32 mode0;
    u32 mode1;
    u32 image0;
    GXTexObj obj;
};

DLTexImage l_dlTexImages[32];
int l_dlTexImageNum;

GXTexObj* dlTexImageObj(const void* image, u32 mode0, u32 mode1, u32 image0) {
    for (int i = 0; i < l_dlTexImageNum; i++) {
        DLTexImage& e = l_dlTexImages[i];
        if (e.image == image && e.mode0 == mode0 && e.mode1 == mode1 && e.image0 == image0) {
            return &e.obj;
        }
    }
    if (l_dlTexImageNum >= (int)ARRAY_SIZE(l_dlTexImages)) {
        OSPanic(__FILE__, __LINE__, "mDoLib_loadDLTexImage: more than %d display-list textures",
                (int)ARRAY_SIZE(l_dlTexImages));
    }

    const u16 width = (u16)((image0 & 0x3FF) + 1);
    const u16 height = (u16)(((image0 >> 10) & 0x3FF) + 1);
    const u32 format = (image0 >> 20) & 0xF;
    if (format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2) {
        // No static material list uses a colour-indexed image; its TLUT would need the same care.
        OSPanic(__FILE__, __LINE__, "mDoLib_loadDLTexImage: colour-indexed format %u", format);
    }
    static const GXTexFilter hw2MinFilt[8] = {
        GX_NEAR, GX_NEAR_MIP_NEAR, GX_NEAR_MIP_LIN, GX_NEAR,
        GX_LINEAR, GX_LIN_MIP_NEAR, GX_LIN_MIP_LIN, GX_NEAR,
    };
    const f32 minLod = (f32)(mode1 & 0xFF) / 16.0f;
    const f32 maxLod = (f32)((mode1 >> 8) & 0xFF) / 16.0f;

    DLTexImage& e = l_dlTexImages[l_dlTexImageNum++];
    e.image = image;
    e.mode0 = mode0;
    e.mode1 = mode1;
    e.image0 = image0;
    GXInitTexObj(&e.obj, (void*)image, width, height, (GXTexFmt)format, (GXTexWrapMode)(mode0 & 3),
                 (GXTexWrapMode)((mode0 >> 2) & 3), maxLod > 0.0f ? GX_TRUE : GX_FALSE);
    GXInitTexObjLOD(&e.obj, hw2MinFilt[(mode0 >> 5) & 7], (mode0 >> 4) & 1 ? GX_LINEAR : GX_NEAR, minLod,
                    maxLod, (f32)(s8)((mode0 >> 9) & 0xFF) / 32.0f, (mode0 >> 21) & 1 ? GX_TRUE : GX_FALSE,
                    (mode0 >> 8) & 1 ? GX_FALSE : GX_TRUE, (GXAnisotropy)((mode0 >> 19) & 3));
    return &e.obj;
}
}  // namespace

void mDoLib_loadDLTexImage(const void* dl, u32 size, const void* image) {
    // Texture registers per map: 0x80-0x83 and 0xA0-0xA3 MODE0, +4 MODE1, +8 IMAGE0, +0x14 IMAGE3.
    u32 mode0[8] = {}, mode1[8] = {}, image0[8] = {};
    bool named[8] = {};
    const u8* p = (const u8*)dl;
    const u8* end = p + size;
    while (p < end) {
        const u8 cmd = *p;
        if (cmd == GX_NOP || cmd == 0x48) {  // NOP, invalidate vertex cache
            p += 1;
        } else if (cmd == GX_LOAD_BP_REG) {
            if (end - p < 5)
                break;
            const u8 reg = p[1];
            const u32 val = ((u32)p[2] << 16) | ((u32)p[3] << 8) | p[4];
            if (reg >= 0x80 && reg < 0xC0) {
                const u32 idx = reg - 0x80;
                const u32 map = (idx & 3) + (idx >= 0x20 ? 4 : 0);
                switch ((idx & 0x1F) / 4) {
                case 0: mode0[map] = val; break;
                case 1: mode1[map] = val; break;
                case 2: image0[map] = val; break;
                case 5: named[map] = true; break;
                default: break;
                }
            }
            p += 5;
        } else if (cmd == GX_LOAD_CP_REG) {
            p += 6;
        } else if (cmd == GX_LOAD_XF_REG) {
            if (end - p < 5)
                break;
            const u32 num = (((u32)p[1] << 8) | p[2]) + 1;
            p += 5 + 4 * num;
        } else if (cmd == 0x20 || cmd == 0x28 || cmd == 0x30 || cmd == 0x38) {  // indexed XF loads
            p += 5;
        } else {
            break;  // a draw or call: material lists end before their first one
        }
    }

    bool any = false;
    for (int map = 0; map < 8; map++) {
        if (named[map]) {
            GXLoadTexObj(dlTexImageObj(image, mode0[map], mode1[map], image0[map]), (GXTexMapID)map);
            any = true;
        }
    }
    if (!any) {
        OSPanic(__FILE__, __LINE__, "mDoLib_loadDLTexImage: display list %p names no texture image", dl);
    }
}
