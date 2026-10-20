// pc_rando_modes.cpp - the randomizer's small modes (docs/RANDOMIZER_PLAN.md step R6.15; API and
// wwrando references in native/include/pc/pc_rando_modes.h). Pure: part of cos_rando_core.
#include "pc/pc_rando_modes.h"

namespace {

PcRandoModes sModes = {0, 0};

} // namespace

extern "C" {

void pc_rando_modes_defaults(PcRandoModes* modes) {
    modes->remove_music = 0;
    modes->invert_sea_compass_x_axis = 0;
}

void pc_rando_modes_set(const PcRandoModes* modes) {
    if (modes == nullptr) {
        pc_rando_modes_defaults(&sModes);
    } else {
        sModes = *modes;
    }
}

const PcRandoModes* pc_rando_modes(void) {
    return &sModes;
}

void pc_rando_config_set_hero_mode(PcRandoConfig* cfg, int on) {
    cfg->damage_multiplier = on ? PC_RANDO_HERO_DAMAGE_MULTIPLIER : 1.0f;
}

int pc_rando_hero_mode(void) {
    return pc_rando() && pc_rando_cfg()->damage_multiplier != 1.0f;
}

float pc_rando_hero_damage(float amount) {
    // hero_mode.asm multiply_damage_amount: only a negative amount is multiplied.
    if (pc_rando_hero_mode() && amount < 0.0f) {
        return amount * pc_rando_cfg()->damage_multiplier;
    }
    return amount;
}

uint8_t pc_rando_hero_drop(uint8_t item) {
    // dItemNo_HEART_e 0x00 -> dItemNo_GREEN_RUPEE_e 0x01 (hero_mode.asm ki_item_d, d_a_bl 0x50C4).
    if (pc_rando_hero_mode() && item == 0x00) {
        return 0x01;
    }
    return item;
}

int pc_rando_remove_music(void) {
    return pc_rando() && sModes.remove_music;
}

int pc_rando_invert_sea_compass(void) {
    return pc_rando() && sModes.invert_sea_compass_x_axis;
}

uint8_t pc_rando_targeting_mode(uint8_t vanilla) {
    return pc_rando() ? pc_rando_cfg()->targeting_mode : vanilla;
}

int pc_rando_low_health_patch(void) {
    return pc_rando() && pc_rando_cfg()->starting_quarter_hearts < PC_RANDO_LOW_HEALTH_BEEP_MIN_START;
}

int pc_rando_mute_low_health_beep(int life, int max_life) {
    // remove_low_health_beep_anim.asm processHeartGaugeSound: the "<= 4" (one heart) branch skips its
    // beep when the maximum health is under 8; the "<= 2" and "<= 6" branches are unchanged.
    return pc_rando_low_health_patch() && life > 2 && life <= 4 && max_life < 8;
}

int pc_rando_skip_low_health_anim(int life, int max_life) {
    // remove_low_health_anim_at_full_health: (max & ~3) - life < 2 -> return 0.
    return pc_rando_low_health_patch() && (max_life & ~3) - life < 2;
}

} // extern "C"
