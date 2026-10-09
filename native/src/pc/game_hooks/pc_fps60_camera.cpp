// COS_FPS60_TEST step B (docs/FPS60_PLAN.md, "Step B"): paint B shows the camera halfway between the
// camera of the draw pass before (the one paint A just showed) and this frame's.
//
// - Capture: camera_draw (d_camera.cpp, the draw pass) calls pc_fps60_camera_drawn once it has built
//   the view: eye, center, up, bank, fovy, aspect, near, far, the view and projection matrices, and
//   what the cut checks need (room, stage, event, demo, wipe, overlap, menu, a dCamera_c::Reset).
//   The last two captures are kept: N (shown by paint A) and N+1 (this frame's draw pass).
// - Paint B: mDoGph_Painter calls pc_fps60_view_begin right after it loads the world projection and
//   pc_fps60_view_end after the camera block. Begin computes V_t = lookAt(lerped eye, center, up,
//   bank), P_t = perspective(lerped fovy, aspect, near, far) and C = V_t * V_cur^-1, and records
//   them in the GX command stream (AuroraSetViewDelta, Aurora patch 0019), so Aurora's FIFO worker
//   applies them to the draws that follow: each draw whose projection is the world projection gets
//   every position matrix premultiplied by C (normal matrices and light directions by its
//   rotation, light positions by C) and P_t. J3D's view-space draw matrices (baked with V_cur in
//   the draw pass), JPA, the sea, weather and the packets that concatenate the view themselves all
//   move alike; orthographic passes (the 2D, full-screen quads) and other perspective projections
//   do not. End records "off".
// - A cut (paint B as in step A, t = 1): no capture of the previous draw pass, another camera or
//   view, a reset starting, an event or demo starting or stopping, a room or stage change, a wipe or
//   overlap, the pause menu toggled, the eye or center moving more than 400 units, the fovy more
//   than 10 degrees, the bank more than ~22 degrees, or the view turning more than 45 degrees.
//   Logged (one line per reason change, else every 2 s with the count) and counted in the perf line.
//
// COS_FPS60_CAMERA=0 turns the interpolation off (paint B as in step A); COS_FPS60_CAMERA_T=<t> sets
// the blend (default 0.5; 0 = the previous camera, a check of the delta: paint B's still world then
// matches paint A's).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_harness.h"
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_drawlist.h"
#include "d/d_meter.h"
#include "dolphin/mtx/mtx44.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_view.h"
#include "JSystem/JUtility/JUTFader.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_mtx.h"

#include <dolphin/gx/GXAurora.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <unistd.h>

namespace {

struct CamSnap {
    bool valid = false;
    unsigned int frame = 0;
    const view_class* view = nullptr;
    cXyz eye, center, up;
    s16 bank = 0;
    f32 fovy = 0, aspect = 0, nearZ = 0, farZ = 0;
    Mtx viewMtx;
    Mtx44 projMtx;
    int stayNo = -1;
    char stage[8] = {};
    bool event = false, demo = false, wipe = false, overlap = false, menu = false, reset = false;
};

CamSnap sPrev, sCur;
bool sResetPending = false; // dCamera_c::Reset since the last capture
bool sDeltaOn = false;      // AuroraSetViewDelta on in this paint

// counters for the perf line (pc_fps60_camera_stats)
unsigned long sInterp = 0, sSkips = 0;

// rate-limited cut log
const char* sLastReason = nullptr;
unsigned long sReasonCount = 0;
uint64_t sLastLogNs = 0;

uint64_t nowNs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

bool enabled() {
    static const bool on = [] {
        const char* v = getenv("COS_FPS60_CAMERA");
        return !(v != nullptr && v[0] == '0');
    }();
    return on;
}

f32 blendT() {
    static const f32 t = [] {
        const char* v = getenv("COS_FPS60_CAMERA_T");
        if (v == nullptr || v[0] == '\0') {
            return 0.5f;
        }
        f32 x = strtof(v, nullptr);
        return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    }();
    return t;
}

void skip(const char* reason) {
    sSkips++;
    sReasonCount++;
    const uint64_t now = nowNs();
    if (reason != sLastReason || now - sLastLogNs > 2000000000ull) {
        pc::writef(STDERR_FILENO, "[cos] fps60 camera: frame %u: paint B not interpolated (%s; %lu such paint(s) "
                              "since the last line)\n",
               pc_frame_count() + 1, reason, sReasonCount);
        sLastReason = reason;
        sReasonCount = 0;
        sLastLogNs = now;
    }
}

f32 lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

cXyz lerpv(const cXyz& a, const cXyz& b, f32 t) {
    return cXyz(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t));
}

