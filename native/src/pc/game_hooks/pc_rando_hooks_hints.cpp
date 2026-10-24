// Hint placement at runtime, game side (docs/RANDOMIZER_PLAN.md step R5.3): wwrando 1.10.0_9775811's
// HintsRandomizer._save (randomizers/hints.py) for the active seed's hints (PcRandoSeed::hints, the
// generator's R5.1 value):
//   - the hint texts (R5.2's buildHintTexts: Savage Labyrinth tablet 837, Big Octo Great Fairy
//     12015-12017, fishmen 13026 + island, Ho Ho 14001-14010, KoRL 1502 and 3443-3448) go into the
//     message overlay at every activation, word wrapped with the message font and each message's
//     own BMG text box type (read through the game's fopMsgM_msgGet_c);
//   - rotate_hoho_to_face_hint: an archive patcher turns the "Ah" actors of the Ho Ho sea rooms'
//     room.dzr (pc_rando_hint_place.h);
//   - patcher.apply_patch("flexible_hint_locations"): the d_a_bigelf hunk asks
//     pc_rando_hints_flexible().
// Order against R3.21 (wwrando's apply_necessary_tweaks writes KoRL's 3443 before the randomizers
// save, so the KoRL hints win): the texts are built by this file's archive patcher, at the first
// archive load after each activation (the stage load that follows it), never by the activation
// listener. R3.21 builds at activation and again at an archive load while the font or the message
// archive was missing (a debug boot); its patcher is listed before this one in pc_rando_patchers.def,
// so in that load it runs first and this file's texts come last.
// Declared in native/include/pc/pc_rando_hooks.h (section R5.3).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_gen_hints.h"
#include "features/rando/pc_rando_hint_place.h"
#include "features/rando/pc_rando_hint_text.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_wrap.h"

#include "d/d_com_inf_game.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "f_op/f_op_msg_mng.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// The active seed's hints, parsed once per activation.
const PcRandoSeed* sParsedFor = nullptr;
bool sParsed = false;
bool sHaveHints = false;
pcr::HintsResult sHints;
// The texts were put in the overlay for this activation.
bool sBuilt = false;
// Per sea room (0..49): the Ho Ho patcher's last result since the activation.
struct RoomState {
    int status = PCR_HINTS_HOHO_NOT_RUN;
    pcr::HohoPatchResult r;
};
RoomState sRooms[50];

void reset() {
    sParsedFor = nullptr;
    sParsed = false;
    sHaveHints = false;
    sHints = pcr::HintsResult();
    sBuilt = false;
    for (RoomState& r : sRooms) {
        r = RoomState();
    }
}

const pcr::HintsResult* activeHints() {
    if (!pc_rando()) {
        return nullptr;
    }
    const PcRandoSeed* seed = pc_rando_seed();
    if (!sParsed || sParsedFor != seed) {
        // R7.4d audit: sHints lasts for the activation, but the first call can come from an archive
        // patcher (the current JKRHeap is the archive's, which a stage change frees) or d_a_bigelf.
        JKRPcHostAllocScope hostAlloc;
        sParsed = true;
        sParsedFor = seed;
        sHaveHints = false;
        sHints = pcr::HintsResult();
        if (seed == nullptr || !seed->has_hints || seed->hints.isNull()) {
            std::fprintf(stderr, "[cos] rando: hints: %s, hint messages stay vanilla\n",
                         seed == nullptr ? "no seed" : "the seed has no hints");
        } else {
            std::string err;
            sHaveHints = pcr::hintsFromJson(seed->hints, sHints, &err);
            if (!sHaveHints) {
                std::fprintf(stderr, "[cos] rando: hints: the seed's hints are malformed (%s), left vanilla\n",
                             err.c_str());
            }
        }
    }
    return sHaveHints ? &sHints : nullptr;
}

// The BMG's INF1 text box type of msg_no, as the talk code reads the entry (the hint messages only
// get overlay text, so the entry is the BMG's own).
bool boxType(void*, uint16_t msg_no, uint8_t* type) {
    if (dComIfGp_getMsgDtArchive() == nullptr) {
        return false;
    }
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    if (get.getMessage(head) == nullptr || get.getMesgNumber() != msg_no) {
        return false;
    }
    *type = get.getMesgEntry(head).mTextboxType;
    return true;
}

void tryBuild() {
    if (!sBuilt && activeHints() != nullptr && pc_rando_font_widths() != nullptr &&
        dComIfGp_getMsgDtArchive() != nullptr) {
        pc_rando_hints_build();
    }
}

