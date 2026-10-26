// COS_SMOKE=rando COS_RANDO_SMOKE=bossloc (docs/RANDOMIZER_PLAN.md step R9.1): the location sweep's
// boss Heart Container locations under the seed the run has loaded, in a boss room, e.g.
//   run.sh rando --stage M_DragB:0 --env COS_RANDO_SMOKE=bossloc \
//                --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_rotation.json [--env COS_BOSSLOC_T2=1]
// native/tools/rando/location_sweep.py runs it for the "boss" kind and parses its per-location lines
//   [cos] rando-smoke bossloc: T1|T2 <PCR_LOC_*> <what>: ...: ok|FAIL
// The bossitem smoke (R2.5) checks the same paths with a synthetic placement and the vanilla arms;
// this one uses the loaded seed (pc_rando_loc_item, or the vanilla Heart Container 0x08 with no
// placement). Once the play scene has run 10 frames, with the loaded stage S's boss defeated and its
// item not taken (and the same for stage 2, Forsaken Fortress), one at a time in front of the player:
//   T1 1. a Bitem (daBossItem) for S;
//      2. a Bitem for stage 2 (Helmaroc King's location, as the two sea-stage Bitems are);
//      3. fopAcM_createItemForBoss(..., 0, ..., 1), as Gohdan and Molgera call it;
//      4. a disappear cloud with the Heart Container drop, as Gohma, Kalle Demos, Helmaroc King and
//         Jalhalla make it;
//   each must give a field item of the placed item (or its progressive tier), with item flag 0x15
//   under the randomizer (0xFF without);
//   T2 (COS_BOSSLOC_T2=1): a last Bitem for S; its field item is moved onto the player, the item-get
//   event must start with dComIfGp_event_getGtItm() the field item's id, A goes through it, and the
//   save must have changed when it ends (the item's item_func ran; the clock is left out). An item the
//   game gives without that event (rupees, hearts, ammo, a spoil or bait already seen) must be picked
//   up with the save changed instead.
// The save bits the setup touches are restored at the end.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_boss_items.h"
#include "features/rando/pc_rando_lockeys.h"

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

#include <dolphin/pad.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "bossloc";
constexpr unsigned int kWaitFrames = 30;       // Bitem create + item create
constexpr unsigned int kCloudWaitFrames = 120; // + the cloud's 58-frame timer
constexpr unsigned int kStartFrames = 300;
constexpr unsigned int kEventFrames = 3000;

enum Job { BITEM_HERE, BITEM_FF, DIRECT, CLOUD, T2_BITEM, JOB_COUNT };
const char* const kJobName[JOB_COUNT] = {"Bitem (loaded stage)", "Bitem (stage 2)", "createItemForBoss direct",
                                         "disappear cloud", "Bitem (loaded stage), picked up"};

enum State { kInit, kSpawn, kPoll, kT2Start, kT2Event, kDone };
State sState = kInit;
int sJob = BITEM_HERE;
unsigned int sSince = 0;
int sStage = -1;
cXyz sPos;
bool sT2 = false;
fpc_ProcID sItemId = fpcM_ERROR_PROCESS_ID_e;
int sItemNo = -1;
dSv_memBit_c sSavedMem;
dSv_memBit_c sSavedSave[8];

const char* locName(int loc) {
    const pcr::LocInfo* info = pcr::location_info(loc);
    return info != nullptr ? info->enum_name : "?";
}
int jobLoc() {
    return pc_rando_boss_loc(sJob == BITEM_FF ? 2 : sStage);
}
u8 want() {
    return pc_rando() ? pc_rando_loc_item(jobLoc(), 0xFE) : (u8)PC_RANDO_BOSS_VANILLA_ITEM;
}
bool sameItem(int got, u8 w) {
    return got == w || (pc_rando() && got == pc_rando_convert_progressive_item_id(w));
}

dSv_memBit_c& saved(int stage) {
    return g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
}
dSv_memBit_c& memory() {
    return g_dComIfG_gameInfo.save.getMemory().getBit();
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
void* judgeById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}

