// The game side of the enemy rando particles (docs/RANDOMIZER_PLAN.md step R7.4c; the pure patcher is
// features/rando/pc_rando_enemy_particles.{h,cpp}). Listed in features/rando/pc_rando_patchers.def.
//
// Patchers run inside dRes_info_c::loadResource, where the current JKRHeap is the archive's own data
// heap, so the port's global operator new would put the patcher's lasting state (the parsed seed
// enemies, the banks read per place) on a heap that is freed with that archive, and the next stage
// would read freed memory. The patcher therefore runs in a host allocation scope; the registry it
// writes (pc_rando_jpc_scene.h) has fixed-size storage.
#include "JSystem/JKernel/JKRHeap.h"
#include "features/rando/pc_rando_enemy_particles.h"

namespace {

void patchInHostMemory(PcRandoResFile* file) {
    JKRPcHostAllocScope scope;
    pc_rando_enemy_particles_patch(file);
}

bool sInit = false;

} // namespace

extern "C" void pc_rando_enemy_particles_hooks_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_dzx_patcher(patchInHostMemory);
}
