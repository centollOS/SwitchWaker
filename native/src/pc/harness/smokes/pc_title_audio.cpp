// COS_SMOKE=title-audio (docs/NATIVE_PORT_PHASE4_6.md, step 5.5: sequences and sound effects):
// the game boots as usual; once milestone M9 title is reached (pc_title.cpp), titleAudioFrame
// (pc_frame_end, every game frame) measures what the audio did over the next kWindowFrames game
// frames:
// - the RMS level of what the AI DMA played over SDL3 in that time (cos_sdk's
//   COSAIGetOutputStats, both channels, silence included), which must be above -40 dBFS;
// - the sequence ticks JAudio ran (JASystem::getSeqTickCount, rootCallback's mainProc calls),
//   which must advance.
// It logs both with the main, sub and stream BGM sound IDs, then exits 0 (reached) or 1 (check
// failed). The probe changes no game state. COS_AUDIO=off fails at once: there is nothing to
// measure.
#include "pc_internal.h"

#include "JAZelAudio/JAIZelBasic.h"
#include "JSystem/JAudio/JAISound.h"
#include "JSystem/JAudio/JASTrack.h"
#include "cos_sdk/audio.h"

#include <cmath>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kWindowFrames = 300; // 10 s at 30 frames a second
constexpr double kMinRmsDbfs = -40.0;

bool sChecked = false; // COS_SMOKE was looked at
bool sActive = false;  // COS_SMOKE=title-audio
bool sStarted = false;
unsigned int sStartFrame = 0;
u64 sStartFrames = 0;
u64 sStartSquares = 0;
u32 sStartTicks = 0;

unsigned long soundId(JAISound* sound) {
    return sound != nullptr ? (unsigned long)sound->getID() : 0xfffffffful;
}

} // namespace

void titleAudioFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        sActive = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "title-audio") == 0;
        if (sActive && !gConfig.audio) {
            writef(STDERR_FILENO, "[cos] title-audio: COS_AUDIO is off; nothing to measure\n");
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
    }
    if (!sActive || !titleReached()) {
        return;
    }
    u64 played = 0;
    u64 squares = 0;
    COSAIGetOutputStats(&played, &squares);
    const u32 ticks = JASystem::getSeqTickCount();
    if (!sStarted) {
        sStarted = true;
        sStartFrame = frames;
        sStartFrames = played;
        sStartSquares = squares;
        sStartTicks = ticks;
        return;
    }
    if (frames - sStartFrame < kWindowFrames) {
        return;
    }

    const u64 dFrames = played - sStartFrames;
    const u64 dSquares = squares - sStartSquares;
    const u32 dTicks = ticks - sStartTicks;
    const double rms = dFrames > 0 ? std::sqrt((double)dSquares / (2.0 * (double)dFrames)) : 0.0;
    const double dbfs = rms > 0.0 ? 20.0 * std::log10(rms / 32768.0) : -999.0;
    JAIZelBasic* zel = JAIZelBasic::getInterface();
    writef(STDERR_FILENO, "[cos] title-audio: frames %u-%u after the title: %llu sample frames "
                          "played, RMS %.1f dBFS (needs > %.0f), %u sequence ticks (needs > 0); "
                          "BGM main 0x%lx sub 0x%lx stream 0x%lx\n",
           sStartFrame, frames, (unsigned long long)dFrames, dbfs, kMinRmsDbfs,
           (unsigned int)dTicks, soundId(zel != nullptr ? zel->mpMainBgmSound : nullptr),
           soundId(zel != nullptr ? zel->mpSubBgmSound : nullptr),
           soundId(zel != nullptr ? zel->mpStreamBgmSound : nullptr));
    pc_exit(dFrames > 0 && dbfs > kMinRmsDbfs && dTicks > 0 ? PC_EXIT_REACHED
                                                              : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
