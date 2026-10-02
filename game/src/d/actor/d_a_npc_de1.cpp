/**
* d_a_npc_de1.cpp
* NPC - Great Deku Tree
*/
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_de1.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_leaflift.h"
#include "d/actor/d_a_cc.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_audio.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/De.h"
#include <string.h>
static daNpc_De1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[100];
static int l_check_wrk;
daNpc_De1_HIO_c::daNpc_De1_HIO_c() {
    static hio_prm_c a_prm_tbl = {100.0f, 150.0f, 1.0f, 800.0f, {44.0f, 100.0f, 976.0f}, 0};
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    mChild = -1;
    mCount = -1;
}
daNpc_De1_HIO_c::~daNpc_De1_HIO_c() {}

static void* searchActor_leafLift(void* actor, void*) {
    if (l_check_wrk < 100 && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_LEAF_LIFT_e) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

bool daNpc_De1_c::createInit() {
    mEventCut.setActorInfo2("De1", this);
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 21;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 22;
    gravity = 0.0f;
    mHomePos = current.pos;
    switch (mSubType) {
    case 0:
        if (dComIfGs_isEventBit(0x1801)) {
            set_action(&daNpc_De1_c::wait_action2, NULL);
        } else {
            set_action(&daNpc_De1_c::wait_action1, NULL);
        }
        break;
    case 1:
        set_action(&daNpc_De1_c::wait_action2, NULL);
        break;
    }
    shape_angle = current.angle;
    mpMorf->setMorf(0.0f);
    m7AC = 1;
    setMtx();
    return true;
}

void daNpc_De1_c::setMtx() {
    if (!mDemo) {
        mAnmEnd = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mFrame) {
            mAnmEnd = 1;
        }
        mFrame = mpMorf->getFrame();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    if (cLib_calcTimer(&mSoundTimer) == 0 && mAnmEnd && mAnmNo == 3) {
        mDoAud_seStart(JA_SE_CV_DEKU_AGONY, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mDoMtx_stack_c::scaleM(l_HIO.mPrm.mScale, l_HIO.mPrm.mScale, l_HIO.mPrm.mScale);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpBgW->CopyBackVtx();
    mpMorf->calc();
    mpBgW->MoveAfterAnmCalc(mpMorf->getModel());
    followPa_happa();
    setDemoStartCenter();
    if (mSubType == 0 && !dComIfGs_isEventBit(0x1801)) {
        cc_set();
    }
    setAttention();
}

bool daNpc_De1_c::anmResID(int i_num, int* o_bck_num, int* o_bas_num) {
    static const int a_anm_idx_tbl[][2] = {
        {dRes_ID_DE_BCK_WAIT01_e, -1}, {dRes_ID_DE_BCK_SLEEP_e, -1},
        {dRes_ID_DE_BCK_WAKE_e, -1}, {dRes_ID_DE_BCK_SUFFER_e, -1},
        {dRes_ID_DE_BCK_TALK01_e, -1}, {dRes_ID_DE_BCK_TALK02_e, -1},
        {dRes_ID_DE_BCK_TALK03_e, -1},
    };
    JUT_ASSERT(0x11C, 0 <= i_num && i_num < ANM_END);
    JUT_ASSERT(0x11D, o_bck_num && o_bas_num);
    *o_bck_num = a_anm_idx_tbl[i_num][0];
    *o_bas_num = a_anm_idx_tbl[i_num][1];
    return true;
}

bool daNpc_De1_c::setAnm_anm(anm_prm_c* prm) {
    bool result = false;
    if (prm->mAnm >= 0 && mAnmNo != prm->mAnm) {
        mAnmNo = prm->mAnm;
        if (mpMorf != NULL) {
            int bck, bas;
            anmResID(mAnmNo, &bck, &bas);
            if (bck >= 0) {
                dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bck, bas, "De");
            }
            if (mAnmNo == 3) {
                set_pa_happa();
            } else {
                del_pa_happa();
            }
            result = true;
        }
        mFrame = 0.0f;
        mAnmLoops = 0;
        mAnmEnd = 0;
    }
    return result;
}

void daNpc_De1_c::setAnm_NUM(int num) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 8.0f, 1.0f, 2}, {1, 8.0f, 1.0f, 2}, {2, 8.0f, 1.0f, 0}, {3, 8.0f, 1.0f, 2},
        {4, 8.0f, 1.0f, 2}, {5, 8.0f, 1.0f, 2}, {6, 8.0f, 1.0f, 2},
    };
    setAnm_anm(&a_anm_prm_tbl[num]);
}

