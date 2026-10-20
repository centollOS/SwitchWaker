// cos_rando_test_R0_10 (docs/RANDOMIZER_PLAN.md step R0.10): the archive-load dispatcher
// (features/rando/pc_rando_resources.h) without the game:
//   - pc_rando_res_describe: stage directory, archive and room from dRes_info_c::set's directory and
//     the archive name (ma2room/ma3room, object archives, no directory, a 14-byte name without NUL);
//   - the directory table of the dRes_info_c::set hook (record, overwrite, unknown);
//   - registration (once per function, type filters, the dzx filter, unregister) and dispatch order;
//   - in-place edits, pc_rando_res_replace and pc_rando_res_grow_dzx through a test allocator (the
//     game's is the archive heap), the next patcher seeing the copy, and the failures (no allocator,
//     out of memory, a refused grow) that leave the file as it was;
//   - the stats counters.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_resources.h"
#include "pc/pc_rando_hooks.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// A synthetic stage.dzs: one TRE2 chunk with two chests (0x20 bytes each, item at +0x1C).
Bytes makeDzs() {
    Bytes b(4 + 12 + 2 * 0x20, 0);
    put32(&b[0], 1);
    memcpy(&b[4], "TRE2", 4);
    put32(&b[8], 2);
    put32(&b[12], 16);
    for (int i = 0; i < 2; i++) {
        uint8_t* e = &b[16 + i * 0x20];
        memcpy(e, i == 0 ? "takara3" : "takara", i == 0 ? 7 : 6);
        e[0x1C] = (uint8_t)(0x31 + i);
    }
    return b;
}

// The test allocator: blocks kept until the end of the test (the game's live with the archive).
std::vector<void*> sBlocks;
size_t sAllocBudget = (size_t)-1;
int sAllocCalls = 0;

void* testAlloc(size_t size, void* ctx) {
    sAllocCalls++;
    RT_CHECK(ctx == &sBlocks);
    if (size > sAllocBudget) {
        return nullptr;
    }
    void* p = calloc(1, size);
    sBlocks.push_back(p);
    return p;
}

std::string sOrder;

void patchA(PcRandoResFile* f) {
    sOrder += 'A';
    (void)f;
}
void patchB(PcRandoResFile* f) {
    sOrder += 'B';
    (void)f;
}
void patchDat(PcRandoResFile* f) {
    sOrder += 'D';
    (void)f;
}

// Sets TRE2[0]'s item to 0x07 with the vanilla fingerprint 0x31.
int sEditResult = -1;
void patchEdit(PcRandoResFile* f) {
    sOrder += 'E';
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        return;
    }
    const uint8_t vanilla = 0x31;
    PcRandoDzxFingerprint fp = {"takara3", 0x1C, &vanilla, 1};
    sEditResult = pc_rando_dzx_set_u8(&dzx, "TRE2", 0, 0x1C, 0x07, &fp);
}

// Appends one chest (item 0x09) to TRE2.
int sGrowResult = -1;
uint32_t sGrowFirst = 0;
void patchGrow(PcRandoResFile* f) {
    sOrder += 'G';
    uint8_t e[0x20] = {};
    memcpy(e, "takara", 6);
    e[0x1C] = 0x09;
    PcRandoDzxAdd add = {"TRE2", e, 1};
    sGrowResult = pc_rando_res_grow_dzx(f, &add, 1, &sGrowFirst);
}

// Records what the patcher after the grow sees.
const uint8_t* sSeenData = nullptr;
size_t sSeenSize = 0;
int sSeenReplaced = -1;
void patchSee(PcRandoResFile* f) {
    sOrder += 'S';
    sSeenData = f->data;
    sSeenSize = f->size;
    sSeenReplaced = f->replaced;
}

PcRandoResFile makeFile(PcRandoResNames* names, Bytes& b, const char* arc, const char* dir,
                        const char* name, uint32_t type) {
    PcRandoResFile f = {};
    pc_rando_res_describe(&f, names, arc, dir);
    f.name = name;
    f.type = type;
    f.data = b.data();
    f.size = b.size();
    f.alloc = testAlloc;
    f.allocCtx = &sBlocks;
    return f;
}

