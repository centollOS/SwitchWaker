/**
 * d_minigame_terminater.cpp
 * Boating Course - Completion text
 */

#include "d/dolzel.h" // IWYU pragma: keep
#include "d/d_minigame_terminater.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter.h"
#include "f_op/f_op_msg.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_math.h"
#if VERSION == VERSION_PAL
#include "stdio.h"
#endif

#if VERSION <= VERSION_JPN
static const s16 dMgtem_perfect_tex = 8;
static const s16 dMgtem_failed_tex = 9;
static const s16 dMgtem_yougot_tex = 6;
static const s16 dMgtem_remain_tex = 13;
#else
static s16 dMgtem_perfect_tex = 8;
static s16 dMgtem_failed_tex = 8;
static s16 dMgtem_yougot_tex = 6;
static s16 dMgtem_remain_tex = 13;
#endif

struct fopMsg_prm_MGameTerm : public fopMsg_prm_class {
    /* 0x1C */ u32 mTime;
    /* 0x20 */ u32 mRupee;
    /* 0x24 */ s16 field_0x24;
    /* 0x26 */ s16 mFinishType;
};

/* 80206FC0-80207164       .text _create__22dMinigame_Terminater_cFv */
cPhs_State dMinigame_Terminater_c::_create() {
    cPhs_State phase_state = dComIfG_resLoad(&mPhs, "Mgtem");
    if (phase_state == cPhs_COMPLEATE_e) {
        dRes_info_c* resInfo = dComIfG_getObjectResInfo("Mgtem");
        JUT_ASSERT(VERSION_SELECT(59, 59, 102, 102), resInfo != NULL);

        mHeap = mDoExt_createSolidHeapFromGameToCurrent(VERSION_SELECT(0xBE20, 0xBCA0, 0xC0A0, 0xE230), 0x20);
        if (mHeap != NULL) {
            mScrn = new dDlst_TerminaterScrnDraw_c();
#if VERSION == VERSION_PAL
            u8 lang = dComIfGs_getPalLanguage();
            if (lang == 1) {
                dMgtem_perfect_tex = 8;
                dMgtem_failed_tex = 13;
                dMgtem_yougot_tex = 10;
                dMgtem_remain_tex = 15;
            } else if (lang == 2) {
                dMgtem_perfect_tex = 8;
                dMgtem_failed_tex = 17;
                dMgtem_yougot_tex = 13;
                dMgtem_remain_tex = 12;
            } else if (lang == 3) {
                dMgtem_perfect_tex = 10;
                dMgtem_failed_tex = 6;
                dMgtem_yougot_tex = 9;
                dMgtem_remain_tex = 14;
            } else if (lang == 4) {
                dMgtem_perfect_tex = 9;
                dMgtem_failed_tex = 13;
                dMgtem_yougot_tex = 11;
                dMgtem_remain_tex = 14;
            } else {
                dMgtem_perfect_tex = 8;
                dMgtem_failed_tex = 8;
                dMgtem_yougot_tex = 6;
                dMgtem_remain_tex = 13;
            }
            char buf[32];
            sprintf(buf, "ship_race2_%d.blo", lang);
            mScrn->setScreen(buf, resInfo->getArchive());
#else
            mScrn->setScreen("ship_race2.blo", resInfo->getArchive());
#endif
            mDoExt_restoreCurrentHeap();
            mDoExt_adjustSolidHeap(mHeap);
        } else {
            return cPhs_ERROR_e;
        }
    } else {
        return phase_state;
    }

    fopMsg_prm_MGameTerm* prm = (fopMsg_prm_MGameTerm*)fopMsgM_GetAppend(this);
    field_0x11a = prm->field_0x24;
    mFinishType = prm->mFinishType;
    setResultTime(prm->mTime);
    setResultRupee(prm->mRupee);

    if (mFinishType == 2 || mFinishType == 1) {
        mScrn->setScrnSuccess(getResultRupee(), getResultTime());
    } else {
        mScrn->setScrnFailed();
    }

    mTimer = 330;
    mStatus = 1;
    return cPhs_COMPLEATE_e;
}

/* 80207164-8020725C       .text _execute__22dMinigame_Terminater_cFv */
BOOL dMinigame_Terminater_c::_execute() {
    if (mTimer > 240) {
        mTimer--;
        return TRUE;
    }

    if (mFinishType == 2) {
        if (mTimer > 0) {
            mScrn->animeSuccess();
            mScrn->animePerfect();
            mTimer--;
        } else {
            mStatus = 3;
        }
    } else if (mFinishType == 1) {
        if (mTimer > 0) {
            mScrn->animeSuccess();
            mTimer--;
        } else {
            mStatus = 3;
        }
    } else {
        if (mTimer > 0) {
            if (mTimer < 120) {
                mScrn->animeFailed2();
            } else {
                mScrn->animeFailed1();
            }
            mTimer--;
        } else {
            mStatus = 3;
        }
    }

    return TRUE;
}

/* 8020725C-802072CC       .text _draw__22dMinigame_Terminater_cFv */
BOOL dMinigame_Terminater_c::_draw() {
    if (mFinishType == 2 || mFinishType == 1) {
        mScrn->setAlphaSuccess();
    } else {
        mScrn->setAlphaFailed();
    }

    dComIfGd_set2DOpa(mScrn);
    return TRUE;
}

/* 802072CC-80207318       .text _delete__22dMinigame_Terminater_cFv */
BOOL dMinigame_Terminater_c::_delete() {
    if (mHeap != NULL) {
        mDoExt_destroySolidHeap(mHeap);
    }

    dComIfG_resDeleteDemo(&mPhs, "Mgtem");
    return TRUE;
}

/* 80207318-802073C4       .text setScreen__26dDlst_TerminaterScrnDraw_cFPCcP10JKRArchive */
void dDlst_TerminaterScrnDraw_c::setScreen(const char* i_layoutName, JKRArchive* i_archive) {
    mpScrn = new J2DScreen();
    mpScrn->set(i_layoutName, i_archive);
}

