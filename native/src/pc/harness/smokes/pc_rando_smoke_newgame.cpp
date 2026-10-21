// COS_SMOKE=rando COS_RANDO_SMOKE=newgame (docs/RANDOMIZER_PLAN.md step R3.1): the new-game state of a
// randomized file (pc_rando_new_game_init, custom_funcs.asm init_save_with_tweaks and
// reveal_sea_chart.asm; tables in features/rando/pc_rando_newgame.h):
//   native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=newgame
// The run has its own memory card (pc_rando_smoke_wants_run_card, as the slot smoke), since a rando new
// game writes its sidecar into the card folder. Not with COS_RANDO_FORCE (it checks "off").
//
// Once the play scene has run 10 frames, in one frame:
//   1. vanilla: a new game through R0.7's hooks (pc_rando_slot_new_game_begin, dComIfGs_init,
//      pc_rando_slot_new_game_end; no pending seed) gives the same dSv_info_c bytes as a plain
//      dComIfGs_init (but mRandomSalvagePoint, which dSv_player_info_c::init draws at random), and pc_rando_new_game_init does nothing while pc_rando() is 0;
//   2. a randomized new game through the same hooks with a pending seed (COS_RANDO_NEWGAME_FIXTURE,
//      default the TestSeed1 placement: start with sword, gear Ballad of Gales + Progressive Shield +
//      Song of Passing + 1 magic meter, skip rematches, reveal chart): every item, count, event bit,
//      register, switch word, dungeon bit and byte of the list in pc_rando_newgame.h is set as the asm
//      sets it, nothing more in the stage switch words and dungeon bits, the mTmp bit 0x0310 is not
//      set (wwrando's own write is cleared by mTmp.init), and the record is in mReserve;
//   3. other configs, activated directly: swordless (barrier bits, no sword), 8 shards (0xFF and
//      0x3D04), no rematch skip, no chart, magic 32, switch targeting; then no starting sword;
//   4. back to vanilla (pc_rando_deactivate): a new game gives the step-1 bytes again.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_newgame.h"
#include "features/rando/pc_rando_slot.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "newgame";
constexpr const char* kDefaultFixture = "native/check/rando/fixtures/R0_6_TestSeed1.json";

std::vector<uint8_t> snapshot() {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&g_dComIfG_gameInfo.save);
    return std::vector<uint8_t>(p, p + sizeof(g_dComIfG_gameInfo.save));
}

// dSv_player_info_c::init draws mRandomSalvagePoint with cM_rndF(3.0f), so two inits may differ there.
size_t salvageOffset() {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&g_dComIfG_gameInfo.save);
    return (size_t)(&g_dComIfG_gameInfo.save.getSavedata().getPlayer().getPlayerInfo().mRandomSalvagePoint - base);
}

bool sameAs(const std::vector<uint8_t>& a, const char* what) {
    const std::vector<uint8_t> b = snapshot();
    const size_t skip = salvageOffset();
    size_t diffs = 0;
    size_t first = 0;
    for (size_t i = 0; i < a.size(); i++) {
        if (i != skip && a[i] != b[i]) {
            if (diffs++ == 0) {
                first = i;
            }
        }
    }
    if (diffs != 0) {
        writef(STDERR_FILENO, "[cos] rando-smoke newgame: %s: %zu byte(s) differ, first at dSv_info_c+0x%zX\n",
               what, diffs, first);
    }
    return randoSmokeCheck(kName, what, diffs == 0);
}

void vanillaNewGame() {
    pc_rando_slot_new_game_begin();
    dComIfGs_init();
    pc_rando_slot_new_game_end();
}

std::string resolve(const char* path) {
    if (path[0] == '/' || access(path, R_OK) == 0) {
        return path;
    }
    const char* user = pc_user_path();
    return std::string(user != nullptr ? user : ".") + "/../../../" + path;
}

dSv_save_c& saveData() {
    return g_dComIfG_gameInfo.save.getSavedata();
}

// Every switch word and dungeon-item byte of the 16 stage infos is what the plan sets on a cleared
// stage, nothing more.
bool stageInfoExact() {
    bool ok = true;
    for (int stage = 0; stage < dSv_save_c::STAGE_MAX; stage++) {
        dSv_memBit_c& bit = saveData().getSave(stage).getBit();
        for (int w = 0; w < 4; w++) {
            const uint32_t want = pc_rando_newgame_switch_word(stage, w);
            const uint32_t got = bit.mSwitch[w];
            if (got != want) {
                writef(STDERR_FILENO, "[cos] rando-smoke newgame: stage %d switch word %d = 0x%08X, want 0x%08X\n",
                       stage, w, got, want);
                ok = false;
            }
        }
        const bool demo = stage >= PCR_NEWGAME_BOSS_DEMO_FIRST_STAGE && stage <= PCR_NEWGAME_BOSS_DEMO_LAST_STAGE;
        const uint8_t wantDi = demo ? (uint8_t)(1 << PCR_NEWGAME_BOSS_DEMO_BIT) : 0;
        if (bit.mDungeonItem != wantDi) {
            writef(STDERR_FILENO, "[cos] rando-smoke newgame: stage %d dungeon bits 0x%02X, want 0x%02X\n", stage,
                   bit.mDungeonItem, wantDi);
            ok = false;
        }
    }
    return ok;
}

