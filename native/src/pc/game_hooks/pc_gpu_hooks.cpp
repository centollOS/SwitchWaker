// The GPU options' changes to the game's drawing (pc_gpu_opts.h; docs/SWITCH_PERF_STUDY.md),
// moved out of game/ (step G3 of docs/GAME_CODE_ORGANIZATION.md): the forest mist of
// drawCloudShadow (d_kankyo_rain.cpp), the sky and the A/B copies of mDoGph_Painter
// (m_Do_graphic.cpp) and the shadow casters' offscreen target (d_drawlist.cpp). Declared in
// native/include/pc/game_hooks.h; the game calls them under TARGET_PC.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_gpu_opts.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "f_op/f_op_camera.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DSys.h"

// ---- drawCloudShadow (d_kankyo_rain.cpp) ------------------------------------------------------

// The whole EFB (logical 640x480) into an RGBA8 copy texture named by buf.
static void pcMistCopyEfb(void* buf) {
    GXSetTexCopySrc(0, 0, 640, 480);
    GXSetTexCopyDst(640, 480, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(buf, GX_FALSE);
    GXPixModeSync();
}

// Draws the RGBA8 copy texture named by buf over the current viewport: premultiplied over the EFB
// (ONE, INV_SRC_ALPHA; colour only) when blend, else replacing colour and alpha (nearest texels).
static void pcMistDrawFullscreen(void* buf, GXBool blend, u32 w, u32 h) {
    GXTexObj texObj;
    GXInitTexObj(&texObj, buf, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    const GXTexFilter filter = blend ? GX_LINEAR : GX_NEAR;
    GXInitTexObjLOD(&texObj, filter, filter, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    if (blend) {
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INV_SRC_ALPHA, GX_LO_SET);
        GXSetColorUpdate(GX_TRUE);
        GXSetAlphaUpdate(GX_FALSE);
    } else {
        GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
        GXSetColorUpdate(GX_TRUE);
        GXSetAlphaUpdate(GX_TRUE);
    }
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetFog(GX_FOG_NONE, 0.0f, 1.0f, 0.1f, 1.0f, g_clearColor);
    GXSetFogRangeAdj(GX_FALSE, 0, NULL);
    GXSetCullMode(GX_CULL_NONE);
    Mtx44 ortho;
    C_MTXOrtho(ortho, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 10.0f);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    GXLoadPosMtxImm(cMtx_getIdentity(), GX_PNMTX1);
    GXSetCurrentMtx(GX_PNMTX1);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_S8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
    GXBegin(GX_QUADS, GX_VTXFMT1, 4);
    GXPosition3s8(0, 0, -5);
    GXTexCoord2s8(0, 0);
    GXPosition3s8(1, 0, -5);
    GXTexCoord2s8(1, 0);
    GXPosition3s8(1, 1, -5);
    GXTexCoord2s8(1, 1);
    GXPosition3s8(0, 1, -5);
    GXTexCoord2s8(0, 1);
    GXEnd();
    view_class* view = dComIfGd_getView();
    if (view != NULL) {
        GXSetProjection(view->mProjMtx, GX_PERSPECTIVE);
    }
    GXSetCurrentMtx(GX_PNMTX0);
}

static void pcMistDrawFullscreen(void* buf, GXBool blend) {
    pcMistDrawFullscreen(buf, blend, 640, 480);
}

// COS_MIST_LOWRES (pc_gpu_opts.h; docs/SWITCH_PERF_STUDY.md, section 8): the forest mist
// ("moya", up to 100 camera-facing sprites drawn with no depth test, blended with
// SRC_ALPHA/INV_SRC_ALPHA) costs 10-12 screens of blended fragments in A_mori at 1280x720. Drawn
// into an offscreen target 1/divisor the EFB's size and composited, the result is the same
// blend: each sprite i of alpha a_i and colour c_i turns the scene S into S(1-a_i) + c_i a_i, so
// after all of them S' = S T + M with T = prod(1-a_i) and M the sprites blended over black. Pass 1
// accumulates M in the target's colour (the original blend, colour only); pass 2 accumulates
// D = 1-T in its alpha (blend ONE/INV_SRC_ALPHA, alpha only: D' = a + D(1-a)); the composite
// draws the target over the EFB with ONE/INV_SRC_ALPHA: S' = M + S(1-D). The EFB alpha is left
// as it was (the original also blended the sprites' alpha into it).
static void drawCloudShadowLowres(PcFnRef drawSprites, void* buf, u32 w, u32 h, view_port_class* vp) {
    f32 viewport[6];
    GXGetViewportv(viewport);
    u32 scLeft, scTop, scWidth, scHeight;
    GXGetScissor(&scLeft, &scTop, &scWidth, &scHeight);
    GXCreateFrameBuffer(w, h);
    GXSetViewport(0.0f, 0.0f, (f32)w, (f32)h, vp->mNearZ, vp->mFarZ);
    GXSetScissor(0, 0, w, h);

    // Pass 1: colour, the original blend (state set by drawCloudShadow).
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    drawSprites();

    // Pass 2: coverage D in alpha; fog only changes colour.
    GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INV_SRC_ALPHA, GX_LO_SET);
    GXSetColorUpdate(GX_FALSE);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetFog(GX_FOG_NONE, 0.0f, 1.0f, 0.1f, 1.0f, g_clearColor);
    GXSetFogRangeAdj(GX_FALSE, 0, NULL);
    drawSprites();

    GXSetTexCopySrc(0, 0, w, h);
    GXSetTexCopyDst(w, h, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(buf, GX_FALSE);
    GXRestoreFrameBuffer();
    GXSetViewport(viewport[0], viewport[1], viewport[2], viewport[3], viewport[4], viewport[5]);
    GXSetScissor(scLeft, scTop, scWidth, scHeight);

    // Composite: S' = M + S(1-D), colour only.
    pcMistDrawFullscreen(buf, GX_TRUE, w, h);

    // Leave the state as the original drawing did.
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRC_ALPHA, GX_BL_INV_SRC_ALPHA, GX_LO_SET);
    GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_GREATER, 0);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    dKy_GxFog_set();
}

