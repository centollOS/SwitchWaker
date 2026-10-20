// pc_rando_lockeys.cpp - the location key table (docs/RANDOMIZER_PLAN.md, step R0.5). The data is
// generated/rando_location_keys.inc (native/tools/rando/gen_location_keys.py); this file wraps it.
// Pure: no game headers, no allocation (pc_rando_lockeys.h).
#include "pc_rando_lockeys.h"

#include <stdio.h>
#include <string.h>

namespace pcr {
namespace gen {
#include "generated/rando_location_keys.inc"
} // namespace gen

namespace {

constexpr size_t kNumKeys = sizeof(gen::kLocKeys) / sizeof(gen::kLocKeys[0]);
constexpr size_t kNumLocs = sizeof(gen::kLocInfo) / sizeof(gen::kLocInfo[0]);
static_assert(kNumLocs == PCR_LOC_COUNT, "rando_location_keys.inc and pc_rando_locs.h disagree");

uint32_t shiftOf(uint32_t mask) {
    uint32_t s = 0;
    while (mask != 0 && (mask & 1u) == 0) {
        mask >>= 1;
        s++;
    }
    return s;
}

uint32_t readWord(const uint8_t* p, int width) {
    return width == 4 ? (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]
                      : (uint32_t)p[0] << 8 | p[1];
}

void writeWord(uint8_t* p, int width, uint32_t v) {
    if (width == 4) {
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    } else {
        p[0] = (uint8_t)(v >> 8);
        p[1] = (uint8_t)v;
    }
}

} // namespace

Span<LocKey> location_keys() { return Span<LocKey>{gen::kLocKeys, kNumKeys}; }

Span<LocKey> location_keys(int loc) {
    if (loc < 0 || (size_t)loc >= kNumLocs || (size_t)loc >= locations().size) {
        return Span<LocKey>{gen::kLocKeys, 0};
    }
    const Location& l = locations()[loc];
    if ((size_t)l.first_path + l.num_paths > kNumKeys) {
        return Span<LocKey>{gen::kLocKeys, 0};
    }
    return Span<LocKey>{gen::kLocKeys + l.first_path, l.num_paths};
}

const LocInfo* location_info(int loc) {
    if (loc < 0 || (size_t)loc >= kNumLocs) {
        return nullptr;
    }
    return &gen::kLocInfo[loc];
}

const char* location_class_name(int loc, char* buf, size_t size) {
    static const char* const kNames[] = {"CHEST", "ADDED", "ACTR", "SCOB", "EVENT", "CODE", "CUSTOM"};
    if (size == 0) {
        return buf;
    }
    buf[0] = '\0';
    const LocInfo* info = location_info(loc);
    if (info == nullptr) {
        return buf;
    }
    size_t n = 0;
    for (int i = 0; i < 7; i++) {
        if ((info->classes & (1u << i)) == 0) {
            continue;
        }
        int w = snprintf(buf + n, size - n, "%s%s", n == 0 ? "" : "+", kNames[i]);
        if (w < 0 || (size_t)w >= size - n) {
            break;
        }
        n += (size_t)w;
    }
    return buf;
}

uint32_t dzx_key_read(const LocKey& key, const uint8_t* entry) {
    return (readWord(entry + key.word_off, key.width) & key.mask) >> shiftOf(key.mask);
}

void dzx_key_write(const LocKey& key, uint8_t* entry, uint32_t value) {
    uint32_t w = readWord(entry + key.word_off, key.width);
    w = (w & ~key.mask) | ((value << shiftOf(key.mask)) & key.mask);
    writeWord(entry + key.word_off, key.width, w);
}

bool dzx_key_fingerprint_ok(const LocKey& key, const uint8_t* entry) {
    if (key.kind != KeyKind::Dzx || entry == nullptr) {
        return false;
    }
    return memcmp(entry, key.fp_name, sizeof(key.fp_name)) == 0 && dzx_key_read(key, entry) == key.disc_byte;
}

} // namespace pcr
