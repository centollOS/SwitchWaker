#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/support.h"
#include "TRK_MINNOW_DOLPHIN/utils/common/MWTrace.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "string.h"

DSError TRKSuppAccessFile(u32 file_handle, u8* data, size_t* count, u8* io_result,
                          BOOL need_reply, BOOL read) {
    DSError error;
    int replyBufferId;
    TRKBuffer* replyBuffer;
    u32 length;
    int bufferId;
    TRKBuffer* buffer;
    u32 i;
    u8 replyIOResult;
    u16 replyLength;
    BOOL exit;

    if (data == NULL || *count == 0) {
        return DS_ParameterError;
    }

    exit = FALSE;
    *io_result = DS_IONoError;
    i = 0;
    error = DS_NoError;
    while (!exit && i < *count && error == DS_NoError && *io_result == 0) {
        if (*count - i <= 0x800) {
            length = *count - i;
        } else {
            length = 0x800;
        }

        error = TRKGetFreeBuffer(&bufferId, &buffer);
        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui8(buffer, read ? DSMSG_ReadFile : DSMSG_WriteFile);
        }
        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui32(buffer, file_handle);
        }
        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui16(buffer, length);
        }

        if (!read && error == DS_NoError) {
            error = TRKAppendBuffer_ui8(buffer, data + i, length);
        }

        if (error == DS_NoError) {
            if (need_reply) {
                BOOL b;
                replyLength = 0;
                replyIOResult = 0;
                b = read && file_handle == 0;

                error = TRKRequestSend(buffer, &replyBufferId, read ? 5 : 5, 3, !b);
                if (error == DS_NoError) {
                    replyBuffer = TRKGetBuffer(replyBufferId);
                    TRKSetBufferPosition(replyBuffer, 2);
                }
                if (error == DS_NoError) {
                    error = TRKReadBuffer1_ui8(replyBuffer, &replyIOResult);
                }
                if (error == DS_NoError) {
                    error = TRKReadBuffer1_ui16(replyBuffer, &replyLength);
                }
                if (read && error == DS_NoError) {
                    if (replyBuffer->length != replyLength + 5) {
                        replyLength = replyBuffer->length - 5;
                        if (replyIOResult == DS_IONoError) {
                            replyIOResult = DS_IOError;
                        }
                    }
                    if (replyLength <= length) {
                        error = TRKReadBuffer_ui8(replyBuffer, data + i, replyLength);
                    }
                }
                if (replyLength != length) {
                    if ((!read || replyLength >= length) && replyIOResult == DS_IONoError) {
                        replyIOResult = DS_IOError;
                    }
                    length = replyLength;
                    exit = TRUE;
                }

                *io_result = (DSIOResult)replyIOResult;
                TRKReleaseBuffer(replyBufferId);
            } else {
                error = TRKMessageSend(buffer);
            }
        }

        TRKReleaseBuffer(bufferId);
        i += length;
    }

    *count = i;
    return error;
}

DSError TRKRequestSend(TRKBuffer* msgBuf, int* bufferId, u32 p1, u32 p2, int p3) {
    int error = DS_NoError;
    TRKBuffer* buffer;
    u32 counter;
    int count;
    u8 msgCmd;
    u8 msgReplyError;
    BOOL badReply = TRUE;

    *bufferId = -1;

    for (count = p2 + 1; count != 0 && *bufferId == -1 && error == DS_NoError; count--) {
        error = TRKMessageSend(msgBuf);
        if (error == DS_NoError) {
            if (p3) {
                counter = 0;
            }

            while (TRUE) {
                do {
                    *bufferId = TRKTestForPacket();
                    if (*bufferId != -1)
                        break;
                } while (!p3 || ++counter < 79999980);

                if (*bufferId == -1)
                    break;

                badReply = 0;

                buffer = TRKGetBuffer(*bufferId);
                TRKSetBufferPosition(buffer, 0);
                error = TRKReadBuffer1_ui8(buffer, &msgCmd);

                if (error != DS_NoError || msgCmd >= DSMSG_ReplyACK)
                    break;

                TRKProcessInput(*bufferId);
                *bufferId = -1;
            }

            if (*bufferId != -1) {
                if (buffer->length < p1) {
                    // OSReport("MetroTRK - bad reply size %ld\n", buffer->length);
                    badReply = TRUE;
                }
                if (error == DS_NoError && !badReply) {
                    error = TRKReadBuffer1_ui8(buffer, &msgReplyError);
                }
                if (error == DS_NoError && !badReply) {
                    if (msgCmd != DSMSG_ReplyACK || msgReplyError != DSREPLY_NoError) {
                        badReply = TRUE;
                    }
                }
                if (error != DS_NoError || badReply) {
                    TRKReleaseBuffer(*bufferId);
                    *bufferId = -1;
                }
            }
        }
    }

    if (*bufferId == -1) {
        error = DS_Error800;
    }

    return error;
}

