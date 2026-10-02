/**
 * d_msg.cpp
 * Textboxes (normal gameplay) (talking, item collection, etc.)
 */

#include "d/dolzel.h" // IWYU pragma: keep
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JUtility/JUTReport.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_file_error.h"
#include "d/d_meter.h"
#include "d/d_msg.h"
#include "d/d_s_play.h"
#include "f_op/f_op_msg_mng.h"
#include "d/actor/d_a_npc_bs1.h"
#include "dolphin/types.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_lib.h"
#include <stdio.h>

class J2DTextBox;
struct fopMsgM_pane_class;

#define dMsg_OUTFONT_MAX VERSION_SELECT(8, 8, 8, 15)

J2DPicture* button_icon[dMsg_OUTFONT_MAX];
J2DPicture* button_kage[dMsg_OUTFONT_MAX];
s16 buttonTimer[dMsg_OUTFONT_MAX];
J2DTextBox* numberPane[3];

MyScreen* sScreen;
static J2DScreen* sScreen2;
JUTFont* textFont;
JUTFont* rubyFont;
J2DPicture* arrowPane;
J2DPicture* maskPane;
f32 talkPosX;
f32 talkPosY;
s16 textOffsetY;
#if VERSION >= VERSION_USA
bool dMsg_font_flag;
#endif
JKRHeap* agb_work_area;
static dDlst_2DMSG_c message;
dDlst_2Dtact_c capture;
dDlst_2DM_c multiTex;

const char* mLayout[] = {
    "hukidashi_14_1.blo",
    "hukidashi_14_1.blo",
    "hukidashi_14_3.blo",
    "hukidashi_14_5.blo",
    "hukidashi_14_6.blo",
    "hukidashi_14_6.blo",
    "hukidashi_14_0_1.blo",
    "hukidashi_14_0_2.blo"
};

const u8 mBeatNum[] = {0x03, 0x04, 0x04, 0x06, 0x06, 0x03, 0x03, 0x04};

#if VERSION == VERSION_DEMO
#define dMsg_CHECK_TRIG_AB() (CPad_CHECK_TRIG_A(0) || CPad_CHECK_TRIG_A(3) || CPad_CHECK_TRIG_B(3) || CPad_CHECK_TRIG_B(0))
#else
#define dMsg_CHECK_TRIG_AB() (CPad_CHECK_TRIG_A(0) || CPad_CHECK_TRIG_B(0))
#endif

void dMsg_textPosition(sub_msg_class*);
void dMsg_mesgOutPos(sub_msg_class*);
void dMsg_setCloseSound(sub_msg_class*, u8);
void dMsg_setCancelMode(sub_msg_class*);

/* 8020A620-8020A628       .text dMsg_getAgbWorkArea__Fv */
JKRHeap* dMsg_getAgbWorkArea() {
    return agb_work_area;
}

/* 8020A628-8020A950       .text draw__13dDlst_2DMSG_cFv */
void dDlst_2DMSG_c::draw() {
    sub_msg_class* msg = mpMsg;
    J2DOrthoGraph* graf = dComIfGp_getCurrentGrafPort();
    graf->setPort();
    sScreen->draw(0.0f, 0.0f, graf);
    if (sScreen2 != NULL) {
        sScreen2->draw(0.0f, 0.0f, graf);
    }

    outFontDraw();

    if (msg->mStatus == fopMsgStts_SELECT_YOKO_e) {
        mpMsg->mMsgDataProc.selectArrow(arrowPane);
    } else if (msg->mStatus == fopMsgStts_INPUT_e) {
        if ((mpMsg->mMesgEntry.mTextboxType == 0) || (mpMsg->mMesgEntry.mTextboxType == 10)) {
            f32 dVar3 = (numberPane[0]->getWidth() + numberPane[0]->getCharSpace());
            if (msg->mMsgNo == 0x1cfa) {
                maskPane->draw(mpMsg->m10EC + (dVar3 * (2 - msg->mSelectNum)), mpMsg->m10F0, maskPane->getWidth(), maskPane->getHeight(), false, false, false);
                numberPane[2]->draw(mpMsg->m10EC, mpMsg->m10F0 + numberPane[2]->getHeight(), numberPane[2]->getWidth(), HBIND_CENTER);
                numberPane[1]->draw((mpMsg->m10EC + dVar3), mpMsg->m10F0 + numberPane[1]->getHeight(), numberPane[1]->getWidth(), HBIND_CENTER);
                numberPane[0]->draw(mpMsg->m10EC + (dVar3 * 2.0f), mpMsg->m10F0 + numberPane[0]->getHeight(), numberPane[0]->getWidth(), HBIND_CENTER);
            } else {
                numberPane[2]->hide();
                maskPane->draw(mpMsg->m10EC + (dVar3 * (1 - msg->mSelectNum)), mpMsg->m10F0, maskPane->getWidth(), maskPane->getHeight(), false, false, false);
                numberPane[1]->draw(mpMsg->m10EC, mpMsg->m10F0 + numberPane[1]->getHeight(), numberPane[1]->getWidth(), HBIND_CENTER);
                numberPane[0]->draw((mpMsg->m10EC + dVar3), mpMsg->m10F0 + numberPane[0]->getHeight(), numberPane[0]->getWidth(), HBIND_CENTER);
            }
        }
    }
}

/* 8020A950-8020AC40       .text outFontDraw__13dDlst_2DMSG_cFv */
void dDlst_2DMSG_c::outFontDraw() {
#if VERSION == VERSION_PAL
    sub_msg_class* msg = mpMsg;
#endif
    for (int i = 0; i < dMsg_OUTFONT_MAX; i++) {
        u8 iconNum = mpMsg->mMsgDataProc.getIconNum(i);
        int posX = mpMsg->mMsgDataProc.getIconPosX(i);
        int posY = mpMsg->mMsgDataProc.getIconPosY(i);
        int scale = mpMsg->mMsgDataProc.getIconScale(i);
        if (
            iconNum != fopMsgM_Icon_NONE_e &&
            iconNum != fopMsgM_Icon_SELECT_YOKO_LEFT_e &&
            iconNum != fopMsgM_Icon_SELECT_YOKO_RIGHT_e &&
            iconNum != fopMsgM_Icon_INPUT_e
        ) {
            J2DScreen* scrn = (J2DScreen*)mpMsg->m0544[0].pane;
            const JGeometry::TBox2<f32>& b = scrn->getBounds();
            int r5 = posX + b.i.x;
            int r6;
            u8 alpha;
            if (mpMsg->mMesgEntry.mTextboxType == 5) {
#if VERSION <= VERSION_JPN
                if (g_msgDHIO.field_0x08 == 0) {
                    f32 local_38 = mpMsg->m1104 * ((1 - mpMsg->m1108) + posY * 2);
                    r6 = (int)(local_38 + b.i.y);
                } else
#endif
                {
                f32 local_30 = g_msgHIO.field_0x5e * posY;
                f32 local_38 = mpMsg->m1104 * (2 - mpMsg->m1108);
                r6 = (int)(local_38 + b.i.y + local_30);
                }
                alpha = mpMsg->m0544[0].mNowAlpha;
            } else if (mpMsg->mMesgEntry.mTextboxType == 0xe) {
#if VERSION == VERSION_PAL
                if (msg->mMsgNo == 0x5b3 || msg->mMsgNo == 0x5b4) {
#endif
#if VERSION <= VERSION_JPN
                if (g_msgDHIO.field_0x08 == 0) {
                    f32 local_30 = mpMsg->m1104;
                    r6 = (int)(local_30 + b.i.y);
                } else
#endif
                {
                    f32 local_28 = g_msgHIO.field_0x5e * posY;
                    f32 local_30 = mpMsg->m1104;
                    r6 = (int)(local_30 + b.i.y + local_28);
                }
#if VERSION == VERSION_PAL
                } else {
                    f32 lineSpace = ((J2DTextBox*)scrn)->getLineSpace();
                    f32 local_28 = g_msgHIO.field_0x5e * posY;
                    r6 = (int)(local_28 + (b.i.y - mpMsg->m1108 * (lineSpace / 2.0f)));
                }
#endif
                alpha = mpMsg->m0544[0].mNowAlpha;
            } else {
#if VERSION <= VERSION_JPN
                if (g_msgDHIO.field_0x08 == 0) {
                    if (scale > mpMsg->m110C) {
                        if (mpMsg->m1108 > 1) {
                            f32 local_30 = mpMsg->m1104 * (2 - posY);
                            r6 = (int)((local_30 + b.i.y) - (f32)(int)(scale / 2));
                        } else {
                            f32 local_30 = mpMsg->m1104 * 3;
                            r6 = (int)((local_30 + b.i.y) - (f32)(int)(scale / 2));
                        }
                    } else {
                        f32 local_30 = mpMsg->m1104 * ((2 - mpMsg->m1108) + posY * 2);
                        r6 = (int)(local_30 + b.i.y);
                    }
                } else {
                    f32 local_28 = g_msgHIO.field_0x5e * posY;
                    f32 local_30 = mpMsg->m1104 * (3 - mpMsg->m1108);
                    r6 = (int)(local_30 + b.i.y + local_28);
                }
#else
                if (scale > mpMsg->m110C) {
                    if (mpMsg->m1108 > 1) {
                        int temp1 = (mpMsg->m1104 * ((3 - posY) - (mpMsg->m1108 + -2)));
                        r6 = ((scale - mpMsg->m110C) / 2) + (int)((temp1 + b.i.y) - (f32)(int)(scale / 2));
                    } else {
                        r6 = (int)(((mpMsg->m1104 << 2) + b.i.y) - (f32)(int)(scale / 2));
                    }
                } else {
                    f32 local_28 = g_msgHIO.field_0x5e * posY;
                    f32 local_30 = mpMsg->m1104 * (3 - mpMsg->m1108);
                    r6 = (int)(local_30 + b.i.y + local_28);
                }
#endif
                alpha = mpMsg->m0544[0].mNowAlpha;
            }
            fopMsgM_outFontDraw(button_icon[i], button_kage[i], r5, r6, scale, &buttonTimer[i], alpha, iconNum);
        }
    }
}

/* 8020AC40-8020AD3C       .text draw__14dDlst_2Dtact_cFv */
#if VERSION == VERSION_DEMO
void dDlst_2Dtact_c::draw() {
    int left;
    int top;
    int width;
    int height;
    int r27 = 0;
    int r28 = 0;
    int r26 = 372;
    int r25 = 72;

    J2DOrthoGraph* graf = dComIfGp_getCurrentGrafPort();
    graf->setPort();

    left = r27 * 1.0296875f + -9.0f;
    top = r28 * 1.0916667f + -21.0f;
    width = r26 * 1.0296875f;
    height = r25 * 1.0916667f;

    GXSetScissor(left, top, width, height);
    sScreen->draw(0.0f, 0.0f, graf);
    GXSetTexCopySrc(left, top, width, height);
    GXSetTexCopyDst(width, height, GX_TF_RGB565, GX_FALSE);
    GXCopyTex(field_0x8, REG1_S(6) == 0 ? GX_TRUE : GX_FALSE);
}
#else
void dDlst_2Dtact_c::draw() {
    int r27 = 0;
    int r28 = 0;
    int r26 = 372;
    int r25 = 72;

    J2DOrthoGraph* graf = dComIfGp_getCurrentGrafPort();
    graf->setPort();
    
    int left = r27 * 1.0296875f + -9.0f;
    int top = r28 * 1.0916667f + -21.0f;
    int width = r26 * 1.0296875f;
    int height = r25 * 1.0916667f;

    GXSetScissor(left, top, width, height);
    sScreen->draw(0.0f, 0.0f, graf);
    GXSetTexCopySrc(left, top, width, height);
    GXSetTexCopyDst(width, height, GX_TF_RGB565, GX_FALSE);
    GXCopyTex(field_0x8, REG1_S(6) == 0 ? GX_TRUE : GX_FALSE);
}
#endif

/* 8020AD3C-8020ADD8       .text dMsg_msg_pane_parts_set__FP18fopMsgM_pane_classUc */
void dMsg_msg_pane_parts_set(fopMsgM_pane_class* pPane, u8 param_2) {
#if VERSION == VERSION_DEMO
    switch (param_2) {
    case 1:
        pPane->mPosTopLeft.y = (480.0f - (pPane->mPosTopLeftOrig.y + pPane->mSizeOrig.y)) + -26.0f;
        break;
    case 2:
        pPane->mPosTopLeft.y = 240.0f - pPane->mSizeOrig.y / 2.0f;
        break;
    default:
        pPane->mPosTopLeft.y = pPane->mPosTopLeftOrig.y + 32.0f;
        break;
    }
    pPane->mPosCenter.x = pPane->mPosTopLeft.x + pPane->mSizeOrig.x / 2.0f;
    pPane->mPosCenter.y = pPane->mPosTopLeft.y + pPane->mSizeOrig.y / 2.0f;
#else
    f32 tmp;
    switch (param_2) {
    case 1:
        pPane->mPosTopLeft.y = (480.0f - (pPane->mPosTopLeftOrig.y + pPane->mSizeOrig.y)) + -26.0f;
        break;
    case 2:
        tmp = 0.5f;
        pPane->mPosTopLeft.y = 240.0f - pPane->mSizeOrig.y * tmp;
        break;
    default:
        pPane->mPosTopLeft.y = pPane->mPosTopLeftOrig.y + 32.0f;
        break;
    }
    tmp = 0.5f;
    pPane->mPosCenter.x = pPane->mPosTopLeft.x + pPane->mSizeOrig.x * tmp;
    pPane->mPosCenter.y = pPane->mPosTopLeft.y + pPane->mSizeOrig.y * tmp;
#endif
}

/* 8020ADD8-8020AE28       .text dMsg_arw_pane_parts_set__FP18fopMsgM_pane_classP18fopMsgM_pane_class */
void dMsg_arw_pane_parts_set(fopMsgM_pane_class* pDst, fopMsgM_pane_class* pSrc) {
#if VERSION == VERSION_DEMO
    pDst->mPosTopLeft.y = (pSrc->mPosTopLeft.y + pDst->mPosTopLeftOrig.y) - pSrc->mPosTopLeftOrig.y;
    pDst->mPosCenter.y = pDst->mPosTopLeft.y + pDst->mSizeOrig.y / 2.0f;
#else
    f32 tmp = 0.5f;
    pDst->mPosTopLeft.y = (pSrc->mPosTopLeft.y + pDst->mPosTopLeftOrig.y) - pSrc->mPosTopLeftOrig.y;
    pDst->mPosCenter.y = pDst->mPosTopLeft.y + pDst->mSizeOrig.y * tmp;
#endif
    fopMsgM_cposMove(pDst);
}

/* 8020AE28-8020B958       .text dMsg_screenDataSetTalk__FP13sub_msg_class */
void dMsg_screenDataSetTalk(sub_msg_class* i_Msg) {
    sScreen = new MyScreen();
    sScreen->set("hukidashi_00.blo", dComIfGp_getMsgArchive());
    fopMsgM_setPaneData(&i_Msg->m049C, sScreen->search('ms00'));
    fopMsgM_setPaneData(&i_Msg->mPane_Arrow, sScreen->search('yz00'));
    fopMsgM_setPaneData(&i_Msg->m050C, sScreen->search('dt00'));
    fopMsgM_setPaneData(&i_Msg->m0544[0], sScreen->search('tx02'));
    fopMsgM_setPaneData(&i_Msg->m0544[1], sScreen->search('tx03'));
    fopMsgM_setPaneData(&i_Msg->m0544[2], sScreen->search('txt0'));
    fopMsgM_setPaneData(&i_Msg->m0544[3], sScreen->search('tx01'));
    fopMsgM_setPaneData(&i_Msg->m011C[0], sScreen->search('txc2'));
    fopMsgM_setPaneData(&i_Msg->m011C[1], sScreen->search('tec3'));
    fopMsgM_setPaneData(&i_Msg->m011C[2], sScreen->search('txc0'));
    fopMsgM_setPaneData(&i_Msg->m011C[3], sScreen->search('txc1'));
    fopMsgM_setPaneData(&i_Msg->m01FC, sScreen->search('cur1'));
    fopMsgM_setPaneData(&i_Msg->m0234, sScreen->search('cur2'));
    fopMsgM_setPaneData(&i_Msg->m026C[0], sScreen->search('mc00'));
    fopMsgM_setPaneData(&i_Msg->m026C[1], sScreen->search('mc01'));
    fopMsgM_setPaneData(&i_Msg->m026C[2], sScreen->search('mc02'));
    fopMsgM_setPaneData(&i_Msg->m026C[3], sScreen->search('mc03'));
    fopMsgM_setPaneData(&i_Msg->m026C[4], sScreen->search('mc04'));
    fopMsgM_setPaneData(&i_Msg->m026C[5], sScreen->search('mc05'));
    fopMsgM_setPaneData(&i_Msg->m026C[6], sScreen->search('mc06'));
    fopMsgM_setPaneData(&i_Msg->m026C[7], sScreen->search('mc07'));
    fopMsgM_setPaneData(&i_Msg->m026C[8], sScreen->search('mc08'));
    fopMsgM_setPaneData(&i_Msg->m026C[9], sScreen->search('mc09'));
    for (s32 i = 0; i < 10; i++) {
        i_Msg->m026C[i].mInitAlpha = 0xaa;
    }
    ((J2DPicture*)i_Msg->m049C.pane)->setWhite(g_msgHIO.field_0x5);
    ((J2DPicture*)i_Msg->m049C.pane)->setBlack(g_msgHIO.field_0x9);
    ((J2DPicture*)i_Msg->m026C[0].pane)->setWhite(0);
    ((J2DPicture*)i_Msg->m026C[0].pane)->setBlack(0);
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFont(rubyFont);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFont(rubyFont);
    ((J2DTextBox*)i_Msg->m011C[0].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m011C[1].pane)->setFont(rubyFont);
    ((J2DTextBox*)i_Msg->m011C[2].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m011C[3].pane)->setFont(rubyFont);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFont(textFont);
    }
#if VERSION <= VERSION_JPN
    J2DTextBox::TFontSize fontSize;
    J2DTextBox::TFontSize rubySize;
    if (g_msgDHIO.field_0x08 == 0) {
        fontSize.mSizeX = (int)g_msgHIO.field_0x58;
        fontSize.mSizeY = (int)g_msgHIO.field_0x58;
        rubySize.mSizeX = g_msgHIO.field_0x68;
        rubySize.mSizeY = g_msgHIO.field_0x68;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
        ((J2DTextBox*)i_Msg->m011C[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m011C[1].pane)->setFontSize(rubySize);
        ((J2DTextBox*)i_Msg->m011C[2].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m011C[3].pane)->setFontSize(rubySize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    } else {
        textOffsetY = 4;
        fontSize.mSizeX = g_msgHIO.field_0x70;
        fontSize.mSizeY = g_msgHIO.field_0x70;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m011C[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m011C[2].pane)->setFontSize(fontSize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    }
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m011C[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m011C[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m011C[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m011C[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    if (g_msgDHIO.field_0x08 == 0) {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m011C[0].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m011C[1].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m011C[2].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m011C[3].pane)->setLineSpace(42.0f);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(42.0f);
        }
    } else {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
        ((J2DTextBox*)i_Msg->m011C[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
        ((J2DTextBox*)i_Msg->m011C[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
        }
    }
    fopMsgM_blendInit(&i_Msg->m01FC, "cursor_00_02.bti");
    fopMsgM_blendInit(&i_Msg->m0234, "cursor_00_02.bti");
#else
    textOffsetY = 2;
    J2DTextBox::TFontSize fontSize;
    fontSize.mSizeX = g_msgHIO.field_0x70;
    fontSize.mSizeY = g_msgHIO.field_0x70;
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
    ((J2DTextBox*)i_Msg->m011C[0].pane)->setFontSize(fontSize);
    ((J2DTextBox*)i_Msg->m011C[2].pane)->setFontSize(fontSize);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFontSize(fontSize);
        numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
    }
    maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m011C[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m011C[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m011C[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m011C[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
    ((J2DTextBox*)i_Msg->m011C[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
    ((J2DTextBox*)i_Msg->m011C[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
    }
    fopMsgM_blendInit(&i_Msg->m01FC, "cursor_00_02.bti");
    fopMsgM_blendInit(&i_Msg->m0234, "cursor_00_02.bti");
    J2DTextBox::TFontSize rubySize;
#endif
    ((J2DTextBox*)i_Msg->m0544[0].pane)->getFontSize(fontSize);
    i_Msg->m110C = (int)fontSize.mSizeX;
    i_Msg->m1110 = (int)rubySize.mSizeX; // !@bug uninitialized use
    i_Msg->m10BC.set(((J2DPicture*)i_Msg->m050C.pane)->getBlack());
    i_Msg->m10C4.set(((J2DPicture*)i_Msg->m050C.pane)->getWhite());
    i_Msg->m026C[0].mUserArea = 0;
}

/* 8020B958-8020CC6C       .text dMsg_screenDataSetItem__FP13sub_msg_class */
void dMsg_screenDataSetItem(sub_msg_class* i_Msg) {
    sScreen = new MyScreen();
    sScreen->set("hukidashi_09.blo", dComIfGp_getMsgArchive());
    fopMsgM_setPaneData(&i_Msg->m049C, sScreen->search('ms90'));
    fopMsgM_setPaneData(&i_Msg->mPane_Arrow, sScreen->search('yz90'));
    fopMsgM_setPaneData(&i_Msg->m050C, sScreen->search('dt90'));
    fopMsgM_setPaneData(&i_Msg->m0544[0], sScreen->search('tx92'));
    fopMsgM_setPaneData(&i_Msg->m0544[1], sScreen->search('tx93'));
    fopMsgM_setPaneData(&i_Msg->m0544[2], sScreen->search('tx90'));
    fopMsgM_setPaneData(&i_Msg->m0544[3], sScreen->search('tx91'));
    fopMsgM_setPaneData(&i_Msg->m0624[8], sScreen->search('item'));
    fopMsgM_setPaneData(&i_Msg->m0624[9], sScreen->search('rin1'));
    fopMsgM_setPaneData(&i_Msg->m0624[0], sScreen->search('lig1'));
    fopMsgM_setPaneData(&i_Msg->m0624[1], sScreen->search('lig2'));
    fopMsgM_setPaneData(&i_Msg->m0624[2], sScreen->search('lig3'));
    fopMsgM_setPaneData(&i_Msg->m0624[3], sScreen->search('lig4'));
    fopMsgM_setPaneData(&i_Msg->m0624[4], sScreen->search('lig5'));
    fopMsgM_setPaneData(&i_Msg->m0624[5], sScreen->search('lig6'));
    fopMsgM_setPaneData(&i_Msg->m0624[6], sScreen->search('lig7'));
    fopMsgM_setPaneData(&i_Msg->m0624[7], sScreen->search('lig8'));
    i_Msg->buffer_p = (ResTIMG*)i_Msg->mpHeap->alloc(0xc00, 0x20);
    JUT_ASSERT(VERSION_SELECT(652, 654, 661, 673), i_Msg->buffer_p != NULL);
    if ((i_Msg->mMesgEntry.mTextboxType == 9) && (dItem_data::getTexture(i_Msg->mMsgNo - 101) != NULL)) {
        JKRArchive* archive = dComIfGp_getItemIconArchive();
        JKRReadTypeResource(i_Msg->buffer_p, 0xc00, 'TIMG', dItem_data::getTexture(i_Msg->mMsgNo - 101), archive);
        ((J2DPicture*)i_Msg->m0624[8].pane)->changeTexture(i_Msg->buffer_p, 0);
    } else {
        for (s32 i = 0; i < 10; i++) {
            i_Msg->m0624[i].pane->hide();
        }
        i_Msg->m0624[8].mUserArea = 1;
        f32 dVar12 = (i_Msg->m049C.mPosCenterOrig.x - i_Msg->m0544[0].mPosCenterOrig.x);
        for (s32 i = 0; i < 4; i++) {
            i_Msg->m0544[i].mSize.x = 486.0f;
            fopMsgM_paneTrans(&i_Msg->m0544[i], dVar12, 0.0f);
            i_Msg->m0544[i].mPosTopLeftOrig.x = i_Msg->m0544[i].mPosTopLeft.x;
            i_Msg->m0544[i].mPosCenterOrig.x = i_Msg->m0544[i].mPosCenter.x;
            i_Msg->m0544[i].mSizeOrig.x = i_Msg->m0544[i].mSize.x;
        }
    }
    if (i_Msg->mMsgNo == 0x66) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x0A280A00));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0x28FF28FF));
    } else if (i_Msg->mMsgNo == 0x67) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x1414A000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0x32C8FFFF));
    } else if (i_Msg->mMsgNo == 0x68) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x50500000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFFFF00FF));
    } else if (i_Msg->mMsgNo == 0x69) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x50282800));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFF5050FF));
    } else if (i_Msg->mMsgNo == 0x6a) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x501E8200));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xBE78FFFF));
    } else if (i_Msg->mMsgNo == 0x6b) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x50280000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFF8C00FF));
    } else if (i_Msg->mMsgNo == 0x74) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFFFFFFFF));
