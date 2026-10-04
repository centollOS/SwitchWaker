// COS_SMOKE=sailing (with COS_BOOT_PRESET=sailing, pc_preset.cpp): the player starts on the talking
// boat in Windfall's sea room (11, point 102, the room's north-west corner), raises the sail, and
// sails north for kSailFrames game frames (60 s at 30 fps) across the boundary into the next sea
// room (18), with the wind turned to blow north the way the wind song does it.
//
// The test drives controller port 0 itself (setDrivenPad, pc_input.cpp), so it does not depend on
// how many frames the boot takes:
//   1. wait for the player in sea room 11, then kSettleFrames; the player must ride the boat
//      (daPyStts0_SHIP_RIDE_e) and the boat actor must exist (dComIfGp_getShipActor);
//   2. the wind: dKyw_tact_wind_set(0, 0x4000), what the wind song's bird (d_a_wbird, through
//      d_operate_wind's windSet) calls for "north"; the wind vector must turn to +z within
//      kWindFrames (the game turns it smoothly);
//   3. X (the sail is on X): the sail must be up (daShip_c::getSailOn) within kSailUpFrames, else X
//      again, up to three times;
//   4. sail: the main stick steers the tiller (left/right) toward heading 0 (+z, north, downwind);
//      every frame's wall time is kept for the perf summary; the stay room must change from 11
//      (a sea room boundary crossed) and the boat must cover at least kMinDistance;
//   5. exit 0 when every check held, else 1. Shots (shot-<frame>.png in the run directory): on the
//      boat, the sail up at speed, just after the room boundary, and the last frame.
// The perf summary line: "[cos] sailing: perf ..." with the mean, median, p95 and worst frame wall
// time over the sailing part, the frames over 36.7 ms (a 30 fps frame plus 10 %) and the three
// slowest frames (uncapped runs measure what the Mac can do, capped runs the pacing; a frame with a
// shot is slower). Run with COS_PERF_EVERY (or --perf) for the game-thread split.
#include "pc_internal.h"

#include "d/actor/d_a_player.h"
#include "d/actor/d_a_ship.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "d/d_kankyo_wether.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 90;   // after the player is in the room
constexpr unsigned int kWindFrames = 150;    // the wind must have turned by then
constexpr unsigned int kSailUpFrames = 45;   // after X, the sail must be up
constexpr unsigned int kSailFrames = 1800;   // 60 s at 30 fps
constexpr float kMinDistance = 20000.0f;     // units the boat must cover (the boundary is 25000 away)
constexpr s16 kHeading = 0;                  // +z: north from Windfall's corner to room 18
constexpr s16 kWindNorth = 0x4000;           // dKyw_wind_set: (cos y, 0, sin y) = +z

enum State { kOff, kWaitPlayer, kSettle, kWind, kSailUp, kSail, kDone };
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
unsigned int sErrors = 0;
int sSailTries = 0;
int sStartRoom = -1;
int sLastRoom = -1;
bool sCrossed = false;
unsigned int sCrossFrame = 0;
bool sShotAfterCross = false;
cXyz sStartPos;
cXyz sLastPos;
float sTravelled = 0.0f;
float sMaxSpeed = 0.0f;
unsigned int sEventFrames = 0;
uint64_t sLastNs = 0;
std::vector<float> sFrameMs;
std::vector<unsigned int> sFrameNo; // the game frame of each sFrameMs entry

