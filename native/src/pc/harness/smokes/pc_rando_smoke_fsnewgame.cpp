// COS_SMOKE=rando COS_RANDO_SMOKE=fsnewgame (docs/RANDOMIZER_PLAN.md step R8.1): the file select's
// "Normal / Randomizer" question, driven by input scripts through the real boot (title, name scene,
// file select, name entry, PLAY). The run has its own empty memory card (pc_rando_smoke_wants_run_card)
// and its logs in <run dir>/user/rando (COS_RANDO_DIR, set here). COS_RANDO_FSNG_CASE picks the case
// (lines in native/check/regress_rando/R8.1.txt):
//   yes         no pending seed, COS_RANDO_SEED=TestSeed1, input rando-fsnewgame-yes.txt (A on blank
//               slot 1, stick left to Yes, A): the question is asked, the seed of the menu's settings
//               is generated on the worker job during the wait state, the new file is randomized
//               with it: the record in mReserve and the sidecar name its hash, the sidecar holds the
//               generated placement, which is TestSeed1's (fixture R0_6_TestSeed1.json, locations),
//               the pending seed was consumed, the spoiler log is written;
//   yespending  the fixture is the pending seed, the same input: Yes uses it without generating;
//   no          the fixture is the pending seed, input new-game-skip-intro.txt (A, A on the preselected
//               No): a vanilla file (pc_rando() 0, no rando record), the seed is still pending;
//   off         as no with COS_RANDO_ASK_NEWGAME=0: nothing is asked and the file binds the pending
//               seed, as R4.4 (the newfile smoke covers the rest of that flow unchanged);
//   error       every progression category off (set here, so generation fails: "Not enough progress
//               locations..."), the yes input: the error is shown, the error log is written, A goes
//               back to the slot list (the run ends there);
//   occupied    COS_CARD_DIR=<a save-sweep run's card> (slot 1 holds a save), the fixture pending,
//               input rando-fsnewgame-occupied.txt (A on slot 1, A on Start): nothing is asked, the
//               save loads vanilla, the seed is still pending. A two-run sequence:
//                 native/tools/run.sh save-sweep --stage sea:44 --run-dir <A>
//                 native/tools/run.sh rando --input native/check/input/rando-fsnewgame-occupied.txt \
//                     --env COS_RANDO_SMOKE=fsnewgame --env COS_RANDO_FSNG_CASE=occupied \
//                     --env COS_CARD_DIR=<A>/card
// R8.3 (lines in native/check/regress_rando/R8.3.txt; the R8.1 yes/yespending/error lines answer the
// second question "Make a new seed?" with No through rando-fsnewgame-yes.txt). COS_RANDO_FSNG_SEED, when
// set, is stored in COS_RANDO_SEED by the smoke (not by the environment, which would lock the setting):
//   newseed      COS_RANDO_FSNG_SEED=TestSeed1, no pending seed, input rando-fsnewgame-newseed.txt (Yes,
//                then A on the preselected Yes): a new random name, different from TestSeed1, is stored
//                in COS_RANDO_SEED and generated; the file's record and sidecar name its hash;
//   keeppending  the fixture pending and COS_RANDO_FSNG_SEED=OtherSeed, input rando-fsnewgame-yes.txt
//                (Yes, No): the second question showed the pending seed and its hash, the file binds it,
//                nothing is generated, the seed setting is unchanged;
//   keepname     COS_RANDO_FSNG_SEED=TestSeed1, no pending seed, the same input: TestSeed1's seed is
//                generated (the fixture's placement), the seed setting is unchanged;
//   noseed       no seed at all, input rando-fsnewgame-yes1.txt (R8.1's one-question script): no second
//                question, a new random name is stored and generated;
//   back2        COS_RANDO_FSNG_SEED=TestSeed1, input rando-fsnewgame-back2.txt (Yes, B): back to the slot
//                list from the second question, nothing generated (the run ends there).
// Exit 0 when every check held, else 1. It logs the generation time and the wait state's frames.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_fileselect.h"
#include "features/rando/pc_rando_fsnewgame.h"
#include "features/rando/pc_rando_menu.h"
#include "features/rando/pc_rando_options.h"
#include "features/rando/pc_rando_settings.h"
#include "features/rando/pc_rando_sidecars.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_settings.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "fsnewgame";
constexpr const char* kFixture = "native/check/rando/fixtures/R0_6_TestSeed1.json";

