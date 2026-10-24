// COS_SMOKE=rando COS_RANDO_SMOKE=cosmetic (docs/RANDOMIZER_PLAN.md step R7.5a): the cosmetic tweaks
// wwrando applies on every seed (features/rando/pc_rando_tweak_cosmetic.h), e.g.
//   run.sh rando --stage Fairy01:0:0 --env COS_RANDO_SMOKE=cosmetic --env COS_RANDO_FORCE=1
// Once the play scene has run 10 frames:
//   1. Fairy fountains: the six Fairy0N Stage.arc are read from the disc into a private heap (as the
//      entr smoke does); on a copy of each stage.dzs the patch must change every Pale record's bg0_c0
//      and bg0_k0 and no other byte. The patched records are written to COS_COSMETIC_DUMP (default
//      $COS_RUN_DIR/cosmetic_pale.txt) for native/tools/rando/cosmetic_check.py, which compares them
//      with wwrando's give_fairy_fountains_distinct_colors run with gclib on the same disc.
//   2. The boot stage, when it is a Fairy0N: its live Pale records (dStage_stageDt_c::getPaletInfo)
//      equal the patched copy under pc_rando(), the disc's bytes otherwise (the patcher never ran).
//   3. Rainbow Rupee: item_resource[0xB8].mTevFrm is 7 under pc_rando(), 5 (vanilla) otherwise.
//   4. Field items are created in front of the player (createItem type 3, action 5, as
//      custom_createItem): an orange rupee (mTevFrm 5, 0xB8's vanilla colour) always, and the 0xB8
//      item under pc_rando() (a vanilla game has no field model for it). For 200 frames their BRK
//      frames are read after each draw: the orange rupee's stays 5.0; under pc_rando() every 0xB8
//      value equals fabs() of the key frame stepped by the draws counted since the previous sample
//      (pc_rando_cosmetic_rainbow_advance) and goes 0 -> 6 -> 0 -> up again; without it the rainbow
//      key frame never moves.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_tweak_cosmetic.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "d/actor/d_a_itembase.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "cosmetic";
constexpr const char* kFairy[PCR_COSMETIC_FAIRY_STAGES] = {"Fairy01", "Fairy02", "Fairy03",
                                                           "Fairy04", "Fairy05", "Fairy06"};
constexpr uint32_t kPaleSize = 0x2C;
constexpr unsigned kSampleFrames = 200;

enum State { kInit, kSpawn, kSample, kDone };
State sState = kInit;
JKRExpHeap* sHeap = nullptr;

// The boot stage's Pale records: raw (disc) and patched.
int sBootK = -1;
uint8_t* sBootRaw = nullptr;
uint8_t* sBootPatched = nullptr;
uint32_t sBootCount = 0;

fpc_ProcID sItem = fpcM_ERROR_PROCESS_ID_e; // 0xB8 (pc_rando() only)
fpc_ProcID sRef = fpcM_ERROR_PROCESS_ID_e;  // the orange rupee
unsigned sRefSamples = 0;
bool sRefOk = true;
unsigned sSampleStart = 0;
unsigned sSamples = 0;
unsigned sSpawnWait = 0;
uint32_t sLastDraws = 0;
float sSimKeyframe = 0.0f;
bool sSimOk = true;
int sPhase = 0; // 0 rising to 6, 1 falling to 0, 2 rising again, 3 seen all
float sPrev = -1.0f;
bool sVanillaOk = true;

void cosLogf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void cosLogf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

// The stage.dzs of /res/Stage/<stage>/Stage.arc, read into malloc memory (NULL on failure).
uint8_t* readStageDzs(const char* stage, size_t* outSize) {
    char path[128];
    snprintf(path, sizeof path, "/res/Stage/%s/Stage.arc", stage);
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        cosLogf("[cos] rando-smoke cosmetic: %s: cannot mount a private copy\n", path);
        sHeap->freeAll();
        return nullptr;
    }
    for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst(); l != nullptr; l = l->getNext()) {
        if (l->getObject() == arc) {
            JKRFileLoader::getVolumeList().remove(l);
            break;
        }
    }
    uint8_t* out = nullptr;
    void* res = arc->getResource(0, "stage.dzs");
    if (res != nullptr) {
        *outSize = arc->getResSize(res);
        out = (uint8_t*)malloc(*outSize);
        if (out != nullptr) {
            memcpy(out, res, *outSize);
        }
    }
    arc->unmountFixed();
    sHeap->freeAll();
    return out;
}

