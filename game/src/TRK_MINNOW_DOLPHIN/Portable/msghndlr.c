#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msghndlr.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"
#include "trk.h"

static BOOL IsTRKConnected;

BOOL GetTRKConnected(void) {
    return IsTRKConnected;
}

void SetTRKConnected(BOOL isTRKConnected) {
    IsTRKConnected = isTRKConnected;
}

DSError TRKSendACK(TRKBuffer* buffer) {
    DSError error;
    int attempts = 3;
    do {
        error = TRKMessageSend(buffer);
        attempts--;
    } while (error != DS_NoError && attempts > 0);
    return error;
}

DSError TRKStandardACK(TRKBuffer* buffer, MessageCommandID commandID,
                      DSReplyError replyError) {
    TRKResetBuffer(buffer, TRUE);
    TRKAppendBuffer1_ui8(buffer, commandID);
    TRKAppendBuffer1_ui8(buffer, replyError);
    return TRKSendACK(buffer);
}

DSError TRKDoUnsupported(TRKBuffer* buffer) {
    return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_UnsupportedCommandError);
}

DSError TRKDoConnect(TRKBuffer* buffer) {
    IsTRKConnected = TRUE;
    return TRKStandardACK(buffer, 0x80, DSREPLY_NoError);
}

DSError TRKDoDisconnect(TRKBuffer* buffer) {
    TRKEvent event;
    DSError error;

    IsTRKConnected = FALSE;
    error = TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
    if (error == DS_NoError) {
        TRKConstructEvent(&event, 1);
        TRKPostEvent(&event);
    }
    return error;
}

DSError TRKDoReset(TRKBuffer* buffer) {
    TRKStandardACK(buffer, 0x80, DSREPLY_NoError);
    __TRK_copy_vectors();
    return DS_NoError;
}

DSError TRKDoOverride(TRKBuffer* buffer) {
    TRKStandardACK(buffer, 0x80, DSREPLY_NoError);
    __TRK_copy_vectors();
    return DS_NoError;
}

DSError TRKDoVersions(TRKBuffer* buffer) {
    DSError error;
    struct {
        u8 kernelMajor;
        u8 kernelMinor;
        u8 protocolMajor;
        u8 protocolMinor;
    } result;
    if (buffer->length != 1) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKResetBuffer(buffer, TRUE);
    TRKAppendBuffer1_ui8(buffer, DSMSG_ReplyACK);
    TRKAppendBuffer1_ui8(buffer, DSREPLY_NoError);
    error = TRKTargetVersions(&result);
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.kernelMajor);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.kernelMinor);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.protocolMajor);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.protocolMinor);
    }
    if (error != DS_NoError) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_CWDSError);
    }
    return TRKSendACK(buffer);
}

DSError TRKDoSupportMask(TRKBuffer* buffer) {
    DSError error;
    u8 result[32];
    if (buffer->length != 1) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKResetBuffer(buffer, TRUE);
    TRKAppendBuffer1_ui8(buffer, DSMSG_ReplyACK);
    TRKAppendBuffer1_ui8(buffer, DSREPLY_NoError);
    error = TRKTargetSupportMask(result);
    if (error == DS_NoError) {
        error = TRKAppendBuffer(buffer, result, sizeof(result));
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, 2);
    }
    if (error != DS_NoError) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_CWDSError);
    }
    return TRKSendACK(buffer);
}

DSError TRKDoCPUType(TRKBuffer* buffer) {
    DSError error;
    struct {
        u8 major;
        u8 minor;
        u8 bigEndian;
        u8 defaultSize;
        u8 floatingSize;
        u8 extended1Size;
        u8 extended2Size;
    } result;
    if (buffer->length != 1) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKResetBuffer(buffer, TRUE);
    TRKAppendBuffer1_ui8(buffer, DSMSG_ReplyACK);
    TRKAppendBuffer1_ui8(buffer, DSREPLY_NoError);
    error = TRKTargetCPUType(&result);
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.major);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.minor);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.bigEndian);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.defaultSize);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.floatingSize);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.extended1Size);
    }
    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buffer, result.extended2Size);
    }
    if (error != DS_NoError) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_CWDSError);
    }
    return TRKSendACK(buffer);
}

