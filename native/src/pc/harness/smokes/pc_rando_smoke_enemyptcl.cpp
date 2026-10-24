// COS_SMOKE=rando COS_RANDO_SMOKE=enemyptcl (docs/RANDOMIZER_PLAN.md step R7.4c): the enemy randomizer's
// particles (features/rando/pc_rando_enemy_particles.h), run as
//   run.sh rando --stage <stage:room:point[:layer]> --env COS_RANDO_SMOKE=enemyptcl
//                [--env COS_RANDO_PLACEMENT=<placement with "enemies">] [--env COS_ENEMY_PTCL_DUMP=<file>]
//                [--env COS_RANDO_PTCL_HOPS=<stage:room:point:layer>[,...]]
// (the lines are in native/check/regress_rando/R7.4c.txt; native/tools/rando/enemy_particle_check.py runs it).
//
// 1. Static, once the play scene has run 60 frames: the bank file of every place of the enemy table
//    (stage.dzs of the 51 stage folders, room.dzr of the 35 sea rooms; enemy_stages() order, which is
//    done_enemy_locations' order) is read from the disc into a private test heap and given to
//    pc_rando_enemy_particles_for_file with the active seed. The result is wwrando's
//    particles_to_load_for_each_jpc_index (banks in first-seen order, ids in order); for each id the
//    disc's bank lacks, the "any" source (pc_rando_particles_find_any) must exist. Lines
//        bank <NNN> <id> <id> ...           (every bank with ids, as wwrando's dict)
//        need <NNN> <id> <source NNN>        (ids the disc's bank lacks: what update_loaded_particles copies)
//    go to COS_ENEMY_PTCL_DUMP when set, for enemy_particle_check.py (wwrando's own code with gclib).
//    Without a seed holding "enemies" no file yields ids.
// 2. Live, in the boot room and in each COS_RANDO_PTCL_HOPS stop: the place's required ids
//    (pc_rando_enemy_particles_required; on the sea the stay room's): the scene's bank is the one the
//    patcher read; every id is in the scene resource manager and dPa_control_c::set makes an emitter
//    for it (destroyed at once); the bank hook's last scene build failed nothing and imported every
//    id the disc's bank lacked (with COS_ENEMY_PTCL_NEED set, there must be such an id). Without
//    enemies: the patcher registered nothing.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_enemy_data.h"
#include "features/rando/pc_rando_enemy_particles.h"
#include "features/rando/pc_rando_gen_enemies.h"
#include "features/rando/pc_rando_jpc.h"
#include "features/rando/pc_rando_seedfile.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "d/d_com_inf_game.h"
#include "d/d_particle.h"
#include "d/d_stage.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include "cos_sdk/host_alloc.h"

#include <dolphin/dvd.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "enemyptcl";
constexpr unsigned kSettle = 60;
constexpr unsigned kArriveFrames = 3000;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

bool check(const char* fmt, bool ok, ...) __attribute__((format(printf, 1, 3)));
bool check(const char* fmt, bool ok, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, ok);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return randoSmokeCheck(kName, buf, ok);
}

JKRExpHeap* sHeap = nullptr;
pcr::EnemyRandoResult sSeed;
bool sHasSeed = false;

// The banks of wwrando's dict, first-seen order.
struct Bank {
    int bank;
    std::vector<uint16_t> ids;
    std::vector<uint16_t> lacking; // ids the disc's bank file lacks
};
std::vector<Bank> sBanks;

// A file of a stage archive ("Stage" or "Room<N>") read from the disc through a private mount.
bool readArcFile(const char* stage, const char* arcName, const char* file, std::vector<uint8_t>* out) {
    char path[128];
    snprintf(path, sizeof path, "/res/Stage/%s/%s.arc", stage, arcName);
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        logf("[cos] rando-smoke %s: %s: cannot mount a private copy\n", kName, path);
        sHeap->freeAll();
        return false;
    }
    for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst(); l != nullptr; l = l->getNext()) {
        if (l->getObject() == arc) {
            JKRFileLoader::getVolumeList().remove(l);
            break;
        }
    }
    void* res = arc->getResource(0, file);
    bool ok = false;
    if (res != nullptr) {
        const uint32_t size = arc->getResSize(res);
        out->assign((const uint8_t*)res, (const uint8_t*)res + size);
        ok = true;
    }
    arc->unmountFixed();
    sHeap->freeAll();
    return ok;
}

