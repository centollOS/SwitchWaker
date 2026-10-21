// COS_SMOKE=rando COS_RANDO_SMOKE=codec (docs/RANDOMIZER_PLAN.md step R2.8, T1): the lookups of code
// locations C in a booted game, e.g.
//   run.sh rando --stage sea:11 --env COS_RANDO_SMOKE=codec
//   run.sh rando --stage sea:11 --env COS_RANDO_SMOKE=codec --env COS_RANDO_PLACEMENT=<abs path>/R0_6_rotation.json
// The decompiled sites (auction, Battlesquid, Barrel Shooting, Bird-Man, Goron trader, mailbox,
// Withered Trees, Zunari, Doc Bandam) pass their vanilla value to pc_rando_code_c_item and use what it
// returns. Once the play scene has run 10 frames:
//   1. the mailbox's m_letter[] (the one table the smoke can see) holds the site table's vanilla ids;
//   2. off (a plain boot): every row gives its vanilla value;
//   3. with a placement: the debug boot's (COS_RANDO_PLACEMENT) or, without one, a synthetic one that
//      the smoke sets (each of the 27 locations a different id that is not its vanilla one): every row
//      (32 rows for the 31 location keys; Barrel Shooting registers 0 and 1 share one) gives the
//      placed item, and the indexes that are no location (Battlesquid's third prize, the orange
//      rupee, Barrel Shooting register 3, Doc Bandam's other potion) keep the vanilla value.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_code_c.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_seedfile.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/actor/d_a_obj_toripost.h"

#include <cstdio>

namespace pc {

namespace {

constexpr const char* kName = "codec";
bool sDone = false;
uint8_t sItems[PC_RANDO_SEED_LOCATIONS];

bool allRows(const char* what, bool placed) {
    int bad = 0;
    for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
        const uint8_t want = placed ? pc_rando_loc_item(s.loc, 0xFE) : s.vanilla;
        const uint8_t got = pc_rando_code_c_item(s.site, s.index, s.vanilla);
        if (got != want || (placed && want == 0xFE)) {
            fprintf(stderr, "[cos] rando-smoke %s: %s[%d] (loc %d) gives 0x%02X, want 0x%02X\n", kName,
                    pcr::code_c_site_name(s.site), s.index, s.loc, got, want);
            bad++;
        }
    }
    char buf[96];
    snprintf(buf, sizeof buf, "%s: %zu rows", what, pcr::code_c_sites().size);
    return randoSmokeCheck(kName, buf, bad == 0);
}

} // namespace

void randoSmokeCodec(unsigned int frames) {
    (void)frames;
    if (sDone || !randoSmokeInPlay(10)) {
        return;
    }
    sDone = true;

    int letters = 0, mismatch = 0;
    for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
        if (s.site == PC_RANDO_CODE_C_MAILBOX) {
            letters++;
            mismatch += daObjTpost_c::m_letter[s.index].mItemNo != s.vanilla;
        }
    }
    randoSmokeCheck(kName, "m_letter[0..12] holds the rows' vanilla ids", letters == 13 && mismatch == 0);

    const bool seeded = pc_rando_seed() != nullptr;
    if (!pc_rando()) {
        allRows("off: vanilla", false);
        // a synthetic placement: the vanilla items, the 27 locations moved to other ids
        for (int i = 0; i < PC_RANDO_SEED_LOCATIONS; i++) {
            pcr::Span<pcr::LocKey> keys = pcr::location_keys(i);
            sItems[i] = keys.size > 0 ? keys[0].vanilla : 0xFF;
        }
        int n = 0;
        for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
            if (sItems[s.loc] == s.vanilla) {
                sItems[s.loc] = (uint8_t)(0x20 + n++); // 0x20.. differs from every vanilla id here
            }
        }
        pc_rando_set_loc_items(sItems, PC_RANDO_SEED_LOCATIONS);
        pc_rando_set_active(nullptr, nullptr);
    } else if (!seeded) {
        allRows("forced, no placement: vanilla", false);
        randoSmokeEnd(kName);
    }
    allRows(seeded ? "seed placement" : "synthetic placement", true);
    randoSmokeCheck(kName, "indexes that are no location keep the vanilla value",
                    pc_rando_code_c_item(PC_RANDO_CODE_C_BATTLESQUID, 2, 0x05) == 0x05 &&
                        pc_rando_code_c_item(PC_RANDO_CODE_C_BATTLESQUID_RECORD, 1, 0x08) == 0x08 &&
                        pc_rando_code_c_item(PC_RANDO_CODE_C_BARREL_SHOOTING, 3, 0x06) == 0x06 &&
                        pc_rando_code_c_item(PC_RANDO_CODE_C_DOC_BANDAM, 0x51, 0x51) == 0x51);
    if (!seeded) {
        pc_rando_deactivate();
        if (!pc_rando()) {
            allRows("deactivated: vanilla again", false);
        }
    }
    randoSmokeEnd(kName);
}

} // namespace pc
