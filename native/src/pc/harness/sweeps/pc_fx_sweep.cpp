// COS_SMOKE=fx-sweep: draw every particle emitter the stage has loaded, a few at a time, in front of
// the player, so each emitter's draw state reaches Aurora once and its pipelines get recorded
// (native/tools/gen_pipeline_cache.sh, tier fx-sweep; docs/SWITCH_BUILD.md, "Pipeline precompile").
// With the debug stage boot (COS_BOOT_STAGE), once the player is in the start room (the M12 probe,
// pc_outset.cpp) and kSettleFrames later:
// - the emitters are those of the common particle resource (common.jpc, resource manager 0) and of
//   the stage's scene resource (Pscene*.jpc, manager 1, user indexes with bit 15 set), each listed in
//   the order of its resource; COS_FX_SWEEP=common|scene|all (default all) picks them;
//   COS_FX_SWEEP=disc takes the common ones, then every /res/Particle/Pscene*.jpc of the disc in
//   turn, whichever stage uses it: each loaded into a heap of this sweep and its resource manager
//   put in the scene manager's place (JPAEmitterManager::pResMgrArray[1]) while its emitters run,
//   the stage's own put back at the end;
// - each one is created twice, in the Normal and the Toon draw groups (dPa_control_c::setNormal /
//   setToon, the groups most game effects use; m_Do_graphic draws them in different passes), with
//   no callback but the heat-haze one dPa_control_c::set gives to user indexes with bit 14 set;
// - kBatch at a time, side by side kSpawnDistance in front of the player and kRise above his feet,
//   run kRunFrames game frames, then deleted (forceDeleteEmitter, only if the emitter is still in
//   its group's list at the place it was created: one that ended by itself may already be reused);
// - after each batch the PLAY scene and the player must still be there (exit 1 otherwise), and
//   when every emitter has run the sweep exits 0.
// <COS_RUN_DIR>/fx_sweep.txt gets a "begin" line before each batch (written straight to the file,
// so it names the emitters when a fault ends the process) and the counts at the end. The sweep
// creates and deletes only its own emitters; what they draw is whatever the resource says.
#include "pc_internal.h"

#include "SSystem/SComponent/c_math.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAEmitterLoader.h"
#include "JSystem/JParticle/JPAEmitterManager.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_graphic.h"

#include <dolphin/dvd.h>
#include "d/d_com_inf_game.h"
#include "d/d_particle.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60; // after the player is in the room, before the first batch
constexpr unsigned int kRunFrames = 45;    // per batch
constexpr unsigned int kGapFrames = 2;     // between one batch deleted and the next
constexpr int kBatch = 6;
constexpr int kMaxIds = 4096;
constexpr float kSpawnDistance = 260.0f;
constexpr float kSpacing = 70.0f;
constexpr float kRise = 60.0f;

enum State { kOff, kWaitLink, kNext, kRunning, kGap, kDone };

struct Slot {
    JPABaseEmitter* emtr;
    u8 group;
    cXyz pos;
};

State sState = kOff;
bool sChecked = false;
u16 sIds[kMaxIds];
int sNumIds = 0;
int sNumCommon = 0;
int sNext = 0;   // index into sIds x groups: (pass * sNumIds + i)
int sPasses = 2; // Normal, Toon
Slot sSlots[kBatch];
int sNumSlots = 0;
unsigned int sSince = 0;
int sFd = -1;
unsigned int sCreated = 0, sRefused = 0, sEnded = 0, sDeleted = 0, sBatches = 0;
// COS_FX_SWEEP=disc: every /res/Particle/Pscene*.jpc of the disc in turn, each in its own resource
// manager put in the scene manager's place (pResMgrArray[1]) while its emitters run.
bool sDisc = false;
char sJpcs[128][48];
int sNumJpcs = 0, sJpc = -1;
JKRExpHeap* sJpcHeap = nullptr;
JPAResourceManager* sSavedScene = nullptr;
unsigned int sJpcsSwept = 0;

const u8 kGroups[2] = {dPa_control_c::dPtclGroup_Normal_e, dPa_control_c::dPtclGroup_Toon_e};
const char* const kGroupNames[2] = {"normal", "toon"};

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

const char* sceneLost() {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return "PLAY scene gone";
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr || fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        return "player gone";
    }
    return nullptr;
}

void addIds(JPAResourceManager* mng, bool scene) {
    if (mng == nullptr || mng->getEmitterResource() == nullptr) {
        return;
    }
    JPAEmitterResource* res = mng->getEmitterResource();
    for (u32 i = 0; i < res->registNum && sNumIds < kMaxIds; i++) {
        JPAEmitterData* data = res->pEmtrResArray[i];
        if (data == nullptr) {
            continue;
        }
        const u16 id = data->getUserIndex();
        // dPa_control_c::getRM_ID: bit 15 picks the scene resource; an index of the other kind
        // would be looked up in the wrong manager.
        if (((id & 0x8000) != 0) != scene) {
            continue;
        }
        sIds[sNumIds++] = id;
    }
}

