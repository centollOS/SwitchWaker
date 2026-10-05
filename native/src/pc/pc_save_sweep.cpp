// Save/load sweep: a save made in a given situation, written through the game's own save code,
// then loaded back through the title and the file select in a second run and compared.
//
// COS_SMOKE=save-sweep (needs COS_BOOT_STAGE; COS_BOOT_PRESET / COS_BOOT_EVENTS / COS_BOOT_ITEMS set
// the situation), and the end of COS_SMOKE=item-sweep with COS_ITEM_SWEEP_SAVE=1: once the player is
// in the start room, no event runs (A through any opening talk) and kSettleFrames passed, the file
// is saved with the steps of the save screen (d_menu_save.cpp), in its order, minus its screens:
//   memCardCheck: the card status; no game file (CARD_STAT_RESTORE) -> memCardMakeGameFileSel:
//     dComIfGs_setInitDataToCard + mDoMemCdRWm_SetCheckSumGameData into files 1-3, mDoMemCd_Save,
//     memCardMakeGameFile/Check: mDoMemCd_SaveSync until done, dComIfGs_setNewFile(1), check again;
//     a game file (CARD_STAT_CREATE) -> mDoMemCd_Load, memCardDataLoadWait: mDoMemCd_LoadSync;
//   memCardDataSave: dComIfGs_exchangePlayerRecollectionData, dComIfGs_putSave(the stage's save
//     table), dComIfGs_setGameStartStage (the return place: the island / dungeon entrance / sea
//     room the game restarts at), mDoMemCdRWm_TestCheckSumGameData, dataWrite:
//     dComIfGs_setMemoryToCard into file dComIfGs_getDataNum, mDoMemCdRWm_SetCheckSumGameData,
//     mDoMemCd_Save, memCardDataSaveWait: mDoMemCd_SaveSync until done, then
//     dComIfGs_exchangePlayerRecollectionData, setMemCardCheckID, setNewFile(0), setNoFile(0).
// The card is <COS_RUN_DIR>/card/ (or COS_CARD_DIR). Then the GCI on disk must hold the bytes the
// game packed (copy 1 and copy 2), and <COS_RUN_DIR>/save_expect.txt gets the summary below.
// Exit 0, or 1 at the first failed step.
//
// COS_SMOKE=save-load (no COS_BOOT_STAGE; COS_CARD_DIR names the card folder of a save-sweep run):
// the real boot: START on the title (every kStartEvery frames) until the name scene runs, then A
// every kAEvery frames (the file select: file 1, start). When the PLAY scene first exists (the name
// scene loaded the file with dComIfGs_setCardToMemory and dComIfGs_gameStart asked for its return
// place), the state in memory is packed again (dComIfGs_setMemoryToCard) and compared region by
// region with file 1 of the card, read from the GCI on disk (only status B's save date, 8 bytes,
// differs: memory_to_card stamps it anew); <COS_RUN_DIR>/save_loaded.txt gets the summary. Then
// the start stage must be the save's return place and the player must stand in its room for
// kPlayFrames frames. Exit 0 when everything holds, else 1.
//
// Summary (save_expect.txt / save_loaded.txt): "return <stage> <room> <point>", "items <21 slot
// bytes>", "status <life> <max life> <rupees> <magic>", then "region <name> <hex>" for every part
// of the packed save (memory_to_card's order). native/tools/save_sweep.py runs both and diffs them.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <dolphin/pad.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kSettleFrames = 90;
constexpr unsigned int kCardFrames = 30 * 20; // per card step
constexpr unsigned int kStartEvery = 60;
constexpr unsigned int kAEvery = 40;
constexpr unsigned int kNoPlayFrames = 30 * 120;
constexpr unsigned int kPlayFrames = 300;
constexpr uint32_t kGciHeaderSize = 0x40;
constexpr uint32_t kBlockSize = 0x2000;
constexpr uint32_t kSaveDataFiles = 0x8;
constexpr uint32_t kFileBlocks = 12;

