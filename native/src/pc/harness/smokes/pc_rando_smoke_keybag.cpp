// COS_SMOKE=rando COS_RANDO_SMOKE=keybag (docs/RANDOMIZER_PLAN.md step R3.5): the key bag text
// (features/rando/pc_rando_keybag.h, game_hooks/pc_rando_hooks_keybag.cpp, the stringSet hunk), e.g.
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=keybag
// Once the play scene has run 10 frames, messages are rendered the way the item screen renders an
// item's description (dMenu_Item_c::itemnoteSet: fopMsgM_itemMsgGet_c, then a
// fopMsgM_msgDataProc_c with shortCut, stringLength, stringShift and stringSet), and the output has
// its ESC formatting codes removed before it is compared:
//   1. plain boot (not forced): pc_rando() is 0, 403/603 have no overlay record, and a test message
//      "A<1A 05 00 00 4B>B" renders as "AB" (the tag is skipped, as in the original game);
//   2. activation: 403 is "Key Bag"; with known saved key counts and Big Keys (DRC 2 + Big Key, FW 0,
//      TotG 1, ET 3 + Big Key, WT 0) message 603 renders into wwrando's text with those numbers, and
//      the test message renders as "A2 +BigB";
//   3. in the dungeon: with the current save stage set to DRC (3) the DRC column reads the loaded
//      stage memory (here 1 key, no Big Key), the other columns still the saved stage infos;
//   4. deactivation (not forced): 403/603 have no overlay record again and the test message renders
//      as "AB" once more.
// The save state the smoke changes is restored before it ends. Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_keybag.h"
#include "features/rando/pc_rando_msg.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/d_meter.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_ext.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace pc {

namespace {

constexpr const char* kName = "keybag";

bool check(const char* what, bool ok) {
    return randoSmokeCheck(kName, what, ok);
}

// The rendered text without the ESC codes stringSet adds ("\x1B" two letters "[...]").
std::string strip(const char* s) {
    std::string out;
    for (const char* p = s; *p != '\0'; p++) {
        if (*p == '\x1B') {
            const char* end = strchr(p, ']');
            if (end == nullptr) {
                break;
            }
            p = end;
            continue;
        }
        out += *p;
    }
    return out;
}

char sNote[2][0x800];
char sDummy[2][0x800];

// itemnoteSet's rendering of the BMG bytes `mesg` with `entry`.
std::string render(const char* mesg, JMSMesgEntry_c* entry) {
    memset(sNote, 0, sizeof sNote);
    memset(sDummy, 0, sizeof sDummy);
    fopMsgM_msgDataProc_c proc;
    proc.dataInit();
    proc.setBmgData((char*)mesg);
    proc.setOutMessage(sNote[0], sNote[1], sDummy[0], sDummy[1]);
    proc.setFont(mDoExt_getMesgFont());
    proc.setRubyFont(mDoExt_getRubyFont());
    proc.setCharSpace(0);
    proc.setRubyCharSpace(0);
    proc.setLineSpace(g_msgHIO.field_0x5e);
    proc.setMesgEntry(entry);
    proc.setFontSize(g_msgHIO.field_0x70);
    proc.setRubyFontSize(g_msgHIO.field_0x70 / 2);
    proc.setLineWidth(0x1FE);
    proc.setCenterLineWidth(0x1E6);
    proc.setSendSpeed(2);
    proc.setSpaceTimer(0);
    proc.shortCut();
    proc.setSpaceFlagOff();
    proc.stringLength();
    proc.stringShift();
    proc.iconIdxRefresh();
    proc.setLineCount(0);
    proc.stringSet();
    return strip(sNote[0]);
}

// Message `no` as the item screen reads it (overlay included); "" when there is none.
std::string renderMsg(u16 no) {
    fopMsgM_itemMsgGet_c get;
    mesg_header* head = get.getMesgHeader(no);
    if (head == nullptr) {
        return "";
    }
    const char* mesg = get.getMessage(head);
    JMSMesgEntry_c entry = get.getMesgEntry(head);
    return mesg != nullptr ? render(mesg, &entry) : "";
}

// "A", the DRC key tag, "B": stringSet with the entry of message 603 (a text box the item screen uses).
std::string renderTag() {
    static const char kBytes[] = {'A', 0x1A, 0x05, 0x00, 0x00, 0x4B, 'B', '\0'};
    fopMsgM_itemMsgGet_c get;
    mesg_header* head = get.getMesgHeader(PCR_KEYBAG_MSG_DESC);
    if (head == nullptr) {
        return "<no 603>";
    }
    JMSMesgEntry_c entry = get.getMesgEntry(head);
    return render(kBytes, &entry);
}

dSv_memBit_c& saved(int stage) {
    return g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
}

void setDungeon(dSv_memBit_c& m, int keys, bool big) {
    m.setKeyNum((u8)keys);
    if (big) {
        m.onDungeonItemBossKey();
    } else {
        m.offDungeonItemBossKey();
    }
}

void report(const char* what, const std::string& got, const std::string& want) {
    const bool ok = got == want;
    check(what, ok);
    if (!ok) {
        fprintf(stderr, "[cos] rando-smoke %s:   got  \"%s\"\n[cos] rando-smoke %s:   want \"%s\"\n", kName,
                got.c_str(), kName, want.c_str());
    }
}

} // namespace

