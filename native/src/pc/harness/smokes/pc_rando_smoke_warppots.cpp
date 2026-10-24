// COS_SMOKE=rando COS_RANDO_SMOKE=warppots (docs/RANDOMIZER_PLAN.md step R7.1c): the inter-dungeon warp
// pots (features/rando/pc_rando_tweak_warp_pots.h, the d_a_obj_warpt.cpp createInit hunk), run as
//   run.sh rando --stage M_NewD2:2:1 --env COS_RANDO_SMOKE=warppots
//                [--env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R7_1c_warppots.json]
// (the lines are in native/check/regress_rando/R7.1c.txt).
//
// 1. Static (always), once the play scene has run 10 frames: the 6 pots' Room<N>.arc room.dzr files
//    and their stages' Stage.arc stage.dzs are read from the disc into a private test heap (as
//    smokes/pc_rando_smoke_entr.cpp does).
//    - The pure edit (pc_rando_warp_pots_apply) on each disc file: in each room.dzr the SCLS chunk
//      gains exactly 3 entries at its end (destination stage, spawn 69, room, fade 4 for the 3 pots of
//      the cycle, in order), the ACTR chunk one Warpts<k+1> with type k+2, the cycle's event register
//      and the 3 new SCLS indexes, x/z_rot 0xFFFF; the spawn 69 (type 7, the pot's position) is added
//      to the room (DRC, FW, TotG, FF) or to stage.dzs (ET, WT) and is the file's only spawn 69; every
//      vanilla entry of every chunk keeps its bytes; other stage.dzs files are not touched.
//    - The patcher itself (pc_rando_tweak_warp_pots_patch) on a copy of each file: with the option on
//      (pc_rando() and add_shortcut_warps_between_dungeons) it gives the same bytes as the pure edit;
//      with the option off (no placement, or R0_6_identity.json) the bytes keep the disc's hash.
// 2. Live, with the option on: from DRC (the boot room) the smoke takes, in each pot's room, the pot's
//    next exit (the SCLS daObj_Warpt_c::spWarp tries first: dLib_setNextStageBySclsNum) and checks it
//    lands on the next dungeon's spawn 69: DRC -> FW -> TotG -> DRC, then a stage change to FF spawn
//    69, FF -> ET -> WT -> FF (6 hops through pots). In every room: the patcher patched the files
//    (status), the live room's SCLS count is the disc's + 3 with the 3 cycle exits, a live
//    daObj_Warpt_c has the expected params (event register 2 or 5), its smoke colours m86C/m870 and
//    its live emitter's base colours are the custom 0xE5101B80 / 0x3C379D80, the live spawn 69
//    (room or stage data) is the expected record, and the scene's particle manager has 0x8161,
//    0x8162, 0x8165, 0x8166 and 0x8112 (imported into Pscene050 and Pscene043 from Pscene035).
//    Without the option: the live boot room holds no Warpts actor, no spawn 69 and the disc's SCLS
//    count, and no particle import was registered.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_warp_pots.h"
#include "pc/pc_rando.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "d/actor/d_a_obj_warpt.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_particle.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "warppots";
constexpr unsigned kSettle = 60;       // frames in a new room before its checks
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

