// COS_SMOKE=rando COS_RANDO_SMOKE=hoho (docs/RANDOMIZER_PLAN.md step R5.3): hint placement at runtime
// (game_hooks/pc_rando_hooks_hints.cpp). Boot on the boat:
//   run.sh rando --preset sailing --uncapped --env COS_RANDO_SMOKE=hoho
//                [--env COS_HINTS_SEED=R5_3_Heavy5.json] [--env COS_HINTS_VANILLA=1]
//                [--env COS_HINTS_ROOM=<a Ho Ho sea room, default 44>]
// The expectations are native/check/rando/fixtures/R5_3_hints_game.txt (R5_3_make_fixture.py --disc:
// wwrando's HintsRandomizer._save on the disc's BMG, font and sea rooms for our generator's
// placements), one section per placement (P line); COS_HINTS_SEED picks one (default the first,
// R5_3_TestSeed1.json). Once the play scene has run 10 frames:
//   1. not randomized yet: no hint message in the overlay, the Big Octo fairy hunk is off;
//   2. the placement is activated as a loaded randomized slot does (pc_rando_slot_activate), unless
//      COS_HINTS_VANILLA=1, and the boat sails to sea room 44 (or COS_HINTS_ROOM) again (point 100), so the stage loads
//      under the seed as after a file load (the hint texts are built at the first archive load after
//      the first activation: pc_rando_hooks_hints.cpp); 30 frames after arriving:
//   3. the disc BMG's text box type of every hint message (B lines) is the one the game reads
//      (fopMsgM_msgGet_c::getMesgEntry, what buildHintTexts wraps with);
//   4. every message wwrando writes (T lines), read back through the game's fopMsgM_msgGet_c (so the
//      R0.12 overlay hunks) and decoded to wwrando's syntax, is wwrando's text byte for byte (KoRL's
//      3443 too, over R3.21's text); every other hint message is the BMG's own;
//   5. flexible_hint_locations: daBigelf_c::getMsg on message 0x2EEE gives the hint 0x2EEF without
//      the Hero's Bow (the hunk; vanilla: 0x2EEE), and with it as in vanilla;
//   6. sea/Room44's room.dzr (or COS_HINTS_ROOM's): the Ho Ho patcher turned every Ah actor of the
//      room (room 44: the 4 of layers 4-7) to the y_rot of the R lines, or left them alone when his
//      hints name no island, and a live Ah actor (when the layer has one) faces that way.
// Vanilla (COS_HINTS_VANILLA=1): nothing in the overlay, getMsg is vanilla, the room not patched.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_gen_hints.h"
#include "features/rando/pc_rando_hint_place.h"
#include "features/rando/pc_rando_hint_text.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"

#include "d/actor/d_a_bigelf.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <dolphin/pad.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "hoho";
constexpr const char* kFixture = "R5_3_hints_game.txt";
int kRoom = 44; // COS_HINTS_ROOM; 44 (Outset): Ho Ho index 2, on layers 4-7
constexpr int kSeaPoint = 100;
constexpr unsigned int kArriveFrames = 1200;
constexpr unsigned int kSettleFrames = 30;
constexpr unsigned int kTalkFrames = 30;

std::string fixtureDir() {
    const char* env = std::getenv("COS_RANDO_FIXTURES");
    if (env != nullptr && env[0] != '\0') {
        return env;
    }
    // .../native/src/pc/harness/smokes/<this file> -> .../native/check/rando/fixtures
    std::string dir = __FILE__;
    for (int up = 0; up < 5; up++) {
        const size_t slash = dir.find_last_of('/');
        if (slash == std::string::npos) {
            return std::string();
        }
        dir.resize(slash);
    }
    dir += "/check/rando/fixtures";
    // A relative __FILE__ (ccache's base_dir) is relative to the repository root; run.sh runs the
    // game in <root>/build/runs/<run>.
    const char* run = std::getenv("COS_RUN_DIR");
    if (dir[0] != '/' && run != nullptr && run[0] != '\0') {
        const std::string fromRun = std::string(run) + "/../../" + dir;
        if (FILE* f = std::fopen((fromRun + "/" + kFixture).c_str(), "rb")) {
            std::fclose(f);
            return fromRun;
        }
    }
    return dir;
}

std::string unhex(const std::string& h) {
    std::string out;
    if (h == "-") {
        return out;
    }
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char)std::strtol(h.substr(i, 2).c_str(), nullptr, 16));
    }
    return out;
}

struct Rot {
    int room;
    std::string chunk;
    int index;
    uint16_t yRot;
};

