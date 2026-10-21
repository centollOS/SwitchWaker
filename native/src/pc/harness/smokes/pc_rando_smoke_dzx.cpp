// COS_SMOKE=rando COS_RANDO_SMOKE=dzx (docs/RANDOMIZER_PLAN.md step R0.10): the archive-load hook
// (features/rando/pc_rando_resources.h) on Tetra's ship, run as
//   run.sh rando --stage Asoko:0:0:2 --env COS_RANDO_SMOKE=dzx [--env COS_RANDO_FORCE=1]
//                [--env COS_RANDO_DZX_MODE=identity|item]
// From the first frame, before the stage loads, the smoke registers two dzx patchers: a first one
// and a last one. For every dispatched stage.dzs / room.dzr of the stage Asoko the first one records
// an FNV-1a hash of the raw bytes and the last one the hash of what loadResource stores in mRes[].
// The chest is wwrando's "Tetra's Ship - ... Bombs" location: Asoko/Stage.arc/Layer2/Chest000, i.e.
// stage.dzs TRE2 entry 0, whose item is the byte at +0x1C (the high byte of angle z; vanilla 0x31).
//   - identity (the default; stands for the plan's COS_RANDO_PLACEMENT=identity, since slots and
//     placements come with R0.7): the first patcher writes the vanilla item back over itself
//     (pc_rando_dzx_set_u8 with the vanilla byte as fingerprint), so the stored bytes must hash as
//     the raw ones;
//   - item: it writes kNewItem there (fingerprint 0x31) and appends a copy of the entry to TRE2 with
//     kGrownItem, kGrownDx units along x (pc_rando_res_grow_dzx), so stage.dzs is replaced by a grown
//     copy on the archive heap.
// Once the play scene has run 10 frames:
//   1. with pc_rando() 0 (no COS_RANDO_FORCE): no file was dispatched, no patcher ran, and the stage's
//      chest (a TBOX actor) holds the vanilla item 0x31 in home.angle.z >> 8;
//   2. with pc_rando() 1: the patchers saw Asoko's stage.dzs (stage "Asoko", archive "Stage", room -1:
//      the directory came from dRes_info_c::set) and room.dzr (archive "Room0", room 0); the vanilla
//      byte was 0x31; identity: every stored hash equals its raw hash, no copy was made, and the
//      chest holds 0x31; item: the stored stage.dzs is a copy (hash differs, size grew), the chest
//      holds kNewItem and a second TBOX kGrownDx along x holds kGrownItem;
//   3. the game then runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_resources.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "dzx";
constexpr uint8_t kVanillaItem = 0x31; // Bombs (wwrando item_locations.txt, Asoko Layer2/Chest000)
constexpr uint8_t kNewItem = 0x04;     // Red Rupee
constexpr uint8_t kGrownItem = 0x05;   // Purple Rupee
constexpr float kGrownDx = 150.0f;
constexpr uint32_t kItemOff = 0x1C;

enum Mode { kIdentity, kItem };
Mode sMode = kIdentity;
bool sRegistered = false;
unsigned int sEndFrame = 0;

struct Seen {
    bool seen = false;
    int room = -2;
    bool stageOk = false;
    uint32_t rawHash = 0, finalHash = 0;
    size_t rawSize = 0, finalSize = 0;
    int replaced = 0;
};
Seen sStage, sRoom0;
int sVanillaByte = -1;
int sEditResult = -1, sGrowResult = -1;
float sChestX = 0.0f;
int sCalls = 0;