bool readScene(int scene, cos_sdk::HostVector<uint8_t>* out) {
    char path[48];
    snprintf(path, sizeof path, "/res/Particle/Pscene%03d.jpc", scene);
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        return false;
    }
    out->resize(info.length);
    const s32 got = DVDReadPrio(&info, out->data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    return got == (s32)info.length;
}

Bank* bankOf(int bank) {
    for (Bank& b : sBanks) {
        if (b.bank == bank) {
            return &b;
        }
    }
    return nullptr;
}

bool contains(const std::vector<uint16_t>& v, uint16_t id) {
    for (uint16_t x : v) {
        if (x == id) {
            return true;
        }
    }
    return false;
}

void loadSeed() {
    sHasSeed = false;
    sSeed = pcr::EnemyRandoResult();
    const PcRandoSeed* s = pc_rando() ? pc_rando_seed() : nullptr;
    const pcr::Json* j = s != nullptr ? s->extra.find("enemies") : nullptr;
    std::string err;
    sHasSeed = j != nullptr && pcr::enemiesFromJson(*j, sSeed, &err);
}

bool runStatic() {
    for (u32 size = 16u << 20; sHeap == nullptr && size >= (4u << 20); size >>= 1) {
        sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
    }
    if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr)) {
        return false;
    }
    const pcr::Span<pcr::EnemyStage> stages = pcr::enemy_stages();
    int places = 0, readOk = 0, withIds = 0, parseFail = 0;
    for (size_t i = 0; i < stages.size; i++) {
        const char* key = stages[i].key;
        char stage[16] = {};
        int room = -1;
        const char* arcName = "Stage";
        const char* file = "stage.dzs";
        char roomArc[16] = {};
        if (sscanf(key, "sea/Room%d.arc", &room) == 1) {
            snprintf(stage, sizeof stage, "sea");
            snprintf(roomArc, sizeof roomArc, "Room%d", room);
            arcName = roomArc;
            file = "room.dzr";
        } else {
            snprintf(stage, sizeof stage, "%s", key);
        }
        places++;
        std::vector<uint8_t> bytes;
        if (!readArcFile(stage, arcName, file, &bytes)) {
            logf("[cos] rando-smoke %s: %s: %s/%s.arc %s not read\n", kName, key, stage, arcName, file);
            continue;
        }
        readOk++;
        if (!sHasSeed) {
            continue;
        }
        int bank = -1;
        std::vector<uint16_t> ids;
        const int r = pc_rando_enemy_particles_for_file(sSeed, stage, room, file, bytes.data(), bytes.size(), &bank, &ids);
        if (r < 0) {
            parseFail++;
            logf("[cos] rando-smoke %s: %s: %s does not parse or has no bank entry\n", kName, key, file);
            continue;
        }
        if (r == 0) {
            continue;
        }
        withIds++;
        Bank* b = bankOf(bank);
        if (b == nullptr) {
            sBanks.push_back(Bank{bank, {}, {}});
            b = &sBanks.back();
        }
        for (uint16_t id : ids) {
            if (!contains(b->ids, id)) {
                b->ids.push_back(id);
            }
        }
    }
    check("%d of %d places' bank files read from the disc", readOk == places, readOk, places);
    check("every bank file parses (%d failed)", parseFail == 0, parseFail);

    // What update_loaded_particles copies: the ids each disc bank lacks, and their "any" source.
    FILE* dump = nullptr;
    if (const char* path = getenv("COS_ENEMY_PTCL_DUMP")) {
        dump = fopen(path, "w");
        check("dump file %s opened", dump != nullptr, path);
    }
    int needs = 0, sourced = 0;
    for (Bank& b : sBanks) {
        if (dump != nullptr) {
            fprintf(dump, "bank %03d", b.bank);
            for (uint16_t id : b.ids) {
                fprintf(dump, " %04X", id);
            }
            fprintf(dump, "\n");
        }
        JKRPcHostAllocScope scope;
        cos_sdk::HostVector<uint8_t> raw;
        if (!readScene(b.bank, &raw)) {
            check("Pscene%03d.jpc read from the disc", false, b.bank);
            continue;
        }
        for (uint16_t id : b.ids) {
            if (pc_rando_jpc_has_particle(raw.data(), raw.size(), id)) {
                continue;
            }
            b.lacking.push_back(id);
            needs++;
            const int src = pc_rando_particles_find_any(id);
            sourced += src >= 0 ? 1 : 0;
            if (dump != nullptr) {
                fprintf(dump, "need %03d %04X %03d\n", b.bank, id, src);
            }
        }
    }
    if (dump != nullptr) {
        fclose(dump);
    }
    logf("[cos] rando-smoke %s: seed enemies %d: %d places with required particles, %zu banks, %d imports needed\n",
         kName, sHasSeed ? 1 : 0, withIds, sBanks.size(), needs);
    check("every needed particle has an \"any\" source (%d of %d)", sourced == needs, sourced, needs);
    if (!sHasSeed) {
        check("no seed enemies: no bank file yields ids", withIds == 0 && sBanks.empty());
    }
    return true;
}

