// "Completar la Galería Nintendo" (native/include/pc/pc_figurines.h, issue #44): the figurine cheat of
// SwitchWakerHD (Settings > Mods > Cheats there) redone on the decompiled game.
//
// The figurines are bits in 17 event registers (d_a_npc_mt.cpp's l_figure_comp: register figure / 8,
// bit figure % 8; the last register holds 6 of the 134). The gallery's figures (daObjFigure_c) and
// Carlov's count (daNpcMt_c::isComp: 133 or more) read the same bits. A complete collection makes Carlov
// leave the next time his workshop loads (event 0x3D08, the game's own ending), so the membership he
// gives with the first figurine (event 0x3401: he took a picto to make one) could never be had
// afterwards: without it, nothing is set.
#include "pc/pc_figurines.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"

#include "pc_internal.h"

#include <unistd.h>

namespace {

constexpr int kFigures = 0x86;

// d_a_npc_mt.cpp's l_figure_comp (static there; d_a_obj_figure.cpp has the same table).
const u16 kFigureRegs[(kFigures + 7) / 8] = {
    dSv_event_flag_c::UNK_95FF, dSv_event_flag_c::UNK_94FF, dSv_event_flag_c::UNK_93FF,
    dSv_event_flag_c::UNK_92FF, dSv_event_flag_c::UNK_91FF, dSv_event_flag_c::UNK_90FF,
    dSv_event_flag_c::UNK_8FFF, dSv_event_flag_c::UNK_8EFF, dSv_event_flag_c::UNK_8DFF,
    dSv_event_flag_c::UNK_8CFF, dSv_event_flag_c::UNK_B1FF, dSv_event_flag_c::UNK_9CFF,
    dSv_event_flag_c::UNK_84FF, dSv_event_flag_c::UNK_83FF, dSv_event_flag_c::UNK_82FF,
    dSv_event_flag_c::UNK_81FF, dSv_event_flag_c::UNK_80FF,
};

bool made(int figure) {
    return (dComIfGs_getEventReg(kFigureRegs[figure / 8]) & (1 << (figure % 8))) != 0;
}

} // namespace

int pc_figurines_made(void) {
    int num = 0;
    for (int i = 0; i < kFigures; i++) {
        num += made(i) ? 1 : 0;
    }
    return num;
}

PcFigurinesResult pc_figurines_complete(void) {
    if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_3401)) {
        pc::writef(STDERR_FILENO, "[cos] figurines: not a gallery member yet (event 3401), nothing set\n");
        return PC_FIGURINES_NOT_MEMBER;
    }
    const int before = pc_figurines_made();
    if (before == kFigures) {
        return PC_FIGURINES_ALREADY;
    }
    for (int i = 0; i < kFigures; i++) {
        const u16 reg = kFigureRegs[i / 8];
        dComIfGs_setEventReg(reg, dComIfGs_getEventReg(reg) | (u8)(1 << (i % 8)));
    }
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_3A01); // setFigure's "a figurine was made"
    pc::writef(STDERR_FILENO, "[cos] figurines: %d -> %d\n", before, pc_figurines_made());
    return PC_FIGURINES_DONE;
}
