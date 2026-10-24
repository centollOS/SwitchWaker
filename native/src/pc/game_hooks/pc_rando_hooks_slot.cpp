// The game side of the randomizer's slot binding (docs/RANDOMIZER_PLAN.md step R0.7, decisions D3,
// D4, D7; declarations in native/include/pc/pc_rando_hooks.h, the pure record/pending/activation code
// in features/rando/pc_rando_slot.{h,cpp}):
//   - New Game (dScnName_c::NameInMain around dComIfGs_init): a pending seed is activated before the
//     init, so its init hunks see pc_rando() == 1, and the slot record is written into mReserve after
//     it; a vanilla new file clears a stale record (D4);
//   - load (FileSelectMainNormal after card_to_memory): a record activates its sidecar from the card
//     directory;
//   - the title and the file select turn the randomizer off;
//   - the debug stage boot (pcBootStage) with COS_RANDO_PLACEMENT=<json>.
// Every path leaves a vanilla file's bytes alone: mReserve is written only for a randomized file, and
// cleared only when it holds a rando record, which vanilla code never writes.
// R4.4: every hook runs in a JKRPcHostAllocScope. The seeds they copy (the active seed and what the
// activation listeners build) live across scenes; allocated from the heap the game had current (the
// name scene's), the next activation freed them into a heap that was gone ("Bad Block" panic on the
// first load after a randomized new file).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_slot.h"
#include "pc_internal.h"
#include "pc/pc_game_id.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"

#include <aurora/card.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// The current new file is randomized (set by begin, read by end).
bool sNewGameRando = false;
uint8_t sNewGameFlags = 0;

uint8_t* reserveBytes() {
    return dComIfGs_getPReserve()->mReserve;
}

std::string cardDir() {
    char buf[PATH_MAX];
    return pc_rando_slot_card_dir(buf, sizeof buf) != 0 ? std::string(buf) : std::string();
}

void newGameBegin(const char* who) {
    sNewGameRando = false;
    sNewGameFlags = 0;
    const PcRandoSeed* pending = pc_rando_slot_pending();
    // R8.1: "No" to the file select's Randomizer question makes a normal file; the pending seed
    // stays pending for a later one.
    if (pc_rando_fs_newgame_take_choice() == 1 && pending != nullptr) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: %s: normal file chosen on the file select; seed \"%s\" stays pending\n",
                   who, pending->seed.c_str());
        pending = nullptr;
    }
    if (pending == nullptr) {
        if (pc_rando() && !pc_rando_forced()) {
            pc_rando_deactivate();
        }
        return;
    }
    const std::string dir = cardDir();
    std::string err;
    if (dir.empty()) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: %s: no card folder; the sidecar is not written\n", who);
    } else if (!pc_rando_slot_write_sidecar(dir, *pending, &err)) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: %s: sidecar not written: %s\n", who, err.c_str());
    }
    if (!pc_rando_slot_activate(*pending, &err)) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: %s: seed not activated: %s\n", who, err.c_str());
        pc_rando_slot_clear_pending();
        return;
    }
    pc_rando_slot_clear_pending();
    sNewGameRando = true;
    const PcRandoSeed* seed = pc_rando_slot_active_seed();
    pc::writef(STDERR_FILENO, "[cos] rando slot: %s: randomized new file, seed \"%s\" hash %s\n", who,
               seed->seed.c_str(), pc_rando_seed_hash_hex(*seed).c_str());
}

void newGameEnd(const char* who) {
    uint8_t* reserve = reserveBytes();
    const PcRandoSeed* seed = pc_rando_slot_active_seed();
    if (sNewGameRando && pc_rando() && seed != nullptr && pc_rando_seed() == seed) {
        pc_rando_slot_record_write(reserve, seed->hash, sNewGameFlags);
        pc::writef(STDERR_FILENO, "[cos] rando slot: %s: record written (%s)\n", who,
                   pc_rando_slot_sidecar_name(seed->hash).c_str());
        // R3.1: init_save_with_tweaks (custom_funcs.asm), after the plain init above.
        pc_rando_new_game_init();
    } else if (pc_rando_slot_record_clear_stale(reserve)) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: %s: vanilla new file: stale rando record cleared\n", who);
    }
    sNewGameRando = false;
    sNewGameFlags = 0;
}

} // namespace

