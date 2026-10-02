// Forwarder (phase 2, step 2.4): the game's dolphin/gba/GBAPriv.h, the GBA library's private state.
// Aurora has no GBA library, so these are the decomp's declarations, for a host GBA implementation
// to reuse; nothing on the host defines them yet. The structs keep the decomp's fields.
// GBASecParam keeps the SDK's 32-byte alignment, spelled out because the decomp's ALIGN_DECL(32)
// is empty off Metrowerks (global.h).
#ifndef COS_SDK_DOLPHIN_GBA_GBAPRIV_H
#define COS_SDK_DOLPHIN_GBA_GBAPRIV_H

#include <dolphin/dsp.h>
#include <dolphin/gba/GBA.h>
#include <dolphin/os/OS.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GBATransferCallback)(s32 chan);

typedef struct GBASecParam {
    u8 readbuf[4];
    s32 paletteColor;
    s32 paletteSpeed;
    s32 length;
    u32* out;
    u8 _padding0[12];
    u32 keyA;
    s32 keyB;
    u8 _padding1[24];
} __attribute__((aligned(32))) GBASecParam;

typedef struct GBABootInfo {
    s32 paletteColor;
    s32 paletteSpeed;
    u8* programp;
    s32 length;
    u8* status;
    GBACallback callback;
    u8 readbuf[4];
    u8 writebuf[4];
    int i;
    OSTick start;
    OSTime begin;
    int firstXfer;
    int curOffset;
    u32 crc;
    u32 dummyWord[7];
    u32 keyA;
    s32 keyB;
    u32 initialCode;
    int realLength;
} GBABootInfo;

typedef struct GBAControl {
    u8 output[5];
    u8 input[5];
    s32 outputBytes;
    s32 inputBytes;
    u8* status;
    u8* ptr;
    GBACallback callback;
    s32 ret;
    OSThreadQueue threadQueue;
    OSTime delay;
    GBATransferCallback proc;
    GBABootInfo bootInfo;
    DSPTaskInfo task;
    GBASecParam* param;
} GBAControl;

extern GBAControl __GBA[4];
extern BOOL __GBAReset;

void __GBAHandler(s32 chan, u32 error, OSContext* context);
void __GBASyncCallback(s32 chan, s32 ret);
s32 __GBASync(s32 chan);
OSTime __GBASetDelay(s32 chan, OSTime delay);
s32 __GBATransfer(s32 chan, s32 w1, s32 w2, GBATransferCallback callback);

#ifdef __cplusplus
}
#endif

#endif
