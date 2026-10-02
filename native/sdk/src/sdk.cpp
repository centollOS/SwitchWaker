// cos_sdk: library-level information. The SDK functions themselves live in the subdirectories of
// native/sdk/src (one per SDK library), added by the phase 2 steps.
#include "cos_sdk/sdk.h"

#ifndef COS_AURORA_COMMIT_STR
#error "COS_AURORA_COMMIT_STR must be set by native/cmake/sdk.cmake"
#endif

extern "C" const char* COSSdkAuroraCommit(void) {
    return COS_AURORA_COMMIT_STR;
}
