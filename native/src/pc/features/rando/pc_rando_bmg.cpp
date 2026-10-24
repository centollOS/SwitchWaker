// A read-only BMG message lookup (pc_rando_bmg.h, docs/RANDOMIZER_PLAN.md step R7.6c). Keep this file
// free of game headers and game calls: it links into cos_rando_core and the unit tests.
#include "pc_rando_bmg.h"

#include <cstring>

namespace pcr {

namespace {

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

uint16_t be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

constexpr size_t kHeaderSize = 0x20;    // "MESGbmg1", file size, section count, encoding, padding
constexpr size_t kEntrySize = 0x18;     // the INF1 entry the game reads (JMSMesgEntry_c)
constexpr size_t kInfEntries = 0x10;    // INF1: header 8, count u16, entry size u16, group u16, colour u8, pad
constexpr size_t kDatText = 0x08;       // DAT1: header 8, then the strings

} // namespace

bool bmgFind(const uint8_t* bmg, size_t size, uint16_t msg_no, BmgMessage* out) {
    if (bmg == nullptr || size < kHeaderSize || std::memcmp(bmg, "MESGbmg1", 8) != 0) {
        return false;
    }
    const uint32_t sections = be32(bmg + 0x0C);
    const uint8_t* inf = nullptr;
    size_t infSize = 0;
    const uint8_t* dat = nullptr;
    size_t datSize = 0;
    size_t at = kHeaderSize;
    for (uint32_t s = 0; s < sections && at + 8 <= size; s++) {
        const uint32_t secSize = be32(bmg + at + 4);
        if (secSize < 8 || secSize > size - at) {
            return false;
        }
        if (inf == nullptr && std::memcmp(bmg + at, "INF1", 4) == 0) {
            inf = bmg + at;
            infSize = secSize;
        } else if (dat == nullptr && std::memcmp(bmg + at, "DAT1", 4) == 0) {
            dat = bmg + at;
            datSize = secSize;
        }
        at += secSize;
    }
    if (inf == nullptr || dat == nullptr || infSize < kInfEntries) {
        return false;
    }
    const uint16_t count = be16(inf + 0x08);
    const uint16_t entrySize = be16(inf + 0x0A);
    if (entrySize < kEntrySize || kInfEntries + (size_t)count * entrySize > infSize) {
        return false;
    }
    for (uint16_t i = 0; i < count; i++) {
        const uint8_t* e = inf + kInfEntries + (size_t)i * entrySize;
        const uint32_t offs = be32(e);
        if (offs == 0 || be16(e + 4) != msg_no) {
            continue;
        }
        if (kDatText + (size_t)offs >= datSize) {
            return false;
        }
        out->entry = e;
        out->text = dat + kDatText + offs;
        out->text_max = datSize - kDatText - offs;
        return true;
    }
    return false;
}

} // namespace pcr