enum class Case { Yes, YesPending, No, Off, Error, Occupied, NewSeed, KeepPending, KeepName, NoSeed, Back2 };

bool sStarted = false;
Case sCase = Case::Yes;
PcRandoSeed sFixture;
std::string sRandoDir;
std::string sSeedBefore; // COS_RANDO_SEED when the smoke began (after COS_RANDO_FSNG_SEED)
int sLastProc = -1;
bool sSawBackToList = false;

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

bool samePlacement(const PcRandoSeed& a, const PcRandoSeed& b) {
    return memcmp(a.locations, b.locations, sizeof a.locations) == 0;
}

std::string hash32(const PcRandoSeed& s) {
    char b[16];
    snprintf(b, sizeof b, "%08x", (unsigned)pc_rando_seed_hash32(s));
    return b;
}

bool pendingIsFixture() {
    const PcRandoSeed* p = pc_rando_slot_pending();
    return p != nullptr && memcmp(p->hash, sFixture.hash, 16) == 0;
}

bool reserveRecord(uint8_t hash[16]) {
    return pc_rando_slot_record_read(dComIfGs_getPReserve()->mReserve, hash, nullptr);
}

void logInfo(const char* when) {
    const RandoFsNewgameInfo& i = randoFsNewgameInfo();
    writef(STDERR_FILENO,
           "[cos] rando-smoke fsnewgame: %s: asked %d, not asked %d, no %d, yes %d, back %d, pending used %d, "
           "generated %d, errors %d, wait %d frame(s), %.3f s; second question %d (yes %d, no %d), new names %d "
           "(\"%s\"); title \"%s\" message \"%s\"\n",
           when, i.asked, i.notAsked, i.answeredNo, i.answeredYes, i.backs, i.usedPending, i.generated, i.errors,
           i.waitFrames, i.seconds, i.asked2, i.answered2Yes, i.answered2No, i.newSeeds, i.lastNewSeed.c_str(),
           i.lastTitle.c_str(), i.lastMessage.c_str());
}