bool stillOurs(const Slot& s) {
    JPAEmitterManager* mng = dPa_control_c::getEmitterManager();
    if (mng == nullptr || s.emtr == nullptr) {
        return false;
    }
    for (JSULink<JPABaseEmitter>* link = mng->mEmtrGroup[s.group].getFirst(); link != nullptr;
         link = link->getNext()) {
        if (link->getObject() == s.emtr) {
            JGeometry::TVec3<f32> t;
            s.emtr->getGlobalTranslation(t);
            return t.x == s.pos.x && t.y == s.pos.y && t.z == s.pos.z;
        }
    }
    return false;
}

void deleteBatch() {
    JPAEmitterManager* mng = dPa_control_c::getEmitterManager();
    for (int i = 0; i < sNumSlots; i++) {
        if (stillOurs(sSlots[i])) {
            mng->forceDeleteEmitter(sSlots[i].emtr);
            sDeleted++;
        } else if (sSlots[i].emtr != nullptr) {
            sEnded++;
        }
    }
    sNumSlots = 0;
}

void spawnBatch() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    dPa_control_c* particle = g_dComIfG_gameInfo.play.getParticle();
    const s16 yaw = player->shape_angle.y;
    const float fx = cM_ssin(yaw), fz = cM_scos(yaw); // forward
    const float sx = fz, sz = -fx;                     // sideways
    sNumSlots = 0;
    if (sFd >= 0) {
        writef(sFd, "begin");
    }
    const int total = sNumIds * sPasses;
    for (int k = 0; k < kBatch && sNext < total; k++, sNext++) {
        const int pass = sNext / sNumIds;
        const u16 id = sIds[sNext % sNumIds];
        const float side = ((float)k - (kBatch - 1) * 0.5f) * kSpacing;
        Slot& s = sSlots[sNumSlots++];
        s.group = kGroups[pass];
        s.pos = player->current.pos;
        s.pos.x += kSpawnDistance * fx + side * sx;
        s.pos.z += kSpawnDistance * fz + side * sz;
        s.pos.y += kRise;
        if (sFd >= 0) {
            writef(sFd, " %s:0x%04X", kGroupNames[pass], id);
        }
        s.emtr = particle->set(s.group, id, &s.pos, nullptr, nullptr, 0xFF, nullptr, -1, nullptr,
                               nullptr, nullptr);
        if (s.emtr == nullptr) {
            sRefused++;
        } else {
            sCreated++;
        }
    }
    if (sFd >= 0) {
        writef(sFd, "\n");
    }
    sBatches++;
}

void listJpcs() {
    DVDDir dir;
    if (!DVDOpenDir("/res/Particle", &dir)) {
        return;
    }
    DVDDirEntry entry;
    while (DVDReadDir(&dir, &entry) && sNumJpcs < 128) {
        if (entry.name == nullptr || entry.isDir || strncmp(entry.name, "Pscene", 6) != 0) {
            continue;
        }
        snprintf(sJpcs[sNumJpcs++], sizeof(sJpcs[0]), "/res/Particle/%s", entry.name);
    }
    DVDCloseDir(&dir);
}

// The next scene JPC of the disc in the scene manager's place, its emitters in sIds; false when
// none is left (the stage's own scene manager is back).
bool nextJpc() {
    JPAEmitterManager* emng = dPa_control_c::getEmitterManager();
    if (sJpcHeap != nullptr) {
        sJpcHeap->freeAll();
    }
    for (;;) {
        sJpc++;
        if (sJpc >= sNumJpcs) {
            emng->pResMgrArray[1] = sSavedScene;
            return false;
        }
        void* data = JKRDvdRipper::loadToMainRAM(sJpcs[sJpc], nullptr, EXPAND_SWITCH_UNKNOWN1, 0,
                                                 sJpcHeap, JKRDvdRipper::ALLOC_DIRECTION_FORWARD,
                                                 0, nullptr);
        if (data == nullptr) {
            writef(STDERR_FILENO, "[cos] fx-sweep: %s not read\n", sJpcs[sJpc]);
            continue;
        }
        JPAResourceManager* mng = new (sJpcHeap, 0) JPAResourceManager(data, sJpcHeap);
        if (mng == nullptr) {
            sJpcHeap->freeAll();
            continue;
        }
        mng->swapTexture(mDoGph_gInf_c::getFrameBufferTimg(), "AK_kagerouSwap00");
        emng->pResMgrArray[1] = mng;
        sNumIds = 0;
        addIds(mng, true);
        sNext = 0;
        sJpcsSwept++;
        if (sFd >= 0) {
            writef(sFd, "jpc %s: %d emitters\n", sJpcs[sJpc], sNumIds);
        }
        if (sNumIds > 0) {
            return true;
        }
    }
}

} // namespace

void fxSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "fx-sweep") != 0) {
            return;
        }
        if (pc_boot_stage() == nullptr) {
            writef(STDERR_FILENO, "[cos] fx-sweep: needs COS_BOOT_STAGE (e.g. --stage sea:44:0)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        sFd = openRunFile("fx_sweep.txt");
        if (sFd >= 0) {
            writef(sFd, "# fx-sweep (COS_SMOKE=fx-sweep): \"begin\" and the batch's <group>:<user "
                        "index> before each batch, the counts at the end\n");
        }
        sState = kWaitLink;
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
        const char* which = getenv("COS_FX_SWEEP");
        if (which == nullptr || *which == '\0') {
            which = "all";
        }
        sDisc = strcmp(which, "disc") == 0;
        const bool common = sDisc || strcmp(which, "all") == 0 || strcmp(which, "common") == 0;
        const bool scene = strcmp(which, "all") == 0 || strcmp(which, "scene") == 0;
        if (!common && !scene) {
            writef(STDERR_FILENO, "[cos] fx-sweep: COS_FX_SWEEP=\"%s\" is not common, scene, all "
                                  "or disc\n", which);
            pc_exit(PC_EXIT_USAGE);
        }
        dPa_control_c* particle = g_dComIfG_gameInfo.play.getParticle();
        if (particle == nullptr || dPa_control_c::getEmitterManager() == nullptr) {
            writef(STDERR_FILENO, "[cos] fx-sweep: no particle control in the PLAY scene\n");
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        if (common) {
            addIds(particle->mCommonResMng, false);
        }
        if (sDisc) {
            listJpcs();
            sJpcHeap = JKRExpHeap::create(16 << 20, JKRHeap::getRootHeap(), false);
            if (sJpcHeap == nullptr) {
                writef(STDERR_FILENO, "[cos] fx-sweep: no heap for the scene JPCs (root free %d)\n",
                       (int)JKRHeap::getRootHeap()->getTotalFreeSize());
                pc_exit(PC_EXIT_CHECK_FAILED);
            }
            sSavedScene = dPa_control_c::getEmitterManager()->pResMgrArray[1];
            writef(STDERR_FILENO, "[cos] fx-sweep: COS_FX_SWEEP=disc: the common emitters, then "
                                  "the %d scene JPCs of /res/Particle\n", sNumJpcs);
        }
        sNumCommon = sNumIds;
        if (scene) {
            addIds(particle->mSceneResMng, true);
        }
        writef(STDERR_FILENO, "[cos] fx-sweep: the player in the room; %d emitters (%d common, %d "
                              "scene of scene resource %u, COS_FX_SWEEP=%s) x %d groups, %d at a "
                              "time, %u frames each, from frame %u\n",
               sNumIds, sNumCommon, sNumIds - sNumCommon, (unsigned)particle->mSceneNo, which, sPasses, kBatch, kRunFrames,
               frames);
        sState = kNext;
    }

    if (sState == kGap) {
        if (sSince < kGapFrames) {
            return;
        }
        if (const char* lost = sceneLost()) {
            writef(STDERR_FILENO, "[cos] fx-sweep: after batch %u: %s; stopping\n", sBatches, lost);
            if (sFd >= 0) {
                writef(sFd, "scene-lost: %s\n", lost);
            }
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        sState = kNext;
    }

    if (sState == kNext) {
        if (sNext >= sNumIds * sPasses && sDisc && nextJpc()) {
            // the next JPC's emitters (sIds, sNext reset)
        }
        if (sNext >= sNumIds * sPasses) {
            sState = kDone;
            writef(STDERR_FILENO, "[cos] fx-sweep: %u batches: %u emitters created, %u refused, %u "
                                  "deleted after %u frames, %u ended by themselves; %u scene JPCs "
                                  "of the disc\n",
                   sBatches, sCreated, sRefused, sDeleted, kRunFrames, sEnded, sJpcsSwept);
            if (sFd >= 0) {
                writef(sFd, "done: %u batches, %u created, %u refused, %u deleted, %u ended\n",
                       sBatches, sCreated, sRefused, sDeleted, sEnded);
                close(sFd);
                sFd = -1;
            }
            pc_exit(PC_EXIT_REACHED);
        }
        spawnBatch();
        sState = kRunning;
        sSince = 0;
        return;
    }

    // kRunning
    if (sSince < kRunFrames) {
        return;
    }
    deleteBatch();
    sState = kGap;
    sSince = 0;
}

} // namespace pc
