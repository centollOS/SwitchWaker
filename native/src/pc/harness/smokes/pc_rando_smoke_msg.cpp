// COS_SMOKE=rando COS_RANDO_SMOKE=msg (docs/RANDOMIZER_PLAN.md step R0.12): the message overlay in a
// booted game, e.g. run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=msg. Once the play scene has run
// 10 frames:
//   1. the overlay is built while the file is not randomized: new text for vanilla message 3906 (a
//      shop description) and a new message 849 (wwrando's Aryll line, bottom text box);
//   2. with pc_rando() 0 (a plain boot) the game still reads the BMG: fopMsgM_messageGet(3906) gives
//      the vanilla text and fopMsgM_msgGet_c finds no message 849;
//   3. pc_rando_set_active: fopMsgM_messageGet returns the overlay texts (tags stripped, as for BMG
//      text), both message getters return the overlay bytes, 849's INF1 entry is the overlay's and
//      3906 keeps its BMG entry;
//   4. message 849 is shown (fopMsgM_messageSet): the d_msg process reads the overlay's text and
//      entry and runs 90 frames;
//   5. pc_rando_deactivate: the overlay is cleared (unless COS_RANDO_FORCE=1) and 3906 reads the BMG
//      again.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_msg.h"

#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_msg.h"
#include "f_op/f_op_msg.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstring>
#include <unistd.h>

static_assert(sizeof(JMSMesgEntry_c) == PCR_MSG_ENTRY_SIZE, "the overlay writes a 0x18-byte BMG entry");

namespace pc {

namespace {

constexpr const char* kName = "msg";
constexpr u16 kShop = 3906;
constexpr u16 kNew = 849;
constexpr const char* kShopText = "Rando " PCR_MSG_COLOR_RED "shop" PCR_MSG_COLOR_DEFAULT " text\nline two";
constexpr const char* kShopPlain = "Rando shop text\nline two";
constexpr const char* kNewText = "Oh! Did you get stuck in there, Big Brother?";

unsigned int sEndFrame = 0;
fpc_ProcID sMsgId = fpcM_ERROR_PROCESS_ID_e;
bool sShown = false;
char sVanilla[1024];

bool entryOf(u16 msgNo, JMSMesgEntry_c* out, const char** text) {
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msgNo);
    if (head == nullptr) {
        return false;
    }
    *text = get.getMessage(head);
    if (*text == nullptr) {
        return false;
    }
    *out = get.getMesgEntry(head);
    return get.getMesgNumber() == msgNo;
}

void* isMsg(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_MSG_e ? proc : nullptr;
}

void staticChecks(unsigned int frames) {
    const int forced = pc_rando_forced();
    char buf[1024];
    JMSMesgEntry_c vanillaEntry;
    const char* vanillaText = nullptr;
    const bool have3906 = entryOf(kShop, &vanillaEntry, &vanillaText);
    randoSmokeCheck(kName, "BMG has message 3906", have3906);

    // 1. build while inactive
    PcRandoMsgEntry e;
    pc_rando_msg_entry_defaults(&e);
    e.text_alignment = 4; // tweaks.py: bottom text box
    randoSmokeCheck(kName, "set_text 3906", pc_rando_msg_set_text(kShop, kShopText) == PCR_MSG_OK);
    randoSmokeCheck(kName, "add 849", pc_rando_msg_add(kNew, &e, kNewText) == PCR_MSG_OK);

    // 2. not randomized: the BMG
    if (!forced) {
        fopMsgM_messageGet(sVanilla, kShop);
        randoSmokeCheck(kName, "inactive: 3906 is the BMG's text", sVanilla[0] != '\0' && strcmp(sVanilla, kShopPlain) != 0);
        fopMsgM_msgGet_c get;
        mesg_header* head = get.getMesgHeader(kNew);
        randoSmokeCheck(kName, "inactive: no message 849", head != nullptr && get.getMessage(head) == nullptr);
        writef(STDERR_FILENO, "[cos] rando-smoke msg: vanilla 3906: \"%.60s\"\n", sVanilla);
    }

    // 3. randomized: the overlay
    pc_rando_set_active(nullptr, nullptr);
    fopMsgM_messageGet(buf, kShop);
    randoSmokeCheck(kName, "messageGet 3906 is the overlay text", strcmp(buf, kShopPlain) == 0);
    fopMsgM_messageGet(buf, kNew);
    randoSmokeCheck(kName, "messageGet 849 is the overlay text", strcmp(buf, kNewText) == 0);

    JMSMesgEntry_c entry;
    const char* text = nullptr;
    randoSmokeCheck(kName, "msgGet 3906 gives the overlay bytes",
                    entryOf(kShop, &entry, &text) && text == pc_rando_msg_hook_text(kShop));
    randoSmokeCheck(kName, "3906 keeps its BMG entry",
                    have3906 && memcmp(&entry, &vanillaEntry, sizeof entry) == 0);
    randoSmokeCheck(kName, "msgGet 849 gives the overlay bytes",
                    entryOf(kNew, &entry, &text) && text == pc_rando_msg_hook_text(kNew));
    randoSmokeCheck(kName, "849 entry is the overlay's",
                    entry.mMsgNo == kNew && entry.mTextAlignment == 4 && entry.mTextboxType == 0 &&
                        entry.mTextboxPosition == 3 && entry.mItemImage == 0xFF);

    fopMsgM_itemMsgGet_c item;
    mesg_header* head = item.getMesgHeader(kNew);
    const char* itemText = head != nullptr ? item.getMessage(head) : nullptr;
    randoSmokeCheck(kName, "itemMsgGet 849 gives the overlay bytes and entry",
                    itemText == pc_rando_msg_hook_text(kNew) && item.mResMsgNo == kNew &&
                        item.getMesgEntry(head).mTextAlignment == 4);

    // 4. show 849
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    layer_class* savedLayer = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(static_cast<base_process_class*>((fopAc_ac_c*)player)->mLyTg.mpLayer);
    sMsgId = fopMsgM_messageSet(kNew, &player->eyePos);
    fpcLy_SetCurrentLayer(savedLayer);
    randoSmokeCheck(kName, "messageSet 849", sMsgId != fpcM_ERROR_PROCESS_ID_e);
    sEndFrame = frames + 90;
}

} // namespace

