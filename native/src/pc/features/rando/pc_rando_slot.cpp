// Slot binding, pure part (docs/RANDOMIZER_PLAN.md step R0.7; API and record layout in
// pc_rando_slot.h). Part of cos_rando_core: no game headers or calls here (the hooks are
// native/src/pc/game_hooks/pc_rando_hooks_slot.cpp).
#include "pc_rando_slot.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

constexpr uint8_t kMagic[4] = {'W', 'W', 'R', '1'};
constexpr int kHashOffset = 8;

std::unique_ptr<PcRandoSeed> sPending;
std::unique_ptr<PcRandoSeed> sActive;

bool failWith(std::string* err, std::string what) {
    if (err != nullptr) {
        *err = std::move(what);
    }
    return false;
}

const pcr::Json* opt(const PcRandoSeed& s, const char* name) {
    return s.options.find(name);
}

bool readFile(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    out->clear();
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        out->append(buf, n);
    }
    fclose(f);
    return true;
}

} // namespace

bool pc_rando_slot_record_is_rando(const uint8_t* reserve) {
    return reserve != nullptr && memcmp(reserve, kMagic, sizeof kMagic) == 0;
}

void pc_rando_slot_record_write(uint8_t* reserve, const uint8_t hash[16], uint8_t flags) {
    memset(reserve, 0, PC_RANDO_SLOT_RECORD_SIZE);
    memcpy(reserve, kMagic, sizeof kMagic);
    reserve[4] = PC_RANDO_SLOT_RECORD_VERSION;
    reserve[5] = flags;
    memcpy(reserve + kHashOffset, hash, 16);
}

bool pc_rando_slot_record_read(const uint8_t* reserve, uint8_t hash[16], uint8_t* flags) {
    if (!pc_rando_slot_record_is_rando(reserve) || reserve[4] != PC_RANDO_SLOT_RECORD_VERSION) {
        return false;
    }
    if (hash != nullptr) {
        memcpy(hash, reserve + kHashOffset, 16);
    }
    if (flags != nullptr) {
        *flags = reserve[5];
    }
    return true;
}

bool pc_rando_slot_record_clear_stale(uint8_t* reserve) {
    if (!pc_rando_slot_record_is_rando(reserve)) {
        return false;
    }
    // dSv_reserve_c::init: all zero.
    memset(reserve, 0, PC_RANDO_SLOT_RECORD_SIZE);
    return true;
}

std::string pc_rando_slot_sidecar_name(const uint8_t hash[16]) {
    char b[48];
    snprintf(b, sizeof b, "switchwaker-rando-%02x%02x%02x%02x.json", hash[0], hash[1], hash[2], hash[3]);
    return b;
}