void daNpc_De1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, 0.0f, 0.0f, -1}, {0, 8.0f, 1.0f, 2}, {-1, 0.0f, 0.0f, -1}, {0, 8.0f, 1.0f, 2},
        {-1, 0.0f, 0.0f, -1}, {0, 8.0f, 1.0f, 2}, {1, 8.0f, 1.0f, 2}, {-1, 0.0f, 0.0f, -1},
    };
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

void daNpc_De1_c::chngAnmTag() {
    switch (mAnmTag) {
    case 0:
        break;
    }
}

void daNpc_De1_c::ctrlAnmTag() {
    switch (mAnmAttr) {
    case 0:
        break;
    }
}

void daNpc_De1_c::chngAnmAtr(u8 attr) {
    if (attr < 4 && attr != mAnmAttr) {
        mAnmAttr = attr;
        setAnm_ATR();
    }
}

void daNpc_De1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 0:
    case 1:
    case 2:
        break;
    case 3:
        if (mAnmEnd) {
            mAnmAttr = 0;
            setAnm_ATR();
        }
        break;
    }
}

void daNpc_De1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 8.0f, 1.0f, 2}, {4, 8.0f, 1.0f, 2}, {5, 8.0f, 1.0f, 2}, {6, 8.0f, 1.0f, 2},
    };
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

void daNpc_De1_c::anmAtr(u16 status) {
    if (status == 6) {
        if (!mMsgAnmStarted) {
            mAnmTag = 0xFF;
            chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        u8 tag = dComIfGp_getMesgAnimeTagInfo();
        if (tag != 0xFF && tag != mAnmTag) {
            dComIfGp_clearMesgAnimeTagInfo();
            mAnmTag = tag;
            chngAnmTag();
        }
    } else if (status == 14) {
        mMsgAnmStarted = 0;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

void daNpc_De1_c::setStt(s8 state) {
    s8 previous = mState;
    mState = state;
    switch (mState) {
    case 2:
    case 4:
        mAnmAttr = 0xFF;
        mPreviousState = previous;
        break;
    case 7:
        attention_info.flags |= 0x4000000;
        attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 34;
        // Fall through.
    case 0:
    case 1:
    case 3:
    case 5:
    case 6:
    default:
        setAnm();
        break;
    }
}

u16 daNpc_De1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x139D:
        *msg = 0x139E;
        break;
    case 0x139F:
        *msg = 0x13A0;
        break;
    case 0x13A1:
        *msg = 0x13A2;
        break;
    case 0x13A3:
        *msg = 0x13A4;
        break;
    case 0x13A4:
        *msg = 0x13A5;
        break;
    case 0x13A5:
        *msg = 0x13A6;
        break;
    case 0x13A8:
        *msg = 0x13A9;
        break;
    case 0x13AC:
        *msg = 0x13A9;
        break;
    case 0x13A9:
    case 0x13C5:
        if (dComIfGs_isEventBit(0x1D40)) {
            *msg = 0x13AA;
        } else {
            *msg = 0x13AB;
        }
        break;
    case 0x13AA:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *msg = 0x13AD;
            break;
        case 1:
            *msg = 0x13C7;
            break;
        default:
            goto end_selection;
        }
        break;
    case 0x13AB:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *msg = 0x13AD;
            break;
        case 1:
            *msg = 0x13CC;
            break;
        default:
            goto end_selection;
        }
        break;
    case 0x13CC:
        *msg = 0x13CD;
        break;
    case 0x13CD:
        *msg = 0x13CE;
        break;
    case 0x13CE:
        *msg = 0x13CF;
        break;
    case 0x13CF:
        *msg = 0x13D0;
        break;
    case 0x13D0:
        *msg = 0x13D1;
        break;
    case 0x13C7:
        *msg = 0x13C8;
        break;
    case 0x13C8:
        *msg = 0x13C9;
        break;
    case 0x13C9:
        *msg = 0x13CA;
        break;
    case 0x13CA:
        *msg = 0x13CB;
        break;
    case 0x13AD:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            if (dComIfGs_isEventBit(0x102)) {
                *msg = 0x13B0;
            } else if (dComIfGs_isEventBit(0x3940)) {
                *msg = 0x13AF;
            } else {
                dComIfGs_onEventBit(0x3940);
                *msg = 0x13AE;
            }
            break;
        case 1:
            *msg = 0x13B6;
            break;
        default:
            goto end_selection;
        }
        break;
    case 0x13B6:
        if (dComIfGs_isEventBit(0x1820)) {
            if (dComIfGs_isStageBossEnemy(6)) {
                *msg = 0x13C2;
            } else {
                *msg = 0x13C4;
            }
        } else *msg = 0x13C0;
        break;
    case 0x13C0:
        *msg = 0x13C1;
        break;
    case 0x13C2:
        *msg = 0x13C3;
        break;
    case 0x13AE:
    case 0x13AF:
    case 0x13B0:
    case 0x13C1:
    case 0x13C3:
    case 0x13C4:
        *msg = 0x13C5;
        break;
        end_selection:
        *msg = 0x13C6;
        break;
    default: status = 16;
        break;
    }
    return status;
}
u32 daNpc_De1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0:
        if (dComIfGs_checkGetItem(dItemNo_TELESCOPE_e)) {
            msg = dLib_setFirstMsg(0xE40, 0x139F, 0x13A1);
        } else {
            msg = dLib_setFirstMsg(0xE80, 0x139C, 0x139D);
        }
        break;
    case 1:
        if (dComIfGs_isEventBit(0x1C40)) {
            if (mMet) {
                msg = 0x13AC;
            } else {
                mMet = 1;
                msg = 0x13A8;
            }
        } else msg = dLib_setFirstMsg(0x1C20, 0x13A3, 0x13A7);
        break;
    }
    return msg;
}

