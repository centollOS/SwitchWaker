#ifndef M_DO_M_DO_LIB_H
#define M_DO_M_DO_LIB_H

#include "JSystem/J3DU/J3DUClipper.h"

#if TARGET_PC
// Aurora defines GXTexObj and GXTlutObj as typedefs of unnamed structs, so forward declarations
// through the tags _GXTexObj/_GXTlutObj would declare second, different types. Take the real ones.
#include "dolphin/gx/GXStruct.h"
#else
typedef struct _GXTexObj GXTexObj;
typedef struct _GXTlutObj GXTlutObj;
#endif
typedef struct Vec Vec;
struct ResTIMG;

struct mDoLib_clipper {
    static void changeFar(f32 far) {
        mClipper.setFar(far);
        mClipper.calcViewFrustum();
    }

    static BOOL clip(const Mtx m, Vec* max, Vec* min) {
        return mClipper.clip(m, max, min);
    }

    static BOOL clip(const Mtx m, Vec center, f32 radius) {
        return mClipper.clip(m, center, radius);
    }
    
    static BOOL clip(J3DModel* model) {
        return mClipper.clipByBox(model);
    }

    static f32 getFar() { return mSystemFar; }
    static f32 getFovyRate() { return mFovyRate; }
    
    static void resetFar() {
        mClipper.setFar(mSystemFar);
        mClipper.calcViewFrustum();
    }

    static void setup(f32, f32, f32, f32);

    static J3DUClipper mClipper;
    static f32 mSystemFar;
    static f32 mFovyRate;
};

void mDoLib_project(Vec* src, Vec* dst);
u32 mDoLib_setResTimgObj(ResTIMG* res, GXTexObj* o_texObj, u32 tlut_name, GXTlutObj* o_tlutObj);
void mDoLib_pos2camera(Vec* src, Vec* dst);
u32 mDoLib_cnvind32(u32 r3);
u16 mDoLib_cnvind16(u16 r3);
#if TARGET_PC
// Host form of the texture address a static material display list carries (fix R6-grass, see
// m_Do_lib.cpp): binds `image` to the texture map(s) the list `dl` (`size` bytes) names through
// BP SETIMAGE3. Call it right before GXCallDisplayList(dl, size).
void mDoLib_loadDLTexImage(const void* dl, u32 size, const void* image);
#endif

#endif /* M_DO_M_DO_LIB_H */
