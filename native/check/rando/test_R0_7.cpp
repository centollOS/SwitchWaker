// cos_rando_test_R0_7 (docs/RANDOMIZER_PLAN.md step R0.7): the pure part of the slot binding
// (features/rando/pc_rando_slot.h), without a game:
//   - the mReserve record: layout (magic "WWR1", version, flags, hash at +8, zero padding), read
//     back, wrong magic/version refused, the D4 stale-record clear touching only rando records;
//   - sidecar names from a hash equal the seed file's;
//   - the seed -> config mapping: wwrando's default options give pc_rando_config_defaults exactly;
//     sword modes, hero mode, casual clothes, health, shards, tweaks, targeting, modes and extra
//     starting items; an unknown starting item is refused;
//   - the pending seed; activation (pc_rando(), the seed, the placement, the config) and activation
//     from a directory (missing file, another hash, success), and sidecar writing.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_modes.h"
#include "pc_rando_slot.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

bool loadFixture(const char* name, PcRandoSeed* seed) {
    std::string err;
    const bool ok = pc_rando_seed_load_file(rt::fixturePath(name).c_str(), seed, &err);
    if (!ok) {
        fprintf(stderr, "fixture %s: %s\n", name, err.c_str());
    }
    return ok;
}

void testRecord() {
    uint8_t hash[16];
    for (int i = 0; i < 16; i++) {
        hash[i] = (uint8_t)(0xA0 + i);
    }
    uint8_t r[PC_RANDO_SLOT_RECORD_SIZE];
    memset(r, 0x77, sizeof r);
    pc_rando_slot_record_write(r, hash, PC_RANDO_SLOT_FLAG_DEBUG_BOOT);
    RT_CHECK(memcmp(r, "WWR1", 4) == 0);
    RT_CHECK_EQ(r[4], PC_RANDO_SLOT_RECORD_VERSION);
    RT_CHECK_EQ(r[5], PC_RANDO_SLOT_FLAG_DEBUG_BOOT);
    RT_CHECK_EQ(r[6], 0);
    RT_CHECK_EQ(r[7], 0);
    RT_CHECK(memcmp(r + 8, hash, 16) == 0);
    bool padZero = true;
    for (int i = 0x18; i < PC_RANDO_SLOT_RECORD_SIZE; i++) {
        padZero = padZero && r[i] == 0;
    }
    RT_CHECK(padZero);
    RT_CHECK(pc_rando_slot_record_is_rando(r));
    uint8_t got[16] = {};
    uint8_t flags = 0;
    RT_CHECK(pc_rando_slot_record_read(r, got, &flags));
    RT_CHECK(memcmp(got, hash, 16) == 0);
    RT_CHECK_EQ(flags, PC_RANDO_SLOT_FLAG_DEBUG_BOOT);
    RT_CHECK(pc_rando_slot_record_read(r, nullptr, nullptr));

    r[4] = 2; // another version: still a rando record, not readable
    RT_CHECK(pc_rando_slot_record_is_rando(r));
    RT_CHECK(!pc_rando_slot_record_read(r, got, &flags));

    // D4: a rando record is cleared, other bytes are not
    RT_CHECK(pc_rando_slot_record_clear_stale(r));
    bool allZero = true;
    for (uint8_t b : r) {
        allZero = allZero && b == 0;
    }
    RT_CHECK(allZero);
    RT_CHECK(!pc_rando_slot_record_is_rando(r));
    RT_CHECK(!pc_rando_slot_record_read(r, got, &flags));
    RT_CHECK(!pc_rando_slot_record_clear_stale(r));
    memset(r, 0x5A, sizeof r);
    RT_CHECK(!pc_rando_slot_record_clear_stale(r));
    RT_CHECK_EQ(r[0], 0x5A);
    RT_CHECK_EQ(r[PC_RANDO_SLOT_RECORD_SIZE - 1], 0x5A);
    memcpy(r, "WWR0", 4);
    RT_CHECK(!pc_rando_slot_record_is_rando(r));
}