#if VERSION > VERSION_DEMO
    } else if (i_Msg->mMsgNo == 0x7f) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x50500000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFFFF00FF));
    } else if ((((i_Msg->mMsgNo == 0x118) || (i_Msg->mMsgNo == 0x119)) || (i_Msg->mMsgNo == 0x11a)) || (i_Msg->mMsgNo == 0x11b || (i_Msg->mMsgNo == 0x11c))) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x501E8200));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xBE78FFFF));
    } else if (i_Msg->mMsgNo == 0x11d) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x50280000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFF8C00FF));
#endif
    } else if (i_Msg->mMsgNo == 0xd2) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0x50FFFFFF));
    } else if (i_Msg->mMsgNo == 0xd3) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xAAAAFFFF));
    } else if (i_Msg->mMsgNo == 0xd4) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFFC800FF));
    } else if (i_Msg->mMsgNo == 0xd5) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFF5050FF));
    } else if (i_Msg->mMsgNo == 0xd6) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xDC96FFFF));
    } else if (i_Msg->mMsgNo == 0xd7) {
        ((J2DPicture*)i_Msg->m0624[8].pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m0624[8].pane)->setWhite(JUtility::TColor(0xFFFFFFFF));
    }
#if VERSION > VERSION_DEMO
    if (i_Msg->mMsgNo == 0xc6) {
        i_Msg->m0624[8].mSize.x = 32.0f;
        i_Msg->m0624[8].mSize.y = 32.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 199) {
        i_Msg->m0624[8].mSize.x = 48.0f;
        i_Msg->m0624[8].mSize.y = 40.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 200) {
        i_Msg->m0624[8].mSize.x = 32.0f;
        i_Msg->m0624[8].mSize.y = 32.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 0xc9) {
        i_Msg->m0624[8].mSize.x = 32.0f;
        i_Msg->m0624[8].mSize.y = 32.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 0xca) {
        i_Msg->m0624[8].mSize.x = 16.0f;
        i_Msg->m0624[8].mSize.y = 40.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 0xcb) {
        i_Msg->m0624[8].mSize.x = 56.0f;
        i_Msg->m0624[8].mSize.y = 32.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 0xcc) {
        i_Msg->m0624[8].mSize.x = 40.0f;
        i_Msg->m0624[8].mSize.y = 32.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    } else if (i_Msg->mMsgNo == 0xcd) {
        i_Msg->m0624[8].mSize.x = 40.0f;
        i_Msg->m0624[8].mSize.y = 40.0f;
        fopMsgM_paneTrans(&i_Msg->m0624[8], 0.0f, 0.0f);
    }
#endif
    if (i_Msg->mMesgEntry.mTextboxType == 1) {
        ((J2DPicture*)i_Msg->m049C.pane)->setBlack(JUtility::TColor(0x00000000));
        ((J2DPicture*)i_Msg->m049C.pane)->setWhite(JUtility::TColor(0xFFFFFFFF));
        ((J2DPicture*)i_Msg->m049C.pane)->setCornerColor(
            JUtility::TColor(0x3232B4FF),
            JUtility::TColor(0x3232B4FF),
            JUtility::TColor(0x3232B496),
            JUtility::TColor(0x646478FF)
        );
    }
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFont(rubyFont);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFont(rubyFont);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFont(textFont);
    }
#if VERSION <= VERSION_JPN
    J2DTextBox::TFontSize fontSize;
    J2DTextBox::TFontSize rubySize;
    if (g_msgDHIO.field_0x08 == 0) {
        fontSize.mSizeX = (int)g_msgHIO.field_0x58;
        fontSize.mSizeY = (int)g_msgHIO.field_0x58;
        rubySize.mSizeX = g_msgHIO.field_0x68;
        rubySize.mSizeY = g_msgHIO.field_0x68;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    } else {
        textOffsetY = 4;
        fontSize.mSizeX = g_msgHIO.field_0x70;
        fontSize.mSizeY = g_msgHIO.field_0x70;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    }
    rubySize.mSizeX = g_msgHIO.field_0x68;
    rubySize.mSizeY = g_msgHIO.field_0x68;
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    if (g_msgDHIO.field_0x08 == 0) {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setLineSpace(42.0f);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(42.0f);
        }
    } else {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
        }
    }
#else
    textOffsetY = 2;
    J2DTextBox::TFontSize fontSize;
    fontSize.mSizeX = g_msgHIO.field_0x70;
    fontSize.mSizeY = g_msgHIO.field_0x70;
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFontSize(fontSize);
        numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
    }
    maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    J2DTextBox::TFontSize rubySize;
    rubySize.mSizeX = g_msgHIO.field_0x68;
    rubySize.mSizeY = g_msgHIO.field_0x68;
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
    }
#endif
    ((J2DTextBox*)i_Msg->m0544[0].pane)->getFontSize(fontSize);
    i_Msg->m110C = (int)fontSize.mSizeX;
    i_Msg->m1110 = (int)rubySize.mSizeX;
    i_Msg->m10BC.set(((J2DPicture*)i_Msg->m050C.pane)->getBlack());
    i_Msg->m10C4.set(((J2DPicture*)i_Msg->m050C.pane)->getWhite());
    i_Msg->m026C[0].mUserArea = 0;
}

/* 8020CC6C-8020D160       .text dMsg_screenDataSetDemo__FP13sub_msg_class */
void dMsg_screenDataSetDemo(sub_msg_class* i_Msg) {
    sScreen = new MyScreen();
    sScreen->set("hukidashi_05.blo", dComIfGp_getMsgArchive());
    fopMsgM_setPaneData(&i_Msg->m0544[0], sScreen->search('tx82'));
    fopMsgM_setPaneData(&i_Msg->m0544[1], sScreen->search('tx83'));
    fopMsgM_setPaneData(&i_Msg->m0544[2], sScreen->search('tx80'));
    fopMsgM_setPaneData(&i_Msg->m0544[3], sScreen->search('tx81'));
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFont(rubyFont);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFont(rubyFont);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFont(textFont);
    }

    J2DTextBox::TFontSize fontSize;
    J2DTextBox::TFontSize rubySize;
#if VERSION <= VERSION_JPN
    if (g_msgDHIO.field_0x08 == 0) {
        fontSize.mSizeX = (int)g_msgHIO.field_0x58;
        fontSize.mSizeY = (int)g_msgHIO.field_0x58;
        rubySize.mSizeX = g_msgHIO.field_0x68;
        rubySize.mSizeY = g_msgHIO.field_0x68;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    } else
#endif
    {
        textOffsetY = VERSION_SELECT(4, 4, 2, 2);
        fontSize.mSizeX = g_msgHIO.field_0x70;
        fontSize.mSizeY = g_msgHIO.field_0x70;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    }

    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    
#if VERSION <= VERSION_JPN
    if (g_msgDHIO.field_0x08 == 0) {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setLineSpace(42.0f);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(42.0f);
        }
    } else
#endif
    {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
        }
    }
    
    i_Msg->m110C = (int)fontSize.mSizeX;
    i_Msg->m1110 = (int)rubySize.mSizeX; // !@bug uninitialized use
    i_Msg->m026C[0].mUserArea = 0;
}

/* 8020D160-8020DAC0       .text dMsg_screenDataSetTact__FP13sub_msg_class */
void dMsg_screenDataSetTact(sub_msg_class* i_Msg) {
    static const u32 ar_t[] = {'ar01', 'ar02', 'ar03', 'ar04', 'ar05', 'ar06'};
    static const u32 wn_t[] = {'wn01', 'wn02', 'wn03', 'wn04', 'wn05', 'wn06'};
    static const u32 bs_t[] = {'bs01', 'bs02', 'bs03', 'bs04', 'bs05', 'bs06'};
    static const u32 i0_t[] = {'i011', 'i021', 'i031', 'i041', 'i051', 'i061'};

    sScreen = new MyScreen();
    sScreen->set("hukidashi_14.blo", dComIfGp_getMsgArchive());
    if ((i_Msg->mMsgNo >= 0x5ac) && (i_Msg->mMsgNo <= 0x5b2)) {
        fopMsgM_setPaneData(&i_Msg->m049C, sScreen, 'm141');
        fopMsgM_setPaneData(&i_Msg->m0544[0], sScreen, 't141');
        fopMsgM_setPaneData(&i_Msg->m0544[1], sScreen, 'r141');
        fopMsgM_setPaneData(&i_Msg->m0544[2], sScreen, 'k141');
        fopMsgM_setPaneData(&i_Msg->m0544[3], sScreen, 'b141');
        sScreen->search('m142')->hide();
        sScreen->search('t142')->hide();
        sScreen->search('r142')->hide();
        sScreen->search('k142')->hide();
        sScreen->search('b142')->hide();
    } else {
        fopMsgM_setPaneData(&i_Msg->m049C, sScreen, 'm142');
        fopMsgM_setPaneData(&i_Msg->m0544[0], sScreen, 't142');
        fopMsgM_setPaneData(&i_Msg->m0544[1], sScreen, 'r142');
        fopMsgM_setPaneData(&i_Msg->m0544[2], sScreen, 'k142');
        fopMsgM_setPaneData(&i_Msg->m0544[3], sScreen, 'b142');
        sScreen->search('m141')->hide();
        sScreen->search('t141')->hide();
        sScreen->search('r141')->hide();
        sScreen->search('k141')->hide();
        sScreen->search('b141')->hide();
    }
    if ((i_Msg->mMsgNo == 0x5b3) || (i_Msg->mMsgNo == 0x5b4)) {
        JUT_ASSERT(VERSION_SELECT(976, 1033, 1036, 1048), dComIfGp_getMelodyNum() <= 7);
        sScreen2 = new J2DScreen();
        sScreen2->set(mLayout[dComIfGp_getMelodyNum()], dComIfGp_getMsgArchive());
        for (s32 i = 0; i < mBeatNum[dComIfGp_getMelodyNum()]; i++) {
            fopMsgM_setPaneData(&i_Msg->m08C4[i], sScreen2, ar_t[i]);
            fopMsgM_setPaneData(&i_Msg->m0A14[i], sScreen2, wn_t[i]);
            fopMsgM_setPaneData(&i_Msg->m0B64[i], sScreen2, bs_t[i]);
            if (dComIfGp_getMelodyNum() >= 5) {
                fopMsgM_setPaneData(&i_Msg->m0CB4[i], sScreen2, i0_t[i]);
            }
        }
    }
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFont(rubyFont);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFont(textFont);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFont(rubyFont);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFont(textFont);
    }
#if VERSION <= VERSION_JPN
    J2DTextBox::TFontSize fontSize;
    J2DTextBox::TFontSize rubySize;
    if (g_msgDHIO.field_0x08 == 0) {
        fontSize.mSizeX = (int)g_msgHIO.field_0x58;
        fontSize.mSizeY = (int)g_msgHIO.field_0x58;
        rubySize.mSizeX = g_msgHIO.field_0x68;
        rubySize.mSizeY = g_msgHIO.field_0x68;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    } else {
        textOffsetY = 4;
        fontSize.mSizeX = g_msgHIO.field_0x70;
        fontSize.mSizeY = g_msgHIO.field_0x70;
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setFontSize(fontSize);
            numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
        }
        maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    }
    rubySize.mSizeX = g_msgHIO.field_0x68;
    rubySize.mSizeY = g_msgHIO.field_0x68;
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    if (g_msgDHIO.field_0x08 == 0) {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[1].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(42.0f);
        ((J2DTextBox*)i_Msg->m0544[3].pane)->setLineSpace(42.0f);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(42.0f);
        }
    } else {
        ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
        ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
        for (s32 i = 0; i < 3; i++) {
            numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
        }
    }
#else
#if VERSION == VERSION_PAL
    if (i_Msg->mMsgNo == 0x5b3 || i_Msg->mMsgNo == 0x5b4) {
        textOffsetY = 8;
    } else
#endif
    {
        textOffsetY = 2;
    }
    J2DTextBox::TFontSize fontSize;
    fontSize.mSizeX = g_msgHIO.field_0x70;
    fontSize.mSizeY = g_msgHIO.field_0x70;
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setFontSize(fontSize);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setFontSize(fontSize);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setFontSize(fontSize);
        numberPane[i]->resize(fontSize.mSizeX, fontSize.mSizeY);
    }
    maskPane->resize(fontSize.mSizeX, fontSize.mSizeY);
    J2DTextBox::TFontSize rubySize;
    rubySize.mSizeX = g_msgHIO.field_0x68;
    rubySize.mSizeY = g_msgHIO.field_0x68;
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setFontSize(rubySize);
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setCharSpace(g_msgHIO.field_0x5c);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setCharSpace(g_msgHIO.field_0x5a);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setCharSpace(g_msgHIO.field_0x5c);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setCharSpace(g_msgHIO.field_0x5a);
    }
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setLineSpace(g_msgHIO.field_0x5e);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setLineSpace(g_msgHIO.field_0x5e);
    for (s32 i = 0; i < 3; i++) {
        numberPane[i]->setLineSpace(g_msgHIO.field_0x5e);
    }
#endif
    ((J2DTextBox*)i_Msg->m0544[0].pane)->getFontSize(fontSize);
    i_Msg->m110C = (int)fontSize.mSizeX;
    i_Msg->m1110 = (int)rubySize.mSizeX;
    i_Msg->m1168 = mDoAud_tact_getBeat();
    i_Msg->m1144 = 0;
    i_Msg->buffer_p = (ResTIMG*)i_Msg->mpHeap->alloc(0x1a280, 0x20);
    JUT_ASSERT(VERSION_SELECT(1070, 1127, 1128, 1145), i_Msg->buffer_p != NULL);
}

/* 8020DAC0-8020DC78       .text dMsg_screenDataSet__FP13sub_msg_class */
void dMsg_screenDataSet(sub_msg_class* i_Msg) {
    sScreen2 = NULL;
#if VERSION >= VERSION_USA
    if (fopMsgM_hyrule_language_check(i_Msg->mMsgNo)) {
        textFont = mDoExt_getRubyFont();
        JUT_ASSERT(VERSION_SELECT(1162, 1162, 1162, 1179), textFont != NULL);
        dMsg_font_flag = 1;
    } else {
        textFont = mDoExt_getMesgFont();
        JUT_ASSERT(VERSION_SELECT(1167, 1167, 1167, 1184), textFont != NULL);
        dMsg_font_flag = 0;
    }
    rubyFont = textFont;
#else
    textFont = mDoExt_getMesgFont();
    JUT_ASSERT(VERSION_SELECT(1100, 1157, 1157, 1157), textFont != NULL);
    rubyFont = mDoExt_getRubyFont();
    JUT_ASSERT(VERSION_SELECT(1102, 1159, 1159, 1159), rubyFont != NULL);
#endif
    for (s32 i = 0; i < 3; i++) {
#if VERSION <= VERSION_JPN
        numberPane[i] = new J2DTextBox("rock_24_20_4i1.bfn", "０");
#else
        numberPane[i] = new J2DTextBox("rock_24_20_4i_usa.bfn", "0");
#endif
#if VERSION == VERSION_PAL
        JUT_ASSERT(1202, numberPane[i] != NULL);
#endif
    }
    maskPane = new J2DPicture("black.bti");
#if VERSION == VERSION_PAL
    JUT_ASSERT(1208, maskPane != NULL);
#endif
    switch (i_Msg->mMesgEntry.mTextboxType) {
    case 5:
        dMsg_screenDataSetDemo(i_Msg);
        break;
    case 1:
    case 9:
        dMsg_screenDataSetItem(i_Msg);
        break;
    case 0xe:
        dMsg_screenDataSetTact(i_Msg);
        break;
    default:
        dMsg_screenDataSetTalk(i_Msg);
        break;
    }
}

/* 8020DC78-8020DD10       .text dMsg_messagePaneShow__FP13sub_msg_class */
void dMsg_messagePaneShow(sub_msg_class* i_Msg) {
    for (s32 i = 0; i < 4; i++) {
        i_Msg->m0544[i].pane->show();
        fopMsgM_setInitAlpha(&i_Msg->m0544[i]);
    }
    if ((i_Msg->mMesgEntry.mTextboxType == 9) || (i_Msg->mMesgEntry.mTextboxType == 1)) {
        for (s32 i = 0; i < 10; i++) {
            fopMsgM_setInitAlpha(&i_Msg->m0624[i]);
        }
    }
}

/* 8020DD10-8020DE1C       .text dMsg_messagePaneHide__FP13sub_msg_class */
void dMsg_messagePaneHide(sub_msg_class* i_Msg) {
    for (s32 i = 0; i < 4; i++) {
        i_Msg->m0544[i].pane->hide();
    }
    if ((i_Msg->mMesgEntry.mTextboxType == 9) || (i_Msg->mMesgEntry.mTextboxType == 1)) {
        for (s32 i = 0; i < 10; i++) {
            fopMsgM_setNowAlphaZero(&i_Msg->m0624[i]);
        }
    } else if ((i_Msg->mMesgEntry.mTextboxType == 0xe) && (i_Msg->mMsgNo == 0x5b3 || (i_Msg->mMsgNo == 0x5b4))) {
        for (s32 i = 0; i < mBeatNum[dComIfGp_getMelodyNum()]; i++) {
            fopMsgM_setNowAlphaZero(&i_Msg->m08C4[i]);
            fopMsgM_setNowAlphaZero(&i_Msg->m0A14[i]);
            fopMsgM_setNowAlphaZero(&i_Msg->m0B64[i]);
            if (dComIfGp_getMelodyNum() >= 5) {
                fopMsgM_setNowAlphaZero(&i_Msg->m0CB4[i]);
            }
        }
    }
}

/* 8020DE1C-8020DEDC       .text dMsg_outFontHide__FP13sub_msg_class */
void dMsg_outFontHide(sub_msg_class*) {
    for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
        button_icon[i]->hide();
        button_kage[i]->hide();
        button_icon[i]->rotate(0.0f);
        button_kage[i]->rotate(0.0f);
        buttonTimer[i] = -1;
    }
}

/* 8020DEDC-8020E1D0       .text dMsg_ScreenDataValueInitTalk__FP13sub_msg_class */
void dMsg_ScreenDataValueInitTalk(sub_msg_class* i_Msg) {
    int iVar1;
    int iVar2;
    cXyz VStack_3c;
    cXyz local_48;

    iVar1 = (int)((J2DTextBox*)i_Msg->m0544[0].pane)->getCharSpace();
    iVar2 = (int)((J2DTextBox*)i_Msg->m0544[1].pane)->getCharSpace();
    VStack_3c.setall(0.0f);
    if (i_Msg->mPos == VStack_3c) {
        i_Msg->m10E0.x = i_Msg->m049C.mPosCenterOrig.x;
        i_Msg->m10E0.y = i_Msg->m049C.mPosCenterOrig.y;
    } else {
        mDoLib_project(&i_Msg->mPos, &i_Msg->m10E0);
    }
    dMsg_mesgOutPos(i_Msg);
    i_Msg->m10D8 = (int)i_Msg->m049C.mPosCenter.x;
    i_Msg->m10DC = (int)i_Msg->m049C.mPosCenter.y;
#if VERSION == VERSION_DEMO
    i_Msg->m1104 = (int)(((J2DTextBox*)i_Msg->m0544[0].pane)->mLineSpace / 2.0f);
#else
    f32 tmp = 0.5f;
    i_Msg->m1104 = (int)(((J2DTextBox*)i_Msg->m0544[0].pane)->mLineSpace * tmp);
#endif
    i_Msg->m10C0 = i_Msg->m10BC;
    i_Msg->m10C8 = i_Msg->m10C4;
    local_48.setall(0.0f);
    if ((i_Msg->mPos != local_48) && (i_Msg->mMesgEntry.mTextboxType != 10)) {
        i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x;
        i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y;
    } else {
        i_Msg->m10E0.x = i_Msg->m049C.mPosCenter.x;
        i_Msg->m10E0.y = i_Msg->m049C.mPosCenter.y;
    }
    i_Msg->m049C.mSize.x = 100.0f;
    i_Msg->m049C.mSize.y = (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x) * 100.0f;
    fopMsgM_cposMove(&i_Msg->m049C);
    i_Msg->mMsgDataProc.dataInit();
    i_Msg->mMsgDataProc.setStringColor(i_Msg->m1164);
    i_Msg->mMsgDataProc.setActorPosition(&i_Msg->mPos);
    i_Msg->mMsgDataProc.setBmgData(i_Msg->mpMesgStr);
    i_Msg->mMsgDataProc.setOutMessage(i_Msg->output_text, i_Msg->output_rub, i_Msg->output_textSdw, i_Msg->output_rubSdw);
    i_Msg->mMsgDataProc.setSelectMessage(i_Msg->select_text, i_Msg->select_rub, i_Msg->select_textSdw, i_Msg->select_rubSdw);
    i_Msg->mMsgDataProc.setFont(textFont);
    i_Msg->mMsgDataProc.setRubyFont(rubyFont);
    i_Msg->mMsgDataProc.setCharSpace(iVar1);
    i_Msg->mMsgDataProc.setRubyCharSpace(iVar2);
    i_Msg->mMsgDataProc.setLineSpace((int)((J2DTextBox*)i_Msg->m0544[0].pane)->getLineSpace());
    i_Msg->mMsgDataProc.setMesgEntry(&i_Msg->mMesgEntry);
    i_Msg->mMsgDataProc.setFontSize(i_Msg->m110C);
    i_Msg->mMsgDataProc.setRubyFontSize(i_Msg->m1110);
    i_Msg->mMsgDataProc.setLineWidth(0x1f7);
    i_Msg->mMsgDataProc.setCenterLineWidth(0x1e6);
    i_Msg->mMsgDataProc.setSendSpeed(g_msgHIO.field_0x82);
    i_Msg->mMsgDataProc.setSpaceTimer(2);
    if (g_msgHIO.field_0x83 != 0) {
        i_Msg->mMsgDataProc.setSpaceFlagOn();
    } else {
        i_Msg->mMsgDataProc.setSpaceFlagOff();
    }
    i_Msg->m10D0 = (i_Msg->m10D8) - i_Msg->m049C.mPosCenter.x;
    i_Msg->m10D4 = (i_Msg->m10DC) - i_Msg->m049C.mPosCenter.y;
}