uint32_t fnv(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

Seen* seenFor(const PcRandoResFile* f) {
    if (strcmp(f->stage, "Asoko") != 0) {
        return nullptr;
    }
    if (strcmp(f->arc, "Stage") == 0 && strcmp(f->name, "stage.dzs") == 0) {
        return &sStage;
    }
    if (strcmp(f->arc, "Room0") == 0 && strcmp(f->name, "room.dzr") == 0) {
        return &sRoom0;
    }
    return nullptr;
}

void firstPatcher(PcRandoResFile* f) {
    sCalls++;
    Seen* s = seenFor(f);
    if (s == nullptr) {
        return;
    }
    s->seen = true;
    s->room = f->room;
    s->stageOk = strcmp(f->dir, "/res/Stage/Asoko/") == 0;
    s->rawHash = fnv(f->data, f->size);
    s->rawSize = f->size;
    if (s != &sStage) {
        return;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        return;
    }
    const uint8_t* e = pc_rando_dzx_entry(&dzx, "TRE2", 0);
    if (e == nullptr) {
        return;
    }
    sVanillaByte = e[kItemOff];
    sChestX = pc_rando_dzx_bef32(e + 0x0C);
    writef(STDERR_FILENO, "[cos] rando-smoke dzx: Asoko stage.dzs TRE2[0] %.8s item 0x%02X x %.1f\n",
           (const char*)e, sVanillaByte, sChestX);
    const uint8_t vanilla = kVanillaItem;
    PcRandoDzxFingerprint fp = {nullptr, (int32_t)kItemOff, &vanilla, 1};
    if (sMode == kIdentity) {
        sEditResult = pc_rando_dzx_set_u8(&dzx, "TRE2", 0, kItemOff, vanilla, &fp);
        return;
    }
    sEditResult = pc_rando_dzx_set_u8(&dzx, "TRE2", 0, kItemOff, kNewItem, &fp);
    uint8_t entry[0x20];
    memcpy(entry, e, sizeof entry);
    entry[kItemOff] = kGrownItem;
    const float x = sChestX + kGrownDx;
    uint32_t xb;
    memcpy(&xb, &x, 4);
    entry[0x0C] = (uint8_t)(xb >> 24);
    entry[0x0D] = (uint8_t)(xb >> 16);
    entry[0x0E] = (uint8_t)(xb >> 8);
    entry[0x0F] = (uint8_t)xb;
    PcRandoDzxAdd add = {"TRE2", entry, 1};
    uint32_t first = 0;
    sGrowResult = pc_rando_res_grow_dzx(f, &add, 1, &first);
    writef(STDERR_FILENO, "[cos] rando-smoke dzx: edit %s, grow %s (new index %u)\n",
           pc_rando_dzx_result_name(sEditResult), pc_rando_dzx_result_name(sGrowResult), first);
}

void lastPatcher(PcRandoResFile* f) {
    Seen* s = seenFor(f);
    if (s == nullptr) {
        return;
    }
    s->finalHash = fnv(f->data, f->size);
    s->finalSize = f->size;
    s->replaced = f->replaced;
    writef(STDERR_FILENO, "[cos] rando-smoke dzx: %s/%s/%s (dir %s, room %d) raw %08X/0x%zx final %08X/0x%zx%s\n",
           f->stage, f->arc, f->name, f->dir, f->room, s->rawHash, s->rawSize, s->finalHash, s->finalSize,
           f->replaced ? " (copy)" : "");
}

struct Chests {
    int n;
    fopAc_ac_c* list[16];
};

void* collectTbox(void* proc, void* data) {
    Chests* c = static_cast<Chests*>(data);
    if (fpcM_GetName(proc) == fpcNm_TBOX_e && c->n < 16) {
        c->list[c->n++] = static_cast<fopAc_ac_c*>(proc);
    }
    return nullptr;
}

int chestItem(const fopAc_ac_c* a) {
    return ((u16)a->home.angle.z >> 8) & 0xFF;
}

// The TBOX whose home x is nearest `x` (within 1 unit), or nullptr.
fopAc_ac_c* chestAt(const Chests& c, float x) {
    for (int i = 0; i < c.n; i++) {
        if (std::fabs(c.list[i]->home.pos.x - x) < 1.0f) {
            return c.list[i];
        }
    }
    return nullptr;
}

void checkSeen(const char* what, const Seen& s, int room) {
    char buf[96];
    snprintf(buf, sizeof buf, "%s dispatched (stage Asoko, dir /res/Stage/Asoko/, room %d)", what, room);
    randoSmokeCheck(kName, buf, s.seen && s.stageOk && s.room == room);
}

} // namespace

