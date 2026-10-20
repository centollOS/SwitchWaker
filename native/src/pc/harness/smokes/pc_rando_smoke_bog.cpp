// COS_SMOKE=rando COS_RANDO_SMOKE=bog (docs/RANDOMIZER_PLAN.md step R3.13): the Ballad of Gales' 10th
// warp, to Forsaken Fortress (wwrando tweaks.py add_new_bog_warp, misc_rando_features.asm 0x801B80EC),
// and the step's small helpers. Boot on the boat:
//   run.sh rando --preset sailing --env COS_RANDO_SMOKE=bog [--env COS_RANDO_FORCE=1]
// Once the play scene has run 30 frames:
//   1. on a dMenu_Fmap_c that is never constructed (the warp-table lookups touch no member): the warp
//      grid holds 10 targets (9 without the randomizer), (-3,-3) is warp 9 with left/up of warp 0 leading
//      to it; messages 848 ("Warp to Forsaken Fortress?", dialog, instant, centered, 2 lines) and 704
//      (blank, item-get box) are in the overlay (absent without); pc_rando_isTbox_unloaded(13, 5) follows
//      the saved Pirate Ship memory (the bit is restored afterwards);
//   2. the boat's cyclone is made (from the player's layer) as daPy_lk_c::procTactPlay_init does when
//      the song starts, then the real warp map is opened (dComIfGp_fmapOpenOn, what the song does next)
//      with FF's sector charted and the cursor on warp 0, and the driven stick pushes left: the cursor
//      must land on (-3,-3), or stay on warp 0 without the randomizer;
//   3. A opens the confirmation (message 848 under the randomizer; a shot is saved), A again picks Yes:
//      the map closes and the boat's tact warp number is 9 (0 without);
//   4. the boat's warp starts as procTactPlay does after the map (daShip_c::setTactWarp), and the next
//      play scene must be sea room 1, Forsaken Fortress (SCLS 0xC5 + 9), or room 9 for warp 0 without the
//      randomizer, within kWarpFrames; a shot of the arrival is saved.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_msg.h"

#include "d/actor/d_a_player.h"
#include "d/actor/d_a_ship.h"
#include "d/d_com_inf_game.h"
#include "d/d_menu_fmap.h"
#include "d/d_menu_window.h"
#include "d/d_meter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "bog";
constexpr unsigned int kOpenFrames = 90;   // the map's open animation, then some
constexpr unsigned int kStepFrames = 40;   // between two inputs
constexpr unsigned int kCloseFrames = 300; // for the map to close
constexpr unsigned int kWarpFrames = 3000; // cyclone, boat rise, stage change, arrival

enum State { kStatic, kOpen, kLeft, kLeftCheck, kConfirm, kShot, kYes, kClosed, kWarp, kDone };
State sState = kStatic;
unsigned int sSince = 0;
bool sLeftStage = false;
unsigned int sOpenAt = 0;

void shotSink(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t w, uint32_t h, void*) {
    saveFramePng(frame, rgb, w, h);
}