uint32_t fnv(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

bool optionOn() {
    return pc_rando() && pc_rando_cfg()->inter_dungeon_warps != 0;
}

// --- the disc files ---

struct DiscFile {
    uint8_t* data = nullptr; // malloc
    size_t size = 0;
};
DiscFile sRoom[PCR_WARP_POT_COUNT];
DiscFile sStage[PCR_WARP_POT_COUNT];
uint32_t sVanillaScls[PCR_WARP_POT_COUNT];
uint32_t sStaticRoom[PCR_WARP_POT_COUNT];  // the smoke's own patcher runs (static phase)
uint32_t sStaticStage[PCR_WARP_POT_COUNT];
JKRExpHeap* sHeap = nullptr;

bool readDisc(const char* stage, int room, DiscFile* out) {
    char path[128];
    if (room < 0) {
        snprintf(path, sizeof path, "/res/Stage/%s/Stage.arc", stage);
    } else {
        snprintf(path, sizeof path, "/res/Stage/%s/Room%d.arc", stage, room);
    }
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
    void* res = arc->getResource(0, room < 0 ? "stage.dzs" : "room.dzr");
    bool ok = false;
    if (res != nullptr) {
        out->size = arc->getResSize(res);
        out->data = (uint8_t*)malloc(out->size);
        memcpy(out->data, res, out->size);
        ok = true;
    }
    arc->unmountFixed();
    sHeap->freeAll();
    return ok;
}

void* mallocAlloc(size_t size, void*) {
    return calloc(1, size);
}

// The real patcher on a malloc copy of a disc file. Returns the resulting bytes (malloc; the input copy
// when nothing replaced it) in *out / *outSize.
void runPatcher(const char* stage, int room, const DiscFile& f, uint8_t** out, size_t* outSize) {
    uint8_t* copy = (uint8_t*)malloc(f.size);
    memcpy(copy, f.data, f.size);
    PcRandoResFile file = {};
    PcRandoResNames names = {};
    char dir[PCR_RES_DIR_MAX];
    snprintf(dir, sizeof dir, "/res/Stage/%s/", stage);
    char arcName[16];
    if (room < 0) {
        snprintf(arcName, sizeof arcName, "Stage");
    } else {
        snprintf(arcName, sizeof arcName, "Room%d", room);
    }
    pc_rando_res_describe(&file, &names, arcName, dir);
    file.name = room < 0 ? "stage.dzs" : "room.dzr";
    file.type = room < 0 ? PCR_RES_DZS : PCR_RES_DZR;
    file.data = copy;
    file.size = f.size;
    file.alloc = mallocAlloc;
    pc_rando_tweak_warp_pots_patch(&file);
    if (file.data != copy) {
        free(copy);
    }
    *out = file.data;
    *outSize = file.size;
}

// Every entry of every chunk of `a` has the same bytes in `b` (b grown from a).
bool vanillaKept(const uint8_t* a, size_t aSize, uint8_t* b, size_t bSize) {
    PcRandoDzx da, db;
    if (pc_rando_dzx_open(&da, const_cast<uint8_t*>(a), aSize) != PCR_DZX_OK ||
        pc_rando_dzx_open(&db, b, bSize) != PCR_DZX_OK) {
        return false;
    }
    for (uint32_t c = 0; c < da.chunkCount; c++) {
        char tag[5] = {};
        memcpy(tag, da.data + 4 + c * 0xC, 4);
        const uint32_t esize = pc_rando_dzx_entry_size(tag);
        if (esize == 0) {
            continue; // RTBL and other variable chunks: the grow keeps their bytes
        }
        const uint32_t n = pc_rando_dzx_count(&da, tag);
        if (pc_rando_dzx_count(&db, tag) < n) {
            return false;
        }
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t* ea = pc_rando_dzx_entry(&da, tag, i);
            const uint8_t* eb = pc_rando_dzx_entry(&db, tag, i);
            if (ea == nullptr || eb == nullptr || memcmp(ea, eb, esize) != 0) {
                return false;
            }
        }
    }
    return true;
}

int countSpawn69(const PcRandoDzx* dzx) {
    int n = 0;
    const uint32_t c = pc_rando_dzx_count(dzx, "PLYR");
    for (uint32_t i = 0; i < c; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, "PLYR", i);
        n += e != nullptr && e[0x1D] == PCR_WARP_POT_SPAWN_ID ? 1 : 0;
    }
    return n;
}

const uint8_t* findSpawn69(const PcRandoDzx* dzx) {
    const uint32_t c = pc_rando_dzx_count(dzx, "PLYR");
    for (uint32_t i = 0; i < c; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, "PLYR", i);
        if (e != nullptr && e[0x1D] == PCR_WARP_POT_SPAWN_ID) {
            return e;
        }
    }
    return nullptr;
}

