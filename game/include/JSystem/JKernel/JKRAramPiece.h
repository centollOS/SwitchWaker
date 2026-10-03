#ifndef JKRARAMPIECE_H
#define JKRARAMPIECE_H

#include "JSystem/JSupport/JSUList.h"
#include "dolphin/ar/arq.h"
#include "dolphin/os/OSMessage.h"
#include "dolphin/os/OSMutex.h"

class JKRAramBlock;
class JKRDecompCommand;
class JKRAMCommand {
public:
#if TARGET_PC
    // The callback receives the command's address (a host pointer), which a u32 would truncate.
    typedef void (*AsyncCallback)(uintptr_t);
#else
    typedef void (*AsyncCallback)(u32);
#endif

    JKRAMCommand();
    ~JKRAMCommand();

public:
    /* 0x00 */ ARQRequest mRequest;
    /* 0x20 */ JSULink<JKRAMCommand> mPieceLink;
    /* 0x30 */ JSULink<JKRAMCommand> field_0x30;

    /* 0x40 */ s32 mTransferDirection;
    /* 0x44 */ u32 mDataLength;
#if TARGET_PC
    // One side of a transfer is main memory, a host pointer; the other an ARAM offset (as in
    // Dusklight's JKRAramPiece.h, CC0, ref/dusklight at 40457c6). Aurora's ARQPostRequest takes
    // both as uintptr_t.
    /* 0x48 */ uintptr_t mSrc;
    /* 0x4C */ uintptr_t mDst;
#else
    /* 0x48 */ u32 mSrc;
    /* 0x4C */ u32 mDst;
#endif
    /* 0x50 */ JKRAramBlock* mAramBlock;
    /* 0x54 */ u32 field_0x54;
    /* 0x58 */ AsyncCallback mCallback;
    /* 0x5C */ OSMessageQueue* field_0x5C;
    /* 0x60 */ s32 field_0x60;
    /* 0x64 */ JKRDecompCommand* mDecompCommand;
    /* 0x68 */ OSMessageQueue mMessageQueue;
    /* 0x88 */ OSMessage mMessage;
    /* 0x8C */ void* field_0x8C;
    /* 0x90 */ void* field_0x90;
    /* 0x94 */ void* field_0x94;
};

class JKRAramPiece {
public:
    static OSMutex mMutex;
    // TODO: fix type
    static JSUList<JKRAMCommand> sAramPieceCommandList;

public:
    struct Message {
        s32 field_0x00;
        JKRAMCommand* command;
    };

public:
#if TARGET_PC
    // Source and destination: a main-memory host pointer or an ARAM offset (see mSrc).
    static JKRAMCommand* prepareCommand(int, uintptr_t, uintptr_t, u32, JKRAramBlock*,
                                        JKRAMCommand::AsyncCallback);
#else
    static JKRAMCommand* prepareCommand(int, u32, u32, u32, JKRAramBlock*,
                                        JKRAMCommand::AsyncCallback);
#endif
    static void sendCommand(JKRAMCommand*);

#if TARGET_PC
    static JKRAMCommand* orderAsync(int, uintptr_t, uintptr_t, u32, JKRAramBlock*, JKRAMCommand::AsyncCallback);
    static BOOL sync(JKRAMCommand*, int);
    static BOOL orderSync(int, uintptr_t, uintptr_t, u32, JKRAramBlock*);
#else
    static JKRAMCommand* orderAsync(int, u32, u32, u32, JKRAramBlock*, JKRAMCommand::AsyncCallback);
    static BOOL sync(JKRAMCommand*, int);
    static BOOL orderSync(int, u32, u32, u32, JKRAramBlock*);
#endif
    static void startDMA(JKRAMCommand*);
#if TARGET_PC
    // Aurora's ARQCallback takes the request address as uintptr_t (a host pointer).
    static void doneDMA(uintptr_t);
#else
    static void doneDMA(u32);
#endif

private:
    static void lock() { OSLockMutex(&mMutex); }
    static void unlock() { OSUnlockMutex(&mMutex); }
};

#if TARGET_PC
inline BOOL JKRAramPcs(int direction, uintptr_t source, uintptr_t destination, u32 length,
                       JKRAramBlock* block) {
#else
inline BOOL JKRAramPcs(int direction, u32 source, u32 destination, u32 length,
                       JKRAramBlock* block) {
#endif
    return JKRAramPiece::orderSync(direction, source, destination, length, block);
}

#endif /* JKRARAMPIECE_H */
