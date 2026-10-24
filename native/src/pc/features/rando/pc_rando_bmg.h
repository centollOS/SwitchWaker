// A read-only BMG message lookup (docs/RANDOMIZER_PLAN.md step R7.6c). On the European disc the game
// loads the BMG of the chosen language (/res/Msg/data<language>/bmgres.arc), but the texts wwrando
// builds from vanilla text (R3.21: the chart item-get messages, Beedle's refusal, the Rock Spire
// letter) must start from the English one, so that every wwrando text stays English in all five
// languages (R7.6c's language rule). game_hooks/pc_rando_hooks_paltext.cpp reads the English
// zel_00.bmg from the disc into host memory and finds messages in it with this.
//
// Pure: no game headers, no game calls (cos_rando_core and the unit tests link it).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pcr {

// A message of a BMG file: its 0x18-byte INF1 entry (big-endian, as pc_rando_msg_entry_unpack reads
// it) and its DAT1 string, which ends with a NUL outside any control tag; `text_max` is the number of
// bytes from `text` to the end of DAT1 (the bound for pc_rando_msg_decode).
struct BmgMessage {
    const uint8_t* entry = nullptr;
    const uint8_t* text = nullptr;
    size_t text_max = 0;
};

// gclib bmg.py BMG.read / read_inf1 on a "MESGbmg1" file: walks the sections, takes the first INF1 and
// DAT1, and finds the first entry with this message number whose string offset is not 0 (as
// fopMsgM_msgGet_c::getMessage skips such entries). False when the file is malformed, a section or an
// offset runs past its end, the entry size is under 0x18, or the number is not there.
bool bmgFind(const uint8_t* bmg, size_t size, uint16_t msg_no, BmgMessage* out);

} // namespace pcr
