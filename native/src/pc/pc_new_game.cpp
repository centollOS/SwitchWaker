// Milestones M11 new-game and M14 outset-real (docs/NATIVE_PORT_PHASE4_6.md, milestone table and
// step NG-probe): the real new-game flow, from a clean memory card, driven by the controller script
// native/check/input/new-game.txt. The probe only reads game state.
//
// The clean card: for COS_MILESTONE=new-game or outset-real, pc_aurora_init points slot A at the
// empty folder <COS_RUN_DIR>/card/ (prepareRunCard, pc_save.cpp), so the name scene finds a
// formatted card without the game's file.
//
// M11 new-game: the title's START request reaches the name scene (M10), which
// - offers to create the save file and makes it (memory card check procedures
//   MemCardMakeGameFileSel -> MemCardMakeGameFile -> MemCardMakeGameFileCheck);
// - opens the file select (main procedure FileSelectMain) and, for a new file, name entry
//   (NameInMain); once the name is confirmed, changeGameScene requests the OPEN scene (the
//   prologue, fpcNm_OPEN_SCENE_e, d_s_open.cpp) because the file is new.
// pc_name_scene_drawn (end of dScnName_c::draw) hands every frame's procedure indices to
// newGameNameScene, which logs each change and remembers which of those procedures ran. Once per
// game frame newGameFrame (pc_frame_end) looks for the OPEN scene; when it finished creating, the
// probe checks that the name scene went through every step above, that the player name is set and
// that the save file exists on the card, then logs new-game after kOpenFrames frames of the OPEN
// scene executing. With the milestone as target, a missing step is a failed check (exit 1).
//
// M14 outset-real continues the same run: the prologue's states are logged as they change; when
// it ends the game changes to the PLAY scene at the save's return place (Outset, sea room 44). The
// probe then waits for the PLAY scene to execute, the start room to be up (stageRoomReady,
// pc_title_stage.cpp) and the player actor to exist, and from then on logs every change of the
// event system (dComIfGp_event_runCheck) and of the STB demo manager (dDemo_manager_c mode: 1
// playing, 2 ended). Link is free once an event ran since the PLAY scene began (the intro) and no
// event or demo is running. While he is free, every hold of pad 0's converted main stick past
// kStickHeld is measured as in M13 (pc_outset.cpp): a kHoldFrames hold must move him more than
// kMoveUnits. outset-real is logged once he moved so and kFreeFrames frames passed since he was
// first free. Until each milestone, the first unmet condition is logged when it changes and every
// kStatusEvery frames, which records where a run stops.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_s_open.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cmath>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kOpenFrames = 60;
constexpr unsigned int kStatusEvery = 600;
constexpr float kStickHeld = 0.5f;
constexpr unsigned int kHoldFrames = 120;
constexpr float kMoveUnits = 300.0f;
constexpr unsigned int kFreeFrames = 300;

// dScnName_c::MainProc and MemCardCheckProc (d_s_name.cpp), in table order.
const char* const kMainProcs[] = {
    "MemCardCheckMain", "NoteOpen",      "NoteOpenWait", "FileSelectOpen",  "FileSelectMain",
    "FileSelectClose",  "NameInOpen",    "NameInMain",   "NameInClose",     "changeGameScene",
    "SaveOpen",         "SaveMain",      "SaveClose",    "ResetWait",       "ShopDemoDataLoad",
    "ShopDemoDataSet",
};
const char* const kMemCardProcs[] = {
    "MemCardStatCheck",         "MemCardLoadWait",          "MemCardErrMsgWaitKey",
    "MemCardErrMsgWaitKey2",    "MemCardErrMsgWaitNoSaveSel", "MemCardErrMsgWaitFormatSel",
    "MemCardErrMsgWaitFormatSel2", "MemCardFormat",         "MemCardFormatCheck",
    "MemCardMakeGameFileSel",   "MemCardMakeGameFile",      "MemCardMakeGameFileCheck",
    "MemCardGotoFileSelect",    "MemCardGotoIPLSelect",     "MemCardGotoIPL",
    "MemCardCheckDbg",          "MemCardCheckDbgWait",
};
const char* const kDrawProcs[] = {"FileErrorDraw", "FileSelectDraw", "NameInDraw", "SaveDraw",
                                  "NoneDraw"};
