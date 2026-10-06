// COS_SMOKE=npc-variants (bug B9, docs/NATIVE_PORT_PLAN.md "Known bugs"): NPCs that share one
// model and differ by a material table (BMT) must each show their own clothes, the same before
// and after a scene reload.
//
// B9: Windfall's townspeople (d_a_npc_people: one Ub model for four women, Uo for three men, ...)
// and Outset's Joel (d_a_npc_ko1, Ko with the KO02 table) are drawn with mDoExt_McaMorf::updateDL
// / entryDL(J3DMaterialTable*): the model's texture table is swapped for the variant's while the
// material display lists are built, then put back. On the GameCube the lists keep the variant's
// image addresses. On the host the image comes from the texture objects of the table that
// J3DMatPacket::draw sets (its mpTexture, the model's own table): every variant wore the base
// clothes, and where the variant's image registers in the list disagreed with the base texture
// object (size or format) Aurora decoded the base image with the variant's registers, cached under
// the base object's id: whichever NPC was drawn first decided what all of them showed until the
// next reload. Fixed in J3DMaterial (the packet records the table its list was built with) and in
// Aurora patch 0012 (a cached texture object is only reused for the same size, format and mips).
//
// The check: with COS_BOOT_STAGE=sea:11:0 (Windfall) a fixed camera (COS_CAMERA, set here) looks
// at two views (kViews: two women of the Ub model, a man of the Sa model) and measures the mean
// colour of two clothes regions in each; then Link goes into a house (Orichh) and back and the
// views are measured again. Every region must lie within kMaxDistance of its reference colour
// (Mac, Metal) both times: a variant drawn with the base clothes, or with a garbled decode, is
// far off. 4:3 picture (the regions are fractions of it); the measured frames are saved as
// shot-<frame>.png in the run directory.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 120; // after Link is in the room: NPCs created and drawn
constexpr unsigned int kViewFrames = 10;    // after moving the camera to the next view
constexpr unsigned int kHouseFrames = 30;   // inside the house before going back
constexpr double kMaxDistance = 35.0;       // RGB distance of a region's mean from its reference

struct Region {
    const char* name;
    float x0, x1, y0, y1;   // fractions of the picture
    double expected[3];     // mean colour on the Mac (Metal), fixed build
};
struct View {
    float eye[3];
    float center[3];
    Region regions[2];
};
// View 1: people 3 (Ub01, blue dress) and people 4 (Ub02, grey and yellow; before the fix the
// Ub01 dress) by the plaza. View 2: people 15 (Sa model, the big man on the pier: olive shirt,
// red sash; before the fix black and garbled, a C4 image decoded as C8 or the other way round,
// or right, depending on which Sa variant was drawn first).
const View kViews[2] = {
    {{1540.0f, 900.0f, -202600.0f}, {1540.0f, 730.0f, -203036.0f},
     {{"Ub01 dress", 0.390f, 0.450f, 0.420f, 0.550f, {101, 106, 136}},
      {"Ub02 dress", 0.570f, 0.640f, 0.450f, 0.630f, {156, 131, 80}}}},
    {{958.0f, 250.0f, -196400.0f}, {958.0f, 150.0f, -196792.0f},
     {{"Sa man shirt", 0.460f, 0.530f, 0.320f, 0.440f, {129, 136, 97}},
      {"Sa man sash", 0.460f, 0.530f, 0.470f, 0.520f, {118, 18, 31}}}},
};

enum State { kOff, kWaitWindfall, kSettle, kView, kWaitHouse, kHouse, kDone };
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
int sPass = 0; // 0: first visit, 1: after the house
int sView = 0;
double sMeans[2][2][2][3]; // [pass][view][region][rgb]

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

bool linkReady(const char* stage, int room) {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return false;
    }
    const char* start = dComIfGp_getStartStageName();
    if (start == nullptr || strcmp(start, stage) != 0 || stageRoomReady(room, nullptr) != nullptr) {
        return false;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    return player != nullptr && fopAcM_GetName(player) == fpcNm_PLAYER_e &&
           fpcM_IsCreating(fopAcM_GetID(player)) == FALSE;
}

