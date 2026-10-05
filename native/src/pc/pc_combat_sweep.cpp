// COS_SMOKE=combat-sweep: fights, for crash detection and for the pipeline cache
// (native/tools/combat_sweep.py, native/tools/gen_pipeline_cache.sh). With the debug stage boot
// (COS_BOOT_STAGE), once the player is in the start room (the M12 probe, pc_outset.cpp), the smoke
// steers pad 0 itself (setDrivenPad) through a fixed combat cycle and keeps the player alive.
//
// Two modes (COS_COMBAT_MODE):
// - spawn (the default): each case is an enemy spawned kSpawnDistance in front of the player, in the
//   PLAY scene's layer (from the root layer a process is drawn twice a frame and Aurora aborts; see
//   pc_ky_procs.cpp), fought for COS_COMBAT_FRAMES game frames (default 1200) or until it is gone
//   (killed, or it deleted itself), then deleted with the actors it created (pc_actor_sweep.cpp's
//   rule). The cases: the lines of COS_COMBAT_SWEEP_LIST (a file: "<dStage name> <parameters hex>
//   [<angle x> <angle z>]" per line, '#' comments: a placement of the disc, created by name, so
//   with the name's subtype, as the stage loader does, facing the player),
//   or, without a list, every actor profile whose group is fopAc_ENEMY_e, with parameters 0.
//   COS_COMBAT_SWEEP=<first>[-<last>] limits the cases (list lines from 0, or process names).
// - room: nothing is spawned; the player fights whatever the room holds (a boss room, a
//   minigame) for COS_COMBAT_FRAMES frames (default 6000), walking towards the nearest enemy
//   actor (horizontal distance), or wandering when there is none, then the run exits 0.
// The cycle (kCycle frames): first kIdleFrames frames standing still with L released, so the
// enemy comes and hits the player; then, L held to lock on: sword slashes (B) while
// walking towards the target, a spin attack (B held, the stick around), A (jump attack locked on,
// parry when the prompt shows, talk/advance text otherwise), X (the first boot item: bombs: take
// one out, A throws it), Y (bow: ready, draw, release), Z (hookshot: ready, fire). The boot items
// (COS_BOOT_ITEMS, e.g. 31,38,3B,27,2F,50: bombs on X, sword, shield, bow, hookshot, an empty
// bottle, which gets a fairy) go to X, then the first two items found of bow/hookshot/boomerang/hammer to Y and Z.
// Hearts: the maximum is raised to kMaxLife quarters; when the life drops to kLowLife or less it is
// refilled (counted), so the player takes hits without dying. COS_COMBAT_DEATH=1: from kDeathStart
// frames into each case on, no refill, one heart left and no fighting back: the player dies (a
// fairy in a bottle revives him once; then the game-over scene leaves the stage, which ends the
// run as reached once a death was seen).
// Bombs and arrows are topped up every second.
// COS_COMBAT_SHOT_EVERY=<n>: a screenshot (shot-<frame>.png) every n frames of fighting (and 90
// frames into each case), default 0 (off) in spawn mode, 600 in room mode.
// <COS_RUN_DIR>/combat_sweep.txt: a "begin" line before each spawn (written at once, so it names
// the case when a fault ends the process) and a result line after it:
//   <case> <name> <proc> <params> <result> frames=<n> hp=<start>-><min> hits=<life lost events>
//   refills=<n> locked=<frames with a lock-on target> dist=<closest approach>
// result: died (gone after it executed kMinAliveFrames frames), timeout (still there after the
// frames), refused (never executed), self-deleted (gone sooner), left-stage (the stage changed:
// a floor master carried the player away, a void-out restart; the run then exits 1 so the driver
// goes on with the next case), not-deleted.
// The smoke reads game state and creates and deletes only its own actors; it changes the save
// file in memory (hearts, ammo, button items) only.
#include "pc_internal.h"

