// COS_SMOKE=picto-box (bugs B36, B37): photos with the picto box. Run with
//   --stage Asoko:0:0:2 --input native/check/input/picto-box.txt [--env COS_BOOT_ITEMS=26]
//   --stage sea:11:9 --input native/check/input/picto-box.txt (a subject in the photos)
// The test gives the debug boot's new file the picto box on X (COS_BOOT_ITEMS=23, unless the
// environment names another: 26 is the deluxe picto box). The script raises it with X and takes a
// photo with A, twice. Each photo's GXCopyTex must be read back into the game's buffer
// (pc_capture.cpp) with a picture in it (a luma range of at least kMinSpread, not the grey of a
// failed readback) in the box's format (I8, the deluxe box RGB565), and the capture thread's
// encode_s3tc must not panic (its JUT_ASSERT on the I8 range ended the game before the B36 fix).
// On Windfall's town square (stage sea) a Killer Bee (NPC_MK) is held kSubjectDist in front of the
// player, facing them, until the photos are judged: dSnap's subject check (Judge, peeking the EFB
// alpha of the shutter area; bug B37: it read white and saw nothing) must see something in every
// photo and judge at least one of them DSNAP_TYPE_NPC_MK. Pass kSettleFrames after the second photo; fail if a readback
// failed, a photo is flat, the subject was not seen, or the photos were not taken by kLastFrame.
// Exit 0 or 1.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_math.h"

#include <dolphin/gx.h>

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kPhotos = 2;
constexpr unsigned int kSettleFrames = 60;
constexpr unsigned int kLastFrame = 1800;
constexpr unsigned int kMinSpread = 32;
constexpr f32 kSubjectDist = 200.0f;

bool sChecked = false;
bool sOn = false;
int sFormat = GX_TF_I8;
unsigned int sSeen = 0;
unsigned int sDoneFrame = 0;
bool sSubject = false;
unsigned int sJudged = 0;
bool sSubjectLogged = false;
bool sBeeJudged = false;

// The Killer Bee held in front of the player (it walks off between frames).
void holdSubject(unsigned int frames) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    fopAc_ac_c* bee = fopAcM_SearchByName(fpcNm_NPC_MK_e);
    if (player == nullptr || bee == nullptr) {
        return;
    }
    const s16 yaw = player->shape_angle.y;
    const cXyz pos(player->current.pos.x + kSubjectDist * cM_ssin(yaw), player->current.pos.y,
                   player->current.pos.z + kSubjectDist * cM_scos(yaw));
    bee->current.pos = pos;
    bee->old.pos = pos;
    bee->speedF = 0.0f;
    bee->shape_angle.y = (s16)(yaw + 0x8000);
    bee->current.angle.y = (s16)(yaw + 0x8000);
    if (!sSubjectLogged) {
        sSubjectLogged = true;
        writef(STDERR_FILENO, "[cos] picto-box: frame %u: Killer Bee held %.0f in front of the player\n", frames,
               (double)kSubjectDist);
    }
}

[[noreturn]] void fail(const char* why) {
    writef(STDERR_FILENO, "[cos] picto-box: FAIL: %s\n", why);
    pc_exit(PC_EXIT_CHECK_FAILED);
}

} // namespace

void pictoBoxFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "picto-box") != 0) {
            return;
        }
        // Read by the debug boot in the logo scene, many frames from now.
        setenv("COS_BOOT_ITEMS", "23", 0);
        sFormat = strtoul(getenv("COS_BOOT_ITEMS"), nullptr, 16) == 0x26 ? GX_TF_RGB565 : GX_TF_I8;
        const PcBootStage* boot = pc_boot_stage();
        sSubject = boot != nullptr && strcmp(boot->stage, "sea") == 0;
        sOn = true;
        writef(STDERR_FILENO, "[cos] picto-box: item %s, %u photos expected as %s\n", getenv("COS_BOOT_ITEMS"),
               kPhotos, sFormat == GX_TF_I8 ? "I8" : "RGB565");
    }
    if (!sOn) {
        return;
    }
    const CaptureStats& stats = captureStats();
    if (sSubject && stats.snapJudged < kPhotos) {
        holdSubject(frames);
    }
    if (sSubject && stats.snapJudged != sJudged) {
        sJudged = stats.snapJudged;
        if (stats.snapSeen == 0) {
            fail("the subject check saw nothing in the photo");
        }
        // SetResult lets a later subject with a quest judgement (another villager in the photo) replace
        // the figure result with its own 0, as on the GameCube: one photo judged the bee is enough.
        if (stats.snapResult == DSNAP_TYPE_NPC_MK) {
            sBeeJudged = true;
        }
    }
    if (stats.failed != 0) {
        fail("a photo's copy was not read back");
    }
    if (stats.readBack != sSeen) {
        sSeen = stats.readBack;
        if (stats.lastFormat != sFormat) {
            fail("the photo was copied in another format than the box's");
        }
        if (stats.maxLuma < stats.minLuma + kMinSpread) {
            fail("the photo is flat (no picture in the read-back copy)");
        }
        if (sSeen == kPhotos) {
            sDoneFrame = frames;
        }
    }
    if (sDoneFrame != 0 && frames >= sDoneFrame + kSettleFrames) {
        if (sSubject && sJudged < kPhotos) {
            fail("fewer photos judged than taken");
        }
        if (sSubject && !sBeeJudged) {
            fail("no photo was judged a photo of the Killer Bee");
        }
        writef(STDERR_FILENO, "[cos] picto-box: pass (%u photos)\n", sSeen);
        pc_exit(PC_EXIT_REACHED);
    }
    if (frames > kLastFrame) {
        fail("the photos were not taken");
    }
}

} // namespace pc
