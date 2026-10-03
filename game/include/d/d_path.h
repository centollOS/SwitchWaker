#ifndef D_PATH_H
#define D_PATH_H

#include "d/d_bg_s.h"
#include "dolphin/types.h"
#include "helpers/offset_ptr.h"

struct dPnt {
    /* 0x00 */ u8 mArg0;
    /* 0x01 */ u8 mArg1;
    /* 0x02 */ u8 mArg2;
    /* 0x03 */ u8 mArg3;
    /* 0x04 */ Vec m_position;
};

struct dPath {
    /* 0x00 */ u16 m_num;
    /* 0x02 */ u16 m_nextID;
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
