//
// SwOp (d_a_switch_op): a custom actor of the Wind Waker Randomizer (wwrando, MIT), which ships it
// only as the binary files/rels/d_a_switch_op.rel (2,340 bytes; its source is not public). This is
// a port written from that REL's disassembly (ref/wwrando/asm/d_a_switch_op.rel, wwrando 9775811,
// read with capstone and the GZLE01 symbol map; docs/RANDOMIZER_PLAN.md D13, step R3.7). It keeps
// the REL's behaviour, including its quirks (noted below), because wwrando's stage data is tuned
// against it.
//
// The randomizer adds these actors to stage data (pirate ship on Windfall, nonlinear Tower of the
// Gods servants, required bosses); vanilla stage data never names "SwOp", so a game that is not
// randomized never creates one. Port code only: the GameCube build has no such unit.
//
#if TARGET_PC
// pc_rando(R3.7): new actor SwOp 0x1F6 from wwrando asm/d_a_switch_op.rel; original: none (wwrando-only REL)
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"

// Parameters:
//   params & 0x0000000F  operation over the checked switches (see checkCondition)
//   params & 0x00000010  continuous: once set, the switch is cleared again when the condition fails
//   params & 0x0000FF00  switch to set (0xFF: none, the actor stays idle)
//   params & 0x00FF0000  first switch to check
//   params & 0xFF000000  number of switches to check, from the first one on
//   home.angle.x & 0xFF  map tool id of a stage EVNT to play before setting the switch (0xFF: none)
//   home.angle.z & 0xFF  delay in frames before the event or the switch
// The switches are those of the actor's current room (current.roomNo), as the REL reads them.
class daSwitchOp_c : public fopAc_ac_c {
public:
    enum Op {
        OP_AND = 0,  // all checked switches are set
        OP_NAND = 1, // at least one is not set
        OP_OR = 2,   // at least one is set
        OP_NOR = 3,  // none is set
        OP_XOR = 4,  // exactly one is set
        OP_XNOR = 5, // not exactly one is set
    };
    enum State {
        STATE_CHECK = 0,      // wait for the condition
        STATE_DELAY = 1,      // count the delay down
        STATE_EVENT = 2,      // order the event until it starts
        STATE_EVENT_RUN = 3,  // wait for the event to end
        STATE_WATCH_OFF = 4,  // continuous: switch set, clear it when the condition fails
        STATE_DONE = 5,       // idle
    };

    s32 create();
    BOOL execute();
    BOOL checkCondition();
    void actionCheck();
    void actionDelay();
    void actionEvent();
    void actionEventRun();
    void actionWatchOff();
    void onSwitchToSet() { dComIfGs_onSwitch(mSwitchToSet, fopAcM_GetRoomNo(this)); }
    void offSwitchToSet() { dComIfGs_offSwitch(mSwitchToSet, fopAcM_GetRoomNo(this)); }
    u8 stateAfterSet() const { return mContinuous ? STATE_WATCH_OFF : STATE_DONE; }

    /* 0x290 */ u8 mState;
    /* 0x291 */ u8 mOp;
    /* 0x292 */ u8 mContinuous;
    /* 0x293 */ u8 mSwitchToSet;
    /* 0x294 */ u8 mFirstSwitch;
    /* 0x295 */ u8 mNumSwitches;
    /* 0x296 */ u8 mEvntNo;
    /* 0x298 */ s16 mEventIdx;
    /* 0x29A */ u8 mDelay;
    /* 0x29B */ u8 mTimer;
}; // Size: 0x29C on the GameCube

s32 daSwitchOp_c::create() {
    fopAcM_SetupActor(this, daSwitchOp_c);

    u32 prm = fopAcM_GetParam(this);
    mOp = prm & 0xF;
    mContinuous = (prm >> 4) & 1;
    mSwitchToSet = (prm >> 8) & 0xFF;
    mFirstSwitch = (prm >> 16) & 0xFF;
    mNumSwitches = (prm >> 24) & 0xFF;
    mEvntNo = home.angle.x & 0xFF;
    mDelay = home.angle.z & 0xFF;
    mEventIdx = dComIfGp_evmng_getEventIdx(NULL, mEvntNo);
    mTimer = mDelay;

    if (mSwitchToSet == 0xFF) {
        mState = STATE_DONE;
    } else if (dComIfGs_isSwitch(mSwitchToSet, fopAcM_GetRoomNo(this))) {
        mState = stateAfterSet();
    } else {
        mState = STATE_CHECK;
    }
    return cPhs_COMPLEATE_e;
}