void testDescribe() {
    PcRandoResFile f = {};
    PcRandoResNames n;
    pc_rando_res_describe(&f, &n, "Stage", "/res/Stage/Asoko/");
    RT_CHECK_EQ(std::string(f.stage), std::string("Asoko"));
    RT_CHECK_EQ(std::string(f.arc), std::string("Stage"));
    RT_CHECK_EQ(std::string(f.dir), std::string("/res/Stage/Asoko/"));
    RT_CHECK_EQ(f.room, -1);

    pc_rando_res_describe(&f, &n, "Room12", "/res/Stage/ma3room/");
    RT_CHECK_EQ(std::string(f.stage), std::string("ma3room"));
    RT_CHECK_EQ(f.room, 12);
    pc_rando_res_describe(&f, &n, "Room0", "/res/Stage/ma2room/");
    RT_CHECK_EQ(std::string(f.stage), std::string("ma2room"));
    RT_CHECK_EQ(f.room, 0);
    pc_rando_res_describe(&f, &n, "Roomx", "/res/Stage/sea/");
    RT_CHECK_EQ(f.room, -1);
    pc_rando_res_describe(&f, &n, "Room", "/res/Stage/sea/");
    RT_CHECK_EQ(f.room, -1);

    pc_rando_res_describe(&f, &n, "Always", "/res/Object/");
    RT_CHECK_EQ(std::string(f.stage), std::string(""));
    RT_CHECK_EQ(std::string(f.arc), std::string("Always"));
    pc_rando_res_describe(&f, &n, "Stage", nullptr);
    RT_CHECK_EQ(std::string(f.dir), std::string(""));
    RT_CHECK_EQ(std::string(f.stage), std::string(""));

    // dRes_info_c::mArchiveName is 14 bytes filled by strncpy: no NUL when the name is 14 long.
    char raw[16];
    memcpy(raw, "ABCDEFGHIJKLMNXX", 16);
    pc_rando_res_describe(&f, &n, raw, "/res/Object/");
    RT_CHECK(strlen(f.arc) <= 15);
    RT_CHECK_EQ(std::string(f.arc).substr(0, 14), std::string("ABCDEFGHIJKLMN"));
}

void testDirs() {
    pc_rando_res_clear_dirs();
    int a, b;
    RT_CHECK(pc_rando_res_dir_of(&a) == nullptr);
    pc_rando_res_hook_set(&a, "/res/Stage/Asoko/");
    pc_rando_res_note_dir(&b, "/res/Object/");
    RT_CHECK_EQ(std::string(pc_rando_res_dir_of(&a)), std::string("/res/Stage/Asoko/"));
    RT_CHECK_EQ(std::string(pc_rando_res_dir_of(&b)), std::string("/res/Object/"));
    pc_rando_res_note_dir(&a, "/res/Stage/ma3room/");
    RT_CHECK_EQ(std::string(pc_rando_res_dir_of(&a)), std::string("/res/Stage/ma3room/"));
    RT_CHECK(pc_rando_res_dir_of(nullptr) == nullptr);
    // More infos than slots: the newest ones are kept.
    static int many[300];
    for (int i = 0; i < 300; i++) {
        pc_rando_res_note_dir(&many[i], "/res/Object/");
    }
    RT_CHECK(pc_rando_res_dir_of(&many[299]) != nullptr);
    pc_rando_res_clear_dirs();
    RT_CHECK(pc_rando_res_dir_of(&a) == nullptr);
}

