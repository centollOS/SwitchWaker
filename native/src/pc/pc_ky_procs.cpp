// COS_SMOKE=ky-procs: the effect processes that make their own fixed-size heaps (audit of GameCube
// sizes on PC). In play at sea (COS_BOOT_PRESET=sailing: on the boat at Windfall), one at a time
// kRunFrames apart, each created next to the player (or, for the lightning, near the camera) the
// way the game creates it, inside the play scene's layer (the game creates them while the play
// scene runs; created from the root layer a process is drawn twice a frame, its J3D packets linked
// to themselves, and the FIFO overflows):
//   - the storm's lightning (d_ky_thunder, fopKyM_create(-1) as dKyr_thunder_move does; bug B22:
//     its GameCube-sized 0x4A0 solid heap ran out at J3DModel::newDifferedDisplayList, so no
//     lightning was ever created: the console's repeated 0x100-byte failures in a 0x4B0 heap),
//   - a water mark (d_water_mark, 0x12A0, fopKyM_create(1, pos) as d_a_player_particle does),
//   - a water pillar (d_wpillar, 0x3440, fopKyM_createWpillar as c_damagereaction does).
// Each must still exist kCheckFrames after its creation and be drawn, and no JKR allocation may
// fail while it is created. Exit 0 when all held, else 1. A shot of each.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_kankyo_mng.h"
#include "f_pc/f_pc_executor.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node.h"

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 90;
constexpr unsigned int kCheckFrames = 3;
constexpr unsigned int kRunFrames = 40;

struct Proc {
    const char* name;
    s16 procName;
};
const Proc kProcs[] = {
    {"d_ky_thunder (lightning)", fpcNm_KY_THUNDER_e},
    {"d_water_mark (water mark)", fpcNm_WATER_MARK_e},
    {"d_wpillar (water pillar)", fpcNm_WPILLAR_e},
};

bool sChecked = false;
bool sOn = false;
unsigned int sSince = 0;
int sIndex = -1;
unsigned int sFailuresBefore = 0;
unsigned int sErrors = 0;
fpc_ProcID sId = fpcM_ERROR_PROCESS_ID_e;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

fpc_ProcID create(const Proc& p) {
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    cXyz pos = dComIfGp_getPlayer(0)->current.pos;
    pos.z += 300.0f;
    fpc_ProcID id;
    switch (p.procName) {
    case fpcNm_KY_THUNDER_e:
        id = fopKyM_create(fpcNm_KY_THUNDER_e, -1); // dKyr_thunder_move
        break;
    case fpcNm_WATER_MARK_e:
        id = fopKyM_create(fpcNm_WATER_MARK_e, 1, &pos); // d_a_player_particle
        break;
    default:
        id = fopKyM_createWpillar(&pos, 1.0f, 1.0f, 0); // c_damagereaction
        break;
    }
    fpcLy_SetCurrentLayer(saved);
    return id;
}

} // namespace

void kyProcsFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        sOn = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "ky-procs") == 0;
    }
    if (!sOn || dComIfGp_getPlayer(0) == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return;
    }
    ++sSince;
    if (sIndex < 0) {
        if (sSince >= kSettleFrames) {
            sIndex = 0;
            sSince = 0;
        }
        return;
    }
    if (sIndex >= (int)(sizeof(kProcs) / sizeof(kProcs[0]))) {
        sOn = false;
        writef(STDERR_FILENO, "[cos] ky-procs: %s\n", sErrors == 0 ? "PASS" : "FAIL");
        pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
        return;
    }
    const Proc& p = kProcs[sIndex];
    if (sSince == 1) {
        sFailuresBefore = heapAllocFailures();
        sId = create(p);
        writef(STDERR_FILENO, "[cos] ky-procs: frame %u: %s created, process %u\n", frames, p.name, (unsigned)sId);
    } else if (sSince == 1 + kCheckFrames) {
        const unsigned int failures = heapAllocFailures() - sFailuresBefore;
        const bool exists = sId != fpcM_ERROR_PROCESS_ID_e && fpcEx_SearchByID(sId) != nullptr;
        writef(STDERR_FILENO, "[cos] ky-procs: frame %u: %s %s, %u failed JKR allocation(s)\n", frames, p.name,
               exists ? "exists" : "is GONE (its create failed)", failures);
        if (!exists || failures != 0) {
            sErrors++;
            writef(STDERR_FILENO, "[cos] ky-procs: FAIL %s\n", p.name);
        }
        captureFrame(frames, shotSink, nullptr);
    } else if (sSince >= kRunFrames) {
        sIndex++;
        sSince = 0;
    }
}

} // namespace pc
