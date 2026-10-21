// COS_SMOKE=rando COS_RANDO_SMOKE=charts (docs/RANDOMIZER_PLAN.md step R6.6): randomized charts
// (features/rando/pc_rando_charts.h). Boot on the boat at Outset:
//   run.sh rando --stage sea:44:206 --preset sailing --env COS_RANDO_SMOKE=charts
//                [--env COS_CHARTS_SEED=native/check/rando/fixtures/R6_6_seed_A.json]
//                [--env COS_CHARTS_VANILLA=1]
// Once the play scene has run 10 frames the smoke activates the seed COS_CHARTS_SEED (a
// randomize_charts placement; relative paths from the repository root; default R6_6_seed_A.json)
// as a loaded randomized slot does (pc_rando_slot_activate), unless COS_CHARTS_VANILLA=1 (pc_rando()
// stays 0: all must stay vanilla), and sails to sea room 44 again (point 100, on the boat) so that
// its room.dzr is loaded under the seed. (A debug boot with COS_RANDO_PLACEMENT at sea room 44 hits
// a PACKAGE event assert in dEvDtStaff_c::specialProcPackage, with any placement, also R0.6's
// identity; not a charts problem, so the smoke does not boot that way.) 30 frames after arriving,
// with C the seed's chart for island 44 (Outset; vanilla: Triforce Chart 4) and k its owned index
// (0xFF - item id):
//   1. the salvage: sea/Room44's salvage_type 0 SCOBs were patched at load (4 points, index k), and
//      the salvage registry (daSalvage_c::mTagData_p) holds Outset's sunken treasure point with save
//      number k (vanilla: 4, and the patcher never ran);
//   2. the sea chart: dMenu_Fmap_c::aramCmapDatRead (the real function, on a menu object that is
//      never constructed: it only reads the resource and sets mCmapDatPnt) reads CmapDat.bin from
//      fmapres.arc through the R6.6 hunk; C's record (found as the menu does,
//      aramCmapDat_c::getCmapDatPnt4) now has Outset's sector (-2, 3), and the texture id and the 4
//      random positions of Outset's vanilla record, its owned index stays k; the vanilla chart of
//      Outset (Triforce Chart 4) points to the island the seed gives it; the other chart records
//      are the seed's islands too (all 49 sectors checked);
//   3. a second aramCmapDatRead on the still-loaded resource changes nothing (ALREADY).
// Vanilla: the hook never runs and Triforce Chart 4 still points to Outset. The resource is then
// removed again, as dMenu_Fmap_c::_delete does. Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "features/rando/pc_rando_charts.h"
#include "features/rando/pc_rando_data.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "d/actor/d_a_salvage.h"
#include "d/d_com_inf_game.h"
#include "d/d_menu_fmap.h"
#include "d/d_menu_fmapSv.h"
#include "d/d_salvage.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>
#include <cstdio>

#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "charts";
constexpr int kRoom = 44; // Outset

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

std::string seedChart(const PcRandoSeed* seed, int island) {
    if (seed == nullptr) {
        return pcr::default_chart_for_island(island);
    }
    for (const auto& c : seed->charts) {
        if (c.first == island) {
            return c.second;
        }
    }
    return std::string();
}

// The CmapDat record of chart `number`, found as dMenu_Fmap_c does (aramCmapDat_c::getCmapDatPnt4).
// The table's count is a big-endian int that the port reads in host order (the game's struct has
// a plain int), so the lookup runs on a copy of the count in host order.
const uint8_t* recordOf(void* pat, int number) {
    struct HostPat {
        int num;
        aramCmapDatPnt_t pnt[1];
    };
    static_assert(sizeof(aramCmapDatPnt_t) == PCR_CMAP_RECORD_SIZE, "record size");
    const int n = (int)rd32((const uint8_t*)pat);
    const int saved = *(int*)pat;
    *(int*)pat = n;
    aramCmapDat_c dat;
    dat.init((aramCmapDatPat_t*)pat);
    aramCmapDatPnt_t* p = dat.getCmapDatPnt4(number - 1);
    *(int*)pat = saved;
    return (const uint8_t*)p;
}

constexpr int kSeaPoint = 100; // a corner spawn of every sea room (mode 2: on the boat)
constexpr unsigned int kArriveFrames = 1200;
constexpr unsigned int kSettleFrames = 30;
constexpr unsigned int kTalkFrames = 30;

enum State { kOff, kArrive, kSettle, kDone };
State sState = kOff;
bool sVanilla = false;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
fpc_ProcID sScene = fpcM_ERROR_PROCESS_ID_e;
PcRandoSeed sSeed;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* s = fpcM_Search(isPlayScene, nullptr);
    return s != nullptr ? fpcM_GetID(s) : fpcM_ERROR_PROCESS_ID_e;
}

bool readText(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        out->append(buf, n);
    }
    fclose(f);
    return true;
}

