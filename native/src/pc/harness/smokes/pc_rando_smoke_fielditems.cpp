// COS_SMOKE=rando COS_RANDO_SMOKE=fielditems (docs/RANDOMIZER_PLAN.md step R2.11): "any item anywhere"
// in a booted game, e.g.
//   run.sh rando --stage Asoko:0:0:2 --env COS_RANDO_SMOKE=fielditems [--env COS_FIELDITEMS=0x20-0x30,0xAA]
// Once the play scene has run 10 frames, the smoke turns pc_rando() on (pc_rando_set_active, no seed)
// unless it already is (COS_RANDO_FORCE=1), then:
//   1. static: for every id of wwrando's items_without_field_models.txt, field_item_res holds the arc,
//      model block and mUnknown of its source's item_resource (pc_rando_field_item_source) and the
//      fixed get models (swords, songs, magic, Hurricane Spin) hold their source's; every item_info
//      height and radius is non-zero;
//   2. once the player is free (A through any opening talk), for each id of COS_FIELDITEMS (default:
//      every listed id except the 16 unused, unnamed ones, pc_rando_field_item_unnamed, whose
//      resources lack the models their entries name):
//      - field (COS_FIELDITEMS_MODE=field|chest|both, default both): the item is created with
//        fopAcM_createItem on the player (action 2: it pops up and falls back on him); it must be a
//        field item of that id with getYOffset 23.0, and touching it must start the item-get demo
//        (DEFAULT_GETITEM, dComIfGp_event_getGtItm() == id) within kStartFrames; A through the
//        messages; the event must end within kEventSeconds of wall time;
//      - chest: a big chest (takara3 0xFF200200, chest flag 4, as COS_SMOKE=item-sweep) with the id is
//        created in front of the player, who walks at it and presses A; same checks on its
//        DEFAULT_TREASURE event;
//   3. after the last id, ends: PASS (exit 0) when every check held, else FAIL (1).
// <COS_RUN_DIR>/fielditems.txt: "<id> <mode> begin" before each item (it names the item when a fault
// ends the process) and one result line per item: "<id> <mode> ok|no-event|wrong-item|refused|stuck ...".
// Shots are off (COS_FIELDITEMS_SHOTS=1: one shot per item, 30 frames after its message opened).
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_field_items.h"
#include "features/rando/pc_rando_items.h"

#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_item.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <dolphin/pad.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "fielditems";
constexpr unsigned int kSettleFrames = 60;
constexpr unsigned int kStartFrames = 300;
constexpr double kEventSeconds = 60.0; // wall time: the fanfare plays in real time
constexpr unsigned int kTalkFrames = 45;
constexpr unsigned int kGapFrames = 30;
constexpr unsigned int kDeleteFrames = 120;
constexpr float kChestSpawn = 150.0f;
constexpr u32 kChestParams = 0xFF200200; // shape 2 (big chest), chest flag 4, function 0
constexpr int kChestFlag = 4;
constexpr unsigned int kShotDelay = 30;

enum Mode { kField, kChest };
struct Job {
    u8 id;
    Mode mode;
};

enum State { kOff, kWaitFree, kNext, kPlace, kStart, kEvent, kDelete, kGap, kDone };
State sState = kOff;
std::vector<Job> sJobs;
size_t sJob = 0;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
uint64_t sEventStartNs = 0;
fpc_ProcID sProcId = fpcM_ERROR_PROCESS_ID_e;
cXyz sChestPos;
s16 sChestYaw = 0;
int sRoom = 0;
bool sHaveSpot = false;
bool sSawItem = false;
bool sShots = false;
unsigned int sMesgFrame = 0;
int sFd = -1;
unsigned int sOk = 0, sFailed = 0, sSkipped = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void* judgeById(void* proc, void* id) {
    return fpcM_GetID(proc) == *(fpc_ProcID*)id ? proc : nullptr;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

const char* modeName(Mode m) {
    return m == kField ? "field" : "chest";
}

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (sFd >= 0) {
        writef(sFd, "%s\n", buf);
    }
    writef(STDERR_FILENO, "[cos] rando-smoke %s: %s\n", kName, buf);
}

bool hasArc(u8 id) {
    const char* arc = dItem_data::getArcname(id);
    return arc != nullptr && arc[0] != '\0';
}