struct Region {
    const char* name;
    uint32_t size;
};
// memory_to_card's parts, in order (d_save.cpp).
const Region kRegions[] = {
    {"status_a", sizeof(dSv_player_status_a_c)},
    {"status_b", sizeof(dSv_player_status_b_c)},
    {"return_place", sizeof(dSv_player_return_place_c)},
    {"item", sizeof(dSv_player_item_c)},
    {"get_item", sizeof(dSv_player_get_item_c)},
    {"item_record", sizeof(dSv_player_item_record_c)},
    {"item_max", sizeof(dSv_player_item_max_c)},
    {"bag_item", sizeof(dSv_player_bag_item_c)},
    {"get_bag_item", sizeof(dSv_player_get_bag_item_c)},
    {"bag_item_record", sizeof(dSv_player_bag_item_record_c)},
    {"collect", sizeof(dSv_player_collect_c)},
    {"map", sizeof(dSv_player_map_c)},
    {"info", sizeof(dSv_player_info_c)},
    {"config", sizeof(dSv_player_config_c)},
    {"priest", sizeof(dSv_player_priest_c)},
    {"status_c", sizeof(dSv_player_status_c_c) * dSv_player_c::PLAYER_STATUS_C_COUNT},
    {"memory", sizeof(dSv_memory_c) * dSv_save_c::STAGE_MAX},
    {"ocean", sizeof(dSv_ocean_c)},
    {"event", sizeof(dSv_event_c)},
    {"reserve", sizeof(dSv_reserve_c)},
};
constexpr uint32_t kStatusBDate = sizeof(dSv_player_status_a_c); // u64 at the start of status B

enum Mode { kModeOff, kModeSaveSweep, kModeLoad };
enum State {
    kOff,
    kWait,      // save-sweep: the player free and settled
    kCheck,     // memCardCheck
    kMakeWait,  // memCardMakeGameFile
    kLoadWait,  // memCardDataLoadWait
    kSave,      // memCardDataSave + dataWrite
    kSaveWait,  // memCardDataSaveWait
    kTitle,     // save-load: START until the name scene
    kName,      // A until the PLAY scene
    kPlay,      // the PLAY scene: wait for the player in the return room
    kDone,
};

Mode sMode = kModeOff;
State sState = kOff;
bool sChecked = false;
unsigned int sSince = 0;
unsigned int sEventFrames = 0;
const char* sWho = "save-sweep";
alignas(32) u8 sDataBuf[sizeof(card_gamedata) * 3];
alignas(32) u8 sRepacked[sizeof(card_gamedata) * 3];
u8 sGci[kGciHeaderSize + kFileBlocks * kBlockSize + 16];
char sReturnName[9];
int sReturnRoom = -1;
int sReturnPoint = -1;
int sNameMain = -1, sNameCard = -1, sNameDraw = -1;

void* findByName(void* proc, void* name) {
    return fpcM_GetName(proc) == *(s16*)name ? proc : nullptr;
}

base_process_class* findScene(s16 name) {
    return (base_process_class*)fpcM_Search(findByName, &name);
}

