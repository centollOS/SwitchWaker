// cos_rando_test_R6_15 (docs/RANDOMIZER_PLAN.md step R6.15): the small modes' decisions
// (native/include/pc/pc_rando_modes.h) without a game:
//   - while pc_rando() is 0 every function gives the vanilla answer, whatever the modes and config;
//   - hero mode (damage_multiplier 4.0): negative amounts x4, positive ones unchanged, the heart drop
//     becomes a green rupee;
//   - remove_music and the inverted sea compass follow the modes; the targeting mode follows the config;
//   - the low-health beep/animation patch (starting health under 8 quarter hearts), checked against
//     remove_low_health_beep_anim.asm's comparisons over every life/max pair.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_modes.h"

#include <cstdlib>

namespace {

// remove_low_health_beep_anim.asm, written out as the asm does it.
bool asmBeepMuted(int life, int max) {
    if (life <= 2) return false;      // check_half: beep 3
    if (life <= 4) return max < 8;    // check_one: beep 2 unless max < 8
    return false;                     // check_one_and_a_half or nothing
}
bool asmAnimSkipped(int life, int max) {
    return (max & ~3) - life < 2;
}

void allVanilla() {
    RT_CHECK_EQ(pc_rando_hero_mode(), 0);
    RT_CHECK_EQ(pc_rando_hero_damage(-1.0f), -1.0f);
    RT_CHECK_EQ(pc_rando_hero_damage(-0.25f), -0.25f);
    RT_CHECK_EQ(pc_rando_hero_drop(0x00), 0x00);
    RT_CHECK_EQ(pc_rando_remove_music(), 0);
    RT_CHECK_EQ(pc_rando_invert_sea_compass(), 0);
    RT_CHECK_EQ(pc_rando_targeting_mode(0), 0);
    RT_CHECK_EQ(pc_rando_low_health_patch(), 0);
    RT_CHECK_EQ(pc_rando_mute_low_health_beep(4, 4), 0);
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(4, 4), 0);
}

} // namespace

int main() {
    unsetenv("COS_RANDO_FORCE");

    // Defaults.
    PcRandoModes m;
    pc_rando_modes_defaults(&m);
    RT_CHECK_EQ(m.remove_music, 0);
    RT_CHECK_EQ(m.invert_sea_compass_x_axis, 0);
    RT_CHECK_EQ(pc_rando_modes()->remove_music, 0);
    RT_CHECK_EQ(pc_rando_modes()->invert_sea_compass_x_axis, 0);
    PcRandoConfig cfg;
    pc_rando_config_defaults(&cfg);
    pc_rando_config_set_hero_mode(&cfg, 1);
    RT_CHECK_EQ(cfg.damage_multiplier, 4.0f);
    pc_rando_config_set_hero_mode(&cfg, 0);
    RT_CHECK_EQ(cfg.damage_multiplier, 1.0f);

    // Off: everything vanilla, even with every mode set.
    m.remove_music = 1;
    m.invert_sea_compass_x_axis = 1;
    pc_rando_modes_set(&m);
    allVanilla();

    // On with the default config: hero mode, the targeting mode and the low-health patch are off
    // (multiplier 1.0, targeting 0, 12 quarter hearts); the modes set above are on.
    pc_rando_set_active(nullptr, &cfg);
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK_EQ(pc_rando_hero_mode(), 0);
    RT_CHECK_EQ(pc_rando_hero_damage(-1.0f), -1.0f);
    RT_CHECK_EQ(pc_rando_hero_drop(0x00), 0x00);
    RT_CHECK_EQ(pc_rando_remove_music(), 1);
    RT_CHECK_EQ(pc_rando_invert_sea_compass(), 1);
    RT_CHECK_EQ(pc_rando_targeting_mode(1), 0); // the seed's value replaces the vanilla one
    RT_CHECK_EQ(pc_rando_low_health_patch(), 0);
    RT_CHECK_EQ(pc_rando_mute_low_health_beep(4, 4), 0);
    pc_rando_modes_set(nullptr);
    RT_CHECK_EQ(pc_rando_remove_music(), 0);
    RT_CHECK_EQ(pc_rando_invert_sea_compass(), 0);

    // Hero mode, switch targeting, 1 starting heart.
    pc_rando_config_set_hero_mode(&cfg, 1);
    cfg.targeting_mode = 1;
    cfg.starting_quarter_hearts = 4;
    pc_rando_set_active(nullptr, &cfg);
    RT_CHECK_EQ(pc_rando_hero_mode(), 1);
    RT_CHECK_EQ(pc_rando_hero_damage(-1.0f), -4.0f);
    RT_CHECK_EQ(pc_rando_hero_damage(-0.25f), -1.0f);
    RT_CHECK_EQ(pc_rando_hero_damage(0.0f), 0.0f);
    RT_CHECK_EQ(pc_rando_hero_damage(2.0f), 2.0f); // healing is not multiplied
    RT_CHECK_EQ(pc_rando_hero_drop(0x00), 0x01);   // heart -> green rupee
    RT_CHECK_EQ(pc_rando_hero_drop(0x09), 0x09);   // large magic unchanged
    RT_CHECK_EQ(pc_rando_hero_drop(0x0C), 0x0C);   // arrows unchanged
    RT_CHECK_EQ(pc_rando_targeting_mode(0), 1);
    RT_CHECK_EQ(pc_rando_low_health_patch(), 1);
    int bad = 0;
    for (int max = 0; max <= 80; max++) {
        for (int life = 0; life <= max; life++) {
            bad += pc_rando_mute_low_health_beep(life, max) != (int)asmBeepMuted(life, max);
            bad += pc_rando_skip_low_health_anim(life, max) != (int)asmAnimSkipped(life, max);
        }
    }
    RT_CHECK_EQ(bad, 0);
    RT_CHECK_EQ(pc_rando_mute_low_health_beep(4, 4), 1);  // full single heart: no beep
    RT_CHECK_EQ(pc_rando_mute_low_health_beep(4, 8), 0);  // two hearts max: the vanilla beep
    RT_CHECK_EQ(pc_rando_mute_low_health_beep(2, 4), 0);  // half a heart: still beeps
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(4, 4), 1);  // full: no animation
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(3, 4), 1);  // a quarter missing: none either
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(2, 4), 0);  // half a heart missing: the animation
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(7, 7), 1);  // 7 -> 4: unfinished pieces ignored
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(4, 7), 1);

    // 8 starting quarter hearts: not patched (tweaks.py: starting_health < 8).
    cfg.starting_quarter_hearts = 8;
    pc_rando_set_active(nullptr, &cfg);
    RT_CHECK_EQ(pc_rando_low_health_patch(), 0);
    RT_CHECK_EQ(pc_rando_mute_low_health_beep(4, 4), 0);
    RT_CHECK_EQ(pc_rando_skip_low_health_anim(8, 8), 0);

    // Off again: vanilla.
    m.remove_music = 1;
    m.invert_sea_compass_x_axis = 1;
    pc_rando_modes_set(&m);
    pc_rando_deactivate();
    allVanilla();
    return rt::finish();
}