// COS_FIELDITEMS: a comma list of <first>[-<last>] (decimal or 0x hex); only listed ids are used.
bool parseWanted(bool wanted[256]) {
    const char* v = getenv("COS_FIELDITEMS");
    for (int i = 0; i < 256; i++) {
        wanted[i] = v == nullptr || *v == '\0';
    }
    if (v == nullptr || *v == '\0') {
        return true;
    }
    const char* p = v;
    for (;;) {
        char* end = nullptr;
        long first = strtol(p, &end, 0);
        bool ok = end != p;
        long last = first;
        if (ok && *end == '-') {
            const char* q = end + 1;
            last = strtol(q, &end, 0);
            ok = end != q;
        }
        if (!ok || first < 0 || last < first || last > 0xFF || (*end != ',' && *end != '\0')) {
            writef(STDERR_FILENO, "[cos] rando-smoke %s: COS_FIELDITEMS=\"%s\" is not a list of <first>[-<last>]\n",
                   kName, v);
            return false;
        }
        for (long i = first; i <= last; i++) {
            wanted[i] = true;
        }
        if (*end == '\0') {
            return true;
        }
        p = end + 1;
    }
}

void staticChecks() {
    int count = 0;
    const uint8_t* ids = pc_rando_field_item_ids(&count);
    int badField = 0, badGet = 0;
    for (int i = 0; i < count; i++) {
        int fix = 0;
        const u8 id = ids[i];
        const int src = pc_rando_field_item_source(id, &fix);
        const dItem_data_field_item_res& f = dItem_data::field_item_res[id];
        const dItem_data_item_resource& s = dItem_data::item_resource[src];
        const bool fieldOk = f.mArc == s.mArcname && f.mBmdIdx == s.mBmdIdx && f.mSrtIdx == s.mSrtIdx &&
                             f.mSrtIdx2 == s.mSrtIdx2 && f.mTevIdx == s.mTevIdx && f.mTevIdx2 == s.mTevIdx2 &&
                             f.mBckIdx == s.mBckIdx && f.mTevFrm == s.mTevFrm && f.mUnknown == s.mUnknown;
        if (!fieldOk) {
            badField++;
            writef(STDERR_FILENO, "[cos] rando-smoke %s: field_item_res[0x%02X] is not item_resource[0x%02X]'s model\n",
                   kName, id, src);
        }
        if (fix) {
            const dItem_data_item_resource& r = dItem_data::item_resource[id];
            if (r.mArcname != s.mArcname || r.mBmdIdx != s.mBmdIdx || r.mUnknown != s.mUnknown) {
                badGet++;
                writef(STDERR_FILENO, "[cos] rando-smoke %s: item_resource[0x%02X] is not 0x%02X's get model\n", kName,
                       id, src);
            }
        }
    }
    int zeroHr = 0;
    for (int id = 0; id < 0x100; id++) {
        zeroHr += dItem_data::getH((u8)id) == 0 || dItem_data::getR((u8)id) == 0;
    }
    randoSmokeCheck(kName, "171 field models copied from their item_resource source", count == 171 && badField == 0);
    randoSmokeCheck(kName, "get models of swords, songs, magic and Hurricane Spin fixed", badGet == 0);
    randoSmokeCheck(kName, "every item_info height and radius non-zero", zeroHr == 0);
    randoSmokeCheck(kName, "song arc is the Wind Waker's",
                    dItem_data::getFieldArc(0x6D) == dItem_data::getArcname(0x22) &&
                        dItem_data::getArcname(0x6D) == dItem_data::getArcname(0x22));
}

bool buildJobs() {
    bool wanted[256];
    if (!parseWanted(wanted)) {
        return false;
    }
    const char* m = getenv("COS_FIELDITEMS_MODE");
    const bool field = m == nullptr || *m == '\0' || strcmp(m, "both") == 0 || strcmp(m, "field") == 0;
    const bool chest = m == nullptr || *m == '\0' || strcmp(m, "both") == 0 || strcmp(m, "chest") == 0;
    if (!field && !chest) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s: COS_FIELDITEMS_MODE=\"%s\" is not field, chest or both\n", kName,
               m);
        return false;
    }
    int count = 0;
    const uint8_t* ids = pc_rando_field_item_ids(&count);
    for (int i = 0; i < count; i++) {
        const u8 id = ids[i];
        if (!wanted[id]) {
            continue;
        }
        if (pc_rando_field_item_unnamed(id) || !hasArc(id) || dItem_data::getFieldArc(id) == nullptr) {
            sSkipped++;
            continue;
        }
        if (field) {
            sJobs.push_back({id, kField});
        }
        if (chest) {
            sJobs.push_back({id, kChest});
        }
    }
    return true;
}

