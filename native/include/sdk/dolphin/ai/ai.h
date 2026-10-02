// Forwarder (phase 2, step 2.4): the game's dolphin/ai/ai.h over Aurora's <dolphin/ai.h>, which
// declares the whole AI API (AIInitDMA takes a uintptr_t under TARGET_PC). Added here: the SDK's
// internal __AI_set_stream_sample_rate, the one name Aurora lacks.
#ifndef COS_SDK_DOLPHIN_AI_AI_H
#define COS_SDK_DOLPHIN_AI_AI_H

#include <dolphin/ai.h>

#ifdef __cplusplus
extern "C" {
#endif

void __AI_set_stream_sample_rate(u32 rate);

#ifdef __cplusplus
}
#endif

#endif