void fail(const char* what) {
    sErrors++;
    writef(STDERR_FILENO, "[cos] sailing: FAIL %s\n", what);
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

bool playerReady() {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return false;
    }
    const char* start = dComIfGp_getStartStageName();
    if (start == nullptr || strcmp(start, kSailingSpawn.stage) != 0 ||
        stageRoomReady(kSailingSpawn.room, nullptr) != nullptr) {
        return false;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    return player != nullptr && fopAcM_GetName(player) == fpcNm_PLAYER_e &&
           fpcM_IsCreating(fopAcM_GetID(player)) == FALSE;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

void shot(unsigned int frames, const char* what) {
    writef(STDERR_FILENO, "[cos] sailing: shot-%06u.png: %s\n", frames, what);
    if (!captureFrame(frames, shotSink, nullptr)) {
        writef(STDERR_FILENO, "[cos] sailing: no image of frame %u\n", frames);
    }
}

// The running event's name and its actors, for the log.
void logEvent(unsigned int frames) {
    dEvt_control_c* ev = g_dComIfG_gameInfo.play.getEvent();
    const char* name = "(none)";
    if (ev->mEventId != -1) {
        dEvDtEvent_c* data = dComIfGp_getPEvtManager()->getEventData(ev->mEventId);
        if (data != nullptr) {
            name = data->getName();
        }
    }
    fopAc_ac_c* pt1 = ev->getPt1();
    fopAc_ac_c* pt2 = ev->getPt2();
    fopAc_ac_c* ptT = ev->getPtT();
    writef(STDERR_FILENO, "[cos] sailing: frame %u: event %d \"%s\" mode %u, actors %d / %d, talk %d\n", frames,
           ev->mEventId, name, ev->getMode(), pt1 != nullptr ? fopAcM_GetName(pt1) : -1,
           pt2 != nullptr ? fopAcM_GetName(pt2) : -1, ptT != nullptr ? fopAcM_GetName(ptT) : -1);
}

s16 windAngle() {
    const cXyz* v = dKyw_get_wind_vec();
    return cM_atan2s(v->x, v->z);
}

void pad(uint16_t buttons, int stickX, int stickY) {
    setDrivenPad(true, buttons, (int8_t)std::max(-127, std::min(127, stickX)),
                 (int8_t)std::max(-127, std::min(127, stickY)));
}

void finish(unsigned int frames) {
    setDrivenPad(true, 0, 0, 0);
    sState = kDone;
    if (!sCrossed) {
        fail("the boat stayed in the start room (no sea room boundary crossed)");
    }
    if (sTravelled < kMinDistance) {
        char line[128];
        snprintf(line, sizeof(line), "the boat covered %.0f units, under %.0f", sTravelled, kMinDistance);
        fail(line);
    }
    std::vector<float> ms = sFrameMs;
    std::sort(ms.begin(), ms.end());
    double sum = 0;
    unsigned int slow = 0;
    for (float v : ms) {
        sum += v;
        slow += v > 36.7f;
    }
    const size_t n = ms.size();
    if (n > 0) {
        const double mean = sum / (double)n;
        writef(STDERR_FILENO, "[cos] sailing: perf over %zu frames (%s): mean %.2f ms (%.1f fps), median %.2f, "
                              "p95 %.2f, p99 %.2f, worst %.2f ms; %u frames over 36.7 ms (a 30 fps frame plus 10 %%)\n",
               n, gConfig.uncapped ? "uncapped" : "capped", mean, mean > 0 ? 1000.0 / mean : 0.0, ms[n / 2],
               ms[std::min(n - 1, n * 95 / 100)], ms[std::min(n - 1, n * 99 / 100)], ms[n - 1], slow);
    }
    // The three slowest frames and when they were (room loads, pipelines built on the way).
    std::vector<size_t> order(sFrameMs.size());
    for (size_t i = 0; i < order.size(); i++) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [](size_t a, size_t b) { return sFrameMs[a] > sFrameMs[b]; });
    for (size_t i = 0; i < std::min<size_t>(3, order.size()); i++) {
        writef(STDERR_FILENO, "[cos] sailing: slow frame %u: %.2f ms\n", sFrameNo[order[i]], sFrameMs[order[i]]);
    }
    writef(STDERR_FILENO, "[cos] sailing: travelled %.0f units in %u frames, top speed %.1f units/frame, "
                          "rooms %d -> %d (boundary at frame %u), %u frames in events\n",
           sTravelled, kSailFrames, sMaxSpeed, sStartRoom, sLastRoom, sCrossFrame, sEventFrames);
    shot(frames, "the end of the run");
    writef(STDERR_FILENO, "[cos] sailing: %s\n", sErrors == 0 ? "PASS" : "FAIL");
    pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace

void sailingFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "sailing") != 0) {
            return;
        }
        const PcBootStage* boot = pc_boot_stage();
        if (bootPreset() == nullptr || boot == nullptr || strcmp(boot->stage, kSailingSpawn.stage) != 0 ||
            boot->room != kSailingSpawn.room || boot->point != kSailingSpawn.point) {
            writef(STDERR_FILENO, "[cos] sailing: needs COS_BOOT_PRESET=sailing and no other COS_BOOT_STAGE "
                                  "than %s\n", kSailingSpawn.spec);
            pc_exit(PC_EXIT_USAGE);
        }
        setDrivenPad(true, 0, 0, 0);
        sState = kWaitPlayer;
    }
    daShip_c* ship = sState >= kSettle ? dComIfGp_getShipActor() : nullptr;
    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWaitPlayer:
        if (playerReady()) {
            writef(STDERR_FILENO, "[cos] sailing: the player is in sea room %d at frame %u\n", kSailingSpawn.room,
                   frames);
            sState = kSettle;
            sSince = 0;
        }
        return;
    case kSettle: {
        if (++sSince < kSettleFrames) {
            return;
        }
        const bool riding = dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e) != 0;
        writef(STDERR_FILENO, "[cos] sailing: frame %u: boat %s, player %s the boat, wind angle 0x%04X\n", frames,
               ship != nullptr ? "present" : "missing", riding ? "rides" : "does not ride",
               (unsigned int)(u16)windAngle());
        if (ship == nullptr || !riding) {
            fail("the player is not on the boat");
            finish(frames);
            return;
        }
        sStartPos = ship->current.pos;
        sLastPos = sStartPos;
        sStartRoom = sLastRoom = fopAcM_GetRoomNo(ship);
        shot(frames, "on the boat, sail down");
        // The wind song's effect (d_operate_wind windSet -> dKyw_tact_wind_set): wind to the north.
        dKyw_tact_wind_set(0, kWindNorth);
        writef(STDERR_FILENO, "[cos] sailing: wind set to the north (the wind song's dKyw_tact_wind_set) at frame %u\n",
               frames);
        sState = kWind;
        sSince = 0;
        return;
    }
    case kWind: {
        const s16 a = windAngle();
        if (std::abs((int)a) < 0x200) {
            writef(STDERR_FILENO, "[cos] sailing: the wind blows north after %u frames\n", sSince);
            pad(PAD_BUTTON_X, 0, 0);
            sState = kSailUp;
            sSince = 0;
            sSailTries = 1;
            return;
        }
        if (++sSince >= kWindFrames) {
            fail("the wind did not turn north");
            finish(frames);
        }
        return;
    }
    case kSailUp:
        ++sSince;
        if (sSince == 3) {
            pad(0, 0, 0);
        }
        if (ship->getSailOn()) {
            writef(STDERR_FILENO, "[cos] sailing: the sail is up at frame %u (X pressed %d time(s))\n", frames,
                   sSailTries);
            sState = kSail;
            sSince = 0;
            sLastNs = monotonicNs();
            return;
        }
        if (sSince >= kSailUpFrames) {
            if (sSailTries >= 3) {
                fail("the sail did not go up after X three times");
                finish(frames);
                return;
            }
            sSailTries++;
            pad(PAD_BUTTON_X, 0, 0);
            sSince = 0;
        }
        return;
    case kSail: {
        const uint64_t now = monotonicNs();
        sFrameMs.push_back((float)((now - sLastNs) / 1e6));
        sFrameNo.push_back(frames);
        sLastNs = now;
        ++sSince;
        if (dComIfGp_event_runCheck()) {
            if (sEventFrames++ % 300 == 0) {
                logEvent(frames);
            }
        }
        // Steer toward kHeading: the tiller follows the stick's X (daShip_c::procSteerMove).
        const int diff = (s16)(kHeading - ship->shape_angle.y);
        const int stickX = std::abs(diff) < 0x100 ? 0 : (diff > 0 ? -1 : 1) * std::min(100, 30 + std::abs(diff) / 64);
        pad(0, stickX, 0);
        const cXyz pos = ship->current.pos;
        const float step = std::sqrt((pos.x - sLastPos.x) * (pos.x - sLastPos.x) + (pos.z - sLastPos.z) * (pos.z - sLastPos.z));
        sTravelled += step;
        sMaxSpeed = std::max(sMaxSpeed, step);
        sLastPos = pos;
        const int room = fopAcM_GetRoomNo(ship);
        if (room != sLastRoom) {
            writef(STDERR_FILENO, "[cos] sailing: frame %u: stay room %d -> %d at (%.0f, %.0f, %.0f)\n", frames,
                   sLastRoom, room, pos.x, pos.y, pos.z);
            // Room 0 is the open water between the numbered sea rooms' areas.
            if (!sCrossed && room > 0 && room != sStartRoom) {
                sCrossed = true;
                sCrossFrame = frames;
            }
            sLastRoom = room;
        }
        if (sSince == 300) {
            shot(frames, "sailing north, sail up");
        }
        if (sCrossed && !sShotAfterCross && frames >= sCrossFrame + 30) {
            sShotAfterCross = true;
            shot(frames, "after the sea room boundary");
        }
        if (sSince % 150 == 0) {
            writef(STDERR_FILENO, "[cos] sailing: frame %u: boat at (%.0f, %.0f, %.0f) heading 0x%04X speed %.1f, "
                                  "travelled %.0f, room %d\n",
                   frames, pos.x, pos.y, pos.z, (unsigned int)(u16)ship->shape_angle.y, step, sTravelled, room);
        }
        if (sSince >= kSailFrames) {
            finish(frames);
        }
        return;
    }
    }
}

} // namespace pc
