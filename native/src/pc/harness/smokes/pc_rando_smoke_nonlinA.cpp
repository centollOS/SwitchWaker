// COS_SMOKE=rando COS_RANDO_SMOKE=nonlinA (docs/RANDOMIZER_PLAN.md step R3.9): the nonlinear patches
// of make_game_nonlinear.asm items 1-11 in a booted game, e.g.
//   run.sh rando --stage sea:44:206 --env COS_RANDO_SMOKE=nonlinA [--env COS_RANDO_FORCE=1]
// Every check expects the randomizer's value when pc_rando() is 1 and the original game's when it is
// 0, so the plain run checks that a non-randomized game is unchanged. On a fresh debug-boot save
// (no pearls, no bags, no bosses beaten), once the play scene has run 10 frames:
//   1. direct calls: check_itemno with bait and spoils and no bags (d_item.cpp); the four
//      recollection functions leave the save alone (rando only: the original copies the inventory);
//      pc_rando_reset_partner_position for a Wind Temple and an Earth Temple entry and for an entry
//      outside the tables (the player's position); the boat (when the stage has one): checkOutRange
//      in the middle of the sea before Dragon Roost, and its "get in" offer with event 0x0908, without
//      and with the Sail (the player put next to it; skipped when the player rides it);
//   2. actors, one at a time, created next to the player after setting the save state their hunk
//      tests, then deleted: a fishman (exists only in rando: Gohma alive), Koroks of both variants
//      (Farore's Pearl counts as owned), a withered tree (its first state), an Earth Temple song
//      tablet (acts as if the song is owned), a mailbox with a letter waiting (no Din's Pearl), the
//      Hyrule warp with Zelda awakened (FF3 bit 0x1820 untouched in rando), the Forbidden Woods void
//      with Farore's Pearl (stays in rando), Makar with the Master Sword (Full Power) and Medli with
//      the Master Sword (Half Power) (both stay in rando).
// Not checked here: dMenu_Dmap_c::treasureSet (the dungeon map's partner marker, a menu).
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_qol.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_npc_bj1.h"
#include "d/actor/d_a_obj_ftree.h"
#include "d/actor/d_a_obj_toripost.h"
#include "d/actor/d_a_ship.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_letter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstring>
#include <unistd.h>

// daObjTpost_c::mNumReadable and dEvt_info_c::mpCheckCB are private: the standard
// explicit-instantiation access for a test.
namespace {
struct TpostReadable {
    using type = int daObjTpost_c::*;
    friend type robbed(TpostReadable);
};
struct EvtCheckCB {
    using type = dEvt_info_c::CallbackFunc dEvt_info_c::*;
    friend type robbed(EvtCheckCB);
};
template <typename Tag, typename Tag::type M> struct Rob {
    friend typename Tag::type robbed(Tag) { return M; }
};
template struct Rob<TpostReadable, &daObjTpost_c::mNumReadable>;
template struct Rob<EvtCheckCB, &dEvt_info_c::mpCheckCB>;
} // namespace

