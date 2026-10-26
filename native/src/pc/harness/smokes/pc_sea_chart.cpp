// COS_SMOKE=sea-chart (issue #14): the sea chart's island check points (/res/FmapDat/FmapDat.bin, read by
// dMap_c::setFmapChkDtPrm) give each island's sector to the chart's zooms. Once the play scene runs:
//   1. the count is at least kMinPoints (the disc has one per island sector);
//   2. every point's sector is on the 7 x 7 grid (-3..3) and dMap_getCheckPointUseGrid finds that point
//      again from its sector, as setDispIslandPos does for the zooms;
//   3. some points lie in the western and northern sectors (negative coordinates).
// Before the fix the count was read in host order (0: no sector found an island, so the 1000 x 1000
// zoom stayed empty and the 200 x 200 one kept Forsaken Fortress) and the sectors were char, unsigned
// on ARM (no negative sector matched).
// Issue #40: the treasure and Triforce charts' X marks (/res/FmapDat's CmapDat.bin, read by
// dMenu_Fmap_c::aramCmapDatRead) are placed from each entry's salvage points. On the disc:
//   4. the entries are numbered 1..count in order (49 sectors, then the special charts);
//   5. every salvage point's x and y (the X's offset in its sector) are whole hundreds, and some are
//      not zero.
// Before that fix the count and the points were read in host order (count 0x3D000000, the points'
// bytes swapped), so the marks were drawn off the chart or not at all.
// Exit 0 when all five hold, else 1.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_map.h"
#include "d/d_menu_fmapSv.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr int kMinPoints = 40;
constexpr int kMaxChartEntries = 64; // CmapDat.bin has 61

enum State { kOff, kWait, kDone };
State sState = kOff;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

} // namespace

void seaChartFrame(unsigned int frames) {
    if (sState == kOff) {
        sState = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "sea-chart") == 0 ? kWait : kDone;
    }
    if (sState != kWait || dComIfGp_getPlayer(0) == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return;
    }
    sState = kDone;

    const int count = dComIfGp_getFmapData() != nullptr ? (int)(u32)((dMap_FmapChkPnt*)dComIfGp_getFmapData())->field_0x0 : -1;
    int found = 0;
    int west = 0;
    int north = 0;
    for (int i = 0; i < count && i < 49; i++) {
        s8 gx;
        s8 gy;
        s16 x;
        s16 y;
        dMap_getFmapChkPntPrm(i, &gx, &gy, &x, &y, NULL);
        const int again = dMap_getCheckPointUseGrid(gx, gy);
        if (gx >= -3 && gx <= 3 && gy >= -3 && gy <= 3 && again == i) {
            found++;
        } else {
            writef(STDERR_FILENO, "[cos] sea-chart: point %d: sector %d,%d found again as %d\n", i, gx, gy, again);
        }
        west += gx < 0;
        north += gy < 0;
    }
    const bool islandsOk = count >= kMinPoints && found == count && west > 0 && north > 0;
    writef(STDERR_FILENO,
           "[cos] sea-chart: frame %u: %d check points (at least %d), %d found from their sector, %d western, "
           "%d northern: %s\n",
           frames, count, kMinPoints, found, west, north, islandsOk ? "PASS" : "FAIL");

    // the charts' X marks (issue #40)
    auto* pat = (aramCmapDatPat_t*)JKRGetResource('DATA', "CmapDat.bin", dComIfGp_getFmapResArchive());
    const int entries = pat != nullptr ? (int)pat->num : -1;
    int numbered = 0;
    int hundreds = 0;
    int marks = 0;
    int bad = 0;
    for (int i = 0; i < entries && i < kMaxChartEntries; i++) {
        const aramCmapDatPnt_t& pnt = pat->pnt[i];
        numbered += pnt.gridNo == i + 1;
        for (const aramCmapSalvagePnt_t& salvage : pnt.salvagePnt) {
            const int x = salvage.x;
            const int y = salvage.y;
            if (x % 100 == 0 && y % 100 == 0) {
                hundreds++;
                marks += x != 0 || y != 0;
            } else if (++bad <= 5) {
                writef(STDERR_FILENO, "[cos] sea-chart: chart entry %d: X at %d,%d (not whole hundreds)\n", i, x, y);
            }
        }
    }
    const int points = entries > 0 && entries <= kMaxChartEntries ? entries * 4 : 0;
    const bool marksOk = entries >= kMinPoints && entries <= kMaxChartEntries && numbered == entries &&
                         hundreds == points && marks > 0;
    writef(STDERR_FILENO,
           "[cos] sea-chart: %d chart entries (%d..%d), %d numbered in order, %d of %d salvage points in whole "
           "hundreds, %d not at the centre: %s\n",
           entries, kMinPoints, kMaxChartEntries, numbered, hundreds, points, marks, marksOk ? "PASS" : "FAIL");
    pc_exit(islandsOk && marksOk ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
