#ifndef D_A_MANT_H
#define D_A_MANT_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"

struct mant_j_s {
    /* 0x000 */ cXyz pos[9];
    /* 0x06C */ cXyz velocity[9];
    /* 0x0D8 */ csXyz angle;
    /* 0x0DE */ u8 field_0xde[0x110 - 0xde];
};

class daMant_packet_c : public J3DPacket {
public:
    daMant_packet_c() { mBuffer = 0; }
    MtxP getMtx() { return mMtx; }
    MtxP getMtx2() { return mMtx2; }
    cXyz* getNrm() { return mNormal[mBuffer]; }
    cXyz* getPos() { return mPosition[mBuffer]; }
    u8 getTexNo() { return mTexNo; }
    void setTevStr(dKy_tevstr_c* tev) { mpTevStr = tev; }
    void setTexNo(u8 no) { mTexNo = no; }
    void setarg0(u8 arg) { mArg0 = arg; }
    virtual void draw();
    virtual ~daMant_packet_c() {}

    /* 0x010 */ Mtx mMtx;
    /* 0x040 */ Mtx mMtx2;
    /* 0x070 */ dKy_tevstr_c* mpTevStr;
    /* 0x074 */ u8 mBuffer;
    /* 0x078 */ cXyz mPosition[2][81];
    /* 0x810 */ cXyz mNormal[2][81];
    /* 0xFA8 */ u8 mTexNo;
    /* 0xFA9 */ u8 mArg0;
};

class mant_class : public fopAc_ac_c {
public:
    enum CapeType {
        /* 0x0 */ Type_DARKNUT_e = 0x0,
        /* 0x1 */ Type_PHANTOM_GANON_e = 0x1,
    };

public:
    /* 0x0290 */ u8 field_0x290[8];
    /* 0x0298 */ daMant_packet_c mPacket;
    /* 0x1244 */ u8 mType;
    /* 0x1248 */ int mFrame;
    /* 0x124C */ u8 field_0x124c[4];
    /* 0x1250 */ mant_j_s mJoint[9];
    /* 0x1BE0 */ cXyz mAnchor[2];
    /* 0x1BF8 */ f32 m1BF8;
    /* 0x1BFC */ f32 m1BFC;
    /* 0x1C00 */ f32 mWindStrength;
    /* 0x1C04 */ f32 mWindInfluence;
    /* 0x1C08 */ f32 mGravity;
    /* 0x1C0C */ s16 mWindAngle;
    /* 0x1C0E */ s16 m1C0E;
    /* 0x1C10 */ dCcD_Stts mStts;
    /* 0x1C4C */ dCcD_Sph mWindSph;
    /* 0x1D78 */ dCcD_Sph mMeshSph[9];
    /* 0x2804 */ Mtx mMtx;
    /* 0x2834 */ s8 m2834;
    /* 0x2835 */ s8 m2835;
    /* 0x2836 */ s16 m2836;
    /* 0x2838 */ s16 mDeleteTimer;
};

STATIC_ASSERT(sizeof(mant_j_s) == 0x110);
STATIC_ASSERT(sizeof(daMant_packet_c) == 0xFAC);
STATIC_ASSERT(sizeof(mant_class) == 0x283C);

#endif /* D_A_MANT_H */
