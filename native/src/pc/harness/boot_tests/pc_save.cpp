// Save data and memory card on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.15, decision H2):
// the COS_SMOKE=save test, a save round trip through the game's own code.
//
// Before CARDInit (pc_aurora_init calls prepareSaveSmoke) the card of slot A is moved to
// <COS_RUN_DIR>/card, so the test starts from an empty, freshly formatted GCI folder and never
// touches the user's card. Then, from pc_heaps_created (main01, right after mDoMch_Create started
// the memory card thread):
// 1. attach: mDoMemCd_UpDate until the card thread mounted the card (no save file yet);
// 2. new save: dSv_info_c::init, then distinctive values in every multi-byte field that goes to
//    the card (status A/B, item record timer, reserve flags, map, info, priest position, a
//    stage's memory bits, the ocean bits) and a few byte fields, through the game's setters;
// 3. write: as the name scene and the file select do, initdata_to_card into files 2 and 3,
//    memory_to_card into file 1, mDoMemCdRWm_SetCheckSumGameData on each, mDoMemCd_Save of the
//    three, mDoMemCd_SaveSync until done (this creates the file: header block, two copies);
// 4. the GCI file on disk, read independently with plain big-endian loads at the GameCube offsets:
//    its 0x40-byte directory entry (GZLE/01/kSaveFile, 12 blocks, comment at 0x1C00), the header
//    block's title, both copies of card_savedata (save count 1, data version 0, the halfword
//    checksum of the block, each file's byte checksum) and every value set in 2. at its offset in
//    the packed file;
// 5. reload: dSv_info_c::init again, mDoMemCd_Load, mDoMemCd_LoadSync until done; the three files
//    read back must equal the bytes written, pass mDoMemCdRWm_TestCheckSumGameData, and
//    card_to_memory of file 1 must give back every value of 2. through the getters; memory_to_card
//    of the reloaded state must reproduce file 1 byte for byte (but the save date, which
//    memory_to_card stamps anew);
// 6. a second write of the reloaded state into the existing file: save count 2 on the card.
// Exit 0 when every check holds, 1 otherwise. The card stays in the run directory (never in git).
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <dolphin/card.h>
#include <dolphin/os.h>

#include <cerrno>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pc {

namespace {

int sErrors = 0;

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    writef(STDERR_FILENO, "[cos] save: FAIL %s\n", line);
    sErrors++;
}

// The GameCube layout of the card, written down independently of the game's structs.
// The game's card file name (the name m_Do_MemCard.cpp passes to CARDOpen and CARDCreate).
constexpr char kSaveFile[] = "gczelda";
constexpr uint32_t kGciHeaderSize = 0x40;  // the directory entry a .gci starts with
constexpr uint32_t kBlockSize = 0x2000;    // card_savedata, card_pictdata, the header block
constexpr uint32_t kFileBlocks = 0x18000 / kBlockSize; // mDoMemCd_Ctrl_c::store's CARDCreate size
constexpr uint32_t kGameDataSize = 0x770;  // card_gamedata: the packed dSv_save_c and its checksum
constexpr uint32_t kGameDataCsum = 0x768;  // its u64 byte checksum
constexpr uint32_t kSaveDataCsum = 0x1FFC; // card_savedata's u32 halfword checksum
constexpr uint32_t kSaveDataFiles = 0x8;   // card_savedata::gamedata[3]

// Offsets in the packed save (memory_to_card writes the parts one after the other).
constexpr uint32_t kStatusA = 0x000;      // dSv_player_status_a_c, 0x18
constexpr uint32_t kStatusB = 0x018;      // dSv_player_status_b_c, 0x18
constexpr uint32_t kItemRecord = 0x066;   // dSv_player_item_record_c, 8
constexpr uint32_t kGetBagItem = 0x08E;   // dSv_player_get_bag_item_c, 0xC
constexpr uint32_t kCollect = 0x0B2;      // dSv_player_collect_c, 0xD
constexpr uint32_t kMap = 0x0BF;          // dSv_player_map_c, 0x84
constexpr uint32_t kInfo = 0x143;         // dSv_player_info_c, 0x5C
constexpr uint32_t kPriest = 0x1A4;       // dSv_player_priest_c, 0x10
constexpr uint32_t kMemory = 0x374;       // dSv_memory_c[16], 0x24 each
constexpr uint32_t kOcean = 0x5B4;        // dSv_ocean_c, 0x64
constexpr uint32_t kEvent = 0x618;        // dSv_event_c, 0x100
constexpr uint32_t kPacked = 0x768;       // sizeof(dSv_save_c_PACKED)

