// COS_SMOKE=rando COS_RANDO_SMOKE=newfile (docs/RANDOMIZER_PLAN.md step R4.4, the plan's target
// "rando-newgame"): a new file binds the pending seed, the file select shows randomized slots, a
// reload and a copied slot are randomized, and "Clean unused seed files" removes only orphans.
//   native/tools/run.sh rando --input native/check/input/new-game-skip-intro.txt \
//       --env COS_RANDO_SMOKE=newfile
// No --stage: the real boot, on the run's own empty card (pc_rando_smoke_wants_run_card). Not with
// COS_RANDO_FORCE.
//
// 1. First frame: the pending seed is set (COS_RANDO_NEWFILE_FIXTURE, default the TestSeed1
//    placement), as the menu's Generate does (R4.3); pc_rando() is 0.
// 2. File select (input script): the hook in dFile_select_c::_draw sees the blank slot 1 selected and
//    gives the line "New file: randomizer seed TestSeed1 (Ivan Woods)" (in the menu's language); the
//    ImGui overlay draws it.
// 3. Once the PLAY scene runs: R0.7's New Game hook consumed the pending seed (none left), pc_rando()
//    is 1 with the fixture's hash, mReserve holds its record, the sidecar is in the card folder.
// 4. In that frame, the save and the file select, as the game does them on its card buffer
//    (3 card_gamedata): memory_to_card into slot 1 with its checksum; the file select's copy (memcpy
//    of the card_gamedata, checksum) into slot 2; slot 3 left blank. The file-select hook then gives:
//    slot 1 and slot 2 "Randomizer: TestSeed1 · Ivan Woods"; blank slot 3 nothing (no pending seed)
//    and the pending line with one; a vanilla slot (record cleared) nothing; a record whose sidecar is
//    missing the "missing: plays vanilla" line; nothing in the end-of-game save procs.
// 5. Reload: the file select turns the randomizer off; card_to_memory of slot 2 (the copy) and R0.7's
//    load hook: pc_rando() is 1 with the same hash and placement; slot 1 the same; the vanilla slot
//    plays vanilla.
// 6. Clean unused seed files (the menu row's action): with an orphan sidecar and an unrelated file
//    added to the card folder, it removes the orphan only and keeps the active seed's sidecar; the
//    same pure clean over a scratch folder holding the card buffer above as a .gci keeps TestSeed1's
//    sidecar with no keep list (the slots' records name it) and removes an orphan.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_fileselect.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_sidecars.h"
#include "features/rando/pc_rando_slot.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "newfile";
constexpr const char* kDefaultFixture = "native/check/rando/fixtures/R0_6_TestSeed1.json";
constexpr const char* kWords = "Ivan Woods"; // TestSeed1 (fixtures/R1_2_permalinks.txt)

bool sStarted = false;
PcRandoSeed sSeed;
bool sSawPending = false;
bool sSawFileSelect = false;
std::string sPendingText;
u8 sCard[3 * sizeof(card_gamedata)];

std::string resolve(const char* path) {
    if (path[0] == '/' || access(path, R_OK) == 0) {
        return path;
    }
    const char* user = pc_user_path();
    return std::string(user != nullptr ? user : ".") + "/../../../" + path;
}

std::string cardDir() {
    char buf[PATH_MAX];
    return pc_rando_slot_card_dir(buf, sizeof buf) != 0 ? std::string(buf) : std::string();
}

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool writeBytes(const std::string& path, const void* data, size_t n) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const std::string& x : v) {
        if (x == s) {
            return true;
        }
    }
    return false;
}

bool sameHash(const PcRandoSeed* s) {
    return s != nullptr && memcmp(s->hash, sSeed.hash, 16) == 0;
}

bool samePlacement() {
    for (int i = 0; i < PC_RANDO_SEED_LOCATIONS; i++) {
        if (pc_rando_loc_item(i, 0xFE) != sSeed.locations[i]) {
            writef(STDERR_FILENO, "[cos] rando-smoke newfile: location %d: %u, placement %u\n", i,
                   pc_rando_loc_item(i, 0xFE), sSeed.locations[i]);
            return false;
        }
    }
    return true;
}

const RandoFileSelectInfo& infoFor(u8* card, int slot, int proc) {
    pc_rando_file_select_draw(card, (uint8_t)slot, (uint8_t)proc);
    const RandoFileSelectInfo& i = randoFileSelectInfo();
    writef(STDERR_FILENO, "[cos] rando-smoke newfile: file select slot %d proc %d: kind %d \"%s\"\n", slot + 1, proc,
           (int)i.kind, i.text.c_str());
    return i;
}

void begin() {
    if (!randoSmokeCheck(kName, "not forced (COS_RANDO_FORCE is unset)", !pc_rando_forced())) {
        randoSmokeEnd(kName);
    }
    const char* env = getenv("COS_RANDO_NEWFILE_FIXTURE");
    const std::string fixture = resolve(env != nullptr && env[0] != '\0' ? env : kDefaultFixture);
    std::string err;
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &sSeed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke newfile: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    pc_rando_slot_set_pending(sSeed);
    randoSmokeCheck(kName, "1 boot: pc_rando() 0, a pending seed", pc_rando() == 0 && pc_rando_slot_pending() != nullptr);
}

