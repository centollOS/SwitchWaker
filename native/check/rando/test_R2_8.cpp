// cos_rando_test_R2_8 (docs/RANDOMIZER_PLAN.md step R2.8): code locations C without a game.
//   - every row of the site table (features/rando/pc_rando_code_c.h) names a Code key of its location
//     in R0.5's key table: same file (REL path or CustomSymbol), same REL offset, same vanilla item,
//     and for a REL byte the disc byte is that item (KEY_FP_ITEM);
//   - every Code key of the 27 locations these actors hold is covered by a row, and the table covers
//     exactly those 27 locations;
//   - pc_rando_code_c_item: vanilla while off, with no placement (forced) and for an index that is not
//     a location; the placement's item for every row with the R0.6 rotation placement.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_code_c.h"
#include "pc_rando_lockeys.h"
#include "pc_rando_seedfile.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace {

const char* const kFiles[] = {
    "rels/d_a_auction.rel", "rels/d_a_npc_kg1.rel", "rels/d_a_npc_kg2.rel", "rels/d_a_npc_bmcon1.rel",
    "rels/d_a_npc_roten.rel", "rels/d_a_obj_toripost.rel", "rels/d_a_obj_ftree.rel", "rels/d_a_npc_rsh1.rel",
    "zunari_magic_armor_slot_item_id", "doc_bandam_green_potion_slot_item_id",
    "doc_bandam_blue_potion_slot_item_id",
};

bool ownedFile(const char* f) {
    for (const char* k : kFiles) {
        if (f != nullptr && strcmp(f, k) == 0) {
            return true;
        }
    }
    return false;
}

void testTableAgainstKeys() {
    std::set<int> locs;
    for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
        RT_CHECK(s.site < PC_RANDO_CODE_C_SITE_COUNT);
        locs.insert(s.loc);
        bool found = false;
        for (const pcr::LocKey& k : pcr::location_keys(s.loc)) {
            if (k.kind != pcr::KeyKind::Code || k.file == nullptr || strcmp(k.file, s.file) != 0 ||
                k.offset != s.offset) {
                continue;
            }
            found = true;
            RT_CHECK_EQ(k.vanilla, s.vanilla);
            if (k.src == pcr::CodeSource::Rel) {
                RT_CHECK((k.flags & pcr::KEY_FP_ITEM) != 0);
                RT_CHECK_EQ(k.disc_byte, s.vanilla);
            } else {
                RT_CHECK(k.src == pcr::CodeSource::CustomSymbol);
            }
        }
        if (!found) {
            fprintf(stderr, "no key for %s[%d] (loc %d, %s@%X)\n", pcr::code_c_site_name(s.site), s.index, s.loc,
                    s.file, s.offset);
        }
        RT_CHECK(found);
        RT_CHECK(pcr::code_c_find(s.site, s.index) == &s); // (site, index) unique
    }
    RT_CHECK_EQ((int)locs.size(), 27);

    // Every Code key in these files is covered by a row, and these files hold all of the rows' keys.
    int keys = 0;
    std::set<int> keyLocs;
    for (const pcr::LocKey& k : pcr::location_keys()) {
        if (k.kind != pcr::KeyKind::Code || !ownedFile(k.file)) {
            continue;
        }
        keys++;
        keyLocs.insert(k.loc);
        bool covered = false;
        for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
            covered = covered || (s.loc == k.loc && strcmp(s.file, k.file) == 0 && s.offset == k.offset);
        }
        if (!covered) {
            fprintf(stderr, "key %s@%X (loc %d) has no row\n", k.file, k.offset, k.loc);
        }
        RT_CHECK(covered);
    }
    RT_CHECK_EQ(keys, 31); // 4 + 3 + 2 + 1 + 1 + 13 + 2 + 3 + 2
    RT_CHECK(keyLocs == locs);
    // The locations of these sites have no key outside them (no chest or event part to miss).
    for (int loc : locs) {
        for (const pcr::LocKey& k : pcr::location_keys(loc)) {
            RT_CHECK(k.kind == pcr::KeyKind::Code && ownedFile(k.file));
        }
    }
}

void testLookups() {
    unsetenv("COS_RANDO_FORCE");
    RT_CHECK_EQ(pc_rando(), 0);
    for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
        RT_CHECK_EQ(pc_rando_code_c_item(s.site, s.index, s.vanilla), s.vanilla);
    }

    PcRandoSeed seed;
    std::string err;
    const bool loaded = pc_rando_seed_load_file(rt::fixturePath("R0_6_rotation.json").c_str(), &seed, &err);
    if (!loaded) {
        fprintf(stderr, "rotation: %s\n", err.c_str());
    }
    RT_CHECK(loaded);
    if (!loaded) {
        return;
    }
    // Active without a placement: still vanilla.
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_MAILBOX, 3, 0x04), 0x04);

    pc_rando_set_loc_items(seed.locations, PC_RANDO_SEED_LOCATIONS);
    pc_rando_set_active(nullptr, nullptr);
    int differ = 0;
    for (const pcr::CodeCSite& s : pcr::code_c_sites()) {
        const uint8_t placed = seed.locations[s.loc];
        RT_CHECK_EQ(pc_rando_code_c_item(s.site, s.index, s.vanilla), placed);
        differ += placed != s.vanilla;
    }
    RT_CHECK_EQ(differ, (int)pcr::code_c_sites().size); // rotation moves every item
    // Indexes that are not locations keep the vanilla value.
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_BATTLESQUID, 2, 0x05), 0x05);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_BATTLESQUID_RECORD, 1, 0x08), 0x08);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_BARREL_SHOOTING, 3, 0x06), 0x06);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_MAILBOX, 13, 0x01), 0x01);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_DOC_BANDAM, 0x51, 0x51), 0x51);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_SITE_COUNT, 0, 0x33), 0x33);
    RT_CHECK_EQ(pc_rando_code_c_item(-1, 0, 0x33), 0x33);
    // The two Baito's Mother rows and the Barrel Shooting registers 0/1 share their location.
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_MAILBOX, 0, 0x07),
                pc_rando_code_c_item(PC_RANDO_CODE_C_MAILBOX, 8, 0x07));
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_BARREL_SHOOTING, 0, 0x07),
                pc_rando_code_c_item(PC_RANDO_CODE_C_BARREL_SHOOTING, 1, 0x07));

    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK_EQ(pc_rando_code_c_item(PC_RANDO_CODE_C_AUCTION, 0, 0x1F), 0x1F);
}

} // namespace

int main() {
    testTableAgainstKeys();
    testLookups();
    return rt::finish();
}