constexpr int kMainFileSelectMain = 4;
constexpr int kMainNameInMain = 7;
constexpr int kMainChangeGameScene = 9;
constexpr int kCardMakeGameFile = 10;
constexpr int kCardMakeGameFileCheck = 11;

template <size_t N> const char* procName(const char* const (&table)[N], int i) {
    return i >= 0 && (size_t)i < N ? table[i] : "?";
}

enum Phase { kWaitOpen, kOpen, kWaitPlay, kPlay, kDone };

Phase sPhase = kWaitOpen;
int sMainProc = -1;
int sCardProc = -1;
int sDrawProc = -1;
bool sMadeFile = false;    // MemCardMakeGameFile ran
bool sFileChecked = false; // MemCardMakeGameFileCheck ran
bool sFileSelect = false;  // FileSelectMain ran
bool sNameIn = false;      // NameInMain ran
bool sChangeScene = false; // changeGameScene ran
unsigned int sOpenFrame = 0;
int sOpenState = -1;
const char* sLastReason = nullptr;
unsigned int sReasonFrame = 0;
unsigned int sPlayFrame = 0;
int sRoomNo = -1;
int sEventRunning = -1;
int sDemoMode = -1;
bool sEventSeen = false;
bool sDemoSeen = false;
unsigned int sDemos = 0;
bool sFree = false;
unsigned int sFreeFrame = 0;
bool sControlled = false;
unsigned int sHeld = 0;
cXyz sHoldStart;

bool isTarget() {
    return gConfig.milestone != nullptr && (strcmp(gConfig.milestone, "new-game") == 0 ||
                                            strcmp(gConfig.milestone, "outset-real") == 0);
}

void* findByName(void* proc, void* name) {
    return fpcM_GetName(proc) == *(s16*)name ? proc : nullptr;
}

// The process named `name` once it finished creating, else nullptr.
base_process_class* executing(s16 name) {
    base_process_class* proc = (base_process_class*)fpcM_Search(findByName, &name);
    if (proc == nullptr || fpcM_IsCreating(fpcM_GetID(proc)) != FALSE) {
        return nullptr;
    }
    return proc;
}

// Logs `reason` when it changes and every kStatusEvery frames.
void waiting(const char* tag, unsigned int frames, const char* reason) {
    if (reason != sLastReason || frames - sReasonFrame >= kStatusEvery) {
        writef(STDERR_FILENO, "[cos] %s: frame %u: waiting: %s\n", tag, frames, reason);
        sLastReason = reason;
        sReasonFrame = frames;
    }
}

