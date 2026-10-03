// COS_SMOKE=bgm-hop (bug B1, docs/NATIVE_PORT_PLAN.md "Known bugs"): the island BGM after a
// house. Run with the debug stage boot on Outset, e.g. --stage sea:44:0.
//
// B1 (hardware): in Outset, after entering a house and walking back out, the island's BGM did not
// come back. The test does the same scene changes the doors make, without the door events: once
// Link is in the boot room (the M12 probe, pc_outset.cpp), bgmHopFrame (pc_frame_end, every game
// frame, between game frames) waits kSettleFrames, then
// 1. measures the island: kWindowFrames of audio output (cos_sdk's COSAIGetOutputStats, RMS in
//    dBFS) with JAIZelBasic's main BGM, which must be playing (mpMainBgmSound set);
// 2. asks for the house (dComIfGp_setNextStage, as a door's SCLS exit does: Omasao room 0 point 0,
//    Mesa's house) and after kHouseFrames checks that the PLAY scene runs it with the house BGM
//    (another main BGM, playing);
// 3. asks for the boot stage, room and point again and, kSettleFrames after the island's BGM is
//    started again, measures kWindowFrames as in 1.
// Pass: the island's main BGM is the one of step 1 and playing, the sequence ticks advance and
// the output is no more than kMaxDropDb below step 1's level. Each step logs a "[cos] bgm-hop:"
// line with the BGM state (scene, island room, main/sub/stream BGM IDs). Exit 0 or 1.
// COS_BGM_HOP_HOUSE=<stage>[:<room>[:<point>]] names another house (e.g. Onobuta, LinkRM).
#include "pc_internal.h"

#include "JAZelAudio/JAIZelBasic.h"
#include "JSystem/JAudio/JAISound.h"
#include "JSystem/JAudio/JASTrack.h"
#include "d/d_com_inf_game.h"
#include "cos_sdk/audio.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 150; // the BGM starts about 60 frames after the scene
constexpr unsigned int kWindowFrames = 150;
constexpr unsigned int kHouseFrames = 330;
constexpr unsigned int kBgmWaitFrames = 300; // for the island BGM to be started again
constexpr double kMaxDropDb = 6.0;

enum State { kOff, kWaitLink, kSettle, kMeasure, kHouse, kBack, kSettleBack, kMeasureBack };

State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
u64 sFrames0 = 0, sSquares0 = 0;
u32 sTicks0 = 0;
double sIslandDb = 0.0;
u32 sIslandBgm = 0;
char sHouse[8] = "Omasao";
int sHouseRoom = 0, sHousePoint = 0;

unsigned long soundId(JAISound* sound) {
    return sound != nullptr ? (unsigned long)sound->getID() : 0xfffffffful;
}

void logState(const char* what) {
    JAIZelBasic* z = JAIZelBasic::getInterface();
    writef(STDERR_FILENO, "[cos] bgm-hop: %s: stage %s, audio scene %d isle %d, BGM main 0x%lx "
                          "(num 0x%x) sub 0x%lx stream 0x%lx\n",
           what, dComIfGp_getStartStageName(), z->field_0x0224, (int)z->mIslandRoomNo,
           soundId(z->mpMainBgmSound), (unsigned int)z->mMainBgmNum, soundId(z->mpSubBgmSound),
           soundId(z->mpStreamBgmSound));
}

void startWindow() {
    COSAIGetOutputStats(&sFrames0, &sSquares0);
    sTicks0 = JASystem::getSeqTickCount();
}

// RMS of the window in dBFS; *ticks gets the sequence ticks it ran.
double endWindow(u32* ticks) {
    u64 frames = 0, squares = 0;
    COSAIGetOutputStats(&frames, &squares);
    *ticks = JASystem::getSeqTickCount() - sTicks0;
    const u64 dFrames = frames - sFrames0;
    const double rms =
        dFrames > 0 ? std::sqrt((double)(squares - sSquares0) / (2.0 * (double)dFrames)) : 0.0;
    return rms > 0.0 ? 20.0 * std::log10(rms / 32768.0) : -999.0;
}

void fail(const char* why) {
    logState("fail");
    writef(STDERR_FILENO, "[cos] bgm-hop: FAIL: %s\n", why);
    pc_exit(PC_EXIT_CHECK_FAILED);
}

} // namespace

