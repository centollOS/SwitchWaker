// COS_SMOKE=rando COS_RANDO_SMOKE=scob (docs/RANDOMIZER_PLAN.md step R2.3): the 57 SCOB item
// locations (salvage points, Big Octos, Gunboats, the black-soil dig spot) placed at archive load
// (features/rando/pc_rando_place_scob.h), the Needle Rock salvage flags, the salvage light hunk
// (daSalvage_c::proc_wait) and the dig hunk (daTagKbItem_c::dig_main). Run with the boat:
//   run.sh rando --stage sea:44:206 --preset sailing --env COS_RANDO_SMOKE=scob
//                [--env COS_SCOB_MODE=t1|dig|salvage] [--env COS_SCOB_ROOMS=1-49|44,29,...]
//                [--env COS_SCOB_VANILLA=1] [--env COS_RANDO_PLACEMENT=<placement.json>]
// Once the play scene has run 10 frames the smoke turns pc_rando() on with its own placement (every
// SCOB location gets an item other than its vanilla one, kItems round-robin; the dig spot gets the
// Grappling Hook and Outset's sunken treasure a Piece of Heart; every other location keeps its vanilla
// item), unless a COS_RANDO_PLACEMENT seed is active already (then its items are expected) or
// COS_SCOB_VANILLA=1 (pc_rando() stays 0 and everything must stay vanilla). Then:
//   t1 (default): for each room of COS_SCOB_ROOMS (default 1-49) it warps to sea room N point 100 (on
//      the boat) and, 30 frames after the room is up, checks
//        - every SCOB key of the room: patched (PCR_SCOB_PLACED) with the expected item; vanilla: not
//          dispatched at all (PCR_SCOB_NOT_SEEN);
//        - the salvage registry (daSalvage_c::mTagData_p) holds, for every Salvage/SwSlvg location of
//          the room, an entry of that room with that item (kind 0 entries need no chart: the registry
//          takes them before the chart is opened);
//        - room 44: a live TagKb actor holds the dig spot's item;
//        - room 29: the Needle Rock flags were rewritten (8 and 9) on its load (vanilla: untouched).
//   dig: Outset (room 44); the player stands by the dig spot, a pig (KB, the room's own or a new one)
//      digs there (daTagKbItem_c::kb_dig); the spot must create a field item of the placed item (the
//      R2.3 hunk: fopAcM_createItem, so its model is loaded), the item is moved onto the player and
//      its item-get demo must give that item (dComIfGp_event_getGtItm, then checkItemGet, or the max
//      life for a Piece of Heart).
//   salvage: Outset's sunken treasure (room 44, Triforce Chart 4's point, a Piece of Heart here): the
//      chart opened but not deciphered: the point must stay unused under rando (the R2.3 hunk tests the
//      chart index: the light waits for the decipher) while the vanilla rule (by item) would light it;
//      once deciphered it must be used. Then the boat stops over the point with the Grappling Hook's
//      crane lowered (X held) and the SALVAGE_GETITEM event must give the placed item.
// Exit 0 when all hold, else 1. The full t1 (49 warps) takes about 40 s uncapped; the regress lines
// run a sample of rooms. Vanilla t1 fails on sea room 1 by design of the check: in the sailing preset's
// state Forsaken Fortress registers no salvage, so the vanilla lines leave it out.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_place_scob.h"

#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_item.h"
#include "d/actor/d_a_salvage.h"
#include "d/actor/d_a_ship.h"
#include "d/actor/d_a_tag_kb_item.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_salvage.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "scob";
constexpr unsigned int kArriveFrames = 1200;
constexpr unsigned int kRoomSettle = 30;
constexpr unsigned int kEventFrames = 3000;
constexpr unsigned int kTalkFrames = 30;
constexpr int kSeaPoint = 100;  // a corner spawn of every sea room (mode 2: on the boat)
constexpr int kOutsetLand = 206; // Outset, where the new game starts
// Items the smoke places (none progressive, none a dungeon item): round-robin over the locations.
const uint8_t kItems[] = {0x01, 0x02, 0x04, 0x05, 0x06, 0x07, 0x0F, 0x20, 0x25, 0x30, 0x33, 0x34};
constexpr uint8_t kDigItem = dItemNo_GRAPPLING_HOOK_e;
constexpr uint8_t kOutsetSalvageItem = dItemNo_HEART_PIECE_e; // vanilla: Triforce Shard 4 (0x64)