void testRegistry() {
    RT_CHECK_EQ(pc_rando_res_patcher_count(), 0);
    RT_CHECK_EQ(pc_rando_register_res_patcher(nullptr, PCR_RES_ANY), 0);
    RT_CHECK_EQ(pc_rando_register_res_patcher(patchA, PCR_RES_ANY), 1);
    RT_CHECK_EQ(pc_rando_register_dzx_patcher(patchB), 1);
    RT_CHECK_EQ(pc_rando_register_res_patcher(patchDat, PCR_RES_DAT), 1);
    RT_CHECK_EQ(pc_rando_register_res_patcher(patchA, PCR_RES_DAT), 1); // already there: kept as ANY
    RT_CHECK_EQ(pc_rando_res_patcher_count(), 3);

    Bytes dzs = makeDzs();
    PcRandoResNames n;
    PcRandoResFile f = makeFile(&n, dzs, "Stage", "/res/Stage/Asoko/", "stage.dzs", PCR_RES_DZS);
    sOrder.clear();
    RT_CHECK(pc_rando_res_dispatch(&f) == dzs.data());
    RT_CHECK_EQ(sOrder, std::string("AB"));
    f = makeFile(&n, dzs, "Room0", "/res/Stage/Asoko/", "room.dzr", PCR_RES_DZR);
    sOrder.clear();
    pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sOrder, std::string("AB"));
    f = makeFile(&n, dzs, "Stage", "/res/Stage/Asoko/", "event_list.dat", PCR_RES_DAT);
    sOrder.clear();
    pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sOrder, std::string("AD"));
    f = makeFile(&n, dzs, "Stage", "/res/Stage/Asoko/", "x.bdl", PCR_RES_TYPE('B', 'D', 'L', ' '));
    sOrder.clear();
    pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sOrder, std::string("A"));

    RT_CHECK_EQ(pc_rando_unregister_res_patcher(patchA), 1);
    RT_CHECK_EQ(pc_rando_unregister_res_patcher(patchA), 0);
    RT_CHECK_EQ(pc_rando_unregister_res_patcher(patchB), 1);
    RT_CHECK_EQ(pc_rando_unregister_res_patcher(patchDat), 1);
    RT_CHECK_EQ(pc_rando_res_patcher_count(), 0);
}

void testEditAndGrow() {
    PcRandoResStats before;
    pc_rando_res_stats(&before);
    pc_rando_register_dzx_patcher(patchEdit);
    pc_rando_register_dzx_patcher(patchGrow);
    pc_rando_register_dzx_patcher(patchSee);

    Bytes dzs = makeDzs();
    const Bytes raw = dzs;
    PcRandoResNames n;
    PcRandoResFile f = makeFile(&n, dzs, "Stage", "/res/Stage/Asoko/", "stage.dzs", PCR_RES_DZS);
    sOrder.clear();
    sAllocCalls = 0;
    uint8_t* out = (uint8_t*)pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sOrder, std::string("EGS"));
    RT_CHECK_EQ(sEditResult, (int)PCR_DZX_OK);
    RT_CHECK_EQ(sGrowResult, (int)PCR_DZX_OK);
    RT_CHECK_EQ(sGrowFirst, 2u);
    RT_CHECK_EQ(sAllocCalls, 1);
    RT_CHECK(out != dzs.data());
    RT_CHECK(out == f.data && f.replaced == 1);
    RT_CHECK(sSeenData == out && sSeenSize == f.size && sSeenReplaced == 1);
    RT_CHECK(f.size > raw.size());
    // The in-place edit is in the archive's buffer and in the copy; the copy has the new chest.
    RT_CHECK_EQ(dzs[16 + 0x1C], 0x07);
    PcRandoDzx dzx;
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, out, f.size), (int)PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_dzx_count(&dzx, "TRE2"), 3u);
    RT_CHECK_EQ(pc_rando_dzx_entry(&dzx, "TRE2", 0)[0x1C], 0x07);
    RT_CHECK_EQ(pc_rando_dzx_entry(&dzx, "TRE2", 1)[0x1C], 0x32);
    RT_CHECK_EQ(pc_rando_dzx_entry(&dzx, "TRE2", 2)[0x1C], 0x09);

    PcRandoResStats after;
    pc_rando_res_stats(&after);
    RT_CHECK_EQ(after.dispatched - before.dispatched, 1u);
    RT_CHECK_EQ(after.patched - before.patched, 1u);
    RT_CHECK_EQ(after.replaced - before.replaced, 1u);
    RT_CHECK_EQ(after.failed - before.failed, 0u);

    // A second load of the same vanilla file with the edit's fingerprint now failing is refused
    // without writing (D9): the patched buffer is not vanilla any more.
    sEditResult = -1;
    f = makeFile(&n, dzs, "Stage", "/res/Stage/Asoko/", "stage.dzs", PCR_RES_DZS);
    pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sEditResult, (int)PCR_DZX_MISMATCH);

    // Out of memory: the grow fails, the file stays the archive's buffer with its in-place edit.
    Bytes dzs2 = makeDzs();
    sAllocBudget = 8;
    f = makeFile(&n, dzs2, "Stage", "/res/Stage/Asoko/", "stage.dzs", PCR_RES_DZS);
    out = (uint8_t*)pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sGrowResult, (int)PCR_DZX_NO_SPACE);
    RT_CHECK(out == dzs2.data() && f.size == dzs2.size() && f.replaced == 0);
    RT_CHECK(sSeenData == dzs2.data() && sSeenReplaced == 0);
    RT_CHECK_EQ(dzs2[16 + 0x1C], 0x07);
    sAllocBudget = (size_t)-1;

    // No allocator: same.
    Bytes dzs3 = makeDzs();
    f = makeFile(&n, dzs3, "Stage", "/res/Stage/Asoko/", "stage.dzs", PCR_RES_DZS);
    f.alloc = nullptr;
    out = (uint8_t*)pc_rando_res_dispatch(&f);
    RT_CHECK_EQ(sGrowResult, (int)PCR_DZX_NO_SPACE);
    RT_CHECK(out == dzs3.data() && f.replaced == 0);

    // A file that is not a dzx: the grow is refused before allocating.
    Bytes junk(8, 0xFF);
    sAllocCalls = 0;
    f = makeFile(&n, junk, "Stage", "/res/Stage/Asoko/", "stage.dzs", PCR_RES_DZS);
    out = (uint8_t*)pc_rando_res_dispatch(&f);
    RT_CHECK(sGrowResult != (int)PCR_DZX_OK);
    RT_CHECK_EQ(sAllocCalls, 0);
    RT_CHECK(out == junk.data() && f.replaced == 0);

    PcRandoResStats fin;
    pc_rando_res_stats(&fin);
    RT_CHECK_EQ(fin.failed - after.failed, 2u);

    pc_rando_unregister_res_patcher(patchEdit);
    pc_rando_unregister_res_patcher(patchGrow);
    pc_rando_unregister_res_patcher(patchSee);
}