void watchFileSelect() {
    const RandoFileSelectInfo& i = randoFileSelectInfo();
    if (i.frame == 0 || i.frame + 1 < pc_frame_count()) {
        return;
    }
    sSawFileSelect = true;
    static int lastKind = -1, lastSlot = -1, lastProc = -1;
    if ((int)i.kind != lastKind || i.slot != lastSlot || i.proc != lastProc) {
        lastKind = (int)i.kind;
        lastSlot = i.slot;
        lastProc = i.proc;
        writef(STDERR_FILENO, "[cos] rando-smoke newfile: file select frame %u: slot %d proc %d kind %d \"%s\"\n", i.frame,
               i.slot + 1, i.proc, (int)i.kind, i.text.c_str());
    }
    if (!sSawPending && i.slot == 0 && i.kind == pcr::FileInfoKind::Pending) {
        sSawPending = true;
        sPendingText = i.text;
        writef(STDERR_FILENO, "[cos] rando-smoke newfile: file select (frame %u, proc %d): \"%s\"\n", i.frame, i.proc,
               i.text.c_str());
    }
}

void playChecks() {
    using K = pcr::FileInfoKind;
    const bool es = randoFileSelectSpanish(); // R7.6a: the game's language (English on USA)
    const RandoFileSelectInfo& fs = randoFileSelectInfo();
    randoSmokeCheck(kName, "2 file select: the hook ran", sSawFileSelect);
    randoSmokeCheck(kName, "2 file select: blank slot 1 shows the pending seed's line",
                    sSawPending && sPendingText == pcr::fileInfoText(pcr::FileInfoKind::Pending, sSeed.seed, kWords, "", es));
    writef(STDERR_FILENO, "[cos] rando-smoke newfile: the overlay drew the line in %u frame(s)\n", fs.shownFrames);
    randoSmokeCheck(kName, "2 file select: the overlay drew it", fs.shownFrames > 0);

    randoSmokeCheck(kName, "3 new game: the pending seed was consumed", pc_rando_slot_pending() == nullptr);
    randoSmokeCheck(kName, "3 new game: pc_rando() 1 with the fixture's hash", pc_rando() == 1 && sameHash(pc_rando_slot_active_seed()));
    uint8_t h[16];
    randoSmokeCheck(kName, "3 new game: mReserve holds the record",
                    pc_rando_slot_record_read(dComIfGs_getPReserve()->mReserve, h, nullptr) && memcmp(h, sSeed.hash, 16) == 0);
    const std::string dir = cardDir();
    const std::string sidecar = dir + "/" + pc_rando_seed_sidecar_name(sSeed);
    randoSmokeCheck(kName, "3 new game: the sidecar is in the card folder", !dir.empty() && exists(sidecar));

    // 4. save into slot 1, copy to slot 2 (d_file_select.cpp CommandExec), slot 3 blank
    memset(sCard, 0, sizeof sCard);
    dComIfGs_setMemoryToCard(sCard, 0);
    mDoMemCdRWm_SetCheckSumGameData(sCard, 0);
    memcpy(sCard + sizeof(card_gamedata), sCard, sizeof(card_gamedata));
    mDoMemCdRWm_SetCheckSumGameData(sCard, 1);
    const std::string line = pcr::fileInfoText(K::Rando, sSeed.seed, kWords, "", es);
    const RandoFileSelectInfo* i = &infoFor(sCard, 0, 3);
    randoSmokeCheck(kName, "4 file select: slot 1 shows its seed", i->kind == K::Rando && i->text == line);
    i = &infoFor(sCard, 1, 3);
    randoSmokeCheck(kName, "4 file select: the copy in slot 2 shows the same seed", i->kind == K::Rando && i->text == line);
    i = &infoFor(sCard, 1, 0);
    randoSmokeCheck(kName, "4 file select: in the slot list too", i->kind == K::Rando);
    i = &infoFor(sCard, 2, 3);
    randoSmokeCheck(kName, "4 file select: blank slot 3, no pending seed: nothing", i->kind == K::None && i->text.empty());
    pc_rando_slot_set_pending(sSeed);
    i = &infoFor(sCard, 2, 0);
    randoSmokeCheck(kName, "4 file select: blank slot 3 with a pending seed: its line", i->kind == K::Pending);
    pc_rando_slot_clear_pending();
    i = &infoFor(sCard, 0, 0x1A);
    randoSmokeCheck(kName, "4 file select: nothing in the end-of-game save procs", i->kind == K::None);
    static u8 other[3 * sizeof(card_gamedata)];
    memcpy(other, sCard, sizeof other);
    dSv_save_c_PACKED* o0 = (dSv_save_c_PACKED*)other;
    memset(o0->mReserve.mReserve, 0, PC_RANDO_SLOT_RECORD_SIZE);
    mDoMemCdRWm_SetCheckSumGameData(other, 0);
    i = &infoFor(other, 0, 3);
    randoSmokeCheck(kName, "4 file select: a vanilla slot: nothing", i->kind == K::None && i->text.empty());
    uint8_t bogus[16];
    for (int k = 0; k < 16; k++) {
        bogus[k] = (uint8_t)(0xA0 + k);
    }
    dSv_save_c_PACKED* o1 = (dSv_save_c_PACKED*)(other + sizeof(card_gamedata));
    pc_rando_slot_record_write(o1->mReserve.mReserve, bogus, 0);
    mDoMemCdRWm_SetCheckSumGameData(other, 1);
    i = &infoFor(other, 1, 3);
    randoSmokeCheck(kName, "4 file select: a record without its sidecar: plays vanilla",
                    i->kind == K::Missing && i->text == pcr::fileInfoText(K::Missing, "", "", pc_rando_slot_sidecar_name(bogus), es));

    // 5. reload
    pc_rando_slot_scene_entry();
    randoSmokeCheck(kName, "5 file select: off", pc_rando() == 0);
    dComIfGs_setCardToMemory(sCard, 1);
    pc_rando_slot_card_loaded();
    randoSmokeCheck(kName, "5 load slot 2 (the copy): randomized, same hash and placement",
                    pc_rando() == 1 && sameHash(pc_rando_seed()) && samePlacement());
    pc_rando_slot_scene_entry();
    dComIfGs_setCardToMemory(other, 0);
    pc_rando_slot_card_loaded();
    randoSmokeCheck(kName, "5 load the vanilla slot: vanilla", pc_rando() == 0);
    pc_rando_slot_scene_entry();
    dComIfGs_setCardToMemory(sCard, 0);
    pc_rando_slot_card_loaded();
    randoSmokeCheck(kName, "5 load slot 1: randomized, same hash and placement",
                    pc_rando() == 1 && sameHash(pc_rando_seed()) && samePlacement());

    // 6. clean unused seed files
    const std::string orphan = "switchwaker-rando-0badf00d.json";
    const std::string unrelated = "switchwaker-rando-notes.txt";
    writeBytes(dir + "/" + orphan, "{}\n", 3);
    writeBytes(dir + "/" + unrelated, "x", 1);
    pcr::SidecarCleanResult r;
    const std::string toast = randoCleanSidecars(false, &r);
    writef(STDERR_FILENO, "[cos] rando-smoke newfile: clean: \"%s\" (%d gci, %d records)\n", toast.c_str(), r.gciFiles,
           r.records);
    randoSmokeCheck(kName, "6 clean: the orphan removed", r.ok && r.removed.size() == 1 && r.removed[0] == orphan &&
                                                              !exists(dir + "/" + orphan));
    randoSmokeCheck(kName, "6 clean: the active seed's sidecar kept", contains(r.kept, pc_rando_seed_sidecar_name(sSeed)) &&
                                                                         exists(sidecar));
    randoSmokeCheck(kName, "6 clean: other files untouched", exists(dir + "/" + unrelated));
    randoSmokeCheck(kName, "6 clean: the toast", toast == "1 seed file(s) removed, 1 in use");
    unlink((dir + "/" + unrelated).c_str());

    const std::string scratch = dir + "/../R4.4-clean";
    mkdir(scratch.c_str(), 0755);
    static u8 gci[0x40 + 3 * 0x2000];
    memset(gci, 0, sizeof gci);
    memcpy(gci + 0x40 + 0x2000 + 8, sCard, sizeof sCard); // card_savedata copy 1: gamedata at +8
    std::string json;
    const bool wrote = writeBytes(scratch + "/01-GZLE-gczelda.gci", gci, sizeof gci) &&
                       pc_rando_seed_save_file((scratch + "/" + pc_rando_seed_sidecar_name(sSeed)).c_str(), sSeed, &json) &&
                       writeBytes(scratch + "/" + orphan, "{}\n", 3);
    const pcr::SidecarCleanResult s = pcr::sidecarClean(scratch, {}, false);
    randoSmokeCheck(kName, "6 clean (card buffer as a .gci, no keep list): the slots' sidecar kept, the orphan removed",
                    wrote && s.ok && s.records == 1 && s.kept.size() == 1 &&
                        s.kept[0] == pc_rando_seed_sidecar_name(sSeed) && s.removed.size() == 1 && s.removed[0] == orphan);
}

} // namespace

void randoSmokeNewfile(unsigned int frames) {
    (void)frames;
    JKRPcHostAllocScope hostAlloc; // its strings and seeds outlive the current game heap
    if (!sStarted) {
        sStarted = true;
        begin();
    }
    if (!randoSmokeInPlay(30)) {
        watchFileSelect();
        return;
    }
    playChecks();
    randoSmokeEnd(kName);
}

} // namespace pc
