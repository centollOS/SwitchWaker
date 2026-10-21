// COS_SMOKE=rando COS_RANDO_SMOKE=actr (docs/RANDOMIZER_PLAN.md step R2.2): field items, pots and
// stone heads (features/rando/pc_rando_place_actr.h), e.g.
//   run.sh rando --stage M_NewD2:6:9 --env COS_RANDO_SMOKE=actr \
//                --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_rotation.json [--env COS_ACTR_T2=pot]
// Once the play scene has run 30 frames:
//   - pc_rando() 0 (no placement): the patcher recorded nothing, and every stone head of the room keeps
//     the profile's fopAcStts_NOCULLEXEC_e (the vanilla behaviour fix_stone_head_bugs removes);
//   - pc_rando() 1, T1: every record of the boot stage (pc_rando_place_actr_record) is OK; each
//     location record holds pc_rando_loc_item in its item field; for the records of the boot room
//     (dComIfGp_roomControl_getStayNo) a live actor of the entry's class stands at the entry's position
//     and carries the value as its hunk reads it: d_a_item params & 0xFF (the placed item or, for a
//     progressive item, the tier R3.3 converts it to), d_a_tsubo mPrmZ >> 8, d_a_obj_homen
//     home.angle.x & 0xFF with fopAcStts_NOCULLEXEC_e off; the tweak records: the Deku Leaf entry is a
//     d_a_item, the pickup flags are 0x16, the stone heads' destroyed switch 0xFF. At least one
//     record of the boot room must exist (so a wrong --stage fails);
//   - T2 (COS_ACTR_T2=item|pot|homen): the first location record of that class in the boot room gives
//     its item: a field item is moved onto the player; a pot is broken (daTsubo::Act_c::damaged, the
//     hunk) and a stone head makes its item (daObjHomen::Act_c::make_item, the hunk), and the field
//     item they create (the placed item, or its tier) is moved onto the player. The item-get event must
//     start within kStartFrames with dComIfGp_event_getGtItm() equal to the field item's id; A through
//     its messages until it ends; then the save must differ from before
//     (its item_func ran; the clock and the pickup flags are left out).
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_lockeys.h"
#include "features/rando/pc_rando_place_actr.h"

#include "d/actor/d_a_item.h"
#include "d/actor/d_a_obj_homen.h"
#include "d/actor/d_a_tsubo.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "actr";
constexpr unsigned int kStartFrames = 300;
constexpr unsigned int kEventFrames = 3000;

enum State { kInit, kT2Wait, kT2Spawn, kT2Start, kT2Event, kDone };
State sState = kInit;
unsigned int sSince = 0;
const PcRandoActrRecord* sTarget = nullptr;
fpc_ProcID sItemId = fpcM_ERROR_PROCESS_ID_e;
int sItemNo = -1;
cXyz sSpawnPos;

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    writef(STDERR_FILENO, "[cos] rando-smoke actr: %s\n", buf);
}

bool nameIs(const uint8_t* e, const char* name) {
    char n[8] = {};
    strncpy(n, name, sizeof n);
    return memcmp(e, n, sizeof n) == 0;
}

