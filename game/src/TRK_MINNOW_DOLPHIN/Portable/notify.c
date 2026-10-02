#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/notify.h"
#include "trk.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"

DSError TRKDoNotifyStopped(MessageCommandID cmd) {
    int reqIdx;
    int bufIdx;
    TRKBuffer* msg;
    DSError err;
    DSError bufError;

    bufError = TRKGetFreeBuffer(&bufIdx, &msg);
    if ((err = bufError) == FALSE) {
        err = TRKAppendBuffer1_ui8(msg, cmd);
        if (err == DS_NoError) {
            if ((u8)cmd == DSMSG_NotifyStopped) {
                TRKTargetAddStopInfo(msg);
            } else {
                TRKTargetAddExceptionInfo(msg);
            }
        }
        bufError = TRKRequestSend(msg, &reqIdx, 2, 3, 1);
        err = bufError;
        if (err == DS_NoError) {
            TRKReleaseBuffer(reqIdx);
        }
        TRKReleaseBuffer(bufIdx);
    }
    return err;
}
