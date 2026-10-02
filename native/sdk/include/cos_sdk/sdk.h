// cos_sdk: the game-specific part of the GameCube SDK over Aurora (native/sdk/README.md).
#ifndef COS_SDK_SDK_H
#define COS_SDK_SDK_H

#ifdef __cplusplus
extern "C" {
#endif

// The Aurora commit cos_sdk was built against (COS_AURORA_COMMIT in native/cmake/Aurora.cmake).
const char* COSSdkAuroraCommit(void);

#ifdef __cplusplus
}
#endif

#endif // COS_SDK_SDK_H