void checkStaticPot(int i) {
    const PcRandoWarpPot* pot = pc_rando_warp_pot(i);
    const DiscFile& room = sRoom[i];
    const DiscFile& stage = sStage[i];
    PcRandoDzx dr, ds;
    if (!check("%s/Room%d: disc room.dzr and stage.dzs parse", pc_rando_dzx_open(&dr, room.data, room.size) == PCR_DZX_OK &&
                                                                  pc_rando_dzx_open(&ds, stage.data, stage.size) == PCR_DZX_OK,
               pot->stage, pot->room)) {
        return;
    }
    sVanillaScls[i] = pc_rando_dzx_count(&dr, "SCLS");
    const uint32_t first = sVanillaScls[i];
    logf("[cos] rando-smoke %s: %s/Room%d: disc SCLS %u, ACTR %u, PLYR %u (stage PLYR %u), spawns 69: room %d stage %d\n",
         kName, pot->stage, pot->room, (unsigned)first, (unsigned)pc_rando_dzx_count(&dr, "ACTR"),
         (unsigned)pc_rando_dzx_count(&dr, "PLYR"), (unsigned)pc_rando_dzx_count(&ds, "PLYR"), countSpawn69(&dr),
         countSpawn69(&ds));
    check("%s/Room%d: the disc files have no spawn 69", countSpawn69(&dr) == 0 && countSpawn69(&ds) == 0, pot->stage,
          pot->room);

    uint8_t plyr[0x20], scls[3][0xC], actr[0x20];
    pc_rando_warp_pot_entries(i, first, plyr, scls, actr);

    // room.dzr
    const size_t cap = pc_rando_warp_pots_grown_size(pot->stage, pot->room, room.data, room.size);
    uint8_t* grown = cap > 0 ? (uint8_t*)malloc(cap) : nullptr;
    size_t gsize = 0;
    const int r = grown != nullptr ? pc_rando_warp_pots_apply(pot->stage, pot->room, room.data, room.size, grown, cap, &gsize)
                                   : PCR_WARP_POTS_FAILED;
    PcRandoDzx dg;
    if (check("%s/Room%d: room.dzr patched (%d)", r == PCR_WARP_POTS_PATCHED && pc_rando_dzx_open(&dg, grown, gsize) == PCR_DZX_OK,
              pot->stage, pot->room, r)) {
        const uint32_t n = pc_rando_dzx_count(&dg, "SCLS");
        bool exits = n == first + 3;
        for (int d = 0; exits && d < 3; d++) {
            const uint8_t* e = pc_rando_dzx_entry(&dg, "SCLS", first + d);
            const PcRandoWarpPot* to = pc_rando_warp_pot(pc_rando_warp_pot_dest(i, d));
            char name[8] = {};
            strncpy(name, to->stage, 8);
            exits = e != nullptr && memcmp(e, name, 8) == 0 && e[8] == 69 && e[9] == (uint8_t)to->room && e[10] == 4 &&
                    e[11] == 0xFF && memcmp(e, scls[d], 0xC) == 0;
        }
        check("%s/Room%d: SCLS %u -> %u, the new 3 lead to the cycle's spawns 69 with fade 4", exits, pot->stage, pot->room,
              (unsigned)first, (unsigned)n);
        const uint32_t na = pc_rando_dzx_count(&dg, "ACTR");
        const uint8_t* a = na > 0 ? pc_rando_dzx_entry(&dg, "ACTR", na - 1) : nullptr;
        char want[8];
        pc_rando_warp_pot_name(i, want);
        const uint32_t params = a != nullptr ? pc_rando_dzx_be32(a + 8) : 0;
        const uint32_t wantParams = (uint32_t)(pot->index + 2) | (uint32_t)pot->eventReg << 4 | first << 8 |
                                    (first + 1) << 16 | (first + 2) << 24;
        check("%s/Room%d: ACTR %u -> %u, the last is %.8s params %08X (want %08X), x/z_rot FFFF",
              na == pc_rando_dzx_count(&dr, "ACTR") + 1 && a != nullptr && memcmp(a, want, 8) == 0 && params == wantParams &&
                  pc_rando_dzx_be16(a + 0x18) == 0xFFFF && pc_rando_dzx_be16(a + 0x1C) == 0xFFFF &&
                  pc_rando_dzx_be16(a + 0x1A) == pot->yRot && memcmp(a, actr, 0x20) == 0,
              pot->stage, pot->room, (unsigned)pc_rando_dzx_count(&dr, "ACTR"), (unsigned)na, a != nullptr ? (const char*)a : "-",
              (unsigned)params, (unsigned)wantParams);
        if (!pot->spawnInStage) {
            const uint8_t* s = findSpawn69(&dg);
            check("%s/Room%d: room.dzr gained the only spawn 69 (type 7, room %d, the pot's position)",
                  countSpawn69(&dg) == 1 && s != nullptr && memcmp(s, plyr, 0x20) == 0 &&
                      pc_rando_dzx_count(&dg, "PLYR") == pc_rando_dzx_count(&dr, "PLYR") + 1 &&
                      ((pc_rando_dzx_be32(s + 8) >> 12) & 0xF) == 7 && (pc_rando_dzx_be32(s + 8) & 0x3F) == (uint32_t)pot->room &&
                      pc_rando_dzx_bef32(s + 0x0C) == pot->x && pc_rando_dzx_bef32(s + 0x10) == pot->y &&
                      pc_rando_dzx_bef32(s + 0x14) == pot->z,
                  pot->stage, pot->room, pot->room);
        } else {
            check("%s/Room%d: room.dzr gained no PLYR (the spawn goes in stage.dzs)",
                  pc_rando_dzx_count(&dg, "PLYR") == pc_rando_dzx_count(&dr, "PLYR") && countSpawn69(&dg) == 0, pot->stage,
                  pot->room);
        }
        check("%s/Room%d: every vanilla entry keeps its bytes", vanillaKept(room.data, room.size, grown, gsize), pot->stage,
              pot->room);
        // The patcher with the option on gives the same bytes; off, the disc's.
        uint8_t* out = nullptr;
        size_t outSize = 0;
        runPatcher(pot->stage, pot->room, room, &out, &outSize);
        if (optionOn()) {
            check("%s/Room%d: the patcher (option on) = the pure edit", outSize == gsize && memcmp(out, grown, gsize) == 0,
                  pot->stage, pot->room);
        } else {
            check("%s/Room%d: the patcher (option off) keeps the disc hash %08X (is %08X)",
                  outSize == room.size && fnv(out, outSize) == fnv(room.data, room.size), pot->stage, pot->room,
                  (unsigned)fnv(room.data, room.size), (unsigned)fnv(out, outSize));
        }
        free(out);
    }
    free(grown);

    // stage.dzs
    const size_t scap = pc_rando_warp_pots_grown_size(pot->stage, -1, stage.data, stage.size);
    uint8_t* sg = scap > 0 ? (uint8_t*)malloc(scap) : nullptr;
    size_t sgsize = 0;
    const int sr = pc_rando_warp_pots_apply(pot->stage, -1, stage.data, stage.size, sg, scap, &sgsize);
    uint8_t* out = nullptr;
    size_t outSize = 0;
    runPatcher(pot->stage, -1, stage, &out, &outSize);
    if (pot->spawnInStage) {
        PcRandoDzx dsg;
        if (check("%s/Stage: stage.dzs patched (%d)", sr == PCR_WARP_POTS_PATCHED && pc_rando_dzx_open(&dsg, sg, sgsize) == PCR_DZX_OK,
                  pot->stage, sr)) {
            const uint8_t* s = findSpawn69(&dsg);
            check("%s/Stage: stage.dzs gained the only spawn 69 (type 7, room %d, the pot's position)",
                  countSpawn69(&dsg) == 1 && s != nullptr && memcmp(s, plyr, 0x20) == 0 &&
                      pc_rando_dzx_count(&dsg, "PLYR") == pc_rando_dzx_count(&ds, "PLYR") + 1,
                  pot->stage, pot->room);
            check("%s/Stage: every vanilla entry keeps its bytes", vanillaKept(stage.data, stage.size, sg, sgsize), pot->stage);
            if (optionOn()) {
                check("%s/Stage: the patcher (option on) = the pure edit", outSize == sgsize && memcmp(out, sg, sgsize) == 0,
                      pot->stage);
            }
        }
    } else {
        check("%s/Stage: stage.dzs has nothing to add", sr == PCR_WARP_POTS_NONE, pot->stage);
    }
    if (!optionOn() || !pot->spawnInStage) {
        check("%s/Stage: the patcher keeps the disc hash %08X (is %08X)",
              outSize == stage.size && fnv(out, outSize) == fnv(stage.data, stage.size), pot->stage,
              (unsigned)fnv(stage.data, stage.size), (unsigned)fnv(out, outSize));
    }
    free(out);
    free(sg);
}

