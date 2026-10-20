// The randomizer's cutscene-removal hooks (docs/RANDOMIZER_PLAN.md step R3.11, wwrando
// remove_cutscenes.asm and two flexible_item_locations.asm functions). Declared in
// native/include/pc/pc_rando_hooks.h; called only under pc_rando() from the hunks listed in
// docs/RANDO_HOOKS.md, section R3.11.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "JAZelAudio/JAZelAudio_BGM.h"

// remove_cutscenes.asm check_hyrule_warp_unlocked: getTriforceNum() >= 8.
int pc_rando_hyrule_warp_unlocked(void) {
    return dComIfGs_getTriforceNum() >= 8;
}

// remove_cutscenes.asm give_pearl_and_raise_totg_if_necessary, after its onSymbol: the event bits the
// statue's pearl-placing events would set, and the tower once the pearl bitfield is 7.
void pc_rando_place_pearl(int symbol) {
    switch (symbol) {
    case dSymbol_NAYRU_e:
        dComIfGs_onEventBit(0x1410); // placed Nayru's Pearl
        break;
    case dSymbol_DIN_e:
        dComIfGs_onEventBit(0x1480); // placed Din's Pearl
        break;
    case dSymbol_FARORE_e:
        dComIfGs_onEventBit(0x1440); // placed Farore's Pearl
        break;
    default:
        break;
    }
    if (dComIfGs_isSymbol(dSymbol_NAYRU_e) && dComIfGs_isSymbol(dSymbol_DIN_e) && dComIfGs_isSymbol(dSymbol_FARORE_e)) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_1E40); // TOWER_OF_THE_GODS_RAISED
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_2E80); // PEARL_TOWER_CUTSCENE
    }
}

// flexible_item_locations.asm check_play_special_item_get_music. The original game played the pearl
// music from the pearls' .stb cutscenes and the song music from 059get_dance only.
uint32_t pc_rando_item_get_bgm(uint16_t item) {
    switch (item) {
    case dItemNo_PEARL_NAYRU_e:
    case dItemNo_PEARL_DIN_e:
    case dItemNo_PEARL_FARORE_e:
        return JA_BGM_GET_PEARL;
    case dItemNo_WINDS_REQUIEM_e:
    case dItemNo_BALLAD_OF_GALES_e:
    case dItemNo_COMMAND_MELODY_e:
    case dItemNo_EARTH_GODS_LYRIC_e:
    case dItemNo_WIND_GODS_ARIA_e:
    case dItemNo_SONG_OF_PASSING_e:
        return JA_BGM_GET_SONG;
    default:
        return JA_BGM_ITEM_GET;
    }
}