void bgmHopFrame(unsigned int frames) {
    (void)frames;
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "bgm-hop") != 0) {
            return;
        }
        const PcBootStage* boot = pc_boot_stage();
        if (boot == nullptr || strcmp(boot->stage, "sea") != 0) {
            writef(STDERR_FILENO, "[cos] bgm-hop: needs COS_BOOT_STAGE on an island, e.g. "
                                  "sea:44:0 (Outset)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        if (!gConfig.audio) {
            writef(STDERR_FILENO, "[cos] bgm-hop: COS_AUDIO is off; nothing to measure\n");
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        const char* house = getenv("COS_BGM_HOP_HOUSE");
        if (house != nullptr && *house != '\0') {
            char name[8] = {};
            int n = sscanf(house, "%7[^:]:%d:%d", name, &sHouseRoom, &sHousePoint);
            if (n < 1) {
                writef(STDERR_FILENO, "[cos] bgm-hop: COS_BGM_HOP_HOUSE=\"%s\" is not "
                                      "<stage>[:<room>[:<point>]]\n", house);
                pc_exit(PC_EXIT_USAGE);
            }
            memcpy(sHouse, name, sizeof(sHouse));
        }
        sState = kWaitLink;
    }
    if (sState == kOff) {
        return;
    }
    JAIZelBasic* z = JAIZelBasic::getInterface();
    const PcBootStage* boot = pc_boot_stage();
    sSince++;
    switch (sState) {
    case kWaitLink:
        if (outsetLinkReady()) {
            sState = kSettle;
            sSince = 0;
        }
        break;
    case kSettle:
        if (sSince >= kSettleFrames) {
            if (z->mpMainBgmSound == nullptr) {
                fail("no island BGM before the house");
            }
            sIslandBgm = z->mMainBgmNum;
            startWindow();
            sState = kMeasure;
            sSince = 0;
        }
        break;
    case kMeasure:
        if (sSince >= kWindowFrames) {
            u32 ticks = 0;
            sIslandDb = endWindow(&ticks);
            logState("island");
            writef(STDERR_FILENO, "[cos] bgm-hop: island output %.1f dBFS, %u sequence ticks; "
                                  "-> %s room %d point %d\n",
                   sIslandDb, (unsigned int)ticks, sHouse, sHouseRoom, sHousePoint);
            dComIfGp_setNextStage(sHouse, (s16)sHousePoint, (s8)sHouseRoom);
            sState = kHouse;
            sSince = 0;
        }
        break;
    case kHouse:
        if (sSince >= kHouseFrames) {
            logState("house");
            if (strcmp(dComIfGp_getStartStageName(), sHouse) != 0) {
                fail("the house did not load");
            }
            if (z->mpMainBgmSound == nullptr || z->mMainBgmNum == sIslandBgm) {
                fail("no house BGM");
            }
            writef(STDERR_FILENO, "[cos] bgm-hop: -> %s room %d point %d\n", boot->stage,
                   boot->room, boot->point);
            dComIfGp_setNextStage(boot->stage, (s16)boot->point, (s8)boot->room);
            sState = kBack;
            sSince = 0;
        }
        break;
    case kBack:
        if (strcmp(dComIfGp_getStartStageName(), boot->stage) == 0 && z->mpMainBgmSound != nullptr &&
            z->mMainBgmNum == sIslandBgm)
        {
            sState = kSettleBack;
            sSince = 0;
        } else if (sSince >= kBgmWaitFrames) {
            fail("the island BGM was not started again after the house");
        }
        break;
    case kSettleBack:
        if (sSince >= kSettleFrames) {
            startWindow();
            sState = kMeasureBack;
            sSince = 0;
        }
        break;
    case kMeasureBack:
        if (sSince >= kWindowFrames) {
            u32 ticks = 0;
            const double db = endWindow(&ticks);
            logState("island again");
            writef(STDERR_FILENO, "[cos] bgm-hop: island output after the house %.1f dBFS (before "
                                  "%.1f, may drop %.0f dB at most), %u sequence ticks\n",
                   db, sIslandDb, kMaxDropDb, (unsigned int)ticks);
            if (z->mpMainBgmSound == nullptr || z->mMainBgmNum != sIslandBgm) {
                fail("the island BGM stopped again");
            }
            if (ticks == 0 || db < sIslandDb - kMaxDropDb) {
                fail("the island is quieter than before the house");
            }
            writef(STDERR_FILENO, "[cos] bgm-hop: pass\n");
            pc_exit(PC_EXIT_REACHED);
        }
        break;
    default:
        break;
    }
}

} // namespace pc