bool fileExists(const char* path) {
    struct stat st;
    return path != nullptr && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// M11's checks once the OPEN scene runs: nullptr, or the first step the run did not go through.
const char* newGameGap() {
    if (!sMadeFile || !sFileChecked) {
        return "the name scene did not create the save file (MemCardMakeGameFile/Check)";
    }
    if (runCardGciPath() != nullptr && !fileExists(runCardGciPath())) {
        return "no save file on the card";
    }
    if (!sFileSelect) {
        return "the file select did not run (FileSelectMain)";
    }
    if (!sNameIn) {
        return "name entry did not run (NameInMain)";
    }
    if (!sChangeScene) {
        return "the name scene did not request the game scene (changeGameScene)";
    }
    const char* name = dComIfGs_getPlayerName();
    if (name == nullptr || name[0] == '\0') {
        return "no player name";
    }
    return nullptr;
}

void openFrame(unsigned int frames) {
    base_process_class* proc = executing(fpcNm_OPEN_SCENE_e);
    if (sPhase == kWaitOpen) {
        if (proc == nullptr) {
            if (isTarget()) {
                waiting("new-game", frames, "OPEN scene not executing");
            }
            return;
        }
        sPhase = kOpen;
        sOpenFrame = frames;
        writef(STDERR_FILENO, "[cos] new-game: OPEN scene (prologue) executing at frame %u; player "
                              "name \"%s\"; save file %s\n",
               frames, dComIfGs_getPlayerName(),
               runCardGciPath() == nullptr        ? "(user card)"
               : fileExists(runCardGciPath()) ? "on the card"
                                                  : "missing");
        if (const char* gap = newGameGap()) {
            writef(STDERR_FILENO, "[cos] new-game: FAIL %s\n", gap);
            if (isTarget()) {
                pc_exit(PC_EXIT_CHECK_FAILED);
            }
        }
    }
    if (proc == nullptr) {
        // The prologue ended: its scene was deleted for the game scene.
        writef(STDERR_FILENO, "[cos] new-game: OPEN scene gone at frame %u (last state %d)\n",
               frames, sOpenState);
        sPhase = kWaitPlay;
        sLastReason = nullptr;
        return;
    }
    dScnOpen_proc_c* openProc = ((dScnOpen_c*)proc)->mpProc;
    const int state = openProc != nullptr ? (int)openProc->mState : -1;
    if (state != sOpenState) {
        writef(STDERR_FILENO, "[cos] new-game: frame %u: prologue state %d\n", frames, state);
        sOpenState = state;
    }
    if (frames - sOpenFrame == kOpenFrames) {
        writef(STDERR_FILENO, "[cos] new-game: OPEN scene ran %u frames\n", kOpenFrames);
        pc_milestone("new-game");
    }
}

// The first unmet condition for Link in the start room of the PLAY scene, or nullptr.
const char* playUnmet() {
    if (executing(fpcNm_PLAY_SCENE_e) == nullptr) {
        return "PLAY scene not executing";
    }
    if (const char* room = stageRoomReady(dComIfGp_getStartStageRoomNo(), nullptr)) {
        return room;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return "no player actor";
    }
    if (fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        return "player 0 is not a PLAYER actor";
    }
    if (fpcM_IsCreating(fopAcM_GetID(player)) != FALSE) {
        return "player actor still creating";
    }
    return nullptr;
}

void controlFrame(unsigned int frames, fopAc_ac_c* player) {
    const cXyz& pos = player->current.pos;
    if ((float)g_mDoCPd_cpadInfo[0].mMainStickValue <= kStickHeld) {
        sHeld = 0;
        return;
    }
    if (sHeld++ == 0) {
        sHoldStart = pos;
        return;
    }
    if (sHeld != kHoldFrames) {
        return;
    }
    const float dx = pos.x - sHoldStart.x;
    const float dz = pos.z - sHoldStart.z;
    const float moved = std::sqrt(dx * dx + dz * dz);
    writef(STDERR_FILENO, "[cos] outset-real: frame %u: stick held %u frames, Link moved %.1f "
                          "units (%.1f, %.1f, %.1f) -> (%.1f, %.1f, %.1f)\n",
           frames, kHoldFrames, (double)moved, (double)sHoldStart.x, (double)sHoldStart.y,
           (double)sHoldStart.z, (double)pos.x, (double)pos.y, (double)pos.z);
    if (moved > kMoveUnits) {
        sControlled = true;
    }
}

void playFrame(unsigned int frames) {
    if (sPhase == kWaitPlay) {
        if (const char* reason = playUnmet()) {
            waiting("outset-real", frames, reason);
            return;
        }
        sPhase = kPlay;
        sPlayFrame = frames;
        sRoomNo = dComIfGp_getStartStageRoomNo();
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        writef(STDERR_FILENO, "[cos] outset-real: Link in %s room %d at frame %u: PLAY scene "
                              "executing, room up, player at (%.1f, %.1f, %.1f)\n",
               dComIfGp_getStartStageName(), sRoomNo, frames, (double)player->current.pos.x,
               (double)player->current.pos.y, (double)player->current.pos.z);
    }

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (executing(fpcNm_PLAY_SCENE_e) == nullptr || player == nullptr) {
        // A scene or stage change (a cutscene may move to another stage): wait for the next one.
        writef(STDERR_FILENO, "[cos] outset-real: frame %u: PLAY scene or player gone\n", frames);
        sPhase = kWaitPlay;
        sLastReason = nullptr;
        sFree = false;
        sHeld = 0;
        return;
    }

    const int running = dComIfGp_event_runCheck() ? 1 : 0;
    if (running != sEventRunning) {
        writef(STDERR_FILENO, "[cos] outset-real: frame %u: event %s\n", frames,
               running ? "running" : "over");
        sEventRunning = running;
    }
    sEventSeen |= running != 0;
    dDemo_manager_c* demo = dComIfGp_demo_get();
    const int demoMode = demo != nullptr ? (int)demo->getMode() : 0;
    if (demoMode != sDemoMode) {
        if (demoMode == 1) {
            sDemos++;
        }
        writef(STDERR_FILENO, "[cos] outset-real: frame %u: STB demo %s (demo frame %d)\n", frames,
               demoMode == 1 ? "playing" : demoMode == 2 ? "ended" : "none",
               demo != nullptr ? demo->getFrame() : -1);
        sDemoMode = demoMode;
    }
    sDemoSeen |= demoMode != 0;

    const bool free = sEventSeen && !running && demoMode == 0;
    if (free != sFree) {
        sFree = free;
        sHeld = 0;
        if (free) {
            sFreeFrame = frames;
            writef(STDERR_FILENO, "[cos] outset-real: frame %u: Link free in %s room %d at (%.1f, "
                                  "%.1f, %.1f); %u STB demo(s) played\n",
                   frames, dComIfGp_getStartStageName(), dComIfGp_getStartStageRoomNo(),
                   (double)player->current.pos.x, (double)player->current.pos.y,
                   (double)player->current.pos.z, sDemos);
        }
    }
    if (!free) {
        waiting("outset-real", frames,
                !sEventSeen ? "the intro event has not started"
                : running   ? "event running"
                            : "STB demo playing");
        return;
    }
    controlFrame(frames, player);
    if (!sControlled) {
        waiting("outset-real", frames, "Link free, no stick hold has moved him yet");
        return;
    }
    if (frames - sFreeFrame < kFreeFrames) {
        return;
    }
    sPhase = kDone;
    writef(STDERR_FILENO, "[cos] outset-real: Link controllable in %s room %d, %u frames since he "
                          "was free; %u STB demo(s) played\n",
           dComIfGp_getStartStageName(), dComIfGp_getStartStageRoomNo(), frames - sFreeFrame,
           sDemos);
    pc_milestone("outset-real");
}

} // namespace