void clearChestFlag() {
    dSv_memBit_c& bit = g_dComIfG_gameInfo.save.getMemory().getBit();
    bit.mTbox = (u32)bit.mTbox & ~(1u << kChestFlag);
}

void* findProc() {
    if (sProcId == fpcM_ERROR_PROCESS_ID_e) {
        return nullptr;
    }
    fpc_ProcID id = sProcId;
    return fpcM_Search(judgeById, &id);
}

bool procGone() {
    if (sProcId == fpcM_ERROR_PROCESS_ID_e) {
        return true;
    }
    fpc_ProcID id = sProcId;
    return fpcM_Search(judgeById, &id) == nullptr && !fpcM_IsCreating(id);
}

void fail(const Job& j, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void fail(const Job& j, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sFailed++;
    line("%02X %s %s", j.id, modeName(j.mode), buf);
}

void finish() {
    setDrivenPad(true, 0, 0, 0);
    sState = kDone;
    line("# done: %u ok, %u failed, %u skipped (unused)", sOk, sFailed, sSkipped);
    if (sFd >= 0) {
        close(sFd);
        sFd = -1;
    }
    randoSmokeCheck(kName, "every item gave its get demo as a field item and from a chest", sFailed == 0);
    randoSmokeEnd(kName);
}

void spawn(const Job& j) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!sHaveSpot) {
        const s16 yaw = player->shape_angle.y;
        sChestPos.set(player->current.pos.x + kChestSpawn * cM_ssin(yaw), player->current.pos.y,
                      player->current.pos.z + kChestSpawn * cM_scos(yaw));
        sChestYaw = (s16)(yaw + 0x8000);
        sRoom = fopAcM_GetRoomNo(player);
        sHaveSpot = true;
    }
    if (sFd >= 0) {
        writef(sFd, "%02X %s begin\n", j.id, modeName(j.mode));
    }
    dComIfGp_event_setGtItm(dItemNo_NONE_e);
    sSawItem = false;
    sMesgFrame = 0;
    sEventFrames = 0;
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    if (j.mode == kField) {
        cXyz pos = player->current.pos;
        csXyz angle = csXyz::Zero;
        sProcId = fopAcM_createItem(&pos, j.id, -1, fopAcM_GetRoomNo(player), 0, &angle, 2);
    } else {
        clearChestFlag();
        csXyz angle((s16)(sRoom & 0x3F), sChestYaw, (s16)((j.id << 8) | 0xFF));
        cXyz pos = sChestPos;
        sProcId = fopAcM_create(fpcNm_TBOX_e, kChestParams, &pos, sRoom, &angle);
    }
    fpcLy_SetCurrentLayer(saved);
    sSince = 0;
    if (sProcId == fpcM_ERROR_PROCESS_ID_e) {
        fail(j, "refused");
        sState = kGap;
        return;
    }
    sState = kPlace;
}

} // namespace