void begin() {
    const char* run = getenv("COS_RUN_DIR");
    sRandoDir = std::string(run != nullptr ? run : ".") + "/user/rando";
    setenv("COS_RANDO_DIR", sRandoDir.c_str(), 1);
    const char* c = getenv("COS_RANDO_FSNG_CASE");
    const std::string cs = c != nullptr ? c : "yes";
    if (cs == "yes") {
        sCase = Case::Yes;
    } else if (cs == "yespending") {
        sCase = Case::YesPending;
    } else if (cs == "no") {
        sCase = Case::No;
    } else if (cs == "off") {
        sCase = Case::Off;
    } else if (cs == "error") {
        sCase = Case::Error;
    } else if (cs == "occupied") {
        sCase = Case::Occupied;
    } else if (cs == "newseed") {
        sCase = Case::NewSeed;
    } else if (cs == "keeppending") {
        sCase = Case::KeepPending;
    } else if (cs == "keepname") {
        sCase = Case::KeepName;
    } else if (cs == "noseed") {
        sCase = Case::NoSeed;
    } else if (cs == "back2") {
        sCase = Case::Back2;
    } else {
        randoSmokeCheck(kName, ("known case \"" + cs + "\"").c_str(), false);
        randoSmokeEnd(kName);
    }
    writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: case %s\n", cs.c_str());
    randoSmokeCheck(kName, "not forced (COS_RANDO_FORCE is unset)", !pc_rando_forced());
    std::string err;
    const std::string fixture = resolve(kFixture);
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &sFixture, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    const bool wantAsk = sCase != Case::Off;
    randoSmokeCheck(kName, wantAsk ? "Ask on New Game is on" : "Ask on New Game is off",
                    randoFsNewgameEnabled() == wantAsk);
    if (const char* seed = getenv("COS_RANDO_FSNG_SEED"); seed != nullptr) {
        randoSmokeCheck(kName, "COS_RANDO_SEED is not set by the environment", !pc_settings_locked(kRandoSeedKey));
        pc_settings_set(kRandoSeedKey, seed);
    }
    sSeedBefore = pc_settings_get(kRandoSeedKey);
    writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: seed setting \"%s\"\n", sSeedBefore.c_str());
    if (sCase == Case::NoSeed) {
        randoSmokeCheck(kName, "noseed: the seed setting is empty", sSeedBefore.empty());
    }
    if (sCase == Case::YesPending || sCase == Case::No || sCase == Case::Off || sCase == Case::Occupied ||
        sCase == Case::KeepPending) {
        pc_rando_slot_set_pending(sFixture); // as the menu's Generate leaves it (R4.3)
    }
    if (sCase == Case::Error) {
        for (int id = 0; id < pcr::OPT_COUNT; id++) {
            if (strncmp(pcr::kOptionInfo[id].name, "progression_", 12) == 0 && pcr::settingExposed(id)) {
                pc_settings_set(pcr::settingKey(id).c_str(), "0");
            }
        }
    }
}

void watchFileSelect() {
    const RandoFileSelectInfo& f = randoFileSelectInfo();
    if (f.frame == 0 || f.frame + 1 < pc_frame_count()) {
        return;
    }
    if (f.proc != sLastProc) {
        sLastProc = f.proc;
        writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: file select frame %u: proc %d\n", f.frame, f.proc);
        if ((sCase == Case::Error || sCase == Case::Back2) && randoFsNewgameInfo().backs > 0 && f.proc == 0) {
            sSawBackToList = true;
        }
    }
}

void errorChecks() {
    const RandoFsNewgameInfo& i = randoFsNewgameInfo();
    logInfo("back on the slot list");
    randoSmokeCheck(kName, "error: asked once, Yes, a generation started", i.asked == 1 && i.answeredYes == 1 &&
                                                                               i.generated == 1);
    randoSmokeCheck(kName, "error: the generation failed with wwrando's error",
                    i.errors == 1 && i.lastError.find("Not enough progress locations") != std::string::npos);
    randoSmokeCheck(kName, "error: the panel showed it", i.lastTitle == "The seed could not be made." &&
                                                           i.lastMessage.find("Not enough") == 0);
    randoSmokeCheck(kName, "error: back to the slot list", i.backs == 1 && sSawBackToList);
    const RandoGenLast& l = randoGenLast();
    randoSmokeCheck(kName, "error: the error log is written", !l.errorPath.empty() && exists(l.errorPath));
    randoSmokeCheck(kName, "error: no pending seed, randomizer off", pc_rando_slot_pending() == nullptr && pc_rando() == 0);
}

void back2Checks() {
    const RandoFsNewgameInfo& i = randoFsNewgameInfo();
    logInfo("back on the slot list");
    randoSmokeCheck(kName, "back2: asked, Yes, then the second question", i.asked == 1 && i.answeredYes == 1 &&
                                                                          i.asked2 == 1);
    randoSmokeCheck(kName, "back2: the second question showed the seed setting's name",
                    i.lastTitle == "Make a new seed?" && i.lastMessage.find("Current seed: TestSeed1") == 0);
    randoSmokeCheck(kName, "back2: B went back to the slot list", i.backs == 1 && sSawBackToList &&
                                                                   i.answered2Yes == 0 && i.answered2No == 0);
    randoSmokeCheck(kName, "back2: nothing generated, no pending seed, randomizer off",
                    i.generated == 0 && i.newSeeds == 0 && !randoGenBusy() && !randoGenLast().have &&
                        pc_rando_slot_pending() == nullptr && pc_rando() == 0);
    randoSmokeCheck(kName, "back2: the seed setting is unchanged", sSeedBefore == pc_settings_get(kRandoSeedKey));
}