void randoSmokeDzx(unsigned int frames) {
    if (!sRegistered) {
        sRegistered = true;
        const char* mode = getenv("COS_RANDO_DZX_MODE");
        sMode = mode != nullptr && strcmp(mode, "item") == 0 ? kItem : kIdentity;
        pc_rando_register_dzx_patcher(firstPatcher);
        pc_rando_register_dzx_patcher(lastPatcher);
        writef(STDERR_FILENO, "[cos] rando-smoke dzx: mode %s, pc_rando() %d\n",
               sMode == kItem ? "item" : "identity", pc_rando());
    }
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    sEndFrame = frames + 30;

    Chests c = {};
    fopAcM_Search(collectTbox, &c);
    for (int i = 0; i < c.n; i++) {
        writef(STDERR_FILENO, "[cos] rando-smoke dzx: TBOX %d at x %.1f item 0x%02X\n", i,
               c.list[i]->home.pos.x, chestItem(c.list[i]));
    }
    PcRandoResStats st;
    pc_rando_res_stats(&st);

    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: no file dispatched", st.dispatched == 0);
        randoSmokeCheck(kName, "vanilla: no patcher ran", sCalls == 0);
        bool vanilla = false;
        for (int i = 0; i < c.n; i++) {
            vanilla = vanilla || chestItem(c.list[i]) == kVanillaItem;
        }
        randoSmokeCheck(kName, "vanilla: the chest holds 0x31", vanilla);
        return;
    }

    checkSeen("stage.dzs", sStage, -1);
    checkSeen("room.dzr", sRoom0, 0);
    randoSmokeCheck(kName, "TRE2[0] vanilla byte is 0x31", sVanillaByte == kVanillaItem);
    randoSmokeCheck(kName, "TRE2[0] edit ok", sEditResult == PCR_DZX_OK);
    fopAc_ac_c* chest = chestAt(c, sChestX);
    randoSmokeCheck(kName, "the TRE2[0] chest exists", chest != nullptr);
    if (sMode == kIdentity) {
        randoSmokeCheck(kName, "identity: stage.dzs stored bytes hash as the raw ones",
                        sStage.rawHash == sStage.finalHash && sStage.rawSize == sStage.finalSize &&
                            !sStage.replaced);
        randoSmokeCheck(kName, "identity: room.dzr stored bytes hash as the raw ones",
                        sRoom0.rawHash == sRoom0.finalHash && sRoom0.rawSize == sRoom0.finalSize &&
                            !sRoom0.replaced);
        // Other steps' patchers (pc_rando_patchers.def, e.g. R3.16's spawn 0 in Asoko/Room0) may grow
        // files after the smoke's last patcher, so only failed copies count here.
        randoSmokeCheck(kName, "identity: no copy failed", st.failed == 0);
        randoSmokeCheck(kName, "identity: the chest holds 0x31", chest != nullptr && chestItem(chest) == kVanillaItem);
        return;
    }
    randoSmokeCheck(kName, "item: grow ok", sGrowResult == PCR_DZX_OK);
    randoSmokeCheck(kName, "item: stage.dzs replaced by a bigger copy",
                    sStage.replaced && sStage.finalSize > sStage.rawSize && sStage.finalHash != sStage.rawHash);
    randoSmokeCheck(kName, "item: room.dzr untouched", sRoom0.rawHash == sRoom0.finalHash && !sRoom0.replaced);
    randoSmokeCheck(kName, "item: the chest holds the new item (home.angle.z >> 8)",
                    chest != nullptr && chestItem(chest) == kNewItem);
    fopAc_ac_c* grown = chestAt(c, sChestX + kGrownDx);
    randoSmokeCheck(kName, "item: the appended chest exists with its item",
                    grown != nullptr && chestItem(grown) == kGrownItem);
}

} // namespace pc