enum Mode { kT1, kDig, kSalvage };
enum State { kOff, kWarp, kArrive, kSettle, kDigStart, kDigWait, kDigEvent, kSlvgStart, kSlvgCrane,
             kSlvgEvent, kDone };

Mode sMode = kT1;
State sState = kOff;
bool sVanilla = false;
uint8_t sItems[PCR_LOC_COUNT];
std::vector<int> sRooms;
size_t sRoomIdx = 0;
int sRoom = -1;
int sPoint = kSeaPoint;
unsigned int sSince = 0;
fpc_ProcID sScene = fpcM_ERROR_PROCESS_ID_e;
unsigned int sEventFrames = 0;
bool sSawEvent = false;
fpc_ProcID sPigId = fpcM_ERROR_PROCESS_ID_e;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* s = fpcM_Search(isPlayScene, nullptr);
    return s != nullptr ? fpcM_GetID(s) : fpcM_ERROR_PROCESS_ID_e;
}

uint8_t expected(int loc, uint8_t vanilla) {
    return sVanilla ? vanilla : pc_rando_loc_item(loc, vanilla);
}

void buildPlacement() {
    size_t n = 0;
    for (int l = 0; l < PCR_LOC_COUNT; l++) {
        const pcr::Span<pcr::LocKey> ks = pcr::location_keys(l);
        const uint8_t vanilla = ks.size > 0 ? ks[0].vanilla : 0xFF;
        sItems[l] = vanilla;
        const pcr::LocInfo* info = pcr::location_info(l);
        if (info == nullptr || info->classes != pcr::PCR_CLASS_SCOB) {
            continue;
        }
        uint8_t it = kItems[n++ % sizeof kItems];
        if (it == vanilla) {
            it = kItems[n++ % sizeof kItems];
        }
        sItems[l] = it;
    }
    sItems[PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL] = kDigItem;
    sItems[PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE] = kOutsetSalvageItem;
}