f32 dist(const cXyz& a, const cXyz& b) {
    const f32 dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The reason paint B must not blend N with N+1, or nullptr.
const char* cutReason(const view_class* view) {
    if (!sCur.valid || sCur.frame != pc_frame_count() + 1) {
        return "no camera drawn this frame";
    }
    if (!sPrev.valid || sPrev.frame + 1 != sCur.frame) {
        return "no camera in the previous draw pass";
    }
    if (sCur.view != view || sPrev.view != view) {
        return "another camera";
    }
    if (memcmp(sCur.viewMtx, view->mViewMtx, sizeof(Mtx)) != 0 ||
        memcmp(sCur.projMtx, view->mProjMtx, sizeof(Mtx44)) != 0) {
        return "view changed after the draw pass";
    }
    if (strncmp(sCur.stage, sPrev.stage, sizeof(sCur.stage)) != 0) {
        return "stage change";
    }
    if (sCur.stayNo != sPrev.stayNo) {
        return "room change";
    }
    if (sCur.reset && !sPrev.reset) {
        return "camera reset";
    }
    if (sCur.event != sPrev.event) {
        return "event start/stop";
    }
    if (sCur.demo != sPrev.demo) {
        return "demo start/stop";
    }
    if (sCur.wipe || sPrev.wipe) {
        return "wipe";
    }
    if (sCur.overlap || sPrev.overlap) {
        return "overlap";
    }
    if (sCur.menu != sPrev.menu) {
        return "menu toggled";
    }
    if (dist(sCur.eye, sPrev.eye) > 400.0f || dist(sCur.center, sPrev.center) > 400.0f) {
        return "eye/center jump";
    }
    if (std::fabs(sCur.fovy - sPrev.fovy) > 10.0f) {
        return "fovy jump";
    }
    if (std::abs((int)(s16)(sCur.bank - sPrev.bank)) > 0x1000) {
        return "bank jump";
    }
    // the view's forward axes (row 2 of the view matrices) more than 45 degrees apart
    const f32 dot = sCur.viewMtx[2][0] * sPrev.viewMtx[2][0] + sCur.viewMtx[2][1] * sPrev.viewMtx[2][1] +
                    sCur.viewMtx[2][2] * sPrev.viewMtx[2][2];
    if (dot < 0.7071f) {
        return "view turned over 45 degrees";
    }
    return nullptr;
}

// GXSetProjection's six perspective parameters of a 4x4 projection.
void projParams(const Mtx44 p, f32 out[6]) {
    out[0] = p[0][0];
    out[1] = p[0][2];
    out[2] = p[1][1];
    out[3] = p[1][2];
    out[4] = p[2][2];
    out[5] = p[2][3];
}

} // namespace

void pc_fps60_camera_reset(void) {
    if (pc_fps60_test()) {
        sResetPending = true;
    }
}

void pc_fps60_camera_drawn(view_class* view) {
    if (!pc_fps60_test()) {
        return;
    }
    sPrev = sCur;
    CamSnap& s = sCur;
    s.valid = true;
    // camera_draw runs in the draw pass of the game frame pc_frame_end closes next
    s.frame = pc_frame_count() + 1;
    s.view = view;
    s.eye = view->mLookat.mEye;
    s.center = view->mLookat.mCenter;
    s.up = view->mLookat.mUp;
    s.bank = view->mBank;
    s.fovy = view->mFovy;
    s.aspect = view->mAspect;
    s.nearZ = view->mNear;
    s.farZ = view->mFar;
    memcpy(s.viewMtx, view->mViewMtx, sizeof(Mtx));
    memcpy(s.projMtx, view->mProjMtx, sizeof(Mtx44));
    s.stayNo = dComIfGp_roomControl_getStayNo();
    strncpy(s.stage, dComIfGp_getStartStageName(), sizeof(s.stage));
    s.event = dComIfGp_event_runCheck() != FALSE;
    dDemo_manager_c* demo = dComIfGp_demo_get();
    s.demo = demo != nullptr && demo->getMode() != 0;
    s.wipe = dDlst_list_c::mWipe;
    s.overlap = fopOvlpM_IsDoingReq() != FALSE;
    s.menu = dMenu_flag() != 0;
    s.reset = sResetPending;
    sResetPending = false;
}