namespace pc {

namespace {

constexpr const char* kName = "nonlinA";
constexpr unsigned int kSpawnTimeout = 600; // frames for a creation (archives load from the disc)
constexpr unsigned int kRunFrames = 3;      // executing frames before the check
constexpr float kSpawnDistance = 300.0f;

bool check(const char* what, bool ok) {
    return randoSmokeCheck(kName, what, ok);
}

dSv_player_collect_c& collect() {
    return g_dComIfG_gameInfo.save.getPlayer().getCollect();
}

// ---- 1. direct calls ----

void checkItemNo() {
    const bool r = pc_rando() != 0;
    check("check_itemno: Hyoi Pear without the Bait Bag",
          check_itemno(dItemNo_HYOI_PEAR_e) == (r ? dItemNo_HYOI_PEAR_e : dItemNo_GREEN_RUPEE_e));
    check("check_itemno: Skull Necklace without the Spoils Bag",
          check_itemno(dItemNo_SKULL_NECKLACE_e) == (r ? dItemNo_SKULL_NECKLACE_e : dItemNo_GREEN_RUPEE_e));
}

void checkRecollection() {
    if (!pc_rando()) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: recollection: original code, not called\n", kName);
        return;
    }
    static dSv_info_c before;
    memcpy(&before, &g_dComIfG_gameInfo.save, sizeof before);
    dComIfGs_copyPlayerRecollectionData();
    dComIfGs_setPlayerRecollectionData();
    dComIfGs_exchangePlayerRecollectionData();
    dComIfGs_revPlayerRecollectionData();
    check("recollection: the four functions leave the save alone",
          memcmp(&before, &g_dComIfG_gameInfo.save, sizeof before) == 0);
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.01f;
}

void setStartPoint(s16 point) {
    dStage_startStage_c* start = dComIfGp_getStartStage();
    char name[8];
    memcpy(name, start->getName(), sizeof name);
    start->set(name, start->getRoomNo(), point, start->getLayer());
}

void checkPartner() {
    const s16 savedPoint = dComIfGp_getStartStage()->getPoint();
    const u8 flag = dComIfGs_getPlayerPriestFlag();

    setStartPoint(15); // Wind Temple entrance
    pc_rando_reset_partner_position(1);
    cXyz p = dComIfGs_getPlayerPriestPos();
    check("partner reset: Makar at the Wind Temple entrance",
          near(p.x, -3651.02f) && near(p.y, 1557.67f) && near(p.z, 13235.2f) &&
              dComIfGs_getPlayerPriestRotate() == (s16)0x94A0 && dComIfGs_getPlayerPriestRoomNo() == 0xF &&
              dComIfGs_getPlayerPriestFlag() == flag);

    setStartPoint(27); // boss door
    pc_rando_reset_partner_position(2);
    p = dComIfGs_getPlayerPriestPos();
    check("partner reset: Medli at the Earth Temple boss door",
          near(p.x, -5689.21f) && near(p.y, -2300.0f) && near(p.z, 9667.67f) &&
              dComIfGs_getPlayerPriestRotate() == 0x4000 && dComIfGs_getPlayerPriestRoomNo() == 15);

    setStartPoint(206); // not in the tables: the player's place
    pc_rando_reset_partner_position(2);
    p = dComIfGs_getPlayerPriestPos();
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    check("partner reset: another entry takes the player's place",
          near(p.x, player->current.pos.x) && near(p.y, player->current.pos.y) && near(p.z, player->current.pos.z) &&
              dComIfGs_getPlayerPriestRotate() == player->shape_angle.y &&
              dComIfGs_getPlayerPriestRoomNo() == (s8)dComIfGp_roomControl_getStayNo());
    setStartPoint(savedPoint);
}

// The boat: checkOutRange now, the "get in" offer over the next frames.
enum BoatState { kBoatNone, kBoatOffer, kBoatSail, kBoatDone };
BoatState sBoat = kBoatNone;
unsigned int sBoatFrames = 0;

void checkBoatRange(daShip_c* boat) {
    const bool free = pc_rando() || pc_qol(PC_QOL_FREE_BOAT);
    const bool saved0902 = dComIfGs_isEventBit(dSv_event_flag_c::UNK_0902);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_0902);
    const cXyz pos = boat->current.pos;
    const s16 angle = boat->m038C;
    boat->current.pos.x = 0.0f; // the middle of the sea: outside path 0 (before Dragon Roost)
    boat->current.pos.z = 0.0f;
    const BOOL outMiddle = boat->checkOutRange();
    boat->current.pos.x = 0.0f; // past the north edge: outside path 3, kept
    boat->current.pos.z = -355000.0f;
    const BOOL outEdge = boat->checkOutRange();
    boat->current.pos = pos;
    boat->m038C = angle;
    if (saved0902) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0902);
    }
    check("boat: checkOutRange in the middle of the sea before Dragon Roost", outMiddle == (free ? FALSE : TRUE));
    check("boat: checkOutRange past the edge of the sea", outEdge == TRUE);
}

// Link next to the boat (behind it), 0x0908 on, no Sail: the boat offers "get in" only in the
// original game.
void placeLinkAtBoat(daShip_c* boat) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    player->current.pos.x = boat->current.pos.x - 60.0f * cM_ssin(boat->shape_angle.y);
    player->current.pos.z = boat->current.pos.z - 60.0f * cM_scos(boat->shape_angle.y);
    player->current.pos.y = boat->current.pos.y;
}