static_assert(sizeof(card_gamedata) == kGameDataSize, "card_gamedata");
static_assert(sizeof(card_savedata) == kBlockSize, "card_savedata");
static_assert(sizeof(dSv_save_c_PACKED) == kPacked, "the packed save");
// The card layout through zeldaret's packed structs (d_save.h; STATIC_ASSERT is empty off MWCC).
static_assert(sizeof(dSv_player_c_PACKED) == kMemory, "the packed player");
static_assert(offsetof(dSv_player_c_PACKED, mGetBagItem) == kGetBagItem, "mGetBagItem");
static_assert(offsetof(dSv_player_c_PACKED, mCollect) == kCollect, "mCollect");
static_assert(offsetof(dSv_player_c_PACKED, mMap) == kMap, "mMap");
static_assert(offsetof(dSv_player_c_PACKED, mInfo) == kInfo, "mInfo");
static_assert(offsetof(dSv_player_c_PACKED, mPriest) == kPriest, "mPriest");
static_assert(offsetof(dSv_save_c_PACKED, mMemory) == kMemory, "mMemory");
static_assert(offsetof(dSv_save_c_PACKED, mOcean) == kOcean, "mOcean");
static_assert(offsetof(dSv_save_c_PACKED, mEvent) == kEvent, "mEvent");

// The values the test saves.
constexpr u16 kMaxLife = 0x28;
constexpr u16 kLife = 0x1B;
constexpr u16 kRupee = 4321;
constexpr u16 kStatusAField6 = 0xBEEF;
constexpr f32 kStatusBField8 = -2.75f;
constexpr f32 kTime = 123.5f;
constexpr u16 kDate = 7;
constexpr s16 kWindX = -12345;
constexpr s16 kWindY = 0x1234;
constexpr u16 kTimer = 0xA55A;
constexpr u8 kReserveBitA = 3;
constexpr u8 kReserveBitB = 29;
constexpr u32 kMapWord = 0x89ABCDEF; // field_0x0[3][2]
constexpr u16 kInfoField10 = 0x1357;
constexpr int kDeaths = 3;
const char kName[] = "Tetra";
constexpr u8 kPriestFlag = 2;
constexpr f32 kPriestX = 1.5f;
constexpr f32 kPriestY = -2.25f;
constexpr f32 kPriestZ = 100000.0f;
constexpr s16 kPriestAngle = -1234;
constexpr s8 kPriestRoom = 7;
constexpr int kStage = dSv_save_c::STAGE_DRC;
constexpr int kTbox = 13;
constexpr int kSwitch = 70;
constexpr int kItem = 5;
constexpr int kVisitedRoom = 40;
constexpr u8 kKeys = 3;
constexpr u8 kOceanGrid = 9;
constexpr u16 kOceanBit = 13;
constexpr u16 kEventBit = 0x1820;
constexpr u8 kTriforce = 2;
constexpr u8 kArrows = 27;

u16 be16At(const u8* p) {
    return (u16)((p[0] << 8) | p[1]);
}