bool runStatic() {
    for (u32 size = 16u << 20; sHeap == nullptr && size >= (2u << 20); size >>= 1) {
        sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
    }
    if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr)) {
        return false;
    }
    bool read = true;
    for (int i = 0; i < PCR_WARP_POT_COUNT; i++) {
        const PcRandoWarpPot* pot = pc_rando_warp_pot(i);
        read = readDisc(pot->stage, pot->room, &sRoom[i]) && readDisc(pot->stage, -1, &sStage[i]) && read;
    }
    sHeap->destroy();
    sHeap = nullptr;
    if (!randoSmokeCheck(kName, "the 6 rooms' room.dzr and their stage.dzs read from the disc", read)) {
        return false;
    }
    PcRandoWarpPotStatus before[PCR_WARP_POT_COUNT];
    for (int i = 0; i < PCR_WARP_POT_COUNT; i++) {
        pc_rando_warp_pots_status(i, &before[i]);
    }
    for (int i = 0; i < PCR_WARP_POT_COUNT; i++) {
        checkStaticPot(i);
    }
    // The static runs of the patcher count in its status: keep them apart from the game's loads.
    for (int i = 0; i < PCR_WARP_POT_COUNT; i++) {
        PcRandoWarpPotStatus after;
        pc_rando_warp_pots_status(i, &after);
        sStaticRoom[i] = after.roomPatched - before[i].roomPatched;
        sStaticStage[i] = after.stagePatched - before[i].stagePatched;
    }
    return true;
}

