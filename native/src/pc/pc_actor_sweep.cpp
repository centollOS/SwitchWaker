// COS_SMOKE=actor-sweep (docs/NATIVE_PORT_PHASE4_6.md, step 6.9 robustness): with the debug stage
// boot (COS_BOOT_STAGE, e.g. sea:44:206, Outset), once Link is in the start room (the M12 probe,
// pc_outset.cpp), every actor profile of g_fpcPfLst_ProfileList is spawned next to Link in turn,
// run for kRunFrames game frames and deleted. Pass = no fault; a refused creation is fine.
//
// actorSweepFrame (pc_frame_end, every game frame) drives it between game frames, outside
// fapGm_Execute:
// - an actor profile is one whose leaf sub-method is g_fopAc_Method (the scenes, overlaps, kankyo,
//   message and camera processes are not actors and are skipped);
// - it is created with fopAcM_create(procName, parameter 0, kSpawnDistance in front of Link,
//   Link's room), from Link's layer (as if Link had spawned it), facing Link;
// - each frame it is looked for among the creating processes and the executing ones. Gone before
//   kRunFrames frames: refused (creation failed) or deleted itself. Still there after kRunFrames
//   frames: fpcM_Delete (a process still creating is aborted that way too), then wait until it is
//   gone (kDeleteFrames at most);
// - after each profile, the PLAY scene and Link must still be there, otherwise the sweep stops
//   (exit 1: the rest would not run in Outset).
// COS_ACTOR_SWEEP=<first>[-<last>] limits the sweep to those process names (fpcNm_*, the
// profile list index); native/tools/actor_sweep.py uses it to go on after a fault.
// <COS_RUN_DIR>/actor_sweep.txt gets a "begin" line before each creation (written straight to
// the file, so it names the profile when a fault ends the process) and a result line after.
// The sweep reads game state and creates and deletes only its own actors.
#include "pc_internal.h"

#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_create_iter.h"
#include "f_pc/f_pc_create_req.h"
#include "f_pc/f_pc_executor.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_leaf.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_profile.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;  // after Link is in the room, before the first spawn
constexpr unsigned int kRunFrames = 30;     // per actor (step 6.9)
constexpr unsigned int kDeleteFrames = 120; // to finish a deletion
constexpr unsigned int kGapFrames = 3;      // between one actor gone and the next spawn (Aurora
                                            // renders a frame later: a fault stays attributed)
constexpr float kSpawnDistance = 150.0f;

enum State { kOff, kWaitLink, kNext, kRunning, kDeleting, kGap, kDone };

State sState = kOff;
bool sChecked = false;
int sFirst = 0;
int sLast = fpcNm_MAX_NUM_e - 1;
int sProc = -1;          // the process name being swept
fpc_ProcID sId = fpcM_ERROR_PROCESS_ID_e;
unsigned int sSince = 0; // frames in the current state
unsigned int sAlive = 0; // frames since the current actor was created
bool sWasExecuting = false;
int sFd = -1;
unsigned int sSwept = 0, sExecuted = 0, sRefused = 0, sSelfDeleted = 0, sDeletedCreating = 0,
             sStuck = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void* judgeExecutingById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}

// fpcCtIt_Judge hands the judge each create_request (see fpcCtRq_isCreatingByID).
void* judgeCreatingById(void* req, void* id) {
    create_request* r = static_cast<create_request*>(req);
    return r->mBsPcId == *(fpc_ProcID*)id ? r : nullptr;
}

bool isActorProfile(const process_profile_definition* prof) {
    if (prof == nullptr) {
        return false;
    }
    const leaf_process_profile_definition* leaf =
        reinterpret_cast<const leaf_process_profile_definition*>(prof);
    return leaf->sub_method == &g_fopAc_Method.base;
}

const char* stageName(int proc) {
    const char* name = dStage_getName((s16)proc, -1);
    if (name == nullptr || (unsigned char)name[0] >= 0x80) {
        name = dStage_getName((s16)proc, 0);
    }
    return name != nullptr && (unsigned char)name[0] < 0x80 ? name : "-";
}

void finish(const char* result) {
    if (sFd >= 0) {
        writef(sFd, "%d %s %s %u\n", sProc, stageName(sProc), result, sAlive);
    }
    sSwept++;
    sState = kGap;
    sSince = 0;
    sId = fpcM_ERROR_PROCESS_ID_e;
}

// nullptr while Outset is still running with Link in it.
const char* sceneLost() {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return "PLAY scene gone";
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr || fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        return "Link gone";
    }
    const PcBootStage* boot = pc_boot_stage();
    const char* stage = dComIfGp_getStartStageName();
    if (boot != nullptr && (stage == nullptr || strcmp(stage, boot->stage) != 0)) {
        return "left the stage";
    }
    return nullptr;
}

void parseRange() {
    const char* v = getenv("COS_ACTOR_SWEEP");
    if (v == nullptr || *v == '\0') {
        return;
    }
    char* end = nullptr;
    long first = strtol(v, &end, 10);
    long last = first;
    if (end != v && *end == '-') {
        const char* p = end + 1;
        last = strtol(p, &end, 10);
        if (end == p) {
            end = nullptr;
        }
    }
    if (end == nullptr || end == v || *end != '\0' || first < 0 || last < first ||
        last >= fpcNm_MAX_NUM_e) {
        writef(STDERR_FILENO, "[cos] actor-sweep: COS_ACTOR_SWEEP=\"%s\" is not <first>[-<last>] "
                              "in 0..%d\n", v, fpcNm_MAX_NUM_e - 1);
        pc_exit(PC_EXIT_USAGE);
    }
    sFirst = (int)first;
    sLast = (int)last;
}

