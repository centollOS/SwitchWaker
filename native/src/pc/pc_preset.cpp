// Debug story presets: COS_BOOT_PRESET=<name> on a debug boot, and the options menu's
// Depuración > "Navegar (barco, vela y batuta)" on the file being played.
//
// A preset is the part of a later story state that a test needs, put on the file in memory: event
// bits (dSv_event_flag_c), items given the way a chest gives them (execItemGet) and the items on
// the X/Y buttons, plus the stage spawn where it starts. Nothing is written to the memory card:
// the file only keeps the preset if the player saves it in the game's own save screen.
//
// sailing: the talking boat, its sail, the wind baton and the wind song, and the open sea, as after
// the second pearl of the sea story (the boat met and boarded at Windfall, the sail bought, the
// boat's sail talk with the sea chart, the arrival at Dragon Roost and the song learnt there, the
// arrival at the forest island and its pearl). Checked against the game code:
//   0x0F80  the boat met (d_a_ship create returns cPhs_ERROR_e without it; d_a_player_main
//           starts a mode 2 sea spawn on the boat only with it; dComIfGs_setGameStartStage)
//   0x2A08  the boat boarded once (d_stage dStage_chkTaura: otherwise Windfall puts the boat at
//           its pier; d_a_player_main: otherwise a hit at Outset or Windfall sends the player back
//           to the pier)
//   0x0908  the boat's sail talk done, sea chart open (d_a_ship checkForceMessage: with the sail
//           and without it the boat forces message 0x5E0; d_meter: the chart on Up)
//   0x0902  Dragon Roost arrival seen (d_a_tag_island type 1: otherwise the arrival event plays
//           when the boat comes near; dComIfGs_checkSeaLandingEvent)
//   0x0A20  the forest island's arrival seen (d_a_tag_island type 2)
//   0x0A08  the boat's talk about that pearl done (d_a_ship checkForceMessage: with the pearl and
//           without it the boat stops the player for message 0x5F6 as soon as no event runs)
//   0x2A80  the hero's clothes (d_a_player_main: casual clothes without it), also the telescope
//           from the sister on Outset's lookout (bug B6)
// Items: 0x78 the sail (slot 1, dComIfGs_isGetItem(1, 0)), 0x22 the wind baton, 0x6D the wind
// song (dComIfGs_onTact(0): wind direction), 0x20 the telescope, 0x6B the forest island's pearl
// (dSymbol_FARORE_e); the sail on X, the baton on Y.
// The open sea: daShip_c::checkOutRange turns the boat back (message 0x609) at the sea stage's
// paths: path 0 (the Windfall and Dragon Roost row) until 0x0902, path 1 (that row and the east
// column) until the forest island's pearl, path 2 (the fortress corner, north-west) until the
// master sword, path 3 (the edge of the sea) always. With this preset only the fortress corner and
// the edge stay closed.
// Start: sea room 11 (Windfall) point 102, a mode 2 spawn (on the boat) at the room's north-west
// corner, (-25000, 0, -175000), 25000 units from the boundary with room 18.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

const unsigned short kSailingEvents[] = {0x0F80, 0x2A08, 0x0908, 0x0902, 0x0A20, 0x0A08, 0x2A80};

struct PresetItem {
    unsigned char item;
    int button; // dItemBtn_*, or -1
};
const PresetItem kSailingItems[] = {
    {dItemNo_SAIL_e, dItemBtn_X_e},
    {dItemNo_WIND_WAKER_e, dItemBtn_Y_e},
    {dItemNo_WINDS_REQUIEM_e, -1},
    {dItemNo_TELESCOPE_e, -1},
    {dItemNo_PEARL_FARORE_e, -1},
};

const char* sBootPreset = nullptr; // COS_BOOT_PRESET, checked
bool sBootPresetRead = false;

// The inventory slot holding item, or -1.
int slotOf(unsigned char item) {
    for (int slot = 0; slot < dInvSlot_ItemLast_e; slot++) {
        if (dComIfGs_getItem(slot) == item) {
            return slot;
        }
    }
    return -1;
}

} // namespace

const char* bootPreset() {
    if (!sBootPresetRead) {
        sBootPresetRead = true;
        const char* v = getenv("COS_BOOT_PRESET");
        if (v != nullptr && v[0] != '\0') {
            if (strcmp(v, "sailing") != 0) {
                writef(STDERR_FILENO, "[cos] COS_BOOT_PRESET=\"%s\" is not a preset; known: sailing\n", v);
                pc_exit(PC_EXIT_USAGE);
            }
            sBootPreset = v;
        }
    }
    return sBootPreset;
}

void applySailingPreset(bool inPlay) {
    int events = 0;
    for (unsigned short bit : kSailingEvents) {
        if (!dComIfGs_isEventBit(bit)) {
            dComIfGs_onEventBit(bit);
            events++;
        }
    }
    int items = 0;
    for (const PresetItem& it : kSailingItems) {
        // dComIfGs_checkGetItem knows the songs (dComIfGs_isTact) and pearls (symbols) too.
        if (!dComIfGs_checkGetItem(it.item)) {
            execItemGet(it.item);
            items++;
        }
        if (it.button >= 0) {
            const int slot = slotOf(it.item);
            if (slot >= 0) {
                dComIfGs_setSelectItem(it.button, (u8)slot);
                if (inPlay) {
                    dComIfGp_setSelectItem(it.button);
                }
            }
        }
    }
    writef(STDERR_FILENO, "[cos] preset sailing: %d event bits and %d items added (sail on X, wind "
                          "baton on Y); the file keeps them only if saved in the game\n",
           events, items);
}

} // namespace pc

using namespace pc;

extern "C" {

void pc_boot_preset_apply(void) {
    if (bootPreset() != nullptr) {
        applySailingPreset(false);
    }
}

} // extern "C"