DSError TRKDoReadMemory(TRKBuffer* b) {
    u8 buf[0x800];
    u32 start;
    size_t tempLength;
    u16 length;
    u8 command;
    u8 options;
    DSError result;
    DSReplyError replyErr;

    if (b->length != 8) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKSetBufferPosition(b, 0);
    result = TRKReadBuffer1_ui8(b, &command);
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui8(b, &options);
    }
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui16(b, &length);
    }
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui32(b, &start);
    }
    if (options & DSMSGMEMORY_Extended) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }
    if (length > sizeof(buf)) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
    }
    TRKResetBuffer(b, TRUE);
    TRKAppendBuffer1_ui8(b, DSMSG_ReplyACK);
    TRKAppendBuffer1_ui8(b, DSREPLY_NoError);
    if (result == DS_NoError) {
        tempLength = length;
        result = TRKTargetAccessMemory(buf, start, &tempLength,
                                      options & DSMSGMEMORY_Userview ? 0 : 1, TRUE);
        length = tempLength;
        if (result == DS_NoError) {
            result = TRKAppendBuffer1_ui16(b, length);
        }
        if (result == DS_NoError) {
            result = TRKAppendBuffer(b, buf, tempLength);
        }
    }

    if (result != DS_NoError) {
        switch (result) {
        case DS_CWDSException:
            replyErr = DSREPLY_CWDSException;
            break;
        case DS_InvalidMemory:
            replyErr = DSREPLY_InvalidMemoryRange;
            break;
        case DS_InvalidProcessID:
            replyErr = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyErr = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyErr = DSREPLY_OSError;
            break;
        default:
            replyErr = DSREPLY_CWDSError;
            break;
        }
        return TRKStandardACK(b, DSMSG_ReplyACK, replyErr);
    }

    return TRKSendACK(b);
}

DSError TRKDoWriteMemory(TRKBuffer* b) {
    u8 buf[0x800];
    u32 start;
    size_t tempLength;
    u16 length;
    u8 command;
    u8 options;
    DSError result;
    DSReplyError replyErr;

    if (b->length <= 8) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKSetBufferPosition(b, 0);
    result = TRKReadBuffer1_ui8(b, &command);
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui8(b, &options);
    }
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui16(b, &length);
    }
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui32(b, &start);
    }
    if (options & DSMSGMEMORY_Extended) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }
    if (b->length != length + 8 || length > sizeof(buf)) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
    }
    if (result == DS_NoError) {
        tempLength = length;
        result = TRKReadBuffer(b, buf, tempLength);
        if (result == DS_NoError) {
            result = TRKTargetAccessMemory(buf, start, &tempLength,
                                          options & DSMSGMEMORY_Userview ? 0 : 1, FALSE);
        }
        length = tempLength;
    }
    if (result == DS_NoError) {
        TRKResetBuffer(b, TRUE);
        TRKAppendBuffer1_ui8(b, DSMSG_ReplyACK);
        TRKAppendBuffer1_ui8(b, DSREPLY_NoError);
    }

    if (result == DS_NoError) {
        result = TRKAppendBuffer1_ui16(b, length);
    }

    if (result != DS_NoError) {
        switch (result) {
        case DS_CWDSException:
            replyErr = DSREPLY_CWDSException;
            break;
        case DS_InvalidMemory:
            replyErr = DSREPLY_InvalidMemoryRange;
            break;
        case DS_InvalidProcessID:
            replyErr = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyErr = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyErr = DSREPLY_OSError;
            break;
        default:
            replyErr = DSREPLY_CWDSError;
            break;
        }
        return TRKStandardACK(b, DSMSG_ReplyACK, replyErr);
    }

    return TRKSendACK(b);
}