struct Section {
    std::string placement;
    std::vector<std::pair<uint16_t, uint8_t>> types;
    std::vector<std::pair<uint16_t, std::string>> texts;
    std::vector<Rot> rots;
};

bool readFixture(const std::string& want, Section* out) {
    FILE* f = std::fopen((fixtureDir() + "/" + kFixture).c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    static char line[65536];
    bool in = false, found = false;
    while (std::fgets(line, sizeof line, f) != nullptr) {
        line[std::strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[1] != ' ') {
            continue;
        }
        const char kind = line[0];
        if (kind == 'P') {
            if (found) {
                break;
            }
            in = want.empty() || want == line + 2;
            if (in) {
                found = true;
                out->placement = line + 2;
            }
            continue;
        }
        if (!in) {
            continue;
        }
        char* end = nullptr;
        if (kind == 'B') {
            const long no = std::strtol(line + 2, &end, 10);
            out->types.push_back({(uint16_t)no, (uint8_t)std::strtol(end, nullptr, 10)});
        } else if (kind == 'T') {
            const long no = std::strtol(line + 2, &end, 10);
            if (end != nullptr && *end == ' ') {
                out->texts.push_back({(uint16_t)no, unhex(end + 1)});
            }
        } else if (kind == 'R') {
            char chunk[8] = {};
            int room = 0, index = 0;
            unsigned y = 0;
            if (std::sscanf(line + 2, "%d %7s %d %u", &room, chunk, &index, &y) == 4) {
                out->rots.push_back({room, chunk, index, (uint16_t)y});
            }
        }
    }
    std::fclose(f);
    return found && !out->texts.empty();
}

// The game's text of msg_no in wwrando's syntax and its entry's text box type, as the talk code reads them.
bool gameMessage(uint16_t msg_no, std::string* text, uint8_t* type) {
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    const char* bytes = get.getMessage(head);
    if (bytes == nullptr || get.getMesgNumber() != msg_no) {
        return false;
    }
    if (type != nullptr) {
        *type = get.getMesgEntry(head).mTextboxType;
    }
    if (text != nullptr) {
        std::vector<char> buf(65536);
        const size_t n = pc_rando_msg_decode((const uint8_t*)bytes, 0x10000, buf.data(), buf.size());
        if (n >= buf.size()) {
            return false;
        }
        text->assign(buf.data(), n);
    }
    return true;
}

enum State { kOff, kArrive, kSettle, kDone };
State sState = kOff;
bool sVanilla = false;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
fpc_ProcID sScene = fpcM_ERROR_PROCESS_ID_e;
Section sSection;
PcRandoSeed sSeed;
std::map<uint16_t, std::string> sVanillaText;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

fpc_ProcID playSceneId() {
    void* s = fpcM_Search(isPlayScene, nullptr);
    return s != nullptr ? fpcM_GetID(s) : fpcM_ERROR_PROCESS_ID_e;
}

bool readText(const std::string& path, std::string* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        out->append(buf, n);
    }
    std::fclose(f);
    return true;
}

// daBigelf_c::getMsg on message 0x2EEE with or without the Hero's Bow as the current bow (on a fairy
// object that is never constructed: getMsg reads only mCurrentMessageId).
u32 octoFairyMsg(bool heroBow) {
    const u8 was = dComIfGs_getItem(dInvSlot_BOW_e);
    dComIfGs_setItem(dInvSlot_BOW_e, heroBow ? dItemNo_BOW_e : dItemNo_NONE_e);
    alignas(daBigelf_c) static unsigned char storage[sizeof(daBigelf_c)];
    std::memset(storage, 0, sizeof storage);
    daBigelf_c* fairy = reinterpret_cast<daBigelf_c*>(storage);
    fairy->mCurrentMessageId = 0x2EEE;
    const u32 msg = fairy->getMsg();
    dComIfGs_setItem(dInvSlot_BOW_e, was);
    return msg;
}

