// COS_SMOKE=rando COS_RANDO_SMOKE=pirateship (docs/RANDOMIZER_PLAN.md step R3.17): the pirate ship at
// Windfall (features/rando/pc_rando_tweak_pirate_ship.h, game_hooks/pc_rando_hooks_pirate_ship.cpp and
// the hunks in JAIZelBasic.cpp and d_a_npc_ls1.cpp).
//
// On Windfall (run.sh rando --stage sea:11:0 --env COS_RANDO_SMOKE=pirateship [--env COS_RANDO_FORCE=1]):
// with pc_rando() 1 the room file holds the edit (checked on the final bytes the game keeps) and a
// live ship (Obj_Pirateship) stands at the layer-2 position with door type 0; with pc_rando() 0
// nothing was patched and no ship with door type 0 is there.
//
// On the ship (run.sh rando --stage Asoko:0:0:2 --env COS_RANDO_SMOKE=pirateship [--env COS_RANDO_FORCE=1]):
//   1. the stage files: the 3 Asoko sites patched once each (the final bytes checked); live actors:
//      Aryll (Ls1) and 3 SwOps, no Niko (P2); Ashut's switch 0xC4; messages 3008 (against
//      R3_17_pirate_ship_game.txt: wwrando's text wrapped with the disc's font by gclib), 849 and 850
//      from the overlay; the BGM scene's second dynamic wave is slot 0x0E holding IsleLink_0.aw (0x19);
//   2. the player is put in the chest room (inside SW_C00) with no countdown running: switch 0xC3 is
//      set, then after the SwOp's delay "AryllOpensDoor" runs (A is pressed for its two messages,
//      Ls1's TALK_MSG cuts 849 and 850 are followed) and ends; switches 0xC1 and 0xC4 are then set.
// With pc_rando() 0 on the ship: nothing was patched, Niko is there, no Aryll, no SwOp, Ashut keeps
// 0xC0; no event starts in the chest room within 600 frames and 0xC4 stays clear; no wave slot 0x0E.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_pirate_ship.h"

#include "JAZelAudio/JAIZelBasic.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#define PC_RANDO_PATCHER(fn) extern "C" void fn(void);
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER

namespace pc {

namespace {

constexpr const char* kName = "pirateship";
constexpr const char* kFixture = "R3_17_pirate_ship_game.txt";
constexpr unsigned int kEventWait = 600;   // the event must start by then (delay 150)
constexpr unsigned int kEventFrames = 3000; // and end by then
const cXyz kChestRoom(0.0f, -550.0f, -3700.0f);

// The final bytes of each site, as the game keeps them (an observer patcher registered after the
// step's own, so it runs last).
struct Observed {
    bool seen = false;
    bool ok = false;
    char why[160] = "";
};
Observed sObs[PCR_PS_SITE_COUNT];
bool sRegistered = false;

void observer(PcRandoResFile* f) {
    for (int i = 0; i < PCR_PS_SITE_COUNT; i++) {
        const PcRandoPirateShipSiteInfo* s = pc_rando_tweak_pirate_ship_site(i);
        if (strcmp(s->stage, f->stage) == 0 && strcmp(s->arc, f->arc) == 0 && strcmp(s->name, f->name) == 0) {
            Observed& o = sObs[i];
            o.seen = true;
            o.why[0] = '\0';
            o.ok = pc_rando_tweak_pirate_ship_verify(i, f->data, f->size, o.why, sizeof o.why) != 0;
        }
    }
}

std::vector<std::string> fixtureDirs() {
    std::vector<std::string> out;
    const char* env = std::getenv("COS_RANDO_FIXTURES");
    if (env != nullptr && env[0] != '\0') {
        out.push_back(env);
        return out;
    }
    // .../native/src/pc/harness/smokes/<this file> -> .../native/check/rando/fixtures
    std::string dir = __FILE__;
    for (int up = 0; up < 5; up++) {
        const size_t slash = dir.find_last_of('/');
        if (slash == std::string::npos) {
            return out;
        }
        dir.resize(slash);
    }
    dir += "/check/rando/fixtures";
    out.push_back(dir);
    const char* run = std::getenv("COS_RUN_DIR");
    if (dir[0] != '/' && run != nullptr && run[0] != '\0') {
        out.push_back(std::string(run) + "/../../" + dir);
    }
    return out;
}

// text3008 <box> <hex>
bool readFixture(int* box, std::string* text) {
    for (const std::string& d : fixtureDirs()) {
        FILE* f = std::fopen((d + "/" + kFixture).c_str(), "rb");
        if (f == nullptr) {
            continue;
        }
        static char line[8192];
        bool found = false;
        while (std::fgets(line, sizeof line, f) != nullptr) {
            line[std::strcspn(line, "\r\n")] = '\0';
            char hex[8192];
            if (std::sscanf(line, "text3008 %d %8191s", box, hex) == 2) {
                text->clear();
                for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
                    const char b[3] = {hex[i], hex[i + 1], 0};
                    text->push_back((char)std::strtol(b, nullptr, 16));
                }
                found = true;
            }
        }
        std::fclose(f);
        return found;
    }
    return false;
}

// The text the game reads for msg_no (through fopMsgM_msgGet_c, so through the overlay), in wwrando's
// syntax, and the entry's initial sound.
bool gameText(uint16_t msg_no, std::string* text, int* sound) {
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    const char* bytes = get.getMessage(head);
    if (bytes == nullptr || get.getMesgNumber() != msg_no) {
        return false;
    }
    std::vector<char> buf(8192);
    const size_t n = pc_rando_msg_decode((const uint8_t*)bytes, 0x10000, buf.data(), buf.size());
    if (n >= buf.size()) {
        return false;
    }
    text->assign(buf.data(), n);
    *sound = get.getMesgEntry(head).mInitialSound;
    return true;
}

struct Procs {
    s16 name;
    int n;
    fopAc_ac_c* list[16];
};

void* collect(void* proc, void* data) {
    Procs* c = static_cast<Procs*>(data);
    if (fpcM_GetName(proc) == c->name && c->n < 16) {
        c->list[c->n++] = static_cast<fopAc_ac_c*>(proc);
    }
    return nullptr;
}

Procs procsNamed(s16 name) {
    Procs p = {};
    p.name = name;
    fopAcM_Search(collect, &p);
    return p;
}

void checkSites(const int* sites, int n) {
    char what[256];
    for (int k = 0; k < n; k++) {
        const int i = sites[k];
        const PcRandoPirateShipSiteInfo* s = pc_rando_tweak_pirate_ship_site(i);
        if (!pc_rando()) {
            snprintf(what, sizeof what, "vanilla: %s/%s/%s not patched", s->stage, s->arc, s->name);
            randoSmokeCheck(kName, what, s->runs == 0);
            continue;
        }
        snprintf(what, sizeof what, "%s/%s/%s patched once (%s)", s->stage, s->arc, s->name,
                 pc_rando_tweak_pirate_ship_result_name(i));
        randoSmokeCheck(kName, what, s->runs == 1 && s->result == 0);
        snprintf(what, sizeof what, "%s/%s/%s: the game keeps the edit%s%s", s->stage, s->arc, s->name,
                 sObs[i].why[0] ? ": " : "", sObs[i].why);
        randoSmokeCheck(kName, what, sObs[i].seen && sObs[i].ok);
    }
}

void checkWindfall() {
    const int sites[] = {PCR_PS_WINDFALL};
    checkSites(sites, 1);
    Procs ships = procsNamed(fpcNm_Obj_Pirateship_e);
    int open = 0;
    for (int i = 0; i < ships.n; i++) {
        fopAc_ac_c* a = ships.list[i];
        const u32 prm = fopAcM_GetParam(a);
        writef(STDERR_FILENO, "[cos] rando-smoke pirateship: ship %d at (%.0f %.0f %.0f) params 0x%08X\n", i,
               a->home.pos.x, a->home.pos.y, a->home.pos.z, prm);
        // The layer-2 ship stands at (-2795.89, 0, -204592.64).
        if ((prm & 0xFF00) == 0 && std::fabs(a->home.pos.x + 2795.89f) < 1.0f && std::fabs(a->home.pos.z + 204592.64f) < 1.0f) {
            open++;
        }
    }
    if (pc_rando()) {
        randoSmokeCheck(kName, "a live pirate ship at Windfall with door type 0 (no password)", open >= 1);
    } else {
        randoSmokeCheck(kName, "vanilla: no ship with door type 0 at Windfall", open == 0);
    }
}

enum State { kSetup, kPlace, kWait, kRun, kAfter };
State sState = kSetup;
unsigned int sSince = 0;
int sRoom = 0;
s16 sEventIdx = -1;
int sCurCut[16];
std::vector<int> sTalks; // msg_num of Ls1's TALK_MSG cuts, in order

bool sw(int no) {
    return dComIfGs_isSwitch(no, sRoom) != FALSE;
}

void checkShipSetup() {
    const int sites[] = {PCR_PS_SHIP_STAGE, PCR_PS_SHIP_EVENTS, PCR_PS_SHIP_ROOM};
    checkSites(sites, 3);
    const int aryll = procsNamed(fpcNm_NPC_LS1_e).n;
    const int niko = procsNamed(fpcNm_NPC_P2_e).n;
    const int swops = procsNamed(fpcNm_SWITCH_OP_e).n;
    Procs ashut = procsNamed(fpcNm_Obj_Ashut_e);
    const int ashutSw = ashut.n > 0 ? (int)(fopAcM_GetParam(ashut.list[0]) & 0xFF) : -1;
    JAIZelBasic* audio = JAIZelBasic::getInterface();
    const int wave2 = audio != nullptr ? audio->mSecondDynamicSceneWaveIndex : -1;
    const int slot = JAIZelBasic::m_dy_wave_set_2nd[PCR_PS_SCENE_WAVE2][0];
    writef(STDERR_FILENO,
           "[cos] rando-smoke pirateship: Ls1 %d, P2 %d, SwOp %d, Ashut %d (switch 0x%02X), scene wave2 0x%02X, slot "
           "0x0E -> 0x%02X, EVNT index %d\n",
           aryll, niko, swops, ashut.n, ashutSw, wave2, slot, pc_rando_tweak_pirate_ship_evnt_index());
    sEventIdx = dComIfGp_evmng_getEventIdx(PCR_PS_EVENT_NAME);
    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: Niko (P2) is on the ship, no Aryll, no SwOp", niko >= 1 && aryll == 0 && swops == 0);
        randoSmokeCheck(kName, "vanilla: Ashut checks 0xC0", ashutSw == PCR_PS_SW_COUNTDOWN);
        randoSmokeCheck(kName, "vanilla: no event AryllOpensDoor", sEventIdx < 0);
        randoSmokeCheck(kName, "vanilla: Asoko's BGM scene has no second dynamic wave", wave2 == 0);
        bool none = true;
        for (uint16_t n : {(uint16_t)PCR_PS_MSG_ARYLL, (uint16_t)PCR_PS_MSG_STUCK, (uint16_t)PCR_PS_MSG_OPEN}) {
            none = none && pc_rando_msg_find(n) == nullptr;
        }
        randoSmokeCheck(kName, "vanilla: no overlay record for 3008/849/850", none);
        return;
    }
    randoSmokeCheck(kName, "Aryll (Ls1) is on the ship, Niko (P2) is not", aryll == 1 && niko == 0);
    randoSmokeCheck(kName, "3 live SwOps", swops == 3);
    randoSmokeCheck(kName, "Ashut checks switch 0xC4", ashutSw == PCR_PS_SW_DOOR_OPEN);
    randoSmokeCheck(kName, "event AryllOpensDoor is in Asoko's event list", sEventIdx >= 0);
    randoSmokeCheck(kName, "Asoko's BGM scene loads second dynamic wave slot 0x0E = IsleLink_0.aw (0x19)",
                    wave2 == PCR_PS_SCENE_WAVE2 && slot == PCR_PS_ISLE_LINK_WAVE);
    // The archive patcher built the texts while the stage loaded.
    randoSmokeCheck(kName, "messages 3008/849/850 in the overlay after the stage load",
                    pc_rando_msg_find(PCR_PS_MSG_ARYLL) != nullptr && pc_rando_msg_find(PCR_PS_MSG_STUCK) != nullptr &&
                        pc_rando_msg_find(PCR_PS_MSG_OPEN) != nullptr);
    int box = -1;
    std::string want, got;
    int sound = -1;
    if (randoSmokeCheck(kName, "fixture read", readFixture(&box, &want))) {
        const bool have = gameText(PCR_PS_MSG_ARYLL, &got, &sound);
        if (!have || got != want) {
            writef(STDERR_FILENO, "[cos] rando-smoke pirateship: 3008 is \"%s\"\n  want \"%s\"\n", got.c_str(), want.c_str());
        }
        randoSmokeCheck(kName, "message 3008 as wwrando writes it (wrapped with the real font)", have && got == want);
        randoSmokeCheck(kName, "message 3008's initial sound 106", sound == PCR_PS_ARYLL_SOUND);
        const PcRandoMsg* r = pc_rando_msg_find(PCR_PS_MSG_ARYLL);
        randoSmokeCheck(kName, "message 3008 keeps the BMG's text box type",
                        r != nullptr && r->has_entry && r->entry.text_box_type == box);
    }
    for (uint16_t n : {(uint16_t)PCR_PS_MSG_STUCK, (uint16_t)PCR_PS_MSG_OPEN}) {
        char what[96];
        snprintf(what, sizeof what, "message %u from the overlay", (unsigned)n);
        randoSmokeCheck(kName, what,
                        gameText(n, &got, &sound) && got == pc_rando_tweak_pirate_ship_new_text(n) && pc_rando_msg_find(n));
    }
}