#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_player.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera.h"
#include "f_pc/f_pc_create_iter.h"
#include "f_pc/f_pc_create_req.h"
#include "f_pc/f_pc_executor.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_leaf.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"
#include "f_pc/f_pc_profile.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 90;     // after the player is in the room
constexpr unsigned int kDeleteFrames = 120;    // to finish a deletion
constexpr unsigned int kGapFrames = 30;        // between one case gone and the next spawn
constexpr unsigned int kMinAliveFrames = 30;   // gone sooner: self-deleted, not killed
constexpr unsigned int kGoneTailFrames = 90;   // after the enemy is gone: its death effects
constexpr unsigned int kCycle = 400 + 80;
constexpr unsigned int kIdleFrames = 80;       // the cycle's first frames: stand still, get hit
constexpr float kSpawnDistance = 300.0f;
constexpr float kApproach = 180.0f;            // walk towards the target while farther
constexpr u16 kMaxLife = 20 * 4;               // quarters: 20 hearts
constexpr u16 kLowLife = 2 * 4;
constexpr int kMaxCases = 512;

enum Mode { kSpawnMode, kRoomMode };
enum State { kOff, kWaitLink, kSetup, kNext, kFight, kTail, kDeleting, kGap, kDone };

struct Case {
    char name[16]; // dStage name, or "" for a process name without a list
    unsigned int params;
    s16 angleX, angleZ; // the placement's angle x and z (parameters to many actors)
    int proc;
};

Mode sMode = kSpawnMode;
State sState = kOff;
bool sChecked = false;
Case sCases[kMaxCases];
int sNumCases = 0;
int sFirst = 0, sLast = -1;
int sCase = -1;
fpc_ProcID sId = fpcM_ERROR_PROCESS_ID_e;
unsigned int sSince = 0;
unsigned int sAlive = 0;    // frames since the current case started
unsigned int sFight = 0;    // frames of the combat cycle (room mode: since the fight started)
unsigned int sFrames = 1200;
unsigned int sShotEvery = 0;
bool sDeathMode = false;
bool sWasExecuting = false;
int sFd = -1;
// Per case.
int sHpStart = 0, sHpMin = 0;
unsigned int sHits = 0, sRefills = 0, sLocked = 0, sDeaths = 0;
float sMinDist = 1e9f;
u16 sLastLife = 0;
const char* sGoneResult = nullptr;
// Totals.
unsigned int sDied = 0, sTimeout = 0, sRefused = 0, sSelfDeleted = 0, sLeft = 0, sStuck = 0;
s16 sWanderYaw = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void* judgeExecutingById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}

void* judgeCreatingById(void* req, void* id) {
    create_request* r = static_cast<create_request*>(req);
    return r->mBsPcId == *(fpc_ProcID*)id ? r : nullptr;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

// The descendants of an actor (parentActorID, transitively), as pc_actor_sweep.cpp deletes them.
struct ChildScan {
    fpc_ProcID ids[64];
    unsigned int count;
    bool added;
};

void* collectChild(void* proc, void* data) {
    ChildScan* scan = static_cast<ChildScan*>(data);
    if (!fopAc_IsActor(proc)) {
        return nullptr;
    }
    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(proc);
    const fpc_ProcID id = fopAcM_GetID(actor);
    const fpc_ProcID parent = fopAcM_GetLinkId(actor);
    bool isChild = false;
    for (unsigned int i = 0; i < scan->count; i++) {
        if (scan->ids[i] == id) {
            return nullptr;
        }
        isChild = isChild || scan->ids[i] == parent;
    }
    if (isChild && scan->count < 64) {
        scan->ids[scan->count++] = id;
        scan->added = true;
    }
    return nullptr;
}

unsigned int deleteChildren(fpc_ProcID root) {
    ChildScan scan;
    scan.ids[0] = root;
    scan.count = 1;
    do {
        scan.added = false;
        fpcM_Search(collectChild, &scan);
    } while (scan.added);
    for (unsigned int i = 1; i < scan.count; i++) {
        fpc_ProcID id = scan.ids[i];
        if (void* child = fpcM_Search(judgeExecutingById, &id)) {
            fpcM_Delete(child);
        }
    }
    return scan.count - 1;
}

bool isEnemyProfile(const process_profile_definition* prof) {
    if (prof == nullptr) {
        return false;
    }
    const leaf_process_profile_definition* leaf =
        reinterpret_cast<const leaf_process_profile_definition*>(prof);
    if (leaf->sub_method != &g_fopAc_Method.base) {
        return false;
    }
    const actor_process_profile_definition* ac =
        reinterpret_cast<const actor_process_profile_definition*>(prof);
    return ac->group == fopAc_ENEMY_e && leaf->base.mProcName != fpcNm_PLAYER_e;
}

const char* procStageName(int proc) {
    const char* name = dStage_getName((s16)proc, -1);
    if (name == nullptr || (unsigned char)name[0] >= 0x80) {
        name = dStage_getName((s16)proc, 0);
    }
    return name != nullptr && (unsigned char)name[0] < 0x80 ? name : "-";
}

const char* caseName(int i) {
    return sCases[i].name[0] != '\0' ? sCases[i].name : procStageName(sCases[i].proc);
}

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
        return "left-stage";
    }
    return nullptr;
}