// --- live ---

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* p = fpcM_Search(isPlayScene, nullptr);
    return p != nullptr ? fpcM_GetID(p) : fpcM_ERROR_PROCESS_ID_e;
}

struct PlayLayerScope {
    layer_class* saved;
    PlayLayerScope() : saved(fpcLy_CurrentLayer()) {
        if (void* play = fpcM_Search(isPlayScene, nullptr)) {
            fpcLy_SetCurrentLayer(&static_cast<process_node_class*>(play)->mLayer);
        }
    }
    ~PlayLayerScope() { fpcLy_SetCurrentLayer(saved); }
};

struct WarptFind {
    int room;
    int n;
    daObj_Warpt_c* pot;
};

void findWarpts(WarptFind* f) {
    f->n = 0;
    f->pot = nullptr;
    fopAcM_Search(
        [](void* proc, void* data) -> void* {
            WarptFind* c = static_cast<WarptFind*>(data);
            if (fpcM_GetName(proc) == fpcNm_OBJ_WARPT_e) {
                daObj_Warpt_c* w = static_cast<daObj_Warpt_c*>(proc);
                if (fopAcM_GetRoomNo(w) == c->room && w->isSp()) {
                    c->n++;
                    c->pot = w;
                }
            }
            return nullptr;
        },
        f);
}

bool rgbIs(GXColor c, uint32_t rgba) {
    return c.r == (uint8_t)(rgba >> 24) && c.g == (uint8_t)(rgba >> 16) && c.b == (uint8_t)(rgba >> 8);
}