// R8.3: a generated seed is the file's: active, its record in mReserve, its sidecar on the card, the
// pending seed consumed, the generation named `seed`.
void generatedFileChecks(const char* c, const std::string& seed, bool record, const uint8_t h[16]) {
    const RandoGenLast& l = randoGenLast();
    const PcRandoSeed* a = pc_rando_slot_active_seed();
    const std::string dir = cardDir();
    writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: generated \"%s\" (%s) in %.3f s\n", l.seed.c_str(),
           l.hash.c_str(), l.seconds);
    randoSmokeCheck(kName, (std::string(c) + ": the generation of \"" + seed + "\" ended ok").c_str(),
                    l.have && l.ok && l.seed == seed);
    randoSmokeCheck(kName, (std::string(c) + ": pc_rando() 1 with that seed").c_str(),
                    pc_rando() == 1 && a != nullptr && a->seed == seed && l.hash32 == hash32(*a));
    randoSmokeCheck(kName, (std::string(c) + ": mReserve holds its record").c_str(),
                    record && a != nullptr && memcmp(h, a->hash, 16) == 0);
    PcRandoSeed onCard;
    std::string err;
    const std::string sidecar = a != nullptr ? dir + "/" + pc_rando_seed_sidecar_name(*a) : "";
    randoSmokeCheck(kName, (std::string(c) + ": the sidecar on the card holds that seed").c_str(),
                    a != nullptr && !dir.empty() && pc_rando_seed_load_file(sidecar.c_str(), &onCard, &err) &&
                        memcmp(onCard.hash, a->hash, 16) == 0 && samePlacement(onCard, *a));
    randoSmokeCheck(kName, (std::string(c) + ": the pending seed was consumed").c_str(),
                    pc_rando_slot_pending() == nullptr);
}

