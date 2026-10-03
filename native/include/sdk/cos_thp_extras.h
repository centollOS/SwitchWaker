// The game THP player types Aurora's <dolphin/thp.h> lacks.
//
// Phase 2, step 2.7 (d-core; docs/NATIVE_PORT_PHASE2_3.md, decision D2). The name dolphin/thp.h is
// both the game's and Aurora's, so Aurora's wins and no forwarder can sit in front of it. The game's thp.h
// holds only the player's types (the decoder itself is the game's own, static in
// d_a_movie_player.cpp); Aurora's declares only THPInit, THPVideoDecode and an extern "C"
// THPAudioDecode(s16*, const u8*, s32), which would clash with the game's static THPAudioDecode.
// So d_a_movie_player.h includes this header *instead of* dolphin/thp.h under
// `TARGET_PC && defined(COS_SDK_AURORA)`. Every type below is a copy of the game's
// include/dolphin/thp.h; none of these names is declared by Aurora.
// TODO(native phase 2.8): include it unconditionally once COS_SDK_HEADERS=decomp is gone.
// TODO(native phase 4): THPHeader/THPVideoInfo/THPAudioInfo/THPFrameCompInfo are read straight
// from big-endian .thp data, and THPFileInfo/THPAudioDecodeInfo hold pointers (Dusklight's
// d_a_movie_player.h uses BE(u32) fields for the former).
#ifndef COS_THP_EXTRAS_H
#define COS_THP_EXTRAS_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef u8 THPSample;
typedef s16 THPCoeff;
typedef f32 THPQuantTab[64];

typedef struct _THPHuffmanTab {
    u8 quick[32];
    u8 increment[32];
    u8* Vij;
    s32 maxCode[18];
    s32 valPtr[18];
    u8 Vij1;
    u8 pad[11];
} THPHuffmanTab;

typedef struct _THPComponent {
    u8 quantizationTableSelector;
    u8 DCTableSelector;
    u8 ACTableSelector;
    THPCoeff predDC;
} THPComponent;

typedef struct _local_THPFileInfo {
    THPQuantTab quantTabs[3];
    THPHuffmanTab huffmanTabs[4];
    THPComponent components[3];
    u16 xPixelSize;
    u16 yPixelSize;
    u16 MCUsPerRow;
    u16 decompressedY;
    u8* c;
    u32 currByte;
    u32 cnt;
    u8 validHuffmanTabs;
    u8 RST;
    u16 nMCU;
    u16 currMCU;
    u8* dLC[3];
} THPFileInfo;

typedef struct THPAudioRecordHeader {
    u32 offsetNextChannel;
    u32 sampleSize;
    s16 lCoef[8][2];
    s16 rCoef[8][2];
    s16 lYn1;
    s16 lYn2;
    s16 rYn1;
    s16 rYn2;
} THPAudioRecordHeader;

typedef struct THPAudioDecodeInfo {
    u8* encodeData;
    u32 offsetNibbles;
    u8 predictor;
    u8 scale;
    s16 yn1;
    s16 yn2;
} THPAudioDecodeInfo;

typedef struct THPTextureSet {
	u8* ytexture;
	u8* utexture;
	u8* vtexture;
	s32 frameNumber;
} THPTextureSet;

typedef struct THPAudioBuffer {
	s16* buffer;
	s16* curPtr;
	u32 validSample;
} THPAudioBuffer;

typedef struct THPVideoInfo {
	u32 xSize;
	u32 ySize;
	u32 videoType;
} THPVideoInfo;

typedef struct THPAudioInfo {
	u32 sndChannels;
	u32 sndFrequency;
	u32 sndNumSamples;
	u32 sndNumTracks;
} THPAudioInfo;

typedef struct THPFrameCompInfo {
	u32 numComponents;
	u8 frameComp[16];
} THPFrameCompInfo;

typedef struct THPHeader {
	/* 0x00 */ char magic[4];
	/* 0x04 */ u32 version;
	/* 0x08 */ u32 bufsize;
	/* 0x0C */ u32 audioMaxSamples;
	/* 0x10 */ f32 frameRate;
	/* 0x14 */ u32 numFrames;
	/* 0x18 */ u32 firstFrameSize;
	/* 0x1C */ u32 movieDataSize;
	/* 0x20 */ u32 compInfoDataOffsets;
	/* 0x24 */ u32 offsetDataOffsets;
	/* 0x28 */ u32 movieDataOffsets;
	/* 0x2C */ u32 finalFrameDataOffsets;
} THPHeader;

#define THP_AUDIO_BUFFER_COUNT 3
#define THP_READ_BUFFER_COUNT  10
#define THP_TEXTURE_SET_COUNT  3

#ifdef __cplusplus
}
#endif

#endif // COS_THP_EXTRAS_H
