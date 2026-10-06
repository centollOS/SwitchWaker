// COS_SMOKE=evcam (bug B20): the event camera's arguments (dCamera_c::StartEventCamera). Callers
// pass name/pointer pairs ("Type", &mProcVar6.m3570 for the bottle, food and item cameras; Bo's
// "@STARTER" and cXyz pointers); the camera reads them back through those pointers
// (getEvIntData *(int*), getEvStringPntData, getEvXyzData...). In play (any stage, e.g.
// COS_BOOT_STAGE=sea:44:206), kSettleFrames after the PLAY scene: StartEventCamera(0x12, the
// player, "Type", &value) as d_a_player_bottle.inc does, getEvIntData(&out, "Type") must give the
// value back, then EndEventCamera. Exit 0 when it does, else 1.
#include "pc_internal.h"

#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 60;
bool sChecked = false;
bool sOn = false;
unsigned int sSince = 0;
int sValue = 0x1234;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

} // namespace

void evcamFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        sOn = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "evcam") == 0;
    }
    if (!sOn || dComIfGp_getPlayer(0) == nullptr || fpcM_Search(isPlayScene, nullptr) == nullptr ||
        dCam_getBody() == nullptr || ++sSince < kSettleFrames) {
        return;
    }
    sOn = false;
    dCamera_c* cam = dCam_getBody();
    const BOOL started =
        cam->StartEventCamera(0x12, fopAcM_GetID(dComIfGp_getPlayer(0)), "Type", &sValue, 0);
    int out = 0;
    const bool got = started && cam->getEvIntData(&out, (char*)"Type");
    cam->EndEventCamera(fopAcM_GetID(dComIfGp_getPlayer(0)));
    writef(STDERR_FILENO, "[cos] evcam: frame %u: started %d, \"Type\" read back %s 0x%x (passed 0x%x at %p)\n",
           frames, started ? 1 : 0, got ? "as" : "NOT", (unsigned)out, (unsigned)sValue, (void*)&sValue);
    const bool ok = started && got && out == sValue;
    writef(STDERR_FILENO, "[cos] evcam: %s\n", ok ? "PASS" : "FAIL");
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
