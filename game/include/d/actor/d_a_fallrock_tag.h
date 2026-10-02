#ifndef D_A_FALLROCK_TAG_H
#define D_A_FALLROCK_TAG_H

#include "f_op/f_op_actor.h"

class daFallRockTag_c : public fopAc_ac_c {
public:
    struct data_s {
        /* 0x00 */ f32 mRadius;
        /* 0x04 */ f32 mMinScale;
        /* 0x08 */ f32 mMaxScale;
        /* 0x0C */ f32 field_0x0C;
        /* 0x10 */ f32 field_0x10;
        /* 0x14 */ s16 field_0x14;
        /* 0x16 */ s16 mStartTime;
        /* 0x18 */ s16 mSpawnRate;
    };

    inline ~daFallRockTag_c();
    inline cPhs_State create();
    inline BOOL draw();
    inline BOOL execute();
    inline data_s* getData();

    void createRock(cXyz*, cXyz*, csXyz*, int, unsigned long);

    static f32 m_div_num;
    static data_s m_data;

public:
    /* 0x290 */ u8 field_0x290[8];
    /* 0x298 */ int field_0x298;
    /* 0x29C */ s16 field_0x29C;
    /* 0x29E */ u8 mSchbit;
};

#endif /* D_A_FALLROCK_TAG_H */