void daNpc_De1_c::eventOrder() {
    static char* a_demo_name_tbl[] = {"LIFT_UP", "DE_CHUCHU", "contact"};
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (mOrder >= 3) {
        if (mOrder == 3) {
            fopAcM_orderOtherEvent2(this, a_demo_name_tbl[mOrder - 3], 1, 15);
        } else {
            fopAcM_orderOtherEvent2(this, a_demo_name_tbl[mOrder - 3], 1, 0xFFFF);
        }
    }
}

void daNpc_De1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck("LIFT_UP") && mOrder == 3) {
            mOrder = 0;
        } else if (dComIfGp_evmng_startCheck("DE_CHUCHU") && mOrder == 4) {
            mDoAud_bgmStart(JA_BGM_DEKU_PINCH);
            mOrder = 0;
        } else if (dComIfGp_evmng_startCheck("contact") && mOrder == 5) {
            dComIfGs_onEventBit(0x1801);
            dComIfGp_setNextStage("Omori", 213, 0, 8, 0.0f, 0, 1, 0);
            mOrder = 0;
        }
    } else if (eventInfo.checkCommandTalk() && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0;
        mTalking = 1;
    }
}

bool daNpc_De1_c::chkAttention() {
    fopAc_ac_c* lift = searchByID(mLiftID);
    bool result = false;
    if (lift != NULL) {
        f32 radius = l_HIO.mPrm.mAttentionRadius;
        if (mAttention) {
            radius += 50.0f;
        }
        result = radius >= (dComIfGp_getPlayer(0)->current.pos - lift->current.pos).absXZ();
    }
    return result;
}

void daNpc_De1_c::setAttention() {
    cXyz offset(0.0f, 1100.0f, 700.0f);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mDoMtx_stack_c::multVec(&offset, &mEyePos);
    mHeadPos = mEyePos;
    attention_info.position.set(mHeadPos.x, mHeadPos.y + l_HIO.mPrm.mAttentionOffset, mHeadPos.z);
    eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
}
fopAc_ac_c* daNpc_De1_c::searchByID(fpc_ProcID id) {
    fopAc_ac_c* actor = NULL;
    fopAcM_SearchByID(id, &actor);
    return actor;
}

