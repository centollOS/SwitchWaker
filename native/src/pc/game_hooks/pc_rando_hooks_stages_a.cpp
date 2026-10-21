// Stage tweaks A, game side (docs/RANDOMIZER_PLAN.md step R3.15; the dzx edits are in the pure
// features/rando/pc_rando_tweak_stages_a.cpp). This file registers the step's patchers with R0.10's
// archive-load dispatcher (pc_rando_patchers.def) and holds the one patch that needs a game type:
//
// tweaks.py fix_forsaken_fortress_door_softlock: in sea/Room1's room.dzb, face 0x1493 gets property
// index 0x11 (one more triangle with the exit set, so entering FF through the left half of the big
// door with the faster block-push animation reaches the exit). wwrando writes the u16 at
// face_list_offset + 0x1493 * 0xA + 6 on the disc. loadResource has run cBgS::ConvDzb on the file
// before the dispatcher sees it (its table offsets are OFFSET_PTRs, the triangle table stays
// big-endian), so the triangle is reached through cBgD_t's m_t_tbl. That replaces the plan's hunk at
// d_a_bg.cpp:206: dBgWSv / cBgW::Set read the same converted bytes later, so no game/ hunk is needed.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_stages_a.h"
#include "SSystem/SComponent/c_bg_w.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

// The face's vanilla property index on the USA disc (the D9 fingerprint; logged when it differs).
constexpr u16 kVanillaProperty = 0x20;

void logLine(const char* line) {
    (void)!write(STDERR_FILENO, line, strlen(line));
}

} // namespace

extern "C" void pc_rando_stages_a_patch_dzb(PcRandoResFile* f) {
    if (f == nullptr || strcmp(f->stage, "sea") != 0 || f->room != 1 || strcmp(f->name, "room.dzb") != 0) {
        return;
    }
    cBgD_t* dzb = (cBgD_t*)f->data;
    char buf[160];
    bool ok = false;
    if (f->size >= sizeof(cBgD_t) && (u32)dzb->m_t_num > PCR_SA_FF_DOOR_FACE &&
        (u32)dzb->m_ti_num > PCR_SA_FF_DOOR_PROPERTY && (u32)dzb->flag & 0x80000000u) {
        cBgD_Tri_t* tri = &((cBgD_Tri_t*)dzb->m_t_tbl)[PCR_SA_FF_DOOR_FACE];
        const u16 was = tri->id;
        if (was == kVanillaProperty) {
            tri->id = (u16)PCR_SA_FF_DOOR_PROPERTY;
            ok = true;
        }
        snprintf(buf, sizeof buf,
                 "[cos] rando: stagesA ff-door: sea/Room1/room.dzb face 0x%X property 0x%X -> 0x%X%s\n",
                 PCR_SA_FF_DOOR_FACE, was, (u16)tri->id, ok ? "" : " (not applied: not the vanilla value)");
    } else {
        snprintf(buf, sizeof buf, "[cos] rando: stagesA ff-door: sea/Room1/room.dzb has no face 0x%X (not applied)\n",
                 PCR_SA_FF_DOOR_FACE);
    }
    logLine(buf);
    pc_rando_stages_a_count(PCR_SA_FF_DOOR, ok);
}

extern "C" void pc_rando_stages_a_init(void) {
    pc_rando_register_dzx_patcher(pc_rando_stages_a_patch);
    pc_rando_register_res_patcher(pc_rando_stages_a_patch_dzb, PCR_RES_TYPE('D', 'Z', 'B', ' '));
}