// daItem_c::itemGetExecute gives these field items without a get-item event (execItemGet at once):
// hearts, rupees, magic, bombs and arrows, and a spoil or bait whose first get the save already holds
// (R9.1: SweepSeedTwo puts a Knight's Crest on Kalle Demos, and the debug save has seen every spoil).
bool directGet(int itemNo) {
    switch (itemNo) {
    case dItemNo_HEART_e:
    case dItemNo_GREEN_RUPEE_e:
    case dItemNo_BLUE_RUPEE_e:
    case dItemNo_YELLOW_RUPEE_e:
    case dItemNo_RED_RUPEE_e:
    case dItemNo_PURPLE_RUPEE_e:
    case dItemNo_ORANGE_RUPEE_e:
    case dItemNo_SILVER_RUPEE_e:
    case dItemNo_SMALL_MAGIC_e:
    case dItemNo_LARGE_MAGIC_e:
    case dItemNo_BOMB_5_e:
    case dItemNo_BOMB_10_e:
    case dItemNo_BOMB_20_e:
    case dItemNo_BOMB_30_e:
    case dItemNo_ARROW_10_e:
    case dItemNo_ARROW_20_e:
    case dItemNo_ARROW_30_e:
    case dItemNo_TRIPLE_HEART_e:
        return true;
    case dItemNo_SKULL_NECKLACE_e: return dComIfGs_isGetItemBeast(dBeastIdx_SKULL_NECKLACE_e);
    case dItemNo_BOKOBABA_SEED_e: return dComIfGs_isGetItemBeast(dBeastIdx_BOKOBABA_SEED_e);
    case dItemNo_GOLDEN_FEATHER_e: return dComIfGs_isGetItemBeast(dBeastIdx_GOLDEN_FEATHER_e);
    case dItemNo_KNIGHTS_CREST_e: return dComIfGs_isGetItemBeast(dBeastIdx_KNIGHTS_CREST_e);
    case dItemNo_RED_JELLY_e: return dComIfGs_isGetItemBeast(dBeastIdx_RED_JELLY_e);
    case dItemNo_GREEN_JELLY_e: return dComIfGs_isGetItemBeast(dBeastIdx_GREEN_JELLY_e);
    case dItemNo_BLUE_JELLY_e: return dComIfGs_isGetItemBeast(dBeastIdx_BLUE_JELLY_e);
    case dItemNo_JOY_PENDANT_e: return dComIfGs_isGetItemBeast(dBeastIdx_JOY_PENDANT_e);
    case dItemNo_BIRD_BAIT_5_e: return dComIfGs_isGetItemBait(0);
    case dItemNo_HYOI_PEAR_e: return dComIfGs_isGetItemBait(1);
    default:
        return false;
    }
}
bool sDirect = false;

struct SaveSnap {
    u8 save[sizeof(dSv_save_c)];
};
SaveSnap sSnap;
void snapSave(SaveSnap& s) {
    dSv_save_c& sv = g_dComIfG_gameInfo.save.getSavedata();
    memcpy(s.save, &sv, sizeof s.save);
    const size_t clock = (size_t)((u8*)&sv.getPlayer().getPlayerStatusB() - (u8*)&sv);
    memset(s.save + clock, 0, sizeof(dSv_player_status_b_c));
}

void setUp() {
    sSavedMem = memory();
    for (int i = 0; i < 8; i++) {
        sSavedSave[i] = saved(i);
    }
    dComIfGs_onStageBossEnemy(sStage);
    dComIfGs_offStageLife(sStage);
    dComIfGs_onStageBossEnemy(2);
    dComIfGs_offStageLife(2);
}

void restoreBits() {
    memory() = sSavedMem;
    for (int i = 0; i < 8; i++) {
        saved(i) = sSavedSave[i];
    }
}

void spawn(unsigned int frames) {
    daPy_lk_c* p = daPy_getPlayerLinkActorClass();
    const s16 yaw = p->shape_angle.y;
    const f32 dist = 150.0f + 120.0f * (f32)(sJob % 4);
    sPos.set(p->current.pos.x + dist * cM_ssin(yaw), p->current.pos.y + 30.0f, p->current.pos.z + dist * cM_scos(yaw));
    const int room = fopAcM_GetRoomNo(p);
    csXyz angle = csXyz::Zero;
    layer_class* savedLayer = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    switch (sJob) {
    case BITEM_HERE:
    case T2_BITEM:
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
    }
    fpcLy_SetCurrentLayer(savedLayer);
    writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u: %s spawned (want 0x%02X)\n", kName, frames, kJobName[sJob],
           want());
}

void finish() {
    setDrivenPad(false, 0, 0, 0);
    restoreBits();
    sState = kDone;
    randoSmokeEnd(kName);
}

// The T1 check of a job's item; true when the job is done.
bool pollT1(unsigned int frames) {
    daItem_c* item = (daItem_c*)fpcM_Search(nearItem, nullptr);
    const unsigned int wait = sJob == CLOUD ? kCloudWaitFrames : kWaitFrames;
    if (item == nullptr && sSince < wait) {
        return false;
    }
    char what[200];
    const u8 w = want();
    if (item == nullptr) {
        snprintf(what, sizeof what, "T1 %s %s: no item came out (want 0x%02X)", locName(jobLoc()), kJobName[sJob], w);
        randoSmokeCheck(kName, what, false);
        return true;
    }
    const u32 prm = fopAcM_GetParam(item);
    const u32 bit = (prm >> 8) & 0xFF;
    const u32 wantBit = pc_rando() ? 0x15u : 0xFFu;
    snprintf(what, sizeof what, "T1 %s %s: item 0x%02X flag 0x%02X (want 0x%02X flag 0x%02X)", locName(jobLoc()),
             kJobName[sJob], (unsigned)item->getItemNo(), (unsigned)bit, w, wantBit);
    randoSmokeCheck(kName, what, w != 0xFE && sameItem(item->getItemNo(), w) && bit == wantBit);
    (void)frames;
    if (sJob == T2_BITEM) {
        sItemId = fopAcM_GetID(item);
        sItemNo = item->getItemNo();
    } else {
        fopAcM_delete(item);
    }
    return true;
}

} // namespace