int procOf(const uint8_t* e) {
    if (nameIs(e, "item")) {
        return fpcNm_ITEM_e;
    }
    if (nameIs(e, "Homen1") || nameIs(e, "Homen2")) {
        return fpcNm_Obj_Homen_e;
    }
    return fpcNm_TSUBO_e; // ootubo1, kotubo, Kmtub, ... (d_a_tsubo)
}

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
float bef32(const uint8_t* p) {
    const uint32_t v = be32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

struct Find {
    int proc;
    cXyz pos;
    float tol;
    fopAc_ac_c* found;
};

void* findAt(void* proc, void* data) {
    Find* f = static_cast<Find*>(data);
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(proc);
    if (fpcM_GetName(proc) == f->proc && std::fabs(a->home.pos.x - f->pos.x) < f->tol &&
        std::fabs(a->home.pos.y - f->pos.y) < f->tol && std::fabs(a->home.pos.z - f->pos.z) < f->tol) {
        f->found = a;
        return proc;
    }
    return nullptr;
}

fopAc_ac_c* actorAt(int proc, const cXyz& pos, float tol) {
    Find f = {proc, pos, tol, nullptr};
    fopAcM_Search(findAt, &f);
    return f.found;
}

cXyz entryPos(const PcRandoActrRecord* r) {
    return cXyz(bef32(r->entry + 0x0C), bef32(r->entry + 0x10), bef32(r->entry + 0x14));
}

int recordRoom(const PcRandoActrRecord* r) {
    return strncmp(r->arc, "Room", 4) == 0 ? atoi(r->arc + 4) : -1;
}

struct Homens {
    int n, culled;
};
void* countHomen(void* proc, void* data) {
    if (fpcM_GetName(proc) == fpcNm_Obj_Homen_e) {
        Homens* h = static_cast<Homens*>(data);
        h->n++;
        if (fopAcM_CheckStatus(static_cast<fopAc_ac_c*>(proc), fopAcStts_NOCULLEXEC_e)) {
            h->culled++;
        }
    }
    return nullptr;
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void* judgeById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}

// The newest field item near `pos` (a pot's or stone head's drop).
struct NearItem {
    cXyz pos;
    fopAc_ac_c* found;
};
void* findNearItem(void* proc, void* data) {
    NearItem* n = static_cast<NearItem*>(data);
    fopAc_ac_c* a = static_cast<fopAc_ac_c*>(proc);
    if (fpcM_GetName(proc) == fpcNm_ITEM_e && a->current.pos.abs(n->pos) < 400.0f) {
        n->found = a;
    }
    return nullptr;
}

// The save data an item_func writes: dSv_save_c without the clock (dSv_player_status_b_c), plus the
// current stage's key count and dungeon item bits (the pickup flags are left out).
struct SaveSnap {
    u8 save[sizeof(dSv_save_c)];
    u8 keyNum, dungeonItem;
};
SaveSnap sSnap;
void snapSave(SaveSnap& s) {
    dSv_save_c& sv = g_dComIfG_gameInfo.save.getSavedata();
    memcpy(s.save, &sv, sizeof s.save);
    const size_t clock = (size_t)((u8*)&sv.getPlayer().getPlayerStatusB() - (u8*)&sv);
    memset(s.save + clock, 0, sizeof(dSv_player_status_b_c));
    dSv_memBit_c& bit = g_dComIfG_gameInfo.save.getMemory().getBit();
    s.keyNum = bit.mKeyNum;
    s.dungeonItem = bit.mDungeonItem;
}

bool checkLive(const PcRandoActrRecord* r) {
    char what[160];
    const cXyz pos = entryPos(r);
    fopAc_ac_c* a = actorAt(procOf(r->entry), pos, 1.0f);
    snprintf(what, sizeof what, "%s/%s %s[%u] %.8s: live actor at (%.0f, %.0f, %.0f)", r->stage, r->arc, r->tag,
             r->index, (const char*)r->entry, pos.x, pos.y, pos.z);
    if (!randoSmokeCheck(kName, what, a != nullptr)) {
        return false;
    }
    const u32 prm = fopAcM_GetParam(a);
    int got = -1, want = -1;
    switch (r->kind) {
    case PCR_ACTR_LOCATION:
        want = r->item;
        if (fpcM_GetName(a) == fpcNm_ITEM_e) {
            got = prm & 0xFF;
            if (got != want && got == pc_rando_convert_progressive_item_id(want)) {
                want = got; // R3.3 turned the placed progressive item into its tier at create
            }
        } else if (fpcM_GetName(a) == fpcNm_TSUBO_e) {
            got = (static_cast<daTsubo::Act_c*>(a)->mPrmZ >> 8) & 0xFF;
        } else {
            got = (u16)a->home.angle.x & 0xFF;
            snprintf(what, sizeof what, "%s/%s %s[%u]: stone head executes off camera (fix_stone_head_bugs)", r->stage,
                     r->arc, r->tag, r->index);
            randoSmokeCheck(kName, what, !fopAcM_CheckStatus(a, fopAcStts_NOCULLEXEC_e));
        }
        break;
    case PCR_ACTR_DEKU:
        got = fpcM_GetName(a) == fpcNm_ITEM_e ? 1 : 0;
        want = 1;
        break;
    case PCR_ACTR_JOY_FLAG:
        got = fpcM_GetName(a) == fpcNm_ITEM_e ? (prm >> 8) & 0xFF : (prm >> 16) & 0x7F;
        want = 0x16;
        break;
    case PCR_ACTR_HOMEN_SWITCH:
        got = prm >> 24;
        want = 0xFF;
        break;
    }
    snprintf(what, sizeof what, "%s/%s %s[%u] kind %d: actor value 0x%X (want 0x%X)", r->stage, r->arc, r->tag,
             r->index, r->kind, got, want);
    return randoSmokeCheck(kName, what, got == want);
}

void t1() {
    const char* stage = dComIfGp_getStartStageName();
    const int stay = dComIfGp_roomControl_getStayNo();
    int inStay = 0;
    for (int i = 0; i < pc_rando_place_actr_count(); i++) {
        const PcRandoActrRecord* r = pc_rando_place_actr_record(i);
        if (strcmp(r->stage, stage) != 0) {
            continue;
        }
        char what[160];
        snprintf(what, sizeof what, "%s/%s %s[%u] kind %d loc %d: %s", r->stage, r->arc, r->tag, r->index, r->kind,
                 r->loc, pc_rando_dzx_result_name(r->result));
        randoSmokeCheck(kName, what, r->result == PCR_DZX_OK);
        if (r->kind == PCR_ACTR_LOCATION) {
            const pcr::LocKey& key = pcr::location_keys()[(size_t)r->key];
            const uint8_t placed = pc_rando_loc_item(r->loc, key.vanilla);
            snprintf(what, sizeof what, "%s/%s %s[%u]: record item 0x%02X (placed 0x%02X, vanilla 0x%02X)", r->stage,
                     r->arc, r->tag, r->index, pc_rando_place_actr_entry_item(r), placed, key.vanilla);
            randoSmokeCheck(kName, what, pc_rando_place_actr_entry_item(r) == placed && r->item == placed);
        }
        if (recordRoom(r) == stay && r->result == PCR_DZX_OK) {
            checkLive(r);
            inStay++;
        }
    }
    char what[96];
    snprintf(what, sizeof what, "%s room %d: %d record(s) checked live", stage, stay, inStay);
    randoSmokeCheck(kName, what, inStay > 0);
}

const PcRandoActrRecord* findTarget(const char* cls) {
    const int stay = dComIfGp_roomControl_getStayNo();
    const int want = strcmp(cls, "item") == 0 ? fpcNm_ITEM_e : strcmp(cls, "pot") == 0 ? fpcNm_TSUBO_e : fpcNm_Obj_Homen_e;
    for (int i = 0; i < pc_rando_place_actr_count(); i++) {
        const PcRandoActrRecord* r = pc_rando_place_actr_record(i);
        if (r->kind == PCR_ACTR_LOCATION && r->result == PCR_DZX_OK && recordRoom(r) == stay &&
            strcmp(r->stage, dComIfGp_getStartStageName()) == 0 && procOf(r->entry) == want) {
            return r;
        }
    }
    return nullptr;
}

void finish() {
    setDrivenPad(false, 0, 0, 0);
    sState = kDone;
    randoSmokeEnd(kName);
}

} // namespace