FILE* openDump() {
    const char* path = getenv("COS_COSMETIC_DUMP");
    char buf[1024];
    if (path == nullptr || path[0] == '\0') {
        const char* run = getenv("COS_RUN_DIR");
        if (run == nullptr || run[0] == '\0') {
            return nullptr;
        }
        snprintf(buf, sizeof buf, "%s/cosmetic_pale.txt", run);
        path = buf;
    }
    FILE* f = fopen(path, "w");
    if (f != nullptr) {
        cosLogf("[cos] rando-smoke cosmetic: Pale dump %s\n", path);
    }
    return f;
}

// Step 1 (and the boot stage's records for step 2).
void checkFairyFiles() {
    for (u32 size = 16u << 20; sHeap == nullptr && size >= (2u << 20); size >>= 1) {
        sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
    }
    if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr)) {
        return;
    }
    FILE* dump = openDump();
    sBootK = pc_rando_cosmetic_fairy_index(dComIfGp_getStartStageName());
    int read = 0, patched = 0, clean = 0, records = 0;
    for (int k = 0; k < PCR_COSMETIC_FAIRY_STAGES; k++) {
        size_t size = 0;
        uint8_t* raw = readStageDzs(kFairy[k], &size);
        if (raw == nullptr) {
            continue;
        }
        read++;
        uint8_t* work = (uint8_t*)malloc(size);
        memcpy(work, raw, size);
        const int n = pc_rando_cosmetic_fairy_patch_dzs(work, size, k);
        if (n > 0) {
            patched++;
            records += n;
        }
        PcRandoDzx dzx;
        bool onlyBg0 = n > 0 && pc_rando_dzx_open(&dzx, raw, size) == PCR_DZX_OK;
        const uint32_t count = onlyBg0 ? pc_rando_dzx_count(&dzx, "Pale") : 0;
        const uint8_t* pale0 = onlyBg0 ? pc_rando_dzx_entry(&dzx, "Pale", 0) : nullptr;
        if (pale0 == nullptr) {
            onlyBg0 = false;
        }
        for (size_t b = 0; onlyBg0 && b < size; b++) {
            const ptrdiff_t rel = (ptrdiff_t)b - (pale0 - raw);
            const bool inPale = rel >= 0 && rel < (ptrdiff_t)(count * kPaleSize);
            const uint32_t field = inPale ? (uint32_t)(rel % kPaleSize) : 0;
            const bool editable = inPale && field >= 0x06 && field < 0x0C;
            if (!editable && work[b] != raw[b]) {
                cosLogf("[cos] rando-smoke cosmetic: %s byte 0x%zx changed outside bg0_c0/bg0_k0\n",
                       kFairy[k], b);
                onlyBg0 = false;
            }
        }
        clean += onlyBg0 ? 1 : 0;
        for (uint32_t i = 0; onlyBg0 && i < count; i++) {
            const uint8_t* e = work + (pale0 - raw) + i * kPaleSize;
            const uint8_t* r = pale0 + i * kPaleSize;
            cosLogf("[cos] rando-smoke cosmetic: %s Pale %u bg0_c0 %d %d %d -> %d %d %d, bg0_k0 %d %d %d -> %d %d %d\n",
                   kFairy[k], i, r[6], r[7], r[8], e[6], e[7], e[8], r[9], r[10], r[11], e[9], e[10], e[11]);
            if (dump != nullptr) {
                fprintf(dump, "pale %s %u bg0_c0 %d %d %d bg0_k0 %d %d %d\n", kFairy[k], i, e[6], e[7], e[8], e[9],
                        e[10], e[11]);
            }
        }
        if (k == sBootK && onlyBg0) {
            sBootCount = count;
            sBootRaw = (uint8_t*)malloc(count * kPaleSize);
            sBootPatched = (uint8_t*)malloc(count * kPaleSize);
            memcpy(sBootRaw, pale0, count * kPaleSize);
            memcpy(sBootPatched, work + (pale0 - raw), count * kPaleSize);
        }
        free(work);
        free(raw);
    }
    if (dump != nullptr) {
        fclose(dump);
    }
    char what[128];
    snprintf(what, sizeof what, "the 6 Fairy0N stage.dzs read and patched (%d read, %d patched, %d Pale records)",
             read, patched, records);
    randoSmokeCheck(kName, what, read == 6 && patched == 6);
    randoSmokeCheck(kName, "only bg0_c0 and bg0_k0 change", clean == 6);
}