void pc_kyr_draw_mist(PcFnRef drawSprites, PcFnRef setupState) {
    view_port_class* pcViewport = dComIfGp_getCurrentViewport();
    unsigned int pcMistW = 0, pcMistH = 0;
    void* pcMistBuf = NULL;
    // The sprites have no depth test, so drawn alone into a smaller target and composited they
    // give the same image, blurred by the upscale only. Not while something after the filter
    // list reads the EFB alpha this pass would no longer write (spot lights, motion blur), nor
    // with a viewport other than the whole EFB.
    if (pc_mist_lowres() > 1 && dComIfGd_getSpotModelNum() == 0 && !mDoGph_gInf_c::isBlure() &&
        pcViewport != NULL && pcViewport->mXOrig == 0.0f && pcViewport->mYOrig == 0.0f &&
        pcViewport->mWidth == 640.0f && pcViewport->mHeight == 480.0f &&
        (pcMistBuf = pc_mist_lowres_target(&pcMistW, &pcMistH)) != NULL) {
        if (pc_mist_ab_frame()) {
            // COS_MIST_AB (pc_gpu_opts.h): both ways from the same scene, each copied for display
            // in the next two frames, the original last so this frame goes on as usual.
            pcMistCopyEfb(pc_mist_ab_buffer(0));
            drawCloudShadowLowres(drawSprites, pcMistBuf, pcMistW, pcMistH, pcViewport);
            pcMistCopyEfb(pc_mist_ab_buffer(2));
            pcMistDrawFullscreen(pc_mist_ab_buffer(0), GX_FALSE);
            dKy_GxFog_set();
            GXSetBlendMode(GX_BM_BLEND, GX_BL_SRC_ALPHA, GX_BL_INV_SRC_ALPHA, GX_LO_SET);
            GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_GREATER, 0);
            GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
            GXSetColorUpdate(GX_TRUE);
            GXSetAlphaUpdate(GX_TRUE);
            setupState();
            drawSprites();
            pcMistCopyEfb(pc_mist_ab_buffer(1));
        } else {
            drawCloudShadowLowres(drawSprites, pcMistBuf, pcMistW, pcMistH, pcViewport);
        }
    } else {
        drawSprites();
    }
}

// ---- mDoGph_Painter (m_Do_graphic.cpp) --------------------------------------------------------

