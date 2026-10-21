// The randomizer's seed-dependent message texts, game side (docs/RANDOMIZER_PLAN.md step R3.21). The
// texts are built by the pure features/rando/pc_rando_posttext.cpp; this file hands it the BMG's
// vanilla texts (through the game's own fopMsgM_msgGet_c), the placement's item names and the
// message font's widths (R3.20), at every activation. No hunk in game/: the overlay's R0.12 hunks
// show the texts. Declared in native/include/pc/pc_rando_hooks.h (section R3.21).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_data.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_posttext.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_wrap.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_msg_mng.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

// done_item_locations[loc]: the seed's logic item name ("Progressive Sword", as wwrando writes it);
// without a seed (a debug activation with only item ids) the item id's name; with no placement at all
// (COS_RANDO_FORCE) the location's original item, which is what the location then gives.
std::string itemName(void*, int loc) {
    const PcRandoSeed* seed = pc_rando_seed();
    if (seed != nullptr && loc >= 0 && (size_t)loc < seed->location_item_names.size()) {
        return seed->location_item_names[loc];
    }
    const uint8_t id = pc_rando_loc_item(loc, 0xFF);
    if (id != 0xFF) {
        const char* name = pcr::item_name(id);
        return name != nullptr ? name : "";
    }
    const pcr::Span<pcr::Location> locs = pcr::locations();
    return loc >= 0 && (size_t)loc < locs.size ? locs[loc].original_item : "";
}

// The BMG's text of msg_no in wwrando's syntax (gclib's read_string) and its text box type. The
// caller has dropped this step's overlay records, so getMessage reads the BMG itself.
bool bmgText(void*, uint16_t msg_no, std::string* text, uint8_t* type) {
    if (dComIfGp_getMsgDtArchive() == nullptr) {
        return false; // before the logo scene loads the message archive (a COS_RANDO_FORCE static init)
    }
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    const char* bytes = get.getMessage(head);
    if (bytes == nullptr || get.getMesgNumber() != msg_no) {
        return false;
    }
    *type = get.getMesgEntry(head).mTextboxType;
    std::vector<char> buf(1024);
    for (;;) {
        // A BMG string ends with a NUL outside any tag; 0x10000 bounds a damaged one.
        const size_t n = pc_rando_msg_decode((const uint8_t*)bytes, 0x10000, buf.data(), buf.size());
        if (n < buf.size()) {
            text->assign(buf.data(), n);
            return true;
        }
        buf.resize(n + 1);
    }
}

// Set when a build ran before the logo scene handed over the message archive or the font (a debug boot
// activates in pcBootStage, before dScnLogo_Delete): the texts that need them are built again at the
// next archive load (the play scene's stage archives), by retryPatcher.
bool sPending = false;

void onRando(int active) {
    if (active) {
        pc_rando_posttext_build();
    } else {
        sPending = false;
    }
}

// An archive patcher that edits nothing: the archive-load hook (R0.10, only under pc_rando()) is the
// first point after activation where the message archive is sure to be set.
void retryPatcher(PcRandoResFile*) {
    if (sPending && pc_rando()) {
        std::fprintf(stderr, "[cos] rando: post texts: building again now that the message archive is loaded\n");
        pc_rando_posttext_build();
    }
}

} // namespace

extern "C" int pc_rando_posttext_build(void) {
    // Old records of these messages would be read back as "vanilla" text (a forced boot keeps the
    // overlay across activations).
    for (uint16_t n : pcr::postTextMessageNumbers()) {
        pc_rando_msg_remove(n);
    }
    pcr::PostTextSource src;
    src.widths = pc_rando_font_widths();
    src.post_randomization = true; // wwrando's randomize_items: every seed randomizes items
    src.swift_sail = pc_rando_cfg()->swift_sail != 0;
    src.item_name = itemName;
    src.vanilla = bmgText;
    std::vector<pcr::PostText> texts;
    std::vector<uint16_t> skipped;
    pcr::buildPostTexts(src, &texts, &skipped);
    const int set = pcr::applyPostTexts(texts);
    sPending = src.widths == nullptr || dComIfGp_getMsgDtArchive() == nullptr;
    if (!skipped.empty() || set != (int)texts.size()) {
        std::fprintf(stderr, "[cos] rando: post texts: %d set, %d skipped (first %u; font %s, BMG %s)\n", set,
                     (int)skipped.size(), skipped.empty() ? 0u : (unsigned)skipped[0],
                     src.widths != nullptr ? "yes" : "no", dComIfGp_getMsgDtArchive() != nullptr ? "yes" : "no");
    } else {
        std::fprintf(stderr, "[cos] rando: post texts: %d messages\n", set);
    }
    return set;
}

// pc_rando_patchers.def (R0.10's archive-load hook calls it before the first dispatch).
extern "C" void pc_rando_posttext_patchers_init(void) {
    pc_rando_register_res_patcher(retryPatcher, PCR_RES_ANY);
}

// Called once at static init by R0.8's table binding (game_hooks/pc_rando_hooks_items.cpp): cos_pc is
// a static library, so this file is linked only through that reference.
extern "C" void pc_rando_posttext_register(void) {
    pc_rando_add_listener(onRando);
}