// Step 2.
void checkLive() {
    if (sBootK < 0) {
        cosLogf("[cos] rando-smoke cosmetic: boot stage %s is not a fairy fountain, no live check\n",
               dComIfGp_getStartStageName());
        return;
    }
    dStage_dt_c* stage = &dComIfGp_getStage();
    const stage_palet_info_class* live = stage->getPaletInfo();
    // (dStage_paletInfoInit sets no count: getPaletNumInfo stays 0; the disc's count is used.)
    const uint8_t* want = pc_rando() ? sBootPatched : sBootRaw;
    bool same = live != nullptr && want != nullptr && sBootCount > 0;
    if (!same) {
        cosLogf("[cos] rando-smoke cosmetic: live Pale %p, disc records %u\n", (const void*)live, sBootCount);
    }
    for (uint32_t i = 0; same && i < sBootCount; i++) {
        // The colours are u8 triples, the same on the disc and in memory.
        same = memcmp((const uint8_t*)live + i * kPaleSize, want + i * kPaleSize, 0x0C) == 0;
        const uint8_t* l = (const uint8_t*)live + i * kPaleSize;
        cosLogf("[cos] rando-smoke cosmetic: live %s Pale %u bg0_c0 %d %d %d bg0_k0 %d %d %d\n",
               dComIfGp_getStartStageName(), i, l[6], l[7], l[8], l[9], l[10], l[11]);
    }
    randoSmokeCheck(kName, pc_rando() ? "live Pale records = the patched copy" : "live Pale records = the disc's",
                    same);
    PcRandoCosmeticStatus st;
    pc_rando_tweak_cosmetic_status(sBootK, &st);
    randoSmokeCheck(kName, pc_rando() ? "the patcher changed the boot stage" : "the patcher never ran",
                    pc_rando() ? (st.patched >= 1 && st.last == (int)sBootCount) : st.seen == 0);
}

// Step 3.
void checkTable() {
    const int tev = dItem_data::getTevFrm(PCR_COSMETIC_RAINBOW_ITEM);
    char what[96];
    snprintf(what, sizeof what, "item_resource[0xB8].mTevFrm = %d", tev);
    randoSmokeCheck(kName, what, tev == (pc_rando() ? PCR_COSMETIC_RAINBOW_TEV_FRM : 5));
    if (pc_rando()) {
        randoSmokeCheck(kName, "the table edit was registered", pc_rando_cosmetic_edit_ok() != 0);
    }
}

fpc_ProcID spawnAt(int itemNo, float side) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz pos = player->current.pos;
    const float a = player->shape_angle.y * (3.14159265f / 0x8000);
    // 250 in front of the player, `side` to his left or right, out of his reach.
    pos.x += 250.0f * sinf(a) + side * cosf(a);
    pos.z += 250.0f * cosf(a) - side * sinf(a);
    pos.y += 20.0f;
    csXyz angle(0, 0, 0);
    return fopAcM_createItem(&pos, itemNo, -1, fopAcM_GetRoomNo(player), 3, &angle, 5, nullptr);
}

void spawn() {
    // The orange rupee (mTevFrm 5, the 0xB8 rupee's vanilla colour) in both modes; the 0xB8 item only
    // under pc_rando(): a vanilla game has no field model for it (R2.11 allow_all_items_to_be_field_items).
    sRef = spawnAt(dItemNo_ORANGE_RUPEE_e, -120.0f);
    randoSmokeCheck(kName, "field item 0x06 (orange rupee) created", sRef != fpcM_ERROR_PROCESS_ID_e);
    if (pc_rando()) {
        sItem = spawnAt(PCR_COSMETIC_RAINBOW_ITEM, 120.0f);
        randoSmokeCheck(kName, "field item 0xB8 created", sItem != fpcM_ERROR_PROCESS_ID_e);
    }
}

daItemBase_c* itemOf(fpc_ProcID id) {
    fopAc_ac_c* a = nullptr;
    if (id == fpcM_ERROR_PROCESS_ID_e || !fopAcM_SearchByID(id, &a) || a == nullptr) {
        return nullptr;
    }
    daItemBase_c* it = (daItemBase_c*)a;
    if (it->mpBrkAnm1 == nullptr || it->mpBrkAnm1->getBrkAnm() == nullptr) {
        return nullptr;
    }
    return it;
}

