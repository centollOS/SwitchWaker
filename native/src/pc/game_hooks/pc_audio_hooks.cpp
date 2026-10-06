// Host code of the audio library's init data (JAIInitData.cpp), moved out of game/ (step G3 of
// docs/GAME_CODE_ORGANIZATION.md). Declared in native/include/pc/game_hooks.h.
#include "pc/game_hooks.h"
#include "JSystem/JAudio/JAIBankWave.h"
#include "JSystem/JAudio/JAIBasic.h"
#include "JSystem/JAudio/JAIInitData.h"
#include "JSystem/JKernel/JKRSolidHeap.h"

// Sections 2 (banks) and 3 (wave systems) of JaiInit.aaf list {offset, size, flags} as three
// big-endian words per entry, ending with a 0 word. The GameCube copies the list and relocates each
// offset in place into a pointer; initOnCode_s has a host pointer (8 bytes) and host-order fields,
// so the host table is built from the words instead, with the same zero terminator. Returns the
// table; *words is the number of list words before the terminator.
JAInter::BankWave::initOnCode_s* JAIPcMakeInitOnCodeTable(BE(u32)* list, int* words) {
    int n;
    for (n = 0; list[n * 3] != 0; n++) {}
    *words = n * 3;
    JAInter::BankWave::initOnCode_s* table = new (JAIBasic::getCurrentJAIHeap(), 0x20) JAInter::BankWave::initOnCode_s[n + 1];
    if (table == NULL) {
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        table[i].field_0x0 = (u8*)JAInter::InitData::aafPointer + list[i * 3];
        table[i].field_0x4 = list[i * 3 + 1];
        table[i].field_0x8 = list[i * 3 + 2];
    }
    table[n].field_0x0 = NULL;
    table[n].field_0x4 = 0;
    table[n].field_0x8 = 0;
    return table;
}