void randoSmokeBossloc(unsigned int frames) {
    if (sState == kDone || !randoSmokeInPlay(10)) {
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return;
    }
    sSince++;
    switch (sState) {
    case kInit: {
        sStage = dStage_stagInfo_GetSaveTbl(dComIfGp_getStageStagInfo());
        const char* t2 = getenv("COS_BOSSLOC_T2");
        sT2 = t2 != nullptr && t2[0] == '1';
        writef(STDERR_FILENO, "[cos] rando-smoke %s: stage %s, save table %d (boss location %d), pc_rando() %d, T2 %d\n",
               kName, dComIfGp_getStartStageName(), sStage, pc_rando_boss_loc(sStage), pc_rando(), sT2 ? 1 : 0);
        if (!randoSmokeCheck(kName, "a boss stage is loaded", pc_rando_boss_loc(sStage) >= 0)) {
            finish();
            return;
        }
        setUp();
        sJob = BITEM_HERE;
        sState = kSpawn;
        sSince = 0;
        return;
    }
    case kSpawn:
        if (sJob == T2_BITEM) {
            // any opening event first (A through it), then a moment of quiet
            if (dComIfGp_event_runCheck()) {
                setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
                sSince = 0;
                return;
            }
            setDrivenPad(true, 0, 0, 0);
            if (sSince < 30) {
                return;
            }
            dComIfGs_offStageLife(sStage);
        }
        spawn(frames);
        sState = kPoll;
        sSince = 0;
        return;
    case kPoll:
        if (!pollT1(frames)) {
            return;
        }
        if (sJob == T2_BITEM) {
            if (sItemId == fpcM_ERROR_PROCESS_ID_e) {
                finish();
                return;
            }
            dComIfGp_event_setGtItm(dItemNo_NONE_e);
            sDirect = directGet(sItemNo);
            snapSave(sSnap);
            sState = kT2Start;
            sSince = 0;
            return;
        }
        sJob++;
        if (sJob == T2_BITEM && !sT2) {
            finish();
            return;
        }
        sState = kSpawn;
        sSince = 0;
        return;
    case kT2Start: {
        setDrivenPad(true, 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            writef(STDERR_FILENO, "[cos] rando-smoke %s: T2: event started after %u frame(s), item 0x%02X\n", kName, sSince,
                   (unsigned)dComIfGp_event_getGtItm());
            sState = kT2Event;
            sSince = 0;
            return;
        }
        daItem_c* item = (daItem_c*)fpcM_Search(judgeById, &sItemId);
        if (item == nullptr && sDirect && sSince >= 10) {
            // picked up and given at once, as the game gives this item (no get-item event)
            SaveSnap now;
            snapSave(now);
            const bool changed = memcmp(&now, &sSnap, sizeof now) != 0;
            char what[200];
            snprintf(what, sizeof what, "T2 %s: picked up without a get-item event (the game gives 0x%02X directly), save changed %d",
                     locName(jobLoc()), (unsigned)sItemNo, changed ? 1 : 0);
            randoSmokeCheck(kName, what, changed);
            finish();
            return;
        }
        if (item != nullptr) {
            if (sSince >= 10) {
                item->current.pos = player->current.pos;
                item->old.pos = player->current.pos;
                item->speedF = 0.0f;
                item->speed.setall(0.0f);
            }
        }
        if (sSince >= kStartFrames) {
            char what[160];
            snprintf(what, sizeof what, "T2 %s: the item-get event started", locName(jobLoc()));
            randoSmokeCheck(kName, what, false);
            finish();
        }
        return;
    }
    case kT2Event:
        setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (!dComIfGp_event_runCheck()) {
            const u8 gt = dComIfGp_event_getGtItm();
            SaveSnap now;
            snapSave(now);
            const bool changed = memcmp(&now, &sSnap, sizeof now) != 0;
            char what[200];
            snprintf(what, sizeof what, "T2 %s: picked up, got 0x%02X (field item 0x%02X), save changed %d", locName(jobLoc()),
                     (unsigned)gt, sItemNo, changed ? 1 : 0);
            randoSmokeCheck(kName, what, sItemNo >= 0 && gt == sItemNo && changed);
            finish();
            return;
        }
        if (sSince >= kEventFrames) {
            char what[160];
            snprintf(what, sizeof what, "T2 %s: the item-get event ended", locName(jobLoc()));
            randoSmokeCheck(kName, what, false);
            finish();
        }
        return;
    case kDone:
        return;
    }
}

} // namespace pc
