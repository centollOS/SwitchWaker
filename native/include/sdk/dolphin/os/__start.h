// Forwarder (phase 2, step 2.3): the game's dolphin/os/__start.h over Aurora.
// The decomp's header describes the GameCube boot path (__start, section-init tables, the
// MetroTRK hooks, Pad3Button at a fixed address). None of it exists on the host, where Aurora owns
// the entry point (aurora/main.h, phase 3), so this forwarder deliberately declares nothing of it;
// it only keeps the name resolvable with the OS declarations the decomp header pulled in.
#ifndef COS_SDK_DOLPHIN_OS___START_H
#define COS_SDK_DOLPHIN_OS___START_H

#include <dolphin/os.h>
#include <dolphin/os/OSUtil.h>

#endif