bool allBits(const uint16_t* bits, int n, bool want) {
    bool ok = true;
    for (int i = 0; i < n; i++) {
        if ((dComIfGs_isEventBit(bits[i]) != FALSE) != want) {
            writef(STDERR_FILENO, "[cos] rando-smoke newgame: event bit 0x%04X is %s\n", bits[i],
                   want ? "off" : "on");
            ok = false;
        }
    }
    return ok;
}

bool chartBits(bool all) {
    dSv_player_map_c& map = saveData().getPlayer().getMap();
    for (int i = 0; i < (int)ARRAY_SIZE(map.mFmapBits); i++) {
        const bool initCharted = i == dIsleIdx_ForsakenFortress_e || i == dIsleIdx_OutsetIsland_e ||
                                 i == dIsleIdx_WindfallIsland_e;
        const uint8_t want = initCharted ? 3 : (all ? 1 : 0);
        if (map.mFmapBits[i] != want) {
            writef(STDERR_FILENO, "[cos] rando-smoke newgame: mFmapBits[%d] = 0x%02X, want 0x%02X\n", i,
                   map.mFmapBits[i], want);
            return false;
        }
    }
    return true;
}

bool hasSword() {
    return dComIfGs_isCollect(0, 0) && dComIfGs_getSelectEquip(0) == dItemNo_SWORD_e;
}

// The checks every config shares (the cfg-dependent ones are the caller's).
void commonChecks(const char* tag, const PcRandoConfig& cfg) {
    char what[160];
    auto check = [&](const char* name, bool ok) {
        snprintf(what, sizeof what, "%s: %s", tag, name);
        randoSmokeCheck(kName, what, ok);
    };
    check("sail", dComIfGs_getItem(dInvSlot_SAIL_e) == dItemNo_SAIL_e && dComIfGs_isGetItem(dInvSlot_SAIL_e, 0));
    check("Wind Waker", dComIfGs_getItem(dInvSlot_WIND_WAKER_e) == dItemNo_WIND_WAKER_e &&
                            dComIfGs_isGetItem(dInvSlot_WIND_WAKER_e, 0));
    check("Wind's Requiem", dComIfGs_isTact(0) != FALSE);
    check("Pirate's Charm", dComIfGs_isCollect(3, 0) != FALSE);
    check("30 arrows and bombs, current and max", dComIfGs_getArrowNum() == 30 && dComIfGs_getBombNum() == 30 &&
                                                      dComIfGs_getArrowMax() == 30 && dComIfGs_getBombMax() == 30);
    check("magic", dComIfGs_getMagic() == cfg.starting_magic && dComIfGs_getMaxMagic() == cfg.starting_magic);
    check("38 story event bits", allBits(pcr_newgame_story_bits, PCR_NEWGAME_STORY_BITS, true));
    check("pigs register", dComIfGs_getEventReg(PCR_NEWGAME_PIGS_REG) == cfg.pigs_bitfield);
    check("targeting mode", dComIfGs_getOptAttentionType() == cfg.targeting_mode);
    check("stage switch words and boss-intro bits exact", stageInfoExact());
    dSv_player_get_bag_item_c& bag = saveData().getPlayer().getGetBagItem();
    check("spoils and bait ever owned", bag.mBeastFlags == 0xFF && bag.mBaitFlags == 0xFF);
    const unsigned n = cfg.num_triforce_shards;
    check("triforce shards", saveData().getPlayer().getCollect().mTriforce == (uint8_t)((1u << n) - 1u));
    check("0x3D04 iff 8 shards", (dComIfGs_isEventBit(PCR_NEWGAME_TRIFORCE_REFUSE) != FALSE) == (n >= 8));
    check("rematch bits iff skip_rematch_bosses",
          allBits(pcr_newgame_rematch_bits, PCR_NEWGAME_REMATCH_BITS, cfg.skip_rematch_bosses == 1));
    check("chart", chartBits(cfg.reveal_full_sea_chart != 0));
    check("mTmp 0x0310 not set (cleared in wwrando)", dComIfGs_isTmpBit(PCR_NEWGAME_TMP_BIT_CLEARED) == FALSE);
    const bool barrier = cfg.sword_mode == PC_RANDO_SWORD_SWORDLESS;
    check("sword by sword mode", hasSword() == (cfg.sword_mode == PC_RANDO_SWORD_START));
    check("barrier bits iff swordless", (dComIfGs_isEventBit(PCR_NEWGAME_BARRIER_DOWN) != FALSE) == barrier &&
                                            (dComIfGs_isEventBit(PCR_NEWGAME_BARRIER_DOWN2) != FALSE) == barrier);
}

