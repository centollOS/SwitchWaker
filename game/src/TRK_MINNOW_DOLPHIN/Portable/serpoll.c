#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/serpoll.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"
#include "trk.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"

static TRKFramingState gTRKFramingState;

void* gTRKInputPendingPtr;

extern UARTError TRKReadUARTPoll(char* byte);
extern DSError TRKStandardACK(TRKBuffer*, MessageCommandID, DSReplyError);

static inline void TRKResetFraming(void) {
    if (gTRKFramingState.msgBufID != -1) {
        TRKReleaseBuffer(gTRKFramingState.msgBufID);
        gTRKFramingState.msgBufID = -1;
    }
    gTRKFramingState.buffer = NULL;
    gTRKFramingState.receiveState = DSRECV_Wait;
}

static inline BOOL TRKValidatePacket(TRKBuffer* msg) {
    if (msg->length < 2) {
        TRKStandardACK(msg, (MessageCommandID)0xFF, DSREPLY_PacketSizeError);
        TRKResetFraming();
        return FALSE;
    }
    msg->position = 0;
    msg->length--;
    return TRUE;
}

MessageBufferID TRKTestForPacket() {
    char byte;
    DSError error = DS_NoError;
    UARTError uartError = TRKReadUARTPoll(&byte);
    while (uartError == UART_NoError && error == DS_NoError) {
        if (gTRKFramingState.receiveState != DSRECV_InFrame) {
            gTRKFramingState.isEscape = FALSE;
        }
        switch (gTRKFramingState.receiveState) {
        case DSRECV_Wait:
            if (byte == 0x7E) {
                error = TRKGetFreeBuffer(&gTRKFramingState.msgBufID, &gTRKFramingState.buffer);
                gTRKFramingState.fcsType = 0;
                gTRKFramingState.receiveState = DSRECV_Found;
            }
            break;
        case DSRECV_Found:
            if (byte == 0x7E)
                break;
            gTRKFramingState.receiveState = DSRECV_InFrame;
            // Fall through to process the first byte in the frame.
        case DSRECV_InFrame:
            if (byte == 0x7E) {
                if (gTRKFramingState.isEscape) {
                    TRKStandardACK(gTRKFramingState.buffer, (MessageCommandID)0xFF, DSREPLY_EscapeError);
                    TRKResetFraming();
                    break;
                }
                if (TRKValidatePacket(gTRKFramingState.buffer)) {
                    MessageBufferID id = gTRKFramingState.msgBufID;
                    gTRKFramingState.msgBufID = -1;
                    gTRKFramingState.buffer = NULL;
                    gTRKFramingState.receiveState = DSRECV_Wait;
                    return id;
                }
                gTRKFramingState.receiveState = DSRECV_Wait;
            } else {
                if (gTRKFramingState.isEscape) {
                    byte ^= 0x20;
                    gTRKFramingState.isEscape = FALSE;
                } else if (byte == 0x7D) {
                    gTRKFramingState.isEscape = TRUE;
                    break;
                }
                error = TRKAppendBuffer1_ui8(gTRKFramingState.buffer, byte);
                gTRKFramingState.fcsType += byte;
            }
            break;
        case DSRECV_FrameOverflow:
            if (byte == 0x7E)
                TRKResetFraming();
            break;
        }
        uartError = TRKReadUARTPoll(&byte);
    }
    return -1;
}

void TRKGetInput(void) {
    TRKBuffer* msg;
    MessageBufferID id = TRKTestForPacket();
    if (id != -1) {
        u8 command;
        msg = TRKGetBuffer(id);
        TRKSetBufferPosition(msg, 0);
        TRKReadBuffer1_ui8(msg, &command);
        if (command < 0x80) {
            TRKEvent event;
            TRKConstructEvent(&event, NUBEVENT_Request);
            event.msgBufID = id;
            gTRKFramingState.msgBufID = -1;
            TRKPostEvent(&event);
        } else {
            TRKReleaseBuffer(id);
        }
    }
}

void TRKProcessInput(int bufferIdx) {
    TRKEvent event;

    TRKConstructEvent(&event, NUBEVENT_Request);
    event.msgBufID = bufferIdx;
    gTRKFramingState.msgBufID = -1;
    TRKPostEvent(&event);
}

DSError TRKInitializeSerialHandler() {
    gTRKFramingState.msgBufID = -1;
    gTRKFramingState.receiveState = DSRECV_Wait;
    gTRKFramingState.isEscape = FALSE;


    return DS_NoError;
}

DSError TRKTerminateSerialHandler(void) {
    return DS_NoError;
}
