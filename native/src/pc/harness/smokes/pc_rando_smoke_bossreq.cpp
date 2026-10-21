// COS_SMOKE=rando COS_RANDO_SMOKE=bossreq (docs/RANDOMIZER_PLAN.md step R6.9): required bosses at
// runtime (features/rando/pc_rando_bossreq.h) in the Puppet Ganon stairway,
//   run.sh rando --stage GanonL:0:0 --env COS_RANDO_SMOKE=bossreq
//                [--env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_abc_full.json]
//                [--env COS_RANDO_FORCE=1]
//
// Once the play scene ran 30 frames:
//   1. the room file: with a seed that has required bosses (R0_6_abc_full.json: Gohma, Kalle Demos,
//      Gohdan, Helmaroc King, stage mask 0x3C) the patcher saw GanonL/Room0 and patched it; else
//      (vanilla, or COS_RANDO_FORCE with no seed) it never patched;
//   2. what the game loaded: one DngSw with that mask, two SwOps, the ALLdie on 0x26, a TagMsg for
//      851 and the NBOX10 wall (vanilla: ALLdie on 0x19, no DngSw, SwOp, TagMsg or akabe);
//   3. message 851 as the game reads it equals wwrando's on the disc's font
//      (fixtures/R6_9_bossreq_game.txt, R6_9_make_fixture.py --disc); vanilla: not in the overlay;
//   4. the sea chart: a J2DScreen built from fmapres.arc's f_map.blo, as dMenu_Fmap_c::_create
//      builds it, then the R6.9 hunk's pc_rando_bossreq_fmap_markers: the four required bosses'
//      marker panes are visible at wwrando's positions and rotations (fixture
//      R6_9_wwrando_bossreq.txt case abc_full: 2 (-250,-188), 3 (-30,-20), 5 (25,-132),
//      7 (-140,148), all 55 degrees) and the other four stay hidden; without required bosses the
//      call changes nothing;
//   5. rando with required bosses only, the door: switches 0x19 (door), 0x25, 0x26 start clear and
//      the NAND SwOp sets 0xE5 (the TagMsg's spawn switch). The smoke sets 0x26 (what ALLdie sets
//      once the room's enemies are dead): 0x19 stays clear. Then it marks the required bosses
//      dead (dSv_memBit_c STAGE_BOSS_ENEMY of save stages 2-5) one by one, 30 frames apart: 0x19
//      stays clear until the last, then DngSw sets 0x25 and the AND SwOp sets 0x19; 30 frames later
//      DngSw is gone and 0x19 still set.
// Exit 0 when all hold, else 1.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_bossreq.h"
#include "features/rando/pc_rando_msg.h"

#include "JSystem/J2DGraph/J2DScreen.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "bossreq";
constexpr const char* kFixture = "R6_9_bossreq_game.txt";
constexpr uint32_t kAbcMask = 0x3C; // R0_6_abc_full.json: stages 2 (FF), 3 (DRC), 4 (FW), 5 (TotG)
constexpr unsigned int kStep = 30;
constexpr unsigned int kWait = 120;

enum State { kStatic, kEnemies, kBosses, kOpen, kDone };
State sState = kStatic;
unsigned int sSince = 0;
int sRoom = 0;
int sNextStage = 0;
std::vector<int> sStages;

bool isSw(int sw) {
    return dComIfGs_isSwitch(sw, sRoom) != 0;
}

struct Procs {
    s16 name;
    int n;
    fopAc_ac_c* first;
};

void* collect(void* proc, void* data) {
    Procs* c = static_cast<Procs*>(data);
    if (fpcM_GetName(proc) == c->name) {
        if (c->n++ == 0) {
            c->first = static_cast<fopAc_ac_c*>(proc);
        }
    }
    return nullptr;
}

