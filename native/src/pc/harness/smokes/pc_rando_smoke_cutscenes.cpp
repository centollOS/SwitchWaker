// COS_SMOKE=rando COS_RANDO_SMOKE=cutscenes (docs/RANDOMIZER_PLAN.md step R3.11): the cutscene-removal
// hunks, e.g. run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=cutscenes. Once the play scene has run
// 10 frames, first with the randomizer off (skipped when COS_RANDO_FORCE=1 forces it on), then on
// (pc_rando_set_active with the default config, unless forced):
//   1. daWarpf_c::checkEndDemo (called on a zeroed object whose stage is Forsaken Fortress, a stage
//      without a post-boss cutscene): FALSE off, TRUE on;
//   2. the pearls (item_func_pearl1/2/3): off, a pearl sets no event bit; on, each pearl sets its
//      "placed" bit (0x1410/0x1480/0x1440) and only the third raises the tower (0x1E40, 0x2E80);
//   3. check_hyrule_warp_unlocked (the Hyrule warp's test): false with 7 Triforce shards, true with 8;
//   4. daPy_lk_c::setGetItemSound on the player with param_2 (the small chest jingle): off, the sub
//      BGM does not change; on, a pearl starts the pearl music, a song the song music and another
//      item the normal item music (the wooden-chest jingle is gone);
//   5. dKankyo_DayProc (a day ends: the clock is set to 23:59, with the Wind Waker owned and the
//      room's clock running) with Helmaroc King dead and 0x1820 unset: off, no letter from Aryll or
//      Tingle; on, both letters are stocked.
// The actor-bound hunks (d_a_obj_doguu, d_a_warpdm20, d_a_obj_toripost, d_a_npc_cb1, d_a_bgn,
// d_a_npc_tt, the Killer Bees, Mrs. Marie, and the QoL-covered intro, Ballad and song replay) are
// one-expression guards listed in docs/RANDO_HOOKS.md; check 3 runs the warp's predicate.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_warpf.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "d/d_letter.h"
#include "JAZelAudio/JAZelAudio_BGM.h"
#include "m_Do/m_Do_audio.h"

#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "cutscenes";
constexpr unsigned int kDayWait = 60; // frames to wait for the day to end

enum Phase { kStart, kWaitVanillaDay, kWaitRandoDay, kTail };
Phase sPhase = kStart;
unsigned int sSince = 0;
u16 sDay = 0;
int sSavedTimePass = 0;
unsigned int sEndFrame = 0;

void check(const char* what, bool ok) {
    randoSmokeCheck(kName, what, ok);
}

dSv_player_collect_c& collect() {
    return g_dComIfG_gameInfo.save.getPlayer().getCollect();
}

BOOL warpfCheckEndDemo() {
    alignas(16) static unsigned char buf[sizeof(daWarpf_c)];
    memset(buf, 0, sizeof buf);
    daWarpf_c* warp = reinterpret_cast<daWarpf_c*>(buf);
    warp->mStageNo = dSv_save_c::STAGE_FF;
    return warp->checkEndDemo();
}

void resetPearls() {
    for (u8 i = 0; i < 3; i++) {
        collect().offSymbol(i);
    }
    static const u16 kBits[] = {0x1410, 0x1480, 0x1440, dSv_event_flag_c::UNK_1E40, dSv_event_flag_c::UNK_2E80};
    for (u16 bit : kBits) {
        dComIfGs_offEventBit(bit);
    }
}

bool anyPearlBit() {
    return dComIfGs_isEventBit(0x1410) || dComIfGs_isEventBit(0x1480) || dComIfGs_isEventBit(0x1440) ||
           dComIfGs_isEventBit(dSv_event_flag_c::UNK_1E40) || dComIfGs_isEventBit(dSv_event_flag_c::UNK_2E80);
}

u32 subBgmAfter(u16 item) {
    daPy_lk_c* link = daPy_getPlayerLinkActorClass();
    link->setGetItemSound(item, TRUE);
    return g_mDoAud_zelAudio.mSubBgmNum;
}

// Sets up a day end with Helmaroc King dead and the cutscene bit 0x1820 unset.
void startDayEnd() {
    g_dComIfG_gameInfo.save.getSavedata().getSave(dSv_save_c::STAGE_FF).getBit().onStageBossEnemy();
    dComIfGs_offEventBit(dSv_event_flag_c::UNK_1820);
    dComIfGs_onEventBit(dSv_event_flag_c::UNK_0B80); // Tingle rescued
    dComIfGs_setEventReg(dSv_event_flag_c::LETTER_ARYLL, 0);  // not sent
    dComIfGs_setEventReg(dSv_event_flag_c::LETTER_TINGLE, 0);
    dComIfGs_onGetItem(2, 0); // the Wind Waker: time passes
    // setDaytime reads the clock from the save every frame and writes it back; the room's stage data
    // stops the clock on an island (time_pass 0), so let it run for this
    sSavedTimePass = dComIfGp_roomControl_getTimePass();
    dStage_roomControl_c::SetTimePass(1);
    sDay = dComIfGs_getDate();
    dComIfGs_setTime(359.99f);
    sSince = 0;
}