// The texts are not built here but by the patcher, at the next archive load (the stage load that
// follows every activation): R3.21's retry patcher may rewrite 3443 at that load, and this file's
// patcher runs after it.
void onRando(int) {
    reset();
}

// Runs at every archive load while pc_rando() is 1: builds the texts once the font and the message
// archive are there (a debug boot activates before the logo scene loads them), and turns the Ho Hos.
void patcher(PcRandoResFile* file) {
    tryBuild();
    if (file->type != PCR_RES_DZR || std::strcmp(file->stage, "sea") != 0 || file->room < 0 || file->room >= 50 ||
        pcr::hohoIndexOfRoom(file->room) < 0) {
        return;
    }
    RoomState& st = sRooms[file->room];
    const pcr::HintsResult* h = activeHints();
    PcRandoDzx dzx;
    if (h == nullptr || pc_rando_dzx_open(&dzx, file->data, file->size) != PCR_DZX_OK) {
        st.status = PCR_HINTS_HOHO_VANILLA;
        st.r = pcr::HohoPatchResult();
        return;
    }
    const bool turned = pcr::patchHohoRoom(&dzx, file->room, *h, &st.r);
    st.status = turned ? PCR_HINTS_HOHO_TURNED : PCR_HINTS_HOHO_VANILLA;
    if (turned || st.r.mismatch > 0) {
        std::fprintf(stderr, "[cos] rando: hints: sea room %d: %d of %d Ho Ho actor(s) turned to y_rot 0x%04X%s\n",
                     file->room, st.r.turned, st.r.found, (unsigned)st.r.yRot,
                     st.r.mismatch > 0 ? " (some with another which_hoho left alone)" : "");
    }
}

} // namespace

extern "C" int pc_rando_hints_build(void) {
    const pcr::HintsResult* h = activeHints();
    if (h == nullptr) {
        return -1;
    }
    const PcRandoSeed* seed = pc_rando_seed();
    const pcr::HintSeedOptions o = pcr::hintSeedOptions(seed->options);
    pcr::HintTextSource src;
    src.widths = pc_rando_font_widths();
    src.cryptic = o.cryptic;
    src.importance = o.importance;
    src.instant_text = pc_rando_cfg()->instant_text_boxes != 0;
    src.floor30_valid = pcr::savageHintValid(h->floor30, o.keylunacy);
    src.floor50_valid = pcr::savageHintValid(h->floor50, o.keylunacy);
    src.box_type = boxType;
    std::vector<pcr::HintText> texts;
    std::vector<uint16_t> skipped;
    std::string err;
    const bool ok = pcr::buildHintTexts(*h, src, &texts, &skipped, &err);
    const int set = pcr::applyHintTexts(texts);
    sBuilt = src.widths != nullptr && dComIfGp_getMsgDtArchive() != nullptr;
    if (!ok) {
        std::fprintf(stderr, "[cos] rando: hints: %s (%d messages set before it)\n", err.c_str(), set);
    } else if (!skipped.empty() || set != (int)texts.size()) {
        std::fprintf(stderr, "[cos] rando: hints: %d messages set, %d skipped (first %u)\n", set, (int)skipped.size(),
                     skipped.empty() ? 0u : (unsigned)skipped[0]);
    } else {
        std::fprintf(stderr, "[cos] rando: hints: %d messages (octo fairy %s)\n", set, h->hasOctoFairy ? "yes" : "no");
    }
    return set;
}

extern "C" int pc_rando_hints_flexible(void) {
    const pcr::HintsResult* h = activeHints();
    return h != nullptr && h->hasOctoFairy ? 1 : 0;
}

extern "C" int pc_rando_hints_hoho_room(int room, int* found, int* turned, uint16_t* yRot) {
    if (room < 0 || room >= 50) {
        return PCR_HINTS_HOHO_NOT_RUN;
    }
    const RoomState& st = sRooms[room];
    if (found != nullptr) {
        *found = st.r.found;
    }
    if (turned != nullptr) {
        *turned = st.r.turned;
    }
    if (yRot != nullptr) {
        *yRot = st.r.yRot;
    }
    return st.status;
}

// pc_rando_patchers.def (R0.10's archive-load hook calls it before the first dispatch).
extern "C" void pc_rando_hints_hooks_init(void) {
    pc_rando_register_res_patcher(patcher, PCR_RES_ANY);
    pc_rando_add_listener(onRando);
}
