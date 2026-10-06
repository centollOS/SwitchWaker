// cos_sdk: what the AI library (src/audio/AI.cpp) tells the harness beyond the SDK's
// <dolphin/ai.h>. Not part of the SDK; nothing in the game calls it.
//
// Step 5.5 (docs/NATIVE_PORT_PHASE4_6.md): the COS_SMOKE=title-audio test
// (native/src/pc/harness/smokes/pc_title_audio.cpp) measures the level of what the AI DMA played.
#ifndef COS_SDK_AUDIO_H
#define COS_SDK_AUDIO_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// The sample frames the AI DMA engine played since the process started (silence included: the
// engine runs from AIInit on) and the sum of the squares of both channels' s16 samples. The RMS
// level over an interval is sqrt(d(sumSquares) / (2 * d(frames))).
void COSAIGetOutputStats(u64* frames, u64* sumSquares);

#ifdef __cplusplus
}
#endif

#endif // COS_SDK_AUDIO_H