unsigned int envUint(const char* name, unsigned int def) {
    const char* v = getenv(name);
    if (v == nullptr || *v == '\0') {
        return def;
    }
    char* end = nullptr;
    unsigned long n = strtoul(v, &end, 10);
    if (end == v || *end != '\0') {
        writef(STDERR_FILENO, "[cos] combat-sweep: %s=\"%s\" is not a number\n", name, v);
        pc_exit(PC_EXIT_USAGE);
    }
    return (unsigned int)n;
}

void loadCases() {
    const char* list = getenv("COS_COMBAT_SWEEP_LIST");
    if (list != nullptr && *list != '\0') {
        FILE* f = fopen(list, "r");
        if (f == nullptr) {
            writef(STDERR_FILENO, "[cos] combat-sweep: cannot read COS_COMBAT_SWEEP_LIST=%s\n", list);
            pc_exit(PC_EXIT_USAGE);
        }
        char line[256];
        while (fgets(line, sizeof(line), f) != nullptr && sNumCases < kMaxCases) {
            char name[16];
            unsigned int params = 0;
            int ax = 0, az = 0;
            if (line[0] == '#' || sscanf(line, "%15s %x %d %d", name, &params, &ax, &az) < 1) {
                continue;
            }
            dStage_objectNameInf* inf = dStage_searchName(name);
            if (inf == nullptr) {
                writef(STDERR_FILENO, "[cos] combat-sweep: list: no actor named \"%s\"\n", name);
                pc_exit(PC_EXIT_USAGE);
            }
            Case& c = sCases[sNumCases++];
            strncpy(c.name, name, sizeof(c.name) - 1);
            c.name[sizeof(c.name) - 1] = '\0';
            c.params = params;
            c.angleX = (s16)ax;
            c.angleZ = (s16)az;
            c.proc = inf->procname;
        }
        fclose(f);
    } else {
        for (int proc = 0; proc < fpcNm_MAX_NUM_e && sNumCases < kMaxCases; proc++) {
            // Without a list the case numbers are process names: the range picks among them.
            if (!isEnemyProfile(g_fpcPfLst_ProfileList[proc])) {
                continue;
            }
            Case& c = sCases[sNumCases++];
            c.name[0] = '\0';
            c.params = 0;
            c.angleX = c.angleZ = 0;
            c.proc = proc;
        }
    }
    sFirst = 0;
    sLast = sNumCases - 1;
    const char* v = getenv("COS_COMBAT_SWEEP");
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
    if (end == nullptr || end == v || *end != '\0' || first < 0 || last < first) {
        writef(STDERR_FILENO, "[cos] combat-sweep: COS_COMBAT_SWEEP=\"%s\" is not <first>[-<last>]\n", v);
        pc_exit(PC_EXIT_USAGE);
    }
    if (list != nullptr && *list != '\0') {
        sFirst = (int)first;
        sLast = (int)last < sNumCases - 1 ? (int)last : sNumCases - 1;
    } else {
        // Process names: keep the cases inside [first, last].
        int lo = sNumCases, hi = -1;
        for (int i = 0; i < sNumCases; i++) {
            if (sCases[i].proc >= first && sCases[i].proc <= last) {
                lo = i < lo ? i : lo;
                hi = i;
            }
        }
        sFirst = lo;
        sLast = hi;
    }
}