void randoSmokeActr(unsigned int frames) {
    (void)frames;
    if (sState == kDone || !randoSmokeInPlay(30)) {
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return;
    }
    sSince++;
    const char* t2 = getenv("COS_ACTR_T2");
    switch (sState) {
    case kInit: {
        line("stage %s room %d, pc_rando() %d, %d record(s)", dComIfGp_getStartStageName(),
             dComIfGp_roomControl_getStayNo(), pc_rando(), pc_rando_place_actr_count());
        if (!pc_rando()) {
            randoSmokeCheck(kName, "vanilla: no entry patched", pc_rando_place_actr_count() == 0);
            Homens h = {0, 0};
            fopAcM_Search(countHomen, &h);
            line("vanilla: %d stone head(s), %d with NOCULLEXEC", h.n, h.culled);
            randoSmokeCheck(kName, "vanilla: stone heads keep fopAcStts_NOCULLEXEC_e", h.culled == h.n);
            finish();
            return;
        }
        t1();
        if (t2 == nullptr || t2[0] == '\0') {
            finish();
            return;
        }
        sTarget = findTarget(t2);
        if (!randoSmokeCheck(kName, "T2 target in the boot room", sTarget != nullptr)) {
            finish();
            return;
        }
        line("T2 %s: %s/%s %s[%u] loc %d item 0x%02X", t2, sTarget->stage, sTarget->arc, sTarget->tag,
             sTarget->index, sTarget->loc, sTarget->item);
        sState = kT2Wait;
        sSince = 0;
        setDrivenPad(true, 0, 0, 0);
        return;
    }
    case kT2Wait:
        // any opening event first (A through it), then the player stands for a moment
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= 30) {
            sState = kT2Spawn;
            sSince = 0;
        }
        return;
    case kT2Spawn: {
        const cXyz pos = entryPos(sTarget);
        fopAc_ac_c* a = actorAt(procOf(sTarget->entry), pos, 1.0f);
        if (!randoSmokeCheck(kName, "T2 actor still there", a != nullptr)) {
            finish();
            return;
        }
        dComIfGp_event_setGtItm(dItemNo_NONE_e);
        snapSave(sSnap);
        if (fpcM_GetName(a) == fpcNm_ITEM_e) {
            sItemId = fopAcM_GetID(a);
        } else {
            layer_class* saved = fpcLy_CurrentLayer();
            fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
            sSpawnPos = a->current.pos;
            if (fpcM_GetName(a) == fpcNm_TSUBO_e) {
                static_cast<daTsubo::Act_c*>(a)->damaged(0, nullptr, false, nullptr);
            } else {
                static_cast<daObjHomen::Act_c*>(a)->make_item();
            }
            fpcLy_SetCurrentLayer(saved);
            sItemId = fpcM_ERROR_PROCESS_ID_e;
        }
        sState = kT2Start;
        sSince = 0;
        return;
    }
    case kT2Start: {
        setDrivenPad(true, 0, 0, 0);
        if (dComIfGp_event_runCheck()) {
            line("T2: event started after %u frame(s), item 0x%02X", sSince, (unsigned)dComIfGp_event_getGtItm());
            sState = kT2Event;
            sSince = 0;
            return;
        }
        daItem_c* item = nullptr;
        if (sItemId == fpcM_ERROR_PROCESS_ID_e) {
            NearItem n = {sSpawnPos, nullptr};
            fopAcM_Search(findNearItem, &n);
            if (n.found != nullptr) {
                sItemId = fopAcM_GetID(n.found);
            }
        }
        if (sItemId != fpcM_ERROR_PROCESS_ID_e) {
            item = (daItem_c*)fpcM_Search(judgeById, &sItemId);
        }
        if (item != nullptr) {
            if (sItemNo < 0) {
                sItemNo = item->getItemNo();
                line("T2: field item 0x%02X (placed 0x%02X)", sItemNo, sTarget->item);
                randoSmokeCheck(kName, "T2: the field item is the placed item (or its tier)",
                                sItemNo == sTarget->item || sItemNo == pc_rando_convert_progressive_item_id(sTarget->item));
            }
            if (sSince >= 10) {
                item->current.pos = player->current.pos;
                item->old.pos = player->current.pos;
                item->speedF = 0.0f;
                item->speed.setall(0.0f);
            }
        }
        if (sSince >= kStartFrames) {
            line("T2: no event (item %s) player (%.0f, %.0f, %.0f)", item != nullptr ? "found" : "missing",
                 player->current.pos.x, player->current.pos.y, player->current.pos.z);
            randoSmokeCheck(kName, "T2: item-get event started", false);
            finish();
            return;
        }
        return;
    }
    case kT2Event:
        setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (!dComIfGp_event_runCheck()) {
            const u8 gt = dComIfGp_event_getGtItm();
            char what[96];
            snprintf(what, sizeof what, "T2: got item 0x%02X (field item 0x%02X)", (unsigned)gt, sItemNo);
            randoSmokeCheck(kName, what, sItemNo >= 0 && gt == sItemNo);
            // the item's item_func ran: it changed the save (or the stage's keys / dungeon items)
            SaveSnap now;
            snapSave(now);
            snprintf(what, sizeof what, "T2: item 0x%02X changed the save (item_func ran)", (unsigned)gt);
            randoSmokeCheck(kName, what, memcmp(&now, &sSnap, sizeof now) != 0);
            finish();
        }
        if (sSince >= kEventFrames) {
            randoSmokeCheck(kName, "T2: item-get event ended", false);
            finish();
        }
        return;
    case kDone:
        return;
    }
}

} // namespace pc