/* 802073C4-8020789C       .text setScrnFailed__26dDlst_TerminaterScrnDraw_cFv */
#if VERSION <= VERSION_JPN
void dDlst_TerminaterScrnDraw_c::setScrnFailed() {
    static const u32 perfect[] = {
        '\0pep',
        '\0pee',
        '\0per',
        '\0pef',
        'pee2',
        '\0pec',
        '\0pet',
        'peex',
    };
    static const u32 perfect_nt[] = {
        'ntt0', 'ntt1', 'ntt2', 'ntt3', 'ntt4', 'ntt5', 'ntt6', 'ntt7', 'ntt8', 'ntt9',
    };
    static const u32 perfect_nk[] = {
        'ntk0', 'ntk1', 'ntk2', 'ntk3', 'ntk4', 'ntk5', 'ntk6', 'ntk7', 'ntk8', 'ntk9',
    };

    for (int i = 0; i < dMgtem_perfect_tex; i++) {
        mpScrn->search(perfect[i])->hide();
    }

    for (int i = 0; i < 10; i++) {
        mpScrn->search(perfect_nt[i])->hide();
        mpScrn->search(perfect_nk[i])->hide();
    }

    mpScrn->search('dmpe')->hide();

    fopMsgM_setPaneData(&mFailed[0], mpScrn->search('\0fad'));
    fopMsgM_setPaneData(&mFailed[1], mpScrn->search('\0fae'));
    fopMsgM_setPaneData(&mFailed[2], mpScrn->search('\0fal'));
    fopMsgM_setPaneData(&mFailed[3], mpScrn->search('\0fai'));
    fopMsgM_setPaneData(&mFailed[4], mpScrn->search('\0faa'));
    fopMsgM_setPaneData(&mFailed[5], mpScrn->search('\0faf'));
    fopMsgM_setPaneData(&mFailed[6], mpScrn->search('\0fau'));
    fopMsgM_setPaneData(&mFailed[7], mpScrn->search('\0fao'));
    fopMsgM_setPaneData(&mFailed[8], mpScrn->search('\0fay'));

    for (int i = 0; i < dMgtem_failed_tex; i++) {
        getRotate(&mFailed[i]);
        mRemain[i].mUserArea = 0;
        mRemainK[i].mUserArea = 0;
    }

    mpScrn->search('\0yoy')->hide();
    mpScrn->search('ree2')->hide();
    mpScrn->search('rem2')->hide();
    mpScrn->search('rei3')->hide();
    mpScrn->search('\0ret')->hide();
    mpScrn->search('\0reg')->hide();
    mpScrn->search('ren2')->hide();
    mpScrn->search('rei2')->hide();
    mpScrn->search('ren1')->hide();
    mpScrn->search('rei1')->hide();
    mpScrn->search('\0rea')->hide();
    mpScrn->search('rem1')->hide();
    mpScrn->search('ree1')->hide();
    mpScrn->search('\0rer')->hide();
    mpScrn->search('rke2')->hide();
    mpScrn->search('rkm2')->hide();
    mpScrn->search('rki3')->hide();
    mpScrn->search('\0rkt')->hide();
    mpScrn->search('\0rkg')->hide();
    mpScrn->search('rkn2')->hide();
    mpScrn->search('rki2')->hide();
    mpScrn->search('rkn1')->hide();
    mpScrn->search('rki1')->hide();
    mpScrn->search('\0rka')->hide();
    mpScrn->search('rkm1')->hide();
    mpScrn->search('rke1')->hide();
    mpScrn->search('\0rkr')->hide();
    mpScrn->search('num6')->hide();
    mpScrn->search('num5')->hide();
    mpScrn->search('num4')->hide();
    mpScrn->search('num3')->hide();
    mpScrn->search('num2')->hide();
    mpScrn->search('num1')->hide();
    mpScrn->search('nmd2')->hide();
    mpScrn->search('nmd1')->hide();
    mpScrn->search('nuk6')->hide();
    mpScrn->search('nuk5')->hide();
    mpScrn->search('nuk4')->hide();
    mpScrn->search('nuk3')->hide();
    mpScrn->search('nuk2')->hide();
    mpScrn->search('nuk1')->hide();
    mpScrn->search('ndk2')->hide();
    mpScrn->search('ndk1')->hide();
    mpScrn->search('\0rpx')->hide();
    mpScrn->search('rpxk')->hide();
    mpScrn->search('nt00')->hide();
    mpScrn->search('nk00')->hide();
    mpScrn->search('dmmy')->hide();
}
#else
void dDlst_TerminaterScrnDraw_c::setScrnFailed() {
#if VERSION == VERSION_PAL
    if (dComIfGs_getPalLanguage() == 1) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            mpScrn->search(perfect[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 2) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            mpScrn->search(perfect[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 3) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
            'pet2',
            'pee3',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            mpScrn->search(perfect[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 4) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
            'pet2',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            mpScrn->search(perfect[i])->hide();
        }
    } else {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            mpScrn->search(perfect[i])->hide();
        }
    }

    static const u32 perfect_nt[] = {
        'ntt0',
        'ntt1',
        'ntt2',
        'ntt3',
        'ntt4',
        'ntt5',
        'ntt6',
        'ntt7',
        'ntt8',
        'ntt9',
    };
    static const u32 perfect_nk[] = {
        'ntk0',
        'ntk1',
        'ntk2',
        'ntk3',
        'ntk4',
        'ntk5',
        'ntk6',
        'ntk7',
        'ntk8',
        'ntk9',
    };

    if (dComIfGs_getPalLanguage() == 1) {
        static const u32 failed[] = {
            'faex',
            '\0fam',
            'fau2',
            '\0fat',
            '\0fad',
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            fopMsgM_setPaneData(&mFailed[i], mpScrn, failed[i]);
            getRotate(&mFailed[i]);
            mRemain[i].mUserArea = 0;
            mRemainK[i].mUserArea = 0;
        }
    } else if (dComIfGs_getPalLanguage() == 2) {
        static const u32 failed[] = {
            'faex',
            'fae3',
            'fal2',
            'fau2',
            'fao2',
            '\0fac',
            'fae2',
            '\0fat',
            '\0fad',
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            fopMsgM_setPaneData(&mFailed[i], mpScrn, failed[i]);
            getRotate(&mFailed[i]);
            mRemain[i].mUserArea = 0;
            mRemainK[i].mUserArea = 0;
        }
    } else if (dComIfGs_getPalLanguage() == 3) {
        static const u32 failed[] = {
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            fopMsgM_setPaneData(&mFailed[i], mpScrn, failed[i]);
            getRotate(&mFailed[i]);
            mRemain[i].mUserArea = 0;
            mRemainK[i].mUserArea = 0;
        }
    } else if (dComIfGs_getPalLanguage() == 4) {
        static const u32 failed[] = {
            'faex',
            'fao2',
            '\0fat',
            'fau2',
            '\0fad',
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            fopMsgM_setPaneData(&mFailed[i], mpScrn, failed[i]);
            getRotate(&mFailed[i]);
            mRemain[i].mUserArea = 0;
            mRemainK[i].mUserArea = 0;
        }
    } else {
        static const u32 failed[] = {
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            fopMsgM_setPaneData(&mFailed[i], mpScrn, failed[i]);
            getRotate(&mFailed[i]);
            mRemain[i].mUserArea = 0;
            mRemainK[i].mUserArea = 0;
        }
    }

    if (dComIfGs_getPalLanguage() == 1) {
        static const u32 remaintime[] = {
            '\0yoy',
            'ret2',
            'rei4',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rkt2',
            'rki4',
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_remain_tex + 1; i++) {
            mpScrn->search(remaintime[i])->hide();
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            mpScrn->search(remaintimek[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 2) {
        static const u32 remaintime[] = {
            '\0yoy',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_remain_tex + 1; i++) {
            mpScrn->search(remaintime[i])->hide();
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            mpScrn->search(remaintimek[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 3) {
        static const u32 remaintime[] = {
            '\0yoy',
            'ree3',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rke3',
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_remain_tex + 1; i++) {
            mpScrn->search(remaintime[i])->hide();
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            mpScrn->search(remaintimek[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 4) {
        static const u32 remaintime[] = {
            '\0yoy',
            'ree3',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rke3',
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_remain_tex + 1; i++) {
            mpScrn->search(remaintime[i])->hide();
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            mpScrn->search(remaintimek[i])->hide();
        }
    } else {
        static const u32 remaintime[] = {
            '\0yoy',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_remain_tex + 1; i++) {
            mpScrn->search(remaintime[i])->hide();
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            mpScrn->search(remaintimek[i])->hide();
        }
    }
#else
    static const u32 perfect[] = {
        '\0pep',
        '\0pee',
        '\0per',
        '\0pef',
        'pee2',
        '\0pec',
        '\0pet',
        'peex',
    };
    static const u32 perfect_nt[] = {
        'ntt0', 'ntt1', 'ntt2', 'ntt3', 'ntt4', 'ntt5', 'ntt6', 'ntt7', 'ntt8', 'ntt9',
    };
    static const u32 perfect_nk[] = {
        'ntk0', 'ntk1', 'ntk2', 'ntk3', 'ntk4', 'ntk5', 'ntk6', 'ntk7', 'ntk8', 'ntk9',
    };
    static const u32 failed[] = {
        '\0fae',
        '\0fal',
        '\0fai',
        '\0faa',
        '\0faf',
        '\0fau',
        '\0fao',
        '\0fay',
    };
    static const u32 remaintime[] = {
        '\0yoy',
        'ree2',
        'rem2',
        'rei3',
        '\0ret',
        '\0reg',
        'ren2',
        'rei2',
        'ren1',
        'rei1',
        '\0rea',
        'rem1',
        'ree1',
        '\0rer',
    };
    static const u32 remaintimek[] = {
        'rke2',
        'rkm2',
        'rki3',
        '\0rkt',
        '\0rkg',
        'rkn2',
        'rki2',
        'rkn1',
        'rki1',
        '\0rka',
        'rkm1',
        'rke1',
        '\0rkr',
    };

    for (int i = 0; i < dMgtem_perfect_tex; i++) {
        mpScrn->search(perfect[i])->hide();
    }

    for (int i = 0; i < dMgtem_failed_tex; i++) {
        fopMsgM_setPaneData(&mFailed[i], mpScrn, failed[i]);
        getRotate(&mFailed[i]);
        mRemain[i].mUserArea = 0;
        mRemainK[i].mUserArea = 0;
    }

    for (int i = 0; i < dMgtem_remain_tex + 1; i++) {
        mpScrn->search(remaintime[i])->hide();
    }

    for (int i = 0; i < dMgtem_remain_tex; i++) {
        mpScrn->search(remaintimek[i])->hide();
    }

#endif

    for (int i = 0; i < 10; i++) {
        mpScrn->search(perfect_nt[i])->hide();
        mpScrn->search(perfect_nk[i])->hide();
    }

    mpScrn->search('dmpe')->hide();
    mpScrn->search('num6')->hide();
    mpScrn->search('num5')->hide();
    mpScrn->search('num4')->hide();
    mpScrn->search('num3')->hide();
    mpScrn->search('num2')->hide();
    mpScrn->search('num1')->hide();
    mpScrn->search('nmd2')->hide();
    mpScrn->search('nmd1')->hide();
    mpScrn->search('nuk6')->hide();
    mpScrn->search('nuk5')->hide();
    mpScrn->search('nuk4')->hide();
    mpScrn->search('nuk3')->hide();
    mpScrn->search('nuk2')->hide();
    mpScrn->search('nuk1')->hide();
    mpScrn->search('ndk2')->hide();
    mpScrn->search('ndk1')->hide();
    mpScrn->search('\0rpx')->hide();
    mpScrn->search('rpxk')->hide();
    mpScrn->search('nt00')->hide();
    mpScrn->search('nk00')->hide();
    mpScrn->search('dmmy')->hide();
}
#endif

/* 8020789C-8020805C       .text setScrnSuccess__26dDlst_TerminaterScrnDraw_cFii */
void dDlst_TerminaterScrnDraw_c::setScrnSuccess(int i_rupee, int i_time) {
#if VERSION <= VERSION_JPN
    static const u32 perfect[] = {
        '\0pep',
        '\0pee',
        '\0per',
        '\0pef',
        'pee2',
        '\0pec',
        '\0pet',
        'peex',
    };
    static const u32 perfect_nt[] = {
        'ntt0', 'ntt1', 'ntt2', 'ntt3', 'ntt4', 'ntt5', 'ntt6', 'ntt7', 'ntt8', 'ntt9',
    };
    static const u32 perfect_nk[] = {
        'ntk0', 'ntk1', 'ntk2', 'ntk3', 'ntk4', 'ntk5', 'ntk6', 'ntk7', 'ntk8', 'ntk9',
    };

    for (int i = 0; i < dMgtem_perfect_tex; i++) {
        fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
    }

    for (int i = 0; i < 10; i++) {
        fopMsgM_setPaneData(&mPerfectNt[i], mpScrn->search(perfect_nt[i]));
        fopMsgM_setPaneData(&mPerfectNk[i], mpScrn->search(perfect_nk[i]));
    }

    fopMsgM_setPaneData(&mDmpe, mpScrn->search('dmpe'));

    mpScrn->search('\0fad')->hide();
    mpScrn->search('\0fae')->hide();
    mpScrn->search('\0fal')->hide();
    mpScrn->search('\0fai')->hide();
    mpScrn->search('\0faa')->hide();
    mpScrn->search('\0faf')->hide();
    mpScrn->search('\0fau')->hide();
    mpScrn->search('\0fao')->hide();
    mpScrn->search('\0fay')->hide();

    fopMsgM_setPaneData(&mYouGot[0], mpScrn->search('\0yoy'));
    fopMsgM_setPaneData(&mYouGot[1], mpScrn->search('yoo1'));
    fopMsgM_setPaneData(&mYouGot[2], mpScrn->search('\0you'));
    fopMsgM_setPaneData(&mYouGot[3], mpScrn->search('\0yog'));
    fopMsgM_setPaneData(&mYouGot[4], mpScrn->search('yoo2'));
    fopMsgM_setPaneData(&mYouGot[5], mpScrn->search('\0yot'));

    fopMsgM_setPaneData(&mRemain[0], mpScrn->search('ree2'));
    fopMsgM_setPaneData(&mRemain[1], mpScrn->search('rem2'));
    fopMsgM_setPaneData(&mRemain[2], mpScrn->search('rei3'));
    fopMsgM_setPaneData(&mRemain[3], mpScrn->search('\0ret'));
    fopMsgM_setPaneData(&mRemain[4], mpScrn->search('\0reg'));
    fopMsgM_setPaneData(&mRemain[5], mpScrn->search('ren2'));
    fopMsgM_setPaneData(&mRemain[6], mpScrn->search('rei2'));
    fopMsgM_setPaneData(&mRemain[7], mpScrn->search('ren1'));
    fopMsgM_setPaneData(&mRemain[8], mpScrn->search('rei1'));
    fopMsgM_setPaneData(&mRemain[9], mpScrn->search('\0rea'));
    fopMsgM_setPaneData(&mRemain[10], mpScrn->search('rem1'));
    fopMsgM_setPaneData(&mRemain[11], mpScrn->search('ree1'));
    fopMsgM_setPaneData(&mRemain[12], mpScrn->search('\0rer'));

    fopMsgM_setPaneData(&mRemainK[0], mpScrn->search('rke2'));
    fopMsgM_setPaneData(&mRemainK[1], mpScrn->search('rkm2'));
    fopMsgM_setPaneData(&mRemainK[2], mpScrn->search('rki3'));
    fopMsgM_setPaneData(&mRemainK[3], mpScrn->search('\0rkt'));
    fopMsgM_setPaneData(&mRemainK[4], mpScrn->search('\0rkg'));
    fopMsgM_setPaneData(&mRemainK[5], mpScrn->search('rkn2'));
    fopMsgM_setPaneData(&mRemainK[6], mpScrn->search('rki2'));
    fopMsgM_setPaneData(&mRemainK[7], mpScrn->search('rkn1'));
    fopMsgM_setPaneData(&mRemainK[8], mpScrn->search('rki1'));
    fopMsgM_setPaneData(&mRemainK[9], mpScrn->search('\0rka'));
    fopMsgM_setPaneData(&mRemainK[10], mpScrn->search('rkm1'));
    fopMsgM_setPaneData(&mRemainK[11], mpScrn->search('rke1'));
    fopMsgM_setPaneData(&mRemainK[12], mpScrn->search('\0rkr'));
#elif VERSION == VERSION_PAL
    if (dComIfGs_getPalLanguage() == 1) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
        }
    } else if (dComIfGs_getPalLanguage() == 2) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
        }
    } else if (dComIfGs_getPalLanguage() == 3) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
            'pet2',
            'pee3',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
        }
    } else if (dComIfGs_getPalLanguage() == 4) {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
            'pet2',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
        }
    } else {
        static const u32 perfect[] = {
            '\0pep',
            '\0pee',
            '\0per',
            '\0pef',
            'pee2',
            '\0pec',
            '\0pet',
            'peex',
        };
        for (int i = 0; i < dMgtem_perfect_tex; i++) {
            fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
        }
    }

    static const u32 perfect_nt[] = {
        'ntt0',
        'ntt1',
        'ntt2',
        'ntt3',
        'ntt4',
        'ntt5',
        'ntt6',
        'ntt7',
        'ntt8',
        'ntt9',
    };
    static const u32 perfect_nk[] = {
        'ntk0',
        'ntk1',
        'ntk2',
        'ntk3',
        'ntk4',
        'ntk5',
        'ntk6',
        'ntk7',
        'ntk8',
        'ntk9',
    };

    if (dComIfGs_getPalLanguage() == 1) {
        static const u32 failed[] = {
            'faex',
            '\0fam',
            'fau2',
            '\0fat',
            '\0fad',
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            mpScrn->search(failed[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 2) {
        static const u32 failed[] = {
            'faex',
            'fae3',
            'fal2',
            'fau2',
            'fao2',
            '\0fac',
            'fae2',
            '\0fat',
            '\0fad',
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            mpScrn->search(failed[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 3) {
        static const u32 failed[] = {
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            mpScrn->search(failed[i])->hide();
        }
    } else if (dComIfGs_getPalLanguage() == 4) {
        static const u32 failed[] = {
            'faex',
            'fao2',
            '\0fat',
            'fau2',
            '\0fad',
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            mpScrn->search(failed[i])->hide();
        }
    } else {
        static const u32 failed[] = {
            '\0fae',
            '\0fal',
            '\0fai',
            '\0faa',
            '\0faf',
            '\0fau',
            '\0fao',
            '\0fay',
        };
        for (int i = 0; i < dMgtem_failed_tex; i++) {
            mpScrn->search(failed[i])->hide();
        }
    }

    if (dComIfGs_getPalLanguage() == 1) {
        static const u32 yougot[] = {
            '\0yoy',
            'yoo1',
            '\0you',
            '\0yog',
            'yoo2',
            '\0yot',
            '\0yol',
            'yot2',
            '\0yos',
            'yot3',
        };
        static const u32 remaintime[] = {
            'ret2',
            'rei4',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rkt2',
            'rki4',
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_yougot_tex; i++) {
            fopMsgM_setPaneData(&mYouGot[i], mpScrn, yougot[i]);
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            fopMsgM_setPaneData(&mRemain[i], mpScrn, remaintime[i]);
            fopMsgM_setPaneData(&mRemainK[i], mpScrn, remaintimek[i]);
        }
    } else if (dComIfGs_getPalLanguage() == 2) {
        static const u32 yougot[] = {
            '\0yoy',
            'yoo1',
            '\0you',
            '\0yog',
            'yoo2',
            '\0yot',
            '\0yoe',
            '\0yoz',
            'yog2',
            '\0yoa',
            'yog3',
            '\0yon',
            'yoe2',
        };
        static const u32 remaintime[] = {
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_yougot_tex; i++) {
            fopMsgM_setPaneData(&mYouGot[i], mpScrn, yougot[i]);
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            fopMsgM_setPaneData(&mRemain[i], mpScrn, remaintime[i]);
            fopMsgM_setPaneData(&mRemainK[i], mpScrn, remaintimek[i]);
        }
    } else if (dComIfGs_getPalLanguage() == 3) {
        static const u32 yougot[] = {
            '\0yoy',
            'yoo1',
            '\0you',
            '\0yog',
            'yoo2',
            '\0yot',
            '\0yoa',
            '\0yod',
            'yoo3',
        };
        static const u32 remaintime[] = {
            'ree3',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rke3',
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_yougot_tex; i++) {
            fopMsgM_setPaneData(&mYouGot[i], mpScrn, yougot[i]);
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            fopMsgM_setPaneData(&mRemain[i], mpScrn, remaintime[i]);
            fopMsgM_setPaneData(&mRemainK[i], mpScrn, remaintimek[i]);
        }
    } else if (dComIfGs_getPalLanguage() == 4) {
        static const u32 yougot[] = {
            '\0yoy',
            'yoo1',
            '\0you',
            '\0yog',
            'yoo2',
            '\0yot',
            'yoex',
            '\0yon',
            'you2',
            'yot2',
            'yoo3',
        };
        static const u32 remaintime[] = {
            'ree3',
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rke3',
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_yougot_tex; i++) {
            fopMsgM_setPaneData(&mYouGot[i], mpScrn, yougot[i]);
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            fopMsgM_setPaneData(&mRemain[i], mpScrn, remaintime[i]);
            fopMsgM_setPaneData(&mRemainK[i], mpScrn, remaintimek[i]);
        }
    } else {
        static const u32 yougot[] = {
            '\0yoy',
            'yoo1',
            '\0you',
            '\0yog',
            'yoo2',
            '\0yot',
        };
        static const u32 remaintime[] = {
            'ree2',
            'rem2',
            'rei3',
            '\0ret',
            '\0reg',
            'ren2',
            'rei2',
            'ren1',
            'rei1',
            '\0rea',
            'rem1',
            'ree1',
            '\0rer',
        };
        static const u32 remaintimek[] = {
            'rke2',
            'rkm2',
            'rki3',
            '\0rkt',
            '\0rkg',
            'rkn2',
            'rki2',
            'rkn1',
            'rki1',
            '\0rka',
            'rkm1',
            'rke1',
            '\0rkr',
        };
        for (int i = 0; i < dMgtem_yougot_tex; i++) {
            fopMsgM_setPaneData(&mYouGot[i], mpScrn, yougot[i]);
        }
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            fopMsgM_setPaneData(&mRemain[i], mpScrn, remaintime[i]);
            fopMsgM_setPaneData(&mRemainK[i], mpScrn, remaintimek[i]);
        }
    }
#else
    static const u32 perfect[] = {
        '\0pep',
        '\0pee',
        '\0per',
        '\0pef',
        'pee2',
        '\0pec',
        '\0pet',
        'peex',
    };
    static const u32 perfect_nt[] = {
        'ntt0', 'ntt1', 'ntt2', 'ntt3', 'ntt4', 'ntt5', 'ntt6', 'ntt7', 'ntt8', 'ntt9',
    };
    static const u32 perfect_nk[] = {
        'ntk0', 'ntk1', 'ntk2', 'ntk3', 'ntk4', 'ntk5', 'ntk6', 'ntk7', 'ntk8', 'ntk9',
    };
    static const u32 failed[] = {
        '\0fae',
        '\0fal',
        '\0fai',
        '\0faa',
        '\0faf',
        '\0fau',
        '\0fao',
        '\0fay',
    };
    static const u32 yougot[] = {
        '\0yoy',
        'yoo1',
        '\0you',
        '\0yog',
        'yoo2',
        '\0yot',
    };
    static const u32 remaintime[] = {
        'ree2',
        'rem2',
        'rei3',
        '\0ret',
        '\0reg',
        'ren2',
        'rei2',
        'ren1',
        'rei1',
        '\0rea',
        'rem1',
        'ree1',
        '\0rer',
    };
    static const u32 remaintimek[] = {
        'rke2',
        'rkm2',
        'rki3',
        '\0rkt',
        '\0rkg',
        'rkn2',
        'rki2',
        'rkn1',
        'rki1',
        '\0rka',
        'rkm1',
        'rke1',
        '\0rkr',
    };

    for (int i = 0; i < dMgtem_perfect_tex; i++) {
        fopMsgM_setPaneData(&mPerfect[i], mpScrn->search(perfect[i]));
    }

    for (int i = 0; i < dMgtem_failed_tex; i++) {
        mpScrn->search(failed[i])->hide();
    }

    for (int i = 0; i < dMgtem_yougot_tex; i++) {
        fopMsgM_setPaneData(&mYouGot[i], mpScrn, yougot[i]);
    }

    for (int i = 0; i < dMgtem_remain_tex; i++) {
        fopMsgM_setPaneData(&mRemain[i], mpScrn, remaintime[i]);
        fopMsgM_setPaneData(&mRemainK[i], mpScrn, remaintimek[i]);
    }

#endif

#if VERSION > VERSION_JPN
    for (int i = 0; i < 10; i++) {
        fopMsgM_setPaneData(&mPerfectNt[i], mpScrn->search(perfect_nt[i]));
        fopMsgM_setPaneData(&mPerfectNk[i], mpScrn->search(perfect_nk[i]));
    }

    fopMsgM_setPaneData(&mDmpe, mpScrn->search('dmpe'));
#endif
    fopMsgM_setPaneData(&mNum[0], mpScrn->search('num6'));
    fopMsgM_setPaneData(&mNum[1], mpScrn->search('num5'));
    fopMsgM_setPaneData(&mNum[2], mpScrn->search('nmd2'));
    fopMsgM_setPaneData(&mNum[3], mpScrn->search('num4'));
    fopMsgM_setPaneData(&mNum[4], mpScrn->search('num3'));
    fopMsgM_setPaneData(&mNum[5], mpScrn->search('nmd1'));
    fopMsgM_setPaneData(&mNum[6], mpScrn->search('num2'));
    fopMsgM_setPaneData(&mNum[7], mpScrn->search('num1'));
    fopMsgM_setPaneData(&mNumK[0], mpScrn->search('nuk6'));
    fopMsgM_setPaneData(&mNumK[1], mpScrn->search('nuk5'));
    fopMsgM_setPaneData(&mNumK[2], mpScrn->search('ndk2'));
    fopMsgM_setPaneData(&mNumK[3], mpScrn->search('nuk4'));
    fopMsgM_setPaneData(&mNumK[4], mpScrn->search('nuk3'));
    fopMsgM_setPaneData(&mNumK[5], mpScrn->search('ndk1'));
    fopMsgM_setPaneData(&mNumK[6], mpScrn->search('nuk2'));
    fopMsgM_setPaneData(&mNumK[7], mpScrn->search('nuk1'));
    fopMsgM_setPaneData(&mRpx, mpScrn->search('\0rpx'));
    fopMsgM_setPaneData(&mRpxk, mpScrn->search('rpxk'));
    fopMsgM_setPaneData(&mNt00, mpScrn->search('nt00'));
    fopMsgM_setPaneData(&mNk00, mpScrn->search('nk00'));
    fopMsgM_setPaneData(&mDummy, mpScrn->search('dmmy'));

    mRupeePane[0] = mpScrn->search('\0rp3');
    mRupeePane[1] = mpScrn->search('\0rp2');
    mRupeePane[2] = mpScrn->search('\0rp1');
    mRupeePane[3] = mpScrn->search('rpk3');
    mRupeePane[4] = mpScrn->search('rpk2');
    mRupeePane[5] = mpScrn->search('rpk1');

    int sec = i_time / 100;
    i_time = i_time - sec * 100;
    int min = sec / 60;
    sec = sec - min * 60;

    changeNumberTexture(mNum[7].pane, min / 10);
    changeNumberTexture(mNum[6].pane, min % 10);
    changeNumberTexture(mNum[4].pane, sec / 10);
    changeNumberTexture(mNum[3].pane, sec % 10);
    changeNumberTexture(mNum[1].pane, i_time / 10);
    changeNumberTexture(mNum[0].pane, i_time % 10);
    changeNumberTexture(mNumK[7].pane, min / 10);
    changeNumberTexture(mNumK[6].pane, min % 10);
    changeNumberTexture(mNumK[4].pane, sec / 10);
    changeNumberTexture(mNumK[3].pane, sec % 10);
    changeNumberTexture(mNumK[1].pane, i_time / 10);
    changeNumberTexture(mNumK[0].pane, i_time % 10);

    int r001 = i_rupee % 10;
    int r010 = (i_rupee % 100) / 10;
    int r100 = i_rupee / 100;
    changeNumberTexture(mRupeePane[0], r001);
    changeNumberTexture(mRupeePane[1], r010);
    changeNumberTexture(mRupeePane[2], r100);
    changeNumberTexture(mRupeePane[3], r001);
    changeNumberTexture(mRupeePane[4], r010);
    changeNumberTexture(mRupeePane[5], r100);
}

/* 8020805C-802080B0       .text changeNumberTexture__26dDlst_TerminaterScrnDraw_cFP7J2DPanei */
void dDlst_TerminaterScrnDraw_c::changeNumberTexture(J2DPane* pane, int no) {
    if (no < 0 || no >= 10)
        no = 0;

    ((J2DPicture*)pane)->changeTexture(getNumber(no), 0);
}

/* 802080B0-802080C4       .text getNumber__26dDlst_TerminaterScrnDraw_cFi */
const char* dDlst_TerminaterScrnDraw_c::getNumber(int no) {
    static const char* number[] = {
        "rupy_num_00.bti",
        "rupy_num_01.bti",
        "rupy_num_02.bti",
        "rupy_num_03.bti",
        "rupy_num_04.bti",
        "rupy_num_05.bti",
        "rupy_num_06.bti",
        "rupy_num_07.bti",
        "rupy_num_08.bti",
        "rupy_num_09.bti",
    };

    return number[no];
}

/* 802080C4-80208120       .text setAlphaFailed__26dDlst_TerminaterScrnDraw_cFv */
void dDlst_TerminaterScrnDraw_c::setAlphaFailed() {
    for (int i = 0; i < dMgtem_failed_tex; i++) {
        fopMsgM_setAlpha(&mFailed[i]);
    }
}

/* 80208120-802081F8       .text setAlphaSuccess__26dDlst_TerminaterScrnDraw_cFv */
void dDlst_TerminaterScrnDraw_c::setAlphaSuccess() {
    fopMsgM_setAlpha(&mPerfect[0]);
    fopMsgM_setAlpha(&mPerfectNt[0]);
    fopMsgM_setAlpha(&mPerfectNk[0]);
    fopMsgM_setAlpha(&mDmpe);
    fopMsgM_setAlpha(&mYouGot[0]);

    int i;
    for (i = 0; i < dMgtem_remain_tex; i++) {
        fopMsgM_setAlpha(&mRemain[i]);
        fopMsgM_setAlpha(&mRemainK[i]);
    }

    for (i = 0; i < 8; i++) {
        fopMsgM_setAlpha(&mNum[i]);
        fopMsgM_setAlpha(&mNumK[i]);
    }

    fopMsgM_setAlpha(&mRpx);
    fopMsgM_setAlpha(&mRpxk);
    fopMsgM_setAlpha(&mNt00);
    fopMsgM_setAlpha(&mNk00);
}

/* 802081F8-80208230       .text getRotate__26dDlst_TerminaterScrnDraw_cFP18fopMsgM_pane_class */
void dDlst_TerminaterScrnDraw_c::getRotate(fopMsgM_pane_class* i_pane) {
    f32 rot = i_pane->pane->getRotate();
    if (rot > 180.0f) {
        rot -= 360.0f;
    }
    i_pane->mUserArea = (s16)rot;
}

/* 80208230-802082D0       .text setRotate__26dDlst_TerminaterScrnDraw_cFP18fopMsgM_pane_classf */
void dDlst_TerminaterScrnDraw_c::setRotate(fopMsgM_pane_class* i_pane, f32 i_angle) {
    i_pane->pane->rotate((int)(i_pane->mSize.x / 2), (int)(i_pane->mSize.y / 2), ROTATE_Z, i_angle);
}

/* 802082D0-80208324       .text setScale__26dDlst_TerminaterScrnDraw_cFP18fopMsgM_pane_classf */
void dDlst_TerminaterScrnDraw_c::setScale(fopMsgM_pane_class* i_pane, f32 i_scale) {
    i_pane->mPosCenter.x = i_pane->mPosCenterOrig.x * i_scale;
    i_pane->mPosCenter.y = i_pane->mPosCenterOrig.y * i_scale;
    i_pane->mSize.x = i_pane->mSizeOrig.x * i_scale;
    i_pane->mSize.y = i_pane->mSizeOrig.y * i_scale;
    fopMsgM_cposMove(i_pane);
}

/* 80208324-80208440       .text animeF1__26dDlst_TerminaterScrnDraw_cFi */
BOOL dDlst_TerminaterScrnDraw_c::animeF1(int i_no) {
    BOOL rt = FALSE;

    if (mRemain[i_no].mUserArea < 26) {
        mRemain[i_no].mUserArea++;
        f32 t = acc(26, mRemain[i_no].mUserArea, 0);
        f32 rot = 720.0f * (1.0f - t) + (f32)mFailed[i_no].mUserArea;
        fopMsgM_paneTrans(&mFailed[i_no], -150.0f * (1.0f - t), 0.0f);
        setRotate(&mFailed[i_no], rot);
        fopMsgM_setNowAlpha(&mFailed[i_no], t);
    } else {
        rt = TRUE;
    }

    return rt;
}

/* 80208440-80208E38       .text animeF2__26dDlst_TerminaterScrnDraw_cFi */
BOOL dDlst_TerminaterScrnDraw_c::animeF2(int i_no) {
    static const s16 animeFrame[] = {26, 29, 31, 34, 37, 40, 43};
    static const s16 transX[] = {-150, 10, -12, -28, -28, -15, 2, 0};
    static const s16 transY[] = {0, 0, -60, 0, 5, 5, 14, 0};
    static const s16 rot[] = {0, 10, 0, 0, -20, -10, 0, 10};

    BOOL rt = FALSE;
    if (mRemain[i_no].mUserArea < animeFrame[6]) {
        mRemain[i_no].mUserArea++;
        s16 timer = mRemain[i_no].mUserArea;
        f32 t;
        f32 x;
        f32 y;
        f32 r;

        if (timer <= animeFrame[0]) {
            t = acc(animeFrame[0], timer, 0);
            x = transX[1] + (1.0f - t) * (f32)(transX[0] - transX[1]);
            y = transY[1] + (1.0f - t) * (f32)(transY[0] - transY[1]);
            r = 710.0f * (1.0f - t) + (f32)rot[1];
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
            fopMsgM_setNowAlpha(&mFailed[i_no], t);
        } else if (timer <= animeFrame[1]) {
            t = acc(animeFrame[1], timer, animeFrame[0]);
            x = transX[2] + (1.0f - t) * (f32)(transX[1] - transX[2]);
            y = transY[2] + (1.0f - t) * (f32)(transY[1] - transY[2]);
            r = rot[2] + (1.0f - t) * (f32)(rot[1] - rot[2]);
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
        } else if (timer <= animeFrame[2]) {
            t = acc(animeFrame[2], timer, animeFrame[1]);
            x = transX[3] + (1.0f - t) * (f32)(transX[2] - transX[3]);
            y = transY[3] + (1.0f - t) * (f32)(transY[2] - transY[3]);
            r = rot[3] + (1.0f - t) * (f32)(rot[2] - rot[3]);
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
        } else if (timer <= animeFrame[3]) {
            t = acc(animeFrame[3], timer, animeFrame[2]);
            x = transX[4] + (1.0f - t) * (f32)(transX[3] - transX[4]);
            y = transY[4] + (1.0f - t) * (f32)(transY[3] - transY[4]);
            r = rot[4] + (1.0f - t) * (f32)(rot[3] - rot[4]);
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
        } else if (timer <= animeFrame[4]) {
            t = acc(animeFrame[4], timer, animeFrame[3]);
            x = transX[5] + (1.0f - t) * (f32)(transX[4] - transX[5]);
            y = transY[5] + (1.0f - t) * (f32)(transY[4] - transY[5]);
            r = rot[5] + (1.0f - t) * (f32)(rot[4] - rot[5]);
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
        } else if (timer <= animeFrame[5]) {
            t = acc(animeFrame[5], timer, animeFrame[4]);
            x = transX[6] + (1.0f - t) * (f32)(transX[5] - transX[6]);
            y = transY[6] + (1.0f - t) * (f32)(transY[5] - transY[6]);
            r = rot[6] + (1.0f - t) * (f32)(rot[5] - rot[6]);
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
        } else if (timer <= animeFrame[6]) {
            t = acc(animeFrame[6], timer, animeFrame[5]);
            x = transX[7] + (1.0f - t) * (f32)(transX[6] - transX[7]);
            y = transY[7] + (1.0f - t) * (f32)(transY[6] - transY[7]);
            r = rot[7] + (1.0f - t) * (f32)(rot[6] - rot[7]);
            fopMsgM_paneTrans(&mFailed[i_no], x, y);
            setRotate(&mFailed[i_no], r);
        }
    } else {
        rt = TRUE;
    }

    return rt;
}

/* 80208E38-802091D8       .text animeF3__26dDlst_TerminaterScrnDraw_cFi */
BOOL dDlst_TerminaterScrnDraw_c::animeF3(int i_no) {
    static const s16 animeFrame[] = {5, 10};
    static const s16 transY[] = {0, -25, 150};
    static const s16 rot[] = {40, 30, 20, -20, -30, -40};

    BOOL rt = FALSE;
    if (mRemainK[i_no].mUserArea == 0) {
        int ridx = (int)cM_rndF(6.0f);
        if (ridx > 5) {
            ridx = 5;
        }
        mRemain[i_no].mUserArea = rot[ridx];
    }

    if (mRemainK[i_no].mUserArea < animeFrame[1]) {
        mRemainK[i_no].mUserArea++;

        f32 t, y, r;
        if (mRemainK[i_no].mUserArea <= animeFrame[0]) {
            t = acc(animeFrame[0], mRemainK[i_no].mUserArea, 0);
            y = transY[1] + (1.0f - t) * (f32)(transY[0] - transY[1]);
            r = SQUARE((f32)mRemainK[i_no].mUserArea) / SQUARE((f32)animeFrame[1]);
            r = mFailed[i_no].mUserArea + mRemain[i_no].mUserArea * r;
            fopMsgM_paneTrans(&mFailed[i_no], 0.0f, y);
            setRotate(&mFailed[i_no], r);
        } else {
            t = acc(animeFrame[1], mRemainK[i_no].mUserArea, animeFrame[0]);
            y = transY[2] + (1.0f - t) * (f32)(transY[1] - transY[2]);
            r = mFailed[i_no].mUserArea + mRemain[i_no].mUserArea * acc(animeFrame[1], mRemainK[i_no].mUserArea, 0);
            fopMsgM_paneTrans(&mFailed[i_no], 0.0f, y);
            fopMsgM_setNowAlpha(&mFailed[i_no], 1.0f - t);
            setRotate(&mFailed[i_no], r);
        }
    } else {
        rt = TRUE;
    }

    return rt;
}

/* 802091D8-80209420       .text animeS1__26dDlst_TerminaterScrnDraw_cFv */
BOOL dDlst_TerminaterScrnDraw_c::animeS1() {
    static const s16 animeFrame[] = {8, 10};

    BOOL rt = FALSE;
    f32 base = (mYouGot[0].mPosTopLeftOrig.x + (mYouGot[0].mPosTopLeftOrig.x + mYouGot[5].mPosTopLeftOrig.x + mYouGot[5].mSizeOrig.x)) / 2.0f;

    if (mYouGot[0].mUserArea < animeFrame[1]) {
        mYouGot[0].mUserArea++;
    } else {
        rt = TRUE;
    }

    s16 timer = mYouGot[0].mUserArea;
    f32 t;
    f32 alpha;
    f32 scale;
    if (timer <= animeFrame[0]) {
        alpha = 0.0f;
        t = acc(animeFrame[0], timer, 0);
        scale = 0.3f + (1.2f - 0.3f) * t;
    } else {
        t = acc(animeFrame[1], timer, animeFrame[0]);
        alpha = t;
        scale = 1.2f + (1.0f - 1.2f) * t;
    }

    for (int i = 0; i < dMgtem_yougot_tex; i++) {
        if (i != 0) {
            mYouGot[i].mPosCenter.x = mYouGot[i].mPosCenterOrig.x * scale;
            mYouGot[i].mPosCenter.y = mYouGot[i].mPosCenterOrig.y * scale;
        } else {
            mYouGot[i].mPosCenter.x = base - (base - mYouGot[i].mPosCenterOrig.x) * scale;
            mYouGot[i].mPosCenter.y = mYouGot[i].mPosCenterOrig.y;
        }
        mYouGot[i].mSize.x = mYouGot[i].mSizeOrig.x * scale;
        mYouGot[i].mSize.y = mYouGot[i].mSizeOrig.y * scale;
        fopMsgM_cposMove(&mYouGot[i]);
        fopMsgM_setNowAlpha(&mYouGot[i], alpha);
    }

    return rt;
}

/* 80209420-802094AC       .text animeS2__26dDlst_TerminaterScrnDraw_cFP18fopMsgM_pane_class */
BOOL dDlst_TerminaterScrnDraw_c::animeS2(fopMsgM_pane_class* i_pane) {
    BOOL rt = FALSE;
    i_pane->mUserArea++;
    if (i_pane->mUserArea <= 7) {
        f32 t = SQUARE((f32)i_pane->mUserArea);
        t /= 49.0f;
        fopMsgM_setNowAlpha(i_pane, t);
    } else {
        rt = TRUE;
    }
    return rt;
}

/* 802094AC-80209AAC       .text animeS3__26dDlst_TerminaterScrnDraw_cFv */
BOOL dDlst_TerminaterScrnDraw_c::animeS3() {
    static const s16 animeFrame[] = {5, 7, 9, 11, 13};
    static const s16 transY[] = {-110, 0, -30, 10, 15, 0};
    static const s16 rot[] = {0, 0, 5, -5, 0, 0};

    BOOL rt = FALSE;
    if (mDummy.mUserArea < animeFrame[4]) {
        mDummy.mUserArea++;

        f32 t;
        f32 y;
        f32 r;
        if (mDummy.mUserArea <= animeFrame[0]) {
            t = acc(animeFrame[0], mDummy.mUserArea, 0);
            y = (f32)transY[1];
            y += (1.0f - t) * (f32)(transY[0] - transY[1]);
            fopMsgM_paneTrans(&mDummy, 0.0f, y);
            fopMsgM_setNowAlpha(&mRpx, t);
            fopMsgM_setNowAlpha(&mRpxk, t);
            fopMsgM_setNowAlpha(&mNt00, t);
            fopMsgM_setNowAlpha(&mNk00, t);
            if (mDummy.mUserArea == animeFrame[0]) {
                mDoAud_seStart(JA_SE_SGAME_DISP_LUPY);
            }
        } else if (mDummy.mUserArea <= animeFrame[1]) {
            t = acc(animeFrame[1], mDummy.mUserArea, animeFrame[0]);
            y = transY[2] + (1.0f - t) * (f32)(transY[1] - transY[2]);
            r = rot[2] + (1.0f - t) * (f32)(rot[1] - rot[2]);
            fopMsgM_paneTrans(&mDummy, 0.0f, y);
            mDummy.pane->rotate(0.0f, 0.0f, ROTATE_Z, r);
        } else if (mDummy.mUserArea <= animeFrame[2]) {
            t = acc(animeFrame[2], mDummy.mUserArea, animeFrame[1]);
            y = transY[3] + (1.0f - t) * (f32)(transY[2] - transY[3]);
            r = rot[3] + (1.0f - t) * (f32)(rot[2] - rot[3]);
            fopMsgM_paneTrans(&mDummy, 0.0f, y);
            mDummy.pane->rotate(0.0f, 0.0f, ROTATE_Z, r);
        } else if (mDummy.mUserArea <= animeFrame[3]) {
            t = acc(animeFrame[3], mDummy.mUserArea, animeFrame[2]);
            y = transY[4] + (1.0f - t) * (f32)(transY[3] - transY[4]);
            r = rot[4] + (1.0f - t) * (f32)(rot[3] - rot[4]);
            fopMsgM_paneTrans(&mDummy, 0.0f, y);
            mDummy.pane->rotate(0.0f, 0.0f, ROTATE_Z, r);
        } else if (mDummy.mUserArea <= animeFrame[4]) {
            t = acc(animeFrame[4], mDummy.mUserArea, animeFrame[3]);
            y = (f32)transY[5];
            y += (1.0f - t) * (f32)(transY[4] - transY[5]);
            fopMsgM_paneTrans(&mDummy, 0.0f, y);
        }
    } else {
        rt = TRUE;
    }

    return rt;
}

/* 80209AAC-80209B64       .text animeFailed1__26dDlst_TerminaterScrnDraw_cFv */
BOOL dDlst_TerminaterScrnDraw_c::animeFailed1() {
    for (int i = 0; i < dMgtem_failed_tex; i++) {
        int start = i * 3;
        if (i < dMgtem_failed_tex - 1) {
            if (mAnimTimer1 >= start && mAnimTimer1 < start + 26) {
                animeF1(i);
            }
        } else {
            if (mAnimTimer1 >= start && mAnimTimer1 < start + 44) {
                animeF2(i);
            }
        }
    }

    mAnimTimer1++;
    return FALSE;
}

/* 80209B64-80209BEC       .text animeFailed2__26dDlst_TerminaterScrnDraw_cFv */
BOOL dDlst_TerminaterScrnDraw_c::animeFailed2() {
    for (int i = 0; i < dMgtem_failed_tex; i++) {
        int start = i * 3;
        if (mAnimTimer2 >= start && mAnimTimer2 < start + 10) {
            animeF3(dMgtem_failed_tex - 1 - i);
        }
    }

    mAnimTimer2++;
    return FALSE;
}

/* 80209BEC-80209D6C       .text animeSuccess__26dDlst_TerminaterScrnDraw_cFv */
BOOL dDlst_TerminaterScrnDraw_c::animeSuccess() {
    if (mAnimTimer2 <= 10) {
        if (mAnimTimer2 == 0) {
            mDoAud_seStart(JA_SE_SGAME_YOU_GOT);
        }
        animeS1();
    }

    if (mAnimTimer2 <= 19) {
        for (int i = 0; i < dMgtem_remain_tex; i++) {
            if (mAnimTimer2 >= i && mAnimTimer2 < i + 7) {
                animeS2(&mRemain[dMgtem_remain_tex - 1 - i]);
                animeS2(&mRemainK[dMgtem_remain_tex - 1 - i]);
            }
        }
    }

    if (mAnimTimer2 >= 2 && mAnimTimer2 <= 19) {
        for (int i = 0; i < 8; i++) {
            if (mAnimTimer2 >= i + 2 && mAnimTimer2 < i + 9) {
                animeS2(&mNum[7 - i]);
                animeS2(&mNumK[7 - i]);
            }
        }
    }

    if (mAnimTimer2 >= 19 && mAnimTimer2 <= 32) {
        animeS3();
    }

    mAnimTimer2++;
    return FALSE;
}

/* 80209D6C-8020A4D8       .text animePerfect__26dDlst_TerminaterScrnDraw_cFv */
BOOL dDlst_TerminaterScrnDraw_c::animePerfect() {
    static const s16 rot[] = {0, 7, 0, 5, 2, 7};

    JUtility::TColor color[8];

    s16 f0;
    s16 f1;
    s16 f2;
    s16 f3;
    s16 f4;
    s16 wait;
    int f5;
    wait = g_menuHIO.field_0xae;
    f0 = g_menuHIO.field_0xa4;
    f1 = f0 + g_menuHIO.field_0xa6;
    f2 = f1 + g_menuHIO.field_0xa8;
    f3 = f2 + g_menuHIO.field_0xaa;
    f4 = f3 + g_menuHIO.field_0xac;
    f5 = (s16)(f4 + wait * 2);

    if (mAnimTimer2 <= g_menuHIO.field_0xb0 + 0x20) {
        return FALSE;
    }

    if (mAnimTimer3 == 0) {
        mDoAud_seStart(JA_SE_SGAME_PERFECT);
    }
    mAnimTimer3++;

    if (mAnimTimer3 <= f0) {
        f32 t = acc(f0, mAnimTimer3, 0);
        f32 r = rot[0] + t * (f32)(rot[1] - rot[0]);
        f32 t2 = acc(f0, f0 - mAnimTimer3, 0);
        f32 scale = 1.0f + ((f32)g_menuHIO.field_0x8e / 100.0f - 1.0f) * t2;

        fopMsgM_setNowAlpha(&mPerfect[0], t);
        fopMsgM_setNowAlpha(&mPerfectNt[0], t);
        fopMsgM_setNowAlpha(&mPerfectNk[0], t);
        setRotate(&mDmpe, r);

        int i;
        for (i = 0; i < dMgtem_perfect_tex; i++) {
            setScale(&mPerfect[i], scale);
        }
        for (i = 0; i < 10; i++) {
            setScale(&mPerfectNt[i], scale);
            setScale(&mPerfectNk[i], scale);
        }
    } else if (mAnimTimer3 <= f1) {
        f32 t = acc(f1, mAnimTimer3, f0);
        f32 r = (f32)rot[1];
        r += t * (f32)(rot[2] - rot[1]);
        setRotate(&mDmpe, r);
    } else if (mAnimTimer3 <= f2) {
        f32 t = acc(f2, mAnimTimer3, f1);
        f32 r = (f32)rot[2];
        r += t * (f32)(rot[3] - rot[2]);
        setRotate(&mDmpe, r);
    } else if (mAnimTimer3 <= f3) {
        f32 t = acc(f3, mAnimTimer3, f2);
        f32 r = (f32)rot[3];
        r += t * (f32)(rot[4] - rot[3]);
        setRotate(&mDmpe, r);
    } else if (mAnimTimer3 <= f4) {
        f32 t = acc(f4, mAnimTimer3, f3);
        f32 r = (f32)rot[4];
        r += t * (f32)(rot[5] - rot[4]);
        setRotate(&mDmpe, r);
    } else {
        if (mAnimTimer3 >= f5) {
            mAnimTimer3 = f4;
        }

        u8 wrap = 0;
        f32 hue = 240.0f + -140.0f * ((f32)(mAnimTimer3 - f4) / (f32)wait);
        if (hue > 240.0f) {
            hue = 480.0f - hue;
            wrap = 1;
        }

        int i;
        for (i = 0; i < dMgtem_perfect_tex; i++) {
            f32 val;
            if (wrap != 0) {
                val = hue - 20.0f * (f32)i;
            } else {
                val = hue + 20.0f * (f32)i;
            }
            if (val > 240.0f) {
                val = 480.0f - val;
            }
            if (val < 100.0f) {
                val = 200.0f - val;
            }

            color[i].set(0xFF, (u8)val, 0, 0xFF);
            ((J2DPicture*)mPerfect[i].pane)->setWhite(color[i]);
        }
    }

    return FALSE;
}

/* 8020A4D8-8020A53C       .text draw__26dDlst_TerminaterScrnDraw_cFv */
void dDlst_TerminaterScrnDraw_c::draw() {
    J2DOrthoGraph* graf = dComIfGp_getCurrentGrafPort();
    graf->setPort();
    mpScrn->draw(0.0f, 0.0f, graf);
}

/* 8020A53C-8020A55C       .text dMinigame_Terminater_Draw__FP22dMinigame_Terminater_c */
static BOOL dMinigame_Terminater_Draw(dMinigame_Terminater_c* i_this) {
    return i_this->_draw();
}

/* 8020A55C-8020A57C       .text dMinigame_Terminater_Execute__FP22dMinigame_Terminater_c */
static BOOL dMinigame_Terminater_Execute(dMinigame_Terminater_c* i_this) {
    return i_this->_execute();
}

/* 8020A57C-8020A584       .text dMinigame_Terminater_IsDelete__FP22dMinigame_Terminater_c */
static BOOL dMinigame_Terminater_IsDelete(dMinigame_Terminater_c* i_this) {
    UNUSED(i_this);
    return TRUE;
}

/* 8020A584-8020A5A4       .text dMinigame_Terminater_Delete__FP22dMinigame_Terminater_c */
static BOOL dMinigame_Terminater_Delete(dMinigame_Terminater_c* i_this) {
    return i_this->_delete();
}

/* 8020A5A4-8020A5C4       .text dMinigame_Terminater_Create__FP9msg_class */
static cPhs_State dMinigame_Terminater_Create(msg_class* msg) {
    dMinigame_Terminater_c* i_this = (dMinigame_Terminater_c*)msg;
    return i_this->_create();
}

static msg_method_class l_dMinigame_Terminater_Method = {
    (process_method_func)dMinigame_Terminater_Create,
    (process_method_func)dMinigame_Terminater_Delete,
    (process_method_func)dMinigame_Terminater_Execute,
    (process_method_func)dMinigame_Terminater_IsDelete,
    (process_method_func)dMinigame_Terminater_Draw,
};

msg_process_profile_definition g_profile_MINIGAME_TERMINATER = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 12,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_MINIGAME_TERMINATER_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(dMinigame_Terminater_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopMsg_Method,
    /* Draw Prio    */ fpcDwPi_MINIGAME_TERMINATER_e,
    /* Msg SubMtd   */ &l_dMinigame_Terminater_Method,
};