void randoSmokeMsg(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame == 0) {
        staticChecks(frames);
        return;
    }
    if (!sShown) {
        if (sub_msg_class* m = (sub_msg_class*)fpcM_Search(isMsg, nullptr); m != nullptr && m->mMsgID == kNew) {
            sShown = m->mpMesgStr == pc_rando_msg_hook_text(kNew) && m->mMesgEntry.mTextAlignment == 4;
            writef(STDERR_FILENO, "[cos] rando-smoke msg: d_msg process for 849 at frame %u (overlay text %s)\n",
                   frames, sShown ? "yes" : "no");
        }
    }
    if (frames < sEndFrame) {
        return;
    }
    randoSmokeCheck(kName, "d_msg shows 849 from the overlay", sShown);

    // 5. off again
    const int forced = pc_rando_forced();
    pc_rando_deactivate();
    if (!forced) {
        randoSmokeCheck(kName, "deactivate clears the overlay", pc_rando_msg_count() == 0);
        char buf[1024];
        fopMsgM_messageGet(buf, kShop);
        randoSmokeCheck(kName, "deactivate: 3906 is the BMG's text again", strcmp(buf, sVanilla) == 0);
    } else {
        // Other steps' messages (e.g. R3.3's 278, R3.4's dungeon item messages, R3.6's 503/703, R3.13's
        // 848/704) are in the overlay too: check this smoke's two records rather than the count.
        randoSmokeCheck(kName, "forced: the overlay stays",
                        pc_rando_msg_find(kShop) != nullptr && pc_rando_msg_find(kNew) != nullptr);
    }
    randoSmokeEnd(kName);
}

} // namespace pc