const uint8_t* liveSpawn69(int room, bool inStage) {
    stage_actor_class* pl = inStage ? dComIfGp_getStage().getPlayer()
                                    : (dComIfGp_roomControl_getStatusRoomDt(room) != nullptr
                                           ? dComIfGp_roomControl_getStatusRoomDt(room)->getPlayer()
                                           : nullptr);
    if (pl == nullptr) {
        return nullptr;
    }
    const uint8_t* found = nullptr;
    int n = 0;
    for (int i = 0; i < (int)pl->num; i++) {
        const uint8_t* e = reinterpret_cast<const uint8_t*>(&pl->m_entries[i]);
        if (e[0x1D] == PCR_WARP_POT_SPAWN_ID) {
            found = e;
            n++;
        }
    }
    return n == 1 ? found : nullptr;
}

stage_scls_info_dummy_class* liveScls(int room) {
    dStage_roomDt_c* rd = dComIfGp_roomControl_getStatusRoomDt(room);
    return rd != nullptr ? rd->getSclsInfo() : nullptr;
}

// The checks of pot i's room, the player just arrived (viaPot: on spawn 69).
void checkRoom(int i, bool viaPot) {
    const PcRandoWarpPot* pot = pc_rando_warp_pot(i);
    const char* stage = dComIfGp_getStartStageName();
    const int room = dComIfGp_roomControl_getStayNo();
    const int point = dComIfGp_getStartStagePoint();
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    logf("[cos] rando-smoke %s: frame %u: on %s room %d spawn %d, player at (%.1f, %.1f, %.1f)\n", kName,
         pc_frame_count(), stage, room, point, player != nullptr ? player->current.pos.x : 0.0f,
         player != nullptr ? player->current.pos.y : 0.0f, player != nullptr ? player->current.pos.z : 0.0f);
    if (!check("pot %d: the play scene runs on %s room %d (is %s room %d)", strcmp(stage, pot->stage) == 0 && room == pot->room,
               i, pot->stage, pot->room, stage, room)) {
        return;
    }
    if (viaPot) {
        const float dx = player != nullptr ? player->current.pos.x - pot->x : 1e9f;
        const float dz = player != nullptr ? player->current.pos.z - pot->z : 1e9f;
        check("pot %d: landed on spawn 69 at the pot (spawn %d, xz distance %.1f)",
              point == PCR_WARP_POT_SPAWN_ID && std::sqrt(dx * dx + dz * dz) < 200.0f, i, point, std::sqrt(dx * dx + dz * dz));
    }
    PcRandoWarpPotStatus st;
    pc_rando_warp_pots_status(i, &st);
    const uint32_t gameRoom = st.roomPatched - sStaticRoom[i];
    const uint32_t gameStage = st.stagePatched - sStaticStage[i];
    check("pot %d: the game's loads were patched: room.dzr%s (game patches: room %u, stage %u)",
          gameRoom > 0 && st.lastRoom == PCR_WARP_POTS_PATCHED && st.firstScls == sVanillaScls[i] &&
              (!pot->spawnInStage || (gameStage > 0 && st.lastStage == PCR_WARP_POTS_PATCHED)),
          i, pot->spawnInStage ? " and stage.dzs" : "", (unsigned)gameRoom, (unsigned)gameStage);

    stage_scls_info_dummy_class* scls = liveScls(room);
    const uint32_t first = sVanillaScls[i];
    bool exits = scls != nullptr && (uint32_t)scls->num == first + 3;
    for (int d = 0; exits && d < 3; d++) {
        const stage_scls_info_class& e = scls->m_entries[first + d];
        const PcRandoWarpPot* to = pc_rando_warp_pot(pc_rando_warp_pot_dest(i, d));
        exits = strncmp(e.mStage, to->stage, 8) == 0 && e.mStart == 69 && e.mRoom == (u8)to->room && e.mWipe == 4;
    }
    check("pot %d: live SCLS count %d = disc %u + 3, the last 3 lead to the cycle", exits, i,
          scls != nullptr ? (int)scls->num : -1, (unsigned)first);

    WarptFind f = {room, 0, nullptr};
    findWarpts(&f);
    daObj_Warpt_c* w = f.pot;
    const uint32_t want = pc_rando_warp_pot_params(i, first);
    check("pot %d: one live cyclic warp pot with params %08X (is %08X), event register %d (is %d)",
          f.n == 1 && w != nullptr && fopAcM_GetParam(w) == want && w->m2B8 == pot->eventReg && w->m2B4 == pot->index + 2, i,
          (unsigned)want, w != nullptr ? (unsigned)fopAcM_GetParam(w) : 0u, pot->eventReg, w != nullptr ? (int)w->m2B8 : -1);
    if (w != nullptr) {
        check("pot %d: smoke colours m86C %02X%02X%02X%02X m870 %02X%02X%02X%02X are the custom E5101B80 / 3C379D80",
              rgbIs(w->m86C, PCR_WARP_POT_PRM_COLOR) && w->m86C.a == 0x80 && rgbIs(w->m870, PCR_WARP_POT_ENV_COLOR) &&
                  w->m870.a == 0x80,
              i, w->m86C.r, w->m86C.g, w->m86C.b, w->m86C.a, w->m870.r, w->m870.g, w->m870.b, w->m870.a);
        JPABaseEmitter* e = w->m858.getEmitter() != nullptr ? w->m858.getEmitter() : w->m844.getEmitter();
        GXColor prm = {}, env = {};
        if (e != nullptr) {
            e->getBasePrmColor(prm);
            e->getBaseEnvColor(env);
        }
        check("pot %d: the live %s emitter's colours %02X%02X%02X / %02X%02X%02X are the custom ones", e != nullptr &&
                  rgbIs(prm, PCR_WARP_POT_PRM_COLOR) && rgbIs(env, PCR_WARP_POT_ENV_COLOR),
              i, w->m858.getEmitter() != nullptr ? "close (0x8162)" : "open (0x8161)", prm.r, prm.g, prm.b, env.r, env.g, env.b);
    }

    uint8_t plyr[0x20];
    pc_rando_warp_pot_entries(i, first, plyr, nullptr, nullptr);
    const uint8_t* s = liveSpawn69(room, pot->spawnInStage != 0);
    check("pot %d: the live %s data hold the only spawn 69, the expected record", s != nullptr && memcmp(s, plyr, 0x20) == 0, i,
          pot->spawnInStage ? "stage" : "room");

    dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle();
    bool ids = pa != nullptr && pa->mSceneResMng != nullptr;
    for (int k = 0; ids && k < PCR_WARP_POT_PARTICLE_COUNT; k++) {
        ids = pa->mSceneResMng->pEmtrRes->checkUserIndexDuplication(pc_rando_warp_pot_particle(k));
    }
    check("pot %d: the scene's particle bank (Pscene%03d) has 0x8161 0x8162 0x8165 0x8166 0x8112", ids, i,
          pa != nullptr ? (int)pa->mSceneNo : -1);
}

