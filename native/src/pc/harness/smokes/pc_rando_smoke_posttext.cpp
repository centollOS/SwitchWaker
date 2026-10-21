// COS_SMOKE=rando COS_RANDO_SMOKE=posttext (docs/RANDOMIZER_PLAN.md step R3.21): the seed-dependent
// message texts in a booted game, e.g. run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=posttext.
// Once the play scene has run 10 frames:
//   1. while the file is not randomized (a plain boot), none of the step's messages is in the overlay
//      and the game reads KoRL's 3443 from the BMG;
//   2. the placement of native/check/rando/fixtures/R3_21_posttext_game.txt (its P line, the
//      `rotation` placement R0_6_rotation.json) is loaded and activated as a slot does
//      (pc_rando_set_loc_items, pc_rando_set_active); when the run is already randomized (a debug
//      boot with COS_RANDO_PLACEMENT=<that file>), the active seed is used instead;
//   3. every message of the fixture, read back through the game's fopMsgM_msgGet_c (so through the
//      R0.12 overlay hunks) and decoded to wwrando's syntax, equals the text wwrando's tweaks.py writes
//      for the same placement on the real BMG and font (native/tools/rando/make_posttext_fixture.py
//      --disc): byte for byte (T lines) or by MD5 where the text keeps vanilla BMG text (H lines);
//   4. fopMsgM_messageGet (menus, shop names) gives the overlay text of the shop description 3906;
//   5. pc_rando_deactivate (not forced): the records are gone and 3443 reads the BMG again.
// Exit 0 when all hold, else 1. The fixture is found as the wrap smoke finds its own (COS_RANDO_FIXTURES
// overrides).
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_posttext.h"
#include "features/rando/pc_rando_rng.h"
#include "features/rando/pc_rando_seedfile.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_msg_mng.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "posttext";
constexpr const char* kFixture = "R3_21_posttext_game.txt";

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

struct Expect {
    uint16_t msg_no;
    bool md5; // `want` is an MD5 hex digest
    std::string want;
};

bool readFixture(std::string* dir, std::string* placement, std::vector<Expect>* out) {
    for (const std::string& d : fixtureDirs()) {
        FILE* f = std::fopen((d + "/" + kFixture).c_str(), "rb");
        if (f == nullptr) {
            continue;
        }
        *dir = d;
        static char line[16384];
        while (std::fgets(line, sizeof line, f) != nullptr) {
            line[std::strcspn(line, "\r\n")] = '\0';
            char kind = line[0];
            if (kind == 'P' && line[1] == ' ') {
                *placement = line + 2;
            } else if ((kind == 'T' || kind == 'H') && line[1] == ' ') {
                char* end = nullptr;
                const long no = std::strtol(line + 2, &end, 10);
                if (end != nullptr && *end == ' ') {
                    out->push_back(Expect{(uint16_t)no, kind == 'H', kind == 'H' ? std::string(end + 1) : unhex(end + 1)});
                }
            }
        }
        std::fclose(f);
        return !placement->empty() && !out->empty();
    }
    return false;
}

// The game's text of msg_no in wwrando's syntax, read as the talk and item-get code reads it.
bool gameText(uint16_t msg_no, std::string* text) {
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    const char* bytes = get.getMessage(head);
    if (bytes == nullptr || get.getMesgNumber() != msg_no) {
        return false;
    }
    std::vector<char> buf(16384);
    const size_t n = pc_rando_msg_decode((const uint8_t*)bytes, 0x10000, buf.data(), buf.size());
    if (n >= buf.size()) {
        return false;
    }
    text->assign(buf.data(), n);
    return true;
}

std::string md5Hex(const std::string& s) {
    uint8_t d[16];
    char hex[33];
    pc::rando::md5(s.data(), s.size(), d);
    pc::rando::md5Hex(d, hex);
    return hex;
}

PcRandoSeed sSeed;