void testReplace() {
    Bytes b = {1, 2, 3, 4};
    PcRandoResNames n;
    PcRandoResFile f = makeFile(&n, b, "Stage", "/res/Stage/sea/", "event_list.dat", PCR_RES_DAT);
    uint8_t* p = pc_rando_res_replace(&f, 6);
    RT_CHECK(p != nullptr && f.data == p && f.size == 6 && f.replaced == 1);
    RT_CHECK(p != nullptr && p[0] == 1 && p[3] == 4 && p[4] == 0 && p[5] == 0);
    uint8_t* q = pc_rando_res_replace(&f, 2);
    RT_CHECK(q != nullptr && f.size == 2 && q[0] == 1 && q[1] == 2);
    f.alloc = nullptr;
    RT_CHECK(pc_rando_res_replace(&f, 8) == nullptr);
    RT_CHECK(f.data == q && f.size == 2);
}

template <int N> void nth(PcRandoResFile*) {}

template <int... I> void registerAll(std::integer_sequence<int, I...>, int* ok) {
    ((*ok += pc_rando_register_res_patcher(nth<I>, PCR_RES_ANY)), ...);
}

template <int... I> void unregisterAll(std::integer_sequence<int, I...>) {
    (pc_rando_unregister_res_patcher(nth<I>), ...);
}

void testFull() {
    // 64 entries; the 65th is refused.
    RT_CHECK_EQ(pc_rando_res_patcher_count(), 0);
    int ok = 0;
    registerAll(std::make_integer_sequence<int, 65>(), &ok);
    RT_CHECK_EQ(ok, 64);
    RT_CHECK_EQ(pc_rando_res_patcher_count(), 64);
    unregisterAll(std::make_integer_sequence<int, 65>());
    RT_CHECK_EQ(pc_rando_res_patcher_count(), 0);
}

} // namespace

int main() {
    testDescribe();
    testDirs();
    testRegistry();
    testEditAndGrow();
    testReplace();
    testFull();
    for (void* p : sBlocks) {
        free(p);
    }
    return rt::finish();
}