[[noreturn]] void failExit(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void failExit(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    writef(STDERR_FILENO, "[cos] %s: FAIL %s\n", sWho, buf);
    pc_exit(PC_EXIT_CHECK_FAILED);
}

void hex(int fd, const u8* p, uint32_t n) {
    char buf[2 * 0x400 + 1];
    uint32_t k = 0;
    for (uint32_t i = 0; i < n && k + 2 < sizeof(buf); i++) {
        static const char digits[] = "0123456789abcdef";
        buf[k++] = digits[p[i] >> 4];
        buf[k++] = digits[p[i] & 15];
    }
    buf[k] = '\0';
    writef(fd, "%s", buf);
}

// The summary of one packed file (card_gamedata, memory_to_card's layout).
void writeSummary(const char* fileName, const u8* file, const char* context) {
    int fd = openRunFile(fileName);
    if (fd < 0) {
        return;
    }
    writef(fd, "# %s\n", context);
    const dSv_player_return_place_c* rp =
        (const dSv_player_return_place_c*)(file + sizeof(dSv_player_status_a_c) + sizeof(dSv_player_status_b_c));
    writef(fd, "return %.8s %d %d\n", rp->mName, (int)rp->mRoomNo, (int)rp->mPoint);
    const u8* items = file + sizeof(dSv_player_status_a_c) + sizeof(dSv_player_status_b_c) +
                      sizeof(dSv_player_return_place_c);
    writef(fd, "items ");
    hex(fd, items, sizeof(dSv_player_item_c));
    writef(fd, "\n");
    const dSv_player_status_a_c* a = (const dSv_player_status_a_c*)file;
    writef(fd, "status life %d max %d rupees %d magic %d\n", (int)((dSv_player_status_a_c*)a)->getLife(),
           (int)((dSv_player_status_a_c*)a)->getMaxLife(), (int)((dSv_player_status_a_c*)a)->getRupee(),
           (int)((dSv_player_status_a_c*)a)->getMagic());
    uint32_t off = 0;
    for (const Region& r : kRegions) {
        writef(fd, "region %s ", r.name);
        if (off == kStatusBDate - 0 && strcmp(r.name, "status_b") == 0) {
            // The save date (stamped by every memory_to_card) is left out.
            writef(fd, "(date)");
            hex(fd, file + off + 8, r.size - 8);
        } else {
            hex(fd, file + off, r.size);
        }
        writef(fd, "\n");
        off += r.size;
    }
    close(fd);
}

// Reads the GCI into sGci; returns its size or 0.
size_t readGci() {
    const char* path = runCardGciPath();
    if (path == nullptr) {
        return 0;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        writef(STDERR_FILENO, "[cos] %s: cannot open %s: %s\n", sWho, path, strerror(errno));
        return 0;
    }
    size_t total = 0;
    for (;;) {
        ssize_t n = read(fd, sGci + total, sizeof(sGci) - total);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    close(fd);
    return total;
}

// File `slot` of copy `copy` (1 or 2) in sGci.
const u8* gciFile(int copy, int slot) {
    return sGci + kGciHeaderSize + copy * kBlockSize + kSaveDataFiles + slot * sizeof(card_gamedata);
}

// Compares two packed files region by region; logs each difference; returns their count.
int compareFiles(const char* what, const u8* got, const u8* want) {
    int diffs = 0;
    uint32_t off = 0;
    for (const Region& r : kRegions) {
        for (uint32_t i = 0; i < r.size; i++) {
            const uint32_t at = off + i;
            if (at >= kStatusBDate && at < kStatusBDate + 8) {
                continue; // the save date
            }
            if (got[at] != want[at]) {
                writef(STDERR_FILENO, "[cos] %s: %s: region %s differs at +0x%X (packed 0x%03X): 0x%02X, card 0x%02X\n",
                       sWho, what, r.name, i, at, got[at], want[at]);
                diffs++;
                break; // one line per region
            }
        }
        off += r.size;
    }
    return diffs;
}

void logReturnPlace(const char* when) {
    dSv_player_return_place_c& rp = g_dComIfG_gameInfo.save.getPlayer().getPlayerReturnPlace();
    snprintf(sReturnName, sizeof(sReturnName), "%.8s", rp.getName());
    sReturnRoom = rp.getRoomNo();
    sReturnPoint = rp.getPoint();
    writef(STDERR_FILENO, "[cos] %s: %s: return place %s room %d point %d\n", sWho, when, sReturnName, sReturnRoom,
           sReturnPoint);
}

void saveFrame(unsigned int frames) {
    ++sSince;
    const int slot = dComIfGs_getDataNum();
    switch (sState) {
    case kCheck: {
        const u32 status = mDoMemCd_getStatus(0);
        if (status == mDoMemCd_Ctrl_c::CARD_STAT_RESTORE) {
            // No game file on the card: memCardMakeGameFileSel's "yes".
            writef(STDERR_FILENO, "[cos] %s: frame %u: no game file on the card: creating it\n", sWho, frames);
            for (int i = 0; i < 3; i++) {
                dComIfGs_setInitDataToCard(sDataBuf, i);
                mDoMemCdRWm_SetCheckSumGameData(sDataBuf, i);
            }
            mDoMemCd_Save(sDataBuf, sizeof(sDataBuf), 0);
            sState = kMakeWait;
            sSince = 0;
        } else if (status == mDoMemCd_Ctrl_c::CARD_STAT_CREATE) {
            writef(STDERR_FILENO, "[cos] %s: frame %u: loading the card's game file\n", sWho, frames);
            mDoMemCd_setPictDataPtr(NULL);
            mDoMemCd_Load();
            sState = kLoadWait;
            sSince = 0;
        } else if (status != mDoMemCd_Ctrl_c::CARD_STAT_WAIT && status != 14) {
            failExit("card status %u (not ready, no game file or game file)", (unsigned)status);
        } else if (sSince > kCardFrames) {
            failExit("card status still %u after %u frames", (unsigned)status, sSince);
        }
        return;
    }
    case kMakeWait: {
        const s32 r = mDoMemCd_SaveSync();
        if (r == 1) {
            dComIfGs_setNewFile(1);
            sState = kCheck;
            sSince = 0;
        } else if (r == 2) {
            failExit("creating the game file: SaveSync 2");
        } else if (sSince > kCardFrames) {
            failExit("creating the game file: no result after %u frames", sSince);
        }
        return;
    }
    case kLoadWait: {
        const u32 r = mDoMemCd_LoadSync(sDataBuf, sizeof(sDataBuf), 0);
        if (r == 1) {
            sState = kSave;
            sSince = 0;
        } else if (r == 2) {
            failExit("loading the game file: LoadSync 2");
        } else if (sSince > kCardFrames) {
            failExit("loading the game file: no result after %u frames", sSince);
        }
        return;
    }
    case kSave: {
        const bool canSave = dComIfGs_getNewFile() == 1 ||
                             (dComIfGs_getNoFile() == 0 && dComIfGs_getMemCardCheckID() == mDoMemCd_getCardSerialNo());
        if (!canSave) {
            failExit("the save screen would refuse (new file %d, no file %d, card id check %d)",
                     (int)dComIfGs_getNewFile(), (int)dComIfGs_getNoFile(),
                     (int)(dComIfGs_getMemCardCheckID() == mDoMemCd_getCardSerialNo()));
        }
        dComIfGs_exchangePlayerRecollectionData();
        dComIfGs_putSave(dStage_stagInfo_GetSaveTbl(dComIfGp_getStage().getStagInfo()));
        dComIfGs_setGameStartStage();
        logReturnPlace("saving");
        if (!mDoMemCdRWm_TestCheckSumGameData(&sDataBuf[slot * sizeof(card_gamedata)])) {
            failExit("file %d on the card fails its checksum (the save screen says the data is damaged)", slot + 1);
        }
        if (dComIfGs_setMemoryToCard(sDataBuf, slot) == -1) {
            failExit("dComIfGs_setMemoryToCard: the save is too large");
        }
        mDoMemCdRWm_SetCheckSumGameData(sDataBuf, slot);
        mDoMemCd_Save(sDataBuf, sizeof(sDataBuf), 0);
        sState = kSaveWait;
        sSince = 0;
        return;
    }
    case kSaveWait: {
        const s32 r = mDoMemCd_SaveSync();
        if (r == 0) {
            if (sSince > kCardFrames) {
                failExit("writing the save: no result after %u frames", sSince);
            }
            return;
        }
        if (r != 1) {
            failExit("writing the save: SaveSync %d", (int)r);
        }
        dComIfGs_exchangePlayerRecollectionData();
        dComIfGs_setMemCardCheckID(mDoMemCd_getCardSerialNo());
        dComIfGs_setNewFile(0);
        dComIfGs_setNoFile(0);
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        char context[256];
        snprintf(context, sizeof(context), "saved by %s on stage %s room %d, player at (%.0f, %.0f, %.0f), file %d",
                 sWho, dComIfGp_getStartStageName(), player != nullptr ? (int)fopAcM_GetRoomNo(player) : -1,
                 player != nullptr ? player->current.pos.x : 0.0f, player != nullptr ? player->current.pos.y : 0.0f,
                 player != nullptr ? player->current.pos.z : 0.0f, slot + 1);
        writeSummary("save_expect.txt", &sDataBuf[slot * sizeof(card_gamedata)], context);
        // The GCI on disk, both copies, against the bytes the game packed.
        const size_t size = readGci();
        if (size != kGciHeaderSize + kFileBlocks * kBlockSize) {
            failExit("the GCI %s is 0x%zX bytes", runCardGciPath() ? runCardGciPath() : "(none)", size);
        }
        int diffs = 0;
        for (int copy = 1; copy <= 2; copy++) {
            if (memcmp(gciFile(copy, 0), sDataBuf, sizeof(sDataBuf)) != 0) {
                writef(STDERR_FILENO, "[cos] %s: copy %d of the card's save differs from the packed data\n", sWho,
                       copy);
                diffs++;
            }
        }
        if (diffs != 0) {
            failExit("the card does not hold what the game wrote");
        }
        writef(STDERR_FILENO, "[cos] %s: PASS saved file %d on %s (return place %s room %d point %d) at frame %u\n",
               sWho, slot + 1, dComIfGp_getStartStageName(), sReturnName, sReturnRoom, sReturnPoint, frames);
        pc_exit(PC_EXIT_REACHED);
    }
    default:
        return;
    }
}

void loadFrame(unsigned int frames) {
    ++sSince;
    if (sState == kTitle) {
        if (findScene(fpcNm_NAME_SCENE_e) != nullptr) {
            writef(STDERR_FILENO, "[cos] save-load: frame %u: name scene (file select)\n", frames);
            sState = kName;
            sSince = 0;
            setDrivenPad(true, 0, 0, 0);
            return;
        }
        setDrivenPad(true, sSince % kStartEvery < 2 ? PAD_BUTTON_START : 0, 0, 0);
        return;
    }
    if (sState == kName) {
        base_process_class* play = findScene(fpcNm_PLAY_SCENE_e);
        if (play == nullptr) {
            setDrivenPad(true, sSince % kAEvery < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (sSince > kNoPlayFrames) {
                failExit("no PLAY scene %u frames after the name scene (main proc %d, card proc %d, draw proc %d)",
                         sSince, sNameMain, sNameCard, sNameDraw);
            }
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        writef(STDERR_FILENO, "[cos] save-load: frame %u: PLAY scene requested for %s room %d point %d\n", frames,
               dComIfGp_getStartStageName(), (int)dComIfGp_getNextStageRoomNo(), (int)dComIfGp_getNextStagePoint());
        logReturnPlace("loaded");
        const int slot = dComIfGs_getDataNum();
        memset(sRepacked, 0, sizeof(sRepacked));
        dComIfGs_setMemoryToCard(sRepacked, slot);
        const u8* mine = &sRepacked[slot * sizeof(card_gamedata)];
        writeSummary("save_loaded.txt", mine, "loaded through the file select");
        const size_t size = readGci();
        if (size != kGciHeaderSize + kFileBlocks * kBlockSize) {
            failExit("the GCI %s is 0x%zX bytes", runCardGciPath() ? runCardGciPath() : "(none)", size);
        }
        const int diffs = compareFiles("loaded state vs card", mine, gciFile(1, slot));
        if (diffs != 0) {
            failExit("%d region(s) of the loaded state differ from file %d of the card", diffs, slot + 1);
        }
        writef(STDERR_FILENO, "[cos] save-load: the loaded state equals file %d of the card (but the save date)\n",
               slot + 1);
        sState = kPlay;
        sSince = 0;
        return;
    }
    if (sState == kPlay) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        const char* stage = dComIfGp_getStartStageName();
        if (findScene(fpcNm_PLAY_SCENE_e) == nullptr || player == nullptr || stage == nullptr ||
            stageRoomReady(dComIfGp_roomControl_getStayNo(), nullptr) != nullptr) {
            if (sSince > kNoPlayFrames) {
                failExit("the PLAY scene of %s did not come up with the player in %u frames", sReturnName, sSince);
            }
            return;
        }
        static unsigned int sUp = 0;
        if (++sUp < kPlayFrames) {
            // A through an arrival event's messages.
            setDrivenPad(true, dComIfGp_event_runCheck() && sUp % kAEvery < 2 ? PAD_BUTTON_A : 0, 0, 0);
            return;
        }
        const int room = fopAcM_GetRoomNo(player);
        writef(STDERR_FILENO, "[cos] save-load: frame %u: on %s, player in room %d at (%.0f, %.0f, %.0f)\n", frames,
               stage, room, player->current.pos.x, player->current.pos.y, player->current.pos.z);
        if (strcmp(stage, sReturnName) != 0) {
            failExit("started on %s, the save's return place is %s", stage, sReturnName);
        }
        if (strcmp(stage, "sea") != 0 && sReturnRoom >= 0 && room != sReturnRoom) {
            failExit("player in room %d, the save's return room is %d", room, sReturnRoom);
        }
        writef(STDERR_FILENO, "[cos] save-load: PASS loaded %s room %d point %d\n", sReturnName, sReturnRoom,
               sReturnPoint);
        pc_exit(PC_EXIT_REACHED);
    }
}

} // namespace

void saveSweepStart(const char* who) {
    sWho = who;
    sMode = kModeSaveSweep;
    sState = kCheck;
    sSince = 0;
    setDrivenPad(true, 0, 0, 0);
    writef(STDERR_FILENO, "[cos] %s: saving through the save screen's steps (card %s)\n", sWho,
           runCardGciPath() ? runCardGciPath() : "(user card)");
}

bool saveSweepWantsRunCard() {
    if (gConfig.smoke == nullptr) {
        return false;
    }
    if (strcmp(gConfig.smoke, "save-sweep") == 0 || strcmp(gConfig.smoke, "save-load") == 0) {
        return true;
    }
    const char* save = getenv("COS_ITEM_SWEEP_SAVE");
    return strcmp(gConfig.smoke, "item-sweep") == 0 && save != nullptr && strcmp(save, "1") == 0;
}

void saveLoadNameScene(int mainProc, int memCardCheckProc, int drawProc) {
    if (sMode != kModeLoad) {
        return;
    }
    if (mainProc != sNameMain || memCardCheckProc != sNameCard || drawProc != sNameDraw) {
        writef(STDERR_FILENO, "[cos] save-load: name scene main proc %d, card proc %d, draw proc %d\n", mainProc,
               memCardCheckProc, drawProc);
        sNameMain = mainProc;
        sNameCard = memCardCheckProc;
        sNameDraw = drawProc;
    }
}

void saveSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke != nullptr && strcmp(gConfig.smoke, "save-sweep") == 0) {
            if (pc_boot_stage() == nullptr) {
                writef(STDERR_FILENO, "[cos] save-sweep: needs COS_BOOT_STAGE (e.g. --stage sea:44:206)\n");
                pc_exit(PC_EXIT_USAGE);
            }
            sMode = kModeSaveSweep;
            sState = kWait;
            setDrivenPad(true, 0, 0, 0);
        } else if (gConfig.smoke != nullptr && strcmp(gConfig.smoke, "save-load") == 0) {
            if (pc_boot_stage() != nullptr) {
                writef(STDERR_FILENO, "[cos] save-load: boots through the title: no COS_BOOT_STAGE\n");
                pc_exit(PC_EXIT_USAGE);
            }
            sWho = "save-load";
            sMode = kModeLoad;
            sState = kTitle;
            setDrivenPad(true, 0, 0, 0);
        }
    }
    if (sMode == kModeOff || sState == kOff || sState == kDone) {
        return;
    }
    if (sMode == kModeLoad) {
        loadFrame(frames);
        return;
    }
    if (sState == kWait) {
        if (!outsetLinkReady() || dComIfGp_getPlayer(0) == nullptr) {
            sSince = 0;
            return;
        }
        ++sSince;
        if (dComIfGp_event_runCheck()) {
            sEventFrames++;
            setDrivenPad(true, sEventFrames % 20 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            sSince = 0;
            if (sEventFrames > 30 * 120) {
                failExit("an event still runs after %u frames", sEventFrames);
            }
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        if (sSince >= kSettleFrames) {
            saveSweepStart("save-sweep");
        }
        return;
    }
    saveFrame(frames);
}

} // namespace pc