void parseRooms(const char* s) {
    sRooms.clear();
    std::string str = s != nullptr && s[0] != '\0' ? s : "1-49";
    size_t pos = 0;
    while (pos < str.size()) {
        size_t end = str.find(',', pos);
        std::string part = str.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        int a = 0, b = 0;
        if (sscanf(part.c_str(), "%d-%d", &a, &b) == 2) {
            for (int r = a; r <= b; r++) {
                sRooms.push_back(r);
            }
        } else if (sscanf(part.c_str(), "%d", &a) == 1) {
            sRooms.push_back(a);
        }
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
}

void warp(int room, int point) {
    sRoom = room;
    sPoint = point;
    sScene = playSceneId();
    sSince = 0;
    writef(STDERR_FILENO, "[cos] rando-smoke scob: to sea room %d point %d\n", room, point);
    dComIfGp_setNextStage("sea", (s16)point, (s8)room);
    sState = kArrive;
}

bool arrived() {
    const fpc_ProcID id = playSceneId();
    return id != fpcM_ERROR_PROCESS_ID_e && id != sScene && dComIfGp_getPlayer(0) != nullptr &&
           strcmp(dComIfGp_getStartStageName(), "sea") == 0 && dComIfGp_roomControl_getStayNo() == sRoom;
}

struct ActorFind {
    s16 name;
    int room;
    fopAc_ac_c* found;
    int (*pred)(fopAc_ac_c*, void*);
    void* arg;
};

int findActor(void* p, void* a) {
    ActorFind* f = (ActorFind*)a;
    fopAc_ac_c* ac = (fopAc_ac_c*)p;
    if (fopAcM_GetName(ac) != f->name || (f->room >= 0 && fopAcM_GetRoomNo(ac) != f->room)) {
        return 0;
    }
    if (f->pred != nullptr && !f->pred(ac, f->arg)) {
        return 0;
    }
    f->found = ac;
    return 1;
}

fopAc_ac_c* find(s16 name, int room, int (*pred)(fopAc_ac_c*, void*) = nullptr, void* arg = nullptr) {
    ActorFind f = {name, room, nullptr, pred, arg};
    fopAcIt_Executor(findActor, &f);
    return f.found;
}

int itemIs(fopAc_ac_c* ac, void* arg) {
    return ((daItem_c*)ac)->getItemNo() == *(uint8_t*)arg;
}

// The registry entry of a room holding an item (index), or -1.
int registryFind(int room, uint8_t item) {
    dSalvage_control_c* c = daSalvage_c::mTagData_p;
    if (c == nullptr) {
        return -1;
    }
    for (int i = 0; i < dSalvage_control_c::MAX_TOTAL; i++) {
        if (c->checkRegist(i) && c->mInfo[i].mRoomNo == room && c->mInfo[i].mItemNo == item) {
            return i;
        }
    }
    return -1;
}

void checkRoom(int room) {
    char arc[16];
    snprintf(arc, sizeof arc, "Room%d", room);
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    int nKeys = 0, nOk = 0;
    std::vector<int> salvageLocs;
    for (size_t i = 0; i < keys.size; i++) {
        const pcr::LocKey& k = keys[i];
        if (k.kind != pcr::KeyKind::Dzx || strcmp(k.stage, "sea") != 0 || strcmp(k.arc, arc) != 0 ||
            (k.field != pcr::DzxField::SalvageItem && k.field != pcr::DzxField::TagKbItem)) {
            continue;
        }
        nKeys++;
        const int r = pc_rando_place_scob_result((int)i);
        const uint8_t want = expected(k.loc, k.vanilla);
        const bool ok = sVanilla ? r == PCR_SCOB_NOT_SEEN : (r == PCR_SCOB_PLACED && pc_rando_place_scob_item((int)i) == want);
        if (!ok) {
            writef(STDERR_FILENO, "[cos] rando-smoke scob: room %d SCOB[%u] loc %d: result %d item 0x%02X, want 0x%02X\n",
                   room, (unsigned)k.index, (int)k.loc, r, (unsigned)pc_rando_place_scob_item((int)i), (unsigned)want);
        }
        nOk += ok ? 1 : 0;
        if (k.field == pcr::DzxField::SalvageItem &&
            (salvageLocs.empty() || salvageLocs.back() != (int)k.loc)) {
            salvageLocs.push_back(k.loc);
        }
    }
    char what[128];
    snprintf(what, sizeof what, "room %d: %d/%d SCOB key(s) %s", room, nOk, nKeys, sVanilla ? "untouched" : "placed");
    randoSmokeCheck(kName, what, nKeys > 0 && nOk == nKeys);

    int nReg = 0;
    for (int loc : salvageLocs) {
        const pcr::Span<pcr::LocKey> ks = pcr::location_keys(loc);
        const uint8_t want = expected(loc, ks[0].vanilla);
        const int e = registryFind(room, want);
        if (e < 0) {
            writef(STDERR_FILENO, "[cos] rando-smoke scob: room %d loc %d: no registry entry with item 0x%02X\n",
                   room, loc, (unsigned)want);
            dSalvage_control_c* c = daSalvage_c::mTagData_p;
            for (int i = 0; c != nullptr && i < dSalvage_control_c::MAX_TOTAL; i++) {
                if (c->mInfo[i].mRoomNo == room || c->mInfo[i].mItemNo == want) {
                    writef(STDERR_FILENO, "[cos] rando-smoke scob:   entry %d: regist %d room %d kind %u save %u item 0x%02X\n", i,
                           (int)c->checkRegist(i), (int)c->mInfo[i].mRoomNo, (unsigned)c->mInfo[i].mKind,
                           (unsigned)c->mInfo[i].mSaveNo, (unsigned)c->mInfo[i].mItemNo);
                }
            }
        } else {
            nReg++;
        }
    }
    snprintf(what, sizeof what, "room %d: %d/%zu salvage location(s) in the registry with their item", room, nReg,
             salvageLocs.size());
    randoSmokeCheck(kName, what, nReg == (int)salvageLocs.size());

    if (room == 44) {
        const uint8_t want = expected(PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL, 0x07);
        bool ok = false;
        ActorFind f = {fpcNm_TAG_KB_ITEM_e, 44, nullptr, nullptr, nullptr};
        struct Ctx { uint8_t want; bool ok; } ctx = {want, false};
        f.pred = [](fopAc_ac_c* ac, void* a) -> int {
            Ctx* c = (Ctx*)a;
            if (((daTagKbItem_c*)ac)->mItemNo == c->want) {
                c->ok = true;
                return 1;
            }
            return 0;
        };
        f.arg = &ctx;
        fopAcIt_Executor(findActor, &f);
        ok = ctx.ok;
        snprintf(what, sizeof what, "room 44: a TagKb actor holds the dig item 0x%02X", (unsigned)want);
        randoSmokeCheck(kName, what, ok);
    }
    if (room == 29) {
        const int nr = pc_rando_place_scob_needle_rock();
        snprintf(what, sizeof what, "room 29: Needle Rock salvage flags %s (%d)", sVanilla ? "untouched" : "8 and 9", nr);
        randoSmokeCheck(kName, what, sVanilla ? nr == -1 : nr == 2);
    }
}

void inPlayLayer(void (*fn)(void*), void* arg) {
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    fn(arg);
    fpcLy_SetCurrentLayer(saved);
}

void putPlayer(const cXyz& pos) {
    fopAc_ac_c* p = dComIfGp_getPlayer(0);
    p->current.pos = pos;
    p->old.pos = pos;
    p->speedF = 0.0f;
    p->speed = cXyz::Zero;
}

void noteBefore(uint8_t want, const char* who);

// --- dig -------------------------------------------------------------------------------------------
void digStart() {
    const uint8_t want = expected(PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL, 0x07);
    struct Ctx { uint8_t want; } ctx = {want};
    daTagKbItem_c* tag = (daTagKbItem_c*)find(fpcNm_TAG_KB_ITEM_e, 44, [](fopAc_ac_c* ac, void* a) -> int {
        return ((daTagKbItem_c*)ac)->mItemNo == ((Ctx*)a)->want;
    }, &ctx);
    if (tag == nullptr) {
        randoSmokeCheck(kName, "dig: the dig spot's TagKb actor", false);
        randoSmokeEnd(kName);
    }
    fopAc_ac_c* pig = sPigId != fpcM_ERROR_PROCESS_ID_e ? fopAcM_SearchByID(sPigId) : find(fpcNm_KB_e, -1);
    if (pig == nullptr) {
        if (sPigId == fpcM_ERROR_PROCESS_ID_e) {
            struct P { cXyz pos; fpc_ProcID id; } p = {tag->current.pos, fpcM_ERROR_PROCESS_ID_e};
            inPlayLayer([](void* a) {
                P* q = (P*)a;
                q->id = fopAcM_create(fpcNm_KB_e, 0, &q->pos, 44);
            }, &p);
            sPigId = p.id;
            writef(STDERR_FILENO, "[cos] rando-smoke scob: dig: no pig in the room; created one (id %d)\n", (int)sPigId);
            if (sPigId == fpcM_ERROR_PROCESS_ID_e) {
                randoSmokeCheck(kName, "dig: a pig", false);
                randoSmokeEnd(kName);
            }
        }
        if (++sSince > 300) {
            randoSmokeCheck(kName, "dig: the pig was created", false);
            randoSmokeEnd(kName);
        }
        return;
    }
    // the pig stands on the spot and digs (what daKb_c does over black soil)
    pig->current.pos = tag->current.pos;
    pig->old.pos = tag->current.pos;
    cXyz near = tag->current.pos;
    near.x += 150.0f;
    putPlayer(near);
    writef(STDERR_FILENO, "[cos] rando-smoke scob: dig at (%.0f, %.0f, %.0f), item 0x%02X\n", tag->current.pos.x,
           tag->current.pos.y, tag->current.pos.z, (unsigned)want);
    struct D { daTagKbItem_c* tag; fopAc_ac_c* pig; } d = {tag, pig};
    inPlayLayer([](void* a) {
        D* q = (D*)a;
        q->tag->kb_dig(q->pig);
    }, &d);
    noteBefore(want, "dig");
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    sSince = 0;
    sState = kDigWait;
}

void digWait() {
    uint8_t want = expected(PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL, 0x07);
    daItem_c* item = (daItem_c*)find(fpcNm_ITEM_e, -1, itemIs, &want);
    if (item == nullptr) {
        if (++sSince > 120) {
            randoSmokeCheck(kName, "dig: the spot created a field item of the placed item", false);
            randoSmokeEnd(kName);
        }
        return;
    }
    if (sSince < 1000) {
        randoSmokeCheck(kName, "dig: the spot created a field item of the placed item", true);
        sSince = 1000;
    }
    if (++sSince < 1000 + 40) {
        return; // let it land
    }
    sSince = 0;
    sEventFrames = 0;
    sSawEvent = false;
    sState = kDigEvent;
}

// The item's item_func ran: a Piece of Heart adds a quarter heart to the max life (it is no owned
// item for checkItemGet); any other item the smoke places is owned once given.
u16 sMaxLifeBefore = 0;

void noteBefore(uint8_t want, const char* who) {
    sMaxLifeBefore = dComIfGs_getMaxLife();
    if (want != dItemNo_HEART_PIECE_e) {
        char what[96];
        snprintf(what, sizeof what, "%s: checkItemGet(0x%02X) is 0 before", who, (unsigned)want);
        randoSmokeCheck(kName, what, checkItemGet(want, 0) == 0);
    }
}

void checkGiven(uint8_t want, const char* who) {
    char what[128];
    if (want == dItemNo_HEART_PIECE_e) {
        snprintf(what, sizeof what, "%s: max life %u -> %u (a Piece of Heart)", who, (unsigned)sMaxLifeBefore,
                 (unsigned)dComIfGs_getMaxLife());
        randoSmokeCheck(kName, what, dComIfGs_getMaxLife() == sMaxLifeBefore + 1);
    } else {
        snprintf(what, sizeof what, "%s: checkItemGet(0x%02X) is 1 after", who, (unsigned)want);
        randoSmokeCheck(kName, what, checkItemGet(want, 0) != 0);
    }
}

bool itemEvent(uint8_t want, const char* who) {
    ++sSince;
    if (dComIfGp_event_runCheck()) {
        if (!sSawEvent) {
            sSawEvent = true;
            writef(STDERR_FILENO, "[cos] rando-smoke scob: %s: event started, GtItm 0x%02X\n", who,
                   (unsigned)dComIfGp_event_getGtItm());
        }
        sEventFrames++;
        setDrivenPad(true, sEventFrames % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (sEventFrames > kEventFrames) {
            randoSmokeCheck(kName, "the item-get event ends", false);
            randoSmokeEnd(kName);
        }
        return false;
    }
    setDrivenPad(true, 0, 0, 0);
    if (!sSawEvent) {
        if (sSince > 600) {
            char what[96];
            snprintf(what, sizeof what, "%s: an item-get event started", who);
            randoSmokeCheck(kName, what, false);
            randoSmokeEnd(kName);
        }
        return false;
    }
    char what[128];
    snprintf(what, sizeof what, "%s: the event gave 0x%02X (GtItm 0x%02X)", who, (unsigned)want,
             (unsigned)dComIfGp_event_getGtItm());
    randoSmokeCheck(kName, what, dComIfGp_event_getGtItm() == want);
    checkGiven(want, who);
    return true;
}

// --- salvage ---------------------------------------------------------------------------------------
int sSlvg = -1;

void slvgStart() {
    const uint8_t want = expected(PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE, 0x64);
    dSalvage_control_c* c = daSalvage_c::mTagData_p;
    sSlvg = registryFind(44, want);
    if (c == nullptr || sSlvg < 0) {
        randoSmokeCheck(kName, "salvage: Outset's sunken treasure in the registry", false);
        randoSmokeEnd(kName);
    }
    const u8 chart = c->getSaveNo(sSlvg);
    if (sSince == 0) {
        writef(STDERR_FILENO, "[cos] rando-smoke scob: salvage: entry %d chart %u kind %u item 0x%02X used %d\n",
               sSlvg, (unsigned)chart, (unsigned)c->getKind(sSlvg), (unsigned)c->getItemNo(sSlvg),
               (int)c->checkUsed(sSlvg));
        dComIfGs_onOpenCollectMap(chart); // opened, not deciphered
        dComIfGs_offCollectMapTriforce(chart);
    }
    ++sSince;
    if (sSince == 30) {
        // proc_wait ran with the chart open: by item (vanilla rule) a Piece of Heart is no Triforce
        // Shard, so the point would be used; by chart (the rando rule) it waits for the decipher.
        const bool used = c->checkUsed(sSlvg) != 0;
        const bool vanillaRule = !isTriforce(want);
        randoSmokeCheck(kName, sVanilla ? "salvage: vanilla, chart opened: used by the item rule"
                                        : "salvage: chart opened, not deciphered: not used (by the chart index)",
                        sVanilla ? used == vanillaRule : !used);
        dComIfGs_onCollectMapTriforce(chart);
    }
    if (sSince == 60) {
        randoSmokeCheck(kName, "salvage: chart deciphered: the point is used", c->checkUsed(sSlvg) != 0);
        // the hook on X, the boat still over the point
        dComIfGs_setItem(dInvSlot_GRAPPLING_HOOK_e, dItemNo_GRAPPLING_HOOK_e);
        dComIfGs_setSelectItem(dItemBtn_X_e, dInvSlot_GRAPPLING_HOOK_e);
        dComIfGp_setSelectItem(dItemBtn_X_e);
        noteBefore(want, "salvage");
        sSince = 0;
        sState = kSlvgCrane;
    }
}

void slvgCrane() {
    dSalvage_control_c* c = daSalvage_c::mTagData_p;
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship == nullptr) {
        randoSmokeCheck(kName, "salvage: the boat", false);
        randoSmokeEnd(kName);
    }
    ++sSince;
    // put the crane top over the point: the boat is moved by the difference twice while the arm is
    // out and still (frames 80 and 100), then left alone while X is held to lower the hook
    const cXyz pt = c->getPos(sSlvg);
    if (sSince == 80 || sSince == 100) {
        if (cXyz* top = ship->getCraneTop()) {
            const f32 dx = pt.x - top->x, dz = pt.z - top->z;
            ship->current.pos.x += dx;
            ship->current.pos.z += dz;
            ship->old.pos = ship->current.pos;
        }
    }
    ship->speedF = 0.0f;
    // X once to take the crane out, then X held to lower it
    uint16_t btn = 0;
    if (sSince >= 10 && sSince < 14) {
        btn = PAD_BUTTON_X;
    } else if (sSince >= 110) {
        btn = PAD_BUTTON_X;
    }
    setDrivenPad(true, btn, 0, 0);
    if (sSince % 60 == 0) {
        cXyz* top = ship->getCraneTop();
        writef(STDERR_FILENO,
               "[cos] rando-smoke scob: salvage: frame %u crane %d top (%.0f, %.0f, %.0f) point (%.0f, %.0f, %.0f) "
               "event %d\n",
               sSince, (int)ship->checkCraneMode(), top ? top->x : 0.0f, top ? top->y : 0.0f, top ? top->z : 0.0f,
               pt.x, pt.y, pt.z, (int)dComIfGp_event_runCheck());
    }
    if (dComIfGp_event_runCheck()) {
        dComIfGp_event_setGtItm(dItemNo_NONE_e);
        randoSmokeCheck(kName, "salvage: the crane started the salvage event", true);
        sSince = 0;
        sEventFrames = 0;
        sSawEvent = false;
        sState = kSlvgEvent;
        return;
    }
    if (sSince > 900) {
        randoSmokeCheck(kName, "salvage: the crane started the salvage event", false);
        randoSmokeEnd(kName);
    }
}

} // namespace

