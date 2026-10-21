// COS_SMOKE=rando COS_RANDO_SMOKE=trap (docs/RANDOMIZER_PLAN.md R6.5): a trap chest, opened.
//
//   run.sh rando --stage LinkRM:0:0 --env COS_RANDO_SMOKE=trap
//                --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R6_5_trap_placement.json
//
// The placement (fixtures/R6_5_make_fixture.py) is R0.6's identity with "Ice Trap Chest" under Link's
// house (LinkUG Stage TRES 0, an Orange Rupee chest on the disc), trap_chests and
// chest_type_matches_contents on. The run boots elsewhere, registers an observer of every
// event_list.dat after the randomizer's own patchers, then goes to LinkUG (point 0, room 0) and
// checks, once the player stands there:
//   T1: the chest plan (features/rando/pc_rando_trap.h) has the trap there; the patcher wrote params
//       bit 0x40 and chest type 2 (metal: a trap is a progress item); the live chest is a trap
//       (daTbox_c::getIsTrap), of shape 2, and still holds the disc's item (0x06); LinkUG's
//       event_list.dat as the game got it is wwrando's file after add_trap_chest_event_to_stage
//       (FNV-1a 64 of fixtures/R6_5_wwrando_disc.txt) and the event manager knows
//       DEFAULT_TREASURE_TRAP;
//   T2: the player is put in front of the chest at full health and opens it with a driven A; the
//       event is DEFAULT_TREASURE_TRAP; the player gets frozen (daPyFlg1_FREEZE_STATE) and later
//       thawed; the life drops by 2 (quarter hearts); no item: the rupees do not change (the chest's
//       Orange Rupee is not given), the item-get stays 0xFF, and the event ends; the chest's opened
//       flag is set.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_event_ser.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_trap.h"

#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_tbox.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "trap";
constexpr const char* kStage = "LinkUG";
constexpr int kLoc = PCR_LOC_OUTSET_ISLAND_UNDERNEATH_LINKS_HOUSE;
constexpr unsigned int kSettle = 30;
constexpr unsigned int kArriveFrames = 1500;
constexpr unsigned int kWalkFrames = 300;
constexpr double kEventSeconds = 60.0;
constexpr float kFront = 130.0f;

enum State { kInit, kArrive, kPrep, kWalk, kEvent, kDone };
State sState = kInit;
unsigned int sSince = 0, sPresent = 0;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
fpc_ProcID sChest = fpcM_ERROR_PROCESS_ID_e;
cXyz sTarget;

// The observer (runs after the randomizer's patchers: registered once they are).
bool sObserved = false;
uint64_t sObservedFnv = 0;
size_t sObservedSize = 0;

// Fixture R6_5_wwrando_disc.txt: LinkUG's trap file.
bool sFixtureOk = false;
uint64_t sWantFnv = 0;
size_t sWantSize = 0;

// T2 observations
int sStartLife = 0, sStartRupee = 0;
int sGtItm = 0xFF;
bool sFrozen = false, sThawed = false;
char sEvent[48] = "";
uint64_t sEventNs = 0;

void* findPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(findPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

void observer(PcRandoResFile* f) {
    if (strcmp(f->stage, kStage) == 0 && strcmp(f->arc, "Stage") == 0 && strcmp(f->name, "event_list.dat") == 0) {
        sObserved = true;
        sObservedSize = f->size;
        sObservedFnv = pcr::pc_rando_evser_fnv64(f->data, f->size);
    }
}

bool readFixture() {
    char file[1024];
    snprintf(file, sizeof file, "%s", __FILE__); // <native>/src/pc/harness/smokes/<this file>
    for (int up = 0; up < 5; up++) {
        char* slash = strrchr(file, '/');
        if (slash == nullptr) {
            return false;
        }
        *slash = '\0';
    }
    std::vector<std::string> tries;
    const char* env = std::getenv("COS_RANDO_FIXTURES");
    if (env != nullptr && env[0] != '\0') {
        tries.push_back(env);
    }
    tries.push_back(std::string(file) + "/check/rando/fixtures");
    const char* run = std::getenv("COS_RUN_DIR");
    if (file[0] != '/' && run != nullptr && run[0] != '\0') {
        tries.push_back(std::string(run) + "/../../" + file + "/check/rando/fixtures");
    }
    for (const std::string& dir : tries) {
        FILE* fp = std::fopen((dir + "/R6_5_wwrando_disc.txt").c_str(), "rb");
        if (fp == nullptr) {
            continue;
        }
        char line[256];
        while (std::fgets(line, sizeof line, fp) != nullptr) {
            char stage[32];
            unsigned size = 0, trapSize = 0;
            unsigned long long fnv = 0, trapFnv = 0;
            if (sscanf(line, "disc %31s %u %llx %u %llx", stage, &size, &fnv, &trapSize, &trapFnv) == 5 &&
                strcmp(stage, kStage) == 0) {
                sWantSize = trapSize;
                sWantFnv = trapFnv;
                std::fclose(fp);
                return true;
            }
        }
        std::fclose(fp);
    }
    return false;
}

struct Chests {
    int n;
    daTbox_c* list[32];
};

void* collectTbox(void* proc, void* data) {
    Chests* c = static_cast<Chests*>(data);
    if (fpcM_GetName(proc) == fpcNm_TBOX_e && c->n < 32) {
        c->list[c->n++] = static_cast<daTbox_c*>(proc);
    }
    return nullptr;
}

const pcr::LocKey* chestKey(int* index) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    for (size_t i = 0; i < keys.size; i++) {
        if (keys[i].loc == kLoc && keys[i].kind == pcr::KeyKind::Dzx && keys[i].field == pcr::DzxField::TboxItem) {
            *index = (int)i;
            return &keys[i];
        }
    }
    return nullptr;
}

// In front of the chest, on the floor (as smokes/pc_rando_smoke_loc.cpp).
cXyz frontOf(const cXyz& c, s16 yaw) {
    static const float kDist[] = {kFront, 100.0f, 170.0f, 80.0f};
    for (float d : kDist) {
        cXyz p(c.x + d * cM_ssin(yaw), c.y, c.z + d * cM_scos(yaw));
        dBgS_ObjGndChk gnd;
        cXyz probe(p.x, c.y + 150.0f, p.z);
        gnd.SetPos(&probe);
        const f32 h = dComIfG_Bgsp()->GroundCross(&gnd);
        if (h > c.y - 100.0f && h < c.y + 100.0f) {
            p.y = h;
            return p;
        }
    }
    return cXyz(c.x + kFront * cM_ssin(yaw), c.y + 10.0f, c.z + kFront * cM_scos(yaw));
}

void stickToward(fopAc_ac_c* player, const cXyz& c) {
    const s16 a = cM_atan2s(c.x - player->current.pos.x, c.z - player->current.pos.z);
    const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
    setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 70.0f), (int8_t)(-cM_scos(st) * 70.0f));
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

void finish() {
    setDrivenPad(false, 0, 0, 0);
    sState = kDone;
    randoSmokeEnd(kName);
}

// T1 once the player stands in LinkUG.
void checkStatic() {
    char what[160];
    const pcr::ChestPlan* plan = pc_rando_trap_plan();
    randoSmokeCheck(kName, "pc_rando() is on with a seed", pc_rando() == 1 && pc_rando_seed() != nullptr);
    randoSmokeCheck(kName, "the chest plan puts a trap under Link's house",
                    plan != nullptr && plan->numTraps == 1 && pc_rando_trap_loc(kLoc) == 1 && plan->ctmc &&
                        plan->trapStages.size() == 1 && plan->trapStages[0] == kStage);
    int keyIdx = -1;
    const pcr::LocKey* key = chestKey(&keyIdx);
    const PcRandoTrapKeyStatus* st = key != nullptr ? pc_rando_trap_key_status(keyIdx) : nullptr;
    snprintf(what, sizeof what, "the patcher wrote the trap bit and chest type 2 (params 0x%08X)",
             st != nullptr ? (unsigned)st->params : 0u);
    randoSmokeCheck(kName, what,
                    st != nullptr && st->result == PCR_TRAP_KEY_DONE && (st->params & PC_RANDO_TBOX_TRAP_BIT) != 0 &&
                        ((st->params & PC_RANDO_TBOX_TYPE_MASK) >> PC_RANDO_TBOX_TYPE_SHIFT) == 2);
    Chests chests = {};
    fopAcM_Search(collectTbox, &chests);
    daTbox_c* chest = chests.n == 1 ? chests.list[0] : nullptr;
    snprintf(what, sizeof what, "one live chest, a trap of shape 2 holding the disc's item (%d chests; trap %d, shape %d, item 0x%02X)",
             chests.n, chest != nullptr ? (int)chest->getIsTrap() : -1, chest != nullptr ? (int)chest->getShapeType() : -1,
             chest != nullptr ? (unsigned)chest->getItemNo() : 0xFFu);
    randoSmokeCheck(kName, what,
                    chest != nullptr && chest->getIsTrap() && chest->getShapeType() == 2 && key != nullptr &&
                        chest->getItemNo() == key->disc_byte);
    sChest = chest != nullptr ? fopAcM_GetID(chest) : fpcM_ERROR_PROCESS_ID_e;
    snprintf(what, sizeof what, "LinkUG's event_list.dat is wwrando's with the trap event (%zu bytes, %016llx; want %zu, %016llx)",
             sObservedSize, (unsigned long long)sObservedFnv, sWantSize, (unsigned long long)sWantFnv);
    randoSmokeCheck(kName, what, sFixtureOk && sObserved && sObservedFnv == sWantFnv && sObservedSize == sWantSize);
    randoSmokeCheck(kName, "the event manager has DEFAULT_TREASURE_TRAP",
                    dComIfGp_getPEvtManager()->getEventIdx("DEFAULT_TREASURE_TRAP", 0xFF) >= 0);
}

} // namespace