bool ready() {
    return itemOf(sRef) != nullptr && (!pc_rando() || itemOf(sItem) != nullptr);
}

// One sample of step 4 (called once per game frame, after the previous frame's draw).
void sample() {
    const uint32_t draws = pc_rando_cosmetic_rainbow_draws();
    if (daItemBase_c* ref = itemOf(sRef)) {
        const float frame = ref->mpBrkAnm1->getBrkAnm()->getFrame();
        sRefSamples++;
        if (frame != 5.0f && sRefOk) {
            cosLogf("[cos] rando-smoke cosmetic: orange rupee BRK frame %.6g\n", frame);
            sRefOk = false;
        }
    }
    if (!pc_rando()) {
        if (draws != 0 && sVanillaOk) {
            cosLogf("[cos] rando-smoke cosmetic: vanilla: %u rainbow draws\n", draws);
            sVanillaOk = false;
        }
        return;
    }
    daItemBase_c* it = itemOf(sItem);
    if (it == nullptr) {
        return;
    }
    const float frame = it->mpBrkAnm1->getBrkAnm()->getFrame();
    sSamples++;
    if (sSamples == 1) {
        // The draws before the first sample: replay them from boot's 0.0.
        sSimKeyframe = 0.0f;
        sLastDraws = 0;
    }
    float want = std::fabs(sSimKeyframe);
    for (uint32_t i = sLastDraws; i < draws; i++) {
        want = pc_rando_cosmetic_rainbow_advance(&sSimKeyframe);
    }
    sLastDraws = draws;
    if (draws == 0 || frame != want) {
        if (sSimOk) {
            cosLogf("[cos] rando-smoke cosmetic: sample %u: BRK frame %.9g, expected %.9g (draws %u)\n", sSamples,
                    frame, want, draws);
        }
        sSimOk = false;
    }
    if (sSamples % 10 == 1) {
        cosLogf("[cos] rando-smoke cosmetic: sample %u: BRK frame %.4f (draws %u)\n", sSamples, frame, draws);
    }
    if (sPrev >= 0.0f) {
        if (sPhase == 0 && frame >= 5.85f && frame < sPrev) {
            sPhase = 1;
        } else if (sPhase == 1 && frame <= 0.15f) {
            sPhase = 2;
        } else if (sPhase == 2 && frame >= 1.0f) {
            sPhase = 3;
        }
    }
    sPrev = frame;
}

} // namespace

void randoSmokeCosmetic(unsigned int frames) {
    switch (sState) {
    case kInit:
        if (!randoSmokeInPlay(10) || dComIfGp_getPlayer(0) == nullptr) {
            return;
        }
        cosLogf("[cos] rando-smoke cosmetic: pc_rando() %d, stage %s\n", pc_rando(),
               dComIfGp_getStartStageName());
        checkFairyFiles();
        checkLive();
        checkTable();
        spawn();
        sState = kSpawn;
        sSpawnWait = 0;
        return;
    case kSpawn:
        if (ready()) {
            sState = kSample;
            sSampleStart = frames;
        } else if (++sSpawnWait > 120) {
            randoSmokeCheck(kName, "the field items exist with their BRK", false);
            randoSmokeEnd(kName);
        }
        return;
    case kSample:
        sample();
        if (frames - sSampleStart < kSampleFrames) {
            return;
        }
        {
            char what[128];
            snprintf(what, sizeof what, "%u samples of the orange rupee's BRK frame, all 5.0", sRefSamples);
            randoSmokeCheck(kName, what, sRefSamples >= kSampleFrames / 2 && sRefOk);
        }
        if (pc_rando()) {
            char what[128];
            snprintf(what, sizeof what, "%u samples of the 0xB8 item's BRK frame", sSamples);
            randoSmokeCheck(kName, what, sSamples >= kSampleFrames / 2);
            randoSmokeCheck(kName, "every BRK frame = fabs(key frame) after the draws counted", sSimOk);
            randoSmokeCheck(kName, "the BRK frame went 0 -> 6 -> 0 -> up again", sPhase == 3);
        } else {
            randoSmokeCheck(kName, "vanilla: the rainbow key frame never moves", sVanillaOk);
        }
        sState = kDone;
        randoSmokeEnd(kName);
    case kDone:
        return;
    }
}

} // namespace pc
