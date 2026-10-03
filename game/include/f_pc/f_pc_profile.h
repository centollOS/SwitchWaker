
#ifndef F_PC_PROFILE_H_
#define F_PC_PROFILE_H_

#include "dolphin/types.h"
#include "f_pc/f_pc_method.h"

typedef struct process_profile_definition {
    /* 0x00 */ uint mLayerID;
    /* 0x04 */ u16 mListID;
    /* 0x06 */ u16 mListPrio;
    /* 0x08 */ s16 mProcName;
    /* 0x0C */ process_method_class* sub_method; // Subclass methods
    /* 0x10 */ s32 mSize;
    /* 0x14 */ s32 mSizeOther;
    /* 0x18 */ u32 mParameters;
} process_profile_definition;

#define LAYER_DEFAULT (-2)

struct leaf_process_profile_definition;
process_profile_definition* fpcPf_Get(s16 profileID);
extern process_profile_definition** g_fpcPf_ProfileList_p;
#if TARGET_PC
// Defined by f_pc_profile_lst.cpp (also declared in f_pc/f_pc_profile_lst.h); declared here too so
// f_pc_profile.cpp can point g_fpcPf_ProfileList_p at it without the typed profile declarations.
extern process_profile_definition* g_fpcPfLst_ProfileList[];
#endif

#endif