bool boatStep(unsigned int) {
    daShip_c* boat = dComIfGp_getShipActor();
    if (sBoat == kBoatDone) {
        return true;
    }
    if (boat == nullptr) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: boat: none in this stage, its checks skipped\n", kName);
        sBoat = kBoatDone;
        return true;
    }
    if (sBoat == kBoatNone) {
        checkBoatRange(boat);
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0908);
        sBoat = kBoatOffer;
        sBoatFrames = 0;
    }
    if (dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: boat: the player rides it, the \"get in\" check skipped\n",
               kName);
        dComIfGs_offEventBit(dSv_event_flag_c::UNK_0908);
        sBoat = kBoatDone;
        return true;
    }
    // First without the Sail (a sailing preset gives it: taken for the check), then with it.
    dSv_player_get_item_c& got = g_dComIfG_gameInfo.save.getPlayer().getGetItem();
    if (sBoat == kBoatOffer) {
        got.offItem(1, 0);
    } else {
        got.onItem(1, 0);
    }
    placeLinkAtBoat(boat);
    if (++sBoatFrames < 4) {
        return false;
    }
    const bool offer = (boat->attention_info.flags & fopAc_Attn_ACTION_SHIP_e) != 0;
    writef(STDERR_FILENO, "[cos] rando-smoke %s: boat: %s Sail, attention flags 0x%x\n", kName,
           sBoat == kBoatOffer ? "no" : "with the", (unsigned)boat->attention_info.flags);
    if (sBoat == kBoatOffer) {
        check("boat: \"get in\" with event 0x0908 and no Sail", offer == !pc_rando());
        sBoat = kBoatSail;
        sBoatFrames = 0;
        return false;
    }
    check("boat: \"get in\" with event 0x0908 and the Sail", offer);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_0908);
    sBoat = kBoatDone;
    return true;
}

// ---- 2. actors ----

struct ActorTest {
    const char* what;
    s16 proc;
    u32 param;
    void (*setup)();
    // Called with the actor once it has executed kRunFrames frames, or nullptr when it is gone
    // (refused or deleted itself).
    void (*verify)(const char* what, fopAc_ac_c* actor);
    void (*teardown)();
};

void expectExists(const char* what, fopAc_ac_c* actor, bool want) {
    char line[160];
    snprintf(line, sizeof line, "%s: %s", what, want ? "exists" : "is not created");
    check(line, (actor != nullptr) == want);
}

void none() {}

// The save states the tests assume (a sailing preset gives the pearls).
void noFarore() {
    collect().offSymbol(dSymbol_FARORE_e);
}

// Fishman: Gohma alive on the fresh save.
void verifySo(const char* what, fopAc_ac_c* a) {
    expectExists(what, a, pc_rando() != 0);
}

// Korok, the Farore's-Pearl variant (param 1, init_BJX_1): no pearl.
void verifyBjOwned(const char* what, fopAc_ac_c* a) {
    expectExists(what, a, pc_rando() != 0);
}
// Korok, the before-the-pearl variant (param 0, init_BJX_0) with event 0x1801.
void setupBjBefore() {
    noFarore();
    dComIfGs_onEventBit(0x1801);
}
void verifyBjBefore(const char* what, fopAc_ac_c* a) {
    expectExists(what, a, pc_rando() == 0);
}
void teardownBjBefore() {
    dComIfGs_offEventBit(0x1801);
}

// Withered tree: action 0 (none) without the pearl in the original game, a waiting state in rando.
void verifyFtree(const char* what, fopAc_ac_c* a) {
    if (!check("withered tree: created", a != nullptr)) {
        return;
    }
    const int mode = static_cast<daObjFtree::Act_c*>(a)->mMode;
    writef(STDERR_FILENO, "[cos] rando-smoke %s: withered tree: mode %d\n", kName, mode);
    check(what, pc_rando() ? mode != 0 : mode == 0);
}