void run() {
    const int forced = pc_rando_forced();
    std::string dir, placement;
    std::vector<Expect> expect;
    if (!randoSmokeCheck(kName, "fixture read", readFixture(&dir, &placement, &expect))) {
        return;
    }
    writef(STDERR_FILENO, "[cos] rando-smoke posttext: %d messages, placement %s\n", (int)expect.size(),
           placement.c_str());

    // 1. not randomized
    std::string korlVanilla;
    if (pc_rando() == 0) {
        bool none = true;
        for (uint16_t n : pcr::postTextMessageNumbers()) {
            none = none && pc_rando_msg_find(n) == nullptr;
        }
        randoSmokeCheck(kName, "inactive: no post text in the overlay", none);
        randoSmokeCheck(kName, "inactive: 3443 from the BMG",
                        gameText(3443, &korlVanilla) && korlVanilla.find("Ganondorf") == std::string::npos);
    }

    // 2. activate the placement (unless a debug boot already did)
    if (pc_rando_seed() == nullptr) {
        std::string err;
        const std::string path = dir + "/" + placement;
        if (!randoSmokeCheck(kName, "placement loaded", pc_rando_seed_load_file(path.c_str(), &sSeed, &err))) {
            writef(STDERR_FILENO, "[cos] rando-smoke posttext: %s: %s\n", path.c_str(), err.c_str());
            return;
        }
        pc_rando_set_loc_items(sSeed.locations, PC_RANDO_SEED_LOCATIONS);
        pc_rando_set_active(&sSeed, nullptr);
    } else {
        writef(STDERR_FILENO, "[cos] rando-smoke posttext: using the active seed \"%s\"\n", pc_rando_seed()->seed.c_str());
    }

    // 3. every message against wwrando
    int good = 0, inOverlay = 0;
    for (const Expect& e : expect) {
        std::string text;
        const bool have = gameText(e.msg_no, &text);
        const bool ok = have && (e.md5 ? md5Hex(text) == e.want : text == e.want);
        inOverlay += pc_rando_msg_find(e.msg_no) != nullptr;
        if (ok) {
            good++;
        } else {
            writef(STDERR_FILENO, "[cos] rando-smoke posttext: message %u differs: \"%.300s\"%s%s\n", (unsigned)e.msg_no,
                   text.c_str(), e.md5 ? " md5 " : "\n  want \"", e.md5 ? md5Hex(text).c_str() : e.want.c_str());
            if (!e.md5) {
                writef(STDERR_FILENO, "\"\n");
            }
        }
    }
    writef(STDERR_FILENO, "[cos] rando-smoke posttext: %d of %d messages as wwrando, %d in the overlay\n", good,
           (int)expect.size(), inOverlay);
    randoSmokeCheck(kName, "every message as wwrando writes it", good == (int)expect.size());
    randoSmokeCheck(kName, "every message from the overlay", inOverlay == (int)expect.size());

    // 4. the plain-text reader (menus, the shop's item name line)
    char buf[1024];
    fopMsgM_messageGet(buf, 3906);
    const PcRandoSeed* seed = pc_rando_seed();
    const std::string name =
        seed != nullptr && seed->location_item_names.size() == PC_RANDO_SEED_LOCATIONS
            ? seed->location_item_names[PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM]
            : std::string();
    writef(STDERR_FILENO, "[cos] rando-smoke posttext: messageGet 3906: \"%s\"\n", buf);
    randoSmokeCheck(kName, "messageGet 3906 names the placed item",
                    !name.empty() && std::string(buf) == name + "  20 Rupees");

    // 5. off again
    pc_rando_deactivate();
    if (!forced) {
        bool none = true;
        for (uint16_t n : pcr::postTextMessageNumbers()) {
            none = none && pc_rando_msg_find(n) == nullptr;
        }
        randoSmokeCheck(kName, "deactivate drops the post texts", none);
        std::string korl;
        // (a debug boot starts randomized: then only "not the randomizer's text" can be checked)
        randoSmokeCheck(kName, "deactivate: 3443 from the BMG again",
                        gameText(3443, &korl) &&
                            (korlVanilla.empty() ? korl.find("Ganondorf") == std::string::npos : korl == korlVanilla));
    }
}

} // namespace

void randoSmokePosttext(unsigned int frames) {
    (void)frames;
    if (!randoSmokeInPlay(10)) {
        return;
    }
    run();
    randoSmokeEnd(kName);
}

} // namespace pc
