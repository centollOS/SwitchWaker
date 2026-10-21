// COS_SMOKE=rando COS_RANDO_SMOKE=slot (docs/RANDOMIZER_PLAN.md step R0.7): the slot binding
// (native/include/pc/pc_rando_hooks.h "R0.7", features/rando/pc_rando_slot.h). The run has its own
// memory card (<run dir>/card/, or COS_CARD_DIR: pc_main.cpp calls prepareRunCard for this smoke), so
// the sidecars it writes never reach the user's card. Not with COS_RANDO_FORCE (it checks "off").
//
// Phase "mem" (the default; needs a debug boot, e.g. --stage sea:44): once the play scene has run 10
// frames, in one frame, with the hooks called in the order d_s_name.cpp calls them (new game:
// pc_rando_slot_new_game_begin, dComIfGs_init, pc_rando_slot_new_game_end; load:
// dComIfGs_setCardToMemory, pc_rando_slot_card_loaded; file select: pc_rando_slot_scene_entry):
//   1. the vanilla debug boot left mReserve all zero and pc_rando() 0;
//   2. a vanilla new game over non-rando bytes in mReserve leaves them as they are (D4);
//   3. a vanilla new game over a stale rando record clears mReserve to zero, pc_rando() stays 0;
//   4. a rando new game (pending seed: COS_RANDO_SLOT_FIXTURE, default the TestSeed1 placement of
//      native/check/rando/fixtures) turns pc_rando() on before dComIfGs_init, consumes the pending
//      seed, writes the record (magic, version, the seed's hash) and the sidecar in the card folder
//      (it parses back to the same hash); pc_rando_loc_item gives the placement; the config follows
//      the seed's options;
//   5. save and reload: memory_to_card into a card buffer, file select (off), a different file in
//      memory, card_to_memory of the buffer and the load hook: pc_rando() is 1 again with the same
//      hash and placement;
//   6. a record whose sidecar is missing, and one whose sidecar (same name) has another hash, load as
//      vanilla (pc_rando() 0);
//   7. a vanilla file saved and reloaded the same way: mReserve all zero, pc_rando() 0.
//
// Phase "load" (COS_RANDO_SLOT_PHASE=load, no --stage; COS_CARD_DIR names the card of a
// `save-sweep` run made with COS_RANDO_PLACEMENT): the real boot, START on the title until the name
// scene runs, then A (file 1, start) until the PLAY scene has the player: pc_rando() must be 1, with
// the seed whose hash the loaded mReserve record holds (COS_RANDO_SLOT_EXPECT=<32 hex>: that hash).
//   run.sh save-sweep --stage sea:44:206 --env COS_RANDO_PLACEMENT=<json> --run-dir <A>
//   run.sh rando --env COS_RANDO_SMOKE=slot --env COS_RANDO_SLOT_PHASE=load --env COS_CARD_DIR=<A>/card
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_slot.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

extern "C" int pc_rando_smoke_wants_run_card(void) {
    const char* smoke = getenv("COS_SMOKE");
    const char* name = getenv("COS_RANDO_SMOKE");
    // R2.1: the loc smoke too (its COS_RANDO_PLACEMENT debug boots write sidecars).
    // R3.1: the newgame smoke writes a sidecar too. R2.4: the evloc smoke (COS_RANDO_PLACEMENT boots).
    // R2.10: the addchest smoke boots with a placement. R3.2: the start smoke (a real new game: an
    // empty card, and the sidecar).
    return smoke != nullptr && strcmp(smoke, "rando") == 0 && name != nullptr &&
           (strcmp(name, "slot") == 0 || strcmp(name, "loc") == 0 || strcmp(name, "newgame") == 0 ||
            strcmp(name, "evloc") == 0 || strcmp(name, "addchest") == 0 || strcmp(name, "start") == 0);
}