DSError TRKDoReadRegisters(TRKBuffer* b) {
    DSError error;
    DSReplyError replyError;
    size_t registersLength;
    u16 firstRegister;
    u16 lastRegister;
    u8 command;
    u8 options;

    if (b->length != 6) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKSetBufferPosition(b, 0);
    error = TRKReadBuffer1_ui8(b, &command);
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &options);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &firstRegister);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &lastRegister);
    }
    if (firstRegister > lastRegister) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_InvalidRegisterRange);
    }

    if (error == DS_NoError) {
        TRKResetBuffer(b, TRUE);
        TRKAppendBuffer1_ui8(b, DSMSG_ReplyACK);
        TRKAppendBuffer1_ui8(b, DSREPLY_NoError);
    }

    switch (options & 7) {
    case DSREG_Default:
        error = TRKTargetAccessDefault(firstRegister, lastRegister, b, &registersLength, TRUE);
        break;
    case DSREG_FP:
        error = TRKTargetAccessFP(firstRegister, lastRegister, b, &registersLength, TRUE);
        break;
    case DSREG_Extended1:
        error = TRKTargetAccessExtended1(firstRegister, lastRegister, b, &registersLength, TRUE);
        break;
    case DSREG_Extended2:
        error = TRKTargetAccessExtended2(firstRegister, lastRegister, b, &registersLength, TRUE);
        break;
    default:
        error = DS_UnsupportedError;
        break;
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            replyError = DSREPLY_UnsupportedOptionError;
            break;
        case DS_InvalidRegister:
            replyError = DSREPLY_InvalidRegisterRange;
            break;
        case DS_CWDSException:
            replyError = DSREPLY_CWDSException;
            break;
        case DS_InvalidProcessID:
            replyError = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyError = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyError = DSREPLY_OSError;
            break;
        default:
            replyError = DSREPLY_CWDSError;
        }

        return TRKStandardACK(b, DSMSG_ReplyACK, replyError);
    } else {
        return TRKSendACK(b);
    }
}

DSError TRKDoWriteRegisters(TRKBuffer* b) {
    DSError error;
    DSReplyError replyError;
    size_t registersLength;
    u16 firstRegister;
    u16 lastRegister;
    u8 command;
    u8 options;

    if (b->length <= 6) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKSetBufferPosition(b, 0);
    error = TRKReadBuffer1_ui8(b, &command);
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &options);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &firstRegister);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &lastRegister);
    }
    if (firstRegister > lastRegister) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_InvalidRegisterRange);
    }

    switch (options) {
    case DSREG_Default:
        error = TRKTargetAccessDefault(firstRegister, lastRegister, b, &registersLength, FALSE);
        break;
    case DSREG_FP:
        error = TRKTargetAccessFP(firstRegister, lastRegister, b, &registersLength, FALSE);
        break;
    case DSREG_Extended1:
        error = TRKTargetAccessExtended1(firstRegister, lastRegister, b, &registersLength, FALSE);
        break;
    case DSREG_Extended2:
        error = TRKTargetAccessExtended2(firstRegister, lastRegister, b, &registersLength, FALSE);
        break;
    default:
        error = DS_UnsupportedError;
        break;
    }

    if (error == DS_NoError) {
        TRKResetBuffer(b, TRUE);
        TRKAppendBuffer1_ui8(b, DSMSG_ReplyACK);
        TRKAppendBuffer1_ui8(b, DSREPLY_NoError);
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            replyError = DSREPLY_UnsupportedOptionError;
            break;
        case DS_InvalidRegister:
            replyError = DSREPLY_InvalidRegisterRange;
            break;
        case DS_MessageBufferReadError:
            replyError = DSREPLY_PacketSizeError;
            break;
        case DS_CWDSException:
            replyError = DSREPLY_CWDSException;
            break;
        case DS_InvalidProcessID:
            replyError = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyError = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyError = DSREPLY_OSError;
            break;
        default:
            replyError = DSREPLY_CWDSError;
        }

        return TRKStandardACK(b, DSMSG_ReplyACK, replyError);
    } else {
        return TRKSendACK(b);
    }
}