/* 8020E1D0-8020E690       .text dMsg_ScreenDataValueInitItem__FP13sub_msg_class */
void dMsg_ScreenDataValueInitItem(sub_msg_class* i_Msg) {
    int uVar1;
    int iVar2;
    int iVar9;
    int fVar3;
    int fVar4;

    fVar3 = (int)((J2DTextBox*)i_Msg->m0544[0].pane)->getCharSpace();
    fVar4 = (int)((J2DTextBox*)i_Msg->m0544[1].pane)->getCharSpace();
    dMsg_msg_pane_parts_set(&i_Msg->m049C, 1);
    if (i_Msg->mMesgEntry.mTextboxPosition != 1) {
        i_Msg->m049C.mPosCenter.y += 225.0f;
        i_Msg->m049C.mPosTopLeft.y += 225.0f;
    }
    dMsg_arw_pane_parts_set(&i_Msg->mPane_Arrow, &i_Msg->m049C);
    dMsg_arw_pane_parts_set(&i_Msg->m050C, &i_Msg->m049C);
    i_Msg->m1114 = (int)(i_Msg->mPane_Arrow.mPosTopLeft.y + i_Msg->mPane_Arrow.mSizeOrig.y);
    uVar1 = (int)i_Msg->m0544[0].mPosTopLeftOrig.x;
    iVar2 = (int)(480.0f - (i_Msg->m0544[0].mPosTopLeftOrig.y + i_Msg->m0544[0].mSizeOrig.y));
    iVar9 = iVar2 + -0x13;
    if (i_Msg->mMesgEntry.mTextboxPosition != 1) {
        i_Msg->m10F8 = uVar1;
        i_Msg->m10FC = iVar9 + 0xE1;
    } else {
        i_Msg->m10F8 = uVar1;
        i_Msg->m10FC = iVar9;
    }
    i_Msg->m0544[0].pane->move(uVar1, (iVar9 - g_msgHIO.field_0x62) - (int)textOffsetY);
    i_Msg->m0544[1].pane->move(uVar1, (iVar9 - g_msgHIO.field_0x60) - (int)textOffsetY);
    i_Msg->m0544[2].pane->move(uVar1 + 2, (iVar9 - g_msgHIO.field_0x62) - (int)textOffsetY + 2);
    i_Msg->m0544[3].pane->move(uVar1 + 2, (iVar9 - g_msgHIO.field_0x60) - (int)textOffsetY + 2);
    i_Msg->m10D8 = (int)i_Msg->m049C.mPosCenter.x;
    i_Msg->m10DC = (int)i_Msg->m049C.mPosCenter.y;
#if VERSION == VERSION_DEMO
    i_Msg->m1104 = (int)(((J2DTextBox*)i_Msg->m0544[0].pane)->mLineSpace / 2.0f);
#else
    f32 tmp = 0.5f;
    i_Msg->m1104 = (int)(((J2DTextBox*)i_Msg->m0544[0].pane)->mLineSpace * tmp);
#endif
    i_Msg->m10C0 = i_Msg->m10BC;
    i_Msg->m10C8 = i_Msg->m10C4;
    i_Msg->m10E0.x = i_Msg->m049C.mPosCenter.x;
    i_Msg->m10E0.y = i_Msg->m049C.mPosCenter.y;
    i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x;
    i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y;
    i_Msg->m049C.mSize.x = 100.0f;
    i_Msg->m049C.mSize.y = (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x) * 100.0f;
    fopMsgM_cposMove(&i_Msg->m049C);
    i_Msg->mMsgDataProc.dataInit();
    i_Msg->mMsgDataProc.setActorPosition(&i_Msg->mPos);
    i_Msg->mMsgDataProc.setBmgData(i_Msg->mpMesgStr);
    i_Msg->mMsgDataProc.setOutMessage(i_Msg->output_text, i_Msg->output_rub, i_Msg->output_textSdw, i_Msg->output_rubSdw);
    i_Msg->mMsgDataProc.setSelectMessage(i_Msg->select_text, i_Msg->select_rub, i_Msg->select_textSdw, i_Msg->select_rubSdw);
    i_Msg->mMsgDataProc.setFont(textFont);
    i_Msg->mMsgDataProc.setRubyFont(rubyFont);
    i_Msg->mMsgDataProc.setCharSpace((int)fVar3);
    i_Msg->mMsgDataProc.setRubyCharSpace((int)fVar4);
    i_Msg->mMsgDataProc.setMesgEntry(&i_Msg->mMesgEntry);
    i_Msg->mMsgDataProc.setFontSize(i_Msg->m110C);
    i_Msg->mMsgDataProc.setRubyFontSize(i_Msg->m1110);
    if ((i_Msg->mMesgEntry.mTextboxType == 9) && (i_Msg->m0624[8].mUserArea == 0)) {
        i_Msg->mMsgDataProc.setLineWidth(0x1a3);
        i_Msg->mMsgDataProc.setCenterLineWidth(0x192);
    } else {
        i_Msg->mMsgDataProc.setLineWidth(0x1f7);
        i_Msg->mMsgDataProc.setCenterLineWidth(0x1e6);
    }
    i_Msg->mMsgDataProc.setSendSpeed(g_msgHIO.field_0x82);
    i_Msg->mMsgDataProc.setSpaceTimer(2);
    if (g_msgHIO.field_0x83 != 0) {
        i_Msg->mMsgDataProc.setSpaceFlagOn();
    } else {
        i_Msg->mMsgDataProc.setSpaceFlagOff();
    }
    i_Msg->m10D0 = (i_Msg->m10D8) - i_Msg->m049C.mPosCenter.x;
    i_Msg->m10D4 = (i_Msg->m10DC) - i_Msg->m049C.mPosCenter.y;
    i_Msg->m0624[9].mUserArea = 0;
    i_Msg->m0624[0].mUserArea = 0;
    i_Msg->m049C.mUserArea = 0;
}

/* 8020E690-8020EAC4       .text dMsg_ScreenDataValueInitTact__FP13sub_msg_class */
void dMsg_ScreenDataValueInitTact(sub_msg_class* i_Msg) {
    int uVar1;
    int iVar2;
#if VERSION == VERSION_DEMO
    int dVar7;
    int dVar8;
#else
    f32 dVar7;
    f32 dVar8;
#endif

    dVar7 = ((J2DTextBox*)i_Msg->m0544[0].pane)->getCharSpace();
    dVar8 = ((J2DTextBox*)i_Msg->m0544[1].pane)->getCharSpace();
    uVar1 = (int)i_Msg->m0544[0].mPosTopLeftOrig.x;
    iVar2 = (int)i_Msg->m0544[0].mPosTopLeftOrig.y;
    i_Msg->m10F8 = uVar1;
    i_Msg->m10FC = iVar2;
    i_Msg->m0544[0].pane->move(uVar1, (iVar2 - g_msgHIO.field_0x62) - (int)textOffsetY);
    i_Msg->m0544[1].pane->move(uVar1, (iVar2 - g_msgHIO.field_0x60) - (int)textOffsetY);
    i_Msg->m0544[2].pane->move(uVar1 + 2, (iVar2 - g_msgHIO.field_0x62) - (int)textOffsetY + 2);
    i_Msg->m0544[3].pane->move(uVar1 + 2, (iVar2 - g_msgHIO.field_0x60) - (int)textOffsetY + 2);
    i_Msg->m10D8 = (int)i_Msg->m049C.mPosCenter.x;
    i_Msg->m10DC = (int)i_Msg->m049C.mPosCenter.y;
    i_Msg->m1104 = 0;
    i_Msg->m10C0 = i_Msg->m10BC;
    i_Msg->m10C8 = i_Msg->m10C4;
    i_Msg->m10E0.x = i_Msg->m049C.mPosCenter.x;
    i_Msg->m10E0.y = i_Msg->m049C.mPosCenter.y;
    i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x;
    i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y;
    i_Msg->m049C.mSize.x = 100.0f;
    i_Msg->m049C.mSize.y = (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x) * 100.0f;
    fopMsgM_cposMove(&i_Msg->m049C);
    i_Msg->mMsgDataProc.dataInit();
    i_Msg->mMsgDataProc.setActorPosition(&i_Msg->mPos);
    i_Msg->mMsgDataProc.setBmgData(i_Msg->mpMesgStr);
    i_Msg->mMsgDataProc.setOutMessage(i_Msg->output_text, i_Msg->output_rub, i_Msg->output_textSdw, i_Msg->output_rubSdw);
    i_Msg->mMsgDataProc.setSelectMessage(i_Msg->select_text, i_Msg->select_rub, i_Msg->select_textSdw, i_Msg->select_rubSdw);
    i_Msg->mMsgDataProc.setFont(textFont);
    i_Msg->mMsgDataProc.setRubyFont(rubyFont);
    i_Msg->mMsgDataProc.setCharSpace((int)dVar7);
    i_Msg->mMsgDataProc.setRubyCharSpace((int)dVar8);
    i_Msg->mMsgDataProc.setMesgEntry(&i_Msg->mMesgEntry);
    i_Msg->mMsgDataProc.setFontSize(i_Msg->m110C);
    i_Msg->mMsgDataProc.setRubyFontSize(i_Msg->m1110);
    if ((i_Msg->mMesgEntry.mTextboxType == 9) && (i_Msg->m0624[8].mUserArea == 0)) {
        i_Msg->mMsgDataProc.setLineWidth(0x1a3);
        i_Msg->mMsgDataProc.setCenterLineWidth(0x192);
    } else {
        i_Msg->mMsgDataProc.setLineWidth(0x1f7);
        i_Msg->mMsgDataProc.setCenterLineWidth(0x1e6);
    }
    i_Msg->mMsgDataProc.setSendSpeed(g_msgHIO.field_0x82);
    i_Msg->mMsgDataProc.setSpaceTimer(2);
    if (g_msgHIO.field_0x83 != 0) {
        i_Msg->mMsgDataProc.setSpaceFlagOn();
    } else {
        i_Msg->mMsgDataProc.setSpaceFlagOff();
    }
    i_Msg->m10D0 = (i_Msg->m10D8) - i_Msg->m049C.mPosCenter.x;
    i_Msg->m10D4 = (i_Msg->m10DC) - i_Msg->m049C.mPosCenter.y;
    i_Msg->m0624[9].mUserArea = 0;
    i_Msg->m0624[0].mUserArea = 0;
    i_Msg->m0624[1].mUserArea = 0;
    i_Msg->m049C.mUserArea = 0;
}

/* 8020EAC4-8020EC28       .text dMsg_ScreenDataValueInitDemo__FP13sub_msg_class */
void dMsg_ScreenDataValueInitDemo(sub_msg_class* i_Msg) {
#if VERSION == VERSION_DEMO
    int charSpace = ((J2DTextBox*)i_Msg->m0544[0].pane)->getCharSpace();
    int rubyCharSpace = ((J2DTextBox*)i_Msg->m0544[1].pane)->getCharSpace();
#else
    f32 charSpace = ((J2DTextBox*)i_Msg->m0544[0].pane)->getCharSpace();
    f32 rubyCharSpace = ((J2DTextBox*)i_Msg->m0544[1].pane)->getCharSpace();
#endif
#if VERSION == VERSION_DEMO
    i_Msg->m1104 = (((J2DTextBox*)i_Msg->m0544[0].pane)->mLineSpace / 2.0f);
#else
    f32 tmp = 0.5f;
    i_Msg->m1104 = (((J2DTextBox*)i_Msg->m0544[0].pane)->mLineSpace * tmp);
#endif
    i_Msg->mMsgDataProc.dataInit();
    i_Msg->mMsgDataProc.setBmgData(i_Msg->mpMesgStr);
    i_Msg->mMsgDataProc.setOutMessage(i_Msg->output_text, i_Msg->output_rub, i_Msg->output_textSdw, i_Msg->output_rubSdw);
    i_Msg->mMsgDataProc.setSelectMessage(i_Msg->select_text, i_Msg->select_rub, i_Msg->select_textSdw, i_Msg->select_rubSdw);
    i_Msg->mMsgDataProc.setFont(textFont);
    i_Msg->mMsgDataProc.setRubyFont(rubyFont);
    i_Msg->mMsgDataProc.setCharSpace(charSpace);
    i_Msg->mMsgDataProc.setRubyCharSpace(rubyCharSpace);
    i_Msg->mMsgDataProc.setMesgEntry(&i_Msg->mMesgEntry);
    i_Msg->mMsgDataProc.setFontSize(i_Msg->m110C);
    i_Msg->mMsgDataProc.setRubyFontSize(i_Msg->m1110);
    i_Msg->mMsgDataProc.setLineWidth(0x1f7);
    i_Msg->mMsgDataProc.setCenterLineWidth(0x1e6);
    i_Msg->mMsgDataProc.setSendSpeed(g_msgHIO.field_0x82);
    i_Msg->mMsgDataProc.setSpaceTimer(2);
    if (g_msgHIO.field_0x83 != 0) {
        i_Msg->mMsgDataProc.setSpaceFlagOn();
    } else {
        i_Msg->mMsgDataProc.setSpaceFlagOff();
    }
}

/* 8020EC28-8020ED14       .text dMsg_value_init__FP13sub_msg_class */
void dMsg_value_init(sub_msg_class* i_Msg) {
    char acStack_30[28];
    char acStack_4c[28];
    char acStack_68[28];

    // clang-format off
    sprintf(acStack_30, "\x1b" "CC[%08x]" "\x1b" "GM[0]",i_Msg->m1164);
    sprintf(acStack_4c, "\x1b" "CC[FFFFFFFF]" "\x1b" "GM[0]");
    sprintf(acStack_68,"\x1b" "CC[000000FF]" "\x1b" "GM[0]");
    // clang-format on
    strcpy(i_Msg->output_text, acStack_30);
    strcpy(i_Msg->output_rub, acStack_4c);
    strcpy(i_Msg->output_textSdw, acStack_68);
    strcpy(i_Msg->output_rubSdw, acStack_68);
    strcpy(i_Msg->select_text, acStack_30);
    strcpy(i_Msg->select_rub, acStack_4c);
    strcpy(i_Msg->select_textSdw, acStack_68);
    strcpy(i_Msg->select_rubSdw, acStack_68);
    i_Msg->mMsgDataProc.setStringColor(i_Msg->m1164);
    dMsg_outFontHide(i_Msg);
    i_Msg->m1108 = 0;
}

/* 8020ED14-8020EDE0       .text dMsg_setString__FP13sub_msg_class */
void dMsg_setString(sub_msg_class* i_Msg) {
    ((J2DTextBox*)i_Msg->m0544[0].pane)->setString(i_Msg->output_text);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->setString(i_Msg->output_rub);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->setString(i_Msg->output_textSdw);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->setString(i_Msg->output_rubSdw);
    if (i_Msg->mMesgEntry.mTextboxType != 9 && i_Msg->mMesgEntry.mTextboxType != 0xe && i_Msg->mMesgEntry.mTextboxType != 1 &&
        i_Msg->mMesgEntry.mTextboxType != 5)
    {
        ((J2DTextBox*)i_Msg->m011C[0].pane)->setString(i_Msg->select_text);
        ((J2DTextBox*)i_Msg->m011C[1].pane)->setString(i_Msg->select_rub);
        ((J2DTextBox*)i_Msg->m011C[2].pane)->setString(i_Msg->select_textSdw);
        ((J2DTextBox*)i_Msg->m011C[3].pane)->setString(i_Msg->select_rubSdw);
    }
}

/* 8020EDE0-8020EE38       .text dMsg_yose_select__FP13sub_msg_class */
void dMsg_yose_select(sub_msg_class* i_Msg) {
    i_Msg->mMsgDataProc.stringLength();
    i_Msg->m1108 = i_Msg->mMsgDataProc.getLineCount();
    i_Msg->mMsgDataProc.setLineCount(0);
    i_Msg->mMsgDataProc.stringShift();
    i_Msg->mMsgDataProc.iconIdxRefresh();
    dMsg_textPosition(i_Msg);
}

/* 8020EE38-8020F080       .text dMsg_frame_openTalk__FP13sub_msg_class */
#if VERSION == VERSION_DEMO
void dMsg_frame_openTalk(sub_msg_class* i_Msg) {
    i_Msg->m1100++;
    if (i_Msg->m1100 >= 0xd) {
        i_Msg->m049C.mPosCenter.x = i_Msg->m10D8;
        i_Msg->m049C.mPosCenter.y = i_Msg->m10DC;
        fopMsgM_paneScaleXY(&i_Msg->m049C, 1.0f);
        fopMsgM_setInitAlpha(&i_Msg->m049C);
    } else {
        if (i_Msg->m1100 < 10) {
            f32 w = 100.0f;
            f32 temp_f31 = w * (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x);
            f32 temp_f30 = i_Msg->m049C.mSizeOrig.x - w;
            f32 temp_f29 = i_Msg->m049C.mSizeOrig.y - temp_f31;
            f32 temp_f1 = fopMsgM_valueIncrease(0xA, i_Msg->m1100, 0U);
            i_Msg->m049C.mSize.x = w + (temp_f30 * temp_f1);
            i_Msg->m049C.mSize.y = temp_f31 + (temp_f29 * temp_f1);
            i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x + (i_Msg->m10D0 * temp_f1);
            i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y + (i_Msg->m10D4 * temp_f1);
            fopMsgM_cposMove(&i_Msg->m049C);
        } else {
            f32 temp_f2_2 = 480.0f / (i_Msg->m049C.mSizeOrig.x - 100.0f);
            f32 a = temp_f2_2 * i_Msg->m10D0;
            f32 b = temp_f2_2 * i_Msg->m10D4;
            f32 temp_f1_2 = a - i_Msg->m10D0;
            f32 temp_f1_3 = b - i_Msg->m10D4;
            f32 w = 580.0f;
            f32 h = w * (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x);
            f32 dx = w - i_Msg->m049C.mSizeOrig.x;
            f32 temp_f1_4 = h - i_Msg->m049C.mSizeOrig.y;
            f32 f0 = (i_Msg->m1100 - 10.0f);
            f32 f0_2 = (0x10000 / 6.0f);
            f32 temp_f3_2 = cM_ssin(f0 * f0_2);
            i_Msg->m049C.mSize.x = i_Msg->m049C.mSizeOrig.x + (dx * temp_f3_2);
            i_Msg->m049C.mSize.y = i_Msg->m049C.mSizeOrig.y + (temp_f1_4 * temp_f3_2);
            i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x + i_Msg->m10D0 + (temp_f1_2 * temp_f3_2);
            i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y + i_Msg->m10D4 + (temp_f1_3 * temp_f3_2);
            fopMsgM_cposMove(&i_Msg->m049C);
        }
        fopMsgM_setNowAlpha(&i_Msg->m049C, fopMsgM_valueIncrease(0xd, i_Msg->m1100, 0));
    }
}
#else
void dMsg_frame_openTalk(sub_msg_class* i_Msg) {
    i_Msg->m1100++;
    if (i_Msg->m1100 >= 0xd) {
        i_Msg->m049C.mPosCenter.x = i_Msg->m10D8;
        i_Msg->m049C.mPosCenter.y = i_Msg->m10DC;
        fopMsgM_paneScaleXY(&i_Msg->m049C, 1.0f);
        fopMsgM_setInitAlpha(&i_Msg->m049C);
    } else {
        if (i_Msg->m1100 < 10) {
            f32 temp_f31 = 100.0f * (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x);
            f32 temp_f30 = i_Msg->m049C.mSizeOrig.x - 100.0f;
            f32 temp_f29 = i_Msg->m049C.mSizeOrig.y - temp_f31;
            f32 temp_f1 = fopMsgM_valueIncrease(0xA, i_Msg->m1100, 0U);
            i_Msg->m049C.mSize.x = 100.0f + (temp_f30 * temp_f1);
            i_Msg->m049C.mSize.y = temp_f31 + (temp_f29 * temp_f1);
            i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x + (i_Msg->m10D0 * temp_f1);
            i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y + (i_Msg->m10D4 * temp_f1);
            fopMsgM_cposMove(&i_Msg->m049C);
        } else {
            f32 temp_f2_2 = 480.0f / (i_Msg->m049C.mSizeOrig.x - 100.0f);
            f32 temp_f1_2 = ((temp_f2_2 * i_Msg->m10D0) - i_Msg->m10D0);
            f32 temp_f1_3 = ((temp_f2_2 * i_Msg->m10D4) - i_Msg->m10D4);
            f32 temp_f1_4 = ((580.0f * (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x)) - i_Msg->m049C.mSizeOrig.y);
            f32 f0_2 = (0x10000 / 6.0f);
            f32 f0 = (i_Msg->m1100 - 10.0f);
            f32 temp_f3_2 = cM_ssin(f0 * f0_2);
            i_Msg->m049C.mSize.x = i_Msg->m049C.mSizeOrig.x + ((580.0f - i_Msg->m049C.mSizeOrig.x) * temp_f3_2);
            i_Msg->m049C.mSize.y = i_Msg->m049C.mSizeOrig.y + (temp_f1_4 * temp_f3_2);
            i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x + i_Msg->m10D0 + (temp_f1_2 * temp_f3_2);
            i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y + i_Msg->m10D4 + (temp_f1_3 * temp_f3_2);
            fopMsgM_cposMove(&i_Msg->m049C);
        }
        fopMsgM_setNowAlpha(&i_Msg->m049C, fopMsgM_valueIncrease(0xd, i_Msg->m1100, 0));
    }
}
#endif

/* 8020F080-8020F1C0       .text dMsg_frame_openItem__FP13sub_msg_class */
#if VERSION == VERSION_DEMO
void dMsg_frame_openItem(sub_msg_class* i_Msg) {
    f32 dVar2;
    s32 max = 8;

    i_Msg->m1100++;
    if (i_Msg->m1100 >= max) {
        i_Msg->m049C.mPosCenter.x = i_Msg->m10D8;
        i_Msg->m049C.mPosCenter.y = i_Msg->m10DC;
        fopMsgM_paneScaleXY(&i_Msg->m049C, 1.0f);
        fopMsgM_setInitAlpha(&i_Msg->m049C);
    } else {
        if (i_Msg->m1100 <= 5) {
            dVar2 = fopMsgM_valueIncrease(5, i_Msg->m1100, 0);
            f32 scale = (dVar2 * 0.5f) + 0.6f;
            i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x + (i_Msg->m10D0 * dVar2);
            i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y + (i_Msg->m10D4 * dVar2);
            fopMsgM_paneScaleXY(&i_Msg->m049C, scale);
            fopMsgM_setNowAlpha(&i_Msg->m049C, dVar2);
        } else {
            dVar2 = fopMsgM_valueIncrease(3, i_Msg->m1100 + -5, 0);
            f32 scale = (dVar2 * -0.100000024f) + 1.1f;
            fopMsgM_paneScaleXY(&i_Msg->m049C, scale);
            fopMsgM_setInitAlpha(&i_Msg->m049C);
        }
    }
}
#else
void dMsg_frame_openItem(sub_msg_class* i_Msg) {
    f32 dVar2;

    i_Msg->m1100++;
    if (i_Msg->m1100 >= 8) {
        i_Msg->m049C.mPosCenter.x = i_Msg->m10D8;
        i_Msg->m049C.mPosCenter.y = i_Msg->m10DC;
        fopMsgM_paneScaleXY(&i_Msg->m049C, 1.0f);
        fopMsgM_setInitAlpha(&i_Msg->m049C);
    } else {
        if (i_Msg->m1100 <= 5) {
            dVar2 = fopMsgM_valueIncrease(5, i_Msg->m1100, 0);
            i_Msg->m049C.mPosCenter.x = i_Msg->m10E0.x + (i_Msg->m10D0 * dVar2);
            i_Msg->m049C.mPosCenter.y = i_Msg->m10E0.y + (i_Msg->m10D4 * dVar2);
            fopMsgM_paneScaleXY(&i_Msg->m049C, (dVar2 * 0.5f) + 0.6f);
            fopMsgM_setNowAlpha(&i_Msg->m049C, dVar2);
        } else {
            dVar2 = fopMsgM_valueIncrease(3, i_Msg->m1100 + -5, 0);
            fopMsgM_paneScaleXY(&i_Msg->m049C, (dVar2 * -0.100000024f) + 1.1f);
            fopMsgM_setInitAlpha(&i_Msg->m049C);
        }
    }
}
#endif

/* 8020F1C0-8020F324       .text dMsg_frame_close__FP13sub_msg_class */
void dMsg_frame_close(sub_msg_class* i_Msg) {
    f32 dVar3;

    dMsg_messagePaneHide(i_Msg);
    dMsg_outFontHide(i_Msg);
    i_Msg->m1100++;
    if (i_Msg->m1100 == 10) {
        fopMsgM_setNowAlphaZero(&i_Msg->m049C);
        JKRRemoveResource(i_Msg->head_p, NULL);
        i_Msg->mStatus = fopMsgStts_BOX_CLOSED_e;
        i_Msg->m049C.pane->hide();
    } else {
        f32 f1 = fopMsgM_valueIncrease(10, i_Msg->m1100, 0);
#if VERSION == VERSION_DEMO
        f32 w = 620.0f;
        f32 h = (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x) * w;
        f32 tmp2 = w - i_Msg->m049C.mSizeOrig.x;
        f32 f29 = h - i_Msg->m049C.mSizeOrig.y;
#else
        f32 tmp2 = 620.0f - i_Msg->m049C.mSizeOrig.x;
        f32 f29 = (i_Msg->m049C.mSizeOrig.y / i_Msg->m049C.mSizeOrig.x) * 620.0f - i_Msg->m049C.mSizeOrig.y;
#endif
        i_Msg->m049C.mSize.x = i_Msg->m049C.mSizeOrig.x + f1 * tmp2;
        i_Msg->m049C.mSize.y = i_Msg->m049C.mSizeOrig.y + f1 * f29;
        i_Msg->m049C.mPosCenter.x = i_Msg->m10D8;
        i_Msg->m049C.mPosCenter.y = i_Msg->m10DC;
        fopMsgM_cposMove(&i_Msg->m049C);
        dVar3 = fopMsgM_valueIncrease(10, 10 - i_Msg->m1100, 0);
        if (dVar3 < (i_Msg->m049C.mNowAlpha / 255.0f)) {
            fopMsgM_setNowAlpha(&i_Msg->m049C, dVar3);
        }
        if (i_Msg->m1100 == 1) {
            dMsg_setCloseSound(i_Msg, 0);
        }
    }
}

