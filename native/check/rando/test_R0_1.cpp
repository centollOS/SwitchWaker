// cos_rando_test_R0_1 (docs/RANDOMIZER_PLAN.md step R0.1): the randomizer's switch and its default
// config (native/include/pc/pc_rando.h) without a game:
//   - off by default; the config holds wwrando 1.10.0_9775811's default-preset values (custom_funcs.asm
//     data as tweaks.py writes it for the default options);
//   - pc_rando_loc_item gives the vanilla item while off, and without a placement;
//   - set_active / deactivate, listeners, the placement table;
//   - COS_RANDO_FORCE=1 (checked in a child process, since the variable is read once).
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"

#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int sLast = -1;
int sCalls = 0;
void listener(int active) {
    sLast = active;
    sCalls++;
}

int forcedChild() {
    // Runs with COS_RANDO_FORCE=1 set before the first pc_rando call.
    int bad = 0;
    bad += pc_rando() != 1;
    bad += pc_rando_forced() != 1;
    bad += pc_rando_loc_item(5, 0x33) != 0x33; // forced: code-only, no placement
    pc_rando_set_active(nullptr, nullptr);
    pc_rando_deactivate();
    bad += pc_rando() != 1; // the force survives deactivate
    return bad == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--forced-child") == 0) {
        return forcedChild();
    }
    unsetenv("COS_RANDO_FORCE");

    // Off by default.
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK_EQ(pc_rando_forced(), 0);
    RT_CHECK(pc_rando_seed() == nullptr);
    RT_CHECK_EQ(pc_rando_loc_item(0, 0x42), 0x42);

    // The defaults (wwrando's default preset).
    const PcRandoConfig* c = pc_rando_cfg();
    RT_CHECK(c != nullptr);
    RT_CHECK_EQ(c->sword_mode, PC_RANDO_SWORD_START);
    RT_CHECK_EQ(c->skip_rematch_bosses, 1);
    RT_CHECK_EQ(c->starting_gear[0], 0x6E); // Ballad of Gales
    RT_CHECK_EQ(c->starting_gear[1], 0x3B); // Progressive Shield
    RT_CHECK_EQ(c->starting_gear[2], 0x72); // Song of Passing
    for (int i = 3; i <= PC_RANDO_MAX_STARTING_GEAR; i++) {
        RT_CHECK_EQ(c->starting_gear[i], PC_RANDO_ITEM_END);
    }
    RT_CHECK_EQ(sizeof c->starting_gear, (size_t)87);
    RT_CHECK_EQ(c->starting_quarter_hearts, 12);
    RT_CHECK_EQ(c->starting_magic, 16);
    RT_CHECK_EQ(c->num_triforce_shards, 0);
    RT_CHECK_EQ(c->heros_clothes, 1);
    RT_CHECK_EQ(c->pigs_bitfield, 0x04);
    RT_CHECK_EQ(c->targeting_mode, 0);
    RT_CHECK_EQ(c->damage_multiplier, 1.0f);
    RT_CHECK_EQ(c->swift_sail, 1);
    RT_CHECK_EQ(c->instant_text_boxes, 1);
    RT_CHECK_EQ(c->reveal_full_sea_chart, 1);
    PcRandoConfig d;
    memset(&d, 0xAA, sizeof d);
    pc_rando_config_defaults(&d);
    RT_CHECK(memcmp(&d, c, sizeof d) == 0);

    // Activation, listeners and the placement.
    RT_CHECK_EQ(pc_rando_add_listener(listener), 1);
    RT_CHECK_EQ(pc_rando_add_listener(listener), 1); // once only
    RT_CHECK_EQ(pc_rando_add_listener(nullptr), 0);
    RT_CHECK_EQ(sCalls, 0); // added while off: not called
    static const uint8_t kItems[4] = {0x01, 0x02, 0x03, 0x04};
    pc_rando_set_loc_items(kItems, 4);
    RT_CHECK_EQ(pc_rando_loc_item(2, 0x42), 0x42); // still off
    PcRandoConfig hero = d;
    hero.damage_multiplier = 4.0f;
    hero.sword_mode = PC_RANDO_SWORD_NO_START;
    const PcRandoSeed* fakeSeed = reinterpret_cast<const PcRandoSeed*>(&kItems);
    pc_rando_set_active(fakeSeed, &hero);
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK(pc_rando_seed() == fakeSeed);
    RT_CHECK_EQ(sCalls, 1);
    RT_CHECK_EQ(sLast, 1);
    RT_CHECK_EQ(pc_rando_cfg()->damage_multiplier, 4.0f);
    RT_CHECK_EQ(pc_rando_cfg()->sword_mode, PC_RANDO_SWORD_NO_START);
    hero.sword_mode = PC_RANDO_SWORD_SWORDLESS; // copied, not referenced
    RT_CHECK_EQ(pc_rando_cfg()->sword_mode, PC_RANDO_SWORD_NO_START);
    RT_CHECK_EQ(pc_rando_loc_item(0, 0x42), 0x01);
    RT_CHECK_EQ(pc_rando_loc_item(3, 0x42), 0x04);
    RT_CHECK_EQ(pc_rando_loc_item(4, 0x42), 0x42);
    RT_CHECK_EQ(pc_rando_loc_item(-1, 0x42), 0x42);

    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK(pc_rando_seed() == nullptr);
    RT_CHECK_EQ(sCalls, 2);
    RT_CHECK_EQ(sLast, 0);
    RT_CHECK(memcmp(&d, pc_rando_cfg(), sizeof d) == 0);
    RT_CHECK_EQ(pc_rando_loc_item(0, 0x42), 0x42); // placement dropped
    pc_rando_deactivate(); // already off: no second call
    RT_CHECK_EQ(sCalls, 2);

    pc_rando_set_active(nullptr, nullptr); // defaults, no seed
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK(memcmp(&d, pc_rando_cfg(), sizeof d) == 0);
    RT_CHECK_EQ(pc_rando_loc_item(0, 0x42), 0x42); // no placement
    pc_rando_deactivate();

    // COS_RANDO_FORCE=1, in a child (the variable is read once per process).
    pid_t pid = fork();
    if (pid == 0) {
        setenv("COS_RANDO_FORCE", "1", 1);
        execl(argv[0], argv[0], "--forced-child", (char*)nullptr);
        _exit(127);
    }
    int status = 0;
    RT_CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    RT_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    return rt::finish();
}
