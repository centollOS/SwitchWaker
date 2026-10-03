#ifndef D_BG_W_HF_H
#define D_BG_W_HF_H

#include "d/d_bg_w_sv.h"

class dBgWHf : public dBgWSv {
public:
    virtual ~dBgWHf() {}

    void SetVtx(Vec* vtx) {
        if (GetVtxTbl() == NULL) {
            SetVtxTbl(vtx);
            CopyBackVtx();
        } else {
            SetVtxTbl(vtx);
        }
    }

    bool Set(cBgD_t*, BE(u16)*, f32, int, int, u32);
    void CalcPlane();
    void ClassifyPlane();
    void MoveHf();
    void MakeBlckMinMaxHf(int, f32*, f32*);
    void MakeBlckBndHf(int, f32*, f32*);
    void MakeNodeTreeRpHf(int);
    void MakeNodeTreeGrpRpHf(int);
    void MakeNodeTreeHf();

public:
    /* 0xC4 */ f32 mC4;
    // The grid's triangle index table is a big-endian u16 array read in place from the actor's
    // archive (Bwdg's GridIdx.dat): BE(u16) on TARGET_PC, u16 on GameCube (helpers/endian.h).
    /* 0xC8 */ BE(u16)* mC8;
    /* 0xCC */ int m_gridx;
    /* 0xD0 */ int m_gridz;
};  // Size: 0xD4

#endif /* D_BG_W_HF_H */