void randoSmokeScob(unsigned int frames) {
    (void)frames;
    if (sState == kDone) {
        return;
    }
    if (sState == kOff) {
        if (!randoSmokeInPlay(10)) {
            return;
        }
        const char* m = getenv("COS_SCOB_MODE");
        sMode = m == nullptr || strcmp(m, "t1") == 0 ? kT1 : strcmp(m, "dig") == 0 ? kDig : kSalvage;
        const char* v = getenv("COS_SCOB_VANILLA");
        sVanilla = v != nullptr && strcmp(v, "1") == 0;
        parseRooms(getenv("COS_SCOB_ROOMS"));
        if (sMode != kT1) {
            sRooms.assign(1, 44);
        }
        if (!sVanilla && !pc_rando()) {
            buildPlacement();
            pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
            pc_rando_set_active(nullptr, nullptr);
        }
        randoSmokeCheck(kName, sVanilla ? "pc_rando() off" : "pc_rando() on", pc_rando() == (sVanilla ? 0 : 1));
        randoSmokeCheck(kName, "204 SCOB keys", pc_rando_place_scob_key_count() == 204);
        pc_rando_place_scob_reset();
        sRoomIdx = 0;
        sState = kWarp;
    }
    switch (sState) {
    case kWarp:
        if (sRoomIdx >= sRooms.size()) {
            sState = kDone;
            setDrivenPad(false, 0, 0, 0);
            randoSmokeEnd(kName);
        }
        warp(sRooms[sRoomIdx], sMode == kDig ? kOutsetLand : kSeaPoint);
        return;
    case kArrive:
        if (!arrived()) {
            if (++sSince > kArriveFrames) {
                char what[64];
                snprintf(what, sizeof what, "sea room %d point %d came up", sRoom, sPoint);
                randoSmokeCheck(kName, what, false);
                randoSmokeEnd(kName);
            }
            return;
        }
        sSince = 0;
        sState = kSettle;
        return;
    case kSettle:
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, ++sEventFrames % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (++sSince < kRoomSettle) {
            return;
        }
        checkRoom(sRoom);
        sSince = 0;
        if (sMode == kDig) {
            sState = kDigStart;
        } else if (sMode == kSalvage) {
            sState = kSlvgStart;
        } else {
            sRoomIdx++;
            sState = kWarp;
        }
        return;
    case kDigStart:
        digStart();
        return;
    case kDigWait:
        digWait();
        return;
    case kDigEvent:
        if (!sSawEvent) {
            uint8_t want = expected(PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL, 0x07);
            daItem_c* item = (daItem_c*)find(fpcNm_ITEM_e, -1, itemIs, &want);
            fopAc_ac_c* p = dComIfGp_getPlayer(0);
            if (item != nullptr && sSince % 120 == 0) {
                writef(STDERR_FILENO, "[cos] rando-smoke scob: dig: frame %u item (%.0f, %.0f, %.0f) player (%.0f, %.0f, %.0f)\n",
                       sSince, item->current.pos.x, item->current.pos.y, item->current.pos.z, p->current.pos.x,
                       p->current.pos.y, p->current.pos.z);
            }
            // the player is not moved by writes to its position here; move the item onto him instead
            if (item != nullptr && sSince % 20 == 0) {
                item->current.pos = p->current.pos;
                item->old.pos = p->current.pos;
            }
        }
        if (itemEvent(expected(PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL, 0x07), "dig")) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        return;
    case kSlvgStart:
        slvgStart();
        return;
    case kSlvgCrane:
        slvgCrane();
        return;
    case kSlvgEvent:
        if (itemEvent(expected(PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE, 0x64), "salvage")) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        return;
    default:
        return;
    }
}

} // namespace pc
