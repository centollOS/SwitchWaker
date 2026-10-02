#include "TRK_MINNOW_DOLPHIN/Os/dolphin/dolphin_trk_glue.h"
#include "dolphin/amcstubs/AmcExi2Stubs.h"
#include "dolphin/db/db.h"

void TRKInterruptHandler();
int Hu_IsStub(void);

u8 gWriteBuf[0x110A];
char gReadBuf[0x110A];
BOOL _MetroTRK_Has_Framing;
int gReadCount;
int gReadPos;
int gWritePos;

DBCommTable gDBCommTable = {};

void TRKEXICallBack(s16 param_0, OSContext* ctx) {
    OSEnableScheduler();
    TRKLoadContext(ctx, 0x500);
}

int InitMetroTRKCommTable(int hwId) {
    int result;
    if (hwId == HARDWARE_GDEV) {
        OSReport("MetroTRK : Set to GDEV hardware\n");
        result = Hu_IsStub();
        gDBCommTable.initialize_func = (DBCommInitFunc)DBInitComm;
        gDBCommTable.init_interrupts_func = (DBCommFunc)DBInitInterrupts;
        gDBCommTable.peek_func = (DBCommFunc)DBQueryData;
        gDBCommTable.read_func = (DBCommReadFunc)DBRead;
        gDBCommTable.write_func = (DBCommWriteFunc)DBWrite;
        gDBCommTable.post_stop_func = (DBCommFunc)DBOpen;
        gDBCommTable.pre_continue_func = (DBCommFunc)DBClose;
    } else {
        OSReport("MetroTRK : Set to AMC DDH hardware\n");
        result = AMC_IsStub();
        gDBCommTable.initialize_func = (DBCommInitFunc)EXI2_Init;
        gDBCommTable.init_interrupts_func = (DBCommFunc)EXI2_EnableInterrupts;
        gDBCommTable.peek_func = (DBCommFunc)EXI2_Poll;
        gDBCommTable.read_func = (DBCommReadFunc)EXI2_ReadN;
        gDBCommTable.write_func = (DBCommWriteFunc)EXI2_WriteN;
        gDBCommTable.post_stop_func = (DBCommFunc)EXI2_Reserve;
        gDBCommTable.pre_continue_func = (DBCommFunc)EXI2_Unreserve;
    }
    return result;
}

DSError TRKInitializeIntDrivenUART(u32 param_0, u32 param_1, u32 param_2, void* param_3) {
    gDBCommTable.initialize_func(param_3, TRKEXICallBack);
    return DS_NoError;
}

void EnableEXI2Interrupts(void) {
    gDBCommTable.init_interrupts_func();
}

DSError WriteUARTFlush(void) {
    int error = 0;
    for (; gWritePos < 0x800; gWritePos++) {
        gWriteBuf[gWritePos] = 0;
    }
    if (gWritePos != 0) {
        int writeError = gDBCommTable.write_func(gWriteBuf, gWritePos);
        error = (-writeError | writeError) >> 31;
        gWritePos = 0;
    }
    return error;
}

DSError WriteUART1(char byte) {
    gWriteBuf[gWritePos++] = byte;
    return DS_NoError;
}

static inline UARTError TRKReadUARTBuffer(void* data, u32 length) {
    int error = gDBCommTable.read_func(data, length);
    return (-error | error) >> 31;
}

UARTError TRKReadUARTPoll(char* byte) {
    UARTError error = 4;
    int cnt;

    if (gReadPos >= gReadCount) {
        gReadPos = 0;
        cnt = gReadCount = gDBCommTable.peek_func();
        if (cnt > 0) {
            if (cnt > 0x110A) {
                gReadCount = 0x110A;
            }
            error = TRKReadUARTBuffer(gReadBuf, gReadCount);
            if (error != 0) {
                gReadCount = 0;
            }
        }
    }
    if (gReadPos < gReadCount) {
        *byte = gReadBuf[gReadPos++];
        error = 0;
    }
    return error;
}

void ReserveEXI2Port(void) {
    gDBCommTable.post_stop_func();
}

void UnreserveEXI2Port(void) {
    gDBCommTable.pre_continue_func();
}

void TRK_board_display(char* str) {
    OSReport(str);
}

void TRKUARTInterruptHandler() {}

asm void TRKLoadContext(OSContext* ctx, u32) {
    // clang-format off
	nofralloc
	lwz r0, OSContext.gpr[0](r3)
	lwz r1, OSContext.gpr[1](r3)
	lwz r2, OSContext.gpr[2](r3)
	lhz r5, OSContext.state(r3)
	rlwinm. r6, r5, 0, 0x1e, 0x1e
	beq lbl_80371C1C
	rlwinm r5, r5, 0, 0x1f, 0x1d
	sth r5, OSContext.state(r3)
	lmw r5, OSContext.gpr[5](r3)
	b lbl_80371C20
lbl_80371C1C:
	lmw r13, OSContext.gpr[13](r3)
lbl_80371C20:
	mr r31, r3
	mr r3, r4
	lwz r4, OSContext.cr(r31)
	mtcrf 0xff, r4
	lwz r4, OSContext.lr(r31)
	mtlr r4
	lwz r4, OSContext.ctr(r31)
	mtctr r4
	lwz r4, OSContext.xer(r31)
	mtxer r4
	mfmsr r4
	rlwinm r4, r4, 0, 0x11, 0xf //Turn off external exceptions
	rlwinm r4, r4, 0, 0x1f, 0x1d //Turn off recoverable exception flag
	mtmsr r4
	mtsprg 1, r2
	lwz r4, OSContext.gpr[3](r31)
	mtsprg 2, r4
	lwz r4, OSContext.gpr[4](r31)
	mtsprg 3, r4
	lwz r2, OSContext.srr0(r31)
	lwz r4, OSContext.srr1(r31)
	lwz r31, OSContext.gpr[31](r31)
	b TRKInterruptHandler
    // clang-format on
}