// Earth Temple song tablet (type 0, Earth God's Lyric not owned): Create sets the X/Y-item check
// callback (state 8, teach the song) in the original game and leaves it unset (state 0, song owned)
// in rando. (d_a_obj_mknjd.h defines Act_c::M_arcname, so it is not included here.)
void verifyMknjd(const char* what, fopAc_ac_c* a) {
    if (!check("song tablet: created", a != nullptr)) {
        return;
    }
    const bool cb = a->eventInfo.*robbed(EvtCheckCB()) != nullptr;
    writef(STDERR_FILENO, "[cos] rando-smoke %s: song tablet: X/Y check callback %s\n", kName, cb ? "set" : "unset");
    check(what, cb == (pc_rando() == 0));
}

// Mailbox with Komali's father's letter waiting (m_letter[1]), no Din's Pearl.
void setupTpost() {
    collect().offSymbol(dSymbol_DIN_e);
    dLetter_stock(dSv_event_flag_c::LETTER_KOMALIS_FATHER);
}
void verifyTpost(const char* what, fopAc_ac_c* a) {
    if (!check("mailbox: created", a != nullptr)) {
        return;
    }
    const int readable = static_cast<daObjTpost_c*>(a)->*robbed(TpostReadable());
    writef(STDERR_FILENO, "[cos] rando-smoke %s: mailbox: readable %d\n", kName, readable);
    check(what, readable == (pc_rando() ? 1 : 0));
}

// Hyrule warp with Zelda awakened: the original sets the FF3 bit 0x1820.
void setupWarp() {
    dComIfGs_onEventBit(dSv_event_flag_c::ZELDA_AWAKENED);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1820);
}
void verifyWarp(const char* what, fopAc_ac_c* a) {
    if (!check("Hyrule warp: created", a != nullptr)) {
        return;
    }
    check(what, (dComIfGs_isEventBit(dSv_event_flag_c::UNK_1820) != 0) == (pc_rando() == 0));
}
void teardownWarp() {
    dComIfGs_offEventBit(dSv_event_flag_c::ZELDA_AWAKENED);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1820);
}

// Forbidden Woods void with Farore's Pearl.
void setupFarore() {
    dComIfGs_onSymbol(dSymbol_FARORE_e);
}
void verifyTagRet(const char* what, fopAc_ac_c* a) {
    expectExists(what, a, pc_rando() != 0);
}

// Makar, the Forbidden Woods type (param 1), with the Master Sword (Full Power) equipped and
// Farore's Pearl: the original game removes him (only the boss-room type stays).
u8 sSavedSword = 0;
void setupMakar() {
    dComIfGs_onSymbol(dSymbol_FARORE_e);
    sSavedSword = dComIfGs_getSelectEquip(0);
    g_dComIfG_gameInfo.save.getPlayer().getPlayerStatusA().setSelectEquip(0, dItemNo_MASTER_SWORD_3_e);
}
void verifyPartner(const char* what, fopAc_ac_c* a) {
    expectExists(what, a, pc_rando() != 0);
}
void teardownMakar() {
    g_dComIfG_gameInfo.save.getPlayer().getPlayerStatusA().setSelectEquip(0, sSavedSword);
    collect().offSymbol(dSymbol_FARORE_e);
    dComIfGp_setCb1Player(nullptr);
}

// Medli on the sea (after Dragon Roost: event 0x1820 and Gohma beaten) with the Master Sword (Half
// Power) collected: the original game removes her outside M_DaiB.
void setupMedli() {
    dComIfGs_onCollect(0, 2);
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_1820);
    dComIfGs_onStageBossEnemy(dSv_save_c::STAGE_DRC);
}
void teardownMedli() {
    dComIfGs_offCollect(0, 2);
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1820);
    dComIfGp_setCb1Player(nullptr);
}

