// Required bosses at runtime, game side (docs/RANDOMIZER_PLAN.md step R6.9; the pure part is
// features/rando/pc_rando_bossreq.{h,cpp}). Declared in native/include/pc/pc_rando_hooks.h (section
// R6.9).
//   - pc_rando_bossreq_hooks_init (pc_rando_patchers.def): registers the GanonL/Room0 patcher and a
//     patcher that puts message 851 in the overlay (the message font's widths are game data) at the
//     first archive load after an activation of a seed with required bosses;
//   - pc_rando_bossreq_fmap_markers: the dMenu_Fmap_c::_create hunk (the sea chart's quest markers).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_bossreq.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_wrap.h"

#include "JSystem/J2DGraph/J2DScreen.h"

#include <cstdio>
#include <string>

namespace {

bool wanted() {
    return pc_rando() && pc_rando_bossreq_bosses_of(pc_rando_seed(), nullptr) > 0;
}

// Runs at every archive load while pc_rando() is 1; the overlay is cleared at deactivation, so the
// text comes back at the first load after the next activation (a debug boot activates before the
// font is there: the next loads retry).
void messagePatcher(PcRandoResFile*) {
    if (wanted() && pc_rando_msg_find(PCR_BR_MSG) == nullptr) {
        pc_rando_bossreq_build_message();
    }
}

} // namespace

extern "C" int pc_rando_bossreq_build_message(void) {
    const pcr::GlyphWidths* widths = pc_rando_font_widths();
    if (widths == nullptr) {
        return 0;
    }
    PcRandoMsgEntry entry;
    std::string text;
    pc_rando_bossreq_message(*widths, &entry, &text);
    const int r = pc_rando_msg_add(PCR_BR_MSG, &entry, text.c_str());
    std::fprintf(stderr, "[cos] rando: required bosses: message %d %s (%d)\n", PCR_BR_MSG,
                 r == PCR_MSG_OK ? "set" : "not set", r);
    return r == PCR_MSG_OK;
}

extern "C" int pc_rando_bossreq_fmap_markers(void* screen) {
    J2DScreen* scrn = static_cast<J2DScreen*>(screen);
    PcRandoBossMarker markers[PCR_BR_MAX_BOSSES];
    const int n = scrn != nullptr ? pc_rando_bossreq_markers(pc_rando_seed(), markers) : 0;
    int set = 0;
    for (int i = 0; i < n; i++) {
        const PcRandoBossMarker& m = markers[i];
        J2DPane* pane = scrn->search(pc_rando_bossreq_marker_tag(m.index));
        if (pane == nullptr) {
            std::fprintf(stderr, "[cos] rando: required bosses: no quest marker pane %d in f_map.blo\n", m.index);
            continue;
        }
        // The PIC1 bytes wwrando writes, as J2DPane::makePaneStream reads them: visible (+9), the
        // bounds' top-left (+0x10/+0x12; width and height kept) and the rotation (+0x18 u16).
        pane->show();
        pane->move((f32)m.x, (f32)m.y);
        pane->rotate((f32)m.rotation);
        set++;
    }
    return set;
}

// pc_rando_patchers.def (R0.10's archive-load hook calls it before the first dispatch).
extern "C" void pc_rando_bossreq_hooks_init(void) {
    pc_rando_bossreq_init();
    pc_rando_register_res_patcher(messagePatcher, PCR_RES_ANY);
}
