#ifndef D_BG_W_DEFORM_H
#define D_BG_W_DEFORM_H

#include "d/d_bg_w_sv.h"
#include "JSystem/J3DGraphAnimator/J3DSkinDeform.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"

class dBgWDeform : public dBgWSv, public J3DSkinDeform {
public:
    virtual ~dBgWDeform() {}

#if TARGET_PC
    // The model's current positions (VTX1, or J3DSkinDeform's output) are big-endian on the host,
    // as Aurora draws them; the collision vertex table is read as host Vec, so it gets a host-order
    // copy of them (mHostVtx, allocated by Set) instead of the model's array itself.
    void MoveAfterAnmCalc(J3DModel* model) {
        const BE(f32)* src = (const BE(f32)*)model->getCurrentVtxPos();
        int num = GetVtxNum();
        for (int i = 0; i < num; i++) {
            mHostVtx[i].x = src[i * 3 + 0];
            mHostVtx[i].y = src[i * 3 + 1];
            mHostVtx[i].z = src[i * 3 + 2];
        }
        SetVtx(mHostVtx);
        Move();
    }
#else
    void MoveAfterAnmCalc(J3DModel* model) {
        cXyz* pPos = (cXyz*)model->getCurrentVtxPos();
        SetVtx(pPos);
        Move();
    }
#endif
    
    void SetVtx(Vec* pPos) {
        if (GetVtxTbl() == NULL) {
            SetVtxTbl(pPos);
            CopyBackVtx();
        } else {
            SetVtxTbl(pPos);
        }
    }

    bool Set(cBgD_t*, J3DModel*, u32);

public:
    /* 0x00 */ /* dBgWSv */
    /* 0xC4 */ /* J3DSkinDeform */
#if TARGET_PC
    Vec* mHostVtx = NULL;
#endif
};  // Size: 0xDC

#endif /* D_BG_W_DEFORM_H */