// 1-5, right after the start (and activation).
void checkMessages(bool before) {
    char what[200];
    const std::vector<uint16_t> all = pcr::hintTextMessageNumbers();
    if (before) {
        bool none = true;
        for (uint16_t n : all) {
            if (n != 3443) { // R3.21's text may be there under rando; not before activation
                none = none && pc_rando_msg_find(n) == nullptr;
            }
            std::string t;
            if (gameMessage(n, &t, nullptr)) {
                sVanillaText[n] = t;
            }
        }
        randoSmokeCheck(kName, "not randomized: no hint message in the overlay", pc_rando() == 0 && none);
        randoSmokeCheck(kName, "not randomized: the octo fairy hunk is off", pc_rando_hints_flexible() == 0);
        randoSmokeCheck(kName, "not randomized: getMsg keeps 0x2EEE without the Hero's Bow", octoFairyMsg(false) == 0x2EEE);
        return;
    }
    // 3. text box types
    int goodTypes = 0;
    for (const auto& bt : sSection.types) {
        uint8_t type = 0xFF;
        if (gameMessage(bt.first, nullptr, &type) && type == bt.second) {
            goodTypes++;
        } else {
            writef(STDERR_FILENO, "[cos] rando-smoke hoho: message %u: text box type %u, the disc's %u\n",
                   (unsigned)bt.first, (unsigned)type, (unsigned)bt.second);
        }
    }
    std::snprintf(what, sizeof what, "%d/%d text box types as the disc's BMG", goodTypes, (int)sSection.types.size());
    randoSmokeCheck(kName, what, goodTypes == (int)sSection.types.size() && !sSection.types.empty());

    // 4. texts
    std::map<uint16_t, const std::string*> want;
    for (const auto& t : sSection.texts) {
        want[t.first] = &t.second;
    }
    int good = 0, inOverlay = 0, keptVanilla = 0, others = 0;
    for (uint16_t n : all) {
        std::string text;
        const bool have = gameMessage(n, &text, nullptr);
        auto w = want.find(n);
        if (w != want.end()) {
            inOverlay += pc_rando_msg_find(n) != nullptr;
            if (have && text == *w->second) {
                good++;
            } else {
                writef(STDERR_FILENO, "[cos] rando-smoke hoho: message %u differs:\n  got  \"%.600s\"\n  want \"%.600s\"\n",
                       (unsigned)n, text.c_str(), w->second->c_str());
            }
        } else if (n != 3443) {
            others++;
            auto v = sVanillaText.find(n);
            if (v != sVanillaText.end() && have && text == v->second && pc_rando_msg_find(n) == nullptr) {
                keptVanilla++;
            } else {
                writef(STDERR_FILENO, "[cos] rando-smoke hoho: message %u is not the BMG's\n", (unsigned)n);
            }
        }
    }
    std::snprintf(what, sizeof what, "%d/%d messages as wwrando writes them, %d from the overlay", good,
                  (int)sSection.texts.size(), inOverlay);
    randoSmokeCheck(kName, what, good == (int)sSection.texts.size() && inOverlay == good);
    std::snprintf(what, sizeof what, "%d/%d other hint messages are the BMG's", keptVanilla, others);
    randoSmokeCheck(kName, what, keptVanilla == others);
    if (want.count(3443) != 0) {
        randoSmokeCheck(kName, "KoRL 3443 is the hint text (over R3.21's)", good == (int)sSection.texts.size());
    }

    // 5. flexible_hint_locations
    randoSmokeCheck(kName, "the seed has an octo fairy hint: the hunk is on", pc_rando_hints_flexible() == 1);
    randoSmokeCheck(kName, "getMsg 0x2EEE -> 0x2EEF without the Hero's Bow", octoFairyMsg(false) == 0x2EEF);
    randoSmokeCheck(kName, "getMsg 0x2EEE -> 0x2EEF with the Hero's Bow", octoFairyMsg(true) == 0x2EEF);
}

