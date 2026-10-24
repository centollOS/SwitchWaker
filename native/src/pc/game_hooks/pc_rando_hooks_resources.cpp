// The game side of the randomizer's archive-load hook (docs/RANDOMIZER_PLAN.md step R0.10, decision
// D8; the dispatcher is features/rando/pc_rando_resources.{h,cpp}, which stays free of game code for
// the unit tests). dRes_info_c::loadResource (game/src/d/d_resorce.cpp) calls
// pc_rando_res_hook_loaded under pc_rando() for every file it stores in mRes[]. The file's bytes are
// still the disc's (stage.dzs, room.dzr, event_list.dat); the registered patchers edit them, or
// replace them with a copy allocated on the current heap, which during loadResource is the archive's
// own data heap (dRes_info_c::setRes), so the copy is freed with the archive.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_resources.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"

#define PC_RANDO_PATCHER(fn) extern "C" void fn(void);
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER

namespace {

bool sPatchersInit = false;

void initPatchers() {
    if (sPatchersInit) {
        return;
    }
    sPatchersInit = true;
#define PC_RANDO_PATCHER(fn) fn();
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER
}

void* allocOnCurrentHeap(size_t size, void*) {
    if (size > 0xFFFFFFFFu) {
        return nullptr;
    }
    return JKRHeap::alloc((u32)size, 0x20, nullptr);
}

} // namespace

// R7.4d audit: the patchers (and their init functions, which run at the first dispatch) keep lasting
// state per seed or per activation (parsed seed keys, plans, caches); allocated with the port's
// global operator new during loadResource it would land on the archive's heap, which the next stage
// change frees (R7.4b's stage-change crash). Both dispatches therefore run in a host allocation
// scope. Grown or replaced files still come from the archive heap: file.alloc calls JKRHeap::alloc
// on the current heap directly, which the scope does not change.
extern "C" void* pc_rando_res_hook_loaded(const void* info, const char* arcName, void* archive,
                                          uint32_t type, const char* fileName, void* res) {
    JKRPcHostAllocScope hostAlloc;
    initPatchers();
    if (res == nullptr || archive == nullptr || pc_rando_res_patcher_count() == 0) {
        return res;
    }
    PcRandoResFile file = {};
    PcRandoResNames names;
    pc_rando_res_describe(&file, &names, arcName, pc_rando_res_dir_of(info));
    file.name = fileName != nullptr ? fileName : "";
    file.type = type;
    file.data = (uint8_t*)res;
    const u32 size = ((JKRArchive*)archive)->getResSize(res); // (u32)-1: not found
    file.size = size != 0xFFFFFFFFu ? size : 0;
    file.alloc = allocOnCurrentHeap;
    file.allocCtx = nullptr;
    if (file.size == 0) {
        return res;
    }
    return pc_rando_res_dispatch(&file);
}

// R7.2c: the raw J3D hook. dRes_info_c::loadResource calls it under pc_rando() right after
// JKRGetResource, before the J3D loaders turn a BMD/BDL/BMT/BRK file into objects that point into its
// bytes. Only J3D types are dispatched here (pc_rando_res_is_j3d_type); everything else is still raw
// after the loaders and goes through pc_rando_res_hook_loaded as before.
extern "C" void* pc_rando_res_hook_raw(const void* info, const char* arcName, void* archive, uint32_t type,
                                       const char* fileName, void* res) {
    JKRPcHostAllocScope hostAlloc; // see pc_rando_res_hook_loaded
    initPatchers();
    if (res == nullptr || archive == nullptr || !pc_rando_res_is_j3d_type(type) ||
        pc_rando_raw_patcher_count() == 0) {
        return res;
    }
    PcRandoResFile file = {};
    PcRandoResNames names;
    pc_rando_res_describe(&file, &names, arcName, pc_rando_res_dir_of(info));
    file.name = fileName != nullptr ? fileName : "";
    file.type = type;
    file.data = (uint8_t*)res;
    const u32 size = ((JKRArchive*)archive)->getResSize(res);
    file.size = size != 0xFFFFFFFFu ? size : 0;
    file.alloc = allocOnCurrentHeap;
    file.allocCtx = nullptr;
    if (file.size == 0) {
        return res;
    }
    return pc_rando_res_raw_dispatch(&file);
}