// Render worker (captureFrame): the mean colour of each region of the current view.
void measure(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    for (int r = 0; r < 2; r++) {
        const Region& g = kViews[sView].regions[r];
        const uint32_t x0 = (uint32_t)(w * g.x0), x1 = (uint32_t)(w * g.x1);
        const uint32_t y0 = (uint32_t)(h * g.y0), y1 = (uint32_t)(h * g.y1);
        double sum[3] = {0, 0, 0};
        for (uint32_t y = y0; y < y1; y++) {
            const uint8_t* p = rgb.data() + ((size_t)y * w + x0) * 3;
            for (uint32_t x = x0; x < x1; x++, p += 3) {
                sum[0] += p[0];
                sum[1] += p[1];
                sum[2] += p[2];
            }
        }
        const double n = (double)(x1 - x0) * (double)(y1 - y0);
        for (int c = 0; c < 3; c++) {
            sMeans[sPass][sView][r][c] = n > 0 ? sum[c] / n : 0.0;
        }
    }
    saveFramePng(frame, rgb, w, h);
}

double distance(const double a[3], const double b[3]) {
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
                     (a[2] - b[2]) * (a[2] - b[2]));
}

void capture(unsigned int frames) {
    if (!captureFrame(frames, measure, nullptr)) {
        writef(STDERR_FILENO, "[cos] npc-variants: FAIL: no image of frame %u\n", frames);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    for (int r = 0; r < 2; r++) {
        const Region& g = kViews[sView].regions[r];
        const double* m = sMeans[sPass][sView][r];
        writef(STDERR_FILENO, "[cos] npc-variants: %s, frame %u: %s (%.0f, %.0f, %.0f), %.1f from (%.0f, %.0f, %.0f)\n",
               sPass == 0 ? "first visit" : "after the house", frames, g.name, m[0], m[1], m[2],
               distance(m, g.expected), g.expected[0], g.expected[1], g.expected[2]);
    }
}

void finish() {
    bool ok = true;
    for (int p = 0; p < 2; p++) {
        for (int v = 0; v < 2; v++) {
            for (int r = 0; r < 2; r++) {
                ok &= distance(sMeans[p][v][r], kViews[v].regions[r].expected) <= kMaxDistance;
            }
        }
    }
    writef(STDERR_FILENO, "[cos] npc-variants: %s (every region within %.0f of its reference, before and "
                          "after the reload)\n",
           ok ? "PASS" : "FAIL", kMaxDistance);
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void showView(int v) {
    sView = v;
    setFixedCamera(kViews[v].eye, kViews[v].center);
}

} // namespace

void npcVariantsFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "npc-variants") != 0) {
            return;
        }
        const PcBootStage* boot = pc_boot_stage();
        if (boot == nullptr || strcmp(boot->stage, "sea") != 0 || boot->room != 11) {
            writef(STDERR_FILENO, "[cos] npc-variants: needs COS_BOOT_STAGE in Windfall, e.g. sea:11:0\n");
            pc_exit(PC_EXIT_USAGE);
        }
        showView(0);
        sState = kWaitWindfall;
    }
    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWaitWindfall:
        if (linkReady("sea", 11)) {
            sState = kSettle;
            sSince = 0;
        }
        return;
    case kSettle:
        if (++sSince < kSettleFrames) {
            return;
        }
        capture(frames);
        showView(1);
        sState = kView;
        sSince = 0;
        return;
    case kView:
        if (++sSince < kViewFrames) {
            return;
        }
        capture(frames);
        if (sPass == 1) {
            sState = kDone;
            finish();
            return;
        }
        sPass = 1;
        showView(0);
        writef(STDERR_FILENO, "[cos] npc-variants: into the house (Orichh) at frame %u\n", frames);
        dComIfGp_setNextStage("Orichh", 0, 0, -1);
        sState = kWaitHouse;
        return;
    case kWaitHouse:
        if (linkReady("Orichh", 0)) {
            sState = kHouse;
            sSince = 0;
        }
        return;
    case kHouse:
        if (++sSince >= kHouseFrames) {
            writef(STDERR_FILENO, "[cos] npc-variants: back to Windfall at frame %u\n", frames);
            dComIfGp_setNextStage("sea", 0, 11, -1);
            sState = kWaitWindfall;
        }
        return;
    }
}

} // namespace pc