void staticChecks() {
    const bool rando = pc_rando() != 0;
    alignas(dMenu_Fmap_c) static unsigned char storage[sizeof(dMenu_Fmap_c)];
    dMenu_Fmap_c* fmap = reinterpret_cast<dMenu_Fmap_c*>(storage);

    int targets = 0;
    for (int y = -3; y <= 3; y++) {
        for (int x = -3; x <= 3; x++) {
            if (fmap->getWarpAreaTablePtr((s8)x, (s8)y) != nullptr) {
                targets++;
            }
        }
    }
    randoSmokeCheck(kName, rando ? "warp grid has 10 targets" : "warp grid has 9 targets", targets == (rando ? 10 : 9));
    const cursorTable_t* ff = fmap->getWarpAreaTablePtr(-3, -3);
    const cursorTable_t* w0 = fmap->getWarpAreaTablePtr(-2, -2);
    if (rando) {
        randoSmokeCheck(kName, "(-3,-3) is warp 9", ff != nullptr && ff->no == 9 && ff->right == 0 && ff->down == 0);
        randoSmokeCheck(kName, "warp 0 leads left and up to warp 9", w0 != nullptr && w0->left == 9 && w0->up == 9);
        randoSmokeCheck(kName, "warp 9's grid is (-3,-3)", fmap->getWarpAreaGridX(9) == -3 && fmap->getWarpAreaGridY(9) == -3);
    } else {
        randoSmokeCheck(kName, "(-3,-3) is no warp", ff == nullptr);
        randoSmokeCheck(kName, "warp 0 has no left or up", w0 != nullptr && w0->left == -1 && w0->up == -1);
    }

    const PcRandoMsg* m848 = pc_rando_msg_find(848);
    const PcRandoMsg* m704 = pc_rando_msg_find(704);
    if (rando) {
        randoSmokeCheck(kName, "message 848 entry",
                        m848 != nullptr && m848->has_entry && m848->entry.text_box_type == 0 &&
                            m848->entry.initial_draw_type == 1 && m848->entry.text_box_position == 2 &&
                            m848->entry.num_lines_per_box == 2);
        static char text[512];
        text[0] = '\0';
        fopMsgM_messageGet(text, 848);
        randoSmokeCheck(kName, "message 848 text", strstr(text, "Warp to ") != nullptr &&
                                                        strstr(text, "Forsaken Fortress") != nullptr);
        randoSmokeCheck(kName, "message 704 blank item-get box",
                        m704 != nullptr && m704->has_entry && m704->entry.text_box_type == 9 && m704->len == 1);
    } else {
        randoSmokeCheck(kName, "no overlay messages 848/704", m848 == nullptr && m704 == nullptr);
    }

    dSv_memBit_c& bit = g_dComIfG_gameInfo.save.getSavedata().getSave(13).getBit();
    const dSv_memBit_c saved = bit;
    const int before = pc_rando_isTbox_unloaded(13, 5);
    bit.onTbox(5);
    const int after = pc_rando_isTbox_unloaded(13, 5);
    bit = saved;
    randoSmokeCheck(kName, "isTbox_unloaded(13, 5) follows the saved Pirate Ship chest",
                    before == 0 && after == 1 && pc_rando_isTbox_unloaded(13, 5) == 0 &&
                        pc_rando_isTbox_unloaded(-1, 5) == 0 && pc_rando_isTbox_unloaded(13, 32) == 0);
}

void next(State s) {
    sState = s;
    sSince = 0;
}

} // namespace