/* 8020F324-8020F3F8       .text dMsg_textPosition__FP13sub_msg_class */
void dMsg_textPosition(sub_msg_class* i_Msg) {
    int pos[2];

    switch (i_Msg->mMesgEntry.mTextboxType) {
    case 5:
#if VERSION <= VERSION_JPN
        if (g_msgDHIO.field_0x08 == 0) {
            pos[1] = i_Msg->m1104 * (1 - i_Msg->m1108);
        } else
#endif
        {
            pos[1] = i_Msg->m1104 * (2 - i_Msg->m1108);
        }
        break;
    case 0xE:
#if VERSION == VERSION_PAL
        if (i_Msg->mMsgNo != 0x5b3 && i_Msg->mMsgNo != 0x5b4) {
            pos[1] = -(int)(((J2DTextBox*)i_Msg->m0544[0].pane)->getLineSpace() / 2.0f) * i_Msg->m1108;
            break;
        }
#endif
        pos[1] = 0;
        break;
    default:
#if VERSION <= VERSION_JPN
        if (g_msgDHIO.field_0x08 == 0) {
            pos[1] = i_Msg->m1104 * (2 - i_Msg->m1108);
        } else
#endif
        {
            pos[1] = i_Msg->m1104 * (3 - i_Msg->m1108);
        }
        break;
    }
    pos[0] = 0;
    ((J2DTextBox*)i_Msg->m0544[0].pane)->shiftSet(pos[0], pos[1]);
    ((J2DTextBox*)i_Msg->m0544[1].pane)->shiftSet(pos[0], pos[1]);
    ((J2DTextBox*)i_Msg->m0544[2].pane)->shiftSet(pos[0], pos[1]);
    ((J2DTextBox*)i_Msg->m0544[3].pane)->shiftSet(pos[0], pos[1]);
}

/* 8020F3F8-8020F4E0       .text dMsg_rubySet__FP13sub_msg_class */
void dMsg_rubySet(sub_msg_class* i_Msg) {
    J2DScreen* pJVar2;

    if (dComIfGs_getOptRuby()) {
        i_Msg->m0544[1].pane->hide();
        i_Msg->m0544[3].pane->hide();
        if (i_Msg->mMesgEntry.mTextboxType == 9) {
            return;
        }
        if (i_Msg->mMesgEntry.mTextboxType == 0xe) {
            return;
        }
        if (i_Msg->mMesgEntry.mTextboxType == 1) {
            return;
        }
        if (i_Msg->mMesgEntry.mTextboxType == 5) {
            return;
        }
        i_Msg->m011C[1].pane->hide();
        i_Msg->m011C[3].pane->hide();
    } else {
        pJVar2 = (J2DScreen*)i_Msg->m0544[1].pane;
        if (pJVar2->isVisible()) {
            pJVar2->show();
        }
        pJVar2 = (J2DScreen*)i_Msg->m0544[3].pane;
        if (pJVar2->isVisible()) {
            pJVar2->show();
        }
        if (i_Msg->mMesgEntry.mTextboxType == 9) {
            return;
        }
        if (i_Msg->mMesgEntry.mTextboxType == 0xe) {
            return;
        }
        if (i_Msg->mMesgEntry.mTextboxType == 1) {
            return;
        }
        if (i_Msg->mMesgEntry.mTextboxType == 5) {
            return;
        }
        pJVar2 = (J2DScreen*)i_Msg->m011C[1].pane;
        if (pJVar2->isVisible()) {
            pJVar2->show();
        }
        pJVar2 = (J2DScreen*)i_Msg->m011C[3].pane;
        if (!pJVar2->isVisible()) {
            return;
        }
        pJVar2->show();
    }
}

/* 8020F4E0-8020F7EC       .text dMsg_mesgOutPos__FP13sub_msg_class */
void dMsg_mesgOutPos(sub_msg_class* i_Msg) {
    s32 uVar1;
    u8 bVar2;
    u8 bVar4;
    int iVar3;
    cXyz local_68;
    f32 local_58;

    bVar4 = i_Msg->mMesgEntry.mTextboxPosition;
    daPy_py_c* player = daPy_getPlayerActorClass();
    if (bVar4 == 0) {
        mDoLib_project(&player->eyePos, &local_68);
        talkPosX = (local_68.x + i_Msg->m10E0.x) * 0.5f;
        talkPosY = (local_68.y + i_Msg->m10E0.y) * 0.5f;
        local_58 = (int)g_msgHIO.field_0x6e;
        if (talkPosY < local_58) {
            bVar4 = 3;
        } else {
            bVar4 = 1;
        }
    }
    dMsg_msg_pane_parts_set(&i_Msg->m049C, bVar4);
    bVar2 = i_Msg->mMesgEntry.mTextboxType;
    if ((bVar2 != 5) && (bVar2 != 0xe)) {
        dMsg_arw_pane_parts_set(&i_Msg->mPane_Arrow, &i_Msg->m049C);
        dMsg_arw_pane_parts_set(&i_Msg->m050C, &i_Msg->m049C);
        i_Msg->m1114 = (int)((i_Msg->mPane_Arrow).mPosTopLeft.y + (i_Msg->mPane_Arrow).mSizeOrig.y);
    }
    uVar1 = i_Msg->m0544[0].mPosTopLeftOrig.x;
    if (bVar4 == 1) {
        iVar3 = (int)(480.0f - (i_Msg->m0544[0].mPosTopLeftOrig.y + i_Msg->m0544[0].mSizeOrig.y)) + -0x13;
    } else if (bVar4 == 2) {
#if VERSION == VERSION_DEMO
        iVar3 = (int)(240.0f - i_Msg->m0544[0].mSizeOrig.y / 2.0f);
#else
        f32 tmp = 0.5f;
        iVar3 = (int)(240.0f - i_Msg->m0544[0].mSizeOrig.y * tmp);
#endif
    } else {
        iVar3 = (int)i_Msg->m0544[0].mPosTopLeftOrig.y + 0x20;
    }
    i_Msg->m10F8 = uVar1;
    i_Msg->m10FC = iVar3;
    i_Msg->m0544[0].pane->move((uVar1), ((iVar3 - g_msgHIO.field_0x62) - (int)textOffsetY));
    i_Msg->m0544[1].pane->move((uVar1), ((iVar3 - g_msgHIO.field_0x60) - (int)textOffsetY));
    i_Msg->m0544[2].pane->move((uVar1 + 2), (((iVar3 - g_msgHIO.field_0x62) - (int)textOffsetY) + 2));
    i_Msg->m0544[3].pane->move((uVar1 + 2), (((iVar3 - g_msgHIO.field_0x60) - (int)textOffsetY) + 2));
}

/* 8020F7EC-8020F844       .text dMsg_arrowInit__FP13sub_msg_class */
void dMsg_arrowInit(sub_msg_class* i_Msg) {
    fopMsgM_setNowAlphaZero(&i_Msg->mPane_Arrow);
    i_Msg->mPane_Arrow.mPosCenter.x = i_Msg->mPane_Arrow.mPosCenterOrig.x;
    i_Msg->mPane_Arrow.mPosCenter.y = i_Msg->mPane_Arrow.mPosCenterOrig.y;
    i_Msg->mPane_Arrow.mSize.x = i_Msg->mPane_Arrow.mSizeOrig.x;
    i_Msg->mPane_Arrow.mSize.y = i_Msg->mPane_Arrow.mSizeOrig.y;
    i_Msg->m1100 = 0;
}

static void unused_5874() {
    static const Vec frameScale = {0.6f, 1.1f, 1.0f};
};

/* 8020F844-8020FC10       .text dMsg_arrowMove__FP13sub_msg_class */
void dMsg_arrowMove(sub_msg_class* i_Msg) {
    static const s32 time[] = {0x00, 0x3C, 0x07, 0x04, 0x03, 0x02};
    static const f32 scaleX[] = {1.0f, 1.3f, 0.8f, 1.2f, 1.0f};
    static const f32 scaleY[] = {1.0f, 0.3f, 1.1f, 0.8f, 1.0f};
    f32 fVar1;
    f32 fVar2;
    f32 dVar4;
    f32 fRatio;
    f32 fDelta;
    s32 temp_r5;
    s32 temp_r6;
    s32 temp_r7;
    s32 temp_r8;
    s32 temp_r9;
    s32 temp_r10;
    s32 temp_r11;
    s32 temp_r12;
    s32 temp_r30;

#if VERSION == VERSION_DEMO
    temp_r9 = time[1];
    temp_r10 = temp_r9 + time[2];
    temp_r11 = temp_r10 + time[3];
    temp_r12 = temp_r11 + time[4];
    temp_r30 = temp_r12 + time[5];
#else
    temp_r9 = time[1];
    temp_r8 = time[2];
    temp_r10 = temp_r9 + temp_r8;
    temp_r7 = time[3];
    temp_r11 = temp_r10 + temp_r7;
    temp_r6 = time[4];
    temp_r12 = temp_r11 + temp_r6;
    temp_r5 = time[5];
    temp_r30 = temp_r12 + temp_r5;
#endif
    i_Msg->m1100++;
    if ((i_Msg->mPane_Arrow).mNowAlpha < (i_Msg->mPane_Arrow).mInitAlpha) {
        dVar4 = fopMsgM_valueIncrease(10, i_Msg->m1100, 0);
        fopMsgM_setNowAlpha(&i_Msg->mPane_Arrow, (f32)dVar4);
        if (i_Msg->m1100 == 10) {
            i_Msg->m1100 = 0;
        }
    } else {
        if (i_Msg->m1100 <= temp_r9) {
            fVar1 = 1.0f;
            fVar2 = 1.0f;
        } else if (i_Msg->m1100 <= temp_r10) {
            fDelta = (f32)(i_Msg->m1100) - temp_r9;
            fRatio = (fDelta * fDelta) / ((f32)time[2] * (f32)time[2]);
            fVar1 = scaleX[0] + (fRatio * (scaleX[1] - scaleX[0]));
            fVar2 = scaleY[0] + (fRatio * (scaleY[1] - scaleY[0]));
        } else if (i_Msg->m1100 <= temp_r11) {
            fDelta = (f32)(i_Msg->m1100) - temp_r10;
            fRatio = (fDelta * fDelta) / ((f32)time[3] * (f32)time[3]);
            fVar1 = scaleX[1] + (fRatio * (scaleX[2] - scaleX[1]));
            fVar2 = scaleY[1] + (fRatio * (scaleY[2] - scaleY[1]));
        } else if (i_Msg->m1100 <= temp_r12) {
            fDelta = (f32)(i_Msg->m1100) - temp_r11;
            fRatio = (fDelta * fDelta) / ((f32)time[4] * (f32)time[4]);
            fVar1 = scaleX[2] + (fRatio * (scaleX[3] - scaleX[2]));
            fVar2 = scaleY[2] + (fRatio * (scaleY[3] - scaleY[2]));
        } else if (i_Msg->m1100 <= temp_r30) {
            fDelta = (f32)(i_Msg->m1100) - temp_r12;
            fRatio = (fDelta * fDelta) / ((f32)time[5] * (f32)time[5]);
            fVar1 = scaleX[3] + (fRatio * (scaleX[4] - scaleX[3]));
            fVar2 = scaleY[3] + (fRatio * (scaleY[4] - scaleY[3]));
        } else {
            fVar1 = 1.0f;
            fVar2 = fVar1;
            i_Msg->m1100 = 0;
        }
        (i_Msg->mPane_Arrow).mSize.x = (i_Msg->mPane_Arrow).mSizeOrig.x * fVar1;
        (i_Msg->mPane_Arrow).mSize.y = (i_Msg->mPane_Arrow).mSizeOrig.y * fVar2;
        (i_Msg->mPane_Arrow).mPosCenter.y = (f32)((int)g_msgHIO.field_0x66) + ((f32)(i_Msg->m1114) - (i_Msg->mPane_Arrow).mSize.y / 2.0f);
        fopMsgM_cposMove(&i_Msg->mPane_Arrow);
    }
}

