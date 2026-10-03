#ifndef JFILEHEADER_H
#define JFILEHEADER_H

// Note: The name of this header is fake.

#include "global.h"
#include "helpers/endian.h"

// Disc data, stored big-endian: BE(T) is T on the GameCube and a swapping field on TARGET_PC
// (phase 4, step 4.3; Dusklight declares these the same way in J3DAnimation.h).

struct JUTDataBlockHeader {
    /* 0x00 */ BE(u32) mType;
    /* 0x04 */ BE(u32) mSize;
};

struct JUTDataFileHeader { // actual struct name unknown
    /* 0x00 */ BE(u32) mMagic;
    /* 0x04 */ BE(u32) mType;
    /* 0x08 */ BE(u32) mFileSize;
    /* 0x0C */ BE(u32) mBlockNum;
    /* 0x10 */ u8 _10[0x1C - 0x10];
    /* 0x1C */ BE(u32) mSeAnmOffset; // Only exists for some BCKs
    /* 0x20 */ JUTDataBlockHeader mFirstBlock;
};

#endif /* JFILEHEADER_H */