void directConfig(const char* tag, const PcRandoConfig& cfg) {
    pc_rando_set_active(nullptr, &cfg);
    dComIfGs_init();
    pc_rando_new_game_init();
    randoSmokeCheck(kName, tag, pc_rando() == 1);
    commonChecks(tag, cfg);
}

void run() {
    if (!randoSmokeCheck(kName, "not forced (COS_RANDO_FORCE is unset)", !pc_rando_forced())) {
        randoSmokeEnd(kName);
    }
    // 1. vanilla
    randoSmokeCheck(kName, "vanilla boot: pc_rando() 0", pc_rando() == 0);
    vanillaNewGame();
    const std::vector<uint8_t> vanilla = snapshot();
    dComIfGs_init();
    sameAs(vanilla, "vanilla: the hooked new game equals a plain dComIfGs_init");
    pc_rando_new_game_init();
    sameAs(vanilla, "vanilla: pc_rando_new_game_init does nothing while pc_rando() is 0");

    // 2. a randomized new game through R0.7's hooks
    const char* env = getenv("COS_RANDO_NEWGAME_FIXTURE");
    const std::string fixture = resolve(env != nullptr && env[0] != '\0' ? env : kDefaultFixture);
    PcRandoSeed seed;
    std::string err;
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &seed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke newgame: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    pc_rando_slot_set_pending(seed);
    vanillaNewGame(); // the same three calls; the pending seed makes it randomized
    randoSmokeCheck(kName, "seed: pc_rando() 1", pc_rando() == 1);
    randoSmokeCheck(kName, "seed: record in mReserve",
                    pc_rando_slot_record_is_rando(dComIfGs_getPReserve()->mReserve) != 0);
    const PcRandoConfig cfg = *pc_rando_cfg();
    randoSmokeCheck(kName, "seed: the fixture's config (sword, magic 16, rematch skip, chart, 3 gear)",
                    cfg.sword_mode == PC_RANDO_SWORD_START && cfg.starting_magic == 16 &&
                        cfg.skip_rematch_bosses == 1 && cfg.reveal_full_sea_chart == 1 &&
                        cfg.starting_gear[0] == 0x6E && cfg.starting_gear[1] == 0x3B &&
                        cfg.starting_gear[2] == 0x72 && cfg.starting_gear[3] == PC_RANDO_ITEM_END);
    commonChecks("seed", cfg);
    randoSmokeCheck(kName, "seed: gear Ballad of Gales", dComIfGs_isTact(1) != FALSE);
    randoSmokeCheck(kName, "seed: gear Song of Passing", dComIfGs_isTact(5) != FALSE);
    randoSmokeCheck(kName, "seed: gear Progressive Shield -> Hero's Shield",
                    dComIfGs_isCollect(1, 0) && dComIfGs_getSelectEquip(1) == dItemNo_SHIELD_e);

    // 3. other configs
    PcRandoConfig c = cfg;
    c.sword_mode = PC_RANDO_SWORD_SWORDLESS;
    c.num_triforce_shards = 8;
    c.skip_rematch_bosses = 0;
    c.reveal_full_sea_chart = 0;
    c.starting_magic = 32;
    c.targeting_mode = 1;
    c.pigs_bitfield = 0x01;
    memset(c.starting_gear, PC_RANDO_ITEM_END, sizeof c.starting_gear);
    directConfig("swordless, 8 shards, no rematch skip, no chart, magic 32, switch targeting", c);
    randoSmokeCheck(kName, "no gear: no shield", dComIfGs_isCollect(1, 0) == FALSE);
    c.sword_mode = PC_RANDO_SWORD_NO_START;
    c.num_triforce_shards = 3;
    directConfig("no starting sword, 3 shards", c);

    // 4. back to vanilla
    pc_rando_deactivate();
    randoSmokeCheck(kName, "deactivated: pc_rando() 0", pc_rando() == 0);
    vanillaNewGame();
    sameAs(vanilla, "vanilla again: same bytes as step 1");
    randoSmokeEnd(kName);
}

} // namespace

void randoSmokeNewgame(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(10)) {
        return;
    }
    run();
}

} // namespace pc