void pc_fps60_view_begin(view_class* view) {
    sDeltaOn = false;
    if (!pc_paint_is_extra() || !enabled() || view == nullptr) {
        return;
    }
    if (const char* reason = cutReason(view)) {
        skip(reason);
        return;
    }
    const f32 t = blendT();
    cXyz eye = lerpv(sPrev.eye, sCur.eye, t);
    cXyz center = lerpv(sPrev.center, sCur.center, t);
    cXyz up = lerpv(sPrev.up, sCur.up, t);
    const s16 bank = (s16)(sPrev.bank + (s16)((f32)(s16)(sCur.bank - sPrev.bank) * t));
    Mtx viewT;
    mDoMtx_lookAt(viewT, &eye, &center, &up, bank);
    Mtx inv;
    if (!MTXInverse(sCur.viewMtx, inv)) {
        skip("singular view");
        return;
    }
    Mtx delta;
    MTXConcat(viewT, inv, delta);

    f32 projFrom[6], projTo[6];
    projParams(view->mProjMtx, projFrom);
    if (sCur.fovy == sPrev.fovy && sCur.aspect == sPrev.aspect && sCur.nearZ == sPrev.nearZ &&
        sCur.farZ == sPrev.farZ) {
        memcpy(projTo, projFrom, sizeof(projTo));
    } else {
        Mtx44 projT;
        C_MTXPerspective(projT, lerpf(sPrev.fovy, sCur.fovy, t), lerpf(sPrev.aspect, sCur.aspect, t),
                         lerpf(sPrev.nearZ, sCur.nearZ, t), lerpf(sPrev.farZ, sCur.farZ, t));
        projParams(projT, projTo);
    }
    AuroraSetViewDelta(&delta[0][0], projFrom, projTo);
    sDeltaOn = true;
    sInterp++;
}

void pc_fps60_view_end(void) {
    if (sDeltaOn) {
        AuroraSetViewDelta(nullptr, nullptr, nullptr);
        sDeltaOn = false;
    }
}

int pc_fps60_paint_b_allowed(void) {
    // the last frame a transition was seen, and why (logged when paint B comes back)
    static unsigned int lastSeen = 0;
    static const char* lastWhy = nullptr;
    static bool blocked = false;
    static s16 lastMonotone = 0;
    static char lastStage[8] = {};
    const unsigned int frame = pc_frame_count() + 1;
    const char* why = nullptr;
    const s16 monotone = mDoGph_gInf_c::getMonotoneRate();
    JUTFader* fader = mDoGph_gInf_c::mFader;
    if (pc_fpcNdRq_pending()) {
        why = "scene request";
    } else if (fopOvlpM_IsDoingReq() || fopOvlpM_IsPeek()) {
        why = "overlap";
    } else if (dDlst_list_c::mWipe) {
        why = "wipe";
    } else if (mDoGph_gInf_c::isFade()) {
        why = "fade";
    } else if (fader != nullptr && (fader->getStatus() == JUTFader::FadeIn || fader->getStatus() == JUTFader::FadeOut)) {
        why = "fader";
    } else if (monotone != lastMonotone) {
        why = "monotone";
    } else if (lastStage[0] != '\0' && strncmp(lastStage, dComIfGp_getStartStageName(), sizeof(lastStage)) != 0) {
        why = "stage change";
    }
    lastMonotone = monotone;
    strncpy(lastStage, dComIfGp_getStartStageName(), sizeof(lastStage));
    // COS_FPS60_GATE=0: paint B even in transitions (step A's behaviour; a test of this gate)
    static const bool gate = [] {
        const char* v = getenv("COS_FPS60_GATE");
        return !(v != nullptr && v[0] == '0');
    }();
    if (!gate) {
        return 1;
    }
    if (why != nullptr) {
        if (!blocked || why != lastWhy) {
            pc::writef(STDERR_FILENO, "[cos] fps60: frame %u: presented once (%s)\n", frame, why);
        }
        lastSeen = frame;
        lastWhy = why;
        blocked = true;
        return 0;
    }
    if (blocked && frame - lastSeen <= 3) {
        return 0; // the first frames after a transition
    }
    if (blocked) {
        pc::writef(STDERR_FILENO, "[cos] fps60: frame %u: paint B again (after %s)\n", frame, lastWhy);
        blocked = false;
    }
    return 1;
}

void pc_fps60_camera_stats(unsigned long* interpolated, unsigned long* skipped) {
    *interpolated = sInterp;
    *skipped = sSkips;
}
