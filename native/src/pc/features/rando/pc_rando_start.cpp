// pc_rando_start.cpp - health, magic, clothes and the start spawn (docs/RANDOMIZER_PLAN.md step R3.2),
// pure part. See pc_rando_start.h.
#include "pc_rando_start.h"

#include <string.h>

namespace {

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint16_t wholeHearts(uint16_t quarters) {
    return (uint16_t)(quarters & ~3u);
}

} // namespace

extern "C" {

uint16_t pc_rando_start_life(uint16_t startingQuarterHearts) {
    return wholeHearts(startingQuarterHearts);
}

uint16_t pc_rando_start_blank_slot_life(uint16_t startingQuarterHearts) {
    return wholeHearts(startingQuarterHearts);
}

uint16_t pc_rando_start_loaded_life(uint16_t maxLife, uint16_t life) {
    // The asm compares as signed halfwords (lha, cmpw); HP never reaches 0x8000.
    const int16_t cap = (int16_t)wholeHearts(maxLife);
    return (int16_t)life > cap ? (uint16_t)cap : life;
}

uint16_t pc_rando_start_gameover_life(uint16_t maxLife) {
    const int16_t whole = (int16_t)wholeHearts(maxLife);
    return whole >= PCR_GAMEOVER_LIFE ? (uint16_t)PCR_GAMEOVER_LIFE : (uint16_t)whole;
}

int pc_rando_start_find_ship_spawn(const void* dzr, size_t size, int shipId, PcRandoShipSpawn* out) {
    PcRandoDzx dzx;
    // pc_rando_dzx_open takes a mutable view; this function only reads.
    if (dzr == nullptr || out == nullptr || pc_rando_dzx_open(&dzx, const_cast<void*>(dzr), size) != PCR_DZX_OK) {
        return 0;
    }
    const uint32_t n = pc_rando_dzx_count(&dzx, "SHIP");
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(&dzx, "SHIP", i);
        if (e != nullptr && e[0x0E] == (uint8_t)shipId) {
            out->x = pc_rando_dzx_bef32(e + 0x00);
            out->y = pc_rando_dzx_bef32(e + 0x04);
            out->z = pc_rando_dzx_bef32(e + 0x08);
            out->yRot = pc_rando_dzx_be16(e + 0x0C);
            out->shipId = e[0x0E];
            return 1;
        }
    }
    return 0;
}

int pc_rando_start_patch_sea_stage(PcRandoDzx* dzx, const PcRandoShipSpawn* spawn, uint32_t* index) {
    const uint32_t n = pc_rando_dzx_count(dzx, "ACTR");
    PcRandoDzxFingerprint fp = {"Ship", -1, nullptr, 0};
    for (uint32_t i = 0; i < n; i++) {
        if (pc_rando_dzx_check(dzx, "ACTR", i, &fp) != PCR_DZX_OK) {
            continue;
        }
        if (index != nullptr) {
            *index = i;
        }
        int r = PCR_DZX_OK;
        // ACTR: name[8], u32 params, f32 x y z (0x0C), u16 x_rot y_rot z_rot (0x18), u16 enemy number.
        if (spawn != nullptr) {
            r = pc_rando_dzx_set_f32(dzx, "ACTR", i, 0x0C, spawn->x, &fp);
            if (r == PCR_DZX_OK) r = pc_rando_dzx_set_f32(dzx, "ACTR", i, 0x10, spawn->y, &fp);
            if (r == PCR_DZX_OK) r = pc_rando_dzx_set_f32(dzx, "ACTR", i, 0x14, spawn->z, &fp);
            if (r == PCR_DZX_OK) r = pc_rando_dzx_set_u16(dzx, "ACTR", i, 0x1A, spawn->yRot, &fp);
        }
        if (r == PCR_DZX_OK) {
            r = pc_rando_dzx_set_f32(dzx, "ACTR", i, 0x10, PCR_START_SHIP_Y, &fp);
        }
        return r;
    }
    return PCR_DZX_NO_CHUNK;
}

uint32_t pc_rando_start_yaz0_size(const void* src, size_t srcLen) {
    const uint8_t* s = (const uint8_t*)src;
    if (s == nullptr || srcLen < 16 || memcmp(s, "Yaz0", 4) != 0) {
        return 0;
    }
    return be32(s + 4);
}

int pc_rando_start_yaz0_expand(const void* src, size_t srcLen, void* dst, size_t cap) {
    const uint32_t size = pc_rando_start_yaz0_size(src, srcLen);
    if (size == 0 || dst == nullptr || cap < size) {
        return 0;
    }
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* out = (uint8_t*)dst;
    size_t sp = 16;
    uint32_t dp = 0;
    while (dp < size) {
        if (sp >= srcLen) return 0;
        uint8_t code = s[sp++];
        for (int bit = 0; bit < 8 && dp < size; bit++, code = (uint8_t)(code << 1)) {
            if (code & 0x80) {
                if (sp >= srcLen) return 0;
                out[dp++] = s[sp++];
                continue;
            }
            if (sp + 1 >= srcLen) return 0;
            const uint32_t dist = (((uint32_t)(s[sp] & 0x0F) << 8) | s[sp + 1]) + 1;
            uint32_t len = s[sp] >> 4;
            sp += 2;
            if (len == 0) {
                if (sp >= srcLen) return 0;
                len = (uint32_t)s[sp++] + 0x12;
            } else {
                len += 2;
            }
            if (dist > dp || dp + len > size) return 0;
            for (uint32_t i = 0; i < len; i++, dp++) out[dp] = out[dp - dist];
        }
    }
    return 1;
}

int pc_rando_start_rarc_find(const void* arc, size_t len, const char* name, const uint8_t** data,
                             uint32_t* size) {
    const uint8_t* b = (const uint8_t*)arc;
    if (b == nullptr || name == nullptr || len < 0x40 || memcmp(b, "RARC", 4) != 0) {
        return 0;
    }
    const uint32_t headerLength = be32(b + 0x08), fileData = be32(b + 0x0C);
    if ((uint64_t)headerLength + 0x20 > len) {
        return 0;
    }
    const uint8_t* info = b + headerLength;
    const uint32_t numEntries = be32(info + 8), entryOff = be32(info + 0x0C), stringOff = be32(info + 0x14);
    if ((uint64_t)headerLength + entryOff + numEntries * 0x14ull > len || (uint64_t)headerLength + stringOff > len ||
        (uint64_t)headerLength + fileData > len) {
        return 0;
    }
    const char* strings = (const char*)info + stringOff;
    const size_t stringsLen = len - headerLength - stringOff;
    const uint8_t* fileBase = b + headerLength + fileData;
    const size_t dataLen = len - headerLength - fileData;
    const size_t want = strlen(name);
    for (uint32_t k = 0; k < numEntries; k++) {
        const uint8_t* e = info + entryOff + k * 0x14;
        const uint32_t tf = be32(e + 4);
        const uint32_t nameOff = tf & 0xFFFFFF;
        const uint8_t flags = (uint8_t)(tf >> 24);
        if ((flags & 0x02) || nameOff >= stringsLen) {
            continue; // a directory
        }
        if (strnlen(strings + nameOff, stringsLen - nameOff) != want || memcmp(strings + nameOff, name, want) != 0) {
            continue;
        }
        const uint32_t off = be32(e + 8), sz = be32(e + 0x0C);
        if ((uint64_t)off + sz > dataLen) {
            return 0;
        }
        *data = fileBase + off;
        *size = sz;
        return 1;
    }
    return 0;
}

} // extern "C"