bool pc_rando_slot_config_from_seed(const PcRandoSeed& seed, PcRandoConfig* cfg, PcRandoModes* modes,
                                    std::string* err) {
    PcRandoConfig c;
    pc_rando_config_defaults(&c);
    PcRandoModes m;
    pc_rando_modes_defaults(&m);

    // tweaks.update_sword_mode_game_variable
    if (const pcr::Json* v = opt(seed, "sword_mode"); v != nullptr && v->isString()) {
        const std::string& s = v->text();
        if (s == "Start with Hero's Sword") {
            c.sword_mode = PC_RANDO_SWORD_START;
        } else if (s == "No Starting Sword") {
            c.sword_mode = PC_RANDO_SWORD_NO_START;
        } else if (s == "Swordless") {
            c.sword_mode = PC_RANDO_SWORD_SWORDLESS;
        } else {
            return failWith(err, "unknown sword_mode \"" + s + "\"");
        }
    }
    auto flag = [&](const char* name, uint8_t* out) {
        if (const pcr::Json* v = opt(seed, name); v != nullptr && v->isBool()) {
            *out = v->asBool() ? 1 : 0;
        }
    };
    flag("skip_rematch_bosses", &c.skip_rematch_bosses);
    flag("switch_targeting_mode", &c.targeting_mode);
    flag("swift_sail", &c.swift_sail);
    flag("instant_text_boxes", &c.instant_text_boxes);
    flag("reveal_full_sea_chart", &c.reveal_full_sea_chart);
    flag("remove_music", &m.remove_music);
    flag("invert_sea_compass_x_axis", &m.invert_sea_compass_x_axis);
    // should_start_with_heros_clothes: 0 only with player_in_casual_clothes (custom models that
    // disable casual clothes do not exist here).
    if (const pcr::Json* v = opt(seed, "player_in_casual_clothes"); v != nullptr && v->isBool()) {
        c.heros_clothes = v->asBool() ? 0 : 1;
    }
    if (const pcr::Json* v = opt(seed, "hero_mode"); v != nullptr && v->isBool()) {
        pc_rando_config_set_hero_mode(&c, v->asBool() ? 1 : 0);
    }
    // set_num_starting_triforce_shards, set_starting_health
    if (const pcr::Json* v = opt(seed, "num_starting_triforce_shards"); v != nullptr && v->isInteger()) {
        c.num_triforce_shards = (uint8_t)v->asInt();
    }
    {
        const pcr::Json* pohs = opt(seed, "starting_pohs");
        const pcr::Json* hcs = opt(seed, "starting_hcs");
        if (pohs != nullptr && hcs != nullptr && pohs->isInteger() && hcs->isInteger()) {
            c.starting_quarter_hearts = (uint16_t)(hcs->asInt() * 4 + pohs->asInt());
        }
    }
    // update_starting_gear(options.starting_gear + extra_starting_items): Progressive Magic Meters
    // become starting_magic (16 each), the rest go to the array in order, 0xFF-terminated.
    if (const pcr::Json* v = opt(seed, "starting_gear"); v != nullptr && v->isArray()) {
        std::vector<std::string> gear;
        for (size_t i = 0; i < v->size(); i++) {
            if (!v->at(i).isString()) {
                return failWith(err, "starting_gear holds a non-string");
            }
            gear.push_back(v->at(i).text());
        }
        gear.insert(gear.end(), seed.extra_starting_items.begin(), seed.extra_starting_items.end());
        int magic = 0;
        int n = 0;
        memset(c.starting_gear, PC_RANDO_ITEM_END, sizeof c.starting_gear);
        for (const std::string& name : gear) {
            if (name == "Progressive Magic Meter") {
                magic++;
                continue;
            }
            const int id = pc_rando_item_id(name);
            if (id < 0) {
                return failWith(err, "unknown starting item \"" + name + "\"");
            }
            if (n >= PC_RANDO_MAX_STARTING_GEAR) {
                return failWith(err, "more starting items than custom_funcs.asm has room for");
            }
            c.starting_gear[n++] = (uint8_t)id;
        }
        c.starting_magic = (uint8_t)(16 * magic);
    }
    *cfg = c;
    if (modes != nullptr) {
        *modes = m;
    }
    return true;
}

void pc_rando_slot_set_pending(const PcRandoSeed& seed) {
    sPending = std::make_unique<PcRandoSeed>(seed);
}

void pc_rando_slot_clear_pending() {
    sPending.reset();
}

const PcRandoSeed* pc_rando_slot_pending() {
    return sPending.get();
}

bool pc_rando_slot_activate(const PcRandoSeed& seed, std::string* err) {
    PcRandoConfig cfg;
    PcRandoModes modes;
    if (!pc_rando_slot_config_from_seed(seed, &cfg, &modes, err)) {
        return false;
    }
    // The listeners of a previous activation see it end before the new placement is set.
    if (pc_rando() && !pc_rando_forced()) {
        pc_rando_deactivate();
    }
    if (sActive.get() != &seed) {
        sActive = std::make_unique<PcRandoSeed>(seed);
    }
    pc_rando_modes_set(&modes);
    pc_rando_set_loc_items(sActive->locations, PC_RANDO_SEED_LOCATIONS);
    pc_rando_set_active(sActive.get(), &cfg);
    return true;
}

bool pc_rando_slot_activate_from_dir(const std::string& dir, const uint8_t hash[16], std::string* err) {
    const std::string path = dir + "/" + pc_rando_slot_sidecar_name(hash);
    PcRandoSeed seed;
    std::string why;
    if (!pc_rando_seed_load_file(path.c_str(), &seed, &why)) {
        return failWith(err, path + ": " + why);
    }
    if (memcmp(seed.hash, hash, 16) != 0) {
        return failWith(err, path + ": hash " + pc_rando_seed_hash_hex(seed) + " is not the slot's");
    }
    return pc_rando_slot_activate(seed, err);
}

const PcRandoSeed* pc_rando_slot_active_seed() {
    return sActive.get();
}

bool pc_rando_slot_write_sidecar(const std::string& dir, const PcRandoSeed& seed, std::string* err) {
    const std::string path = dir + "/" + pc_rando_seed_sidecar_name(seed);
    const std::string text = pc_rando_seed_serialize(seed);
    std::string have;
    if (readFile(path, &have) && have == text) {
        return true;
    }
    return pc_rando_seed_save_file(path.c_str(), seed, err);
}