bool newGameNeedsCleanCard() {
    return isTarget();
}

void newGameNameScene(int mainProc, int memCardCheckProc, int drawProc) {
    if (mainProc != sMainProc || memCardCheckProc != sCardProc || drawProc != sDrawProc) {
        writef(STDERR_FILENO, "[cos] name-scene: frame %u: main %s, memory card %s, draw %s\n",
               pc_frame_count(), procName(kMainProcs, mainProc),
               procName(kMemCardProcs, memCardCheckProc), procName(kDrawProcs, drawProc));
        sMainProc = mainProc;
        sCardProc = memCardCheckProc;
        sDrawProc = drawProc;
    }
    // The memory card procedures only run while the main procedure is MemCardCheckMain.
    if (mainProc == 0) {
        sMadeFile |= memCardCheckProc == kCardMakeGameFile;
        sFileChecked |= memCardCheckProc == kCardMakeGameFileCheck;
    }
    sFileSelect |= mainProc == kMainFileSelectMain;
    sNameIn |= mainProc == kMainNameInMain;
    sChangeScene |= mainProc == kMainChangeGameScene;
}

void newGameFrame(unsigned int frames) {
    switch (sPhase) {
    case kWaitOpen:
    case kOpen:
        openFrame(frames);
        break;
    case kWaitPlay:
    case kPlay:
        playFrame(frames);
        break;
    case kDone:
        break;
    }
}

} // namespace pc