void testConfig(const PcRandoSeed& base) {
    PcRandoConfig def;
    pc_rando_config_defaults(&def);
    PcRandoConfig cfg;
    PcRandoModes modes;
    std::string err;
    RT_CHECK(pc_rando_slot_config_from_seed(base, &cfg, &modes, &err));
    // TestSeed1 has wwrando's default options: exactly the defaults (R0.1).
    RT_CHECK(memcmp(&cfg, &def, sizeof cfg) == 0);
    RT_CHECK_EQ(modes.remove_music, 0);
    RT_CHECK_EQ(modes.invert_sea_compass_x_axis, 0);

    PcRandoSeed s = base;
    s.options.set("sword_mode", pcr::Json::string("Swordless"));
    s.options.set("hero_mode", pcr::Json::boolean(true));
    s.options.set("player_in_casual_clothes", pcr::Json::boolean(true));
    s.options.set("starting_pohs", pcr::Json::integer(2));
    s.options.set("starting_hcs", pcr::Json::integer(1));
    s.options.set("num_starting_triforce_shards", pcr::Json::integer(8));
    s.options.set("skip_rematch_bosses", pcr::Json::boolean(false));
    s.options.set("swift_sail", pcr::Json::boolean(false));
    s.options.set("instant_text_boxes", pcr::Json::boolean(false));
    s.options.set("reveal_full_sea_chart", pcr::Json::boolean(false));
    s.options.set("switch_targeting_mode", pcr::Json::boolean(true));
    s.options.set("remove_music", pcr::Json::boolean(true));
    s.options.set("invert_sea_compass_x_axis", pcr::Json::boolean(true));
    pcr::Json gear = pcr::Json::array();
    gear.push(pcr::Json::string("Progressive Magic Meter"));
    gear.push(pcr::Json::string("Grappling Hook"));
    gear.push(pcr::Json::string("Progressive Magic Meter"));
    s.options.set("starting_gear", gear);
    s.extra_starting_items = {"Deku Leaf"};
    RT_CHECK(pc_rando_slot_config_from_seed(s, &cfg, &modes, &err));
    RT_CHECK_EQ(cfg.sword_mode, PC_RANDO_SWORD_SWORDLESS);
    RT_CHECK(cfg.damage_multiplier == PC_RANDO_HERO_DAMAGE_MULTIPLIER);
    RT_CHECK_EQ(cfg.heros_clothes, 0);
    RT_CHECK_EQ(cfg.starting_quarter_hearts, 6);
    RT_CHECK_EQ(cfg.num_triforce_shards, 8);
    RT_CHECK_EQ(cfg.skip_rematch_bosses, 0);
    RT_CHECK_EQ(cfg.swift_sail, 0);
    RT_CHECK_EQ(cfg.instant_text_boxes, 0);
    RT_CHECK_EQ(cfg.reveal_full_sea_chart, 0);
    RT_CHECK_EQ(cfg.targeting_mode, 1);
    RT_CHECK_EQ(modes.remove_music, 1);
    RT_CHECK_EQ(modes.invert_sea_compass_x_axis, 1);
    RT_CHECK_EQ(cfg.starting_magic, 32);
    RT_CHECK_EQ(cfg.starting_gear[0], pc_rando_item_id("Grappling Hook"));
    RT_CHECK_EQ(cfg.starting_gear[1], pc_rando_item_id("Deku Leaf"));
    RT_CHECK_EQ(cfg.starting_gear[2], PC_RANDO_ITEM_END);
    RT_CHECK_EQ(cfg.starting_gear[PC_RANDO_MAX_STARTING_GEAR], PC_RANDO_ITEM_END);
    RT_CHECK_EQ(pc_rando_item_id("Grappling Hook"), 0x25);

    s.options.set("sword_mode", pcr::Json::string("No Starting Sword"));
    RT_CHECK(pc_rando_slot_config_from_seed(s, &cfg, nullptr, &err));
    RT_CHECK_EQ(cfg.sword_mode, PC_RANDO_SWORD_NO_START);

    PcRandoConfig keep = cfg;
    s.extra_starting_items = {"No Such Item"};
    RT_CHECK(!pc_rando_slot_config_from_seed(s, &cfg, nullptr, &err));
    RT_CHECK(err.find("No Such Item") != std::string::npos);
    RT_CHECK(memcmp(&cfg, &keep, sizeof cfg) == 0); // unchanged on failure
    s.extra_starting_items.clear();
    s.options.set("sword_mode", pcr::Json::string("Sword?"));
    RT_CHECK(!pc_rando_slot_config_from_seed(s, &cfg, nullptr, &err));
}

