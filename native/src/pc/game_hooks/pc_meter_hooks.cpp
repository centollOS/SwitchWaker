// The widescreen helpers of d_meter.cpp (pc_aspect.h), moved out of game/ (step G3 of
// docs/GAME_CODE_ORGANIZATION.md). Declared in native/include/pc/game_hooks.h.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_aspect.h"
#include "d/d_meter.h"
#include "f_op/f_op_msg_mng.h"

// Widescreen (pc_aspect.h): a 16:9 Gecko code value interpolated for COS_ASPECT, rounded as
// scripts/mods/widescreen_aspect.py rounds it.
s16 dMeter_pcLerpS16(f32 v43, f32 v169) {
    f32 v = pc_aspect_lerp(v43, v169);
    return (s16)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

// The HUD tuning values the 16:9 code writes into g_meter_mapHIO and g_meterHIO (0x803E68E4 ..
// 0x803E69A4: minimap left and hidden position, free icon x, HUD pane x offset, map button x).
// Their constructors run before main, so before the Switch harness sets COS_ASPECT: dMeter_Create
// sets them whenever it makes the meter, where the Gecko code rewrites them every frame.
void dMeter_pcAspectHIO() {
    if (!pc_aspect_wide()) {
        return;
    }
    g_meter_mapHIO.field_0x8 = dMeter_pcLerpS16(35.0f, -79.0f);
    g_meter_mapHIO.field_0xc = dMeter_pcLerpS16(-180.0f, -256.0f);
    g_meter_mapHIO.field_0x14 = dMeter_pcLerpS16(590.0f, 704.0f);
    g_meterHIO.field_0x50 = dMeter_pcLerpS16(7.0f, 121.0f);
    g_meterHIO.field_0x9c = dMeter_pcLerpS16(0.0f, -114.0f);
}

// The 16:9 code's added code at 0x800040C0, which dMeter_Draw calls instead of fopMsgM_setAlpha
// for five panes: the pane's matrix x becomes its left + 114, then fopMsgM_setAlpha.
void dMeter_pcSetAlphaShifted(fopMsgM_pane_class* i_pane) {
    if (pc_aspect_wide()) {
        i_pane->pane->pcSetMtxTransX(i_pane->pane->getBounds().i.x + pc_aspect_hud_shift());
    }
    fopMsgM_setAlpha(i_pane);
}