void randoSmokeBog(unsigned int frames) {
    if (sState == kStatic) {
        if (!randoSmokeInPlay(30)) {
            return;
        }
        staticChecks();
        const bool rando = pc_rando() != 0;
        randoSmokeCheck(kName, "on the boat", dComIfGp_getShipActor() != nullptr);
        // daPy_lk_c::procTactPlay_init when the song starts on the boat at sea: the cyclone, before the map.
        // Made from the player's layer, as the player's own request is (pc_qol_probe.cpp).
        if (daShip_c* ship = dComIfGp_getShipActor(); ship != nullptr) {
            daPy_py_c* player = daPy_getPlayerActorClass();
            layer_class* savedLayer = fpcLy_CurrentLayer();
            fpcLy_SetCurrentLayer(static_cast<base_process_class*>(player)->mLyTg.mpLayer);
            ship->setTactWarpID(fopAcM_create(fpcNm_TORNADO_e, 1, &player->current.pos, fopAcM_GetRoomNo(player)));
            fpcLy_SetCurrentLayer(savedLayer);
        }
        dComIfGs_onSaveArriveGrid(0); // FF's sector, else the map offers no warp there
        dMv_CIO_c.setCurWX(-2);       // the cursor on warp 0 (Outset's column, row of FF's neighbours)
        dMv_CIO_c.setCurWY(-2);
        dComIfGp_fmapOpenOn();
        setDrivenPad(true, 0, 0, 0);
        next(kOpen);
        return;
    }
    sSince++;
    const bool rando = pc_rando() != 0;
    daShip_c* ship = dComIfGp_getShipActor();
    switch (sState) {
    case kOpen:
        if (!dMenu_flag()) {
            if (sSince > kCloseFrames) {
                randoSmokeCheck(kName, "warp map opened", false);
                randoSmokeEnd(kName);
            }
            sOpenAt = 0;
            return;
        }
        if (sOpenAt == 0) {
            sOpenAt = sSince;
        }
        if (sSince - sOpenAt >= kOpenFrames) {
            randoSmokeCheck(kName, "warp map opened", true);
            next(kLeft);
        }
        break;
    case kLeft:
        setDrivenPad(true, 0, sSince <= 4 ? -110 : 0, 0);
        if (sSince >= kStepFrames) {
            next(kLeftCheck);
        }
        break;
    case kLeftCheck: {
        const int wx = dMv_CIO_c.getCurWX();
        const int wy = dMv_CIO_c.getCurWY();
        if (rando) {
            randoSmokeCheck(kName, "left from warp 0 reaches (-3,-3)", wx == -3 && wy == -3);
            next(kConfirm);
        } else {
            randoSmokeCheck(kName, "left from warp 0 stays (no 10th warp)", wx == -2 && wy == -2);
            next(kConfirm);
        }
        break;
    }
    case kConfirm:
        setDrivenPad(true, sSince <= 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (sSince >= kStepFrames) {
            next(kShot);
        }
        break;
    case kShot:
        if (sSince == 1) {
            captureFrame(frames, shotSink, nullptr); // the confirmation box with message 848
        }
        if (sSince >= 10) {
            next(kYes);
        }
        break;
    case kYes:
        setDrivenPad(true, sSince <= 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (!dMenu_flag() && sSince > 2) {
            randoSmokeCheck(kName, rando ? "the boat's tact warp is 9" : "the boat's tact warp is 0",
                            ship != nullptr && ship->getTactWarpPosNum() == (rando ? 9 : 0));
            next(kClosed);
        } else if (sSince > kCloseFrames) {
            randoSmokeCheck(kName, "warp map closed after Yes", false);
            randoSmokeEnd(kName);
        }
        break;
    case kClosed:
        if (sSince == 10) {
            // daPy_lk_c::procTactPlay after the map closes with a warp chosen.
            if (ship == nullptr) {
                randoSmokeCheck(kName, "boat for the warp", false);
                randoSmokeEnd(kName);
            }
            ship->setTactWarp();
            next(kWarp);
        }
        break;
    case kWarp:
        if (strcmp(dComIfGp_getStartStageName(), "sea") != 0 || dComIfGp_getStartStageRoomNo() != 11) {
            sLeftStage = true;
        }
        if (sLeftStage && randoSmokeInPlay(0) && strcmp(dComIfGp_getStartStageName(), "sea") == 0 &&
            dComIfGp_getStartStageRoomNo() == (rando ? 1 : 9) && dComIfGp_getPlayer(0) != nullptr) {
            randoSmokeCheck(kName, rando ? "the warp lands on sea room 1 (Forsaken Fortress)" : "the warp lands on sea room 9 (warp 0)", true);
            next(kDone);
        } else if (sSince > kWarpFrames) {
            randoSmokeCheck(kName, "the warp lands on its sea room", false);
            randoSmokeEnd(kName);
        }
        break;
    case kDone:
        setDrivenPad(true, 0, 0, 0);
        if (sSince == 150) {
            captureFrame(frames, shotSink, nullptr); // the arrival
        }
        if (sSince >= 160) {
            randoSmokeEnd(kName);
        }
        break;
    default:
        break;
    }
}

} // namespace pc