/* 8020FC10-8021010C       .text dMsg_dotMove__FP13sub_msg_class */
void dMsg_dotMove(sub_msg_class* i_Msg) {
    int iVar4;
    f32 dVar5;

    i_Msg->m1100++;
    if ((i_Msg->m050C).mNowAlpha < (i_Msg->m050C).mInitAlpha) {
#if VERSION == VERSION_DEMO
        s32 max = 10;
        dVar5 = fopMsgM_valueIncrease(max, i_Msg->m1100, 0);
        fopMsgM_setNowAlpha(&i_Msg->m050C, (f32)dVar5);
        if (max == i_Msg->m1100) {
#else
        dVar5 = fopMsgM_valueIncrease(10, i_Msg->m1100, 0);
        fopMsgM_setNowAlpha(&i_Msg->m050C, (f32)dVar5);
        if (i_Msg->m1100 == 10) {
#endif
            i_Msg->m1100 = 0;
        }
    } else {
        iVar4 = i_Msg->m1100;
        if (iVar4 >= 60) {
            i_Msg->m10C0 = i_Msg->m10BC;
            i_Msg->m10C8 = i_Msg->m10C4;
            fopMsgM_setInitAlpha(&i_Msg->m050C);
            i_Msg->m1100 = 0;
        } else if (iVar4 > 30) {
            iVar4 = 60 - iVar4;
            dVar5 = fopMsgM_valueIncrease(30, iVar4, 0);
            i_Msg->m10C0.r = (u8)(int)((f32)(i_Msg->m10BC.r) + (f32)((150.0f - (f32)(i_Msg->m10BC.r)) * dVar5));
            i_Msg->m10C0.g = (u8)(int)((f32)(i_Msg->m10BC.g) + (f32)((150.0f - (f32)(i_Msg->m10BC.g)) * dVar5));
            i_Msg->m10C0.b = (u8)(int)((f32)(i_Msg->m10BC.b) + (f32)((150.0f - (f32)(i_Msg->m10BC.b)) * dVar5));
            i_Msg->m10C8.r = (u8)(int)((f32)(i_Msg->m10C4.r) + (f32)((255.0f - (f32)(i_Msg->m10C4.r)) * dVar5));
            i_Msg->m10C8.g = (u8)(int)((f32)(i_Msg->m10C4.g) + (f32)((255.0f - (f32)(i_Msg->m10C4.g)) * dVar5));
            i_Msg->m10C8.b = (u8)(int)((f32)(i_Msg->m10C4.b) + (f32)((220.0f - (f32)(i_Msg->m10C4.b)) * dVar5));
            i_Msg->m050C.mNowAlpha = (u8)(int)((f32)(i_Msg->m050C.mInitAlpha) + (f32)((255.0f - (f32)(i_Msg->m050C.mInitAlpha)) * dVar5));
        } else {
            dVar5 = fopMsgM_valueIncrease(30, iVar4, 0);
            i_Msg->m10C0.r = (u8)(int)((f32)(i_Msg->m10BC.r) + (f32)((150.0f - (f32)(i_Msg->m10BC.r)) * dVar5));
            i_Msg->m10C0.g = (u8)(int)((f32)(i_Msg->m10BC.g) + (f32)((150.0f - (f32)(i_Msg->m10BC.g)) * dVar5));
            i_Msg->m10C0.b = (u8)(int)((f32)(i_Msg->m10BC.b) + (f32)((150.0f - (f32)(i_Msg->m10BC.b)) * dVar5));
            i_Msg->m10C8.r = (u8)(int)((f32)(i_Msg->m10C4.r) + (f32)((255.0f - (f32)(i_Msg->m10C4.r)) * dVar5));
            i_Msg->m10C8.g = (u8)(int)((f32)(i_Msg->m10C4.g) + (f32)((255.0f - (f32)(i_Msg->m10C4.g)) * dVar5));
            i_Msg->m10C8.b = (u8)(int)((f32)(i_Msg->m10C4.b) + (f32)((220.0f - (f32)(i_Msg->m10C4.b)) * dVar5));
            i_Msg->m050C.mNowAlpha = (u8)(int)((f32)(i_Msg->m050C.mInitAlpha) + (f32)((255.0f - (f32)(i_Msg->m050C.mInitAlpha)) * dVar5));
        }
        ((J2DPicture*)i_Msg->m050C.pane)->setBlack(i_Msg->m10C0);
        ((J2DPicture*)i_Msg->m050C.pane)->setWhite(i_Msg->m10C8);
    }
}

/* 8021010C-80210194       .text dMsg_ringMove__FP13sub_msg_class */
void dMsg_ringMove(sub_msg_class* i_Msg) {
    int iVar1;
    f32 dVar2;

    i_Msg->m0624[9].mUserArea++;
    if (i_Msg->m0624[9].mUserArea >= 120) {
        i_Msg->m0624[9].mUserArea = 0;
    }
    iVar1 = (int)i_Msg->m0624[9].mUserArea;
    if (iVar1 < 60) {
        dVar2 = fopMsgM_valueIncrease(60, iVar1, 0);
    } else {
        dVar2 = fopMsgM_valueIncrease(60, 120 - iVar1, 0);
    }
    fopMsgM_setNowAlpha(i_Msg->m0624 + 9, (dVar2 * 0.25f) + 0.25f);
}

/* 80210194-802103D4       .text dMsg_lightMove__FP13sub_msg_class */
void dMsg_lightMove(sub_msg_class* i_Msg) {
    i_Msg->m0624[0].mUserArea++;
    if (i_Msg->m0624[0].mUserArea >= 800) {
        i_Msg->m0624[0].mUserArea = 0;
    }
    f32 f31 = (i_Msg->m0624[0].mUserArea / 800.0f) * 360.0f;
    i_Msg->m0624[0].pane->rotate(i_Msg->m0624[0].mSize.x, 0.0f, ROTATE_Z, -f31);
    i_Msg->m0624[1].pane->rotate(0.0f, 0.0f, ROTATE_Z, -f31);
    i_Msg->m0624[2].pane->rotate(i_Msg->m0624[2].mSize.x, i_Msg->m0624[2].mSize.y, ROTATE_Z, -f31);
    i_Msg->m0624[3].pane->rotate(0.0f, i_Msg->m0624[3].mSize.y, ROTATE_Z, -f31);
    i_Msg->m0624[4].pane->rotate(i_Msg->m0624[4].mSize.x, 0.0f, ROTATE_Z, f31);
    i_Msg->m0624[5].pane->rotate(0.0f, 0.0f, ROTATE_Z, f31);
    i_Msg->m0624[6].pane->rotate(i_Msg->m0624[6].mSize.x, i_Msg->m0624[6].mSize.y, ROTATE_Z, f31);
    i_Msg->m0624[7].pane->rotate(0.0f, i_Msg->m0624[7].mSize.y, ROTATE_Z, f31);
    for (s32 i = 0; i < 4; i++) {
        fopMsgM_setInitAlpha(&i_Msg->m0624[i]);
    }
}

/* 802103D4-802109A0       .text dMsg_cornerMove__FP13sub_msg_class */
void dMsg_cornerMove(sub_msg_class* param_1) {
    static const u8 cc_r[] = {200, 0, 0, 0};
    static const u8 cc_g[] = {128, 0, 0, 0};
    static const u8 cc_b[] = {255, 0, 255, 0};
    static const u8 cc2_r[] = {50, 50, 100, 50};
    static const u8 cc2_g[] = {50, 50, 100, 50};
    static const u8 cc2_b[] = {180, 180, 120, 180};
    static const u8 cc2_a[] = {255, 150, 255, 255};

#if VERSION == VERSION_DEMO
    s32 max = 240;
    param_1->m049C.mUserArea++;
    if (param_1->m049C.mUserArea >= max) {
        param_1->m049C.mUserArea = 0;
    }
    s32 now = param_1->m049C.mUserArea % 60;
    f32 tmp = fopMsgM_valueIncrease(60, now, 2);
#else
    param_1->m049C.mUserArea++;
    if (param_1->m049C.mUserArea >= 240) {
        param_1->m049C.mUserArea = 0;
    }
    f32 tmp = fopMsgM_valueIncrease(60, param_1->m049C.mUserArea % 60, 2);
#endif
    GXColor local_68[4];
    if (param_1->mMesgEntry.mTextboxType == 1) {
        for (int i = 0; i < 4; i++) {
            int r9 = i + 1;
            if (r9 >= 4) {
                r9 = 0;
            }
            local_68[i].r = cc2_r[i] + tmp * (cc2_r[r9] - cc2_r[i]);
            local_68[i].g = cc2_g[i] + tmp * (cc2_g[r9] - cc2_g[i]);
            local_68[i].b = cc2_b[i] + tmp * (cc2_b[r9] - cc2_b[i]);
            local_68[i].a = cc2_a[i] + tmp * (cc2_a[r9] - cc2_a[i]);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            int r9 = i + 1;
            if (r9 >= 4) {
                r9 = 0;
            }
            local_68[i].r = cc_r[i] + tmp * (cc_r[r9] - cc_r[i]);
            local_68[i].g = cc_g[i] + tmp * (cc_g[r9] - cc_g[i]);
            local_68[i].b = cc_b[i] + tmp * (cc_b[r9] - cc_b[i]);
            local_68[i].a = 255;
        }
    }
    if (param_1->m049C.mUserArea < 60) {
        ((J2DPicture*)param_1->m049C.pane)
            ->setCornerColor(
                *(JUtility::TColor*)&local_68[0], *(JUtility::TColor*)&local_68[1], *(JUtility::TColor*)&local_68[3], *(JUtility::TColor*)&local_68[2]
            );
    } else if (param_1->m049C.mUserArea < 120) {
        ((J2DPicture*)param_1->m049C.pane)
            ->setCornerColor(
                *(JUtility::TColor*)&local_68[1], *(JUtility::TColor*)&local_68[2], *(JUtility::TColor*)&local_68[0], *(JUtility::TColor*)&local_68[3]
            );
    } else if (param_1->m049C.mUserArea < 180) {
        ((J2DPicture*)param_1->m049C.pane)
            ->setCornerColor(
                *(JUtility::TColor*)&local_68[2], *(JUtility::TColor*)&local_68[3], *(JUtility::TColor*)&local_68[1], *(JUtility::TColor*)&local_68[0]
            );
    } else {
        ((J2DPicture*)param_1->m049C.pane)
            ->setCornerColor(
                *(JUtility::TColor*)&local_68[3], *(JUtility::TColor*)&local_68[0], *(JUtility::TColor*)&local_68[2], *(JUtility::TColor*)&local_68[1]
            );
    }
}

/* 802109A0-80210AA0       .text dMsg_tactGuideShow__FP13sub_msg_classUc */
void dMsg_tactGuideShow(sub_msg_class* i_Msg, u8 param_2) {
    for (s32 i = 0; i < mBeatNum[dComIfGp_getMelodyNum()]; i++) {
        if (i < param_2) {
            if (dComIfGp_getMelodyNum() < 5) {
                fopMsgM_setInitAlpha(&i_Msg->m08C4[i]);
                fopMsgM_setNowAlpha(&i_Msg->m0A14[i], 0.5f);
                fopMsgM_setNowAlphaZero(&i_Msg->m0B64[i]);
            } else {
                fopMsgM_setInitAlpha(&i_Msg->m0B64[i]);
                fopMsgM_setNowAlpha(&i_Msg->m0A14[i], 0.5f);
                fopMsgM_setNowAlpha(&i_Msg->m08C4[i], 0.5f);
                fopMsgM_setNowAlpha(&i_Msg->m0CB4[i], 0.5f);
            }
        } else {
            i_Msg->m08C4[i].pane->hide();
            i_Msg->m0A14[i].pane->hide();
            i_Msg->m0B64[i].pane->hide();
            if (dComIfGp_getMelodyNum() >= 5) {
                i_Msg->m0CB4[i].pane->hide();
            }
        }
    }
}

/* 80210AA0-80210CA4       .text dMsg_numberInput__FP13sub_msg_class */
void dMsg_numberInput(sub_msg_class* i_Msg) {
#if VERSION <= VERSION_JPN
    static const char* num_str[] = {"０", "１", "２", "３", "４", "５", "６", "７", "８", "９"};
#else
    static const char* num_str[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
#endif

    for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
        if (i_Msg->mMsgDataProc.getIconNum(i) == fopMsgM_Icon_INPUT_e) {
            i_Msg->m10EC = ((i_Msg->m0544[0].pane)->getBounds().i.x + i_Msg->mMsgDataProc.getIconPosX(i));
            i_Msg->m10F0 = ((i_Msg->m0544[0].pane)->getBounds().i.y + (i_Msg->m1104 * (s32)((VERSION_SELECT(2, 2, 3, 3) - i_Msg->m1108) + (i_Msg->mMsgDataProc.getIconPosY(i)) * 2)));
        }
    }
    if (i_Msg->mMsgNo == 0x1cfa) {
        i_Msg->mSelectNum = i_Msg->mMsgDataProc.inputNumber(3);
    } else {
        i_Msg->mSelectNum = i_Msg->mMsgDataProc.inputNumber(2);
    }
    i_Msg->mMsgDataProc.colorAnime(maskPane);
    numberPane[2]->setString(num_str[dComIfGp_getMessageSetNumber() / 100]);
    numberPane[1]->setString(num_str[(dComIfGp_getMessageSetNumber() % 100) / 10]);
    numberPane[0]->setString(num_str[dComIfGp_getMessageSetNumber() % 10]);
    daNpc_Bs1_c::setBuyItem(dComIfGp_getMessageSetNumber());
    if (dMsg_CHECK_TRIG_AB() || fopMsgM_checkMessageSend()) {
        dMsg_setCancelMode(i_Msg);
    } else {
        dMeter_setDoStatus(3);
    }
}

static void dummy() {
    DEAD_STRING("hukidashi_021.bti");
    DEAD_STRING("texture != 0");
};

/* 80210CA4-80210F28       .text dMsg_tactInput__FP13sub_msg_class */
void dMsg_tactInput(sub_msg_class* i_Msg) {
    int melody_no = dComIfGp_getMelodyNum();
    if (i_Msg->m1100 < mBeatNum[melody_no]) {
        if ((mBeatNum[melody_no] != mDoAud_tact_getBeat()) || (i_Msg->m1168 != mDoAud_tact_getBeat())) {
            i_Msg->m1100 = 0;
            i_Msg->m0624[0].mUserArea = 0;
            i_Msg->m1168 = mDoAud_tact_getBeat();
            dMsg_tactGuideShow(i_Msg, mBeatNum[melody_no]);
        }
        if (daPy_getPlayerActorClass()->checkTactInput()) {
            s16 sVar1 = daPy_getPlayerLinkActorClass()->getTactLeftHandPos();
            s32 local_38;
            mDoAud_tact_getMelodyPattern(melody_no, i_Msg->m1100, &local_38);
            i_Msg->m1100++;
            if ((sVar1 == local_38) && (mBeatNum[melody_no] == mDoAud_tact_getBeat())) {
                int iVar3 = (int)i_Msg->m0624[0].mUserArea;
                if (iVar3 == i_Msg->m1100 + -1) {
                    i_Msg->m0B64[iVar3].mUserArea = 1;
                    i_Msg->m0624[0].mUserArea++;
                }
            }
            if ((i_Msg->m1100 >= (mDoAud_tact_getBeat())) && (!daPy_getPlayerLinkActorClass()->checkTactLastInput())) {
                i_Msg->m1100 = 0;
                i_Msg->m0624[0].mUserArea = 0;
                dMsg_tactGuideShow(i_Msg, mBeatNum[melody_no]);
            }
        } else {
            if (((((melody_no != 1) && (melody_no != 5)) && (melody_no != 6)) && (melody_no != 7)) && (CPad_CHECK_TRIG_B(0) || (fopMsgM_checkMessageSend()))) {
                dMsg_setCancelMode(i_Msg);
            }
        }
    }
    
    for (int i = 0; i < mBeatNum[melody_no]; i++) {
        s16 sVar1 = i_Msg->m0B64[i].mUserArea;
        if ((sVar1 > 0) && (sVar1 <= 5)) {
            f32 alpha = (fopMsgM_valueIncrease(5, (int)sVar1, 0) * 0.5f) + 0.5f;
            if (dComIfGp_getMelodyNum() < 5) {
                fopMsgM_setNowAlpha(&i_Msg->m0A14[i], alpha);
                fopMsgM_setNowAlpha(&i_Msg->m0B64[i], alpha);
            } else {
                fopMsgM_setNowAlpha(&i_Msg->m0A14[i], alpha);
                fopMsgM_setNowAlpha(&i_Msg->m08C4[i], alpha);
                fopMsgM_setNowAlpha(&i_Msg->m0CB4[i], alpha);
            }
            if (i_Msg->m0B64[mBeatNum[melody_no] - 1].mUserArea == 5) {
                i_Msg->mStatus = fopMsgStts_MSG_DISPLAYED_e;
                i_Msg->m1100 = 0;
            }
            i_Msg->m0B64[i].mUserArea++;
        }
    }
}

/* 80210F28-80210F50       .text dMsg_tactInput2__FP13sub_msg_class */
void dMsg_tactInput2(sub_msg_class*) {
    if (!CPad_CHECK_TRIG_B(0)) {
        return;
    }
    dComIfGp_setMesgCancelButton(1);
}

/* 80210F50-80210FF8       .text dMsg_subTextScale__FP13sub_msg_classf */
void dMsg_subTextScale(sub_msg_class* i_Msg, f32 param_2) {
    for (s32 i = 1; i < 10; i++) {
        i_Msg->m026C[i].pane->move(i_Msg->m026C[i].mPosTopLeft.x * param_2, i_Msg->m026C[i].mPosTopLeft.y * param_2);
        i_Msg->m026C[i].pane->resize(i_Msg->m026C[i].mSize.x * param_2, i_Msg->m026C[i].mSize.y * param_2);
    }
}

// Name unknown; mwcc-instr shows an inline call here (the selection height is a multi-def inline-local @temp with an id
// above the named locals, which swaps f0/f1 against local_78; a named local in any scope gets a declared-local id).
static inline f32 dMsg_subTextSelHeight(sub_msg_class* i_Msg) {
    f32 height;
    int lineSpace = (int)((J2DTextBox*)i_Msg->m0544[0].pane)->getLineSpace();
    if (i_Msg->mStatus == fopMsgStts_SELECT_2_e) {
        height = lineSpace + i_Msg->m110C;
    } else if (i_Msg->mStatus == fopMsgStts_SELECT_3_e) {
        height = i_Msg->m110C + lineSpace * 2;
    }
    return height;
}

/* 80210FF8-8021172C       .text dMsg_subTextSizeSet__FP13sub_msg_class */
void dMsg_subTextSizeSet(sub_msg_class* i_Msg) {
    f32 fVar1;
    f32 fVar2;
    f32 fVar3;
    f32 fVar5;
    int uVar6;
    f32 dVar11;
    f32 local_78;
    f32 local_68;
    f32 posY;

    local_78 = dMsg_subTextSelHeight(i_Msg);
    posY = i_Msg->m049C.mPosCenter.y;
    if (posY >= 240.0f) {
        posY = (int)g_msgHIO.field_0x7c + (posY - i_Msg->m049C.mSize.y / 2.0f);
    } else {
        posY = (posY + i_Msg->m049C.mSize.y / 2.0f) - ((int)g_msgHIO.field_0x7c);
    }
#if VERSION == VERSION_DEMO
    int len = i_Msg->mMsgDataProc.getSelectLength();
    fVar3 = (int)g_msgHIO.field_0x78 + ((2.0f * g_msgHIO.field_0x7e + len) - i_Msg->m011C[0].mSizeOrig.x);
#else
    local_68 = (int)g_msgHIO.field_0x78;
    fVar3 = local_68 + ((2.0f * g_msgHIO.field_0x7e + i_Msg->mMsgDataProc.getSelectLength()) - i_Msg->m011C[0].mSizeOrig.x);
#endif
    for (s32 i = 0; i < 4; i++) {
        i_Msg->m011C[i].mSize.x = i_Msg->m011C[i].mSizeOrig.x + fVar3;
    }
    i_Msg->m026C[5].mSize.x = i_Msg->m026C[5].mSizeOrig.x + fVar3;
    i_Msg->m026C[8].mSize.x = i_Msg->m026C[8].mSizeOrig.x + fVar3;
    i_Msg->m026C[9].mSize.x = i_Msg->m026C[9].mSizeOrig.x + fVar3;
    fVar3 = ((int)g_msgHIO.field_0x7a) + (local_78 - i_Msg->m011C[0].mSizeOrig.y);
    for (s32 i = 0; i < 4; i++) {
        i_Msg->m011C[i].mSize.y = i_Msg->m011C[i].mSizeOrig.y + fVar3;
    }
    i_Msg->m026C[6].mSize.y = i_Msg->m026C[6].mSizeOrig.y + fVar3;
    i_Msg->m026C[7].mSize.y = i_Msg->m026C[7].mSizeOrig.y + fVar3;
    i_Msg->m026C[9].mSize.y = i_Msg->m026C[9].mSizeOrig.y + fVar3;
    fVar3 = (i_Msg->m026C[9].mSizeOrig.x - i_Msg->m026C[9].mSize.x) / 2.0f;
    i_Msg->m026C[0].mPosCenter.x = i_Msg->m026C[0].mPosCenterOrig.x + fVar3;
    i_Msg->m026C[1].mPosCenter.x = i_Msg->m026C[1].mPosCenterOrig.x + fVar3;
    i_Msg->m026C[2].mPosCenter.x = i_Msg->m026C[2].mPosCenterOrig.x - fVar3;
    i_Msg->m026C[3].mPosCenter.x = i_Msg->m026C[3].mPosCenterOrig.x + fVar3;
    i_Msg->m026C[4].mPosCenter.x = i_Msg->m026C[4].mPosCenterOrig.x - fVar3;
    i_Msg->m026C[6].mPosCenter.x = i_Msg->m026C[6].mPosCenterOrig.x + fVar3;
    i_Msg->m026C[7].mPosCenter.x = i_Msg->m026C[7].mPosCenterOrig.x - fVar3;
    fVar3 = (i_Msg->m011C[0].mSizeOrig.x - i_Msg->m011C[0].mSize.x) / 2.0f;
    for (s32 i = 0; i < 4; i++) {
        i_Msg->m011C[i].mPosCenter.x = (i_Msg->m011C[i].mPosCenterOrig.x + fVar3) + g_msgHIO.field_0x7e;
    }
    fVar5 = (i_Msg->m026C[9].mSizeOrig.y - i_Msg->m026C[9].mSize.y) / 2.0f;
    fVar2 = posY - i_Msg->m026C[0].mPosCenterOrig.y;
    i_Msg->m026C[0].mPosCenter.y = posY;
    i_Msg->m026C[1].mPosCenter.y = i_Msg->m026C[1].mPosCenterOrig.y - fVar5;
    i_Msg->m026C[2].mPosCenter.y = i_Msg->m026C[2].mPosCenterOrig.y - fVar5;
    i_Msg->m026C[3].mPosCenter.y = i_Msg->m026C[3].mPosCenterOrig.y + fVar5;
    i_Msg->m026C[4].mPosCenter.y = i_Msg->m026C[4].mPosCenterOrig.y + fVar5;
    i_Msg->m026C[5].mPosCenter.y = i_Msg->m026C[5].mPosCenterOrig.y - fVar5;
    i_Msg->m026C[8].mPosCenter.y = i_Msg->m026C[8].mPosCenterOrig.y + fVar5;
    for (s32 i = 0; i < 4; i++) {
        i_Msg->m011C[i].mPosCenter.y = i_Msg->m011C[i].mPosCenterOrig.y + fVar2;
    }
    i_Msg->m01FC.mPosCenterOrig.x = i_Msg->m01FC.mPosTopLeftOrig.x + i_Msg->m01FC.mSizeOrig.x / 2.0f + fVar3;
#if VERSION == VERSION_DEMO
    i_Msg->m0234.mPosCenterOrig.x = i_Msg->m01FC.mPosCenterOrig.x + i_Msg->m01FC.mSizeOrig.x + i_Msg->mMsgDataProc.getSelectLength() + g_msgHIO.field_0x7e * 2.0f;
#else
    f32 f4 = g_msgHIO.field_0x7e * 2.0f;
    f32 f3 = i_Msg->m01FC.mPosCenterOrig.x + i_Msg->m01FC.mSizeOrig.x;
    i_Msg->m0234.mPosCenterOrig.x = f3 + i_Msg->mMsgDataProc.getSelectLength() + f4;
#endif
    i_Msg->m01FC.mPosCenterOrig.y = i_Msg->m01FC.mPosTopLeftOrig.y + i_Msg->m01FC.mSizeOrig.y / 2.0f + fVar5;
    i_Msg->m0234.mPosCenterOrig.y = i_Msg->m0234.mPosTopLeftOrig.y + i_Msg->m0234.mSizeOrig.y / 2.0f + fVar5;
    for (s32 i = 0; i < 4; i++) {
        fopMsgM_cposMove(&i_Msg->m011C[i]);
    }
    for (s32 i = 0; i < 10; i++) {
        fopMsgM_cposMove(&i_Msg->m026C[i]);
    }
    if (i_Msg->m026C[9].mSize.x >= 282.0f) {
        fVar1 = 2.0f;
    } else {
        fVar1 = 3.0f;
    }
    dVar11 = fVar1;
    fVar3 = i_Msg->m026C[0].mSize.y / 2.0f;
    i_Msg->m026C[0].pane->rotate(i_Msg->m026C[0].mSize.x / 2.0f, fVar3, ROTATE_Z, fVar1);
    for (s32 i = 0; i < 4; i++) {
        fVar1 = i_Msg->m011C[i].mSize.y / 2.0f;
        fVar3 = fVar1 + i_Msg->m026C[0].mPosCenter.y;
        fVar2 = fVar3 - i_Msg->m011C[i].mPosCenter.y;
        i_Msg->m011C[i].pane->rotate(
            ((i_Msg->m011C[i].mSize.x / 2.0f) + i_Msg->m026C[0].mPosCenter.x) - i_Msg->m011C[i].mPosCenter.x, fVar2, ROTATE_Z, dVar11
        );
    }
    fVar1 = i_Msg->m026C[9].mSize.x;
    uVar6 = (int)g_msgHIO.field_0x74;
    if (fVar1 <= uVar6) {
        fVar1 = 1.0f;
    } else {
        fVar1 = (int)((fVar1 / uVar6) + 0.5f);
    }
    fVar3 = i_Msg->m026C[9].mSize.y;
    uVar6 = (int)g_msgHIO.field_0x74;
    if (fVar3 <= uVar6) {
        fVar3 = 1.0f;
    } else {
        fVar3 = (int)((fVar3 / uVar6) + 0.5f);
    }
    ((MyPicture*)i_Msg->m026C[5].pane)->m134 = 1;
    ((MyPicture*)i_Msg->m026C[5].pane)->m124 = 0.0f;
    ((MyPicture*)i_Msg->m026C[5].pane)->m128 = 0.0f;
    ((MyPicture*)i_Msg->m026C[5].pane)->m12C = fVar1;
    ((MyPicture*)i_Msg->m026C[5].pane)->m130 = 1.0f;
    ((MyPicture*)i_Msg->m026C[6].pane)->m134 = 1;
    ((MyPicture*)i_Msg->m026C[6].pane)->m124 = 0.0f;
    ((MyPicture*)i_Msg->m026C[6].pane)->m128 = 0.0f;
    ((MyPicture*)i_Msg->m026C[6].pane)->m12C = fVar3;
    ((MyPicture*)i_Msg->m026C[6].pane)->m130 = 1.0f;
    ((MyPicture*)i_Msg->m026C[7].pane)->m134 = 1;
    ((MyPicture*)i_Msg->m026C[7].pane)->m124 = 0.0f;
    ((MyPicture*)i_Msg->m026C[7].pane)->m128 = 0.0f;
    ((MyPicture*)i_Msg->m026C[7].pane)->m12C = fVar3;
    ((MyPicture*)i_Msg->m026C[7].pane)->m130 = 1.0f;
    ((MyPicture*)i_Msg->m026C[8].pane)->m134 = 1;
    ((MyPicture*)i_Msg->m026C[8].pane)->m124 = 0.0f;
    ((MyPicture*)i_Msg->m026C[8].pane)->m128 = 0.0f;
    ((MyPicture*)i_Msg->m026C[8].pane)->m12C = fVar1;
    ((MyPicture*)i_Msg->m026C[8].pane)->m130 = 1.0f;
    i_Msg->m026C[9].mUserArea = 0;
}

/* 8021172C-802118D8       .text dMsg_subTextOpen__FP13sub_msg_class */
void dMsg_subTextOpen(sub_msg_class* i_Msg) {
    static const Vec frameScale = {1.5f, 0.96f, 1.0f};
    f32 fVar1;
    int iVar2;
    int iVar3;
    f32 dVar4;
    f32 dVar5;

    s32 tmp = g_msgHIO.field_0x44 + g_msgHIO.field_0x46;
    i_Msg->m026C[9].mUserArea++;
    dVar4 = fopMsgM_valueIncrease(tmp, (int)i_Msg->m026C[9].mUserArea, 0);
    iVar3 = (int)i_Msg->m026C[9].mUserArea;
    iVar2 = (int)g_msgHIO.field_0x44;
    if (iVar3 < iVar2) {
        dVar5 = fopMsgM_valueIncrease(iVar2, iVar3, 0);
        fVar1 = (dVar5 * (frameScale.y - frameScale.x)) + frameScale.x;
    } else {
        dVar5 = fopMsgM_valueIncrease((int)g_msgHIO.field_0x46, iVar3 - iVar2, 0);
        fVar1 = (dVar5 * (frameScale.z - frameScale.y)) + frameScale.y;
    }
    dMsg_subTextScale(i_Msg, (fVar1));
    for (s32 i = 0; i < 10; i++) {
        fopMsgM_setNowAlpha(&i_Msg->m026C[i], dVar4);
    }
#if VERSION == VERSION_DEMO
    f32 alpha = 1.0f - ((1.0f - (g_msgHIO.field_0x80) / 255.0f) * dVar4);
    dVar5 = (1.0f - ((1.0f - (g_msgHIO.field_0x81) / 255.0f) * dVar4));
    fopMsgM_setNowAlpha(&i_Msg->m049C, alpha);
#else
    dVar5 = (1.0f - ((1.0f - (g_msgHIO.field_0x81) / 255.0f) * dVar4));
    fopMsgM_setNowAlpha(&i_Msg->m049C, 1.0f - ((1.0f - (g_msgHIO.field_0x80) / 255.0f) * dVar4));
#endif
    fopMsgM_setNowAlpha(&i_Msg->m0544[0], dVar5);
    fopMsgM_setNowAlpha(&i_Msg->m0544[1], dVar5);
    fopMsgM_setNowAlpha(&i_Msg->m0544[2], dVar5);
    fopMsgM_setNowAlpha(&i_Msg->m0544[3], dVar5);
}

/* 802118D8-80211A8C       .text dMsg_subTextClose__FP13sub_msg_class */
void dMsg_subTextClose(sub_msg_class* i_Msg) {
    int iVar1;
    int iVar2;
    f32 dVar3;
    f32 dVar4;

    i_Msg->m026C[9].mUserArea++;
    iVar1 = (int)i_Msg->m026C[9].mUserArea;
    iVar2 = (int)g_msgHIO.field_0x48;
    if (iVar1 <= iVar2) {
        dVar3 = fopMsgM_valueIncrease(iVar2, iVar2 - iVar1, 0);
        dVar4 = fopMsgM_valueIncrease((int)g_msgHIO.field_0x48, (int)i_Msg->m026C[9].mUserArea, 0);
        dMsg_subTextScale(i_Msg, (((g_msgHIO.field_0x40 - 1.0f) * dVar4) + 1.0f));
        for (s32 i = 0; i < 10; i++) {
            fopMsgM_setNowAlpha(&i_Msg->m026C[i], dVar3);
        }
        fopMsgM_setNowAlphaZero(&i_Msg->m01FC);
        fopMsgM_setNowAlphaZero(&i_Msg->m0234);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[0]);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[1]);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[2]);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[3]);
    } else {
        dVar3 = fopMsgM_valueIncrease((int)g_msgHIO.field_0x76, (int)g_msgHIO.field_0x76 - (iVar1 - iVar2), 0);
#if VERSION == VERSION_DEMO
        f32 alpha = 1.0f - ((1.0f - (g_msgHIO.field_0x80) / 255.0f) * dVar3);
        dVar4 = (1.0f - (1.0f - ((g_msgHIO.field_0x81) / 255.0f)) * dVar3);
        fopMsgM_setNowAlpha(&i_Msg->m049C, alpha);
#else
        dVar4 = (1.0f - (1.0f - ((g_msgHIO.field_0x81) / 255.0f)) * dVar3);
        fopMsgM_setNowAlpha(&i_Msg->m049C, 1.0f - ((1.0f - (g_msgHIO.field_0x80) / 255.0f) * dVar3));
#endif
        fopMsgM_setNowAlpha(&i_Msg->m0544[0], dVar4);
        fopMsgM_setNowAlpha(&i_Msg->m0544[1], dVar4);
        fopMsgM_setNowAlpha(&i_Msg->m0544[2], dVar4);
        fopMsgM_setNowAlpha(&i_Msg->m0544[3], dVar4);
    }
}

/* 80211A8C-80211B34       .text dMsg_setCloseSound__FP13sub_msg_classUc */
void dMsg_setCloseSound(sub_msg_class* i_Msg, u8 param_2) {
    mDoAud_talkOut();
    if ((dComIfGp_getDoStatusForce() == dActStts_CANCEL_e) || (param_2 != 0)) {
        mDoAud_seStart(JA_SE_TALK_SEL_CANCEL);
    } else {
        mDoAud_seStart(JA_SE_TALK_WIN_CLOSE);
    }
}

/* 80211B34-80211BCC       .text dMsg_setCancelMode__FP13sub_msg_class */
void dMsg_setCancelMode(sub_msg_class* i_Msg) {
    i_Msg->mStatus = fopMsgStts_MSG_DISPLAYED_e;
    i_Msg->m1100 = 0;
    if (CPad_CHECK_TRIG_B(0)) {
        i_Msg->m1169 = 1;
        dComIfGp_setMesgCancelButton(1);
    } else {
        i_Msg->m1169 = 0;
    }
    dMsg_setCloseSound(i_Msg, 0);
    mDoAud_talkOut();
    i_Msg->m116A++;
    dComIfGp_setMesgSendButton(i_Msg->m116A);
}

/* 80211BCC-80211DA0       .text dMsg_messageShow__FP13sub_msg_class */
void dMsg_messageShow(sub_msg_class* i_Msg) {
    fopMsgM_paneScaleXY(&i_Msg->m049C, 1.0f);
    fopMsgM_setInitAlpha(&i_Msg->m049C);
    dMsg_messagePaneShow(i_Msg);
    dMsg_yose_select(i_Msg);
    if (i_Msg->mMesgEntry.mInitialSound != 0) {
        mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
    }
    i_Msg->mMsgDataProc.field_0x299 = 1;
    i_Msg->mMsgDataProc.stringSet();
    for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
        u8 iconNum = i_Msg->mMsgDataProc.getIconNum(i);
        u32 iconColor = i_Msg->mMsgDataProc.getIconColor(i);
        if (
            (
                iconNum == fopMsgM_Icon_ARROW_LEFT_e ||
                iconNum == fopMsgM_Icon_ARROW_RIGHT_e ||
                iconNum == fopMsgM_Icon_ARROW_UP_e ||
                iconNum == fopMsgM_Icon_ARROW_DOWN_e ||
                iconNum == fopMsgM_Icon_SELECT_YOKO_RIGHT_e ||
                iconNum == fopMsgM_Icon_FLASHING_A_BUTTON_2_e
            ) && iconColor == 0
        ) {
            iconColor = 0xFFFFFFFF;
        }
        if (
            iconNum != fopMsgM_Icon_NONE_e &&
            buttonTimer[i] == -1 &&
            iconNum != fopMsgM_Icon_SELECT_YOKO_LEFT_e &&
            iconNum != fopMsgM_Icon_SELECT_YOKO_RIGHT_e &&
            iconNum != fopMsgM_Icon_INPUT_e
        ) {
            fopMsgM_outFontSet(button_icon[i], button_kage[i], &buttonTimer[i], iconColor, iconNum);
        }
    }
    dMsg_setString(i_Msg);
    i_Msg->m1100 = 0;
    for (s32 i = 0; i < mBeatNum[dComIfGp_getMelodyNum()]; i++) {
        i_Msg->m0B64[i].mUserArea = 0;
        if (dComIfGp_getMelodyNum() < 5) {
            fopMsgM_setNowAlpha(&i_Msg->m0A14[i], 0.5f);
            fopMsgM_setNowAlphaZero(&i_Msg->m0B64[i]);
        } else {
            fopMsgM_setNowAlpha(&i_Msg->m0A14[i], 0.5f);
            fopMsgM_setNowAlpha(&i_Msg->m08C4[i], 0.5f);
            fopMsgM_setNowAlpha(&i_Msg->m0CB4[i], 0.5f);
        }
    }
}

