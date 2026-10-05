// COS_SMOKE=item-sweep: every item number the item tables define goes through the real item-get
// demo, the way a treasure chest gives it. With the debug stage boot (COS_BOOT_STAGE, default of
// native/tools/item_sweep.py: Asoko:0:0:2, the chest of COS_SMOKE=chest), once the player is in
// the start room and no event runs (A through any opening talk):
//   1. a chest (takara3, parameters 0xFF200200: shape 2, the big chest that plays the fanfare
//      JA_BGM_OPEN_BOX, chest flag 4; angle z = <item> << 8 | 0xFF: the item, no open switch) is
//      created kSpawn in front of where the player stood, facing him, inside the play scene's
//      layer (from the root layer a process is drawn twice a frame and Aurora aborts). The same
//      spot is used for every item, so after the first one the player already stands in front of it;
//   2. the player walks at it until the A button says "Open" (dActStts_OPEN_e) and presses A; its
//      event (DEFAULT_TREASURE: daTbox_c::actionOpenWait creates the item with
//      fopAcM_createItemForTrBoxDemo, the player's item-get demo, the item's message, execItemGet)
//      must start within kStartFrames, then A every kTalkFrames for the messages; the event must end
//      within kEventSeconds of wall time (the item's message cannot close while the fanfare, a sub
//      BGM, plays: d_msg checkMesgBgm / mDoAud_checkPlayingSubBgmFlag);
//   3. the chest is deleted, chest flag 4 cleared again (daTbox_c::OpenInit_com set it; a chest
//      with its flag set is created open) and, kGapFrames after the player is free, the next item.
// The items accumulate on the file, as a player collects them.
// Skipped: item numbers whose item_resource has no archive (dItem_data::getArcname NULL; the
// "NOENTRY" and unused numbers) and dItemNo_NONE_e (0xFF).
// <COS_RUN_DIR>/item_sweep.txt: a "begin" line before each chest (written straight to the file, so
// it names the item when a fault ends the process), then one result line per item:
//   <item hex> <arc> ok <event frames> <seconds> got=<0|1> shot=<frame>   the event ended
//   <item hex> <arc> no-open <frames>                         A did not start the event (sweep goes on)
//   <item hex> <arc> refused                                  fopAcM_create failed (sweep goes on)
//   <item hex> <arc> stuck <event frames> <seconds> <event name> subbgm=0x.. mesgbgm=.. mesg=..
//                                                            the event did not end (exit 1)
//   (a shot-<frame>.png of the item held up, kShotDelay frames after its message opened)
//   <item hex> <arc> scene-lost:<why>                         the player or stage is gone (exit 1)
// A stuck item also gets the event-watch report (pc_event_watch.cpp: the event's staff, their cuts,
// the sub BGM and message state) in run.log. native/tools/item_sweep.py runs the smoke again from
// the next item after a stuck item or a fault.
// COS_ITEM_SWEEP=<first>[-<last>][,...] (decimal or 0x hex) limits the item numbers.
// COS_ITEM_SWEEP_SAVE=1: when the sweep is done, save the file through the game's save code
// (pc_save_sweep.cpp, saveSweepStart) instead of exiting; COS_SMOKE=save-load then loads it back.
// Exit 0 when every item ended its event (no-open/refused count as failures: exit 1 at the end).
#include "pc_internal.h"

#include "SSystem/SComponent/c_math.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"
#include "m_Do/m_Do_audio.h"

#include <dolphin/pad.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;
constexpr unsigned int kStartFrames = 300;
constexpr double kEventSeconds = 60.0; // wall time: the fanfare plays in real time
constexpr unsigned int kTalkFrames = 45;
constexpr unsigned int kGapFrames = 30;
constexpr unsigned int kDeleteFrames = 120;
constexpr float kSpawn = 150.0f;
constexpr unsigned int kShotDelay = 30; // frames after the item's message opened: the shot
constexpr u32 kChestParams = 0xFF200200; // shape 2, chest flag 4, function 0
constexpr int kChestFlag = 4;

