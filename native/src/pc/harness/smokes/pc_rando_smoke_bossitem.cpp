// COS_SMOKE=rando COS_RANDO_SMOKE=bossitem (docs/RANDOMIZER_PLAN.md step R2.5): the 6 boss Heart
// Container locations (features/rando/pc_rando_boss_items.h, game_hooks/pc_rando_hooks_boss.cpp and
// the pc_rando(R2.5) hunks in fopAcM_createItemForBoss, daBossItem_Create and item_func_utuwa_heart),
// in a boss room, e.g.
//   run.sh rando --stage M_DragB:0 --env COS_RANDO_SMOKE=bossitem   (Gohma, stage 3)
//   run.sh rando --stage kazeB:0 --env COS_RANDO_SMOKE=bossitem     (Molgera, stage 7)
// Once the play scene has run 10 frames, three rounds run: vanilla (pc_rando() off), rando (a
// placement with Piece of Heart 0x07 on Helmaroc King's location, Telescope 0x20 on the other five,
// then pc_rando_set_active with no seed), and vanilla again after pc_rando_deactivate. Under
// COS_RANDO_FORCE=1 only the rando round runs. Each round spawns, one at a time in front of the player,
// and finds the field item (daItem) that comes out:
//   1. a Bitem (daBossItem) for the loaded stage S, with S's boss defeated and its item not taken;
//   2. a Bitem for stage 2 (Forsaken Fortress, as the two sea-stage Bitems are), FF's boss defeated;
//   3. fopAcM_createItemForBoss(..., 0, ..., 1), as Gohdan and Molgera call it;
//   4. a disappear cloud with the Heart Container drop, as Gohma, Kalle Demos, Helmaroc King and
//      Jalhalla make it;
//   5. (rando only) a Bitem for S with item flag 0x15 already set: no item may come out.
// Vanilla expects item 0x08 with item flag 0xFF (none) every time; rando expects S's placed item for
// 1, 3 and 4 and FF's for 2, with item flag 0x15; actions are BOSS (1-3) and BOSS_DISAPPEAR (4). The
// vanilla rounds also check that spawning changed no save or memory bit (the boss-defeat flags and the
// boss-item flag stay as set up; the visited-room bits are left out). item_func_utuwa_heart is called
// once per round: vanilla sets S's boss-item flag (dComIfGs_isStageLife), rando does not. Every save
// bit is restored after each round.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_boss_items.h"
#include "d/actor/d_a_item.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "bossitem";
constexpr unsigned int kWaitFrames = 30;       // Bitem create + item create
constexpr unsigned int kCloudWaitFrames = 120; // + the cloud's 58-frame timer
constexpr u8 kFfItem = 0x07;             // Piece of Heart on Helmaroc King's location
constexpr u8 kOtherItem = 0x20;          // Telescope on the other five

enum Round { VANILLA, RANDO, VANILLA_AFTER, ROUND_COUNT };
enum Job { BITEM_HERE, BITEM_FF, DIRECT, CLOUD, TAKEN, JOB_COUNT };
const char* const kRoundName[ROUND_COUNT] = {"vanilla", "rando", "vanilla after"};
const char* const kJobName[JOB_COUNT] = {"Bitem (loaded stage)", "Bitem (stage 2, FF)",
                                         "createItemForBoss direct", "disappear cloud",
                                         "Bitem with item flag 0x15 taken"};

uint8_t sPlacement[PCR_LOC_COUNT];
int sRound = -1;
int sJob = 0;
unsigned int sJobStart = 0;
bool sSpawned = false;
cXyz sPos;
int sStage = -1;
dSv_memBit_c sSavedMem;
dSv_memBit_c sSavedSave[8];
dSv_memBit_c sSetupMem;
dSv_memBit_c sSetupSave[8];
u16 sSavedLife = 0;
u16 sSavedMaxLife = 0;

bool check(const char* what, bool ok) {
    char buf[200];
    snprintf(buf, sizeof buf, "%s: %s", kRoundName[sRound], what);
    return randoSmokeCheck(kName, buf, ok);
}

