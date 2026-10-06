// COS_SMOKE=rope (bug B19): the hanging ropes of Tetra's ship (stage Asoko, room 0, layer 2: Niko's
// rope-swing lesson; the actor RopeR, d_a_himo3). Boot with COS_BOOT_STAGE=Asoko:0:0:2.
//   1. wait for the player, A through Niko's opening talk until no event runs, then kSettleFrames;
//   2. the player grabs the rope nearest to the start kGrabDrop below its top and kGrabSide to the
//      side (pc_debug_grab_rope, d_a_player_rope.inc: what changeRopeSwingProc does when the player
//      jumps into it; the rope hangs in the hold, out of reach from the start): the player must be
//      on the rope (daPyStts0_UNK800000_e) within kGrabFrames, hanging about where they grabbed it,
//      |grab point - top| - 95 units below the top within kTolerance (procRopeSwing_init clamps that
//      by the rope's length himo3_class::m15FC; with bug B19 the player snapped to the top);
//   3. let the swing settle with the stick released (kHangFrames), then hold R and the stick
//      back: the player must slide down the rope (procRopeDown) by at least kMinDown within
//      kDownFrames.
// Exit 0 when every check held, else 1. Shots at the grab and at the end.
#include "pc_internal.h"

#include "d/actor/d_a_himo3.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstring>
#include <unistd.h>

extern "C" int pc_debug_grab_rope(fopAc_ac_c* rope, const cXyz* at); // d_a_player_rope.inc

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;
constexpr float kGrabDrop = 200.0f;  // below the rope's top (within its 320-460 length)
constexpr float kGrabSide = 20.0f;
constexpr unsigned int kGrabFrames = 5;
constexpr float kTolerance = 60.0f;
constexpr unsigned int kHangFrames = 90;
constexpr unsigned int kDownFrames = 120;
constexpr float kMinDown = 100.0f;

enum State { kOff, kWait, kSettle, kGrab, kHang, kDown, kDone };
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
unsigned int sErrors = 0;
fpc_ProcID sRopeId = fpcM_ERROR_PROCESS_ID_e;
cXyz sTop;
float sHangY = 0.0f;
unsigned int sEventFrames = 0;

void fail(const char* what) {
    sErrors++;
    writef(STDERR_FILENO, "[cos] rope: FAIL %s\n", what);
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

void shot(unsigned int frames, const char* what) {
    writef(STDERR_FILENO, "[cos] rope: shot-%06u.png: %s\n", frames, what);
    captureFrame(frames, shotSink, nullptr);
}

void finish(unsigned int frames) {
    setDrivenPad(true, 0, 0, 0);
    sState = kDone;
    shot(frames, "the end of the run");
    writef(STDERR_FILENO, "[cos] rope: %s\n", sErrors == 0 ? "PASS" : "FAIL");
    pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

struct Nearest {
    cXyz from;
    himo3_class* best;
    float bestDist;
    int count;
};

void* nearestRope(void* proc, void* data) {
    Nearest* n = (Nearest*)data;
    if (fopAcM_GetName((fopAc_ac_c*)proc) == fpcNm_HIMO3_e) {
        himo3_class* rope = (himo3_class*)proc;
        n->count++;
        const float d = (rope->actor.current.pos - n->from).absXZ();
        writef(STDERR_FILENO, "[cos] rope: rope %u at (%.0f, %.0f, %.0f), length %.0f (m15FC)\n",
               (unsigned)fopAcM_GetID(proc), rope->actor.current.pos.x, rope->actor.current.pos.y,
               rope->actor.current.pos.z, rope->m15FC);
        if (n->best == nullptr || d < n->bestDist) {
            n->best = rope;
            n->bestDist = d;
        }
    }
    return nullptr;
}

bool onRope() {
    return dComIfGp_checkPlayerStatus0(0, daPyStts0_UNK800000_e) != 0;
}

} // namespace

void ropeFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "rope") != 0) {
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        sState = kWait;
    }
    if (sState == kOff || sState == kDone) {
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    ++sSince;
    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWait:
        if (player != nullptr && fpcM_Search(isPlayScene, nullptr) != nullptr) {
            sState = kSettle;
            sSince = 0;
        }
        return;
    case kSettle: {
        // Niko's lesson opens with a talk: A every 20 frames until no event runs.
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sEventFrames % 60 == 1) {
                writef(STDERR_FILENO, "[cos] rope: frame %u: an event runs; A\n", frames);
            }
            if (sEventFrames > 3000) {
                fail("the opening event did not end");
                finish(frames);
            }
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince < kSettleFrames) {
            return;
        }
        Nearest n = {player->current.pos, nullptr, 0.0f, 0};
        fopAcM_Search(nearestRope, &n);
        if (n.best == nullptr) {
            fail("no rope (RopeR, fpcNm_HIMO3_e) in the room: boot Asoko:0:0:2");
            finish(frames);
            return;
        }
        sRopeId = fopAcM_GetID(&n.best->actor);
        sTop = n.best->actor.current.pos;
        cXyz at(sTop.x + kGrabSide, sTop.y - kGrabDrop, sTop.z);
        writef(STDERR_FILENO, "[cos] rope: frame %u: %d rope(s); the player grabs rope %u at (%.0f, %.0f, %.0f), %.0f "
                              "below its top\n", frames, n.count, (unsigned)sRopeId, at.x, at.y, at.z, kGrabDrop);
        pc_debug_grab_rope(&n.best->actor, &at);
        sState = kGrab;
        sSince = 0;
        return;
    }
    case kGrab:
        if (onRope()) {
            const float d = (sTop - player->current.pos).abs();
            const float want = std::sqrt(kGrabDrop * kGrabDrop + kGrabSide * kGrabSide) - 95.0f;
            writef(STDERR_FILENO, "[cos] rope: frame %u: on the rope after %u frames, %.0f units from its top "
                                  "(expected about %.0f), player at y %.0f\n", frames, sSince, d, want,
                   player->current.pos.y);
            shot(frames, "the grab");
            if (std::fabs(d - want) > kTolerance) {
                fail("the player does not hang where they grabbed the rope");
            }
            sState = kHang;
            sSince = 0;
        } else if (sSince >= kGrabFrames) {
            fail("the player did not grab the rope");
            finish(frames);
        }
        return;
    case kHang:
        if (sSince >= kHangFrames) {
            sHangY = player->current.pos.y;
            writef(STDERR_FILENO, "[cos] rope: frame %u: hanging at y %.0f (%.0f below the top); R + stick back\n",
                   frames, sHangY, sTop.y - sHangY);
            sState = kDown;
            sSince = 0;
        }
        return;
    case kDown: {
        // Stick back: the stick's angle is camera-relative (daPy_lk_c::setStickData); "back" is away
        // from where the player faces, so push the stick toward the camera.
        setDrivenPad(true, PAD_TRIGGER_R, 0, -100);
        const float down = sHangY - player->current.pos.y;
        if (sSince % 30 == 0) {
            writef(STDERR_FILENO, "[cos] rope: frame %u: y %.0f, %.0f down, on the rope %d\n", frames,
                   player->current.pos.y, down, onRope() ? 1 : 0);
        }
        if (down >= kMinDown) {
            writef(STDERR_FILENO, "[cos] rope: frame %u: slid %.0f down the rope in %u frames\n", frames, down, sSince);
            finish(frames);
        } else if (sSince >= kDownFrames) {
            fail("R + stick back did not take the player down the rope");
            finish(frames);
        }
        return;
    }
    }
}

} // namespace pc