void testActivation(const PcRandoSeed& seed, const PcRandoSeed& other) {
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK(pc_rando_slot_pending() == nullptr);
    pc_rando_slot_set_pending(seed);
    RT_CHECK(pc_rando_slot_pending() != nullptr);
    RT_CHECK(memcmp(pc_rando_slot_pending()->hash, seed.hash, 16) == 0);
    pc_rando_slot_clear_pending();
    RT_CHECK(pc_rando_slot_pending() == nullptr);

    std::string err;
    RT_CHECK(pc_rando_slot_activate(seed, &err));
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK(pc_rando_seed() == pc_rando_slot_active_seed());
    RT_CHECK(memcmp(pc_rando_slot_active_seed()->hash, seed.hash, 16) == 0);
    bool same = true;
    for (int i = 0; i < PC_RANDO_SEED_LOCATIONS; i++) {
        same = same && pc_rando_loc_item(i, 0xFE) == seed.locations[i];
    }
    RT_CHECK(same);
    RT_CHECK_EQ(pc_rando_cfg()->starting_magic, 16);
    // activating another seed replaces it
    RT_CHECK(pc_rando_slot_activate(other, &err));
    RT_CHECK(memcmp(pc_rando_slot_active_seed()->hash, other.hash, 16) == 0);
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK_EQ(pc_rando_loc_item(0, 0xFE), 0xFE);

    // sidecars in a directory
    char tmpl[] = "/tmp/cos_rando_R0_7_XXXXXX";
    const char* dir = mkdtemp(tmpl);
    RT_CHECK(dir != nullptr);
    if (dir == nullptr) {
        return;
    }
    RT_CHECK(!pc_rando_slot_activate_from_dir(dir, seed.hash, &err)); // missing
    RT_CHECK(err.find("cannot open") != std::string::npos);
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK(pc_rando_slot_write_sidecar(dir, seed, &err));
    const std::string path = std::string(dir) + "/" + pc_rando_seed_sidecar_name(seed);
    RT_CHECK(pc_rando_slot_sidecar_name(seed.hash) == pc_rando_seed_sidecar_name(seed));
    RT_CHECK(access(path.c_str(), R_OK) == 0);
    RT_CHECK(pc_rando_slot_write_sidecar(dir, seed, &err)); // same text: kept
    uint8_t wrong[16];
    memcpy(wrong, seed.hash, 16);
    wrong[15] ^= 1; // same file name, another hash
    RT_CHECK(!pc_rando_slot_activate_from_dir(dir, wrong, &err));
    RT_CHECK(err.find("not the slot's") != std::string::npos);
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK(pc_rando_slot_activate_from_dir(dir, seed.hash, &err));
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK(memcmp(pc_rando_slot_active_seed()->hash, seed.hash, 16) == 0);
    pc_rando_deactivate();
    unlink(path.c_str());
    rmdir(dir);
}

} // namespace

int main() {
    testRecord();
    PcRandoSeed seed;
    PcRandoSeed other;
    if (loadFixture("R0_6_TestSeed1.json", &seed) && loadFixture("R0_6_abc_full.json", &other)) {
        RT_CHECK(memcmp(seed.hash, other.hash, 16) != 0);
        testConfig(seed);
        testActivation(seed, other);
    } else {
        RT_CHECK(false);
    }
    return rt::finish();
}
