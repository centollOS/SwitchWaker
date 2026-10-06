/*
 * game_hooks.h - the larger host changes of game/, moved out of the decompiled files (step G3 of
 * docs/GAME_CODE_ORGANIZATION.md). game/ keeps a one-line call under TARGET_PC, the original code
 * in the #else; the bodies are in native/src/pc/game_hooks/ (one file per area, named in each
 * section below). Header-level replacements (declarations, inline hot-path math) are separate
 * headers in native/include/pc/game_hooks/, included by the game header they replace.
 *
 * C++ only, compiled like the game units (cos_pc links cos_game_headers); the game types are
 * forward-declared so a game file includes this header wherever its own includes end.
 */
#ifndef PC_GAME_HOOKS_H
#define PC_GAME_HOOKS_H

/*
 * A callable handed to a hook, which calls it back: e.g. a lambda that draws with the game
 * function's locals. Holds a reference: the callable must outlive the hook call.
 */
class PcFnRef {
public:
    template <typename F>
    PcFnRef(F& f) : mCall(&callImpl<F>), mObj(&f) {}
    void operator()() const { mCall(mObj); }

private:
    template <typename F>
    static void callImpl(void* obj) { (*static_cast<F*>(obj))(); }
    void (*mCall)(void*);
    void* mObj;
};

/* ---- GPU options (pc_gpu_opts.h), native/src/pc/game_hooks/pc_gpu_hooks.cpp ---------------- */

class camera_process_class;
struct view_port_class;

/* drawCloudShadow (d_kankyo_rain.cpp): draws the forest mist sprites, at a reduced resolution
   with COS_MIST_LOWRES and both ways with COS_MIST_AB, or as the GameCube does. drawSprites draws
   them with the current GX state; setupState sets the state drawCloudShadow set before them. */
void pc_kyr_draw_mist(PcFnRef drawSprites, PcFnRef setupState);

/* mDoGph_Painter (m_Do_graphic.cpp), around the sky lists: COS_SKY_LOWRES draws them into a
   smaller target (begin returns whether it did), end stretches the result back and, on a
   COS_SKY_AB frame, redraws the sky the original way for the comparison copies. */
bool pc_gph_sky_lowres_begin(camera_process_class* camera, view_port_class* viewport);
void pc_gph_sky_lowres_end(camera_process_class* camera);

/* mDoGph_Painter, after the 2D: COS_MIST_AB shows the A/B frame's copies over everything. */
void pc_gph_mist_ab_show();

/* dDlst_shadowControl_c::imageDraw (d_drawlist.cpp), COS_SHADOW_OFFSCREEN: opens the offscreen
   target the casters are drawn into, in place of the EFB's corner (closed by GXRestoreFrameBuffer). */
void pc_shadow_image_offscreen_open();

#endif /* PC_GAME_HOOKS_H */