void checkStop(int stop) {
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    const bool sea = strcmp(stage, "sea") == 0;
    dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle();
    PcRandoEnemyParticleStats st;
    pc_rando_enemy_particles_stats(&st);
    const PcRandoParticleSceneStats* sc = pc_rando_particles_last_scene();
    logf("[cos] rando-smoke %s: stop %d %s room %d: scene Pscene%03d; patcher %u files %u requested %u registered "
         "%u failed; last scene build Pscene%03d imported %d skipped %d failed %d merged %d\n",
         kName, stop, stage, stay, pa->mSceneNo, st.files, st.requested, st.registered, st.failed, sc->scene,
         sc->imported, sc->skipped, sc->failed, sc->merged);
    if (!sHasSeed) {
        check("stop %d: no seed enemies: the patcher registered nothing", st.files == 0 && st.registered == 0, stop);
        return;
    }
    std::vector<uint16_t> ids(64);
    const size_t n = pc_rando_enemy_particles_required(stage, sea ? stay : -1, ids.data(), ids.size());
    ids.resize(n < ids.size() ? n : ids.size());
    const int bank = pc_rando_enemy_particles_bank_of(stage, sea ? stay : -1);
    if (n == 0) {
        logf("[cos] rando-smoke %s: stop %d: %s room %d has no placed enemy with particles\n", kName, stop, stage, stay);
        return;
    }
    check("stop %d (%s room %d): the scene's bank Pscene%03d is the one the patcher read (%d)",
          pa->mSceneResMng != nullptr && pa->mSceneNo == bank, stop, stage, stay, pa->mSceneNo, bank);
    if (pa->mSceneResMng == nullptr) {
        return;
    }
    // The place's ids the disc's bank lacks (the bank's list also holds the other places sharing it).
    const Bank* b = bankOf(bank);
    size_t lacking = 0;
    for (uint16_t id : ids) {
        lacking += b != nullptr && contains(b->lacking, id) ? 1 : 0;
    }
    int inBank = 0, made = 0;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz pos = player != nullptr ? player->current.pos : cXyz(0.0f, 0.0f, 0.0f);
    std::string list;
    for (uint16_t id : ids) {
        char t[8];
        snprintf(t, sizeof t, " %04X", id);
        list += t;
        const bool has = pa->mSceneResMng->pEmtrRes->checkUserIndexDuplication(id);
        inBank += has ? 1 : 0;
        const u8 group = (id & 0x4000) ? dPa_control_c::dPtclGroup_Projection_e
                         : (id & 0x2000) ? dPa_control_c::dPtclGroup_Toon_e
                                         : dPa_control_c::dPtclGroup_Normal_e;
        JPABaseEmitter* e = has ? pa->set(group, id, &pos, nullptr, nullptr, 0xFF, nullptr, -1, nullptr, nullptr, nullptr) : nullptr;
        made += e != nullptr ? 1 : 0;
        if (e != nullptr) {
            e->becomeInvalidEmitter();
        }
        if (!has || e == nullptr) {
            logf("[cos] rando-smoke %s:   0x%04X: in bank %d, emitter %d\n", kName, id, has ? 1 : 0, e != nullptr ? 1 : 0);
        }
    }
    logf("[cos] rando-smoke %s: stop %d: required%s (%zu the disc's Pscene%03d lacks)\n", kName, stop, list.c_str(),
         lacking, bank);
    check("stop %d (%s room %d): %d of %zu required particles in the scene resource manager", inBank == (int)ids.size(),
          stop, stage, stay, inBank, ids.size());
    check("stop %d (%s room %d): %d of %zu emitters created", made == (int)ids.size(), stop, stage, stay, made,
          ids.size());
    if (getenv("COS_ENEMY_PTCL_NEED") != nullptr) {
        check("stop %d (%s room %d): the disc's bank lacks some required particle (%zu)", lacking > 0, stop, stage,
              stay, lacking);
    }
    check("stop %d (%s room %d): the bank build failed nothing and imported at least the %zu lacking ids (%d)",
          sc->scene == bank && sc->failed == 0 && sc->imported >= (int)lacking, stop, stage, stay, lacking, sc->imported);
}