// Logs the running event's cut changes; records Ls1's TALK_MSG message numbers.
void logCuts(unsigned int frames) {
    dEvt_control_c* evc = g_dComIfG_gameInfo.play.getEvent();
    dEvDtEvent_c* ev = dComIfGp_getPEvtManager()->getEventData(evc->mEventId);
    if (ev == nullptr) {
        return;
    }
    dEvDtBase_c& list = dComIfGp_getPEvtManager()->mList;
    for (int i = 0; i < ev->getNStaff() && i < 16; i++) {
        dEvDtStaff_c* s = list.getStaffP(ev->getStaff(i));
        const int cut = (int)s->getCurrentCut();
        if (cut == sCurCut[i]) {
            continue;
        }
        sCurCut[i] = cut;
        dEvDtCut_c* c = cut >= 0 && cut < list.getCutNum() ? list.getCutP(cut) : nullptr;
        writef(STDERR_FILENO, "[cos] rando-smoke pirateship: frame %u %s -> cut %d %.32s\n", frames, s->getName(), cut,
               c != nullptr ? c->getName() : "-");
        if (c != nullptr && strcmp(s->getName(), "Ls1") == 0 && strncmp(c->getName(), "TALK_MSG", 8) == 0) {
            int msg = -1;
            for (s32 d = (s32)c->getDataTop(); d >= 0 && d < list.getDataNum();) {
                dEvDtData_c* data = list.getDataP(d);
                if (strcmp(data->getName(), "msg_num") == 0 && data->getType() == dEvDtData_c::TYPE_INT) {
                    msg = *list.getIDataP(data->getIndex());
                }
                d = (s32)data->getNext();
            }
            sTalks.push_back(msg);
        }
    }
}

} // namespace