/* 80211DA0-8021209C       .text dMsg_stopProc__FP13sub_msg_class */
s32 dMsg_stopProc(sub_msg_class* i_Msg) {
    u8 bVar1;
    if (i_Msg->mMsgDataProc.autoSendFlag != 0) {
        if (i_Msg->mMsgDataProc.dec_waitTimer() == 0) {
            i_Msg->mMsgDataProc.setAutoSendFlagOff();
#if VERSION > VERSION_DEMO
            i_Msg->mMsgDataProc.field_0x296 = 0;
#endif
            i_Msg->mStatus = fopMsgStts_MSG_TYPING_e;
            dMsg_value_init(i_Msg);
            dMsg_yose_select(i_Msg);
            dMsg_setString(i_Msg);
            bVar1 = i_Msg->mMesgEntry.mTextboxType;
            if ((bVar1 != 5) && (bVar1 != 0xe)) {
                dMsg_arrowInit(i_Msg);
            }
            dMeter_setDoStatus(2);
            if (dComIfGp_getDoStatusForce() == dActStts_CANCEL_e) {
                mDoAud_seStart(JA_SE_TALK_SEL_CANCEL);
            } else {
                mDoAud_seStart(JA_SE_TALK_NEXT);
            }
            i_Msg->m116A++;
            dComIfGp_setMesgSendButton(i_Msg->m116A);
        } else {
            bVar1 = i_Msg->mMesgEntry.mTextboxType;
            if ((bVar1 != 5) && (bVar1 != 0xe)) {
                dMsg_arrowMove(i_Msg);
            }
            dMeter_setDoStatus(1);
        }
    } else {
        if (dMsg_CHECK_TRIG_AB() && (!dComIfGp_checkMesgBgm())) {
            i_Msg->mStatus = fopMsgStts_MSG_TYPING_e;
            dMsg_value_init(i_Msg);
            dMsg_yose_select(i_Msg);
            dMsg_setString(i_Msg);
            if ((i_Msg->mMesgEntry.mTextboxType != 5) && (i_Msg->mMesgEntry.mTextboxType != 0xe)) {
                dMsg_arrowInit(i_Msg);
            }
            dMeter_setDoStatus(2);
            if (dComIfGp_getDoStatusForce() == dActStts_CANCEL_e) {
                mDoAud_seStart(JA_SE_TALK_SEL_CANCEL);
            } else {
                mDoAud_seStart(JA_SE_TALK_NEXT);
            }
            if (((i_Msg->mMesgEntry.mDrawType == 0) && (CPad_CHECK_TRIG_B(0))) && (!dComIfGp_checkMesgBgm())) {
#if VERSION > VERSION_DEMO
                if (i_Msg->mMsgDataProc.field_0x296 == 0)
#endif
                {
                    i_Msg->mMsgDataProc.field_0x299 = 1;
                }
                if (i_Msg->mMsgDataProc.getAutoSendFlag() == 0 && i_Msg->mMsgDataProc.getHandSendFlag() == 0 && i_Msg->mMsgDataProc.get_waitTimer() != 0) {
                    i_Msg->mMsgDataProc.set_waitTimerZero();
#if VERSION > VERSION_DEMO
                    i_Msg->mMsgDataProc.field_0x296 = 0;
#endif
                }
            }
            i_Msg->m116A++;
            dComIfGp_setMesgSendButton(i_Msg->m116A);
        } else {
            if ((i_Msg->mMesgEntry.mTextboxType != 5) && (i_Msg->mMesgEntry.mTextboxType != 0xe)) {
                dMsg_arrowMove(i_Msg);
            }
            dMeter_setDoStatus(1);
        }
    }
    return TRUE;
}

/* 8021209C-802128B0       .text dMsg_selectProc__FP13sub_msg_class */
s32 dMsg_selectProc(sub_msg_class* i_Msg) {
    int uVar1;
    s16 sVar3;
    J2DTextBox* pJVar8;
    u8 uVar9;
    int iVar10;
    int iVar11;
    J2DPicture* pJVar14;

    pJVar8 = (J2DTextBox*)i_Msg->m0544[0].pane;
    iVar10 = (int)pJVar8->getBounds().i.x;
    iVar11 = (int)pJVar8->getBounds().i.y;
    uVar1 = pJVar8->getLineSpace();
    sVar3 = i_Msg->m026C[0].mUserArea;
    if (sVar3 == 0) {
        if ((i_Msg->mStatus == fopMsgStts_SELECT_2_e) || (i_Msg->mStatus == fopMsgStts_SELECT_3_e)) {
            if (dMsg_CHECK_TRIG_AB() || (fopMsgM_checkMessageSend())) {
                dMsg_subTextSizeSet(i_Msg);
                i_Msg->m026C[0].mUserArea = 1;
                mDoAud_seStart(JA_SE_TALK_SEL_WIN_OPEN);
                goto end;
            }
            if ((i_Msg->mMesgEntry.mTextboxType != 5) && (i_Msg->mMesgEntry.mTextboxType != 0xe)) {
                dMsg_arrowMove(i_Msg);
            }
            dMeter_setDoStatus(1);
        } else {
            if (i_Msg->mStatus == fopMsgStts_SELECT_YOKO_e) {
                i_Msg->m026C[0].mUserArea = 2;
            }
        }
        goto end;
    }
    if (sVar3 == 1) {
        if ((int)i_Msg->m026C[9].mUserArea < (int)g_msgHIO.field_0x44 + (int)g_msgHIO.field_0x46) {
            dMsg_subTextOpen(i_Msg);
            goto end;
        }
        fopMsgM_setInitAlpha(&i_Msg->m01FC);
        fopMsgM_setInitAlpha(&i_Msg->m0234);
        fopMsgM_setInitAlpha(&i_Msg->m011C[0]);
        fopMsgM_setInitAlpha(&i_Msg->m011C[1]);
        fopMsgM_setInitAlpha(&i_Msg->m011C[2]);
        fopMsgM_setInitAlpha(&i_Msg->m011C[3]);
        i_Msg->m026C[9].mUserArea = 0;
        fopMsgM_paneTrans(&i_Msg->m01FC, 0.0f, 0.0f);
        fopMsgM_paneTrans(&i_Msg->m0234, 0.0f, 0.0f);
        i_Msg->m026C[0].mUserArea = 2;
        if ((i_Msg->mMesgEntry.mTextboxType == 5) || (i_Msg->mMesgEntry.mTextboxType == 0xe)) {
            goto end;
        }
        dMsg_arrowInit(i_Msg);
        goto end;
    }
    if (sVar3 != 2) {
        goto end;
    }
    if (i_Msg->mStatus == fopMsgStts_SELECT_2_e) {
#if VERSION <= VERSION_JPN
        if (g_msgDHIO.field_0x08 == 0) {
            i_Msg->mSelectNum = i_Msg->mMsgDataProc.selectCheck2(arrowPane, iVar10, iVar11 + uVar1, uVar1);
        } else
#endif
        {
            i_Msg->mSelectNum = i_Msg->mMsgDataProc.selectCheck2(arrowPane, iVar10, iVar11 + uVar1 + ((int)uVar1 / 2), uVar1);
        }
        i_Msg->m026C[8].mUserArea++;
        if (i_Msg->m026C[8].mUserArea >= 0x14) {
            i_Msg->m026C[8].mUserArea = 0;
        }
        sVar3 = i_Msg->m026C[8].mUserArea;
        if (sVar3 < 10) {
            pJVar14 = (J2DPicture*)i_Msg->m01FC.pane;
            pJVar14->setBlendRatio(1.0f, 0.0f, 1.0f, 1.0f);
            pJVar14 = (J2DPicture*)i_Msg->m0234.pane;
            pJVar14->setBlendRatio(1.0f, 0.0f, 1.0f, 1.0f);
        } else if (sVar3 < 0x14) {
            pJVar14 = (J2DPicture*)i_Msg->m01FC.pane;
            pJVar14->setBlendRatio(0.0f, 1.0f, 1.0f, 1.0f);
            pJVar14 = (J2DPicture*)i_Msg->m0234.pane;
            pJVar14->setBlendRatio(0.0f, 1.0f, 1.0f, 1.0f);
        }
        fopMsgM_paneTrans(&i_Msg->m01FC, 0.0f, i_Msg->mSelectNum * uVar1);
        fopMsgM_paneTrans(&i_Msg->m0234, 0.0f, i_Msg->mSelectNum * uVar1);
    } else if (i_Msg->mStatus == fopMsgStts_SELECT_3_e) {
        i_Msg->mSelectNum = i_Msg->mMsgDataProc.selectCheck3(arrowPane, iVar10, iVar11, uVar1);
        i_Msg->m026C[8].mUserArea++;
        if (i_Msg->m026C[8].mUserArea >= 0x14) {
            i_Msg->m026C[8].mUserArea = 0;
        }
        sVar3 = i_Msg->m026C[8].mUserArea;
        if (sVar3 < 10) {
            pJVar14 = (J2DPicture*)i_Msg->m01FC.pane;
            pJVar14->setBlendRatio(1.0f, 0.0f, 1.0f, 1.0f);
            pJVar14 = (J2DPicture*)i_Msg->m0234.pane;
            pJVar14->setBlendRatio(1.0f, 0.0f, 1.0f, 1.0f);
        } else if (sVar3 < 0x14) {
            pJVar14 = (J2DPicture*)i_Msg->m01FC.pane;
            pJVar14->setBlendRatio(0.0f, 1.0f, 1.0f, 1.0f);
            pJVar14 = (J2DPicture*)i_Msg->m0234.pane;
            pJVar14->setBlendRatio(0.0f, 1.0f, 1.0f, 1.0f);
        }
        fopMsgM_paneTrans(&i_Msg->m01FC, 0.0f, i_Msg->mSelectNum * uVar1);
        fopMsgM_paneTrans(&i_Msg->m0234, 0.0f, i_Msg->mSelectNum * uVar1);
    } else if (i_Msg->mStatus == fopMsgStts_SELECT_YOKO_e) {
        s32 var_r5 = 0;
        s32 var_r6 = 0;
        s32 var_r11 = 0;
        for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
            u8 iconNum = i_Msg->mMsgDataProc.getIconNum(i);
            if (iconNum == fopMsgM_Icon_SELECT_YOKO_LEFT_e) {
                var_r5 = i_Msg->m0544[0].pane->getBounds().i.x + i_Msg->mMsgDataProc.getIconPosX(i);
                var_r6 = i_Msg->m0544[0].pane->getBounds().i.y + i_Msg->m1104 * ((2 - i_Msg->m1108) + (i_Msg->mMsgDataProc.getIconPosY(i) * 2));
            } else if (iconNum == fopMsgM_Icon_SELECT_YOKO_RIGHT_e) {
                var_r11 = (i_Msg->m0544[0].pane->getBounds().i.x + (i_Msg->mMsgDataProc.getIconPosX(i)));
            }
        }
        i_Msg->mSelectNum = i_Msg->mMsgDataProc.selectCheckYoko(arrowPane, var_r5, var_r6, var_r11 - var_r5);
    }
#if VERSION == VERSION_DEMO
    if (!CPad_CHECK_TRIG_A(0) && !CPad_CHECK_TRIG_A(3)) {
        if (((!CPad_CHECK_TRIG_B(3)) || (dComIfGp_getAStatus() != dActStts_CANCEL_e)) &&
            ((!CPad_CHECK_TRIG_B(0)) || (dComIfGp_getAStatus() != dActStts_CANCEL_e)) && (!fopMsgM_checkMessageSend()))
        {
            goto end;
        }
    }
#else
    if (!CPad_CHECK_TRIG_A(0)) {
        if (((!CPad_CHECK_TRIG_B(0)) || (dComIfGp_getAStatus() != dActStts_CANCEL_e)) && (!fopMsgM_checkMessageSend())) {
            goto end;
        }
    }
#endif
    if ((i_Msg->mStatus == fopMsgStts_SELECT_2_e) || (i_Msg->mStatus == fopMsgStts_SELECT_3_e)) {
        i_Msg->m026C[0].mUserArea = 3;
        fopMsgM_setNowAlphaZero(&i_Msg->m01FC);
        fopMsgM_setNowAlphaZero(&i_Msg->m0234);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[0]);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[1]);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[2]);
        fopMsgM_setNowAlphaZero(&i_Msg->m011C[3]);
    } else {
        i_Msg->m026C[0].mUserArea = 0;
    }
    i_Msg->m1100 = 0;
    i_Msg->m026C[9].mUserArea = 0;
    if (CPad_CHECK_TRIG_B(0)) {
        i_Msg->m1169 = 1;
        dComIfGp_setMesgCancelButton(1);
    } else {
        i_Msg->m1169 = 0;
    }
    if (i_Msg->mMesgEntry.mNextMsgNo == 1) {
        if (i_Msg->mSelectNum == 0) {
            uVar9 = 1;
        } else {
            uVar9 = 0;
        }
    } else if (i_Msg->mMesgEntry.mNextMsgNo == 2) {
        uVar9 = 0;
    } else {
        if (((i_Msg->mStatus == fopMsgStts_SELECT_2_e) && (i_Msg->mSelectNum == 1)) || (i_Msg->mStatus == fopMsgStts_SELECT_3_e && (i_Msg->mSelectNum == 2))) {
            uVar9 = 1;
        } else {
            uVar9 = 0;
        }
    }
    dMsg_setCloseSound(i_Msg, uVar9);
    i_Msg->mStatus = fopMsgStts_MSG_DISPLAYED_e;
    i_Msg->m116A++;
    dComIfGp_setMesgSendButton(i_Msg->m116A);
end:
    return TRUE;
}

/* 802128B0-80212910       .text dMsg_inputProc__FP13sub_msg_class */
s32 dMsg_inputProc(sub_msg_class* i_Msg) {
    if (i_Msg->mMesgEntry.mTextboxType == 0xe) {
        if (fopMsgM_tactMsgFlagCheck()) {
            dMsg_tactInput2(i_Msg);
        } else {
            dMsg_tactInput(i_Msg);
        }
    } else {
        dMsg_numberInput(i_Msg);
    }
    return TRUE;
}