BOOL daSwitchOp_c::checkCondition() {
    int numOff = 0;
    int numOn = 0;
    u8 sw = mFirstSwitch;
    for (int i = 0; i < mNumSwitches; i++, sw++) {
        if (dComIfGs_isSwitch(sw, fopAcM_GetRoomNo(this))) {
            numOn++;
        } else {
            numOff++;
        }
    }
    switch (mOp) {
    case OP_AND:
        return numOff == 0;
    case OP_NAND:
        return numOff > 0;
    case OP_OR:
        return numOn > 0;
    case OP_NOR:
        return numOn == 0;
    case OP_XOR:
        return numOn == 1;
    case OP_XNOR:
        return numOn != 1;
    default:
        return FALSE;
    }
}

void daSwitchOp_c::actionCheck() {
    if (!checkCondition()) {
        return;
    }
    if (mDelay != 0) {
        mState = STATE_DELAY;
    } else if (mEventIdx != -1) {
        mState = STATE_EVENT;
    } else {
        onSwitchToSet();
        mState = stateAfterSet();
    }
}

void daSwitchOp_c::actionDelay() {
    if (mContinuous && !checkCondition()) {
        mTimer = mDelay;
        mState = STATE_CHECK;
        return;
    }
    if (mTimer != 0) {
        mTimer--;
        return;
    }
    if (mEventIdx != -1) {
        mState = STATE_EVENT;
        return;
    }
    onSwitchToSet();
    // Quirk kept from the REL: a continuous SwOp ends here (it never clears its switch after a
    // delay without event), and a one-shot one stays in this state and sets the switch again every
    // frame.
    if (mContinuous) {
        mState = STATE_DONE;
    }
}

void daSwitchOp_c::actionEvent() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        mState = STATE_EVENT_RUN;
        onSwitchToSet();
        return;
    }
    if (mContinuous && !checkCondition()) {
        mState = STATE_CHECK;
        return;
    }
    fopAcM_orderOtherEventId(this, mEventIdx, mEvntNo, 0xFFFF, 0, 1);
}

void daSwitchOp_c::actionEventRun() {
    if (dComIfGp_evmng_endCheck(mEventIdx)) {
        mState = stateAfterSet();
        dComIfGp_event_reset();
    }
}

void daSwitchOp_c::actionWatchOff() {
    if (checkCondition()) {
        return;
    }
    // Quirk kept from the REL: mTimer is not reloaded, so a delay only applies the first time.
    mState = STATE_CHECK;
    offSwitchToSet();
}

BOOL daSwitchOp_c::execute() {
    switch (mState) {
    case STATE_CHECK:
        actionCheck();
        break;
    case STATE_DELAY:
        actionDelay();
        break;
    case STATE_EVENT:
        actionEvent();
        break;
    case STATE_EVENT_RUN:
        actionEventRun();
        break;
    case STATE_WATCH_OFF:
        actionWatchOff();
        break;
    }
    return TRUE;
}

static BOOL daSwitchOp_Draw(daSwitchOp_c*) {
    return TRUE;
}

static BOOL daSwitchOp_Execute(daSwitchOp_c* i_this) {
    return i_this->execute();
}

static BOOL daSwitchOp_IsDelete(daSwitchOp_c*) {
    return TRUE;
}

static BOOL daSwitchOp_Delete(daSwitchOp_c* i_this) {
    i_this->~daSwitchOp_c();
    return TRUE;
}

static s32 daSwitchOp_Create(fopAc_ac_c* i_this) {
    return ((daSwitchOp_c*)i_this)->create();
}

static actor_method_class l_daSwitchOp_Method = {
    (process_method_func)daSwitchOp_Create,
    (process_method_func)daSwitchOp_Delete,
    (process_method_func)daSwitchOp_Execute,
    (process_method_func)daSwitchOp_IsDelete,
    (process_method_func)daSwitchOp_Draw,
};

actor_process_profile_definition g_profile_SWITCH_OP = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_SWITCH_OP_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daSwitchOp_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ 0x009F,
    /* Actor SubMtd */ &l_daSwitchOp_Method,
    /* Status       */ 0,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
#endif /* TARGET_PC */