// The inventory slot holding item, or -1.
int slotOf(u8 item) {
    for (int slot = 0; slot < dInvSlot_ItemLast_e; slot++) {
        if (dComIfGs_getItem(slot) == item) {
            return slot;
        }
    }
    return -1;
}

void setup() {
    dComIfGs_setMaxLife((u8)kMaxLife);
    dComIfGs_setLife(kMaxLife);
    // An empty bottle (boot item 50) gets a fairy, for the revival (execItemGet of the fairy
    // bottle item only sets its "got" flag).
    if (dComIfGs_checkBottle(dItemNo_EMPTY_BOTTLE_e) != 0) {
        dComIfGs_setBottleItemIn(dItemNo_EMPTY_BOTTLE_e, dItemNo_FAIRY_BOTTLE_e);
        writef(STDERR_FILENO, "[cos] combat-sweep: a fairy in the bottle\n");
    } else {
        writef(STDERR_FILENO, "[cos] combat-sweep: bottles %02x %02x %02x %02x\n",
               dComIfGs_getItem(dInvSlot_BOTTLE0_e), dComIfGs_getItem(dInvSlot_BOTTLE0_e + 1),
               dComIfGs_getItem(dInvSlot_BOTTLE0_e + 2), dComIfGs_getItem(dInvSlot_BOTTLE0_e + 3));
    }
    const u8 yz[] = {dItemNo_BOW_e, dItemNo_HOOKSHOT_e, dItemNo_BOOMERANG_e, dItemNo_SKULL_HAMMER_e};
    int btn = dItemBtn_Y_e;
    for (u8 item : yz) {
        const int slot = slotOf(item);
        if (slot < 0 || btn > dItemBtn_Z_e) {
            continue;
        }
        dComIfGs_setSelectItem(btn, (u8)slot);
        dComIfGp_setSelectItem(btn);
        btn++;
    }
    writef(STDERR_FILENO, "[cos] combat-sweep: hearts %u, items X %d Y %d Z %d\n", kMaxLife / 4,
           dComIfGs_getSelectItem(dItemBtn_X_e), dComIfGs_getSelectItem(dItemBtn_Y_e),
           dComIfGs_getSelectItem(dItemBtn_Z_e));
}

// The pad of fight frame t: buttons, and whether the stick walks towards the target.
u16 cycleButtons(unsigned int t, bool* walk, s8* sx, s8* sy) {
    *walk = false;
    *sx = 0;
    *sy = 0;
    if (t % kCycle < kIdleFrames) {
        return 0; // stand still, no lock: let the enemy come and hit
    }
    const unsigned int c = (t % kCycle) - kIdleFrames;
    u16 b = PAD_TRIGGER_L;
    if (c < 140) {
        *walk = true;
        if (c % 16 < 2) {
            b |= PAD_BUTTON_B;
        }
    } else if (c < 230) {
        // Spin attack: charge with B held, then the stick around (a quarter every 10 frames).
        const unsigned int s = c - 140;
        if (s < 85) {
            b |= PAD_BUTTON_B;
        }
        if (s >= 40 && s < 85) {
            const unsigned int q = ((s - 40) / 10) % 4;
            const s8 xs[] = {-127, 0, 127, 0};
            const s8 ys[] = {0, -127, 0, 127};
            *sx = xs[q];
            *sy = ys[q];
        }
    } else if (c < 270) {
        *walk = true;
        if (c % 20 < 2) {
            b |= PAD_BUTTON_A;
        }
    } else if (c < 310) {
        // X item (bombs: take one out, A throws it).
        if (c - 270 < 2) {
            b |= PAD_BUTTON_X;
        } else if (c - 270 >= 24 && c - 270 < 26) {
            b |= PAD_BUTTON_A;
        }
    } else if (c < 360) {
        // Y item (bow: ready, then hold to draw and release).
        const unsigned int s = c - 310;
        if (s < 2 || (s >= 12 && s < 40)) {
            b |= PAD_BUTTON_Y;
        }
    } else {
        // Z item (hookshot: ready, fire), then B to put it away.
        const unsigned int s = c - 360;
        if (s < 2 || (s >= 14 && s < 16)) {
            b |= PAD_TRIGGER_Z;
        } else if (s >= 30 && s < 32) {
            b |= PAD_BUTTON_B;
        }
    }
    return b;
}