/* 80212910-80212DC0       .text dMsg_demoProc__FP13sub_msg_class */
s32 dMsg_demoProc(sub_msg_class* i_Msg) {
    if (i_Msg->m116B == 0) {
        int melody_no = dComIfGp_getMelodyNum();
        static bool flag = 0;
#if VERSION == VERSION_DEMO
        s16 limit = 0x23;
#endif
        static s16 cnt = 0;
        f32 local_64[7];
        local_64[0] = 0.0f;
        for (int i = 0; i < mBeatNum[melody_no]; i++) {
            s32 lStack_68;
            local_64[i + 1] = mDoAud_tact_getMelodyPattern((u32)melody_no, i, &lStack_68);
            local_64[i + 1] += local_64[i];
        }
        if (i_Msg->m1100 == 0) {
            cnt = 0;
            mDoAud_tact_melodyPlay(melody_no);
        }
        if (i_Msg->m1100 < (s16)local_64[mBeatNum[melody_no]]) {
            i_Msg->m1100++;
            if (i_Msg->m1100 >= (s16)local_64[cnt]) {
                i_Msg->m0B64[cnt].mUserArea = 1;
                cnt++;
            }
        }
        for (int i = 0; i < mBeatNum[melody_no]; i++) {
#if VERSION == VERSION_DEMO
            if ((i_Msg->m0B64[i].mUserArea > 0) && (i_Msg->m0B64[i].mUserArea <= limit)) {
#else
            if ((i_Msg->m0B64[i].mUserArea > 0) && (i_Msg->m0B64[i].mUserArea <= 0x23)) {
#endif
                if (i_Msg->m0B64[i].mUserArea < 5) {
                    f32 fVar10 = (fopMsgM_valueIncrease(5, i_Msg->m0B64[i].mUserArea, 0) * 0.5f) + 0.5f;
                    if (dComIfGp_getMelodyNum() < 5) {
                        fopMsgM_setNowAlpha(&i_Msg->m0A14[i], fVar10);
                        fopMsgM_setNowAlpha(&i_Msg->m0B64[i], fVar10);
                    } else {
                        fopMsgM_setNowAlpha(&i_Msg->m0A14[i], fVar10);
                        fopMsgM_setNowAlpha(&i_Msg->m08C4[i], fVar10);
                        fopMsgM_setNowAlpha(&i_Msg->m0CB4[i], fVar10);
                    }
                } else {
                    fopMsgM_setInitAlpha(&i_Msg->m0A14[i]);
                    fopMsgM_setInitAlpha(&i_Msg->m0B64[i]);
                    if (dComIfGp_getMelodyNum() >= 5) {
                        fopMsgM_setInitAlpha(&i_Msg->m0CB4[i]);
                    }
                }
                i_Msg->m0B64[i].mUserArea++;
#if VERSION == VERSION_DEMO
                if (i_Msg->m0B64[mBeatNum[melody_no] - 1].mUserArea > limit) {
#else
                if (i_Msg->m0B64[mBeatNum[melody_no] - 1].mUserArea > 0x23) {
#endif
                    if (flag != 0) {
                        dMsg_messagePaneShow(i_Msg);
                        dMsg_yose_select(i_Msg);
                        if (i_Msg->mMesgEntry.mInitialSound != 0) {
                            mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
                        }
                        i_Msg->mMsgDataProc.field_0x299 = 1;
                        i_Msg->mMsgDataProc.stringSet();
                        for (int j = 0; j < dMsg_OUTFONT_MAX; j++) {
                            u8 iconNum = i_Msg->mMsgDataProc.getIconNum(j);
                            u32 iconColor = i_Msg->mMsgDataProc.getIconColor(j);
                            if (
                                (
                                    iconNum == fopMsgM_Icon_ARROW_LEFT_e ||
                                    iconNum == fopMsgM_Icon_ARROW_RIGHT_e ||
                                    iconNum == fopMsgM_Icon_ARROW_UP_e ||
                                    iconNum == fopMsgM_Icon_ARROW_DOWN_e ||
                                    iconNum == fopMsgM_Icon_SELECT_YOKO_RIGHT_e ||
                                    iconNum == fopMsgM_Icon_FLASHING_A_BUTTON_2_e
                                ) && iconColor == 0
                            ) {
                                iconColor = 0xFFFFFFFF;
                            }
                            if (
                                iconNum != fopMsgM_Icon_NONE_e &&
                                buttonTimer[j] == -1 &&
                                iconNum != fopMsgM_Icon_SELECT_YOKO_LEFT_e &&
                                iconNum != fopMsgM_Icon_SELECT_YOKO_RIGHT_e &&
                                iconNum != fopMsgM_Icon_INPUT_e
                            ) {
                                fopMsgM_outFontSet(button_icon[j], button_kage[j], &buttonTimer[j], iconColor, iconNum);
                            }
                        }
                        dMsg_setString(i_Msg);
                        fopMsgM_setNowAlphaZero(&i_Msg->m0544[0]);
                        fopMsgM_setNowAlphaZero(&i_Msg->m0544[1]);
                        fopMsgM_setNowAlphaZero(&i_Msg->m0544[2]);
                        fopMsgM_setNowAlphaZero(&i_Msg->m0544[3]);
                        i_Msg->m116B = 1;
                        flag = 0;
                    } else {
                        flag += 1;
                    }
                    i_Msg->m1100 = 0;
                    for (int j = 0; j < mBeatNum[melody_no]; j++) {
                        i_Msg->m0B64[j].mUserArea = 0;
                        if (dComIfGp_getMelodyNum() < 5) {
                            fopMsgM_setNowAlpha(&i_Msg->m0A14[j], 0.5f);
                            fopMsgM_setNowAlphaZero(&i_Msg->m0B64[j]);
                        } else {
                            fopMsgM_setNowAlpha(&i_Msg->m0A14[j], 0.5f);
                            fopMsgM_setNowAlpha(&i_Msg->m08C4[j], 0.5f);
                            fopMsgM_setNowAlpha(&i_Msg->m0CB4[j], 0.5f);
                        }
                    }
                }
            }
        }
    } else {
        f32 dVar9 = fopMsgM_valueIncrease(5, i_Msg->m116B, 0);
        fopMsgM_setNowAlpha(&i_Msg->m0544[0], dVar9);
        fopMsgM_setNowAlpha(&i_Msg->m0544[1], dVar9);
        fopMsgM_setNowAlpha(&i_Msg->m0544[2], dVar9);
        fopMsgM_setNowAlpha(&i_Msg->m0544[3], dVar9);
        if (i_Msg->m116B >= 5) {
            i_Msg->mStatus = fopMsgStts_INPUT_e;
        } else {
            i_Msg->m116B++;
        }
    }
    return TRUE;
}

/* 80212DC0-80213650       .text dMsg_continueProc__FP13sub_msg_class */
s32 dMsg_continueProc(sub_msg_class* i_Msg) {
    int iVar1;
    int iVar2;
    f32 dVar3;
    f32 fVar5;
    char* pcVar9;
    fopAc_ac_c* pActor;

    iVar1 = (int)((J2DTextBox*)i_Msg->m0544[0].pane)->getCharSpace();
    iVar2 = (int)((J2DTextBox*)i_Msg->m0544[1].pane)->getCharSpace();
    if (
        (
            dMsg_CHECK_TRIG_AB() ||
            i_Msg->mMsgDataProc.getSelectFlag() != fopMsgM_msgDataProc_c::Select_OFF ||
            fopMsgM_checkMessageSend() ||
            (i_Msg->mMsgNo == 0x5ac && dComIfGp_checkMesgCancelButton())
        ) &&
        !dComIfGp_checkMesgBgm()
    ) {
        i_Msg->mMsgDataProc.setSelectFlagOff();
        i_Msg->m1100 = 0;
        JKRRemoveResource(i_Msg->head_p, NULL);
        if (i_Msg->mMesgEntry.mTextboxType != 5 && i_Msg->mMesgEntry.mTextboxType != 0xe) {
            dMsg_arrowInit(i_Msg);
        }
        if (CPad_CHECK_TRIG_B(0)) {
            i_Msg->m1169 = 1;
            dComIfGp_setMesgCancelButton(1);
        }
        if (i_Msg->m1169 != 0) {
            i_Msg->mMsgNo = i_Msg->field_0xf0;
            i_Msg->m1169 = 0;
        }
        i_Msg->head_p = i_Msg->mMsgGet.getMesgHeader(i_Msg->mMsgNo);
        JUT_ASSERT(VERSION_SELECT(3912, 3973, 4002, 4028), i_Msg->head_p);
        pcVar9 = (char*)i_Msg->mMsgGet.getMessage(i_Msg->head_p);
        i_Msg->mpMesgStr = (char*)pcVar9;
        i_Msg->mMesgEntry = i_Msg->mMsgGet.getMesgEntry(i_Msg->head_p);
        i_Msg->mMsgID = i_Msg->mMsgGet.mGroupID << 8 | i_Msg->mMsgGet.mResMsgNo;
        i_Msg->m116A = 0;
        i_Msg->m1164 = -1;
        dMsg_value_init(i_Msg);
        i_Msg->mMsgDataProc.dataInit();
        i_Msg->mMsgDataProc.setStringColor(i_Msg->m1164);
        i_Msg->mMsgDataProc.setBmgData(i_Msg->mpMesgStr);
        i_Msg->mMsgDataProc.setOutMessage(i_Msg->output_text, i_Msg->output_rub, i_Msg->output_textSdw, i_Msg->output_rubSdw);
        i_Msg->mMsgDataProc.setSelectMessage(i_Msg->select_text, i_Msg->select_rub, i_Msg->select_textSdw, i_Msg->select_rubSdw);
        i_Msg->mMsgDataProc.setFont(textFont);
        i_Msg->mMsgDataProc.setRubyFont(rubyFont);
        i_Msg->mMsgDataProc.setCharSpace(iVar1);
        i_Msg->mMsgDataProc.setRubyCharSpace(iVar2);
        i_Msg->mMsgDataProc.setMesgEntry(&i_Msg->mMesgEntry);
        i_Msg->mMsgDataProc.setFontSize(i_Msg->m110C);
        i_Msg->mMsgDataProc.setRubyFontSize(i_Msg->m1110);
        if ((i_Msg->mMesgEntry.mTextboxType == 9) && (i_Msg->m0624[8].mUserArea == 0)) {
            i_Msg->mMsgDataProc.setLineWidth(0x1a3);
            i_Msg->mMsgDataProc.setCenterLineWidth(0x192);
        } else {
            i_Msg->mMsgDataProc.setLineWidth(0x1f7);
            i_Msg->mMsgDataProc.setCenterLineWidth(0x1e6);
        }
        i_Msg->mMsgDataProc.setSendSpeed(g_msgHIO.field_0x82);
        i_Msg->mMsgDataProc.setSpaceTimer(2);
        if (g_msgHIO.field_0x83 != 0) {
            i_Msg->mMsgDataProc.setSpaceFlagOn();
        } else {
            i_Msg->mMsgDataProc.setSpaceFlagOff();
        }
        dMsg_yose_select(i_Msg);
        if (i_Msg->mMesgEntry.mInitialSound != 0) {
            mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
        }
        dMsg_setString(i_Msg);
        dMeter_setDoStatus(2);
        if (dComIfGp_getDoStatusForce() == dActStts_CANCEL_e) {
            mDoAud_seStart(JA_SE_TALK_SEL_CANCEL);
        } else {
            mDoAud_seStart(JA_SE_TALK_NEXT);
        }
        i_Msg->m116A++;
        dComIfGp_setMesgSendButton(i_Msg->m116A);
        dComIfGp_setMesgCancelButton(0);
        dComIfGp_setMesgAnimeAttrInfo(i_Msg->mMesgEntry.mInitialAnimation);
#if VERSION > VERSION_DEMO
        u8 temp_r4_2 = i_Msg->mMesgEntry.mInitialCamera;
#endif
        dComIfGp_setMesgCameraAttrInfo(i_Msg->mMesgEntry.mInitialCamera);
        dComIfGp_setMessageRupee(i_Msg->mMesgEntry.mItemPrice);
        if ((((i_Msg->mMesgEntry.mInitialCamera == 1) || (i_Msg->mMesgEntry.mInitialCamera == 2)) || (i_Msg->mMesgEntry.mInitialCamera == 3)) ||
            (i_Msg->mMesgEntry.mInitialCamera == 4 || (i_Msg->mMesgEntry.mInitialCamera == 5)))
        {
            dComIfG_MesgCamInfo_c* aMsgCamInfo_p = dComIfGp_getMesgCameraInfo();
#if VERSION == VERSION_DEMO
            pActor = aMsgCamInfo_p->mActor[i_Msg->mMesgEntry.mInitialCamera - 1];
#else
            pActor = aMsgCamInfo_p->mActor[temp_r4_2 - 1];
#endif
            if (pActor != NULL) {
                dComIfGp_event_setTalkPartner(pActor);
            }
        }
        if (i_Msg->mMesgEntry.mTextboxType == 1) {
            dMsg_msg_pane_parts_set(&i_Msg->m049C, 1);
            if (i_Msg->mMesgEntry.mTextboxPosition != 1) {
#if VERSION == VERSION_DEMO
                i_Msg->m049C.mPosCenter.y += 225.0f;
                i_Msg->m049C.mPosTopLeft.y += 225.0f;
#else
                f32 tmp = 225.0f;
                i_Msg->m049C.mPosCenter.y += tmp;
                i_Msg->m049C.mPosTopLeft.y += tmp;
#endif
            }
            dMsg_arw_pane_parts_set(&i_Msg->mPane_Arrow, &i_Msg->m049C);
            dMsg_arw_pane_parts_set(&i_Msg->m050C, &i_Msg->m049C);
            i_Msg->m1114 = i_Msg->mPane_Arrow.mPosTopLeft.y + i_Msg->mPane_Arrow.mSizeOrig.y;
            cXyz tmp;
#if VERSION == VERSION_DEMO
            tmp.x = (int)i_Msg->m0544[0].mPosTopLeftOrig.x;
            tmp.y = ((int)(480.0f - (i_Msg->m0544[0].mPosTopLeftOrig.y + i_Msg->m0544[0].mSizeOrig.y)) - 0x13);
#else
            dVar3 = (int)i_Msg->m0544[0].mPosTopLeftOrig.x;
            tmp.x = dVar3;
            fVar5 = ((int)(480.0f - (i_Msg->m0544[0].mPosTopLeftOrig.y + i_Msg->m0544[0].mSizeOrig.y)) - 0x13);
            tmp.y = fVar5;
#endif
            if (i_Msg->mMesgEntry.mTextboxPosition != 1) {
                i_Msg->m10F8 = (int)tmp.x;
                i_Msg->m10FC = (int)(tmp.y + 225.0f);
            } else {
                i_Msg->m10F8 = (int)tmp.x;
                i_Msg->m10FC = (int)(tmp.y);
            }
            i_Msg->m0544[0].pane->move(tmp.x, tmp.y - (f32)g_msgHIO.field_0x62 - (f32)textOffsetY);
            i_Msg->m0544[1].pane->move(tmp.x, tmp.y - (f32)g_msgHIO.field_0x60 - (f32)textOffsetY);
            i_Msg->m0544[2].pane->move(tmp.x + 2.0f, (tmp.y - (f32)g_msgHIO.field_0x62 - (f32)textOffsetY) + 2.0f);
            i_Msg->m0544[3].pane->move(tmp.x + 2.0f, (tmp.y - (f32)g_msgHIO.field_0x60 - (f32)textOffsetY) + 2.0f);
            i_Msg->m10D8 = i_Msg->m049C.mPosCenter.x;
            i_Msg->m10DC = i_Msg->m049C.mPosCenter.y;
            fopMsgM_cposMove(&i_Msg->m049C);
        } else if (i_Msg->mMesgEntry.mTextboxType != 0xe) {
            dMsg_mesgOutPos(i_Msg);
            i_Msg->m10D8 = (int)i_Msg->m049C.mPosCenter.x;
            i_Msg->m10DC = (int)i_Msg->m049C.mPosCenter.y;
        }
        if (((i_Msg->mMesgEntry.mDrawType == 0) && (CPad_CHECK_TRIG_B(0))) && (!dComIfGp_checkMesgBgm())) {
#if VERSION > VERSION_DEMO
            if (i_Msg->mMsgDataProc.field_0x296 == 0)
#endif
            {
                i_Msg->mMsgDataProc.field_0x299 = 1;
            }
            if (((i_Msg->mMsgDataProc.autoSendFlag == 0) && (i_Msg->mMsgDataProc.getHandSendFlag() == 0)) && (i_Msg->mMsgDataProc.get_waitTimer() !=0)) {
                i_Msg->mMsgDataProc.waitTimer = 0;
#if VERSION > VERSION_DEMO
                i_Msg->mMsgDataProc.field_0x296 = 0;
#endif
            }
        }
        i_Msg->mStatus = fopMsgStts_MSG_TYPING_e;
    } else {
        if ((i_Msg->mMesgEntry.mTextboxType != 5) && (i_Msg->mMesgEntry.mTextboxType != 0xe)) {
            dMsg_arrowMove(i_Msg);
        }
        dMeter_setDoStatus(1);
    }
    return TRUE;
}

/* 80213650-80213830       .text dMsg_closewaitProc__FP13sub_msg_class */
s32 dMsg_closewaitProc(sub_msg_class* i_Msg) {
    u8 bVar1;

    if (i_Msg->mMsgDataProc.autoSendFlag != 0) {
        if ((i_Msg->mMsgDataProc.dec_waitTimer() == 0) || (fopMsgM_checkMessageSend())) {
            i_Msg->mMsgDataProc.setAutoSendFlagOff();
#if VERSION > VERSION_DEMO
            i_Msg->mMsgDataProc.field_0x296 = 0;
#endif
            i_Msg->mStatus = fopMsgStts_BOX_CLOSING_e;
            i_Msg->m1100 = 0;
            i_Msg->mMsgDataProc.set_waitTimerZero();
            i_Msg->m116A++;
            dComIfGp_setMesgSendButton(i_Msg->m116A);
        }
    } else {
        if (i_Msg->mMsgDataProc.handSendFlag != 0) {
            if (i_Msg->mMsgDataProc.dec_waitTimer() != 0) {
                if ((dMsg_CHECK_TRIG_AB() || (fopMsgM_checkMessageSend())) && (!dComIfGp_checkMesgBgm())) {
                    i_Msg->mMsgDataProc.setHandSendFlagOff();
                    i_Msg->mStatus = fopMsgStts_BOX_CLOSING_e;
                    bVar1 = i_Msg->mMesgEntry.mTextboxType;
                    if ((bVar1 != 5) && (bVar1 != 0xe)) {
                        fopMsgM_setNowAlphaZero(&i_Msg->m050C);
                    }
                    i_Msg->m1100 = 0;
                    dMeter_setDoStatus(0);
                    i_Msg->m116A++;
                    dComIfGp_setMesgSendButton(i_Msg->m116A);
                } else {
                    bVar1 = i_Msg->mMesgEntry.mTextboxType;
                    if ((bVar1 != 5) && (bVar1 != 0xe)) {
                        dMsg_dotMove(i_Msg);
                    }
                    dMeter_setDoStatus(4);
                }
            } else {
                i_Msg->mMsgDataProc.handSendFlag = 0;
                i_Msg->mStatus = fopMsgStts_BOX_CLOSING_e;
                bVar1 = i_Msg->mMesgEntry.mTextboxType;
                if ((bVar1 != 5) && (bVar1 != 0xe)) {
                    fopMsgM_setNowAlphaZero(&i_Msg->m050C);
                }
                i_Msg->m1100 = 0;
                dMeter_setDoStatus(0);
                i_Msg->m116A++;
                dComIfGp_setMesgSendButton(i_Msg->m116A);
            }
        }
    }
    return TRUE;
}

/* 80213830-80213960       .text dMsg_finishProc__FP13sub_msg_class */
s32 dMsg_finishProc(sub_msg_class* i_Msg) {
    if (
        (
            dMsg_CHECK_TRIG_AB() ||
            i_Msg->mMsgDataProc.getSelectFlag() != fopMsgM_msgDataProc_c::Select_OFF ||
            fopMsgM_checkMessageSend() ||
            (i_Msg->mMsgNo == 0x5ac && dComIfGp_checkMesgCancelButton())
        ) &&
        !dComIfGp_checkMesgBgm()
    ) {
        i_Msg->mMsgDataProc.setSelectFlagOff();
        i_Msg->mStatus = fopMsgStts_BOX_CLOSING_e;
        if ((i_Msg->mMesgEntry.mTextboxType != 5) && (i_Msg->mMesgEntry.mTextboxType != 0xe)) {
            fopMsgM_setNowAlphaZero(&i_Msg->m050C);
        }
        i_Msg->m1100 = 0;
        dMeter_setDoStatus(0);
        i_Msg->m116A++;
        dComIfGp_setMesgSendButton(i_Msg->m116A);
        if (i_Msg->mMsgNo == 0x1072) {
            mDoAud_bgmStop(60);
        }
    } else {
        if ((i_Msg->mMesgEntry.mTextboxType != 5) && (i_Msg->mMesgEntry.mTextboxType != 0xe)) {
            dMsg_dotMove(i_Msg);
        }
        dMeter_setDoStatus(4);
    }
    return TRUE;
}

/* 80213960-802139F0       .text dMsg_openTalkProc__FP13sub_msg_class */
s32 dMsg_openTalkProc(sub_msg_class* i_Msg) {
    if (i_Msg->m1100 == 0xd) {
        i_Msg->mStatus = fopMsgStts_MSG_TYPING_e;
        dMsg_messagePaneShow(i_Msg);
        dMsg_yose_select(i_Msg);
        if (i_Msg->mMesgEntry.mInitialSound != 0) {
            mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
        }
        dMsg_setString(i_Msg);
        dMeter_setDoStatus(2);
    } else {
        dMsg_frame_openTalk(i_Msg);
    }
    return TRUE;
}

/* 802139F0-80213C20       .text dMsg_openItemProc__FP13sub_msg_class */
s32 dMsg_openItemProc(sub_msg_class* i_Msg) {
    cXyz local_1c;
    cXyz local_28;

    if (i_Msg->m1100 == 8) {
        i_Msg->mStatus = fopMsgStts_MSG_TYPING_e;
        dMsg_messagePaneShow(i_Msg);
        dMsg_yose_select(i_Msg);
        if (i_Msg->mMesgEntry.mInitialSound != 0) {
            mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
        }
        dMsg_setString(i_Msg);
        if ((i_Msg->mMsgNo == 0x74) || (i_Msg->mMsgNo == 0xbe)) {
#if VERSION == VERSION_DEMO
            f32 y = (i_Msg->m0624[8].mSize.y / 2.0f + i_Msg->m0624[8].pane->getGlbBounds().i.y) - 240.0f;
            f32 x = (i_Msg->m0624[8].mSize.x / 2.0f + i_Msg->m0624[8].pane->getGlbBounds().i.x) - 320.0f;
#else
            f32 tmp = 0.5f;
            f32 y = (i_Msg->m0624[8].mSize.y * tmp + i_Msg->m0624[8].pane->getGlbBounds().i.y) - 240.0f;
            f32 x = (i_Msg->m0624[8].mSize.x * tmp + i_Msg->m0624[8].pane->getGlbBounds().i.x) - 320.0f;
#endif
            local_1c.set(x, y, 0.0f);
            i_Msg->m10B4[0] = dComIfGp_particle_set2Dfore(dPa_name::ID_HM_J2_RUPYLIGHT, &local_1c);
            if (i_Msg->mMsgNo == 0xbe) {
                i_Msg->m10B4[1] = dComIfGp_particle_set2Dfore(dPa_name::ID_HM_J2_RUPYLIGHT, &local_1c);
            }
        } else if (i_Msg->mMsgNo == 0x9a) {
#if VERSION == VERSION_DEMO
            f32 y = ((i_Msg->m0624[8].mSize.y / 2.0f + i_Msg->m0624[8].pane->getGlbBounds().i.y) - 240.0f) - 10.0f;
            f32 x = ((i_Msg->m0624[8].mSize.x / 2.0f + i_Msg->m0624[8].pane->getGlbBounds().i.x) - 320.0f) + 10.0f;
#else
            f32 tmp = 0.5f;
            f32 y = ((i_Msg->m0624[8].mSize.y * tmp + i_Msg->m0624[8].pane->getGlbBounds().i.y) - 240.0f) - 10.0f;
            f32 x = ((i_Msg->m0624[8].mSize.x * tmp + i_Msg->m0624[8].pane->getGlbBounds().i.x) - 320.0f) + 10.0f;
#endif
            local_28.set(x, y, 0.0f);
            i_Msg->m10B4[0] = dComIfGp_particle_set2Dfore(dPa_name::ID_HM_J2_ARWG_FLAME00, &local_28);
        }
        dMeter_setDoStatus(2);
    } else {
        dMsg_frame_openItem(i_Msg);
    }
    return TRUE;
}

/* 80213C20-80213E28       .text dMsg_openTactProc__FP13sub_msg_class */
s32 dMsg_openTactProc(sub_msg_class* i_Msg) {
    if (i_Msg->mMsgNo == 0x5ac) {
        i_Msg->mStatus = fopMsgStts_INPUT_e;
    } else if ((i_Msg->mMsgNo == 0x5b3) || (i_Msg->mMsgNo == 0x5b4)) {
        u8 uVar2 = dComIfGp_getMelodyNum();
        if (((s32)uVar2 != 1) && ((s32)uVar2 != 5)) {
            dMsg_tactGuideShow(i_Msg, mBeatNum[uVar2]);
            i_Msg->mStatus = fopMsgStts_DEMO_e;
            i_Msg->m116B = 0;
        } else {
            dMsg_tactGuideShow(i_Msg, mBeatNum[uVar2]);
            dMsg_messageShow(i_Msg);
            i_Msg->mStatus = fopMsgStts_INPUT_e;
            i_Msg->m1168 = mDoAud_tact_getBeat();
            return TRUE;
        }
    } else {
        i_Msg->mStatus = fopMsgStts_TACT_e;
    }
    fopMsgM_paneScaleXY(&i_Msg->m049C, 1.0f);
    fopMsgM_setInitAlpha(&i_Msg->m049C);
    if (i_Msg->mStatus != fopMsgStts_DEMO_e) {
        dMsg_messagePaneShow(i_Msg);
        dMsg_yose_select(i_Msg);
        if (i_Msg->mMesgEntry.mInitialSound != 0) {
            mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
        }
        i_Msg->mMsgDataProc.field_0x299 = 1;
        i_Msg->mMsgDataProc.stringSet();
        for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
            u8 iconNum = i_Msg->mMsgDataProc.getIconNum(i);
            u32 iconColor = i_Msg->mMsgDataProc.getIconColor(i);
            if (
                (
                    iconNum == fopMsgM_Icon_ARROW_LEFT_e ||
                    iconNum == fopMsgM_Icon_ARROW_RIGHT_e ||
                    iconNum == fopMsgM_Icon_ARROW_UP_e ||
                    iconNum == fopMsgM_Icon_ARROW_DOWN_e ||
                    iconNum == fopMsgM_Icon_SELECT_YOKO_RIGHT_e ||
                    iconNum == fopMsgM_Icon_FLASHING_A_BUTTON_2_e
                ) && iconColor == 0
            ) {
                iconColor = 0xFFFFFFFF;
            }
            if (
                iconNum != fopMsgM_Icon_NONE_e &&
                buttonTimer[i] == -1 &&
                iconNum != fopMsgM_Icon_SELECT_YOKO_LEFT_e &&
                iconNum != fopMsgM_Icon_SELECT_YOKO_RIGHT_e &&
                iconNum != fopMsgM_Icon_INPUT_e
            ) {
                fopMsgM_outFontSet(button_icon[i], button_kage[i], &buttonTimer[i], iconColor, iconNum);
            }
        }
        dMsg_setString(i_Msg);
    }
    i_Msg->m1100 = 0;
    return TRUE;
}

/* 80213E28-80213E9C       .text dMsg_openDemoProc__FP13sub_msg_class */
s32 dMsg_openDemoProc(sub_msg_class* i_Msg) {
    i_Msg->mStatus = fopMsgStts_MSG_TYPING_e;
    dMsg_yose_select(i_Msg);
    if (i_Msg->mMesgEntry.mInitialSound != 0) {
        mDoAud_messageSePlay(i_Msg->mMesgEntry.mInitialSound, NULL, dComIfGp_getReverb(dComIfGp_roomControl_getStayNo()));
    }
    dMsg_setString(i_Msg);
    dMsg_messagePaneShow(i_Msg);
    return TRUE;
}

/* 80213E9C-80213EC0       .text dMsg_closeProc__FP13sub_msg_class */
s32 dMsg_closeProc(sub_msg_class* i_Msg) {
    dMsg_frame_close(i_Msg);
    return TRUE;
}

/* 80213EC0-80213F00       .text dMsg_closeProc2__FP13sub_msg_class */
s32 dMsg_closeProc2(sub_msg_class* i_Msg) {
    i_Msg->mStatus = fopMsgStts_BOX_CLOSED_e;
    dMsg_messagePaneHide(i_Msg);
#if VERSION <= VERSION_JPN
    i_Msg->m049C.pane->hide();
#endif
    dMsg_outFontHide(i_Msg);
    return TRUE;
}

/* 80213F00-802140CC       .text dMsg_initProc__FP13sub_msg_class */
s32 dMsg_initProc(sub_msg_class* i_Msg) {
    const char* pcVar3;
    char acStack_28[24];

#if VERSION == VERSION_DEMO
    bool trig = false;
    if (CPad_CHECK_TRIG_A(3)) {
        trig = true;
    }
    if (dComIfGp_checkCameraAttentionStatus(dComIfGp_getPlayerCameraID(0), dCamAttnStts_00000004_e) || fopMsgM_demoMsgFlagCheck() || trig) {
#else
    if (dComIfGp_checkCameraAttentionStatus(dComIfGp_getPlayerCameraID(0), dCamAttnStts_00000004_e) || fopMsgM_demoMsgFlagCheck()) {
#endif
        pcVar3 = i_Msg->mpMesgStr;
        if (((u8)pcVar3[0] == 0x1A) && ((u8)pcVar3[2] == 0x02)) {
            dComIfGp_setMesgCameraTagInfo((s32)(u16)((((u8)pcVar3[3] << 8) & ~0xFFFF00FF) | (((u8)pcVar3[4]) & ~0xFF00)));
            i_Msg->mpMesgStr = (char*)pcVar3 + pcVar3[1];
            i_Msg->mMesgCameraTagInfo = dComIfGp_getMesgCameraTagInfo();
        } else if (((((u8)pcVar3[0] == 0x1A) && ((u8)pcVar3[2] == 0xFF)) && ((u8)pcVar3[3] == 0x00)) && ((u8)pcVar3[4] == 0x00)) {
            if ((pcVar3[5] >= 0x00) && (pcVar3[5] <= 0x08)) {
                i_Msg->m1164 = fopMsgM_getColorTable(pcVar3[5]);
                // clang-format off
                sprintf(acStack_28,"\x1b" "CC[%08x]" "\x1b" "GM[0]", i_Msg->m1164);
                // clang-format on
                strcat(i_Msg->output_text, acStack_28);
            }
            i_Msg->mpMesgStr = i_Msg->mpMesgStr + (s8)i_Msg->mpMesgStr[1];
        } else {
            switch (i_Msg->mMesgEntry.mTextboxType) {
            case 5:
                dMsg_ScreenDataValueInitDemo(i_Msg);
                break;
            case 1:
            case 9:
                dMsg_ScreenDataValueInitItem(i_Msg);
                break;
            case 0xe:
                dMsg_ScreenDataValueInitTact(i_Msg);
                break;
            default:
                dMsg_ScreenDataValueInitTalk(i_Msg);
                break;
            }
            i_Msg->mStatus = fopMsgStts_BOX_OPENING_e;
            mDoAud_talkIn();
            mDoAud_seStart(JA_SE_TALK_WIN_OPEN);
        }
    }
    return TRUE;
}

/* 802140CC-8021411C       .text dMsg_tactProc__FP13sub_msg_class */
s32 dMsg_tactProc(sub_msg_class* i_Msg) {
#if VERSION == VERSION_DEMO
    if (fopMsgM_checkMessageSend() || CPad_CHECK_TRIG_A(3)) {
#else
    if (fopMsgM_checkMessageSend()) {
#endif
        i_Msg->mMsgDataProc.setAutoSendFlagOn();
        i_Msg->mMsgDataProc.set_waitTimer(30);
        i_Msg->mStatus = fopMsgStts_CLOSE_WAIT_e;
    }
    return TRUE;
}

/* 8021411C-802143A8       .text dMsg_outnowProc__FP13sub_msg_class */
s32 dMsg_outnowProc(sub_msg_class* i_Msg) {
    u32 iVar3;

    if (i_Msg->mMesgCameraTagInfo != dComIfGp_getMesgCameraTagInfo()) {
        if ((dComIfGp_checkCameraAttentionStatus(dComIfGp_getPlayerCameraID(0), dCamAttnStts_00000004_e)) || (fopMsgM_demoMsgFlagCheck())
#if VERSION == VERSION_DEMO
            || CPad_CHECK_TRIG_A(3)
#endif
        ) {
            dMsg_mesgOutPos(i_Msg);
            i_Msg->m10D8 = (int)i_Msg->m049C.mPosCenter.x;
            i_Msg->m10DC = (int)i_Msg->m049C.mPosCenter.y;
            fopMsgM_setInitAlpha(&i_Msg->m049C);
            i_Msg->mMesgCameraTagInfo = dComIfGp_getMesgCameraTagInfo();
        }
    }
    if (i_Msg->mMesgEntry.mDrawType == 0) {
        if (dMsg_CHECK_TRIG_AB() && (!dComIfGp_checkMesgBgm())) {
            i_Msg->mMsgDataProc.field_0x299 = 1;
            if ((i_Msg->mMsgDataProc.autoSendFlag == 0 && (i_Msg->mMsgDataProc.getHandSendFlag() == 0)) && ((s32)i_Msg->mMsgDataProc.get_waitTimer() !=0)) {
                i_Msg->mMsgDataProc.set_waitTimerZero();
#if VERSION > VERSION_DEMO
                i_Msg->mMsgDataProc.field_0x296 = 0;
#endif
            }
        }
    } else if (
        i_Msg->mMesgEntry.mDrawType == 1
#if VERSION > VERSION_DEMO
        && i_Msg->mMsgDataProc.field_0x296 == 0
#endif
    ) {
        i_Msg->mMsgDataProc.field_0x299 = 1;
    }
    if (((i_Msg->mMsgDataProc.getAutoSendFlag() == 0) && (i_Msg->mMsgDataProc.getHandSendFlag() == 0)) && (i_Msg->mMsgDataProc.get_waitTimer() != 0)) {
        i_Msg->mMsgDataProc.dec_waitTimer();
    } else {
        i_Msg->mMsgDataProc.stringSet();
        i_Msg->m1164 = i_Msg->mMsgDataProc.getStringColor();
        i_Msg->mStatus = (u16)i_Msg->mMsgDataProc.mesgStatus;
        for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
            u8 iconNum = i_Msg->mMsgDataProc.getIconNum(i);
            u32 iconColor = i_Msg->mMsgDataProc.getIconColor(i);
            if (
                (
                    iconNum == fopMsgM_Icon_ARROW_LEFT_e ||
                    iconNum == fopMsgM_Icon_ARROW_RIGHT_e ||
                    iconNum == fopMsgM_Icon_ARROW_UP_e ||
                    iconNum == fopMsgM_Icon_ARROW_DOWN_e ||
                    iconNum == fopMsgM_Icon_SELECT_YOKO_RIGHT_e ||
                    iconNum == fopMsgM_Icon_SELECT_YOKO_RIGHT_e // !@bug: The same value is checked twice here
                ) && iconColor == 0
            ) {
                iconColor = 0xFFFFFFFF;
            }
            if (
                iconNum != fopMsgM_Icon_NONE_e &&
                buttonTimer[i] == -1 &&
                iconNum != fopMsgM_Icon_SELECT_YOKO_LEFT_e &&
                iconNum != fopMsgM_Icon_SELECT_YOKO_RIGHT_e &&
                iconNum != fopMsgM_Icon_INPUT_e
            ) {
                fopMsgM_outFontSet(button_icon[i], button_kage[i], &buttonTimer[i], iconColor, iconNum);
            }
        }
        if (i_Msg->mStatus == 7) {
            i_Msg->m1100 = 0;
        } else if (i_Msg->mStatus == 10 && i_Msg->mMsgDataProc.autoSendFlag == 0) {
            i_Msg->m1100 = 0;
        }
    }
    dMsg_setString(i_Msg);
    return TRUE;
}

/* 802143A8-80214560       .text dMsg_Draw__FP13sub_msg_class */
static BOOL dMsg_Draw(sub_msg_class* i_Msg) {
    if (i_Msg->mMesgEntry.mTextboxType != 5) {
        fopMsgM_setAlpha(&i_Msg->m049C);
        if (i_Msg->mMesgEntry.mTextboxType != 0xe) {
            fopMsgM_setAlpha(&i_Msg->mPane_Arrow);
            fopMsgM_setAlpha(&i_Msg->m050C);
        }
    }
    fopMsgM_setAlpha(&i_Msg->m0544[0]);
    fopMsgM_setAlpha(&i_Msg->m0544[1]);
    fopMsgM_setAlpha(&i_Msg->m0544[2]);
    fopMsgM_setAlpha(&i_Msg->m0544[3]);
    if ((i_Msg->mMesgEntry.mTextboxType == 9) || (i_Msg->mMesgEntry.mTextboxType == 1)) {
        for (s32 i = 0; i < 10; i++) {
            fopMsgM_setAlpha(&i_Msg->m0624[i]);
        }
    } else if (i_Msg->mMesgEntry.mTextboxType == 0xe) {
        if ((i_Msg->mMsgNo == 0x5b3) || (i_Msg->mMsgNo == 0x5b4)) {
            for (s32 i = 0; i < mBeatNum[dComIfGp_getMelodyNum()]; i++) {
#if VERSION == VERSION_PAL
                fopMsgM_paneTrans(&i_Msg->m08C4[i], 0.0f, g_msgHIO.field_0x64 + 6);
                fopMsgM_paneTrans(&i_Msg->m0A14[i], 0.0f, g_msgHIO.field_0x64 + 6);
                fopMsgM_paneTrans(&i_Msg->m0B64[i], 0.0f, g_msgHIO.field_0x64 + 6);
#endif
                fopMsgM_setAlpha(&i_Msg->m08C4[i]);
                fopMsgM_setAlpha(&i_Msg->m0A14[i]);
                fopMsgM_setAlpha(&i_Msg->m0B64[i]);
                if (dComIfGp_getMelodyNum() >= 5) {
                    fopMsgM_setAlpha(&i_Msg->m0CB4[i]);
#if VERSION == VERSION_PAL
                    fopMsgM_paneTrans(&i_Msg->m0CB4[i], 0.0f, g_msgHIO.field_0x64 + 6);
#endif
                }
            }
        }
    } else if (i_Msg->mMesgEntry.mTextboxType != 5) {
        for (s32 i = 0; i < 10; i++) {
            fopMsgM_setAlpha(&i_Msg->m026C[i]);
        }
        fopMsgM_setAlpha(&i_Msg->m01FC);
        fopMsgM_setAlpha(&i_Msg->m0234);
        fopMsgM_setAlpha(&i_Msg->m011C[0]);
        fopMsgM_setAlpha(&i_Msg->m011C[1]);
        fopMsgM_setAlpha(&i_Msg->m011C[2]);
        fopMsgM_setAlpha(&i_Msg->m011C[3]);
    }
    dComIfGd_set2DOpa(&message);
#if VERSION == VERSION_DEMO
    if (g_msgHIO.field_0x86) {
        J2DDrawLine(0.0f, g_msgHIO.field_0x6e, 640.0f, g_msgHIO.field_0x6e, JUtility::TColor(0xFFFFFFFF), 12);
        J2DFillBox((int)talkPosX, (int)talkPosY, 3.0f, 3.0f, JUtility::TColor(0xFF0000FF));
        JUTReport(480, 340, "x : %f", i_Msg->mPos.x);
        JUTReport(480, 360, "y : %f", i_Msg->mPos.y);
        JUTReport(480, 380, "z : %f", i_Msg->mPos.z);
    }
#endif
    return TRUE;
}

/* 80214560-802153B0       .text dMsg_Execute__FP13sub_msg_class */
static BOOL dMsg_Execute(sub_msg_class* i_Msg) {
    u8 bVar1;
    s32 fVar2;
    s32 fVar3;
    s32 fVar4;
    GXColor GVar7;
    JKRHeap* pJVar8;
    MyScreen* pJVar12;
    f32 dVar15;

    pJVar8 = mDoExt_setCurrentHeap(i_Msg->mpHeap);
    if (dComIfGp_checkMesgSendButton()) {
        dComIfGp_setMesgSendButton(0);
    }
    if (dComIfGp_checkMesgBgm() && (i_Msg->mMesgEntry.mTextboxType != 9 ||
                                    (mDoAud_checkPlayingSubBgmFlag() != JA_BGM_ITEM_GET && mDoAud_checkPlayingSubBgmFlag() != JA_BGM_ITEM_GET_S &&
                                     mDoAud_checkPlayingSubBgmFlag() != JA_BGM_GET_HEART && mDoAud_checkPlayingSubBgmFlag() != JA_BGM_GET_SONG &&
                                     mDoAud_checkPlayingSubBgmFlag() != JA_BGM_MASTER_SWORD && mDoAud_checkPlayingSubBgmFlag() != JA_BGM_GET_PEARL)))
    {
        dComIfGp_setMesgBgmOff();
    }
    if ((i_Msg->m026C[0].mUserArea == 3) || ((i_Msg->mStatus != 8 && (i_Msg->mStatus != 9)) && (i_Msg->m026C[0].mUserArea != 0))) {
        if ((int)i_Msg->m026C[9].mUserArea < (int)g_msgHIO.field_0x48 + (int)g_msgHIO.field_0x76) {
            dMsg_subTextClose(i_Msg);
        } else {
            i_Msg->m026C[9].mUserArea = 0;
            i_Msg->m026C[0].mUserArea = 0;
        }
    }
    if (i_Msg->mStatus == fopMsgStts_MSG_PREPARING_e) {
        dMsg_initProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_MSG_TYPING_e) {
        dMsg_outnowProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_STOP_e) {
        dMsg_stopProc(i_Msg);
    } else if (((i_Msg->mStatus == fopMsgStts_SELECT_2_e) || (i_Msg->mStatus == fopMsgStts_SELECT_3_e)) || (i_Msg->mStatus == fopMsgStts_SELECT_YOKO_e)) {
        dMsg_selectProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_INPUT_e) {
        dMsg_inputProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_DEMO_e) {
        dMsg_demoProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_CLOSE_WAIT_e) {
        dMsg_closewaitProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_BOX_OPENING_e) {
        switch (i_Msg->mMesgEntry.mTextboxType) {
        case 5:
            dMsg_openDemoProc(i_Msg);
            break;
        case 1:
        case 9:
            dMsg_openItemProc(i_Msg);
            break;
        case 0xe:
            dMsg_openTactProc(i_Msg);
            break;
        default:
            dMsg_openTalkProc(i_Msg);
            break;
        }
    } else if (i_Msg->mStatus == fopMsgStts_MSG_CONTINUES_e) {
        dMsg_continueProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_MSG_ENDS_e) {
        dMsg_finishProc(i_Msg);
    } else if (i_Msg->mStatus == fopMsgStts_BOX_CLOSING_e) {
        if (i_Msg->mMesgEntry.mTextboxType == 5) {
            dMsg_closeProc2(i_Msg);
        } else {
            dMsg_closeProc(i_Msg);
        }
    } else if (i_Msg->mStatus == fopMsgStts_MSG_DESTROYED_e) {
        fopMsgM_Delete(i_Msg);
        dComIfGp_clearMesgAnimeTagInfo();
        dComIfGp_clearMesgCameraTagInfo();
        dComIfGp_setMesgAnimeAttrInfo(0);
        dComIfGp_setMesgCameraAttrInfo(0);
    } else if (i_Msg->mStatus == fopMsgStts_TACT_e) {
        dMsg_tactProc(i_Msg);
    }
    bVar1 = i_Msg->mMesgEntry.mTextboxType;
    if ((bVar1 == 9) || (bVar1 == 1)) {
        if (i_Msg->m0624[8].mNowAlpha == i_Msg->m0624[8].mInitAlpha) {
            dMsg_ringMove(i_Msg);
            dMsg_lightMove(i_Msg);
            dMsg_cornerMove(i_Msg);
        }
        for (s32 i = 0; i < 2; i++) {
            if (i_Msg->m10B4[i] != NULL) {
                i_Msg->m10B4[i]->playDrawParticle();
                i_Msg->m10B4[i]->setGlobalAlpha(i_Msg->m0624[8].mNowAlpha);
            }
        }
    } else if (bVar1 == 0xe) {
        dMsg_cornerMove(i_Msg);
    }
#if VERSION == VERSION_DEMO
    if (g_msgDHIO.mIsUpdate) {
        JUTReport(480, 290, "Now:%08d", i_Msg->mMsgID);
    }
#endif
    dMsg_rubySet(i_Msg);
    f32 fvar2_2;
    if (dComIfGs_getOptRuby()) {
        fvar2_2 = -4.0f;
    } else {
        fvar2_2 = 0.0f;
    }
    dVar15 = fvar2_2;
    fopMsgM_messageSendOff();
    GVar7 = g_msgHIO.field_0x5;
    bVar1 = i_Msg->mMesgEntry.mTextboxType;
    if (bVar1 == 5) {
        static s32 posY0 = (i_Msg->m0544[0].pane)->getBounds().i.y;
        static s32 posY1 = (i_Msg->m0544[1].pane)->getBounds().i.y;
        static s32 posY2 = (i_Msg->m0544[2].pane)->getBounds().i.y;
        static s32 posY3 = (i_Msg->m0544[3].pane)->getBounds().i.y;
        pJVar12 = (MyScreen*)i_Msg->m0544[0].pane;
        s32 fVar1 = pJVar12->getBounds().i.x;
        fVar2 = i_Msg->m0544[1].pane->getBounds().i.x;
        fVar3 = i_Msg->m0544[2].pane->getBounds().i.x;
        fVar4 = i_Msg->m0544[3].pane->getBounds().i.x;
        pJVar12->move(fVar1, posY0);
        i_Msg->m0544[1].pane->move(fVar2, posY1);
        i_Msg->m0544[2].pane->move(fVar3, posY2);
        i_Msg->m0544[3].pane->move(fVar4, posY3);
    } else if ((bVar1 == 9) || (bVar1 == 1)) {
        i_Msg->m049C.pane->move(i_Msg->m049C.mPosTopLeft.x, (i_Msg->m049C.mPosTopLeft.y + (int)g_msgHIO.field_0x66));
        i_Msg->m050C.pane->move(i_Msg->m050C.mPosTopLeft.x, (i_Msg->m050C.mPosTopLeft.y + g_msgHIO.field_0x66));
        i_Msg->m0544[0].pane->move(i_Msg->m10F8, (g_msgHIO.field_0x66 + (dVar15 + ((i_Msg->m10FC - (int)g_msgHIO.field_0x62) - (int)textOffsetY))));
        i_Msg->m0544[1].pane->move(i_Msg->m10F8, (g_msgHIO.field_0x66 + (dVar15 + ((i_Msg->m10FC - (int)g_msgHIO.field_0x60) - (int)textOffsetY))));
        i_Msg->m0544[2].pane->move(
            i_Msg->m10F8 + 2, ((int)g_msgHIO.field_0x66 + (dVar15 + (((i_Msg->m10FC - (int)g_msgHIO.field_0x62) + 2) - (int)textOffsetY)))
        );
        i_Msg->m0544[3].pane->move(
            i_Msg->m10F8 + 2, ((int)g_msgHIO.field_0x66 + (dVar15 + (((i_Msg->m10FC - (int)g_msgHIO.field_0x60) + 2) - (int)textOffsetY)))
        );
    } else if (bVar1 == 0xe) {
        i_Msg->m049C.pane->move(i_Msg->m049C.mPosTopLeft.x, (i_Msg->m049C.mPosTopLeft.y + (int)g_msgHIO.field_0x66));
        i_Msg->m0544[0].pane->move(i_Msg->m10F8, ((int)g_msgHIO.field_0x66 + (dVar15 + ((i_Msg->m10FC - (int)g_msgHIO.field_0x62) - (int)textOffsetY))));
        i_Msg->m0544[1].pane->move(i_Msg->m10F8, ((int)g_msgHIO.field_0x66 + (dVar15 + ((i_Msg->m10FC - (int)g_msgHIO.field_0x60) - (int)textOffsetY))));
        i_Msg->m0544[2].pane->move(
            i_Msg->m10F8 + 2, ((int)g_msgHIO.field_0x66 + (dVar15 + (i_Msg->m10FC - ((int)g_msgHIO.field_0x62) + 2 - (int)textOffsetY)))
        );
        i_Msg->m0544[3].pane->move(
            i_Msg->m10F8 + 2, ((int)g_msgHIO.field_0x66 + (dVar15 + (((i_Msg->m10FC - (int)g_msgHIO.field_0x60) + 2) - (int)textOffsetY)))
        );
    } else {
        ((J2DPicture*)i_Msg->m049C.pane)->setWhite(JUtility::TColor(g_msgHIO.field_0x5));
        ((J2DPicture*)i_Msg->m049C.pane)->setBlack(JUtility::TColor(g_msgHIO.field_0x9));
        i_Msg->m049C.pane->move(i_Msg->m049C.mPosTopLeft.x, (i_Msg->m049C.mPosTopLeft.y + (int)g_msgHIO.field_0x66));
        i_Msg->m050C.pane->move(i_Msg->m050C.mPosTopLeft.x, (i_Msg->m050C.mPosTopLeft.y + (int)g_msgHIO.field_0x66));
        i_Msg->m0544[0].pane->move(i_Msg->m10F8, ((int)g_msgHIO.field_0x66 + (dVar15 + ((i_Msg->m10FC - (int)g_msgHIO.field_0x62) - (int)textOffsetY))));
        i_Msg->m0544[1].pane->move(i_Msg->m10F8, ((int)g_msgHIO.field_0x66 + (dVar15 + ((i_Msg->m10FC - (int)g_msgHIO.field_0x60) - (int)textOffsetY))));
        i_Msg->m0544[2].pane->move(
            i_Msg->m10F8 + 2, ((int)g_msgHIO.field_0x66 + (dVar15 + (((i_Msg->m10FC - (int)g_msgHIO.field_0x62) + 2) - (int)textOffsetY)))
        );
        i_Msg->m0544[3].pane->move(
            i_Msg->m10F8 + 2, ((int)g_msgHIO.field_0x66 + (dVar15 + (((i_Msg->m10FC - (int)g_msgHIO.field_0x60) + 2) - (int)textOffsetY)))
        );
    }
    dComIfGp_setMesgStatus(i_Msg->mStatus);
    mDoExt_setCurrentHeap(pJVar8);
    return TRUE;
}

/* 802153B0-802153B8       .text dMsg_IsDelete__FP13sub_msg_class */
static BOOL dMsg_IsDelete(sub_msg_class*) {
    return TRUE;
}

/* 802153B8-80215698       .text dMsg_Delete__FP13sub_msg_class */
static BOOL dMsg_Delete(sub_msg_class* i_Msg) {
    JKRHeap* pJVar1;

    if (fopMsgM_demoMsgFlagCheck()) {
        fopMsgM_demoMsgFlagOff();
    }
    if (fopMsgM_tactMsgFlagCheck()) {
        fopMsgM_tactMsgFlagOff();
    }
    if (fopMsgM_nextMsgFlagCheck()) {
        fopMsgM_nextMsgFlagOff();
    }
    i_Msg->m116A = 0;
    dComIfGp_setMesgSendButton(i_Msg->m116A);
    dComIfGp_setMesgCancelButton(0);
    dComIfGp_setMesgStatus(0);
    if (dComIfGp_checkMesgBgm()) {
        dComIfGp_setMesgBgmOff();
    }
    pJVar1 = mDoExt_setCurrentHeap(i_Msg->mpHeap);
    for (s32 i = 0; i < 2; i++) {
        if (i_Msg->m10B4[i] != NULL) {
            i_Msg->m10B4[i]->becomeInvalidEmitter();
            i_Msg->m10B4[i]->quitImmortalEmitter();
        }
    }
    delete (MyScreen*)sScreen;
    if (sScreen2 != NULL) {
        delete (J2DScreen*)sScreen2;
    }
#if VERSION >= VERSION_USA
    if (dMsg_font_flag != 0) {
        mDoExt_removeRubyFont();
    } else {
        mDoExt_removeMesgFont();
    }
#else
    mDoExt_removeMesgFont();
    mDoExt_removeRubyFont();
#endif
    for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
        delete (J2DPicture*)button_icon[i];
        delete (J2DPicture*)button_kage[i];
    }
    delete (J2DPicture*)arrowPane;
    delete (J2DPicture*)maskPane;
    for (s32 i = 0; i < 3; i++) {
        delete (J2DTextBox*)numberPane[i];
    }
    i_Msg->mpHeap->free(i_Msg->output_text);
    i_Msg->mpHeap->free(i_Msg->output_rub);
    i_Msg->mpHeap->free(i_Msg->output_textSdw);
    i_Msg->mpHeap->free(i_Msg->output_rubSdw);
    i_Msg->mpHeap->free(i_Msg->select_text);
    i_Msg->mpHeap->free(i_Msg->select_rub);
    i_Msg->mpHeap->free(i_Msg->select_textSdw);
    i_Msg->mpHeap->free(i_Msg->select_rubSdw);
    agb_work_area = 0;
    if (i_Msg->buffer_p != NULL) {
        i_Msg->mpHeap->free(i_Msg->buffer_p);
    }
    dComIfGp_getMsgArchive()->removeResourceAll();
    i_Msg->mpHeap->freeAll();
    dComIfGp_offHeapLockFlag();
    mDoExt_setCurrentHeap(pJVar1);
    return TRUE;
}

/* 80215698-80215CD0       .text dMsg_Create__FP9msg_class */
static cPhs_State dMsg_Create(msg_class* i_this) {
    sub_msg_class* i_Msg = (sub_msg_class*)i_this;
    JKRHeap* pJVar2;

    if ((dComIfGp_isHeapLockFlag() != 0) && (dComIfGp_isHeapLockFlag() != 7)) {
        return cPhs_INIT_e;
    }
    i_Msg->mpHeap = dComIfGp_getExpHeap2D();
    dComIfGp_setHeapLockFlag(7);
    pJVar2 = mDoExt_setCurrentHeap(i_Msg->mpHeap);
    if ((i_this->mMsgNo >= 0xc6) && (i_this->mMsgNo <= 0xcd)) {
#if VERSION == VERSION_DEMO
        u32 msgNo = dComIfGs_getTriforceNum() + 0xC6;
        i_Msg->head_p = i_Msg->mMsgGet.getMesgHeader(msgNo);
#else
        i_Msg->head_p = i_Msg->mMsgGet.getMesgHeader(dComIfGs_getTriforceNum() + 0xC6);
#endif
    } else {
        i_Msg->head_p = i_Msg->mMsgGet.getMesgHeader(i_this->mMsgNo);
    }
    JUT_ASSERT(VERSION_SELECT(5304, 5377, 5416, 5450), i_Msg->head_p);
    i_Msg->mpMesgStr = (char*)i_Msg->mMsgGet.getMessage(i_Msg->head_p);
    i_Msg->mMesgEntry = i_Msg->mMsgGet.getMesgEntry(i_Msg->head_p);
    i_Msg->mMsgID = i_Msg->mMsgGet.mGroupID << 8 | i_Msg->mMsgGet.mResMsgNo;
    i_Msg->mMesgCameraTagInfo = dComIfGp_getMesgCameraTagInfo();
    dMsg_screenDataSet(i_Msg);
    for (s32 i = 0; i < dMsg_OUTFONT_MAX; i++) {
        button_icon[i] = new J2DPicture("font_07_02.bti");
        button_kage[i] = new J2DPicture("font_07_02.bti");
        fopMsgM_blendInit(button_icon[i], "font_00.bti");
        fopMsgM_blendInit(button_kage[i], "font_00.bti");
        button_icon[i]->hide();
        button_kage[i]->hide();
        button_icon[i]->setAlpha(0);
        button_kage[i]->setAlpha(0);
        buttonTimer[i] = -1;
    }
    arrowPane = new J2DPicture("font_10.bti");
    i_Msg->output_text = (char*)i_Msg->mpHeap->alloc(0x385, 4);
    JUT_ASSERT(VERSION_SELECT(5332, 5405, 5444, 5481), i_Msg->output_text != NULL);
    i_Msg->output_rub = (char*)i_Msg->mpHeap->alloc(0x385, 4);
    JUT_ASSERT(VERSION_SELECT(5335, 5408, 5447, 5484), i_Msg->output_rub != NULL);
    i_Msg->output_textSdw = (char*)i_Msg->mpHeap->alloc(0x385, 4);
    JUT_ASSERT(VERSION_SELECT(5338, 5411, 5450, 5487), i_Msg->output_textSdw != NULL);
    i_Msg->output_rubSdw = (char*)i_Msg->mpHeap->alloc(0x385, 4);
    JUT_ASSERT(VERSION_SELECT(5341, 5414, 5453, 5490), i_Msg->output_rubSdw != NULL);
    i_Msg->select_text = (char*)i_Msg->mpHeap->alloc(0x65, 4);
    JUT_ASSERT(VERSION_SELECT(5344, 5417, 5456, 5493), i_Msg->select_text != NULL);
    i_Msg->select_rub = (char*)i_Msg->mpHeap->alloc(0x65, 4);
    JUT_ASSERT(VERSION_SELECT(5347, 5420, 5459, 5496), i_Msg->select_rub != NULL);
    i_Msg->select_textSdw = (char*)i_Msg->mpHeap->alloc(0x65, 4);
    JUT_ASSERT(VERSION_SELECT(5350, 5423, 5462, 5499), i_Msg->select_textSdw != NULL);
    i_Msg->select_rubSdw = (char*)i_Msg->mpHeap->alloc(0x65, 4);
    JUT_ASSERT(VERSION_SELECT(5353, 5426, 5465, 5502), i_Msg->select_rubSdw != NULL);
    agb_work_area = i_Msg->mpHeap;
    JUT_ASSERT(VERSION_SELECT(5356, 5429, 5468, 5505), agb_work_area != NULL);
    i_this->mStatus = fopMsgStts_MSG_PREPARING_e;
    i_Msg->m1164 = -1;
    dMsg_value_init(i_Msg);
    dMsg_setString(i_Msg);
    i_Msg->m116A = 0;
    dComIfGp_setMesgSendButton(i_Msg->m116A);
    dComIfGp_setMesgCancelButton(0);
    mDoExt_setCurrentHeap(pJVar2);
    message.setActorP(i_Msg);
    dComIfGp_setMesgStatus(i_this->mStatus);
    return cPhs_COMPLEATE_e;
}

static msg_method_class l_dMsg_Method = {
    (process_method_func)dMsg_Create,
    (process_method_func)dMsg_Delete,
    (process_method_func)dMsg_Execute,
    (process_method_func)dMsg_IsDelete,
    (process_method_func)dMsg_Draw
};

msg_process_profile_definition g_profile_MSG = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 12,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_MSG_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(sub_msg_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopMsg_Method,
    /* Draw Prio    */ fpcDwPi_MSG_e,
    /* Msg SubMtd   */ &l_dMsg_Method,
};