void daNpc_De1_c::setDemoStartCenter() {
    cXyz offset(l_HIO.mPrm.mDemoOffset.x, l_HIO.mPrm.mDemoOffset.y, l_HIO.mPrm.mDemoOffset.z);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mDoMtx_stack_c::multVec(&offset, &mDemoCenter);
}

bool daNpc_De1_c::partner_srch() {
    bool found = false;
    mLiftID = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < 100; i++) {
        l_check_inf[i] = NULL;
    }
    fpcEx_Search(searchActor_leafLift, this);
    if (l_check_wrk != 0) {
        mLiftID = fopAcM_GetID(l_check_inf[0]);
        found = true;
    }
    return found;
}

void daNpc_De1_c::ccCreate() {
    u32 params[] = {2, 0x102, 2, 0x102, 2, 0x102, 2, 0x102, 2, 0x102};
    for (int i = 0; i < 10; i++) {
        m_cc_ID[i] = fopAcM_create(fpcNm_CC_e, params[i], &current.pos, fopAcM_GetRoomNo(this), NULL, NULL, -1, NULL);
        JUT_ASSERT(0x373, m_cc_ID[ i] != fpcM_ERROR_PROCESS_ID_e);
    }
}

void daNpc_De1_c::cc_set() {
    cXyz pos(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 10; i++) {
        cc_class* chu = (cc_class*)searchByID(m_cc_ID[i]);
        if (chu != NULL) {
            mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_c0_jnt_num[i]));
            MTXCopy(mDoMtx_stack_c::get(), chu->m7EC);
        }
    }
}

void daNpc_De1_c::set_pa_happa() {
    mLeaves.remove();
#if VERSION == VERSION_DEMO
    mpLeavesEmitter = dComIfGp_particle_set(0x81BA, &mLeavesPos, &current.angle, NULL, 0xFF, &mLeaves, fopAcM_GetRoomNo(this));
#else
    dComIfGp_particle_set(0x81BA, &mLeavesPos, &current.angle, NULL, 0xFF, &mLeaves, fopAcM_GetRoomNo(this));
#endif
}

void daNpc_De1_c::del_pa_happa() {
#if VERSION == VERSION_DEMO
    if (mpLeavesEmitter != NULL) {
        mpLeavesEmitter->becomeInvalidEmitter();
        mpLeavesEmitter = NULL;
    }
#endif
    mLeaves.remove();
}

void daNpc_De1_c::followPa_happa() {
#if VERSION == VERSION_DEMO
    if (mpLeavesEmitter != NULL) {
#else
    if (mLeaves.getEmitter() != NULL) {
#endif
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_branchL_jnt_num));
        mLeavesPos.x = mDoMtx_stack_c::get()[0][3];
        mLeavesPos.y = mDoMtx_stack_c::get()[1][3];
        mLeavesPos.z = mDoMtx_stack_c::get()[2][3];
    }
}

bool daNpc_De1_c::decideType(int) {
    bool result = true;
    mType = -1;
    switch (fopAcM_GetName(this)) {
    case fpcNm_NPC_DE1_e:
        mType = 0;
        if (!dComIfGs_isSymbol(2)) {
            mSubType = 0;
        } else {
            mSubType = 1;
        }
        break;
    default: result = false;
        break;
    }
    return result;
}