DSError TRKDoFlushCache(TRKBuffer* buffer) {
    DSError error;
    DSReplyError replyError;
    u32 start;
    u32 end;
    u8 command;
    u8 options;

    if (buffer->length != 10) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &command);
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(buffer, &options);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui32(buffer, &start);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui32(buffer, &end);
    }
    if (start > end) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_InvalidMemoryRange);
    }
    if (error == DS_NoError) {
        error = TRKTargetFlushCache(options, start, end);
    }
    if (error == DS_NoError) {
        TRKResetBuffer(buffer, TRUE);
        TRKAppendBuffer1_ui8(buffer, DSMSG_ReplyACK);
        TRKAppendBuffer1_ui8(buffer, DSREPLY_NoError);
    }
    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            replyError = DSREPLY_UnsupportedOptionError;
            break;
        default:
            replyError = DSREPLY_CWDSError;
            break;
        }
        return TRKStandardACK(buffer, DSMSG_ReplyACK, replyError);
    }
    return TRKSendACK(buffer);
}

DSError TRKDoContinue(TRKBuffer* buffer) {
    DSError error;
    if (!TRKTargetStopped()) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NotStopped);
    } else {
        error = TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
        if (error == DS_NoError) {
            error = TRKTargetContinue();
        }
        return error;
    }
}

DSError TRKDoStep(TRKBuffer* b) {
    DSError result;
    u8 command;
    u8 options;
    u8 count;
    u32 rangeStart;
    u32 rangeEnd;
    u32 pc;
    if (b->length < 3) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }
    TRKSetBufferPosition(b, 0);
    result = TRKReadBuffer1_ui8(b, &command);
    if (result == DS_NoError) {
        result = TRKReadBuffer1_ui8(b, &options);
    }

    switch (options) {
    case DSSTEP_IntoCount:
    case DSSTEP_OverCount:
        if (result == DS_NoError) {
            result = TRKReadBuffer1_ui8(b, &count);
        }
        if (count >= 1) {
            break;
        }
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
    case DSSTEP_IntoRange:
    case DSSTEP_OverRange:
        if (b->length != 10) {
            return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
        }
        if (result == DS_NoError) {
            result = TRKReadBuffer1_ui32(b, &rangeStart);
        }
        if (result == DS_NoError) {
            result = TRKReadBuffer1_ui32(b, &rangeEnd);
        }
        pc = TRKTargetGetPC();
        if (pc >= rangeStart && pc <= rangeEnd) {
            break;
        }
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
    default:
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    if (!TRKTargetStopped()) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NotStopped);
    } else {
        result = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);
        if (result == DS_NoError) {
            switch (options) {
            case DSSTEP_IntoCount:
            case DSSTEP_OverCount:
                result = TRKTargetSingleStep(count, (options == DSSTEP_OverCount));
                break;
            case DSSTEP_IntoRange:
            case DSSTEP_OverRange:
                result = TRKTargetStepOutOfRange(rangeStart, rangeEnd, (options == DSSTEP_OverRange));
                break;
            }
        }

        return result;
    }
}

DSError TRKDoStop(TRKBuffer* b) {
    DSReplyError replyError;

    switch (TRKTargetStop()) {
    case DS_NoError:
        replyError = DSREPLY_NoError;
        break;
    case DS_InvalidProcessID:
        replyError = DSREPLY_InvalidProcessID;
        break;
    case DS_InvalidThreadID:
        replyError = DSREPLY_InvalidThreadID;
        break;
    case DS_OSError:
        replyError = DSREPLY_OSError;
        break;
    default:
        replyError = DSREPLY_Error;
        break;
    }

    return TRKStandardACK(b, DSMSG_ReplyACK, replyError);
}

DSError TRKDoSetOption(TRKBuffer* buffer) {
    DSError error;
    u8 command = 0;
    u8 option = 0;
    u8 value = 0;

    TRKSetBufferPosition(buffer, 0);
    error = TRKReadBuffer1_ui8(buffer, &command);
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(buffer, &option);
    }
    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(buffer, &value);
    }
    if (error != DS_NoError) {
        TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_Error);
    } else if (option == 1) {
        SetUseSerialIO(value);
    }
    return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
}
