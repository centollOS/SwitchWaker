/*
 * pc_rando_code_c.h - code locations C (docs/RANDOMIZER_PLAN.md, step R2.8).
 *
 * The 27 item locations whose item id wwrando writes into a REL byte (or one of its own asm data
 * bytes, a CustomSymbol) of these actors: the Windfall auction, Battlesquid, Barrel Shooting, the
 * Bird-Man Contest, the Goron trader, the mailbox letters, the Withered Trees, Zunari and Doc Bandam.
 * The port has no REL bytes to patch; the decompiled site calls pc_rando_code_c_item (pc_rando_hooks.h)
 * with the site, the index the vanilla code uses into its item table and the vanilla value, and gets
 * the placed item.
 *
 * One table row per (site, index). A row names the location (PCR_LOC_*), the vanilla item the
 * decompiled table holds there, and the wwrando path the location key (R0.5, pc_rando_lockeys.h) was
 * made from: rels/<x>.rel and the REL offset, or a CustomSymbol name. cos_rando_test_R2_8 checks every
 * row against the key table, so a row that points at the wrong location fails there.
 *
 * Pure: no game headers (cos_rando_core).
 */
#ifndef PC_RANDO_CODE_C_H
#define PC_RANDO_CODE_C_H

#include <stdint.h>

#include "pc/pc_rando_hooks.h"
#include "pc_rando_data.h"

namespace pcr {

struct CodeCSite {
    uint8_t site;        // PC_RANDO_CODE_C_*
    uint8_t index;       // index the decompiled code uses (table index, event register value, item id)
    uint16_t loc;        // PCR_LOC_*
    uint8_t vanilla;     // the decompiled table's value at that index
    const char* file;    // "rels/<x>.rel", or the CustomSymbol name
    uint32_t offset;     // REL offset of the item byte (0 for a CustomSymbol)
};

// Every row, in site order.
Span<CodeCSite> code_c_sites();
// The row of (site, index), or nullptr when that index is not randomized (e.g. Battlesquid's third
// prize, the Barrel Shooting rupee).
const CodeCSite* code_c_find(int site, int index);
// Short name of a site ("auction", "mailbox", ...), for logs; "?" when out of range.
const char* code_c_site_name(int site);

} // namespace pcr

#endif