void daNpc_De1_c::event_actionInit(int staff) {
    int* act = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    int* number = dComIfGp_evmng_getMyIntegerP(staff, "Num");
    if (act != NULL) {
        daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
        mEventAction = *act;
        switch (mEventAction) {
        case 1:
            if (number != NULL) {
                fopAc_ac_c* chu = searchByID(m_cc_ID[*number]);
                if (chu != NULL) {
                    dComIfGp_event_setItemPartner(chu);
                }
            }
            break;
        case 2:
            if (timer != NULL) {
                mEventTimer = *timer;
            } else {
                mEventTimer = 0;
            }
            if (number != NULL) {
                mChuIndex = *number;
            } else {
                mChuIndex = 0;
            }
            break;
        case 3:
            if (number != NULL) {
                setAnm_NUM(*number);
            }
            break;
        case 4:
            mDoAud_seStart(JA_SE_CV_DEKU_AGONY_L, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            mSoundTimer = 84;
            break;
        case 5:
            player->changeOriginalDemo();
            player->changeDemoMode(24);
            player->voiceStart(29);
            break;
        }
    }
}

bool daNpc_De1_c::event_action() {
    bool done = false;
    switch (mEventAction) {
    case 0: {
            daLlift_c* lift = (daLlift_c*)searchByID(mLiftID);
            if (lift != NULL && lift->MoveUpLift()) {
                done = true;
            }
            break;
        }
    case 2:
        if (cLib_calcTimer(&mEventTimer) == 0) {
            if (mChuIndex >= 0 && mChuIndex < 10) {
                cc_class* chu = (cc_class*)searchByID(m_cc_ID[mChuIndex]);
                if (chu != NULL) {
                    chu->m2FB = 1;
                }
            }
            done = true;
        }
        break;
    case 1:
    case 3:
    case 4:
    case 5: done = true;
        break;
    }
    return done;
}

void daNpc_De1_c::privateCut() {
    static char* cut_name_tbl[] = {"ACTION"};
    int staff = dComIfGp_evmng_getMyStaffId("De1", NULL, 0);
    if (staff == -1) {
        return;
    }
    mCut = dComIfGp_evmng_getMyActIdx(staff, cut_name_tbl, 1, TRUE, FALSE);
    if (mCut == -1) {
        dComIfGp_evmng_cutEnd(staff);
        return;
    }
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch (mCut) {
        case 0:
            event_actionInit(staff);
            break;
        }
    }
    bool done;
    switch (mCut) {
    case 0:
        done = event_action();
        break;
    default: done = true;
        break;
    }
    if (done) {
        dComIfGp_evmng_cutEnd(staff);
    }
}

void daNpc_De1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

void daNpc_De1_c::event_proc() {
    if (dComIfGp_evmng_endCheck("LIFT_UP")) {
        endEvent();
        mOrder = 1;
        setStt(3);
    } else if (dComIfGp_evmng_endCheck("DE_CHUCHU")) {
        dComIfGs_onTmpBit(0x308);
        endEvent();
        setStt(7);
    } else if (dComIfGp_evmng_endCheck("contact")) {
        endEvent();
        set_action(&daNpc_De1_c::wait_action2, NULL);
    } else if (!mEventCut.cutProc()) {
        privateCut();
    }
}