void randoSmokeKeybag(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(10)) {
        return;
    }
    const bool forced = pc_rando_forced() != 0;

    // Save what the smoke changes.
    dSv_memBit_c bootSaved[8];
    for (int i = 0; i < 8; i++) {
        bootSaved[i] = saved(i);
    }
    const dSv_memBit_c bootMem = g_dComIfG_gameInfo.save.getMemory().getBit();
    const u8 bootStage = g_dComIfG_gameInfo.save.getDan().mStageNo;

    // 1. plain
    if (!forced) {
        check("plain: pc_rando() is 0", pc_rando() == 0);
        check("plain: no overlay record for 403/603",
              pc_rando_msg_find(PCR_KEYBAG_MSG_NAME) == nullptr && pc_rando_msg_find(PCR_KEYBAG_MSG_DESC) == nullptr);
        report("plain: tag 0x4B is skipped", renderTag(), "AB");
        const std::string vanilla = renderMsg(PCR_KEYBAG_MSG_DESC);
        check("plain: 603 is the vanilla text", !vanilla.empty() && vanilla.find("DRC:") == std::string::npos);
        pc_rando_set_active(nullptr, nullptr);
    }

    // 2. active
    check("active: pc_rando() is 1", pc_rando() == 1);
    const PcRandoMsg* name = pc_rando_msg_find(PCR_KEYBAG_MSG_NAME);
    const PcRandoMsg* desc = pc_rando_msg_find(PCR_KEYBAG_MSG_DESC);
    check("active: 403 and 603 are text records", name != nullptr && desc != nullptr && !name->has_entry &&
                                                     !desc->has_entry);
    char nameBuf[64];
    fopMsgM_messageGet(nameBuf, PCR_KEYBAG_MSG_NAME);
    report("active: messageGet 403", nameBuf, "Key Bag");
    report("active: 403 renders", renderMsg(PCR_KEYBAG_MSG_NAME), "Key Bag");

    g_dComIfG_gameInfo.save.getDan().mStageNo = 0xFF; // in no dungeon
    setDungeon(saved(3), 2, true);
    setDungeon(saved(4), 0, false);
    setDungeon(saved(5), 1, false);
    setDungeon(saved(6), 3, true);
    setDungeon(saved(7), 0, false);
    report("active: 603 renders the saved counts", renderMsg(PCR_KEYBAG_MSG_DESC),
           "A handy bag for holding your keys!\n"
           "Here's how many you've got with you:\n"
           "DRC: 2 +Big    FW: 0         TotG: 1     \n"
           "ET: 3 +Big      WT: 0     ");
    report("active: tag 0x4B expands", renderTag(), "A2 +BigB");
    char direct[32] = "untouched";
    check("active: pc_rando_text_command ignores code 0x4A",
          pc_rando_text_command(0x4A, direct, sizeof direct) == 0 && strcmp(direct, "untouched") == 0);

    // 3. in DRC: the loaded memory
    g_dComIfG_gameInfo.save.getDan().mStageNo = 3;
    setDungeon(g_dComIfG_gameInfo.save.getMemory().getBit(), 1, false);
    report("in DRC: the DRC column reads the loaded memory", renderMsg(PCR_KEYBAG_MSG_DESC),
           "A handy bag for holding your keys!\n"
           "Here's how many you've got with you:\n"
           "DRC: 1         FW: 0         TotG: 1     \n"
           "ET: 3 +Big      WT: 0     ");

    // Restore the save state.
    for (int i = 0; i < 8; i++) {
        saved(i) = bootSaved[i];
    }
    g_dComIfG_gameInfo.save.getMemory().getBit() = bootMem;
    g_dComIfG_gameInfo.save.getDan().mStageNo = bootStage;

    // 4. deactivated
    if (!forced) {
        pc_rando_deactivate();
        check("deactivated: no overlay record for 403/603",
              pc_rando_msg_find(PCR_KEYBAG_MSG_NAME) == nullptr && pc_rando_msg_find(PCR_KEYBAG_MSG_DESC) == nullptr);
        report("deactivated: tag 0x4B is skipped again", renderTag(), "AB");
    }
    randoSmokeEnd(kName);
}

} // namespace pc
