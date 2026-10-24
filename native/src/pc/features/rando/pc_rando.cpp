// The randomizer's switch and seed-time config (native/include/pc/pc_rando.h, docs/RANDOMIZER_PLAN.md
// step R0.1). Keep this file free of game headers and game calls: the unit tests
// (native/check/rando/, cos_rando_core in native/cmake/executable.cmake) link it with no game code, so
// a game symbol here breaks every test's link. Game-side work on activation goes in a listener
// (pc_rando_add_listener) registered from another file.
#include "pc/pc_rando.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

constexpr int kMaxListeners = 16;

bool sInit = false;
bool sForced = false;
bool sActive = false;
PcRandoConfig sCfg;
const PcRandoSeed* sSeed = nullptr;
const uint8_t* sLocItems = nullptr;
int sLocCount = 0;
PcRandoListener sListeners[kMaxListeners];
int sListenerCount = 0;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

void ensureInit() {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_config_defaults(&sCfg);
    const char* force = getenv("COS_RANDO_FORCE");
    if (force != nullptr && strcmp(force, "1") == 0) {
        sForced = true;
        logf("[cos] rando: COS_RANDO_FORCE=1: code-only patches on with the default config\n");
    }
}

void notify() {
    const int active = pc_rando();
    for (int i = 0; i < sListenerCount; i++) {
        sListeners[i](active);
    }
}

} // namespace

extern "C" {

void pc_rando_config_defaults(PcRandoConfig* cfg) {
    memset(cfg, 0, sizeof *cfg);
    cfg->sword_mode = PC_RANDO_SWORD_START;
    cfg->skip_rematch_bosses = 1;
    memset(cfg->starting_gear, PC_RANDO_ITEM_END, sizeof cfg->starting_gear);
    // sorted(DEFAULT_STARTING_ITEMS) (wwr_ui/inventory.py) less the Progressive Magic Meter, which
    // tweaks.update_starting_gear turns into starting_magic: Ballad of Gales, Progressive Shield
    // (registered as 0x3B, Hero's Shield), Song of Passing.
    cfg->starting_gear[0] = 0x6E;
    cfg->starting_gear[1] = 0x3B;
    cfg->starting_gear[2] = 0x72;
    cfg->starting_quarter_hearts = 12; // starting_hcs 3 * 4 + starting_pohs 0
    cfg->starting_magic = 16;
    cfg->num_triforce_shards = 0;
    cfg->heros_clothes = 1;
    cfg->pigs_bitfield = 0x04;
    cfg->targeting_mode = 0;
    cfg->damage_multiplier = 1.0f;
    cfg->swift_sail = 1;
    cfg->instant_text_boxes = 1;
    cfg->reveal_full_sea_chart = 1;
    cfg->inter_dungeon_warps = 0; // R7.1c: add_shortcut_warps_between_dungeons
}

int pc_rando(void) {
    ensureInit();
    return sActive || sForced ? 1 : 0;
}

int pc_rando_forced(void) {
    ensureInit();
    return sForced ? 1 : 0;
}

const PcRandoConfig* pc_rando_cfg(void) {
    ensureInit();
    return &sCfg;
}

uint8_t pc_rando_loc_item(int loc, uint8_t vanilla) {
    if (!pc_rando() || sLocItems == nullptr || loc < 0 || loc >= sLocCount) {
        return vanilla;
    }
    return sLocItems[loc];
}

void pc_rando_set_active(const PcRandoSeed* seed, const PcRandoConfig* cfg) {
    ensureInit();
    if (cfg != nullptr) {
        sCfg = *cfg;
    } else {
        pc_rando_config_defaults(&sCfg);
    }
    sSeed = seed;
    sActive = true;
    logf("[cos] rando: active%s\n", seed != nullptr ? " (seed)" : " (no seed)");
    notify();
}

void pc_rando_deactivate(void) {
    ensureInit();
    const bool was = sActive;
    sActive = false;
    sSeed = nullptr;
    sLocItems = nullptr;
    sLocCount = 0;
    pc_rando_config_defaults(&sCfg);
    if (was) {
        logf("[cos] rando: inactive%s\n", sForced ? " (still forced by COS_RANDO_FORCE)" : "");
        notify();
    }
}

const PcRandoSeed* pc_rando_seed(void) {
    return sSeed;
}

void pc_rando_set_loc_items(const uint8_t* items, int count) {
    sLocItems = count > 0 ? items : nullptr;
    sLocCount = sLocItems != nullptr ? count : 0;
}

int pc_rando_add_listener(PcRandoListener fn) {
    if (fn == nullptr) {
        return 0;
    }
    for (int i = 0; i < sListenerCount; i++) {
        if (sListeners[i] == fn) {
            return 1;
        }
    }
    if (sListenerCount >= kMaxListeners) {
        return 0;
    }
    sListeners[sListenerCount++] = fn;
    if (pc_rando()) {
        fn(1);
    }
    return 1;
}

} // extern "C"