void spawn() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const s16 yaw = player->shape_angle.y;
    cXyz pos = player->current.pos;
    pos.x += kSpawnDistance * cM_ssin(yaw);
    pos.z += kSpawnDistance * cM_scos(yaw);
    csXyz angle(0, (s16)(yaw + 0x8000), 0);
    const int room = fopAcM_GetRoomNo(player);

    if (sFd >= 0) {
        writef(sFd, "%d %s begin\n", sProc, stageName(sProc));
    }
    // Created from Link's layer, as an actor Link spawned would be.
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(static_cast<base_process_class*>(player)->mLyTg.mpLayer);
    sId = fopAcM_create((s16)sProc, 0, &pos, room, &angle);
    fpcLy_SetCurrentLayer(saved);
    sWasExecuting = false;
    sSince = 0;
    sAlive = 0;
    if (sId == fpcM_ERROR_PROCESS_ID_e) {
        sRefused++;
        finish("refused-request");
        return;
    }
    sState = kRunning;
}

} // namespace

void actorSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "actor-sweep") != 0) {
            return;
        }
        if (pc_boot_stage() == nullptr) {
            writef(STDERR_FILENO, "[cos] actor-sweep: needs COS_BOOT_STAGE (e.g. --stage "
                                  "sea:44:206, Outset)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        parseRange();
        sFd = openRunFile("actor_sweep.txt");
        if (sFd >= 0) {
            writef(sFd, "# actor-sweep (COS_SMOKE=actor-sweep): <proc name> <dStage name> "
                        "<result> <frames alive>; \"begin\" is written before each creation\n");
        }
        writef(STDERR_FILENO, "[cos] actor-sweep: process names %d-%d, %u frames each\n", sFirst,
               sLast, kRunFrames);
        sState = kWaitLink;
        sProc = sFirst - 1;
    }
    if (sState == kOff || sState == kDone) {
        return;
    }
    sSince++;

    if (sState == kWaitLink) {
        if (!outsetLinkReady()) {
            sSince = 0;
            return;
        }
        if (sSince < kSettleFrames) {
            return;
        }
        writef(STDERR_FILENO, "[cos] actor-sweep: Link in the room; sweeping from frame %u\n",
               frames);
        sState = kNext;
    }

    if (sState == kGap) {
        if (sSince < kGapFrames) {
            return;
        }
        if (const char* lost = sceneLost()) {
            writef(STDERR_FILENO, "[cos] actor-sweep: after process %d (%s): %s; stopping\n",
                   sProc, stageName(sProc), lost);
            if (sFd >= 0) {
                writef(sFd, "%d %s scene-lost:%s 0\n", sProc, stageName(sProc), lost);
            }
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        sState = kNext;
    }

    if (sState == kNext) {
        do {
            sProc++;
        } while (sProc <= sLast && !isActorProfile(g_fpcPfLst_ProfileList[sProc]));
        if (sProc > sLast) {
            sState = kDone;
            writef(STDERR_FILENO, "[cos] actor-sweep: %u actor profiles swept: %u ran %u frames, "
                                  "%u refused, %u deleted themselves, %u deleted while still "
                                  "creating, %u not gone after deletion\n",
                   sSwept, sExecuted, kRunFrames, sRefused, sSelfDeleted, sDeletedCreating, sStuck);
            if (sFd >= 0) {
                close(sFd);
                sFd = -1;
            }
            pc_exit(PC_EXIT_REACHED);
        }
        spawn();
        return;
    }

    sAlive++;
    fpc_ProcID id = sId;
    create_request* creating = static_cast<create_request*>(fpcCtIt_Judge(judgeCreatingById, &id));
    base_process_class* executing = fpcM_Search(judgeExecutingById, &id);
    if (executing != nullptr) {
        sWasExecuting = true;
    }

    if (sState == kRunning) {
        if (creating == nullptr && executing == nullptr) {
            if (sWasExecuting) {
                sSelfDeleted++;
                finish("deleted-itself");
            } else {
                sRefused++;
                finish("refused");
            }
            return;
        }
        if (sSince < kRunFrames) {
            return;
        }
        base_process_class* proc = executing != nullptr ? executing : creating->mpRes;
        if (executing != nullptr) {
            sExecuted++;
        } else {
            sDeletedCreating++;
        }
        fpcM_Delete(proc);
        sState = kDeleting;
        sSince = 0;
        return;
    }

    // kDeleting
    if (creating == nullptr && executing == nullptr) {
        finish(sWasExecuting ? "ran" : "creating");
        return;
    }
    if (sSince >= kDeleteFrames) {
        sStuck++;
        writef(STDERR_FILENO, "[cos] actor-sweep: process %d (%s) not gone %u frames after "
                              "fpcM_Delete\n", sProc, stageName(sProc), kDeleteFrames);
        finish("not-deleted");
        return;
    }
    // A deletion refused this frame (the process was busy) is asked again.
    if ((sSince & 15) == 0) {
        fpcM_Delete(executing != nullptr ? executing : creating->mpRes);
    }
}

} // namespace pc
