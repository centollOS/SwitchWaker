#ifndef D_PATH_H
#define D_PATH_H

#include "d/d_bg_s.h"
#include "dolphin/types.h"
#include "helpers/endian_ssystem.h"
#include "helpers/offset_ptr.h"

struct dPnt {
    /* 0x00 */ u8 mArg0;
    /* 0x01 */ u8 mArg1;
    /* 0x02 */ u8 mArg2;
    /* 0x03 */ u8 mArg3;
#if TARGET_PC
    // Big-endian in the PPNT/RPPN chunk, read in place. BE<cXyz> rather than Dusklight's BE<Vec>:
    // the readers copy it into a cXyz, which BE<cXyz> converts to in one step.
    /* 0x04 */ BE(cXyz) m_position;
#else
    /* 0x04 */ Vec m_position;
#endif
};

struct dPath {
    /* 0x00 */ BE(u16) m_num;
    /* 0x02 */ BE(u16) m_nextID;
    /* 0x04 */ u8 mArg0;
    /* 0x05 */ u8 m_closed;
    /* 0x06 */ u8 field4_0x6;
    /* 0x07 */ u8 field5_0x7;
    // A file offset from the PPNT/RPPN chunk's entries, relocated in place by dStage_pathInfoInit
    // and dStage_rpatInfoInit (OFFSET_PTR on PC: a host pointer does not fit the 4-byte field).
    /* 0x08 */ OFFSET_PTR(dPnt) m_points;
};

inline bool dPath_ChkClose(dPath* i_path) { return (i_path->m_closed & 1) != 0; }

dPath* dPath_GetRoomPath(int, int);
dPath* dPath_GetNextRoomPath(dPath*, int);
dPnt* dPath_GetPnt(dPath*, int);
bool dPath_GetPolyRoomPathVec(cBgS_PolyInfo&, cXyz*, int*);

#endif /* D_PATH_H */