bool daNpc_De1_c::set_action(ActionFunc action, void* arg) {
    if (mAction != action) {
        if (mAction) {
            mActionState = -1;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

int daNpc_De1_c::wait01() {
    if (mTalking) {
        setStt(2);
    } else if (mAttention) {
        mOrder = 2;
    } else {
        mOrder = 0;
    }
    return 1;
}

int daNpc_De1_c::wait02() {
    if (mTalking) {
        setStt(4);
    } else {
        daLlift_c* lift = (daLlift_c*)searchByID(mLiftID);
        if (lift != NULL) {
            const f32& h = daLlift_c::m_height;
            if (lift->current.pos.y < lift->home.pos.y + h) {
                setStt(5);
                return 1;
            }
        }
        if (mAttention) {
            mOrder = 2;
        } else {
            mOrder = 0;
        }
    }
    return 1;
}

int daNpc_De1_c::wait03() {
    daLlift_c* lift = (daLlift_c*)searchByID(mLiftID);
    if (lift != NULL && lift->checkEndDownLift()) {
        setStt(1);
    }
    return 1;
}

int daNpc_De1_c::wait04() {
    mOrder = 0;
    f32 distance = (dComIfGp_getPlayer(0)->current.pos - mDemoCenter).absXZ();
    if (distance < l_HIO.mPrm.mDemoRadius) {
        daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
        mOrder = 4;
        player->changeDemoMoveAngle(cLib_targetAngleY(&player->current.pos, &current.pos));
    }
    return 1;
}

int daNpc_De1_c::wait05() {
    if (mOrder == 5) {
        return 1;
    }
    mOrder = 0;
    int i;
    for (i = 0; i < 10; i++) {
        if (searchByID(m_cc_ID[i]) != NULL) {
            break;
        }
    }
    if (i >= 10) {
        attention_info.flags &= ~0x4000000;
        mOrder = 5;
    }
    return 1;
}

int daNpc_De1_c::talk01() {
    if (mOrder != 3) {
        mItemNo = 0xFF;
        m7B8 = 0;
        mTalking = 0;
        endEvent();
        mOrder = 3;
    }
    return 1;
}

int daNpc_De1_c::talk02() {
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case 2:
        case 6:
            break;
        case 19:
            mItemNo = 0xFF;
            setStt(mPreviousState);
            m7B8 = 0;
            mTalking = 0;
            endEvent();
            break;
        }
    }
    return 1;
}

int daNpc_De1_c::wait_action1(void*) {
    if (mActionState == 0) {
        setStt(6);
        ccCreate();
        mActionState++;
    } else if (mActionState != -1) {
        mAttention = chkAttention();
        switch (mState) {
        case 6:
            wait04();
            break;
        case 7:
            wait05();
            break;
        }
    }
    return 1;
}

int daNpc_De1_c::wait_action2(void*) {
    if (mActionState == 0) {
        setStt(1);
        mActionState++;
    } else if (mActionState != -1) {
        if (mActionState == 1) {
            partner_srch();
            mActionState++;
        }
        mAttention = chkAttention();
        switch (mState) {
        case 1:
            wait01();
            break;
        case 2:
            talk01();
            break;
        case 3:
            wait02();
            break;
        case 4:
            talk02();
            break;
        case 5:
            wait03();
            break;
        }
    }
    return 1;
}

u8 daNpc_De1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) {
            mDemo = 0;
        }
    } else {
        mDemo = 1;
        dComIfGp_demo_getActor(demoActorID);
        dDemo_setDemoData(this, 0x6A, mpMorf, "De");
    }
    return mDemo;
}
BOOL daNpc_De1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor red1 = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue1 = {0x00, 0x00, 0xFF, 0x80};
    GXColor red2 = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue2 = {0x00, 0x00, 0xFF, 0x80};
    GXColor green = {0x00, 0xFF, 0x00, 0x80};
    J3DModel* model = mpMorf->getModel();
    g_env_light.settingTevStruct(1, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    dComIfGd_setListBG();
    mpMorf->entryDL();
    dComIfGd_setList();
    if (l_HIO.mPrm.mDebug) {
        fopAc_ac_c* lift = searchByID(mLiftID);
        if (lift != NULL) {
            cXyz pos = lift->current.pos;
            pos.y = 950.0f;
        }
    }
    dSnap_RegistFig(0xA6, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

BOOL daNpc_De1_c::_execute() {
    if (!mSavedInitial) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mSavedInitial = 1;
    }
    checkOrder();
    if (!demo()) {
        if (dComIfGp_event_runCheck() && !eventInfo.checkCommandTalk()) {
            event_proc();
        } else {
            (this->*mAction)(NULL);
        }
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        shape_angle = current.angle;
    }
    eventOrder();
    setMtx();
    return TRUE;
}

BOOL daNpc_De1_c::_delete() {
    dComIfG_resDeleteDemo(&mPhase, "De");
    dComIfG_Bgsp()->Release(mpBgW);
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) {
#else
    if (heap != NULL && mpMorf != NULL) {
#endif
        mpMorf->stopZelAnime();
    }
    del_pa_happa();
    if (l_HIO.mCount >= 0 && --l_HIO.mCount < 0) {
        mDoHIO_deleteChild(l_HIO.mChild);
    }
    return TRUE;
}
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_De1_c*)actor)->CreateHeap();
}
cPhs_State daNpc_De1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_De1_c);
#endif
    cPhs_State phase = dComIfG_resLoad(&mPhase, "De");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    int prm = fopAcM_GetParam(this) & 0xFF;
    if (!decideType(prm)) {
        return cPhs_ERROR_e;
    }
    if (l_HIO.mCount < 0) {
        l_HIO.mChild = mDoHIO_createChild("デクの木", &l_HIO);
    }
    l_HIO.mCount++;
