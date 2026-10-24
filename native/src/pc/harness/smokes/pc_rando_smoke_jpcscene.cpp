// COS_SMOKE=rando COS_RANDO_SMOKE=jpcscene (docs/RANDOMIZER_PLAN.md step R7.1b): the particle bank hook
// (game_hooks/pc_rando_hooks_particles.cpp, registry features/rando/pc_rando_jpc_scene.h), e.g.
//   run.sh rando --stage Siren:6 --env COS_RANDO_SMOKE=jpcscene --env COS_RANDO_FORCE=1
// Under pc_rando(), from the first frame on (before the stage's particles load), it registers for
// Pscene050 (Tower of the Gods, which lacks them): an import of the warp pot particle 0x8161 from
// Pscene035 (as add_inter_dungeon_warp_pots), an import of 0x8162 from "any" (as
// update_loaded_particles), an editor that sets 0x8161's BSP1 prm colour to kSceneColor, and a
// common.jpc editor that sets particle 0x48's BSP1 prm colour (the arrow trail) to kCommonColor.
// Once the play scene has run 10 frames:
//   - the scene is Pscene050 and the disc's Pscene050 lacks both ids (so the test means something);
//   - pc_rando(): the hook built a merged copy (2 imported), equal byte for byte to the JPC library's
//     own load + copy (0x8161 from 035, 0x8162 from the first Pscene000-254 file holding it, found
//     here by an independent scan) + save + the colour edit; the copy is not the loaded file, which
//     still equals the disc file; the scene resource manager has both ids, and
//     dComIfGp_particle_set returns an emitter for each; common.jpc's 0x48 has kCommonColor. The
//     merged bytes go to $COS_RUN_DIR/jpcscene_Pscene050.jpc.
//   - without pc_rando(): the hook never ran, the bytes the manager was built from equal the disc
//     file, the manager lacks 0x8161 and common.jpc's 0x48 keeps the disc colour.
// Under pc_rando() the stage is then entered again (removeRoomScene destroys the scene heap holding
// the merged copy): 30 frames later the new scene is merged again and 0x8161 still makes an emitter.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_jpc.h"
#include "features/rando/pc_rando_jpc_scene.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "d/d_com_inf_game.h"
#include "d/d_particle.h"

#include "cos_sdk/host_alloc.h"

#include <dolphin/dvd.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "jpcscene";
constexpr int kScene = 50;
constexpr int kSource = 35;
constexpr uint16_t kIdFixed = 0x8161; // from Pscene035
constexpr uint16_t kIdAny = 0x8162;   // from "any"
constexpr uint16_t kCommonId = 0x48;
constexpr PcRandoJpcRgba kSceneColor = {0x12, 0x34, 0x56, 0x78};
constexpr PcRandoJpcRgba kCommonColor = {0xE5, 0x10, 0x1B, 0x80};

bool sRegistered = false;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