// COS_SKY_LOWRES: whether a draw buffer holds any packet.
static bool pcDrawBufferUsed(J3DDrawBuffer* buffer) {
    if (buffer == NULL) {
        return false;
    }
    for (u32 i = 0; i < buffer->getEntryTableSize(); i++) {
        if (buffer->getEntryPacket((u16)i) != NULL) {
            return true;
        }
    }
    return false;
}

bool pc_gph_sky_lowres_begin(camera_process_class* camera, view_port_class* viewport_p) {
    // COS_SKY_LOWRES (pc_gpu_opts.h): the sky lists into a smaller target, stretched back.
    const bool pcSkyLowres = pc_sky_lowres_begin(
        viewport_p->mNearZ, viewport_p->mFarZ,
        pcDrawBufferUsed(g_dComIfG_gameInfo.drawlist.mpOpaListSky) ||
            pcDrawBufferUsed(g_dComIfG_gameInfo.drawlist.mpXluListSky),
        g_dComIfG_gameInfo.drawlist.mpCopy2D == &g_dComIfG_gameInfo.drawlist.mpCopy2DArr[0]);
    if (pcSkyLowres) {
        GXSetProjection(camera->view.mProjMtx, GX_PERSPECTIVE);
        j3dSys.reinitGX();
        J3DShape::resetVcdVatCache();
        dKy_setLight();
    }
    return pcSkyLowres;
}

void pc_gph_sky_lowres_end(camera_process_class* camera) {
    pc_sky_lowres_end();
    if (pc_sky_ab_frame()) {
        // COS_SKY_AB: keep this result, redraw the sky the original way from the same
        // cleared EFB, keep that too (shown in the next two frames).
        pc_ab_copy_efb(pc_mist_ab_buffer(2));
        pc_sky_ab_restore();
        GXSetProjection(camera->view.mProjMtx, GX_PERSPECTIVE);
        j3dSys.reinitGX();
        J3DShape::resetVcdVatCache();
        dKy_setLight();
        dComIfGd_drawOpaListSky();
        dComIfGd_drawXluListSky();
        pc_ab_copy_efb(pc_mist_ab_buffer(1));
    }
    j3dSys.reinitGX();
    J3DShape::resetVcdVatCache();
    GXSetProjection(camera->view.mProjMtx, GX_PERSPECTIVE);
    dKy_setLight();
}

void pc_gph_mist_ab_show() {
    // COS_MIST_AB (pc_gpu_opts.h): show the A/B frame's copies over everything.
    if (void* abBuf = pc_mist_ab_show()) {
        GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
        GXSetScissor(0, 0, 640, 480);
        pcMistDrawFullscreen(abBuf, GX_FALSE, 640, 480);
    }
}

// ---- dDlst_shadowControl_c::imageDraw (d_drawlist.cpp) ----------------------------------------

// COS_SHADOW_OFFSCREEN=1 (pc_gpu_opts.h): draw the casters into an offscreen target the size
// of the EFB region they would use (256x256 logical, at the internal resolution) instead of
// the EFB's corner, so the main EFB pass is neither broken nor partially cleared per shadow.
// In an offscreen target logical coordinates are its pixels: viewport, scissor and copy
// source span it, and the copy keeps the size Aurora gives the 128x128 copy from the EFB, so
// the I4 textures (and the shadows drawn with them) are the same.
void pc_shadow_image_offscreen_open() {
    // COS_SHADOW_OFFSCREEN=gc: the GameCube's 256x256 and 128x128, whatever the resolution.
    unsigned int w = 256, h = 256, dstW = 128, dstH = 128;
    if (pc_shadow_offscreen() == PC_SHADOW_OFFSCREEN_SAME) {
        pc_efb_pixel_size(256, 256, &w, &h);
        pc_efb_pixel_size(128, 128, &dstW, &dstH);
    }
    GXCreateFrameBuffer(w, h);
    GXSetViewport(0.0f, 0.0f, (f32)w, (f32)h, 0.0f, 1.0f);
    GXSetScissor(0, 0, w, h);
    GXSetTexCopySrc(0, 0, w, h);
    GXSetTexCopyDst(dstW, dstH, GX_TF_I4, GX_TRUE);
    pc_shadow_offscreen_opened(w, h, dstW, dstH);
}
