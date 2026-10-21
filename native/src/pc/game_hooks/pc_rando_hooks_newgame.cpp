// The game side of the randomizer's new-game state (docs/RANDOMIZER_PLAN.md step R3.1; tables and the
// config -> plan step in features/rando/pc_rando_newgame.{h,cpp}, which stay free of game code for the
// unit tests). pc_rando_new_game_init is wwrando's init_save_with_tweaks (custom_funcs.asm:5-320)
// minus its first line, the plain dSv_save_c::init, which dComIfGs_init has just run; R0.7's slot hooks
// call it after the init of a randomized new file (NameInMain, and the debug boot with
// COS_RANDO_PLACEMENT). reveal_sea_chart.asm's one-instruction change to dSv_player_map_c::init is
// applied here too, as a write to the new save, so dSv_player_map_c::init itself has no hunk.
//
// Differences from the asm, all with the same resulting save:
//   - the switch words of the eight stage infos are stored by the asm as whole u32 words over the
//     zeroes dSv_save_c::init left; the port sets the same switches one by one (onSwitch), which the
//     unit test checks gives the asm's words;
//   - generic_on_dungeon_bit writes to the currently loaded stage info (mMemory) instead of the saved
//     one when dSv_info_c::mDan's stale stage number happens to equal the dungeon's; dSv_info_c::init
//     then clears that copy (mMemory.init), losing the bit. The port always writes the saved stage
//     info, which is what the asm means;
//   - the mTmp bit 0x0310 is not set: dSv_info_c::init runs mTmp.init() right after the call wwrando
//     replaced, so in wwrando it never survives either (pc_rando_newgame.h).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_newgame.h"
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_save.h"

#include <unistd.h>

extern "C" void pc_rando_new_game_init(void) {
    if (!pc_rando()) {
        return;
    }
    PcRandoNewGamePlan plan;
    pc_rando_newgame_plan(pc_rando_cfg(), &plan);
    dSv_save_c& save = g_dComIfG_gameInfo.save.getSavedata();
    dSv_player_c& player = save.getPlayer();

    // reveal_sea_chart.asm 0x8005B2CC (li r4, 1 in dSv_player_map_c::init's mFmapBits loop): every
    // sector's charted bit; the original loop stores 0 (FF, Outset and Windfall then get 1|2).
    if (plan.reveal_chart) {
        dSv_player_map_c& map = player.getMap();
        for (int i = 0; i < (int)ARRAY_SIZE(map.mFmapBits); i++) {
            map.onFmapBit(i, 0);
        }
    }

    // sword_mode
    if (plan.give_sword) {
        item_func_sword();
    }
    for (int i = 0; i < plan.n_barrier_bits; i++) {
        dComIfGs_onEventBit(plan.barrier_bits[i]);
    }

    item_func_normal_sail();
    item_func_wind_tact();      // Wind Waker
    item_func_tact_song1();     // Wind's Requiem
    item_func_pirates_omamori(); // Pirate's Charm

    // 30 bombs and arrows, current and max (not the ability to use them).
    player.getItemRecord().setArrowNum(PCR_NEWGAME_ARROWS_BOMBS);
    player.getItemRecord().setBombNum(PCR_NEWGAME_ARROWS_BOMBS);
    player.getItemMax().setArrowNum(PCR_NEWGAME_ARROWS_BOMBS);
    player.getItemMax().setBombNum(PCR_NEWGAME_ARROWS_BOMBS);

    // starting_magic: max and current.
    player.getPlayerStatusA().setMaxMagic(plan.magic);
    player.getPlayerStatusA().setMagic(plan.magic);

    // init_starting_gear: convert_progressive_item_id, then execItemGet (pc_rando's dispatch).
    for (int i = 0; i < plan.n_gear; i++) {
        execItemGet((u8)pc_rando_convert_progressive_item_id(plan.gear[i]));
    }

    for (uint16_t bit : pcr_newgame_story_bits) {
        dComIfGs_onEventBit(bit);
    }
    dComIfGs_setEventReg(PCR_NEWGAME_PIGS_REG, plan.pigs);
    player.getConfig().setAttentionType(plan.targeting_mode);

    for (const PcrNewgameSwitch& s : pcr_newgame_switches) {
        save.getSave(s.stage).getBit().onSwitch(s.sw);
    }
    for (int stage = PCR_NEWGAME_BOSS_DEMO_FIRST_STAGE; stage <= PCR_NEWGAME_BOSS_DEMO_LAST_STAGE; stage++) {
        save.getSave(stage).getBit().onDungeonItem(PCR_NEWGAME_BOSS_DEMO_BIT);
    }

    // Every spoil and bait "ever owned": no first-pickup item-get animation.
    player.getGetBagItem().mBeastFlags = PCR_NEWGAME_EVER_OWNED;
    player.getGetBagItem().mBaitFlags = PCR_NEWGAME_EVER_OWNED;

    player.getCollect().mTriforce = plan.triforce;
    for (int i = 0; i < plan.n_end_bits; i++) {
        dComIfGs_onEventBit(plan.end_bits[i]);
    }

    pc::writef(STDERR_FILENO,
               "[cos] rando newgame: sword mode %u, %d starting item(s), magic %u, shards 0x%02X, "
               "rematch skip %u, chart %u\n",
               pc_rando_cfg()->sword_mode, plan.n_gear, plan.magic, plan.triforce,
               pc_rando_cfg()->skip_rematch_bosses, plan.reveal_chart);
}