bool dayEnded() {
    if (dComIfGs_getDate() != sDay) {
        dStage_roomControl_c::SetTimePass(sSavedTimePass);
        return true;
    }
    return false;
}

void vanillaChecks() {
    check("off: warpf checkEndDemo is FALSE at FF", warpfCheckEndDemo() == FALSE);
    resetPearls();
    item_func_pearl1();
    item_func_pearl2();
    item_func_pearl3();
    check("off: the pearls are owned", collect().isSymbol(0) && collect().isSymbol(1) && collect().isSymbol(2));
    check("off: the pearls set no event bit", !anyPearlBit());
    resetPearls();
    const u32 before = g_mDoAud_zelAudio.mSubBgmNum;
    check("off: setGetItemSound(pearl, TRUE) keeps the sub BGM (small jingle)",
          subBgmAfter(dItemNo_PEARL_DIN_e) == before);
}

void randoChecks() {
    check("on: pc_rando() is 1", pc_rando() == 1);
    check("on: warpf checkEndDemo is TRUE at FF", warpfCheckEndDemo() == TRUE);

    resetPearls();
    item_func_pearl1();
    check("on: Nayru's Pearl placed (0x1410)", dComIfGs_isEventBit(0x1410));
    item_func_pearl2();
    check("on: Din's Pearl placed (0x1480)", dComIfGs_isEventBit(0x1480));
    check("on: two pearls: the tower stays down",
          !dComIfGs_isEventBit(dSv_event_flag_c::UNK_1E40) && !dComIfGs_isEventBit(dSv_event_flag_c::UNK_2E80));
    item_func_pearl3();
    check("on: Farore's Pearl placed (0x1440)", dComIfGs_isEventBit(0x1440));
    check("on: three pearls: tower raised (0x1E40) and its cutscene seen (0x2E80)",
          dComIfGs_isEventBit(dSv_event_flag_c::UNK_1E40) && dComIfGs_isEventBit(dSv_event_flag_c::UNK_2E80));
    resetPearls();

    for (u8 i = 0; i < 8; i++) {
        collect().offTriforce(i);
    }
    for (u8 i = 0; i < 7; i++) {
        collect().onTriforce(i);
    }
    check("on: Hyrule warp locked with 7 shards", pc_rando_hyrule_warp_unlocked() == 0);
    collect().onTriforce(7);
    check("on: Hyrule warp unlocked with 8 shards", pc_rando_hyrule_warp_unlocked() == 1);
    for (u8 i = 0; i < 8; i++) {
        collect().offTriforce(i);
    }

    check("on: pearl get music", subBgmAfter(dItemNo_PEARL_NAYRU_e) == JA_BGM_GET_PEARL);
    check("on: song get music", subBgmAfter(dItemNo_SONG_OF_PASSING_e) == JA_BGM_GET_SONG);
    check("on: other item: normal item music, not the small jingle",
          subBgmAfter(dItemNo_PICTO_BOX_e) == JA_BGM_ITEM_GET);
    mDoAud_subBgmStop();
}

} // namespace

void randoSmokeCutscenes(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    const bool forced = pc_rando_forced() != 0;
    switch (sPhase) {
    case kStart:
        if (!forced) {
            check("off: pc_rando() is 0", pc_rando() == 0);
            vanillaChecks();
            startDayEnd();
            sPhase = kWaitVanillaDay;
        } else {
            randoChecks();
            startDayEnd();
            sPhase = kWaitRandoDay;
        }
        return;
    case kWaitVanillaDay:
        if (!dayEnded() && ++sSince < kDayWait) {
            return;
        }
        check("off: the day ended", dayEnded());
        check("off: no letter from Aryll", !dLetter_isStock(dSv_event_flag_c::LETTER_ARYLL));
        check("off: no letter from Tingle", !dLetter_isStock(dSv_event_flag_c::LETTER_TINGLE));
        pc_rando_set_active(nullptr, nullptr);
        randoChecks();
        startDayEnd();
        sPhase = kWaitRandoDay;
        return;
    case kWaitRandoDay:
        if (!dayEnded() && ++sSince < kDayWait) {
            return;
        }
        check("on: the day ended", dayEnded());
        check("on: Aryll's letter after Helmaroc King", dLetter_isStock(dSv_event_flag_c::LETTER_ARYLL));
        check("on: Tingle's letter after Helmaroc King", dLetter_isStock(dSv_event_flag_c::LETTER_TINGLE));
        if (!forced) {
            pc_rando_deactivate();
        }
        sEndFrame = frames + 20;
        sPhase = kTail;
        return;
    case kTail:
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
}

} // namespace pc