uint32_t fnv(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

bool readDisc(const char* path, cos_sdk::HostVector<uint8_t>* out) {
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        return false;
    }
    out->resize(info.length);
    const s32 got = DVDReadPrio(&info, out->data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    return got == (s32)info.length;
}

bool readScene(int scene, cos_sdk::HostVector<uint8_t>* out) {
    char path[48];
    snprintf(path, sizeof path, "/res/Particle/Pscene%03d.jpc", scene);
    return readDisc(path, out);
}

// Sets particle `id`'s BSP1 prm colour in a raw file, in place. 1 when set.
int setPrm(uint8_t* data, size_t size, uint16_t id, PcRandoJpcRgba c) {
    std::vector<PcRandoJpcParticle> ps;
    if (pc_rando_jpc_scan(data, size, &ps, nullptr) != PCR_JPC_OK) {
        return 0;
    }
    for (const PcRandoJpcParticle& p : ps) {
        if (p.id == id) {
            return pc_rando_jpc_set_color(data, p, PCR_JPC_BSP1_PRM, c) == PCR_JPC_OK;
        }
    }
    return 0;
}

bool getPrm(const uint8_t* data, size_t size, uint16_t id, PcRandoJpcRgba* c) {
    std::vector<PcRandoJpcParticle> ps;
    if (pc_rando_jpc_scan(data, size, &ps, nullptr) != PCR_JPC_OK) {
        return false;
    }
    for (const PcRandoJpcParticle& p : ps) {
        if (p.id == id) {
            return pc_rando_jpc_get_color(data, p, PCR_JPC_BSP1_PRM, c) == PCR_JPC_OK;
        }
    }
    return false;
}

bool sameColor(PcRandoJpcRgba a, PcRandoJpcRgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int sSceneEdits = 0;
int sCommonEdits = 0;

void sceneEditor(int scene, uint8_t* data, size_t size, void*) {
    if (scene == kScene) {
        sSceneEdits += setPrm(data, size, kIdFixed, kSceneColor);
    }
}

void commonEditor(int scene, uint8_t* data, size_t size, void*) {
    if (scene == PCR_JPC_SCENE_COMMON) {
        sCommonEdits += setPrm(data, size, kCommonId, kCommonColor);
    }
}

void registerAll() {
    if (sRegistered || !pc_rando()) {
        return;
    }
    const bool ok = pc_rando_jpc_scene_add_import(kScene, kIdFixed, kSource) &&
                    pc_rando_jpc_scene_add_import(kScene, kIdAny, PCR_JPC_SCENE_ANY) &&
                    pc_rando_jpc_scene_add_editor(kScene, sceneEditor, nullptr) &&
                    pc_rando_jpc_scene_add_editor(PCR_JPC_SCENE_COMMON, commonEditor, nullptr);
    sRegistered = true;
    randoSmokeCheck(kName, "imports and editors registered", ok);
}

// The first Pscene000-254 that holds `id`, by its own scan of the disc.
int firstHolding(uint16_t id) {
    for (int i = 0; i < 255; i++) {
        cos_sdk::HostVector<uint8_t> raw;
        if (readScene(i, &raw) && pc_rando_jpc_has_particle(raw.data(), raw.size(), id)) {
            return i;
        }
    }
    return -1;
}

void dump(const uint8_t* data, size_t size) {
    const char* dir = getenv("COS_RUN_DIR");
    char path[512];
    snprintf(path, sizeof path, "%s/jpcscene_Pscene%03d.jpc", dir != nullptr ? dir : ".", kScene);
    FILE* f = fopen(path, "wb");
    if (f == nullptr) {
        return;
    }
    fwrite(data, 1, size, f);
    fclose(f);
    logf("[cos] rando-smoke %s: merged bytes dumped to %s\n", kName, path);
}

void checkAll() {
    JKRPcHostAllocScope scope;
    dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle();
    const PcRandoParticleSceneStats* st = pc_rando_particles_last_scene();
    logf("[cos] rando-smoke %s: pc_rando %d, scene %d, stats scene %d serial %u imported %d skipped %d failed %d "
         "edited %d merged %d size %u\n",
         kName, pc_rando(), pa->mSceneNo, st->scene, st->serial, st->imported, st->skipped, st->failed, st->edited,
         st->merged, st->size);
    randoSmokeCheck(kName, "the boot stage's particle bank is Pscene050", pa->mSceneNo == kScene && pa->mSceneResMng != nullptr);
    if (pa->mSceneResMng == nullptr) {
        return;
    }
    cos_sdk::HostVector<uint8_t> disc, src;
    if (!randoSmokeCheck(kName, "Pscene050 and Pscene035 read from the disc", readScene(kScene, &disc) && readScene(kSource, &src))) {
        return;
    }
    randoSmokeCheck(kName, "the disc's Pscene050 lacks 0x8161 and 0x8162",
                    !pc_rando_jpc_has_particle(disc.data(), disc.size(), kIdFixed) &&
                        !pc_rando_jpc_has_particle(disc.data(), disc.size(), kIdAny));
    randoSmokeCheck(kName, "the loaded file (mpData) equals the disc file",
                    pa->mpData != nullptr && memcmp(pa->mpData, disc.data(), disc.size()) == 0);
    JPAEmitterResource* emtr = pa->mSceneResMng->pEmtrRes;
    const bool hasFixed = emtr->checkUserIndexDuplication(kIdFixed);
    const bool hasAny = emtr->checkUserIndexDuplication(kIdAny);
    uint32_t commonSize = 0;
    int commonPasses = 0;
    const uint8_t* common = pc_rando_particles_common_data(&commonSize, &commonPasses);
    PcRandoJpcRgba commonPrm = {};
    const bool commonRead = common != nullptr && getPrm(common, commonSize, kCommonId, &commonPrm);
    logf("[cos] rando-smoke %s: common.jpc %u bytes, %d editor passes, 0x48 prm %02X%02X%02X%02X\n", kName,
         commonSize, commonPasses, commonPrm.r, commonPrm.g, commonPrm.b, commonPrm.a);

    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: the hook never ran", st->serial == 0 && st->scene == -1);
        randoSmokeCheck(kName, "vanilla: the scene manager lacks 0x8161 and 0x8162", !hasFixed && !hasAny);
        cos_sdk::HostVector<uint8_t> commonDisc;
        PcRandoJpcRgba discPrm = {};
        randoSmokeCheck(kName, "vanilla: common.jpc's 0x48 keeps the disc colour",
                        commonRead && readDisc("/res/Particle/common.jpc", &commonDisc) &&
                            getPrm(commonDisc.data(), commonDisc.size(), kCommonId, &discPrm) &&
                            sameColor(commonPrm, discPrm) && commonPasses == 0);
        logf("[cos] rando-smoke %s: Pscene050 fnv %08X (disc %08X)\n", kName,
             fnv((const uint8_t*)pa->mpData, disc.size()), fnv(disc.data(), disc.size()));
        return;
    }

    randoSmokeCheck(kName, "the hook built Pscene050 with 2 imports on the scene heap",
                    st->scene == kScene && st->imported == 2 && st->failed == 0 && st->merged == 1 && st->edited == 1 &&
                        st->loaded == pa->mpData && st->data != pa->mpData);
    const int anySrc = firstHolding(kIdAny);
    const int hookAny = pc_rando_particles_find_any(kIdAny);
    logf("[cos] rando-smoke %s: 0x8162 first held by Pscene%03d (hook: %03d)\n", kName, anySrc, hookAny);
    randoSmokeCheck(kName, "\"any\" finds the first file holding the id", anySrc >= 0 && hookAny == anySrc);
    // The expected bytes: the library's own merge of the disc files.
    PcRandoJpcFile dst, fixedSrc, anyFile;
    cos_sdk::HostVector<uint8_t> anyRaw;
    std::vector<uint8_t> want;
    const bool built = pc_rando_jpc_load(&dst, disc.data(), disc.size()) == PCR_JPC_OK &&
                       pc_rando_jpc_load(&fixedSrc, src.data(), src.size()) == PCR_JPC_OK &&
                       anySrc >= 0 && readScene(anySrc, &anyRaw) &&
                       pc_rando_jpc_load(&anyFile, anyRaw.data(), anyRaw.size()) == PCR_JPC_OK &&
                       pc_rando_jpc_copy_particle(&dst, fixedSrc, kIdFixed) == PCR_JPC_OK &&
                       pc_rando_jpc_copy_particle(&dst, anyFile, kIdAny) == PCR_JPC_OK &&
                       pc_rando_jpc_save(dst, &want) == PCR_JPC_OK &&
                       setPrm(want.data(), want.size(), kIdFixed, kSceneColor);
    const bool same = built && st->data != nullptr && st->size == want.size() && memcmp(st->data, want.data(), want.size()) == 0;
    logf("[cos] rando-smoke %s: merged fnv %08X, expected fnv %08X (%u bytes)\n", kName,
         st->data != nullptr ? fnv((const uint8_t*)st->data, st->size) : 0u,
         built ? fnv(want.data(), want.size()) : 0u, (unsigned)want.size());
    randoSmokeCheck(kName, "the merged copy equals load + copy 0x8161 (035) + copy 0x8162 (any) + save + colour edit", same);
    if (st->data != nullptr) {
        dump((const uint8_t*)st->data, st->size);
    }
    PcRandoJpcRgba prm = {};
    randoSmokeCheck(kName, "the scene editor ran on the merged copy (0x8161 prm colour)",
                    sSceneEdits == 1 && st->data != nullptr && getPrm((const uint8_t*)st->data, st->size, kIdFixed, &prm) &&
                        sameColor(prm, kSceneColor));
    randoSmokeCheck(kName, "the scene manager has 0x8161 and 0x8162", hasFixed && hasAny);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz pos = player != nullptr ? player->current.pos : cXyz(0.0f, 0.0f, 0.0f);
    JPABaseEmitter* e1 = dComIfGp_particle_set(kIdFixed, &pos);
    JPABaseEmitter* e2 = dComIfGp_particle_set(kIdAny, &pos);
    randoSmokeCheck(kName, "dComIfGp_particle_set(0x8161) and (0x8162) return emitters", e1 != nullptr && e2 != nullptr);
    if (e1 != nullptr) {
        e1->becomeInvalidEmitter();
    }
    if (e2 != nullptr) {
        e2->becomeInvalidEmitter();
    }
    randoSmokeCheck(kName, "the common.jpc editor set 0x48's prm colour",
                    commonRead && sameColor(commonPrm, kCommonColor) && sCommonEdits == 1 && commonPasses >= 1);
}

// Under pc_rando(), after the checks: the stage is entered again (removeRoomScene destroys the scene
// heap holding the merged copy and frees the loaded file; the next createRoomScene merges again).
void checkReload() {
    dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle();
    const PcRandoParticleSceneStats* st = pc_rando_particles_last_scene();
    randoSmokeCheck(kName, "re-entered: Pscene050 merged again on the new scene heap",
                    st->serial == 2 && st->scene == kScene && st->merged == 1 && st->imported == 2 && pa->mSceneResMng != nullptr &&
                        st->loaded == pa->mpData);
    if (pa->mSceneResMng == nullptr) {
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz pos = player != nullptr ? player->current.pos : cXyz(0.0f, 0.0f, 0.0f);
    JPABaseEmitter* e = dComIfGp_particle_set(kIdFixed, &pos);
    randoSmokeCheck(kName, "re-entered: dComIfGp_particle_set(0x8161) returns an emitter", e != nullptr);
    if (e != nullptr) {
        e->becomeInvalidEmitter();
    }
}

enum Phase { kFirst, kReload, kFinished };
Phase sPhase = kFirst;
unsigned sReloadFrames = 0;

} // namespace

void randoSmokeJpcscene(unsigned int frames) {
    registerAll();
    if (sPhase == kFirst) {
        if (!randoSmokeInPlay(10)) {
            return;
        }
        checkAll();
        if (!pc_rando()) {
            sPhase = kFinished;
            randoSmokeEnd(kName);
        }
        dComIfGp_setNextStage(dComIfGp_getStartStageName(), dComIfGp_getStartStagePoint(),
                              (s8)dComIfGp_getStartStageRoomNo(), dComIfGp_getStartStageLayer());
        sPhase = kReload;
        return;
    }
    if (sPhase == kReload) {
        // Wait for the new scene's particles and 30 frames with the player in it.
        const PcRandoParticleSceneStats* st = pc_rando_particles_last_scene();
        if (st->serial < 2 || dComIfGp_getPlayer(0) == nullptr) {
            sReloadFrames = 0;
            if (frames > 3000) {
                randoSmokeCheck(kName, "re-entered the stage", false);
                randoSmokeEnd(kName);
            }
            return;
        }
        if (++sReloadFrames < 30) {
            return;
        }
        checkReload();
        sPhase = kFinished;
        randoSmokeEnd(kName);
    }
}

} // namespace pc