u32 be32At(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

u64 be64At(const u8* p) {
    return ((u64)be32At(p) << 32) | be32At(p + 4);
}

f32 bef32At(const u8* p) {
    u32 bits = be32At(p);
    f32 value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void expect16(const char* what, const u8* file, uint32_t offset, u16 want) {
    u16 got = be16At(file + offset);
    if (got != want) {
        fail("%s at 0x%03X: 0x%04X, want big-endian 0x%04X", what, offset, got, want);
    }
}

void expect32(const char* what, const u8* file, uint32_t offset, u32 want) {
    u32 got = be32At(file + offset);
    if (got != want) {
        fail("%s at 0x%03X: 0x%08X, want big-endian 0x%08X", what, offset, got, want);
    }
}

void expectF32(const char* what, const u8* file, uint32_t offset, f32 want) {
    f32 got = bef32At(file + offset);
    if (got != want) {
        fail("%s at 0x%03X: %g, want big-endian %g", what, offset, got, want);
    }
}

void expect8(const char* what, const u8* file, uint32_t offset, u8 want) {
    if (file[offset] != want) {
        fail("%s at 0x%03X: 0x%02X, want 0x%02X", what, offset, file[offset], want);
    }
}

// The checksums of m_Do_MemCardRWmng.cpp, recomputed over the bytes as the GameCube reads them.
u32 savedataChecksum(const u8* block) {
    u16 c0 = 0, c1 = 0;
    for (uint32_t i = 0; i < kSaveDataCsum; i += 2) {
        u16 v = be16At(block + i);
        c0 += v;
        c1 += (u16)~v;
    }
    return ((u32)c0 << 16) | c1;
}

u64 gamedataChecksum(const u8* file) {
    u32 c0 = 0, c1 = 0;
    for (uint32_t i = 0; i < kGameDataCsum; i++) {
        c0 += file[i];
        c1 += (u32)~file[i];
    }
    return ((u64)c0 << 32) | c1;
}

char sCardBase[1024];
char sGciPath[1200];

// Lets the card thread get back to waiting on its condition: mDoMemCd_Ctrl_c::save and load
// only queue a command when OSTryLockMutex gets the mutex (the game issues them frames apart).
void settle() {
    usleep(20000);
    pc_frame_tick();
}

// Polls `done` (with a frame tick, so the stall watchdog sees progress) for up to 20 s.
template <typename F>
bool waitFor(const char* what, F done) {
    uint64_t start = elapsedMs();
    while (!done()) {
        if (elapsedMs() - start > 20000) {
            fail("%s: no result after 20 s (card state %d, command %d)", what,
                 (int)g_mDoMemCd_control.field_0x1660, (int)g_mDoMemCd_control.mCommand);
            return false;
        }
        usleep(2000);
        pc_frame_tick();
    }
    return true;
}

// 2. The new save, through the game's setters.
void fillSave() {
    g_dComIfG_gameInfo.save.init();
    dSv_player_c& player = g_dComIfG_gameInfo.save.getPlayer();
    player.getPlayerStatusA().setMaxLife(kMaxLife);
    player.getPlayerStatusA().setLife(kLife);
    player.getPlayerStatusA().setRupee(kRupee);
    player.getPlayerStatusA().field_0x6 = kStatusAField6;
    player.getPlayerStatusB().field_0x8 = kStatusBField8;
    player.getPlayerStatusB().setTime(kTime);
    player.getPlayerStatusB().setDate(kDate);
    player.getPlayerStatusB().setWindX(kWindX);
    player.getPlayerStatusB().setWindY(kWindY);
    player.getItemRecord().mTimer = kTimer; // resetTimer would also stop the forest water timer
    player.getItemRecord().setArrowNum(kArrows);
    player.getGetBagItem().onReserve(kReserveBitA);
    player.getGetBagItem().onReserve(kReserveBitB);
    player.getCollect().onTriforce(kTriforce);
    player.getMap().field_0x0[3][2] = kMapWord;
    player.getPlayerInfo().setPlayerName(kName);
    player.getPlayerInfo().field_0x10 = kInfoField10;
    for (int i = 0; i < kDeaths; i++) {
        player.getPlayerInfo().addDeathCount();
    }
    cXyz priestPos(kPriestX, kPriestY, kPriestZ);
    player.getPriest().set(kPriestFlag, priestPos, kPriestAngle, kPriestRoom);
    dSv_memBit_c& bit = g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit();
    bit.onTbox(kTbox);
    bit.onSwitch(kSwitch);
    bit.onItem(kItem);
    bit.onVisitedRoom(kVisitedRoom);
    bit.setKeyNum(kKeys);
    g_dComIfG_gameInfo.save.getOcean().onOceanSvBit(kOceanGrid, kOceanBit);
    g_dComIfG_gameInfo.save.getEvent().onEventBit(kEventBit);
}

// 5. The values of fillSave, read back through the getters after card_to_memory.
void checkReloaded() {
    dSv_player_c& player = g_dComIfG_gameInfo.save.getPlayer();
    struct {
        const char* what;
        long got;
        long want;
    } ints[] = {
        {"max life", player.getPlayerStatusA().getMaxLife(), kMaxLife},
        {"life", player.getPlayerStatusA().getLife(), kLife},
        {"rupees", player.getPlayerStatusA().getRupee(), kRupee},
        {"status A field_0x6", player.getPlayerStatusA().field_0x6, kStatusAField6},
        {"date", player.getPlayerStatusB().getDate(), kDate},
        {"wind X", player.getPlayerStatusB().getWindX(), kWindX},
        {"wind Y", player.getPlayerStatusB().getWindY(), kWindY},
        {"timer", player.getItemRecord().mTimer, kTimer},
        {"arrows", player.getItemRecord().getArrowNum(), kArrows},
        {"reserve A", (long)player.getGetBagItem().isReserve(kReserveBitA), 1},
        {"reserve B", (long)player.getGetBagItem().isReserve(kReserveBitB), 1},
        {"reserve 0", (long)player.getGetBagItem().isReserve(0), 0},
        {"triforce", (long)player.getCollect().isTriforce(kTriforce), 1},
        {"map word", (long)(u32)player.getMap().field_0x0[3][2], (long)kMapWord},
        {"info field_0x10", player.getPlayerInfo().field_0x10, kInfoField10},
        {"deaths", player.getPlayerInfo().mDeathCount, kDeaths},
        {"priest flag", player.getPriest().getFlag(), kPriestFlag},
        {"priest angle", player.getPriest().getRotate(), kPriestAngle},
        {"priest room", player.getPriest().getRoomNo(), kPriestRoom},
        {"stage tbox", (long)g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit().isTbox(kTbox), 1},
        {"stage switch", (long)g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit().isSwitch(kSwitch), 1},
        {"stage switch +1", (long)g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit().isSwitch(kSwitch + 1), 0},
        {"stage item", (long)g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit().isItem(kItem), 1},
        {"stage room", (long)g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit().isVisitedRoom(kVisitedRoom), 1},
        {"stage keys", g_dComIfG_gameInfo.save.getSavedata().getSave(kStage).getBit().getKeyNum(), kKeys},
        {"ocean bit", (long)g_dComIfG_gameInfo.save.getOcean().isOceanSvBit(kOceanGrid, kOceanBit), 1},
        {"event bit", (long)g_dComIfG_gameInfo.save.getEvent().isEventBit(kEventBit), 1},
    };
    for (const auto& c : ints) {
        if (c.got != c.want) {
            fail("reloaded %s: %ld, want %ld", c.what, c.got, c.want);
        }
    }
    struct {
        const char* what;
        f32 got;
        f32 want;
    } floats[] = {
        {"status B field_0x8", player.getPlayerStatusB().field_0x8, kStatusBField8},
        {"time", player.getPlayerStatusB().getTime(), kTime},
        {"priest x", player.getPriest().getPos().x, kPriestX},
        {"priest y", player.getPriest().getPos().y, kPriestY},
        {"priest z", player.getPriest().getPos().z, kPriestZ},
    };
    for (const auto& c : floats) {
        if (c.got != c.want) {
            fail("reloaded %s: %g, want %g", c.what, c.got, c.want);
        }
    }
    if (strcmp(player.getPlayerInfo().getPlayerName(), kName) != 0) {
        fail("reloaded name \"%.17s\", want \"%s\"", player.getPlayerInfo().getPlayerName(), kName);
    }
}

// 4. File 1 of a card_savedata copy, at the GameCube offsets.
void checkPackedFile(const u8* file) {
    expect16("max life", file, kStatusA + 0x0, kMaxLife);
    expect16("life", file, kStatusA + 0x2, kLife);
    expect16("rupees", file, kStatusA + 0x4, kRupee);
    expect16("status A field_0x6", file, kStatusA + 0x6, kStatusAField6);
    if (be64At(file + kStatusB + 0x0) == 0) {
        fail("save date at 0x%03X is 0 (memory_to_card stamps OSGetTime)", kStatusB);
    }
    expectF32("status B field_0x8", file, kStatusB + 0x8, kStatusBField8);
    expectF32("time", file, kStatusB + 0xC, kTime);
    expect16("date", file, kStatusB + 0x10, kDate);
    expect16("wind X", file, kStatusB + 0x12, (u16)kWindX);
    expect16("wind Y", file, kStatusB + 0x14, (u16)kWindY);
    expect16("timer", file, kItemRecord + 0x0, kTimer);
    expect8("arrows", file, kItemRecord + 0x3, kArrows);
    expect32("reserve flags", file, kGetBagItem + 0x0, (1u << kReserveBitA) | (1u << kReserveBitB));
    expect8("triforce", file, kCollect + 0xA, 1u << kTriforce);
    expect32("map field_0x0[3][2]", file, kMap + (3 * 4 + 2) * 4, kMapWord);
    expect16("info field_0x10", file, kInfo + 0x10, kInfoField10);
    expect16("deaths", file, kInfo + 0x12, kDeaths);
    if (memcmp(file + kInfo + 0x14, kName, sizeof(kName)) != 0) {
        fail("name at 0x%03X is \"%.17s\", want \"%s\"", kInfo + 0x14, (const char*)file + kInfo + 0x14, kName);
    }
    expectF32("priest x", file, kPriest + 0x0, kPriestX);
    expectF32("priest y", file, kPriest + 0x4, kPriestY);
    expectF32("priest z", file, kPriest + 0x8, kPriestZ);
    expect16("priest angle", file, kPriest + 0xC, (u16)kPriestAngle);
    expect8("priest room", file, kPriest + 0xE, (u8)kPriestRoom);
    expect8("priest flag", file, kPriest + 0xF, kPriestFlag);
    uint32_t mem = kMemory + kStage * 0x24;
    expect32("stage tbox", file, mem + 0x0, 1u << kTbox);
    expect32("stage switch word", file, mem + 0x4 + (kSwitch >> 5) * 4, 1u << (kSwitch & 31));
    expect32("stage item word", file, mem + 0x14, 1u << kItem);
    expect32("stage visited room word", file, mem + 0x18 + (kVisitedRoom >> 5) * 4, 1u << (kVisitedRoom & 31));
    expect8("stage keys", file, mem + 0x20, kKeys);
    expect16("ocean bits", file, kOcean + kOceanGrid * 2, 1u << kOceanBit);
    expect8("event byte", file, kEvent + (kEventBit >> 8), kEventBit & 0xFF);
    u64 csum = be64At(file + kGameDataCsum);
    if (csum != gamedataChecksum(file)) {
        fail("file checksum 0x%016llX, recomputed 0x%016llX", (unsigned long long)csum,
             (unsigned long long)gamedataChecksum(file));
    }
}

// 4. A card_savedata block (copy 1 or 2) of the GCI.
void checkSaveBlock(const char* which, const u8* block, u32 saveCount) {
    expect32(which, block, 0x0, saveCount); // save_count
    expect32(which, block, 0x4, 0);         // data_version
    u32 csum = be32At(block + kSaveDataCsum);
    if (csum != savedataChecksum(block)) {
        fail("%s block checksum 0x%08X, recomputed 0x%08X", which, csum, savedataChecksum(block));
    }
    for (int i = 0; i < 3; i++) {
        const u8* file = block + kSaveDataFiles + i * kGameDataSize;
        if (be64At(file + kGameDataCsum) != gamedataChecksum(file)) {
            fail("%s file %d checksum 0x%016llX, recomputed 0x%016llX", which, i + 1,
                 (unsigned long long)be64At(file + kGameDataCsum), (unsigned long long)gamedataChecksum(file));
        }
    }
}

// Reads the GCI file; returns its size, 0 on error.
size_t readGci(u8* buffer, size_t capacity) {
    int fd = open(sGciPath, O_RDONLY);
    if (fd < 0) {
        fail("cannot open %s: %s", sGciPath, strerror(errno));
        return 0;
    }
    size_t total = 0;
    for (;;) {
        ssize_t n = read(fd, buffer + total, capacity - total);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
        if (total == capacity) {
            break;
        }
    }
    close(fd);
    return total;
}

alignas(32) u8 sWritten[3 * sizeof(card_gamedata)];
alignas(32) u8 sReloaded[3 * sizeof(card_gamedata)];
alignas(32) u8 sRepacked[3 * sizeof(card_gamedata)];
u8 sGci[kGciHeaderSize + kFileBlocks * kBlockSize + 1];

void checkGci(u32 saveCount) {
    size_t size = readGci(sGci, sizeof(sGci));
    if (size == 0) {
        return;
    }
    if (size != kGciHeaderSize + kFileBlocks * kBlockSize) {
        fail("%s is 0x%zX bytes, want 0x%X (a 0x40-byte entry and %u blocks)", sGciPath, size,
             kGciHeaderSize + kFileBlocks * kBlockSize, kFileBlocks);
        return;
    }
    // The directory entry (CARDDir): game, maker, file name, block count, comment address.
    if (memcmp(sGci + 0x0, "GZLE", 4) != 0 || memcmp(sGci + 0x4, "01", 2) != 0) {
        fail("GCI entry game/maker \"%.4s\"/\"%.2s\", want GZLE/01", (const char*)sGci, (const char*)sGci + 4);
    }
    if (strncmp((const char*)sGci + 0x8, kSaveFile, 32) != 0) {
        fail("GCI entry file name \"%.32s\", want %s", (const char*)sGci + 0x8, kSaveFile);
    }
    expect8("GCI banner format", sGci, 0x07, 1);
    expect32("GCI icon address", sGci, 0x2C, 0);
    expect16("GCI block count", sGci, 0x38, kFileBlocks);
    expect32("GCI comment address", sGci, 0x3C, 0x1C00);
    const u8* data = sGci + kGciHeaderSize;
    // Block 0: mDoMemCdRWm_HeaderData (banner, icon, comment, info). The comment is the game's
    // title, written by mDoMemCdRWm_BuildHeader: a non-empty printable string inside its 32 bytes.
    const char* comment = (const char*)data + 0x1C00;
    const size_t commentLen = strnlen(comment, 32);
    bool commentPrintable = commentLen > 0 && commentLen < 32;
    for (size_t i = 0; i < commentLen; i++) {
        commentPrintable = commentPrintable && comment[i] >= 0x20 && comment[i] < 0x7F;
    }
    if (!commentPrintable) {
        fail("header comment \"%.32s\"", comment);
    }
    if (strstr((const char*)data + 0x1C20, " Save Data") == nullptr) {
        fail("header info \"%.32s\"", (const char*)data + 0x1C20);
    }
    // Blocks 1 and 2: the two copies of card_savedata.
    for (int copy = 1; copy <= 2; copy++) {
        const u8* block = data + copy * kBlockSize;
        checkSaveBlock(copy == 1 ? "copy 1" : "copy 2", block, saveCount);
        if (memcmp(block, data + kBlockSize, kBlockSize) != 0) {
            fail("copy 2 differs from copy 1");
        }
    }
    checkPackedFile(data + kBlockSize + kSaveDataFiles);
}

} // namespace

void prepareRunCard(const char* who) {
    if (gConfig.runDir == nullptr) {
        writef(STDERR_FILENO, "[cos] %s: needs COS_RUN_DIR (run it through native/tools/run.sh)\n",
               who);
        pc_exit(PC_EXIT_USAGE);
    }
    // A trailing slash: CARDSetBasePath keeps a path with no file name as the base directory.
    // COS_CARD_DIR: another card folder (the save/load sweep loads the card a save-sweep run wrote).
    const char* cardDir = getenv("COS_CARD_DIR");
    if (cardDir != nullptr && cardDir[0] != '\0') {
        snprintf(sCardBase, sizeof(sCardBase), "%s/", cardDir);
    } else {
        snprintf(sCardBase, sizeof(sCardBase), "%s/card/", gConfig.runDir);
    }
    snprintf(sGciPath, sizeof(sGciPath), "%sUSA/Card A/01-GZLE-%s.gci", sCardBase, kSaveFile);
    CARDSetBasePath(sCardBase, 0);
    writef(STDERR_FILENO, "[cos] %s: memory card A is the folder %s%s\n", who, sCardBase,
           cardDir != nullptr && cardDir[0] != '\0' ? " (COS_CARD_DIR)" : " (empty at start)");
}

const char* runCardGciPath() {
    return sGciPath[0] != '\0' ? sGciPath : nullptr;
}

void prepareSaveSmoke() {
    prepareRunCard("save");
}

[[noreturn]] void smokeSave() {
    uint64_t start = elapsedMs();
    writef(STDERR_FILENO, "[cos] save: card %s\n", sCardBase);

    // 1. attach the card. The card thread clears the command when it is done; the main thread
    // reads the result after that (the game reads it on a later frame).
    settle();
    mDoMemCd_UpDate();
    waitFor("attach", [] { return mDoMemCd_isCardCommNone(); });
    if (g_mDoMemCd_control.field_0x1660 != 2) {
        fail("after attach the card state is %d, want 2 (mounted, no save file)",
             (int)g_mDoMemCd_control.field_0x1660);
    }

    // 2. and 3. a new save, written.
    fillSave();
    memset(sWritten, 0, sizeof(sWritten));
    dComIfGs_setMemoryToCard(sWritten, 0);
    mDoMemCdRWm_SetCheckSumGameData(sWritten, 0);
    dComIfGs_setInitDataToCard(sWritten, 1);
    mDoMemCdRWm_SetCheckSumGameData(sWritten, 1);
    dComIfGs_setInitDataToCard(sWritten, 2);
    mDoMemCdRWm_SetCheckSumGameData(sWritten, 2);
    for (int i = 0; i < 3; i++) {
        if (!mDoMemCdRWm_TestCheckSumGameData(&sWritten[i * sizeof(card_gamedata)])) {
            fail("file %d fails mDoMemCdRWm_TestCheckSumGameData before the write", i + 1);
        }
    }
    s32 sync = 0;
    settle();
    mDoMemCd_Save(sWritten, sizeof(sWritten), 0);
    if (waitFor("first write", [] { return mDoMemCd_isCardCommNone(); })) {
        sync = mDoMemCd_SaveSync();
    }
    if (sync != 1) {
        fail("first write: SaveSync %d, card state %d", (int)sync, (int)g_mDoMemCd_control.field_0x1660);
    }

    // 4. the GCI, independently.
    checkGci(1);

    // 5. reload.
    g_dComIfG_gameInfo.save.init();
    memset(sReloaded, 0xA5, sizeof(sReloaded));
    settle();
    mDoMemCd_Load();
    s32 load = 0;
    if (waitFor("reload", [] { return mDoMemCd_isCardCommNone(); })) {
        load = mDoMemCd_LoadSync(sReloaded, sizeof(sReloaded), 0);
    }
    if (load != 1) {
        fail("reload: LoadSync %d, card state %d", (int)load, (int)g_mDoMemCd_control.field_0x1660);
    }
    if (memcmp(sReloaded, sWritten, sizeof(sWritten)) != 0) {
        for (size_t i = 0; i < sizeof(sWritten); i++) {
            if (sReloaded[i] != sWritten[i]) {
                fail("reloaded data differs from the written data first at 0x%zX", i);
                break;
            }
        }
    }
    for (int i = 0; i < 3; i++) {
        if (!mDoMemCdRWm_TestCheckSumGameData(&sReloaded[i * sizeof(card_gamedata)])) {
            fail("reloaded file %d fails mDoMemCdRWm_TestCheckSumGameData", i + 1);
        }
    }
    dComIfGs_setCardToMemory(sReloaded, 0);
    checkReloaded();
    u64 dateIpl = g_dComIfG_gameInfo.save.getPlayer().getPlayerStatusB().getDateIpl();
    if (dateIpl != be64At(sWritten + kStatusB)) {
        fail("reloaded save date 0x%016llX, written 0x%016llX", (unsigned long long)dateIpl,
             (unsigned long long)be64At(sWritten + kStatusB));
    }
    memcpy(sRepacked, sReloaded, sizeof(sRepacked));
    dComIfGs_setMemoryToCard(sRepacked, 0);
    // memory_to_card stamps a new save date (8 bytes at 0x18): compare around it.
    if (memcmp(sRepacked, sWritten, kStatusB) != 0 ||
        memcmp(sRepacked + kStatusB + 8, sWritten + kStatusB + 8, kPacked - kStatusB - 8) != 0) {
        fail("memory_to_card of the reloaded state differs from the written file 1");
    }

    // 6. a second write into the existing file.
    mDoMemCdRWm_SetCheckSumGameData(sRepacked, 0);
    sync = 0;
    settle();
    mDoMemCd_Save(sRepacked, sizeof(sRepacked), 0);
    if (waitFor("second write", [] { return mDoMemCd_isCardCommNone(); })) {
        sync = mDoMemCd_SaveSync();
    }
    if (sync != 1) {
        fail("second write: SaveSync %d, card state %d", (int)sync, (int)g_mDoMemCd_control.field_0x1660);
    }
    checkGci(2);

    writef(STDERR_FILENO,
           "[cos] save: wrote, reloaded and compared %u bytes (3 files) through %s; %d error(s) "
           "in %llu ms\n",
           (unsigned int)sizeof(sWritten), sGciPath, sErrors, (unsigned long long)(elapsedMs() - start));
    pc_exit(sErrors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