// Steers the stick so the player walks towards world yaw `yaw` (CPad stick angle atan2(x, -y);
// the player's move yaw is stick angle + 0x8000 + the camera's controlled yaw).
void stickTowards(s16 yaw, s8* sx, s8* sy) {
    camera_class* cam = dComIfGp_getCamera(0);
    const s16 camYaw = cam != nullptr ? dCam_getControledAngleY(cam) : 0;
    const s16 a = (s16)(yaw - camYaw + 0x8000);
    *sx = (s8)(110.0f * cM_ssin(a));
    *sy = (s8)(-110.0f * cM_scos(a));
}

struct Nearest {
    fopAc_ac_c* player;
    fopAc_ac_c* best;
    float dist;
};

void* nearestEnemy(void* proc, void* data) {
    Nearest* n = static_cast<Nearest*>(data);
    if (!fopAc_IsActor(proc)) {
        return nullptr;
    }
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(proc);
    if (a == n->player || fopAcM_GetGroup(a) != fopAc_ENEMY_e) {
        return nullptr;
    }
    // Horizontal distance: a boss waiting below the arena for its intro (Molgera at y -20000)
    // is still walked to, and the intro starts.
    const float d = fopAcM_searchActorDistanceXZ(n->player, a);
    if (d < n->dist) {
        n->dist = d;
        n->best = a;
    }
    return nullptr;
}

// COS_COMBAT_DEATH: from kDeathStart frames into the case the player stops fighting and gets
// no refill (his life is cut to one heart once), so the enemy kills him.
constexpr unsigned int kDeathStart = 240;
constexpr unsigned int kGameOverFrames = 900; // dead (or in an event) that long since a death: game over
unsigned int sDeadFrames = 0;
bool sDeathCut = false;

bool dying() {
    return sDeathMode && sAlive > kDeathStart;
}

// One frame of the fight against target (nullptr: the nearest enemy actor, or wander).
void drivePad(fopAc_ac_c* target) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        setDrivenPad(true, 0, 0, 0);
        return;
    }
    if (target == nullptr) {
        Nearest n = {player, nullptr, 1e9f};
        fpcM_Search(nearestEnemy, &n);
        target = n.best;
    }
    bool walk = false;
    s8 sx = 0, sy = 0;
    u16 b = cycleButtons(sFight, &walk, &sx, &sy);
    if (dying()) {
        b = 0;
        walk = false;
        sx = sy = 0;

    }
    if (walk) {
        if (target != nullptr) {
            const float d = fopAcM_searchActorDistance(player, target);
            if (d > kApproach) {
                stickTowards(fopAcM_searchActorAngleY(player, target), &sx, &sy);
            }
        } else {
            if (sFight % 150 == 0) {
                sWanderYaw = (s16)(sWanderYaw + 0x5000);
            }
            stickTowards(sWanderYaw, &sx, &sy);
        }
    }
    if (target != nullptr) {
        const float d = fopAcM_searchActorDistance(player, target);
        sMinDist = d < sMinDist ? d : sMinDist;
    }
    setDrivenPad(true, b, sx, sy);
    sFight++;
}