const ActorTest kTests[] = {
    {"fishman before Gohma", fpcNm_NPC_SO_e, 0, none, verifySo, none},
    {"Korok, Farore's Pearl variant, without the pearl", fpcNm_NPC_BJ1_e, 0x00FF0001, noFarore, verifyBjOwned, none},
    {"Korok, before-the-pearl variant", fpcNm_NPC_BJ1_e, 0x00FF0000, setupBjBefore, verifyBjBefore, teardownBjBefore},
    {"withered tree: first state without Farore's Pearl", fpcNm_Obj_Ftree_e, 0, noFarore, verifyFtree, none},
    {"song tablet: state without the Earth God's Lyric", fpcNm_Obj_MknjD_e, 0, none, verifyMknjd, none},
    {"mailbox: letters without Din's Pearl", fpcNm_OBJ_TORIPOST_e, 0, setupTpost, verifyTpost, none},
    {"Hyrule warp: FF3 bit 0x1820 set", fpcNm_WARPDEMO20_e, 0, setupWarp, verifyWarp, teardownWarp},
    {"Forbidden Woods void with Farore's Pearl", fpcNm_Tag_Ret_e, 0, setupFarore, verifyTagRet, none},
    {"Makar with the Master Sword (Full Power)", fpcNm_NPC_CB1_e, 1, setupMakar, verifyPartner, teardownMakar},
    {"Medli with the Master Sword (Half Power)", fpcNm_NPC_MD_e, 0, setupMedli, verifyPartner, teardownMedli},
};
constexpr int kTestCount = (int)(sizeof kTests / sizeof kTests[0]);

enum Phase { kDirect, kBoat, kSpawn, kWait, kDelete, kEnd };
Phase sPhase = kDirect;
int sTest = 0;
fpc_ProcID sId = fpcM_ERROR_PROCESS_ID_e;
unsigned int sWaited = 0;
unsigned int sRan = 0;
unsigned int sEndFrame = 0;

bool spawn(const ActorTest& t) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const s16 yaw = player->shape_angle.y;
    cXyz pos = player->current.pos;
    pos.x += kSpawnDistance * cM_ssin(yaw);
    pos.z += kSpawnDistance * cM_scos(yaw);
    csXyz angle(0, (s16)(yaw + 0x8000), 0);
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(static_cast<base_process_class*>(player)->mLyTg.mpLayer);
    sId = fopAcM_create(t.proc, t.param, &pos, fopAcM_GetRoomNo(player), &angle);
    fpcLy_SetCurrentLayer(saved);
    return sId != fpcM_ERROR_PROCESS_ID_e;
}

} // namespace

void randoSmokeNonlinA(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    switch (sPhase) {
    case kDirect:
        writef(STDERR_FILENO, "[cos] rando-smoke %s: pc_rando() = %d\n", kName, pc_rando());
        checkItemNo();
        checkRecollection();
        checkPartner();
        sPhase = kBoat;
        return;
    case kBoat:
        if (boatStep(frames)) {
            sPhase = kSpawn;
        }
        return;
    case kSpawn: {
        if (sTest >= kTestCount) {
            sPhase = kEnd;
            sEndFrame = frames + 30;
            return;
        }
        const ActorTest& t = kTests[sTest];
        t.setup();
        if (!spawn(t)) {
            check(t.what, false);
            t.teardown();
            sTest++;
            return;
        }
        sWaited = 0;
        sRan = 0;
        sPhase = kWait;
        return;
    }
    case kWait: {
        const ActorTest& t = kTests[sTest];
        // Only an executing actor counts: fopAcM_SearchByID also finds one still creating.
        fopAc_ac_c* actor = fpcM_IsExecuting(sId) ? fopAcM_SearchByID(sId) : nullptr;
        if (actor != nullptr) {
            if (++sRan < kRunFrames) {
                return;
            }
            t.verify(t.what, actor);
            fopAcM_delete(actor);
            sWaited = 0;
            sPhase = kDelete;
            return;
        }
        if (!fpcM_IsCreating(sId)) {
            t.verify(t.what, nullptr); // refused, or deleted itself
            t.teardown();
            sTest++;
            sPhase = kSpawn;
            return;
        }
        if (++sWaited > kSpawnTimeout) {
            writef(STDERR_FILENO, "[cos] rando-smoke %s: %s: still creating after %u frames\n", kName, t.what,
                   kSpawnTimeout);
            check(t.what, false);
            fpcM_Delete(fopAcM_SearchByID(sId));
            sPhase = kEnd;
            sEndFrame = frames + 30;
        }
        return;
    }
    case kDelete: {
        if ((fopAcM_SearchByID(sId) != nullptr || fpcM_IsCreating(sId)) && ++sWaited < 120) {
            return;
        }
        kTests[sTest].teardown();
        sTest++;
        sPhase = kSpawn;
        return;
    }
    case kEnd:
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
}

} // namespace pc