#if VERSION == VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_De1_c);
#endif
    static u32 a_heap_size_tbl[] = {0x272E0};
    if (fopAcM_entrySolidHeap(this, CheckCreateHeap, a_heap_size_tbl[mType])) {
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        dComIfG_Bgsp()->Regist(mpBgW, this);
    } else return cPhs_ERROR_e;
    if (!createInit()) {
        phase = cPhs_ERROR_e;
    }
    return phase;
}

int daNpc_De1_c::CreateHeap() {
    static char* a_jnt_name_tbl[] = {"c1","c2","c3","c4","c5","c6","c7","c8","c9","c10"};
    J3DModelData* data = (J3DModelData*)dComIfG_getObjectIDRes("De", dRes_ID_DE_BDL_DE_e);
    mpMorf = new mDoExt_McaMorf(data, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectIDRes("De", dRes_ID_DE_BCK_WAIT01_e), 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000002);
    if (mpMorf != NULL) {
        if (mpMorf->getModel() != NULL) {
            m_branchL_jnt_num = data->getJointName()->getIndex("branchL");
            JUT_ASSERT(DEMO_SELECT(0x6C1, 0x6D0), m_branchL_jnt_num >= 0);
            m_head_jnt_num = data->getJointName()->getIndex("head");
            JUT_ASSERT(DEMO_SELECT(0x6C3, 0x6D2), m_head_jnt_num >= 0);
            for (int i = 0; i < 10; i++) {
                m_c0_jnt_num[i] = data->getJointName()->getIndex(a_jnt_name_tbl[i]);
                JUT_ASSERT(DEMO_SELECT(0x6C6, 0x6D5), m_c0_jnt_num[ i] >= 0);
            }
            mpMorf->getModel()->setUserArea(0);
            mpBgW = new dBgWDeform;
            if (mpBgW != NULL && !mpBgW->Set((cBgD_t*)dComIfG_getObjectIDRes("De", dRes_ID_DE_DZB_DE_e), mpMorf->getModel(), 0)) {
                mAcchCir.SetWall(0.0f, 0.0f);
                mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
                mObjAcch.SetWaterNone();
                mObjAcch.SetWallNone();
                mObjAcch.SetRoofNone();
                return TRUE;
            }
        }
        mpMorf = NULL;
    }
    return FALSE;
}

/* 00002E04-00002E24       .text daNpc_De1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_De1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_De1_c*)i_this)->_create();
}

/* 00002E24-00002E44       .text daNpc_De1_Delete__FP11daNpc_De1_c */
static BOOL daNpc_De1_Delete(daNpc_De1_c* i_this) {
    return ((daNpc_De1_c*)i_this)->_delete();
}

/* 00002E44-00002E64       .text daNpc_De1_Execute__FP11daNpc_De1_c */
static BOOL daNpc_De1_Execute(daNpc_De1_c* i_this) {
    return ((daNpc_De1_c*)i_this)->_execute();
}

/* 00002E64-00002E84       .text daNpc_De1_Draw__FP11daNpc_De1_c */
static BOOL daNpc_De1_Draw(daNpc_De1_c* i_this) {
    return ((daNpc_De1_c*)i_this)->_draw();
}

/* 00002E84-00002E8C       .text daNpc_De1_IsDelete__FP11daNpc_De1_c */
static BOOL daNpc_De1_IsDelete(daNpc_De1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_De1_Method = {
    (process_method_func)daNpc_De1_Create,
    (process_method_func)daNpc_De1_Delete,
    (process_method_func)daNpc_De1_Execute,
    (process_method_func)daNpc_De1_IsDelete,
    (process_method_func)daNpc_De1_Draw,
};
actor_process_profile_definition g_profile_NPC_DE1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_DE1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_De1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_DE1_e,
    /* Actor SubMtd */ &l_daNpc_De1_Method,
    /* Status       */ fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