void keepAlive(bool refill) {
    if (!refill && (!sDeathCut || (dComIfGs_getLife() > 4 && sDeaths < 2))) {
        // Once, and again after the fairy revived him.
        sDeathCut = true;
        dComIfGs_setLife(4);
        sLastLife = 4;
        writef(STDERR_FILENO, "[cos] combat-sweep: death mode: life cut to one heart, no refills\n");
    }
    const u16 life = dComIfGs_getLife();
    if (life < sLastLife) {
        sHits++;
    }
    if (life == 0 && sLastLife > 0) {
        sDeaths++;
        writef(STDERR_FILENO, "[cos] combat-sweep: the player died (%u)\n", sDeaths);
    }
    if (refill && life <= kLowLife && life > 0) {
        dComIfGs_setLife(kMaxLife);
        sRefills++;
    }
    sLastLife = dComIfGs_getLife();
    if (sFight % 60 == 0) {
        dComIfGs_setBombNum(30);
        dComIfGs_setArrowNum(30);
    }
    if (dComIfGp_getAttention().LockonTruth()) {
        sLocked++;
    }
}

void startCase() {
    sHpStart = sHpMin = 0;
    sHits = sRefills = sLocked = 0;
    sMinDist = 1e9f;
    sLastLife = dComIfGs_getLife();
    sGoneResult = nullptr;
    sFight = 0;
    sAlive = 0;
    sSince = 0;
    sWasExecuting = false;
}

void writeResult(const char* result) {
    if (sFd < 0) {
        return;
    }
    if (sMode == kRoomMode) {
        writef(sFd, "room - - - %s frames=%u hits=%u refills=%u locked=%u dist=%d deaths=%u\n",
               result, sAlive, sHits, sRefills, sLocked, sMinDist < 1e8f ? (int)sMinDist : -1,
               sDeaths);
        return;
    }
    writef(sFd, "%d %s %d %08x %s frames=%u hp=%d->%d hits=%u refills=%u locked=%u dist=%d\n", sCase,
           caseName(sCase), sCases[sCase].proc, sCases[sCase].params, result, sAlive, sHpStart,
           sHpMin, sHits, sRefills, sLocked, sMinDist < 1e8f ? (int)sMinDist : -1);
}

void finishCase(const char* result) {
    writeResult(result);
    setDrivenPad(true, 0, 0, 0);
    sState = kGap;
    sSince = 0;
    sId = fpcM_ERROR_PROCESS_ID_e;
}

void spawn() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    // In front of the player if there is floor there near his height, else the first of the
    // other seven directions that has (the Outset pier faces the sea).
    s16 yaw = player->shape_angle.y;
    cXyz pos = player->current.pos;
    for (int k = 0; k < 8; k++) {
        const s16 y = (s16)(player->shape_angle.y + k * 0x2000);
        cXyz p = player->current.pos;
        p.x += kSpawnDistance * cM_ssin(y);
        p.z += kSpawnDistance * cM_scos(y);
        dBgS_ObjGndChk gnd;
        cXyz probe(p.x, p.y + 200.0f, p.z);
        gnd.SetPos(&probe);
        const f32 h = dComIfG_Bgsp()->GroundCross(&gnd);
        if (h > player->current.pos.y - 150.0f && h < player->current.pos.y + 150.0f) {
            yaw = y;
            pos = p;
            pos.y = h;
            break;
        }
        if (k == 0) {
            pos = p;
        }
    }
    const Case& c = sCases[sCase];
    csXyz angle(c.angleX, (s16)(yaw + 0x8000), c.angleZ);
    const int room = fopAcM_GetRoomNo(player);
    if (sFd >= 0) {
        writef(sFd, "%d %s %d %08x begin\n", sCase, caseName(sCase), c.proc, c.params);
    }
    startCase();
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    if (c.name[0] != '\0') {
        char name[16];
        memcpy(name, c.name, sizeof(name));
        sId = fopAcM_create(name, c.params, &pos, room, &angle);
    } else {
        sId = fopAcM_create((s16)c.proc, c.params, &pos, room, &angle);
    }
    fpcLy_SetCurrentLayer(saved);
    if (sId == fpcM_ERROR_PROCESS_ID_e) {
        sRefused++;
        finishCase("refused-request");
        return;
    }
    sState = kFight;
}