dSv_memBit_c& saved(int stage) {
    return g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
}
dSv_memBit_c& memory() {
    return g_dComIfG_gameInfo.save.getMemory().getBit();
}
int loadedStage() {
    return dStage_stagInfo_GetSaveTbl(dComIfGp_getStageStagInfo());
}
// Every field but mVisitedRoom, which the game sets on its own once the room has been shown (and
// endRound's restore clears again).
bool sameBits(const dSv_memBit_c& a, const dSv_memBit_c& b) {
    dSv_memBit_c x = a;
    std::memcpy((void*)&x.mVisitedRoom, (const void*)&b.mVisitedRoom, sizeof x.mVisitedRoom);
    return memcmp(&x, &b, sizeof x) == 0;
}
void logDiff(const char* what, const dSv_memBit_c& a, const dSv_memBit_c& b) {
    const u8* pa = (const u8*)&a;
    const u8* pb = (const u8*)&b;
    for (size_t i = 0; i < sizeof a; i++) {
        if (pa[i] != pb[i] && (i < 0x18 || i >= 0x20)) {
            writef(STDERR_FILENO, "[cos] rando-smoke %s: %s byte 0x%02zX: 0x%02X -> 0x%02X\n", kName, what, i,
                   pb[i], pa[i]);
        }
    }
}
bool bitsAsSetUp() {
    bool ok = sameBits(memory(), sSetupMem);
    if (!ok) {
        logDiff("memory", memory(), sSetupMem);
    }
    for (int i = 0; i < 8; i++) {
        if (!sameBits(saved(i), sSetupSave[i])) {
            char what[32];
            snprintf(what, sizeof what, "saved stage %d", i);
            logDiff(what, saved(i), sSetupSave[i]);
            ok = false;
        }
    }
    return ok;
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}
void* nearItem(void* proc, void*) {
    if (fpcM_GetName(proc) != fpcNm_ITEM_e) {
        return nullptr;
    }
    fopAc_ac_c* ac = (fopAc_ac_c*)proc;
    const f32 dx = ac->current.pos.x - sPos.x;
    const f32 dz = ac->current.pos.z - sPos.z;
    return dx * dx + dz * dz < 100.0f * 100.0f ? proc : nullptr;
}

bool on() {
    return sRound == RANDO;
}
u8 expectItem(int job) {
    if (!on()) {
        return PC_RANDO_BOSS_VANILLA_ITEM;
    }
    return job == BITEM_FF || sStage == 2 ? kFfItem : kOtherItem;
}

// Boss of S and of FF defeated, their boss items not taken, item flag 0x15 clear.
void setUpBits() {
    dComIfGs_onStageBossEnemy(sStage);
    dComIfGs_offStageLife(sStage);
    dComIfGs_onStageBossEnemy(2);
    dComIfGs_offStageLife(2);
    sSetupMem = memory();
    for (int i = 0; i < 8; i++) {
        sSetupSave[i] = saved(i);
    }
}

void startRound() {
    sSavedMem = memory();
    for (int i = 0; i < 8; i++) {
        sSavedSave[i] = saved(i);
    }
    sSavedLife = dComIfGs_getLife();
    sSavedMaxLife = dComIfGs_getMaxLife();
    if (sRound == RANDO) {
        if (!pc_rando_forced()) {
            pc_rando_set_loc_items(sPlacement, PCR_LOC_COUNT);
            pc_rando_set_active(nullptr, nullptr);
        } else {
            pc_rando_set_loc_items(sPlacement, PCR_LOC_COUNT);
        }
    } else if (sRound == VANILLA_AFTER) {
        pc_rando_deactivate();
    }
    check("pc_rando() as expected", pc_rando() == (on() ? 1 : 0));
    char what[120];
    snprintf(what, sizeof what, "pc_rando_boss_item(0) in stage %d is 0x%02X", sStage, expectItem(BITEM_HERE));
    if (on()) {
        check(what, pc_rando_boss_item(0) == expectItem(BITEM_HERE));
        check("pc_rando_boss_item(2) is FF's item", pc_rando_boss_item(2) == kFfItem);
    }

    // item_func_utuwa_heart: the boss-item flag.
    dComIfGs_offStageLife(sStage);
    item_func_utuwa_heart();
    dComIfGp_clearItemMaxLifeCount();
    dComIfGp_clearItemLifeCount();
    check(on() ? "item_func_utuwa_heart leaves the boss-item flag clear"
               : "item_func_utuwa_heart sets the boss-item flag",
          dComIfGs_isStageLife(sStage) == (on() ? FALSE : TRUE));
    memory() = sSavedMem;
    for (int i = 0; i < 8; i++) {
        saved(i) = sSavedSave[i];
    }

    setUpBits();
    sJob = BITEM_HERE;
    sSpawned = false;
}

void endRound() {
    memory() = sSavedMem;
    for (int i = 0; i < 8; i++) {
        saved(i) = sSavedSave[i];
    }
    dComIfGs_setLife(sSavedLife);
    dComIfGs_setMaxLife((u8)sSavedMaxLife);
}

