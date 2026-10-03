#ifndef JASDSPCHANNEL_H
#define JASDSPCHANNEL_H

#include "dolphin/os/OSTime.h"

namespace JASystem {
    class TChannel;
#if TARGET_PC

    // The owner tag of a TDSPChannel: its owner's address (a TChannel*, or StreamLib's assign_ch
    // slot), which getLogicalChannel turns back into a pointer. Pointer-sized on the host, where
    // a u32 would truncate it.
    typedef uintptr_t TDSPChannelSign;
#endif

    namespace DSPInterface {
        class DSPBuffer;
    }

    extern OSTick history[];
    extern f32 DSP_LIMIT_RATIO;
    extern OSTick old_time;

    class TDSPChannel {
    public:
        TDSPChannel() {
            field_0xc = NULL;
            mCallback = NULL;
        }
        ~TDSPChannel() {}
        void init(u8);
#if TARGET_PC
        int allocate(TDSPChannelSign);
#else
        int allocate(u32);
#endif
        void free();
        bool forceStop();
        void forceDelete();
        void play();
        void stop();
        static void initAll();
#if TARGET_PC
        static TDSPChannel* alloc(u32, TDSPChannelSign);
        static int free(TDSPChannel*, TDSPChannelSign);
#else
        static TDSPChannel* alloc(u32, u32);
        static int free(TDSPChannel*, u32);
#endif
        static TDSPChannel* getLower();
        static TDSPChannel* getLowerActive();
        static BOOL breakLower(u8);
        static bool breakLowerActive(u8);
        static void updateAll();
        void onUpdate(u32);
        static int getNumBreak();

        u8 getNumber() { return mNumber; }
        u8 getStatus() { return mStatus; }
        u8 getPriority() { return mPriority; }
        void setPriority(u8 priority) { mPriority = priority; }
        u16 getPriorityTime() { return mPriorityTime; }
        void setPriorityTime(u16 time) { mPriorityTime = time; }
        u16 getCBInterval() { return mCBInterval; }
        void setCBInterval(u16 interval) { mCBInterval = interval; }
        void decCBInterval() {
            if (mCBInterval != 0) {
                mCBInterval--;
            }
        }
        TChannel* getLogicalChannel() {
            if (mCallback != NULL) {
                return (TChannel*)field_0x8; // ?? is this userdata?
            } else {
                return NULL;
            }
        }
        BOOL isFree() {
            if (mStatus == 1) {
                return TRUE;
            } else {
                return FALSE;
            }
        }
        // is this right?
#if TARGET_PC
        BOOL checkSign(TDSPChannelSign value) {
#else
        BOOL checkSign(u32 value) {
#endif
            if (field_0x8 == value) {
                return TRUE;
            } else {
                return FALSE;
            }
        }

        // TODO
        void forceStop(TDSPChannel*) {}
        void release(TDSPChannel*, u32) {}

        /* 0x00 */ u8 mNumber;
        /* 0x01 */ u8 mStatus;
        /* 0x02 */ u8 field_0x2;
        /* 0x03 */ u8 mPriority;
        /* 0x04 */ u16 mPriorityTime;
        /* 0x06 */ u16 mCBInterval;
#if TARGET_PC
        /* 0x08 */ TDSPChannelSign field_0x8;
#else
        /* 0x08 */ u32 field_0x8;
#endif
        /* 0x0C */ DSPInterface::DSPBuffer* field_0xc;
        /* 0x10 */ int (*mCallback)(TDSPChannel*, u32);

        static int smnFree;
        static TDSPChannel* DSPCH;
        static int smnUse;
    };
}

#endif /* JASDSPCHANNEL_H */
