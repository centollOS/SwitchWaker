/**
 * msg.c
 * Description:
 */

#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msg.h"
#include "trk.h"

extern DSError WriteUART1(char byte);
extern DSError WriteUARTFlush(void);

DSError TRKMessageSend(TRKBuffer* msg) {
    DSError error;
    u8 checksum;
    int i;
    u8 byte;
    u8 acc = 0;
    int n;

    for (n = 0; n < msg->length; n++) {
        acc = (u8)(acc + msg->data[n]);
    }
    checksum = acc ^ 0xFF;

    error = WriteUART1(0x7E);
    if (error == DS_NoError) {
        for (i = 0; i < msg->length; i++) {
            byte = msg->data[i];
            if (byte == 0x7E || byte == 0x7D) {
                error = WriteUART1(0x7D);
                byte ^= 0x20;
                if (error != DS_NoError) {
                    break;
                }
            }
            error = WriteUART1(byte);
            if (error != DS_NoError) {
                break;
            }
        }
    }
    if (error == DS_NoError) {
        u8 byte = checksum;
        for (i = 0; i < 1; i++) {
            if ((u8)checksum == 0x7E || (u8)checksum == 0x7D) {
                error = WriteUART1(0x7D);
                byte ^= 0x20;
                if (error != DS_NoError) {
                    break;
                }
            }
            error = WriteUART1(byte);
            if (error != DS_NoError) {
                break;
            }
        }
    }
    if (error == DS_NoError) {
        error = WriteUART1(0x7E);
    }
    if (error == DS_NoError) {
        error = WriteUARTFlush();
    }
    return error;
}