enum State { kOff, kWait, kNext, kPlace, kOpen, kEvent, kDelete, kGap, kDone };
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
int sFirst = 0, sLast = 0xFE;
bool sWanted[256];
int sItem = -1;
fpc_ProcID sChestId = fpcM_ERROR_PROCESS_ID_e;
cXyz sChestPos;
s16 sChestYaw = 0;
int sRoom = 0;
bool sHaveSpot = false;
uint64_t sEventStartNs = 0;
unsigned int sEventFrames = 0;
unsigned int sGapEventFrames = 0;
int sFd = -1;
unsigned int sOk = 0, sNoOpen = 0, sRefused = 0, sSkipped = 0;
bool sSaveAfter = false;
bool sShots = true;
unsigned int sShotFrame = 0; // the current item's shot, 0 = none
unsigned int sMesgFrame = 0; // event frame its message opened, 0 = not yet

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

void* judgeById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}

const char* arcName(int item) {
    const char* arc = dItem_data::getArcname((u8)item);
    return arc != nullptr && arc[0] != '\0' ? arc : "-";
}

bool defined(int item) {
    if (item == dItemNo_NONE_e) {
        return false;
    }
    const char* arc = dItem_data::getArcname((u8)item);
    return arc != nullptr && arc[0] != '\0';
}

const char* eventName() {
    dEvt_control_c* ev = g_dComIfG_gameInfo.play.getEvent();
    if (ev->mEventId != -1) {
        dEvDtEvent_c* data = dComIfGp_getPEvtManager()->getEventData(ev->mEventId);
        if (data != nullptr) {
            return data->getName();
        }
    }
    return "(none)";
}

// nullptr while the boot stage still runs with the player in it.
const char* sceneLost() {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return "PLAY-scene-gone";
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr || fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        return "player-gone";
    }
    const PcBootStage* boot = pc_boot_stage();
    const char* stage = dComIfGp_getStartStageName();
    if (boot != nullptr && (stage == nullptr || strcmp(stage, boot->stage) != 0)) {
        return "left-the-stage";
    }
    return nullptr;
}

// COS_ITEM_SWEEP: a comma list of <first>[-<last>] (decimal or 0x hex) in 0..255.
void parseRange() {
    for (int i = 0; i < 256; i++) {
        sWanted[i] = true;
    }
    const char* v = getenv("COS_ITEM_SWEEP");
    if (v == nullptr || *v == '\0') {
        return;
    }
    for (int i = 0; i < 256; i++) {
        sWanted[i] = false;
    }
    sFirst = 255;
    sLast = 0;
    const char* p = v;
    for (;;) {
        char* end = nullptr;
        long first = strtol(p, &end, 0);
        bool ok = end != p;
        long last = first;
        if (ok && *end == '-') {
            const char* q = end + 1;
            last = strtol(q, &end, 0);
            ok = end != q;
        }
        ok = ok && first >= 0 && last >= first && last <= 0xFF && (*end == ',' || *end == '\0');
        if (!ok) {
            writef(STDERR_FILENO, "[cos] item-sweep: COS_ITEM_SWEEP=\"%s\" is not a comma list of <first>[-<last>] "
                                  "in 0..255\n", v);
            pc_exit(PC_EXIT_USAGE);
        }
        for (long i = first; i <= last; i++) {
            sWanted[i] = true;
        }
        sFirst = first < sFirst ? (int)first : sFirst;
        sLast = last > sLast ? (int)last : sLast;
        if (*end == '\0') {
            break;
        }
        p = end + 1;
    }
}

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (sFd >= 0) {
        writef(sFd, "%s\n", buf);
    }
    writef(STDERR_FILENO, "[cos] item-sweep: %s\n", buf);
}

void clearChestFlag() {
    dSv_memBit_c& bit = g_dComIfG_gameInfo.save.getMemory().getBit();
    bit.mTbox = (u32)bit.mTbox & ~(1u << kChestFlag);
}

void deleteChest() {
    if (sChestId == fpcM_ERROR_PROCESS_ID_e) {
        return;
    }
    fpc_ProcID id = sChestId;
    if (void* chest = fpcM_Search(judgeById, &id)) {
        fpcM_Delete(chest);
    }
}

bool chestGone() {
    if (sChestId == fpcM_ERROR_PROCESS_ID_e) {
        return true;
    }
    fpc_ProcID id = sChestId;
    return fpcM_Search(judgeById, &id) == nullptr && !fpcM_IsCreating(id);
}

