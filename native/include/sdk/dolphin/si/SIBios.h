// Forwarder (phase 2, step 2.4): the game's dolphin/si/SIBios.h over Aurora's <dolphin/si.h>, which
// declares the channel, error, type and WaveBird constants and SIProbe (aurora_si implements only
// SIProbe). Added here, as the decomp declares them: the COMCSR register constants, SICallback,
// SITypeCallback, SIPacket, SIControl, SIComm and the rest of the SI API (nothing on the host
// defines those functions yet; JUTGba only calls SIProbe).
//
// Left out on purpose: __SIRegs (the SI registers, defined in the decomp's header at a fixed
// address) and the decomp's file-static SIInterruptHandler, SIEnablePollingInterrupt and
// SIGetResponseRaw.
#ifndef COS_SDK_DOLPHIN_SI_SIBIOS_H
#define COS_SDK_DOLPHIN_SI_SIBIOS_H

#include <dolphin/si.h>
#include <dolphin/os/OSInterrupt.h>
#include <dolphin/os/OSTime.h>
#include <dolphin/os/OSUtil.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SI_MAX_TYPE 4

#define SI_COMCSR_IDX 13
#define SI_STATUS_IDX 14

#define SI_COMCSR_TCINT_MASK       (1 << 31)
#define SI_COMCSR_TCINTMSK_MASK    (1 << 30)
#define SI_COMCSR_COMERR_MASK      (1 << 29)
#define SI_COMCSR_RDSTINT_MASK     (1 << 28)
#define SI_COMCSR_RDSTINTMSK_MASK  (1 << 27)

// Same text as the decomp (unparenthesized, as there).
// 4 bits of padding
#define SI_COMCSR_OUTLNGTH_MASK    (1 << 22) \
                                 | (1 << 21) \
                                 | (1 << 20) \
                                 | (1 << 19) \
                                 | (1 << 18) \
                                 | (1 << 17) \
                                 | (1 << 16)

// 1 bit of padding
#define SI_COMCSR_INLNGTH_MASK     (1 << 14) \
                                 | (1 << 13) \
                                 | (1 << 12) \
                                 | (1 << 11) \
                                 | (1 << 10) \
                                 | (1 << 9)  \
                                 | (1 << 8)

// 5 bits of padding
#define SI_COMCSR_CHANNEL_MASK     (1 << 2) \
                                 | (1 << 1)

#define SI_COMCSR_TSTART_MASK      (1 << 0)

#define SI_MAX_COMCSR_INLNGTH 128
#define SI_MAX_COMCSR_OUTLNGTH 128

typedef void (*SICallback)(s32 chan, u32 sr, OSContext* context);
typedef void (*SITypeCallback)(s32 chan, u32 type);

typedef struct SIPacket {
    s32 chan;
    void* output;
    u32 outputBytes;
    void* input;
    u32 inputBytes;
    SICallback callback;
    OSTime fire;
} SIPacket;

typedef struct SIControl {
    s32 chan;
    u32 poll;
    u32 inputBytes;
    void* input;
    SICallback callback;
} SIControl;

typedef struct SIComm_s {
    u32 tcint : 1;
    u32 tcintmsk : 1;
    u32 comerr : 1;
    u32 rdstint : 1;
    u32 rdstintmsk : 1;
    u32 pad0 : 4;
    u32 outlngth : 7;
    u32 pad1 : 1;
    u32 inlngth : 7;
    u32 pad2 : 5;
    u32 channel : 2;
    u32 tstart : 1;
} SIComm_s;

typedef union SIComm_u {
    u32 val;
    SIComm_s f;
} SIComm_u;

BOOL SIBusy(void);
BOOL SIIsChanBusy(s32 chan);
BOOL SIRegisterPollingHandler(__OSInterruptHandler handler);
BOOL SIUnregisterPollingHandler(__OSInterruptHandler handler);
void SIInit(void);
u32 SIGetStatus(s32 chan);
void SISetCommand(s32 chan, u32 command);
void SITransferCommands(void);
u32 SISetXY(u32 x, u32 y);
u32 SIEnablePolling(u32 poll);
u32 SIDisablePolling(u32 poll);
BOOL SIGetResponse(s32 chan, void* data);
BOOL SITransfer(s32 chan, void* output, u32 outputBytes, void* input, u32 inputBytes,
                SICallback callback, OSTime delay);
u32 SIGetType(s32 chan);
u32 SIGetTypeAsync(s32 chan, SITypeCallback callback);

#ifdef __cplusplus
}
#endif

#endif