Procs procsNamed(s16 name) {
    Procs p = {name, 0, nullptr};
    fopAcM_Search(collect, &p);
    return p;
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

// text851 <box> <hex>
bool readFixture(int* box, std::string* text) {
    for (const std::string& d : fixtureDirs()) {
        FILE* f = std::fopen((d + "/" + kFixture).c_str(), "rb");
        if (f == nullptr) {
            continue;
        }
        static char line[4096];
        static char hex[4096];
        bool found = false;
        while (std::fgets(line, sizeof line, f) != nullptr) {
            line[std::strcspn(line, "\r\n")] = '\0';
            if (std::sscanf(line, "text851 %d %4095s", box, hex) == 2) {
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

// The text the game reads for msg_no (through the overlay), in wwrando's syntax, and its box type.
bool gameText(uint16_t msg_no, std::string* text, int* box) {
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    const char* bytes = get.getMessage(head);
    if (bytes == nullptr || get.getMesgNumber() != msg_no) {
        return false;
    }
    std::vector<char> buf(4096);
    const size_t n = pc_rando_msg_decode((const uint8_t*)bytes, 0x10000, buf.data(), buf.size());
    if (n >= buf.size()) {
        return false;
    }
    text->assign(buf.data(), n);
    *box = get.getMesgEntry(head).mTextboxType;
    return true;
}

bool required() {
    return pc_rando() && pc_rando_bossreq_bosses_of(pc_rando_seed(), nullptr) > 0;
}

void checkRoom() {
    PcRandoBossreqStatus st;
    pc_rando_bossreq_status(&st);
    writef(STDERR_FILENO, "[cos] rando-smoke bossreq: status seen %d applied %d result %d mask 0x%X; stay room %d\n",
           st.seen, st.applied, st.result, (unsigned)st.mask, (int)dComIfGp_roomControl_getStayNo());
    randoSmokeCheck(kName, "booted in GanonL room 0",
                    strcmp(dComIfGp_getStartStageName(), "GanonL") == 0 && dComIfGp_roomControl_getStayNo() == 0);
    const Procs dngsw = procsNamed(fpcNm_DUNGEON_FLAG_SW_e);
    const Procs swop = procsNamed(fpcNm_SWITCH_OP_e);
    const Procs alldie = procsNamed(fpcNm_ALLDIE_e);
    const Procs tagmsg = procsNamed(fpcNm_TAG_MSG_e);
    const Procs akabe = procsNamed(fpcNm_Obj_Akabe_e);
    const u32 alldieSw = alldie.first != nullptr ? (fopAcM_GetParam(alldie.first) >> 8) & 0xFF : 0xFFFFFFFFu;
    writef(STDERR_FILENO,
           "[cos] rando-smoke bossreq: %d DngSw, %d SwOp, %d ALLdie (switch 0x%X), %d TagMsg, %d akabe\n", dngsw.n,
           swop.n, alldie.n, (unsigned)alldieSw, tagmsg.n, akabe.n);
    if (!required()) {
        randoSmokeCheck(kName, "no required bosses: the room was never patched", st.seen == 0 && st.applied == 0);
        randoSmokeCheck(kName, "no required bosses: ALLdie opens the door (0x19)", alldie.n == 1 && alldieSw == 0x19);
        randoSmokeCheck(kName, "no required bosses: no DngSw, SwOp, TagMsg or akabe",
                        dngsw.n == 0 && swop.n == 0 && tagmsg.n == 0 && akabe.n == 0);
        return;
    }
    randoSmokeCheck(kName, "GanonL/Room0 patched once", st.seen == 1 && st.applied == 1 && st.result == 0);
    randoSmokeCheck(kName, "stage mask 0x3C (FF, DRC, FW, TotG)", st.mask == kAbcMask);
    randoSmokeCheck(kName, "one DngSw for the mask, flag 3, switch 0x25",
                    dngsw.n == 1 && fopAcM_GetParam(dngsw.first) == ((3u << 24) | (kAbcMask << 8) | 0x25));
    randoSmokeCheck(kName, "two SwOps", swop.n == 2);
    randoSmokeCheck(kName, "ALLdie sets 0x26", alldie.n == 1 && alldieSw == PCR_BR_SW_ENEMIES_DEAD);
    randoSmokeCheck(kName, "the NBOX10 wall", akabe.n == 1);
}

void checkMessage() {
    std::string text, want;
    int box = -1, wantBox = -1;
    const bool have = gameText(PCR_BR_MSG, &text, &box);
    const PcRandoMsg* m = pc_rando_msg_find(PCR_BR_MSG);
    if (!required()) {
        randoSmokeCheck(kName, "no required bosses: 851 not in the overlay", m == nullptr);
        return;
    }
    if (!randoSmokeCheck(kName, "fixture R6_9_bossreq_game.txt read", readFixture(&wantBox, &want))) {
        return;
    }
    writef(STDERR_FILENO, "[cos] rando-smoke bossreq: 851 box %d: %s\n", box, text.c_str());
    randoSmokeCheck(kName, "851 in the overlay", m != nullptr);
    randoSmokeCheck(kName, "851 the game reads = wwrando's on the disc's font", have && text == want && box == wantBox);
}

void checkMarkers() {
    JKRArchive* arc = dComIfGp_getFmapResArchive();
    if (!randoSmokeCheck(kName, "fmapres.arc mounted", arc != nullptr)) {
        return;
    }
    J2DScreen* scrn = new J2DScreen();
    scrn->set("f_map.blo", arc);
    struct Before {
        bool vis;
        f32 x, y, rot;
    } before[PCR_BR_MARKERS];
    for (int i = 0; i < PCR_BR_MARKERS; i++) {
        J2DPane* p = scrn->search(pc_rando_bossreq_marker_tag(i));
        before[i] = p != nullptr ? Before{p->isVisible(), p->getBounds().i.x, p->getBounds().i.y, p->getRotate()}
                                 : Before{true, -1, -1, -1};
    }
    const int set = pc_rando_bossreq_fmap_markers(scrn);
    struct Want {
        int index, x, y;
    };
    static const Want kWant[4] = {{2, -250, -188}, {3, -30, -20}, {5, 25, -132}, {7, -140, 148}};
    char what[160];
    for (int i = 0; i < PCR_BR_MARKERS; i++) {
        J2DPane* p = scrn->search(pc_rando_bossreq_marker_tag(i));
        if (!randoSmokeCheck(kName, "the quest marker pane exists", p != nullptr)) {
            continue;
        }
        const Want* w = nullptr;
        for (const Want& k : kWant) {
            if (k.index == i && required()) {
                w = &k;
            }
        }
        writef(STDERR_FILENO, "[cos] rando-smoke bossreq: marker %d: visible %d -> %d, (%g, %g) -> (%g, %g), rot %g -> %g\n",
               i, (int)before[i].vis, (int)p->isVisible(), before[i].x, before[i].y, p->getBounds().i.x,
               p->getBounds().i.y, before[i].rot, p->getRotate());
        if (w != nullptr) {
            snprintf(what, sizeof what, "marker %d shown at (%d, %d), 55 degrees", i, w->x, w->y);
            randoSmokeCheck(kName, what,
                            p->isVisible() && p->getBounds().i.x == (f32)w->x && p->getBounds().i.y == (f32)w->y &&
                                p->getRotate() == 55.0f);
        } else {
            snprintf(what, sizeof what, "marker %d unchanged and hidden", i);
            randoSmokeCheck(kName, what,
                            !p->isVisible() && !before[i].vis && p->getBounds().i.x == before[i].x &&
                                p->getBounds().i.y == before[i].y && p->getRotate() == before[i].rot);
        }
    }
    randoSmokeCheck(kName, required() ? "4 markers set" : "no marker set", set == (required() ? 4 : 0));
    delete scrn;
}

void markNextBossDead() {
    const int stage = sStages[(size_t)sNextStage++];
    writef(STDERR_FILENO, "[cos] rando-smoke bossreq: frame %u: stage %d's boss dead\n", sSince, stage);
    g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit().onStageBossEnemy();
}

} // namespace

void randoSmokeBossreq(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(30)) {
        return;
    }
    switch (sState) {
    case kStatic: {
        sRoom = dComIfGp_roomControl_getStayNo();
        checkRoom();
        checkMessage();
        checkMarkers();
        if (!required()) {
            randoSmokeEnd(kName);
        }
        randoSmokeCheck(kName, "0x19, 0x25 and 0x26 start clear",
                        !isSw(PCR_BR_SW_DOOR) && !isSw(PCR_BR_SW_BOSSES_DEAD) && !isSw(PCR_BR_SW_ENEMIES_DEAD));
        randoSmokeCheck(kName, "the NAND SwOp set 0xE5 (bosses not all dead)", isSw(PCR_BR_SW_BOSSES_ALIVE));
        for (int s = 0; s < 16; s++) {
            if (kAbcMask & (1u << s)) {
                sStages.push_back(s);
                randoSmokeCheck(kName, "the required bosses start alive",
                                !g_dComIfG_gameInfo.save.getSavedata().getSave(s).getBit().isStageBossEnemy());
            }
        }
        writef(STDERR_FILENO, "[cos] rando-smoke bossreq: room's enemies dead (0x26 on)\n");
        dComIfGs_onSwitch(PCR_BR_SW_ENEMIES_DEAD, sRoom);
        sSince = 0;
        sState = kEnemies;
        return;
    }
    case kEnemies:
        if (++sSince < kStep * 2) {
            return;
        }
        randoSmokeCheck(kName, "enemies dead alone leave the door shut (0x19 clear)", !isSw(PCR_BR_SW_DOOR));
        sSince = 0;
        markNextBossDead();
        sState = kBosses;
        return;
    case kBosses:
        if (++sSince % kStep != 0) {
            return;
        }
        if (sNextStage < (int)sStages.size()) {
            randoSmokeCheck(kName, "with a required boss alive: 0x25 and 0x19 clear, DngSw there",
                            !isSw(PCR_BR_SW_BOSSES_DEAD) && !isSw(PCR_BR_SW_DOOR) &&
                                procsNamed(fpcNm_DUNGEON_FLAG_SW_e).n == 1);
            markNextBossDead();
            if (sNextStage == (int)sStages.size()) {
                sSince = 0;
                sState = kOpen;
            }
        }
        return;
    case kOpen:
        sSince++;
        if (isSw(PCR_BR_SW_DOOR)) {
            writef(STDERR_FILENO, "[cos] rando-smoke bossreq: 0x19 set %u frames after the last boss\n", sSince);
            randoSmokeCheck(kName, "all required bosses dead: DngSw set 0x25", isSw(PCR_BR_SW_BOSSES_DEAD));
            randoSmokeCheck(kName, "the AND SwOp opened the door (0x19)", true);
            sSince = 0;
            sState = kDone;
            return;
        }
        if (sSince > kWait) {
            randoSmokeCheck(kName, isSw(PCR_BR_SW_BOSSES_DEAD) ? "the AND SwOp opened the door (0x19)"
                                                               : "all required bosses dead: DngSw set 0x25",
                            false);
            randoSmokeEnd(kName);
        }
        return;
    case kDone:
        // fopAcM_delete takes effect a few frames later.
        if (++sSince < kStep) {
            return;
        }
        randoSmokeCheck(kName, "DngSw deleted itself", procsNamed(fpcNm_DUNGEON_FLAG_SW_e).n == 0);
        randoSmokeCheck(kName, "the door stays open (0x19)", isSw(PCR_BR_SW_DOOR));
        randoSmokeEnd(kName);
    }
}

} // namespace pc