namespace pc {

namespace {

constexpr const char* kName = "slot";
constexpr const char* kDefaultFixture = "native/check/rando/fixtures/R0_6_TestSeed1.json";
constexpr unsigned int kStartEvery = 60;
constexpr unsigned int kAEvery = 40;
constexpr unsigned int kLoadFrames = 3600;

u8 sCard[sizeof(card_gamedata)];

uint8_t* reserve() {
    return dComIfGs_getPReserve()->mReserve;
}

bool reserveIs(uint8_t v) {
    for (int i = 0; i < PC_RANDO_SLOT_RECORD_SIZE; i++) {
        if (reserve()[i] != v) {
            return false;
        }
    }
    return true;
}

void newGame() {
    pc_rando_slot_new_game_begin();
    dComIfGs_init();
    pc_rando_slot_new_game_end();
}

void saveToBuffer() {
    memset(sCard, 0, sizeof sCard);
    dComIfGs_setMemoryToCard(sCard, 0);
}

void loadFromBuffer() {
    dComIfGs_setCardToMemory(sCard, 0);
    pc_rando_slot_card_loaded();
}

bool fileExists(const std::string& path) {
    return access(path.c_str(), R_OK) == 0;
}

// The path as given, else from the repository (<repo>/build/native-mac/user is pc_user_path()).
std::string resolve(const char* path) {
    if (path[0] == '/' || fileExists(path)) {
        return path;
    }
    const char* user = pc_user_path();
    return std::string(user != nullptr ? user : ".") + "/../../../" + path;
}

bool sameItems(const PcRandoSeed& s) {
    for (int i = 0; i < PC_RANDO_SEED_LOCATIONS; i++) {
        if (pc_rando_loc_item(i, 0xFE) != s.locations[i]) {
            writef(STDERR_FILENO, "[cos] rando-smoke slot: location %d gives 0x%02X, placement 0x%02X\n", i,
                   pc_rando_loc_item(i, 0xFE), s.locations[i]);
            return false;
        }
    }
    return true;
}

void phaseMem() {
    const char* fixtureEnv = getenv("COS_RANDO_SLOT_FIXTURE");
    const std::string fixture = resolve(fixtureEnv != nullptr && fixtureEnv[0] != '\0' ? fixtureEnv : kDefaultFixture);
    PcRandoSeed seed;
    std::string err;
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &seed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke slot: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    char dirBuf[1024];
    randoSmokeCheck(kName, "card folder known", pc_rando_slot_card_dir(dirBuf, sizeof dirBuf) != 0);
    const std::string dir = dirBuf;
    writef(STDERR_FILENO, "[cos] rando-smoke slot: card folder %s\n", dir.c_str());
    const std::string sidecar = dir + "/" + pc_rando_seed_sidecar_name(seed);
    unlink(sidecar.c_str());

    // 1. the vanilla boot
    randoSmokeCheck(kName, "1 vanilla boot: pc_rando() 0", pc_rando() == 0);
    randoSmokeCheck(kName, "1 vanilla boot: mReserve all zero", reserveIs(0));

    // 2. non-rando bytes are left alone
    memset(reserve(), 0x5A, PC_RANDO_SLOT_RECORD_SIZE);
    newGame();
    randoSmokeCheck(kName, "2 vanilla new game: non-rando mReserve bytes unchanged", reserveIs(0x5A));
    randoSmokeCheck(kName, "2 vanilla new game: pc_rando() 0", pc_rando() == 0);
    memset(reserve(), 0, PC_RANDO_SLOT_RECORD_SIZE);

    // 3. a stale record is cleared
    pc_rando_slot_record_write(reserve(), seed.hash, 0);
    newGame();
    randoSmokeCheck(kName, "3 vanilla new game: stale record cleared (all zero)", reserveIs(0));
    randoSmokeCheck(kName, "3 vanilla new game: pc_rando() 0", pc_rando() == 0);

    // 4. a rando new game
    pc_rando_slot_set_pending(seed);
    pc_rando_slot_new_game_begin();
    randoSmokeCheck(kName, "4 rando new game: pc_rando() 1 before dComIfGs_init", pc_rando() == 1);
    dComIfGs_init();
    pc_rando_slot_new_game_end();
    randoSmokeCheck(kName, "4 rando new game: pending seed consumed", pc_rando_slot_pending() == nullptr);
    uint8_t hash[16] = {};
    uint8_t flags = 0xFF;
    randoSmokeCheck(kName, "4 rando new game: record readable", pc_rando_slot_record_read(reserve(), hash, &flags));
    randoSmokeCheck(kName, "4 rando new game: record hash is the seed's", memcmp(hash, seed.hash, 16) == 0);
    randoSmokeCheck(kName, "4 rando new game: record flags 0", flags == 0);
    randoSmokeCheck(kName, "4 rando new game: record padding zero",
                    memcmp(reserve(), "WWR1\x01\x00\x00\x00", 8) == 0 && [&] {
                        for (int i = 0x18; i < PC_RANDO_SLOT_RECORD_SIZE; i++) {
                            if (reserve()[i] != 0) {
                                return false;
                            }
                        }
                        return true;
                    }());
    PcRandoSeed side;
    randoSmokeCheck(kName, ("4 rando new game: sidecar " + sidecar + " parses").c_str(),
                    pc_rando_seed_load_file(sidecar.c_str(), &side, &err));
    randoSmokeCheck(kName, "4 rando new game: sidecar hash is the seed's", memcmp(side.hash, seed.hash, 16) == 0);
    randoSmokeCheck(kName, "4 rando new game: pc_rando_seed() is the slot's seed",
                    pc_rando_seed() != nullptr && pc_rando_seed() == pc_rando_slot_active_seed());
    randoSmokeCheck(kName, "4 rando new game: pc_rando_loc_item gives the placement", sameItems(seed));
    randoSmokeCheck(kName, "4 rando new game: config from the options (starting magic 16, hearts 12)",
                    pc_rando_cfg()->starting_magic == 16 && pc_rando_cfg()->starting_quarter_hearts == 12 &&
                        pc_rando_cfg()->sword_mode == PC_RANDO_SWORD_START);

    // 5. save, leave, reload
    saveToBuffer();
    pc_rando_slot_scene_entry();
    randoSmokeCheck(kName, "5 file select: pc_rando() 0", pc_rando() == 0);
    newGame(); // another file in memory (clears the record of the rando one)
    randoSmokeCheck(kName, "5 a vanilla file in memory: mReserve zero", reserveIs(0));
    loadFromBuffer();
    randoSmokeCheck(kName, "5 reload: pc_rando() 1", pc_rando() == 1);
    randoSmokeCheck(kName, "5 reload: same hash",
                    pc_rando_seed() != nullptr && memcmp(pc_rando_slot_active_seed()->hash, seed.hash, 16) == 0);
    randoSmokeCheck(kName, "5 reload: pc_rando_loc_item gives the placement", sameItems(seed));

    // 6. records the sidecar does not answer
    pc_rando_slot_scene_entry();
    uint8_t other[16];
    memcpy(other, seed.hash, 16);
    other[0] ^= 0xFF; // another name: no such file
    pc_rando_slot_record_write(reserve(), other, 0);
    pc_rando_slot_card_loaded();
    randoSmokeCheck(kName, "6 missing sidecar: pc_rando() 0", pc_rando() == 0);
    memcpy(other, seed.hash, 16);
    other[15] ^= 0xFF; // the same name, another hash
    pc_rando_slot_record_write(reserve(), other, 0);
    pc_rando_slot_card_loaded();
    randoSmokeCheck(kName, "6 sidecar of another hash: pc_rando() 0", pc_rando() == 0);

    // 7. a vanilla file saved and reloaded
    newGame();
    saveToBuffer();
    pc_rando_slot_scene_entry();
    memset(reserve(), 0, PC_RANDO_SLOT_RECORD_SIZE);
    loadFromBuffer();
    randoSmokeCheck(kName, "7 vanilla reload: pc_rando() 0", pc_rando() == 0);
    randoSmokeCheck(kName, "7 vanilla reload: mReserve all zero", reserveIs(0));
    randoSmokeEnd(kName);
}

void* findScene(void* proc, void* name) {
    return fpcM_GetName(proc) == *(s16*)name ? proc : nullptr;
}

bool sceneRuns(s16 name) {
    return fpcM_Search(findScene, &name) != nullptr;
}

void phaseLoad(unsigned int frames) {
    static int sState = 0; // 0 title, 1 name scene
    static unsigned int sSince = 0;
    ++sSince;
    if (frames > kLoadFrames) {
        randoSmokeCheck(kName, "load: the PLAY scene came up with the player", false);
        randoSmokeEnd(kName);
    }
    if (!randoSmokeInPlay(30)) {
        if (sState == 0 && sceneRuns(fpcNm_NAME_SCENE_e)) {
            writef(STDERR_FILENO, "[cos] rando-smoke slot: frame %u: name scene (file select)\n", frames);
            sState = 1;
            sSince = 0;
        }
        const u16 button = sState == 0 ? PAD_BUTTON_START : PAD_BUTTON_A;
        const unsigned int every = sState == 0 ? kStartEvery : kAEvery;
        setDrivenPad(true, !sceneRuns(fpcNm_PLAY_SCENE_e) && sSince % every < 2 ? button : 0, 0, 0);
        return;
    }
    setDrivenPad(true, 0, 0, 0);
    uint8_t hash[16];
    randoSmokeCheck(kName, "load: mReserve holds a rando record", pc_rando_slot_record_read(reserve(), hash, nullptr));
    randoSmokeCheck(kName, "load: pc_rando() 1", pc_rando() == 1);
    const PcRandoSeed* seed = pc_rando_slot_active_seed();
    const bool same = pc_rando_seed() != nullptr && seed != nullptr && memcmp(seed->hash, hash, 16) == 0;
    randoSmokeCheck(kName, "load: the active seed has the record's hash", same);
    if (const char* expect = getenv("COS_RANDO_SLOT_EXPECT"); expect != nullptr && expect[0] != '\0') {
        randoSmokeCheck(kName, "load: hash is COS_RANDO_SLOT_EXPECT",
                        seed != nullptr && pc_rando_seed_hash_hex(*seed) == expect);
    }
    if (seed != nullptr) {
        writef(STDERR_FILENO, "[cos] rando-smoke slot: loaded seed \"%s\" hash %s\n", seed->seed.c_str(),
               pc_rando_seed_hash_hex(*seed).c_str());
        randoSmokeCheck(kName, "load: pc_rando_loc_item gives the placement", sameItems(*seed));
    }
    randoSmokeEnd(kName);
}

} // namespace

void randoSmokeSlot(unsigned int frames) {
    if (pc_rando_forced()) {
        randoSmokeCheck(kName, "runs without COS_RANDO_FORCE", false);
        randoSmokeEnd(kName);
    }
    const char* phase = getenv("COS_RANDO_SLOT_PHASE");
    if (phase != nullptr && strcmp(phase, "load") == 0) {
        phaseLoad(frames);
        return;
    }
    if (!randoSmokeInPlay(10)) {
        return;
    }
    phaseMem();
}

} // namespace pc