void randoSmokeFielditems(unsigned int frames) {
    if (!randoSmokeInPlay(10) || sState == kDone) {
        return;
    }
    if (sState == kOff) {
        if (!pc_rando()) {
            pc_rando_set_active(nullptr, nullptr);
        }
        randoSmokeCheck(kName, "pc_rando() on, item tables edited", pc_rando() == 1 && pc_rando_tables_applied() == 1);
        staticChecks();
        if (!buildJobs()) {
            randoSmokeCheck(kName, "options", false);
            randoSmokeEnd(kName);
        }
        const char* shots = getenv("COS_FIELDITEMS_SHOTS");
        sShots = shots != nullptr && strcmp(shots, "1") == 0;
        sFd = openRunFile("fielditems.txt");
        if (sFd >= 0) {
            writef(sFd, "# fielditems (COS_RANDO_SMOKE=fielditems): <id> <mode> <result> ...\n");
        }
        line("%zu job(s), %u id(s) skipped (unused)", sJobs.size(), sSkipped);
        setDrivenPad(true, 0, 0, 0);
        sState = kWaitFree;
        sSince = 0;
    }
    if (!outsetLinkReady() || dComIfGp_getPlayer(0) == nullptr) {
        sSince = 0;
        return;
    }
    ++sSince;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const Job* job = sJob < sJobs.size() ? &sJobs[sJob] : nullptr;

    switch (sState) {
    case kOff:
    case kDone:
        return;
    case kWaitFree:
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= kSettleFrames) {
            sState = kNext;
            sSince = 0;
        }
        return;
    case kNext:
        if (job == nullptr) {
            finish();
            return;
        }
        spawn(*job);
        return;
    case kPlace:
        setDrivenPad(true, 0, 0, 0);
        if (job->mode == kField && !sSawItem) {
            if (void* p = findProc()) {
                daItem_c* item = (daItem_c*)p;
                sSawItem = true;
                if (fopAcM_GetName(item) != fpcNm_ITEM_e || item->getItemNo() != job->id) {
                    fail(*job, "wrong-item name=%d no=0x%02X", (int)fopAcM_GetName(item),
                         (unsigned)item->getItemNo());
                } else if (item->getYOffset() != PC_RANDO_FIELD_ITEM_Y_OFFSET) {
                    fail(*job, "y-offset %.1f", item->getYOffset());
                }
            }
        }
        if (sSince >= 10) {
            sState = kStart;
            sSince = 0;
        }
        return;
    case kStart: {
        if (dComIfGp_event_runCheck()) {
            sState = kEvent;
            sSince = 0;
            sEventStartNs = monotonicNs();
            setDrivenPad(true, 0, 0, 0);
            return;
        }
        if (job->mode == kChest) {
            if (dComIfGp_getDoStatus() == dActStts_OPEN_e) {
                setDrivenPad(true, sSince % 15 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            } else {
                const s16 a = cM_atan2s(sChestPos.x - player->current.pos.x, sChestPos.z - player->current.pos.z);
                const s16 st = (s16)(a - 0x8000 - dCam_getControledAngleY((camera_class*)dComIfGp_getCamera(0)));
                setDrivenPad(true, 0, (int8_t)(cM_ssin(st) * 60.0f), (int8_t)(-cM_scos(st) * 60.0f));
            }
        } else {
            setDrivenPad(true, 0, 0, 0);
        }
        if (sSince >= kStartFrames) {
            fail(*job, "no-event %u player=(%.0f,%.0f,%.0f)", sSince, player->current.pos.x, player->current.pos.y,
                 player->current.pos.z);
            setDrivenPad(true, 0, 0, 0);
            if (void* p = findProc()) {
                fpcM_Delete(p);
            }
            sState = kDelete;
            sSince = 0;
        }
        return;
    }
    case kEvent: {
        setDrivenPad(true, sSince % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
        const double secs = (monotonicNs() - sEventStartNs) / 1e9;
        if (sMesgFrame == 0 && dComIfGp_getMesgStatus() != 0) {
            sMesgFrame = sSince;
        }
        if (sShots && sMesgFrame != 0 && sSince == sMesgFrame + kShotDelay) {
            captureFrame(frames, shotSink, nullptr);
        }
        if (!dComIfGp_event_runCheck()) {
            const u8 gt = dComIfGp_event_getGtItm();
            if (gt != job->id) {
                fail(*job, "wrong-demo-item 0x%02X", (unsigned)gt);
            } else if (sMesgFrame == 0) {
                fail(*job, "no-message");
            } else {
                sOk++;
                line("%02X %s ok %u %.1f got=%d", job->id, modeName(job->mode), sSince, secs,
                     (int)dComIfGs_checkGetItem(job->id));
            }
            setDrivenPad(true, 0, 0, 0);
            if (job->mode == kChest) {
                if (void* p = findProc()) {
                    fpcM_Delete(p);
                }
            }
            sState = kDelete;
            sSince = 0;
        } else if (secs >= kEventSeconds) {
            fail(*job, "stuck %u %.1f mesg=%d", sSince, secs, (int)dComIfGp_getMesgStatus());
            eventWatchReport(frames);
            finish();
        }
        return;
    }
    case kDelete:
        // A field item deletes itself at the end of its demo (execMainGetDemoDirection); a chest is
        // deleted by the smoke.
        if (procGone()) {
            sProcId = fpcM_ERROR_PROCESS_ID_e;
            sState = kGap;
            sSince = 0;
        } else if (sSince >= kDeleteFrames) {
            fail(*job, "not-deleted");
            sProcId = fpcM_ERROR_PROCESS_ID_e;
            sState = kGap;
            sSince = 0;
        } else if ((sSince & 15) == 0) {
            if (void* p = findProc()) {
                fpcM_Delete(p);
            }
        }
        return;
    case kGap:
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            sSince = 0;
            setDrivenPad(true, sEventFrames % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sEventFrames > 30 * 60) {
                fail(*job, "stuck-after");
                eventWatchReport(frames);
                finish();
            }
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince < kGapFrames) {
            return;
        }
        sJob++;
        sEventFrames = 0;
        sState = kNext;
        sSince = 0;
        return;
    }
}

} // namespace pc