void endSweep() {
    sState = kDone;
    setDrivenPad(false, 0, 0, 0);
    if (sMode == kSpawnMode) {
        writef(STDERR_FILENO, "[cos] combat-sweep: %d cases: %u died, %u timeout, %u refused, %u "
                              "deleted themselves, %u not gone after deletion\n",
               sLast - sFirst + 1, sDied, sTimeout, sRefused, sSelfDeleted, sStuck);
    }
    if (sFd >= 0) {
        close(sFd);
        sFd = -1;
    }
    pc_exit(PC_EXIT_REACHED);
}

void maybeShot(unsigned int frames) {
    if (sShotEvery != 0 && sAlive > 0 && (sAlive % sShotEvery == 0 || sAlive == 90)) {
        captureFrame(frames, shotSink, nullptr);
    }
}

} // namespace

void combatSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "combat-sweep") != 0) {
            return;
        }
        if (pc_boot_stage() == nullptr) {
            writef(STDERR_FILENO, "[cos] combat-sweep: needs COS_BOOT_STAGE (e.g. --stage sea:44:206)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        const char* mode = getenv("COS_COMBAT_MODE");
        if (mode != nullptr && strcmp(mode, "room") == 0) {
            sMode = kRoomMode;
        } else if (mode != nullptr && *mode != '\0' && strcmp(mode, "spawn") != 0) {
            writef(STDERR_FILENO, "[cos] combat-sweep: COS_COMBAT_MODE=\"%s\" is not spawn or room\n", mode);
            pc_exit(PC_EXIT_USAGE);
        }
        sFrames = envUint("COS_COMBAT_FRAMES", sMode == kRoomMode ? 6000 : 1200);
        sShotEvery = envUint("COS_COMBAT_SHOT_EVERY", sMode == kRoomMode ? 600 : 0);
        sDeathMode = envUint("COS_COMBAT_DEATH", 0) != 0;
        if (sMode == kSpawnMode) {
            loadCases();
        }
        sFd = openRunFile("combat_sweep.txt");
        if (sFd >= 0) {
            writef(sFd, "# combat-sweep (COS_SMOKE=combat-sweep, %s mode): <case> <name> <proc> "
                        "<params> <result> frames= hp=start->min hits= refills= locked= dist=; "
                        "\"begin\" is written before each spawn\n",
                   sMode == kRoomMode ? "room" : "spawn");
        }
        if (sMode == kSpawnMode) {
            writef(STDERR_FILENO, "[cos] combat-sweep: cases %d-%d of %d, %u frames each\n", sFirst,
                   sLast, sNumCases, sFrames);
        } else {
            writef(STDERR_FILENO, "[cos] combat-sweep: room mode, %u frames\n", sFrames);
        }
        sState = kWaitLink;
        sCase = sFirst - 1;
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
        setup();
        writef(STDERR_FILENO, "[cos] combat-sweep: the player in the room; fighting from frame %u\n",
               frames);
        if (sMode == kRoomMode) {
            startCase();
            sState = kFight;
            return;
        }
        sState = kNext;
    }

    // The stage left (game over, a floor master, a void-out restart to another stage).
    if (sState != kNext && sState != kGap) {
        if (const char* lost = sceneLost()) {
            if (sDeathMode && sDeaths > 0) {
                writef(STDERR_FILENO, "[cos] combat-sweep: %s after %u death(s) (death mode)\n", lost, sDeaths);
                writeResult("game-over");
                endSweep();
            }
            writef(STDERR_FILENO, "[cos] combat-sweep: case %d: %s; stopping\n", sCase, lost);
            sLeft++;
            writeResult(lost);
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
    }

    // Death mode: dead for good (no fairy left): the game-over screen (an event that never ends;
    // the game may already have given the life back for the continue); its save prompt is left
    // alone (no card writes) and the run ends.
    if (sDeathMode && sDeaths > 0 && (dComIfGs_getLife() == 0 || dComIfGp_event_runCheck())) {
        if (++sDeadFrames >= kGameOverFrames) {
            captureFrame(frames, shotSink, nullptr);
            writef(STDERR_FILENO, "[cos] combat-sweep: game over after %u death(s) (death mode)\n", sDeaths);
            writeResult("game-over");
            endSweep();
            return;
        }
    } else {
        sDeadFrames = 0;
    }

    if (sMode == kRoomMode) {
        sAlive++;
        keepAlive(!dying());
        drivePad(nullptr);
        maybeShot(frames);
        if (sAlive >= sFrames) {
            writeResult("done");
            endSweep();
        }
        return;
    }

    if (sState == kGap) {
        setDrivenPad(true, 0, 0, 0);
        if (sSince < kGapFrames) {
            return;
        }
        if (const char* lost = sceneLost()) {
            writef(STDERR_FILENO, "[cos] combat-sweep: after case %d: %s; stopping\n", sCase, lost);
            if (sFd >= 0) {
                writef(sFd, "%d %s %d %08x scene-lost:%s\n", sCase, caseName(sCase),
                       sCases[sCase].proc, sCases[sCase].params, lost);
            }
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        sState = kNext;
    }

    if (sState == kNext) {
        sCase++;
        if (sCase > sLast) {
            endSweep();
            return;
        }
        // Heal between cases.
        dComIfGs_setLife(kMaxLife);
        spawn();
        return;
    }

    sAlive++;
    fpc_ProcID id = sId;
    create_request* creating = static_cast<create_request*>(fpcCtIt_Judge(judgeCreatingById, &id));
    base_process_class* executing = fpcM_Search(judgeExecutingById, &id);
    fopAc_ac_c* enemy = nullptr;
    if (executing != nullptr) {
        if (!sWasExecuting && fopAc_IsActor(executing)) {
            sHpStart = sHpMin = static_cast<fopAc_ac_c*>(executing)->health;
        }
        sWasExecuting = true;
        if (fopAc_IsActor(executing)) {
            enemy = static_cast<fopAc_ac_c*>(executing);
            sHpMin = enemy->health < sHpMin ? enemy->health : sHpMin;
        }
    }

    if (sState == kFight || sState == kTail) {
        keepAlive(!dying());
        drivePad(enemy);
        maybeShot(frames);
    }

    if (sState == kFight) {
        if (creating == nullptr && executing == nullptr) {
            if (!sWasExecuting) {
                sRefused++;
                finishCase("refused");
            } else if (sAlive < kMinAliveFrames) {
                sSelfDeleted++;
                finishCase("self-deleted");
            } else {
                // Killed (or gone by itself later): keep fighting a while for its death effects
                // and drops.
                sDied++;
                sGoneResult = "died";
                sState = kTail;
                sSince = 0;
            }
            return;
        }
        if (sAlive < sFrames) {
            return;
        }
        if (executing != nullptr) {
            sTimeout++;
            sGoneResult = "timeout";
        } else {
            sGoneResult = "creating";
        }
        base_process_class* proc = executing != nullptr ? executing : creating->mpRes;
        if (const unsigned int children = deleteChildren(sId)) {
            writef(STDERR_FILENO, "[cos] combat-sweep: case %d (%s): %u child actor(s) deleted with it\n",
                   sCase, caseName(sCase), children);
        }
        fpcM_Delete(proc);
        sState = kDeleting;
        sSince = 0;
        return;
    }

    if (sState == kTail) {
        if (sSince >= kGoneTailFrames) {
            deleteChildren(sId);
            finishCase(sGoneResult);
        }
        return;
    }

    // kDeleting
    setDrivenPad(true, 0, 0, 0);
    if (creating == nullptr && executing == nullptr) {
        finishCase(sGoneResult);
        return;
    }
    if (sSince >= kDeleteFrames) {
        sStuck++;
        finishCase("not-deleted");
        return;
    }
    if ((sSince & 15) == 0) {
        fpcM_Delete(executing != nullptr ? executing : creating->mpRes);
    }
}

} // namespace pc