void checkRoom() {
    char what[200];
    int found = 0, turned = 0;
    uint16_t y = 0;
    const int status = pc_rando_hints_hoho_room(kRoom, &found, &turned, &y);
    if (sVanilla) {
        randoSmokeCheck(kName, "vanilla: the room not patched", status == PCR_HINTS_HOHO_NOT_RUN);
        randoSmokeCheck(kName, "vanilla: getMsg keeps 0x2EEE without the Hero's Bow", octoFairyMsg(false) == 0x2EEE);
        randoSmokeCheck(kName, "vanilla: no hint message in the overlay", pc_rando_msg_find(14003) == nullptr &&
                                                                               pc_rando_msg_find(837) == nullptr);
        return;
    }
    checkMessages(false);
    int expectN = 0;
    uint16_t expectY = 0;
    bool same = true;
    for (const Rot& r : sSection.rots) {
        if (r.room == kRoom) {
            if (expectN > 0 && r.yRot != expectY) {
                same = false;
            }
            expectY = r.yRot;
            expectN++;
        }
    }
    // He keeps his vanilla rotation when none of his hints names an island (look_island null).
    pcr::HintsResult h;
    std::string err;
    const int index = pcr::hohoIndexOfRoom(kRoom);
    const bool look = pcr::hintsFromJson(sSeed.hints, h, &err) && index >= 0 &&
                      (size_t)index < h.hohoLookIsland.size() && h.hohoLookIsland[index] >= 1;
    std::snprintf(what, sizeof what, "room %d: %d/%d Ah actors turned to y_rot %u (status %d; wwrando: %d at %u%s)",
                  kRoom, turned, found, (unsigned)y, status, expectN, (unsigned)expectY, look ? "" : ", vanilla");
    randoSmokeCheck(kName, what,
                    same && expectN > 0 &&
                        (look ? status == PCR_HINTS_HOHO_TURNED && found == expectN && turned == expectN && y == expectY
                              : status == PCR_HINTS_HOHO_VANILLA && turned == 0)); // not searched when vanilla
    fopAc_ac_c* ah = nullptr;
    fopAcM_SearchByName(fpcNm_NPC_AH_e, &ah);
    if (ah != nullptr) {
        std::snprintf(what, sizeof what, "the live Ho Ho faces y_rot %u (home %u, shape %u)", (unsigned)expectY,
                      (unsigned)(u16)ah->home.angle.y, (unsigned)(u16)ah->shape_angle.y);
        randoSmokeCheck(kName, what, (u16)ah->home.angle.y == expectY);
    } else {
        writef(STDERR_FILENO, "[cos] rando-smoke hoho: no live Ho Ho on this layer of room %d\n", kRoom);
    }
}

bool start() {
    const char* v = std::getenv("COS_HINTS_VANILLA");
    sVanilla = v != nullptr && std::strcmp(v, "1") == 0;
    if (const char* r = std::getenv("COS_HINTS_ROOM")) {
        kRoom = std::atoi(r);
    }
    const char* s = std::getenv("COS_HINTS_SEED");
    const std::string want = s != nullptr ? s : "";
    if (!randoSmokeCheck(kName, "fixture read", readFixture(want, &sSection))) {
        return false;
    }
    writef(STDERR_FILENO, "[cos] rando-smoke hoho: %s: %d texts, %d box types, %d Ah rows\n",
           sSection.placement.c_str(), (int)sSection.texts.size(), (int)sSection.types.size(), (int)sSection.rots.size());
    checkMessages(true);
    if (sVanilla) {
        return true;
    }
    std::string text, err;
    if (!randoSmokeCheck(kName, "placement read", readText(fixtureDir() + "/" + sSection.placement, &text)) ||
        !randoSmokeCheck(kName, "placement parsed", pc_rando_seed_parse(text, &sSeed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke hoho: %s\n", err.c_str());
        return false;
    }
    if (!randoSmokeCheck(kName, "seed activated", pc_rando_slot_activate(sSeed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke hoho: activate: %s\n", err.c_str());
        return false;
    }
    return true;
}

} // namespace

void randoSmokeHoho(unsigned int frames) {
    (void)frames;
    switch (sState) {
    case kOff:
        if (!randoSmokeInPlay(10)) {
            return;
        }
        if (!start()) {
            sState = kDone;
            randoSmokeEnd(kName);
        }
        sScene = playSceneId();
        sSince = 0;
        writef(STDERR_FILENO, "[cos] rando-smoke hoho: to sea room %d point %d\n", kRoom, kSeaPoint);
        dComIfGp_setNextStage("sea", (s16)kSeaPoint, (s8)kRoom);
        sState = kArrive;
        return;
    case kArrive: {
        const fpc_ProcID id = playSceneId();
        if (id == fpcM_ERROR_PROCESS_ID_e || id == sScene || dComIfGp_getPlayer(0) == nullptr ||
            std::strcmp(dComIfGp_getStartStageName(), "sea") != 0 || dComIfGp_roomControl_getStayNo() != kRoom) {
            if (++sSince > kArriveFrames) {
                randoSmokeCheck(kName, "the sea room came up again", false);
                sState = kDone;
                randoSmokeEnd(kName);
            }
            return;
        }
        sSince = 0;
        sState = kSettle;
        return;
    }
    case kSettle:
        if (dComIfGp_event_runCheck()) {
            setDrivenPad(true, ++sEventFrames % kTalkFrames < 2 ? PAD_BUTTON_A : 0, 0, 0);
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (++sSince < kSettleFrames) {
            return;
        }
        sState = kDone;
        setDrivenPad(false, 0, 0, 0);
        checkRoom();
        randoSmokeEnd(kName);
    case kDone:
        return;
    }
}

} // namespace pc