DSError HandleOpenFileSupportRequest(const char* path, u8 mode, u32* handle,
                                     u8* ioResult) {
    DSError error;
    int replyId;
    int bufferId;
    TRKBuffer* reply;
    TRKBuffer* buffer;

    *handle = 0;
    error = TRKGetFreeBuffer(&bufferId, &buffer);
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, DSMSG_OpenFile);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, mode);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui16(buffer, strlen(path) + 1);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer_ui8(buffer, (u8*)path, strlen(path) + 1);
    }
    if (error == DS_NoError) {
        *ioResult = DS_IONoError;
        error = TRKRequestSend(buffer, &replyId, 7, 3, FALSE);
        if (error == DS_NoError) {
            reply = TRKGetBuffer(replyId);
            TRKSetBufferPosition(reply, 2);
        }
        if (error == DS_NoError) {
            error = TRKReadBuffer1_ui8(reply, ioResult);
        }
        if (error == DS_NoError) {
            error = TRKReadBuffer1_ui32(reply, handle);
        }
        TRKReleaseBuffer(replyId);
    }
    TRKReleaseBuffer(bufferId);
    return error;
}

DSError HandleCloseFileSupportRequest(int handle, u8* ioResult) {
    DSError error;
    int replyId;
    int bufferId;
    TRKBuffer* buffer;
    TRKBuffer* reply;

    error = TRKGetFreeBuffer(&bufferId, &buffer);
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, DSMSG_CloseFile);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui32(buffer, handle);
    }
    if (error == DS_NoError) {
        *ioResult = DS_IONoError;
        error = TRKRequestSend(buffer, &replyId, 3, 3, FALSE);
        if (error == DS_NoError) {
            reply = TRKGetBuffer(replyId);
            TRKSetBufferPosition(reply, 2);
        }
        if (error == DS_NoError) {
            error = TRKReadBuffer1_ui8(reply, ioResult);
        }
        TRKReleaseBuffer(replyId);
    }
    TRKReleaseBuffer(bufferId);
    return error;
}

DSError HandlePositionFileSupportRequest(DSReplyError handle, u32* position, u8 mode,
                                         u8* ioResult) {
    DSError error;
    int replyId;
    int bufferId;
    TRKBuffer* buffer;
    TRKBuffer* reply;

    error = TRKGetFreeBuffer(&bufferId, &buffer);
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, DSMSG_PositionFile);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui32(buffer, handle);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui32(buffer, *position);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, mode);
    }
    if (error == DS_NoError) {
        *ioResult = DS_IONoError;
        error = TRKRequestSend(buffer, &replyId, 3, 3, FALSE);
        if (error == DS_NoError) {
            reply = TRKGetBuffer(replyId);
            TRKSetBufferPosition(reply, 2);
        }
        if (error == DS_NoError) {
            error = TRKReadBuffer1_ui8(reply, ioResult);
        }
        if (error == DS_NoError) {
            error = TRKReadBuffer1_ui32(reply, position);
        } else {
            *position = -1;
        }
        TRKReleaseBuffer(replyId);
    }
    TRKReleaseBuffer(bufferId);
    return error;
}
