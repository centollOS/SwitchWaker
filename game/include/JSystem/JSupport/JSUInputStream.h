#ifndef JSUINPUTSTREAM_H
#define JSUINPUTSTREAM_H

#include "JSystem/JSupport/JSUIosBase.h"
#include "helpers/endian.h"

enum JSUStreamSeekFrom {
    JSUStreamSeekFrom_SET = 0,  // absolute
    JSUStreamSeekFrom_CUR = 1,  // relative
    JSUStreamSeekFrom_END = 2,  // relative to end
};

// The 16- and 32-bit reads return host values of the big-endian stream data (BLO screens, step 4.13
// of docs/NATIVE_PORT_PHASE4_6.md): BE(T) is T on the original target. As in Dusklight 40457c6
// (CC0), libs/JSystem/include/JSystem/JSupport/JSUInputStream.h.
class JSUInputStream : public JSUIosBase {
public:
    JSUInputStream() {}
    virtual ~JSUInputStream();

    /* vt[3] */ virtual s32 getAvailable() const = 0;
    /* vt[4] */ virtual s32 skip(s32);
    /* vt[5] */ virtual u32 readData(void*, s32) = 0;

    u32 readU32() {
        BE(u32) val;
        this->read(&val, sizeof(val));
        return val;
    }

    u32 read32b() {
        BE(u32) val;
        this->read(&val, sizeof(val));
        return val;
    }

    s32 readS32() {
        BE(s32) val;
        this->read(&val, sizeof(val));
        return val;
    }

    s16 readS16() {
        BE(s16) val;
        this->read(&val, sizeof(val));
        return val;
    }

    u16 readU16() {
        BE(u16) val;
        this->read(&val, sizeof(val));
        return val;
    }

    u8 readU8() {
        u8 val;
        this->read(&val, sizeof(val));
        return val;
    }

    u8 read8b() {
        u8 val;
        this->read(&val, sizeof(val));
        return val;
    }

    u16 read16b() {
        BE(u16) val;
        this->read(&val, sizeof(val));
        return val;
    }

    // TODO: return value probably wrong
    s32 read(void*, s32);
};  // Size = 0x8

// move?
template <typename T>
T* JSUConvertOffsetToPtr(const void*, const void*);

#endif /* JSUINPUTSTREAM_H */
