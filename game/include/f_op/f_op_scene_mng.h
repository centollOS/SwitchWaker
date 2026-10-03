#ifndef F_F_OP_SCENE_MNG_H_
#define F_F_OP_SCENE_MNG_H_

#include "f_pc/f_pc_manager.h"
#include "f_op/f_op_scene.h"

typedef struct base_process_class base_process_class;

scene_class* fopScnM_SearchByID(fpc_ProcID id);
BOOL fopScnM_ChangeReq(scene_class*, s16, s16, u16);
BOOL fopScnM_DeleteReq(scene_class*);
#if TARGET_PC
// The user argument is a pointer to the new scene's parameters (createRoomScene's room number); a
// host pointer does not fit a u32.
BOOL fopScnM_CreateReq(s16, s16, u16, uintptr_t);
u32 fopScnM_ReRequest(s16, uintptr_t);
#else
BOOL fopScnM_CreateReq(s16, s16, u16, u32);
u32 fopScnM_ReRequest(s16, u32);
#endif
void fopScnM_Management(void);
void fopScnM_Init(void);

inline u32 fpcM_LayerID(void* pProc) {
    if (fpcBs_Is_JustOfType(g_fpcNd_type, ((base_process_class*)pProc)->mSubType)) {
        return ((scene_class*)pProc)->base.mLayer.mLayerID;
    } else {
        return fpcLy_NONE_e;
    }
}

inline layer_class* fpcM_Layer(void* pProc) {
    return &((process_node_class*)pProc)->mLayer;
}

inline fpc_ProcID fopScnM_GetID(void* proc) {
    return fpcM_GetID(proc);
}

inline int fopScnM_LayerID(void* proc) {
    return fpcM_LayerID(proc);
}

inline u32 fopScnM_GetParam(void* proc) {
    return fpcM_GetParam(proc);
}

#endif