// Without the option, in the boot room (DRC).
void checkVanillaRoom() {
    const int room = dComIfGp_roomControl_getStayNo();
    const int i = pc_rando_warp_pot_find(dComIfGp_getStartStageName(), room);
    if (!check("vanilla: the boot room is a warp pot room (%s room %d)", i >= 0, dComIfGp_getStartStageName(), room)) {
        return;
    }
    WarptFind f = {room, 0, nullptr};
    findWarpts(&f);
    stage_scls_info_dummy_class* scls = liveScls(room);
    const int n = scls != nullptr ? (int)scls->num : 0;
    check("vanilla: no cyclic warp pot, no spawn 69, SCLS count = the disc's %u (is %d)",
          f.n == 0 && liveSpawn69(room, false) == nullptr && liveSpawn69(room, true) == nullptr && (uint32_t)n == sVanillaScls[i],
          (unsigned)sVanillaScls[i], n);
    PcRandoWarpPotStatus st;
    pc_rando_warp_pots_status(i, &st);
    check("vanilla: the patcher patched nothing (room %u, stage %u) and registered no particle import",
          st.roomPatched == 0 && st.stagePatched == 0 && pc_rando_warp_pots_imports_registered() == 0,
          (unsigned)st.roomPatched, (unsigned)st.stagePatched);
}