void randoSmokePirateship(unsigned int frames) {
    if (!sRegistered) {
        sRegistered = true;
        // Every step's patcher first (idempotent; the game side would register them before its first
        // dispatch), then the observer, so it sees each file's final bytes.
#define PC_RANDO_PATCHER(fn) fn();
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER
        pc_rando_register_res_patcher(observer, PCR_RES_ANY);
        for (int& c : sCurCut) {
            c = -2;
        }
    }
    if (!randoSmokeInPlay(30)) {
        // HAS_HEROS_CLOTHES, which a randomized new game sets (R3.1) and Aryll's type 0 needs
        // (daNpc_Ls1_c::decideType), set while the stage loads, before its actors are created.
        if (pc_rando()) {
            dComIfGs_onEventBit(dSv_event_flag_c::UNK_2A80);
        }
        return;
    }
    const char* stage = dComIfGp_getStartStageName();
    if (strcmp(stage, "sea") == 0) {
        checkWindfall();
        randoSmokeEnd(kName);
    }
    if (strcmp(stage, "Asoko") != 0) {
        randoSmokeCheck(kName, "boot stage sea:11 or Asoko:0", false);
        randoSmokeEnd(kName);
    }
    daPy_lk_c* link = daPy_getPlayerLinkActorClass();
    switch (sState) {
    case kSetup:
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, sSince++ % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            return;
        }
        sRoom = dComIfGp_roomControl_getStayNo();
        checkShipSetup();
        randoSmokeCheck(kName, "no countdown runs (0xC0 clear)", !sw(PCR_PS_SW_COUNTDOWN));
        link->debugPlace(kChestRoom, 0x7FFF);
        writef(STDERR_FILENO, "[cos] rando-smoke pirateship: frame %u: the player put in the chest room (room %d)\n",
               frames, sRoom);
        setDrivenPad(true, 0, 0, 0);
        sState = kPlace;
        sSince = 0;
        return;
    case kPlace:
        setDrivenPad(true, 0, 0, 0);
        if (++sSince < 10) {
            return;
        }
        writef(STDERR_FILENO, "[cos] rando-smoke pirateship: player at (%.0f %.0f %.0f); 0xC2 %d 0xC3 %d\n",
               link->current.pos.x, link->current.pos.y, link->current.pos.z, (int)sw(PCR_PS_SW_NO_COUNTDOWN),
               (int)sw(PCR_PS_SW_IN_CHEST_ROOM));
        if (pc_rando()) {
            randoSmokeCheck(kName, "SW_C00 sets 0xC3 in the chest room, the NOR SwOp sets 0xC2",
                            sw(PCR_PS_SW_IN_CHEST_ROOM) && sw(PCR_PS_SW_NO_COUNTDOWN));
        }
        sState = kWait;
        sSince = 0;
        return;
    case kWait: {
        setDrivenPad(true, 0, 0, 0);
        sSince++;
        if (dComIfGp_event_runCheck()) {
            dEvt_control_c* evc = g_dComIfG_gameInfo.play.getEvent();
            dEvDtEvent_c* ev = dComIfGp_getPEvtManager()->getEventData(evc->mEventId);
            const char* name = ev != nullptr ? ev->getName() : "?";
            writef(STDERR_FILENO, "[cos] rando-smoke pirateship: frame %u: event \"%s\" after %u frames in the room\n",
                   frames, name, sSince);
            if (!pc_rando()) {
                randoSmokeCheck(kName, "vanilla: no event in the chest room", false);
                randoSmokeEnd(kName);
            }
            randoSmokeCheck(kName, "the event is AryllOpensDoor", strcmp(name, PCR_PS_EVENT_NAME) == 0);
            randoSmokeCheck(kName, "it starts after the SwOp's delay (150 frames)", sSince >= PCR_PS_ARYLL_DELAY - 20);
            sState = kRun;
            sSince = 0;
            return;
        }
        if (sSince > kEventWait) {
            if (pc_rando()) {
                randoSmokeCheck(kName, "AryllOpensDoor starts while the player is trapped", false);
            } else {
                randoSmokeCheck(kName, "vanilla: no event in the chest room, 0xC4 clear", !sw(PCR_PS_SW_DOOR_OPEN));
            }
            randoSmokeEnd(kName);
        }
        return;
    }
    case kRun:
        sSince++;
        if (!dComIfGp_event_runCheck()) {
            setDrivenPad(false, 0, 0, 0);
            writef(STDERR_FILENO, "[cos] rando-smoke pirateship: AryllOpensDoor ended after %u frames; talks", sSince);
            for (int m : sTalks) {
                writef(STDERR_FILENO, " %d", m);
            }
            writef(STDERR_FILENO, "\n");
            randoSmokeCheck(kName, "Aryll's two messages: 849 then 850",
                            sTalks.size() == 2 && sTalks[0] == PCR_PS_MSG_STUCK && sTalks[1] == PCR_PS_MSG_OPEN);
            sState = kAfter;
            sSince = 0;
            return;
        }
        logCuts(frames);
        setDrivenPad(true, sSince % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
        if (sSince > kEventFrames) {
            randoSmokeCheck(kName, "AryllOpensDoor ends", false);
            randoSmokeEnd(kName);
        }
        return;
    case kAfter:
        if (++sSince < 5) {
            return;
        }
        writef(STDERR_FILENO, "[cos] rando-smoke pirateship: after the event: 0xC1 %d 0xC4 %d\n", (int)sw(PCR_PS_SW_ARYLL_OPENED),
               (int)sw(PCR_PS_SW_DOOR_OPEN));
        randoSmokeCheck(kName, "the AND SwOp set 0xC1 and the OR SwOp set 0xC4 (the door opens)",
                        sw(PCR_PS_SW_ARYLL_OPENED) && sw(PCR_PS_SW_DOOR_OPEN));
        randoSmokeEnd(kName);
    }
}

} // namespace pc