extern "C" {

size_t pc_rando_slot_card_dir(char* buf, size_t cap) {
    if (cap == 0) {
        return 0;
    }
    buf[0] = '\0';
    char game[5];
    memcpy(game, COS_GAME_ID_STR, 4);
    game[4] = '\0';
    size_t n = aurora_card_get_path(game, AURORA_CARD_GCI_DIRECTORY, 0, buf, cap);
    if (n == 0 || n > cap || buf[0] == '\0') {
        const char* user = pc_user_path();
        if (user == nullptr || user[0] == '\0') {
            buf[0] = '\0';
            return 0;
        }
        snprintf(buf, cap, "%s/%s/Card A", user, COS_GAME_ID_STR[3] == 'P' ? "EUR" : "USA");
    }
    size_t len = strlen(buf);
    while (len > 1 && buf[len - 1] == '/') {
        buf[--len] = '\0';
    }
    return len;
}

void pc_rando_slot_new_game_begin(void) {
    JKRPcHostAllocScope hostAlloc; // R4.4: see the header
    newGameBegin("new game");
}

void pc_rando_slot_new_game_end(void) {
    JKRPcHostAllocScope hostAlloc; // R4.4: see the header
    newGameEnd("new game");
}

void pc_rando_slot_card_loaded(void) {
    JKRPcHostAllocScope hostAlloc; // R4.4: see the header
    const uint8_t* reserve = reserveBytes();
    if (!pc_rando_slot_record_is_rando(reserve)) {
        if (pc_rando() && !pc_rando_forced()) {
            pc_rando_deactivate();
        }
        return;
    }
    uint8_t hash[16];
    uint8_t flags = 0;
    if (!pc_rando_slot_record_read(reserve, hash, &flags)) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: load: record version %u is not %u; playing vanilla\n",
                   reserve[4], PC_RANDO_SLOT_RECORD_VERSION);
        pc_rando_deactivate();
        return;
    }
    std::string err;
    if (!pc_rando_slot_activate_from_dir(cardDir(), hash, &err)) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: load: %s; playing vanilla\n", err.c_str());
        pc_rando_deactivate();
        return;
    }
    const PcRandoSeed* seed = pc_rando_slot_active_seed();
    pc::writef(STDERR_FILENO, "[cos] rando slot: load: randomized file, seed \"%s\" hash %s%s\n",
               seed->seed.c_str(), pc_rando_seed_hash_hex(*seed).c_str(),
               (flags & PC_RANDO_SLOT_FLAG_DEBUG_BOOT) != 0 ? " (debug boot)" : "");
}

void pc_rando_slot_scene_entry(void) {
    JKRPcHostAllocScope hostAlloc; // R4.4: see the header
    pc_rando_deactivate();
}

void pc_rando_slot_debug_boot_begin(void) {
    JKRPcHostAllocScope hostAlloc; // R4.4: see the header
    const char* path = getenv("COS_RANDO_PLACEMENT");
    if (path == nullptr || path[0] == '\0') {
        newGameBegin("debug boot");
        return;
    }
    PcRandoSeed seed;
    std::string err;
    if (!pc_rando_seed_load_file(path, &seed, &err)) {
        pc::writef(STDERR_FILENO, "[cos] rando slot: COS_RANDO_PLACEMENT: %s\n", err.c_str());
        pc_exit(PC_EXIT_USAGE);
    }
    pc_rando_slot_set_pending(seed);
    newGameBegin("debug boot");
    if (!sNewGameRando) {
        pc_exit(PC_EXIT_USAGE);
    }
    sNewGameFlags = PC_RANDO_SLOT_FLAG_DEBUG_BOOT;
}

void pc_rando_slot_debug_boot_end(void) {
    JKRPcHostAllocScope hostAlloc; // R4.4: see the header
    newGameEnd("debug boot");
}

} // extern "C"