void spawn(unsigned int frames) {
    daPy_lk_c* p = daPy_getPlayerLinkActorClass();
    const s16 yaw = p->shape_angle.y;
    const f32 dist = 150.0f + 120.0f * (f32)sJob;
    sPos.set(p->current.pos.x + dist * cM_ssin(yaw), p->current.pos.y + 30.0f,
             p->current.pos.z + dist * cM_scos(yaw));
    const int room = fopAcM_GetRoomNo(p);
    csXyz angle = csXyz::Zero;
    layer_class* savedLayer = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    switch (sJob) {
    case BITEM_HERE:
        fopAcM_create(fpcNm_BOSSITEM_e, (u32)sStage, &sPos, room, &angle);
        break;
    case BITEM_FF:
        fopAcM_create(fpcNm_BOSSITEM_e, 2u, &sPos, room, &angle);
        break;
    case DIRECT:
        fopAcM_createItemForBoss(&sPos, 0, room, &angle, nullptr, 1);
        break;
    case CLOUD:
        fopAcM_createDisappear(p, &sPos, 15, daDisItem_HEART_CONTAINER_e);
        break;
    case TAKEN:
        dComIfGs_onItem(0x15, room);
        sSetupMem = memory();
        fopAcM_create(fpcNm_BOSSITEM_e, (u32)sStage, &sPos, room, &angle);
        break;
    }
    fpcLy_SetCurrentLayer(savedLayer);
    writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u: %s: %s spawned\n", kName, frames, kRoundName[sRound],
           kJobName[sJob]);
}

// True when the job is done.
bool poll(unsigned int frames) {
    daItem_c* item = (daItem_c*)fpcM_Search(nearItem, nullptr);
    // The cloud drops its item after its 58-frame timer; a Bitem's item comes out within a few frames.
    const unsigned int wait = sJob == CLOUD ? kCloudWaitFrames : kWaitFrames;
    if (item == nullptr && frames - sJobStart < wait) {
        return false;
    }
    char what[200];
    if (sJob == TAKEN) {
        snprintf(what, sizeof what, "%s: no item", kJobName[sJob]);
        check(what, item == nullptr);
    } else if (item == nullptr) {
        snprintf(what, sizeof what, "%s: an item came out", kJobName[sJob]);
        check(what, false);
    } else {
        const u32 prm = fopAcM_GetParam(item);
        const u32 bit = (prm >> 8) & 0xFF;
        const u32 action = (prm >> 26) & 0x3F;
        const u32 wantAction = sJob == CLOUD ? daItemAct_BOSS_DISAPPEAR_e : daItemAct_BOSS_e;
        writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u: %s: item 0x%02X, flag 0x%02X, action %u\n", kName,
               frames, kJobName[sJob], (unsigned)item->getItemNo(), (unsigned)bit, (unsigned)action);
        snprintf(what, sizeof what, "%s: item 0x%02X", kJobName[sJob], expectItem(sJob));
        check(what, item->getItemNo() == expectItem(sJob));
        snprintf(what, sizeof what, "%s: item flag 0x%02X", kJobName[sJob], on() ? 0x15 : 0xFF);
        check(what, bit == (on() ? 0x15u : 0xFFu));
        snprintf(what, sizeof what, "%s: action %u", kJobName[sJob], (unsigned)wantAction);
        check(what, action == wantAction);
        fopAcM_delete(item);
    }
    if (!on()) {
        snprintf(what, sizeof what, "%s: save and memory bits unchanged", kJobName[sJob]);
        check(what, bitsAsSetUp());
    }
    return true;
}

} // namespace

void randoSmokeBossitem(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sRound < 0) {
        sStage = loadedStage();
        writef(STDERR_FILENO, "[cos] rando-smoke %s: loaded stage %d (boss location %d)\n", kName, sStage,
               pc_rando_boss_loc(sStage));
        sRound = VANILLA;
        if (!randoSmokeCheck(kName, "a boss stage is loaded", pc_rando_boss_loc(sStage) >= 0)) {
            randoSmokeEnd(kName);
        }
        memset(sPlacement, 0x01, sizeof sPlacement);
        for (int s = 2; s <= 7; s++) {
            sPlacement[pc_rando_boss_loc(s)] = s == 2 ? kFfItem : kOtherItem;
        }
        if (pc_rando_forced()) {
            sRound = RANDO;
        }
        startRound();
    }
    if (!sSpawned) {
        spawn(frames);
        sSpawned = true;
        sJobStart = frames;
        return;
    }
    if (!poll(frames)) {
        return;
    }
    sSpawned = false;
    sJob++;
    if (sJob == TAKEN && !on()) {
        sJob++;
    }
    if (sJob < JOB_COUNT) {
        return;
    }
    endRound();
    sRound++;
    if (sRound >= ROUND_COUNT || pc_rando_forced()) {
        randoSmokeEnd(kName);
    }
    startRound();
}

} // namespace pc