struct Hop {
    char stage[9];
    int room, point, layer;
};
std::vector<Hop> sHops;
size_t sHop = 0;

void parseHops() {
    const char* s = getenv("COS_RANDO_PTCL_HOPS");
    if (s == nullptr) {
        return;
    }
    std::string all(s);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t end = all.find(',', pos);
        if (end == std::string::npos) {
            end = all.size();
        }
        std::string item = all.substr(pos, end - pos);
        Hop h = {};
        h.layer = -1;
        char st[16] = {};
        if (sscanf(item.c_str(), "%8[^:]:%d:%d:%d", st, &h.room, &h.point, &h.layer) >= 3) {
            memcpy(h.stage, st, 8);
            sHops.push_back(h);
        } else {
            logf("[cos] rando-smoke %s: bad hop \"%s\" (stage:room:point[:layer])\n", kName, item.c_str());
        }
        pos = end + 1;
    }
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(isPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

enum State { kBoot, kArrive, kDone };
State sState = kBoot;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
unsigned sSince = 0, sPresent = 0;

void nextHop() {
    if (sHop >= sHops.size()) {
        sState = kDone;
        randoSmokeEnd(kName);
    }
    const Hop& h = sHops[sHop];
    logf("[cos] rando-smoke %s: stage change to %s room %d point %d layer %d\n", kName, h.stage, h.room, h.point,
         h.layer);
    sOldScene = playSceneId();
    sSince = 0;
    sPresent = 0;
    sState = kArrive;
    dComIfGp_setNextStage(h.stage, (s16)h.point, (s8)h.room, (s8)h.layer);
}

} // namespace

void randoSmokeEnemyptcl(unsigned int frames) {
    (void)frames;
    if (sState == kDone) {
        return;
    }
    // The smoke's containers and the patcher module's lasting state (pc_rando_enemy_particles_required
    // parses the seed) go to host memory, never to the game's current heap.
    JKRPcHostAllocScope scope;
    if (sState == kBoot) {
        if (!randoSmokeInPlay(kSettle)) {
            return;
        }
        loadSeed();
        logf("[cos] rando-smoke %s: pc_rando %d, seed enemies %d (%zu placements)\n", kName, pc_rando(),
             sHasSeed ? 1 : 0, sSeed.placements.size());
        if (!runStatic()) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        checkStop(0);
        parseHops();
        nextHop();
        return;
    }
    sSince++;
    const fpc_ProcID scene = playSceneId();
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr || dComIfGp_isEnableNextStage()) {
        sPresent = 0;
        if (sSince >= kArriveFrames) {
            check("hop %zu: %s room %d reached", false, sHop, sHops[sHop].stage, sHops[sHop].room);
            sState = kDone;
            randoSmokeEnd(kName);
        }
        return;
    }
    if (++sPresent < kSettle) {
        return;
    }
    check("hop %zu: arrived in %s room %d (is %s room %d)",
          strcmp(dComIfGp_getStartStageName(), sHops[sHop].stage) == 0 &&
              dComIfGp_roomControl_getStayNo() == sHops[sHop].room,
          sHop, sHops[sHop].stage, sHops[sHop].room, dComIfGp_getStartStageName(), dComIfGp_roomControl_getStayNo());
    checkStop((int)sHop + 1);
    sHop++;
    nextHop();
}

} // namespace pc