void randoSmokeTrap(unsigned int frames) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const fpc_ProcID scene = playSceneId();
    sSince++;
    switch (sState) {
    case kInit:
        if (!randoSmokeInPlay(kSettle)) {
            return;
        }
        // The randomizer's patchers registered at the first archive load (boot stage): this one runs
        // after them.
        pc_rando_register_res_patcher(observer, PCR_RES_DAT);
        sFixtureOk = readFixture();
        randoSmokeCheck(kName, "fixture R6_5_wwrando_disc.txt has LinkUG", sFixtureOk);
        writef(STDERR_FILENO, "[cos] rando-smoke trap: frame %u: to %s\n", frames, kStage);
        sOldScene = scene;
        dComIfGp_setNextStage(kStage, 0, 0, -1);
        sState = kArrive;
        sSince = 0;
        return;
    case kArrive:
        if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr ||
            strcmp(dComIfGp_getStartStageName(), kStage) != 0) {
            sPresent = 0;
            if (sSince >= kArriveFrames) {
                randoSmokeCheck(kName, "LinkUG reached", false);
                finish();
            }
            return;
        }
        if (++sPresent < kSettle) {
            return;
        }
        checkStatic();
        sState = kPrep;
        sSince = 0;
        return;
    case kPrep: {
        daTbox_c* chest = (daTbox_c*)fopAcM_SearchByID(sChest);
        if (chest == nullptr || player == nullptr) {
            randoSmokeCheck(kName, "the chest is there to open", false);
            finish();
            return;
        }
        if (dComIfGp_event_runCheck() || sSince < 30) {
            setDrivenPad(true, 0, 0, 0);
            if (sSince > 900) {
                randoSmokeCheck(kName, "no event before the open", false);
                finish();
            }
            return;
        }
        const s16 yaw = chest->home.angle.y;
        sTarget = frontOf(chest->home.pos, yaw);
        static_cast<daPy_lk_c*>(player)->debugPlace(sTarget, (s16)(yaw + 0x8000));
        dComIfGs_setLife(dComIfGs_getMaxLife());
        sStartLife = dComIfGs_getLife();
        sStartRupee = dComIfGs_getRupee();
        dComIfGp_event_setGtItm(0xFF);
        writef(STDERR_FILENO, "[cos] rando-smoke trap: frame %u: player put at (%.0f, %.0f, %.0f); life %d, rupees %d\n",
               frames, sTarget.x, sTarget.y, sTarget.z, sStartLife, sStartRupee);
        sState = kWalk;
        sSince = 0;
        return;
    }
    case kWalk: {
        daTbox_c* chest = (daTbox_c*)fopAcM_SearchByID(sChest);
        if (chest == nullptr || player == nullptr) {
            randoSmokeCheck(kName, "the chest is there to open", false);
            finish();
            return;
        }
        if (dComIfGp_event_runCheck()) {
            snprintf(sEvent, sizeof sEvent, "%s", eventName());
            writef(STDERR_FILENO, "[cos] rando-smoke trap: frame %u: event \"%s\"\n", frames, sEvent);
            sState = kEvent;
            sSince = 0;
            sEventNs = monotonicNs();
            return;
        }
        if (dComIfGp_getDoStatus() == dActStts_OPEN_e) {
            setDrivenPad(true, sSince % 15 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        } else {
            stickToward(player, chest->home.pos);
        }
        if (sSince >= kWalkFrames) {
            randoSmokeCheck(kName, "A opens the chest", false);
            finish();
        }
        return;
    }
    case kEvent: {
        if (dComIfGp_event_getGtItm() != 0xFF) {
            sGtItm = dComIfGp_event_getGtItm();
        }
        daPy_lk_c* link = static_cast<daPy_lk_c*>(player);
        if (link != nullptr) {
            const bool frozen = link->checkNoResetFlg1(daPy_py_c::daPyFlg1_FREEZE_STATE) != 0;
            if (frozen && !sFrozen) {
                writef(STDERR_FILENO, "[cos] rando-smoke trap: frame %u: frozen (event frame %u), life %d\n", frames, sSince,
                       (int)dComIfGs_getLife());
            }
            if (sFrozen && !frozen && !sThawed) {
                sThawed = true;
                writef(STDERR_FILENO, "[cos] rando-smoke trap: frame %u: thawed (event frame %u)\n", frames, sSince);
            }
            sFrozen = sFrozen || frozen;
        }
        setDrivenPad(true, sSince % 45 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            if ((monotonicNs() - sEventNs) / 1e9 >= kEventSeconds) {
                randoSmokeCheck(kName, "the trap event ends", false);
                finish();
            }
            return;
        }
        char what[160];
        writef(STDERR_FILENO, "[cos] rando-smoke trap: frame %u: event \"%s\" ended after %u frames\n", frames, sEvent, sSince);
        randoSmokeCheck(kName, "the chest's event is DEFAULT_TREASURE_TRAP", strcmp(sEvent, "DEFAULT_TREASURE_TRAP") == 0);
        randoSmokeCheck(kName, "the player froze and thawed", sFrozen && sThawed);
        const bool stillFrozen =
            link != nullptr && link->checkNoResetFlg1(daPy_py_c::daPyFlg1_FREEZE_STATE) != 0;
        randoSmokeCheck(kName, "the player is not frozen after the event", !stillFrozen);
        snprintf(what, sizeof what, "2 quarter hearts of damage (life %d -> %d)", sStartLife, (int)dComIfGs_getLife());
        randoSmokeCheck(kName, what, (int)dComIfGs_getLife() == sStartLife - 2);
        snprintf(what, sizeof what, "no item: rupees %d -> %d, item-get 0x%02X", sStartRupee, dComIfGs_getRupee(), sGtItm);
        randoSmokeCheck(kName, what, dComIfGs_getRupee() == sStartRupee && sGtItm == 0xFF);
        daTbox_c* chest = (daTbox_c*)fopAcM_SearchByID(sChest);
        randoSmokeCheck(kName, "the chest is open (its tbox flag set)", chest != nullptr && dComIfGs_isTbox(chest->getTboxNo()));
        finish();
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc

namespace {

// A relative COS_RANDO_PLACEMENT is taken from the repo's root (as smokes/pc_rando_smoke_loc.cpp),
// before main, so before R0.7's debug boot reads it.
struct ResolvePlacement {
    ResolvePlacement() {
        const char* smoke = getenv("COS_SMOKE");
        const char* name = getenv("COS_RANDO_SMOKE");
        const char* path = getenv("COS_RANDO_PLACEMENT");
        if (smoke == nullptr || strcmp(smoke, "rando") != 0 || name == nullptr || strcmp(name, "trap") != 0 ||
            path == nullptr || path[0] == '\0' || path[0] == '/' || access(path, R_OK) == 0) {
            return;
        }
        std::string root = __FILE__; // <root>/native/src/pc/harness/smokes/<this file>
        for (int up = 0; up < 6; up++) {
            const size_t slash = root.find_last_of('/');
            if (slash == std::string::npos) {
                return;
            }
            root.resize(slash);
        }
        std::vector<std::string> tries = {root + "/" + path};
        const char* run = getenv("COS_RUN_DIR");
        if (root[0] != '/' && run != nullptr && run[0] != '\0') {
            tries.push_back(std::string(run) + "/../../" + root + "/" + path);
        }
        for (const std::string& t : tries) {
            if (access(t.c_str(), R_OK) == 0) {
                setenv("COS_RANDO_PLACEMENT", t.c_str(), 1);
                return;
            }
        }
    }
} sResolvePlacement;

} // namespace