// The seed file: as given, else from the repository root (<root>/native/src/pc/harness/smokes/<this>).
bool loadSeed() {
    const char* p = getenv("COS_CHARTS_SEED");
    const std::string path = p != nullptr && p[0] != '\0' ? p : "native/check/rando/fixtures/R6_6_seed_A.json";
    std::string text;
    bool ok = readText(path, &text);
    if (!ok && path[0] != '/') {
        std::string root = __FILE__;
        for (int up = 0; up < 6; up++) {
            const size_t slash = root.find_last_of('/');
            root.resize(slash == std::string::npos ? 0 : slash);
        }
        ok = !root.empty() && readText(root + "/" + path, &text);
    }
    if (!randoSmokeCheck(kName, "seed file read", ok)) {
        return false;
    }
    std::string err;
    if (!randoSmokeCheck(kName, "seed parsed", pc_rando_seed_parse(text, &sSeed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke charts: %s\n", err.c_str());
        return false;
    }
    const bool on = pc_rando_slot_activate(sSeed, &err);
    if (!on) {
        writef(STDERR_FILENO, "[cos] rando-smoke charts: activate: %s\n", err.c_str());
    }
    return randoSmokeCheck(kName, "seed activated", on);
}

void run() {
    const bool vanilla = sVanilla;
    const PcRandoSeed* seed = pc_rando_seed();
    char what[160];
    randoSmokeCheck(kName, vanilla ? "pc_rando() off" : "pc_rando() on with a seed",
                    vanilla ? pc_rando() == 0 : pc_rando() == 1 && seed != nullptr);
    if (!vanilla && seed != nullptr) {
        const pcr::Json* rc = seed->options.find("randomize_charts");
        randoSmokeCheck(kName, "the seed has randomize_charts", rc != nullptr && rc->isBool() && rc->asBool());
    }
    const std::string chart = seedChart(vanilla ? nullptr : seed, kRoom);
    const int number = pc_rando_chart_number(chart.c_str());
    const int k = pc_rando_chart_owned_index(chart.c_str());
    writef(STDERR_FILENO, "[cos] rando-smoke charts: Outset's chart is %s (number %d, index %d)\n", chart.c_str(),
           number, k);
    if (!vanilla) {
        randoSmokeCheck(kName, "Outset's chart is not the vanilla Triforce Chart 4", chart != "Triforce Chart 4");
    }

    // 1. The salvage.
    if (vanilla) {
        randoSmokeCheck(kName, "salvage: room 44 not patched", pc_rando_charts_room_status(kRoom) == PCR_CHARTS_NOT_RUN);
    } else {
        snprintf(what, sizeof what, "salvage: room 44 patched, 4 points with index %d (status %d, %d point(s), index %d)",
                 k, pc_rando_charts_room_status(kRoom), pc_rando_charts_room_written(kRoom),
                 pc_rando_charts_room_index(kRoom));
        randoSmokeCheck(kName, what,
                        pc_rando_charts_room_status(kRoom) == PCR_CHARTS_PATCHED &&
                            pc_rando_charts_room_written(kRoom) == 4 && pc_rando_charts_room_index(kRoom) == k);
    }
    {
        dSalvage_control_c* c = daSalvage_c::mTagData_p;
        int found = -1;
        for (int i = 0; c != nullptr && i < dSalvage_control_c::MAX_TOTAL; i++) {
            if (c->checkRegist(i) && c->mInfo[i].mRoomNo == kRoom && c->getKind(i) == 0) {
                found = i;
                break;
            }
        }
        const int save = found >= 0 ? (int)c->getSaveNo(found) : -1;
        snprintf(what, sizeof what, "salvage: Outset's sunken treasure registered with chart index %d (got %d)", k, save);
        randoSmokeCheck(kName, what, save == k);
    }

    // 2. The sea chart.
    JKRArchive* arc = dComIfGp_getFmapResArchive();
    if (!randoSmokeCheck(kName, "fmapres.arc mounted", arc != nullptr)) {
        return;
    }
    alignas(dMenu_Fmap_c) static unsigned char storage[sizeof(dMenu_Fmap_c)];
    dMenu_Fmap_c* fmap = reinterpret_cast<dMenu_Fmap_c*>(storage);
    pc_rando_charts_reset();
    fmap->aramCmapDatRead();
    void* pat = JKRGetResource('DATA', "CmapDat.bin", arc);
    if (!randoSmokeCheck(kName, "CmapDat.bin loaded", pat != nullptr)) {
        return;
    }
    const uint32_t num = rd32((const uint8_t*)pat);
    writef(STDERR_FILENO, "[cos] rando-smoke charts: CmapDat.bin: %u records (host-order count 0x%X), %u bytes\n",
           (unsigned)num, (unsigned)*(const uint32_t*)pat, (unsigned)arc->getResSize(pat));
    const uint8_t* rec = recordOf(pat, number);
    const int status = pc_rando_charts_cmapdat_status();
    if (vanilla) {
        randoSmokeCheck(kName, "sea chart: hook not run", status == PCR_CHARTS_NOT_RUN);
        randoSmokeCheck(kName, "sea chart: Triforce Chart 4 points to Outset",
                        rec != nullptr && pc_rando_chart_island_of_sector((int8_t)rec[4], (int8_t)rec[5]) == kRoom &&
                            rec[1] == 4);
    } else {
        snprintf(what, sizeof what, "sea chart: CmapDat.bin patched by the hunk (status %d)", status);
        randoSmokeCheck(kName, what, status == PCR_CHARTS_PATCHED && pc_rando_charts_cmapdat_buffer() == pat);
        const uint8_t* was = pc_rando_charts_vanilla_record(kRoom);
        randoSmokeCheck(kName, "sea chart: Outset's vanilla record is Triforce Chart 4's",
                        was != nullptr && was[2] == 4 && was[1] == 4);
        if (rec != nullptr && was != nullptr) {
            const int island = pc_rando_chart_island_of_sector((int8_t)rec[4], (int8_t)rec[5]);
            snprintf(what, sizeof what, "sea chart: %s points to Outset (sector %d,%d: island %d)", chart.c_str(),
                     (int8_t)rec[4], (int8_t)rec[5], island);
            randoSmokeCheck(kName, what, island == kRoom);
            randoSmokeCheck(kName, "sea chart: texture id and 4 random positions are Outset's",
                            rec[0] == was[0] && memcmp(rec + 6, was + 6, PCR_CMAP_RECORD_SIZE - 6) == 0);
            randoSmokeCheck(kName, "sea chart: owned index and number kept", rec[1] == k && rec[2] == number);
        } else {
            randoSmokeCheck(kName, "sea chart: the chart's record found", false);
        }
        // Every chart at its seed island.
        int good = 0;
        for (int island = 1; island <= 49; island++) {
            const int n = pc_rando_chart_number(seedChart(seed, island).c_str());
            const uint8_t* r = recordOf(pat, n);
            if (r != nullptr && pc_rando_chart_island_of_sector((int8_t)r[4], (int8_t)r[5]) == island) {
                good++;
            } else {
                writef(STDERR_FILENO, "[cos] rando-smoke charts: island %d: chart %d not at its sector\n", island, n);
            }
        }
        snprintf(what, sizeof what, "sea chart: %d/49 charts at the seed's island", good);
        randoSmokeCheck(kName, what, good == 49);
        // Where the vanilla Outset chart went.
        int tf4 = -1;
        for (int island = 1; island <= 49; island++) {
            if (seedChart(seed, island) == "Triforce Chart 4") {
                tf4 = island;
            }
        }
        const uint8_t* r4 = recordOf(pat, 4);
        snprintf(what, sizeof what, "sea chart: Triforce Chart 4 points to island %d", tf4);
        randoSmokeCheck(kName, what,
                        r4 != nullptr && pc_rando_chart_island_of_sector((int8_t)r4[4], (int8_t)r4[5]) == tf4);

        // 3. Read again: nothing changes.
        const size_t bytes = 4 + (size_t)num * PCR_CMAP_RECORD_SIZE;
        std::string copy((const char*)pat, bytes);
        fmap->aramCmapDatRead();
        randoSmokeCheck(kName, "sea chart: a second read leaves the patched table (ALREADY)",
                        pc_rando_charts_cmapdat_status() == PCR_CHARTS_ALREADY &&
                            memcmp(copy.data(), pat, bytes) == 0);
    }
    arc->removeResource(pat);
}

} // namespace

void randoSmokeCharts(unsigned int frames) {
    (void)frames;
    switch (sState) {
    case kOff: {
        if (!randoSmokeInPlay(10)) {
            return;
        }
        const char* v = getenv("COS_CHARTS_VANILLA");
        sVanilla = v != nullptr && strcmp(v, "1") == 0;
        if (!sVanilla && !loadSeed()) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        pc_rando_charts_reset();
        sScene = playSceneId();
        sSince = 0;
        writef(STDERR_FILENO, "[cos] rando-smoke charts: to sea room %d point %d\n", kRoom, kSeaPoint);
        dComIfGp_setNextStage("sea", (s16)kSeaPoint, (s8)kRoom);
        sState = kArrive;
        return;
    }
    case kArrive: {
        const fpc_ProcID id = playSceneId();
        if (id == fpcM_ERROR_PROCESS_ID_e || id == sScene || dComIfGp_getPlayer(0) == nullptr ||
            strcmp(dComIfGp_getStartStageName(), "sea") != 0 || dComIfGp_roomControl_getStayNo() != kRoom) {
            if (++sSince > kArriveFrames) {
                randoSmokeCheck(kName, "sea room 44 came up again", false);
                sState = kDone;
                randoSmokeEnd(kName);
            }
            return;
        }
        sSince = 0;
        sState = kSettle;
        return;
    }
    case kSettle:
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, ++sEventFrames % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (++sSince < kSettleFrames) {
            return;
        }
        sState = kDone;
        setDrivenPad(false, 0, 0, 0);
        run();
        randoSmokeEnd(kName);
    case kDone:
        return;
    }
}

} // namespace pc