void playChecks() {
    const RandoFsNewgameInfo& i = randoFsNewgameInfo();
    logInfo("in play");
    uint8_t h[16];
    const bool record = reserveRecord(h);
    const std::string dir = cardDir();
    switch (sCase) {
    case Case::Yes: {
        randoSmokeCheck(kName, "yes: asked once, answered Yes, a seed generated",
                        i.asked == 1 && i.answeredYes == 1 && i.generated == 1 && i.usedPending == 0 && i.errors == 0);
        randoSmokeCheck(kName, "yes: the question showed the menu's seed", i.lastMessage.find("TestSeed1") != std::string::npos);
        const RandoGenLast& l = randoGenLast();
        writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: generation time %.3f s, wait state %d frame(s)\n",
               l.seconds, i.waitFrames);
        randoSmokeCheck(kName, "yes: the generation ended ok", l.have && l.ok && l.seed == "TestSeed1");
        const PcRandoSeed* a = pc_rando_slot_active_seed();
        randoSmokeCheck(kName, "yes: pc_rando() 1 with the generated seed",
                        pc_rando() == 1 && a != nullptr && a->seed == "TestSeed1" &&
                            l.hash32 == hash32(*a));
        randoSmokeCheck(kName, "yes: its placement is TestSeed1's", a != nullptr && samePlacement(*a, sFixture));
        randoSmokeCheck(kName, "yes: mReserve holds its record", record && a != nullptr && memcmp(h, a->hash, 16) == 0);
        const std::string sidecar = a != nullptr ? dir + "/" + pc_rando_seed_sidecar_name(*a) : "";
        PcRandoSeed onCard;
        std::string err;
        randoSmokeCheck(kName, "yes: the sidecar on the card holds that seed",
                        a != nullptr && !dir.empty() && pc_rando_seed_load_file(sidecar.c_str(), &onCard, &err) &&
                            memcmp(onCard.hash, a->hash, 16) == 0 && samePlacement(onCard, *a));
        randoSmokeCheck(kName, "yes: the pending seed was consumed", pc_rando_slot_pending() == nullptr);
        randoSmokeCheck(kName, "yes: the spoiler log is written", !l.spoilerPath.empty() && exists(l.spoilerPath));
        break;
    }
    case Case::YesPending: {
        randoSmokeCheck(kName, "yespending: asked once, Yes, the pending seed used, nothing generated",
                        i.asked == 1 && i.answeredYes == 1 && i.usedPending == 1 && i.generated == 0);
        randoSmokeCheck(kName, "yespending: the question showed its seed and hash",
                        i.lastMessage.find(sFixture.seed) != std::string::npos);
        randoSmokeCheck(kName, "yespending: pc_rando() 1 with the fixture's hash",
                        pc_rando() == 1 && pc_rando_slot_active_seed() != nullptr &&
                            memcmp(pc_rando_slot_active_seed()->hash, sFixture.hash, 16) == 0);
        randoSmokeCheck(kName, "yespending: mReserve holds its record", record && memcmp(h, sFixture.hash, 16) == 0);
        randoSmokeCheck(kName, "yespending: the sidecar is on the card",
                        !dir.empty() && exists(dir + "/" + pc_rando_seed_sidecar_name(sFixture)));
        randoSmokeCheck(kName, "yespending: the pending seed was consumed", pc_rando_slot_pending() == nullptr);
        break;
    }
    case Case::No:
        randoSmokeCheck(kName, "no: asked once, answered No", i.asked == 1 && i.answeredNo == 1 && i.answeredYes == 0);
        randoSmokeCheck(kName, "no: a vanilla file (pc_rando() 0, no rando record)", pc_rando() == 0 && !record);
        randoSmokeCheck(kName, "no: the seed is still pending", pendingIsFixture());
        break;
    case Case::Off:
        randoSmokeCheck(kName, "off: nothing asked", i.asked == 0 && i.notAsked >= 1);
        randoSmokeCheck(kName, "off: the pending seed is bound as before (R4.4)",
                        pc_rando() == 1 && record && memcmp(h, sFixture.hash, 16) == 0 &&
                            pc_rando_slot_pending() == nullptr);
        break;
    case Case::Occupied:
        randoSmokeCheck(kName, "occupied: nothing asked", i.asked == 0 && i.notAsked >= 1);
        randoSmokeCheck(kName, "occupied: the save loads vanilla", pc_rando() == 0 && !record);
        randoSmokeCheck(kName, "occupied: the seed is still pending", pendingIsFixture());
        break;
    case Case::NewSeed: {
        randoSmokeCheck(kName, "newseed: asked, Yes, second question Yes",
                        i.asked == 1 && i.answeredYes == 1 && i.asked2 == 1 && i.answered2Yes == 1 &&
                            i.answered2No == 0 && i.generated == 1 && i.usedPending == 0 && i.errors == 0);
        const std::string now = pc_settings_get(kRandoSeedKey);
        writef(STDERR_FILENO, "[cos] rando-smoke fsnewgame: seed setting \"%s\" -> \"%s\"\n", sSeedBefore.c_str(),
               now.c_str());
        randoSmokeCheck(kName, "newseed: a new random name, different from the setting's, stored in COS_RANDO_SEED",
                        i.newSeeds == 1 && !i.lastNewSeed.empty() && i.lastNewSeed != sSeedBefore &&
                            now == i.lastNewSeed);
        generatedFileChecks("newseed", i.lastNewSeed, record, h);
        const PcRandoSeed* a = pc_rando_slot_active_seed();
        randoSmokeCheck(kName, "newseed: not TestSeed1's placement", a != nullptr && !samePlacement(*a, sFixture));
        break;
    }
    case Case::KeepPending:
        randoSmokeCheck(kName, "keeppending: asked, Yes, second question No, the pending seed used",
                        i.asked == 1 && i.answeredYes == 1 && i.asked2 == 1 && i.answered2No == 1 &&
                            i.usedPending == 1 && i.generated == 0 && i.newSeeds == 0);
        randoSmokeCheck(kName, "keeppending: the second question showed the pending seed and its hash",
                        i.lastTitle == "Make a new seed?" &&
                            i.lastMessage.find("Current seed: " + sFixture.seed) == 0 &&
                            i.lastMessage.find(pcr::seedHashWords(sFixture)) != std::string::npos);
        randoSmokeCheck(kName, "keeppending: pc_rando() 1 with the fixture's hash, its record and sidecar",
                        pc_rando() == 1 && pc_rando_slot_active_seed() != nullptr &&
                            memcmp(pc_rando_slot_active_seed()->hash, sFixture.hash, 16) == 0 && record &&
                            memcmp(h, sFixture.hash, 16) == 0 && !dir.empty() &&
                            exists(dir + "/" + pc_rando_seed_sidecar_name(sFixture)));
        randoSmokeCheck(kName, "keeppending: the pending seed was consumed", pc_rando_slot_pending() == nullptr);
        randoSmokeCheck(kName, "keeppending: the seed setting is unchanged", sSeedBefore == pc_settings_get(kRandoSeedKey));
        break;
    case Case::KeepName: {
        randoSmokeCheck(kName, "keepname: asked, Yes, second question No, a generation",
                        i.asked == 1 && i.answeredYes == 1 && i.asked2 == 1 && i.answered2No == 1 &&
                            i.usedPending == 0 && i.generated == 1 && i.newSeeds == 0 && i.errors == 0);
        randoSmokeCheck(kName, "keepname: the second question showed the setting's name",
                        i.question2Message.find("Current seed: TestSeed1") == 0);
        generatedFileChecks("keepname", "TestSeed1", record, h);
        const PcRandoSeed* a = pc_rando_slot_active_seed();
        randoSmokeCheck(kName, "keepname: its placement is TestSeed1's", a != nullptr && samePlacement(*a, sFixture));
        randoSmokeCheck(kName, "keepname: the seed setting is unchanged", sSeedBefore == pc_settings_get(kRandoSeedKey));
        break;
    }
    case Case::NoSeed: {
        randoSmokeCheck(kName, "noseed: asked, Yes, no second question, a new seed generated",
                        i.asked == 1 && i.answeredYes == 1 && i.asked2 == 0 && i.generated == 1 &&
                            i.newSeeds == 1 && i.errors == 0);
        const std::string now = pc_settings_get(kRandoSeedKey);
        randoSmokeCheck(kName, "noseed: the new name is stored in COS_RANDO_SEED", !now.empty() && now == i.lastNewSeed);
        generatedFileChecks("noseed", i.lastNewSeed, record, h);
        break;
    }
    case Case::Error:
    case Case::Back2:
        break;
    }
}

} // namespace

void randoSmokeFsnewgame(unsigned int frames) {
    (void)frames;
    JKRPcHostAllocScope hostAlloc; // its strings and seeds outlive the current game heap
    if (!sStarted) {
        sStarted = true;
        begin();
    }
    if (sCase == Case::Error || sCase == Case::Back2) {
        watchFileSelect();
        if (sSawBackToList) {
            if (sCase == Case::Error) {
                errorChecks();
            } else {
                back2Checks();
            }
            randoSmokeEnd(kName);
        }
        return;
    }
    if (!randoSmokeInPlay(30)) {
        watchFileSelect();
        return;
    }
    playChecks();
    randoSmokeEnd(kName);
}

} // namespace pc
