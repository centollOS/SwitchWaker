// Forwarder (phase 2, step 2.4): the game's dolphin/exi/EXIBios.h over Aurora's <dolphin/exi.h>, which
// declares the device and frequency constants, EXICallback, EXIControl and the EXI API (Aurora
// has no EXI library; the host definitions come with the SDK layer). Added here: the decomp's
// EXI_STATE_* flags. Left out on purpose: __EXIRegs (the EXI registers, defined in the decomp's
// header at a fixed address) and the file-static UnlockedHandler. Aurora declares EXIImm and
// EXIImmEx returning BOOL where the decomp has s32; Aurora's wins.
#ifndef COS_SDK_DOLPHIN_EXI_EXIBIOS_H
#define COS_SDK_DOLPHIN_EXI_EXIBIOS_H

#include <dolphin/exi.h>
#include <dolphin/os/OSUtil.h>

#define EXI_STATE_IDLE 0x00
#define EXI_STATE_DMA 0x01
#define EXI_STATE_IMM 0x02
#define EXI_STATE_BUSY (EXI_STATE_DMA | EXI_STATE_IMM)
#define EXI_STATE_SELECTED 0x04
#define EXI_STATE_ATTACHED 0x08
#define EXI_STATE_LOCKED 0x10

#endif