void stop(bool ok) {
    setDrivenPad(true, 0, 0, 0);
    sState = kDone;
    writef(STDERR_FILENO, "[cos] item-sweep: items %d-%d: %u ok, %u no-open, %u refused, %u skipped (no archive)\n",
           sFirst, sLast, sOk, sNoOpen, sRefused, sSkipped);
    if (sFd >= 0) {
        writef(sFd, "# done: %u ok, %u no-open, %u refused, %u skipped\n", sOk, sNoOpen, sRefused, sSkipped);
        close(sFd);
        sFd = -1;
    }
    if (ok && sSaveAfter) {
        setDrivenPad(false, 0, 0, 0);
        saveSweepStart("item-sweep");
        return;
    }
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void spawn(unsigned int frames) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!sHaveSpot) {
        const s16 yaw = player->shape_angle.y;
        sChestPos.set(player->current.pos.x + kSpawn * cM_ssin(yaw), player->current.pos.y,
                      player->current.pos.z + kSpawn * cM_scos(yaw));
        sChestYaw = (s16)(yaw + 0x8000);
        sRoom = fopAcM_GetRoomNo(player);
        sHaveSpot = true;
        writef(STDERR_FILENO, "[cos] item-sweep: chest spot (%.0f, %.0f, %.0f) in room %d\n", sChestPos.x,
               sChestPos.y, sChestPos.z, sRoom);
    }
    clearChestFlag();
    sGapEventFrames = 0;
    sShotFrame = 0;
    sMesgFrame = 0;
    if (sFd >= 0) {
        writef(sFd, "%02X %s begin\n", sItem, arcName(sItem));
    }
    // x: the chest's room (daTbox mRoomNo); z: the item, no open switch.
    csXyz angle((s16)(sRoom & 0x3F), sChestYaw, (s16)((sItem << 8) | 0xFF));
    cXyz pos = sChestPos;
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    sChestId = fopAcM_create(fpcNm_TBOX_e, kChestParams, &pos, sRoom, &angle);
    fpcLy_SetCurrentLayer(saved);
    sSince = 0;
    if (sChestId == fpcM_ERROR_PROCESS_ID_e) {
        sRefused++;
        line("%02X %s refused", sItem, arcName(sItem));
        sState = kGap;
        return;
    }
    (void)frames;
    sState = kPlace;
}

} // namespace

void itemSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "item-sweep") != 0) {
            return;
        }
        if (pc_boot_stage() == nullptr) {
            writef(STDERR_FILENO, "[cos] item-sweep: needs COS_BOOT_STAGE (e.g. --stage Asoko:0:0:2)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        parseRange();
        const char* save = getenv("COS_ITEM_SWEEP_SAVE");
        sSaveAfter = save != nullptr && strcmp(save, "1") == 0;
        const char* shots = getenv("COS_ITEM_SWEEP_SHOTS");
        sShots = shots == nullptr || strcmp(shots, "0") != 0;
        sFd = openRunFile("item_sweep.txt");
        if (sFd >= 0) {
            writef(sFd, "# item-sweep (COS_SMOKE=item-sweep): <item hex> <arc> <result> ...; \"begin\" is "
                        "written before each chest\n");
        }
        writef(STDERR_FILENO, "[cos] item-sweep: items 0x%02X-0x%02X through a chest's item-get demo%s\n", sFirst,
               sLast, sSaveAfter ? ", then a save" : "");
        setDrivenPad(true, 0, 0, 0);
        sState = kWait;
        sItem = sFirst - 1;
    }
    if (sState == kOff || sState == kDone) {
        return;
    }
    if (!outsetLinkReady() || dComIfGp_getPlayer(0) == nullptr) {
        sSince = 0;
        return;
    }
    ++sSince;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWait:
        // A through an opening talk until no event runs, then settle.
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= kSettleFrames) {
            writef(STDERR_FILENO, "[cos] item-sweep: frame %u: the player is free; sweeping\n", frames);
            sState = kNext;
            sSince = 0;
        }
        return;
    case kNext:
        do {
            sItem++;
            if (sItem <= sLast && sWanted[sItem] && !defined(sItem)) {
                sSkipped++;
            }
        } while (sItem <= sLast && !(sWanted[sItem] && defined(sItem)));
        if (sItem > sLast) {
            stop(sNoOpen == 0 && sRefused == 0);
            return;
        }
        spawn(frames);
        return;
    case kPlace:
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= 10) {
            sState = kOpen;
            sSince = 0;
        }
        return;
    case kOpen: {
        if (dComIfGp_event_runCheck()) {
            sState = kEvent;
            sSince = 0;
            sEventStartNs = monotonicNs();
            setDrivenPad(true, 0, 0, 0);
            return;
        }
        if (dComIfGp_getDoStatus() == dActStts_OPEN_e) {
            setDrivenPad(true, sSince % 15 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        } else {
            const s16 a = cM_atan2s(sChestPos.x - player->current.pos.x, sChestPos.z - player->current.pos.z);
            const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
            setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 60.0f), (int8_t)(-cM_scos(st) * 60.0f));
        }
        if (sSince >= kStartFrames) {
            sNoOpen++;
            line("%02X %s no-open %u player=(%.0f,%.0f,%.0f) do=%d", sItem, arcName(sItem), sSince,
                 player->current.pos.x, player->current.pos.y, player->current.pos.z, (int)dComIfGp_getDoStatus());
            setDrivenPad(true, 0, 0, 0);
            deleteChest();
            sState = kDelete;
            sSince = 0;
        }
        return;
    }
    case kEvent: {
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        const double secs = (monotonicNs() - sEventStartNs) / 1e9;
        // One picture per item, the item held up with its message (COS_ITEM_SWEEP_SHOTS=0: none):
        // kShotDelay frames after the item's message opened.
        if (sMesgFrame == 0 && dComIfGp_getMesgStatus() != 0) {
            sMesgFrame = sSince;
        }
        if (sShots && sShotFrame == 0 && sMesgFrame != 0 && sSince == sMesgFrame + kShotDelay &&
            captureFrame(frames, shotSink, nullptr)) {
            sShotFrame = frames;
        }
        if (!dComIfGp_event_runCheck()) {
            sOk++;
            line("%02X %s ok %u %.1f got=%d shot=%u", sItem, arcName(sItem), sSince, secs,
                 (int)dComIfGs_checkGetItem((u8)sItem), sShotFrame);
            setDrivenPad(true, 0, 0, 0);
            deleteChest();
            sState = kDelete;
            sSince = 0;
        } else if (secs >= kEventSeconds) {
            setDrivenPad(true, 0, 0, 0);
            line("%02X %s stuck %u %.1f %s subbgm=0x%x mesgbgm=%d mesg=%d shot=%u", sItem, arcName(sItem), sSince,
                 secs, eventName(), (unsigned)mDoAud_checkPlayingSubBgmFlag(), (int)dComIfGp_checkMesgBgm(),
                 (int)dComIfGp_getMesgStatus(), sShotFrame);
            captureFrame(frames, shotSink, nullptr);
            eventWatchReport(frames);
            if (sFd >= 0) {
                close(sFd);
                sFd = -1;
            }
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        return;
    }
    case kDelete:
        if (chestGone()) {
            sChestId = fpcM_ERROR_PROCESS_ID_e;
            sState = kGap;
            sSince = 0;
            sGapEventFrames = 0;
        } else if (sSince >= kDeleteFrames) {
            line("%02X %s chest-not-deleted", sItem, arcName(sItem));
            sChestId = fpcM_ERROR_PROCESS_ID_e;
            sState = kGap;
            sSince = 0;
        } else if ((sSince & 15) == 0) {
            deleteChest();
        }
        return;
    case kGap:
        if (dComIfGp_event_runCheck()) {
            // Something else started an event (a follow-up message): A through it.
            sGapEventFrames++;
            sSince = 0;
            setDrivenPad(true, sGapEventFrames % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sGapEventFrames > 30 * 60) {
                line("%02X %s stuck-after %s", sItem, arcName(sItem), eventName());
                eventWatchReport(frames);
                pc_exit(PC_EXIT_CHECK_FAILED);
            }
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince < kGapFrames) {
            return;
        }
        if (const char* lost = sceneLost()) {
            line("%02X %s scene-lost:%s", sItem, arcName(sItem), lost);
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        sState = kNext;
        sSince = 0;
        return;
    }
}

} // namespace pc