// The tour: arrivals in pots 0 1 2 0 | 3 4 5 3. After the 4th arrival (back in DRC) the smoke goes to
// FF spawn 69 by a direct stage change.
const int kTour[] = {0, 1, 2, 0, 3, 4, 5, 3};
constexpr int kTourLen = (int)(sizeof kTour / sizeof kTour[0]);

enum State { kBoot, kArrive, kDone };
State sState = kBoot;
int sStep = 0;
unsigned sSince = 0;
unsigned sPresent = 0;
fpc_ProcID sOldScene = fpcM_ERROR_PROCESS_ID_e;
int sHops = 0;

void leave(int i) {
    sOldScene = playSceneId();
    sSince = 0;
    sPresent = 0;
    sState = kArrive;
    if (sStep == 4) {
        const PcRandoWarpPot* to = pc_rando_warp_pot(kTour[sStep]);
        logf("[cos] rando-smoke %s: stage change to %s room %d spawn 69\n", kName, to->stage, to->room);
        dComIfGp_setNextStage(to->stage, PCR_WARP_POT_SPAWN_ID, (s8)to->room, -1);
        return;
    }
    const int d = pc_rando_warp_pot_next_dest(i);
    const int idx = (int)sVanillaScls[i] + d;
    const PcRandoWarpPot* to = pc_rando_warp_pot(pc_rando_warp_pot_dest(i, d));
    {
        PlayLayerScope layer;
        dLib_setNextStageBySclsNum((u8)idx, (s8)pc_rando_warp_pot(i)->room);
    }
    sHops++;
    check("hop %d: pot %d's exit SCLS[%d] requests %s room %d spawn 69 (is %s room %d spawn %d)",
          dComIfGp_isEnableNextStage() && strcmp(dComIfGp_getNextStageName(), to->stage) == 0 &&
              dComIfGp_getNextStageRoomNo() == to->room && dComIfGp_getNextStagePoint() == PCR_WARP_POT_SPAWN_ID,
          sHops, i, idx, to->stage, to->room, dComIfGp_getNextStageName(), dComIfGp_getNextStageRoomNo(),
          dComIfGp_getNextStagePoint());
}

} // namespace

void randoSmokeWarppots(unsigned int frames) {
    (void)frames;
    if (sState == kDone) {
        return;
    }
    if (sState == kBoot) {
        if (!randoSmokeInPlay(10)) {
            return;
        }
        logf("[cos] rando-smoke %s: pc_rando %d, add_shortcut_warps_between_dungeons %d\n", kName, pc_rando(),
             pc_rando() ? (int)pc_rando_cfg()->inter_dungeon_warps : -1);
        const bool ok = runStatic();
        if (!ok || !optionOn()) {
            if (ok) {
                checkVanillaRoom();
            }
            sState = kDone;
            randoSmokeEnd(kName);
        }
        check("the boot room is DRC's pot room (M_NewD2 room 2)",
              strcmp(dComIfGp_getStartStageName(), "M_NewD2") == 0 && dComIfGp_roomControl_getStayNo() == 2);
        checkRoom(0, false);
        sStep = 1;
        leave(0);
        return;
    }
    // kArrive
    sSince++;
    const fpc_ProcID scene = playSceneId();
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (scene == fpcM_ERROR_PROCESS_ID_e || scene == sOldScene || player == nullptr || dComIfGp_isEnableNextStage()) {
        sPresent = 0;
        if (sSince >= kArriveFrames) {
            const PcRandoWarpPot* to = pc_rando_warp_pot(kTour[sStep]);
            check("step %d: %s room %d reached (still on %s room %d)", false, sStep, to->stage, to->room,
                  dComIfGp_getStartStageName(), dComIfGp_roomControl_getStayNo());
            sState = kDone;
            randoSmokeEnd(kName);
        }
        return;
    }
    if (++sPresent < kSettle) {
        return;
    }
    const int i = kTour[sStep];
    checkRoom(i, true);
    if (sStep + 1 >= kTourLen) {
        check("6 hops through pots made", sHops == 6);
        sState = kDone;
        randoSmokeEnd(kName);
    }
    sStep++;
    leave(i);
}

} // namespace pc
