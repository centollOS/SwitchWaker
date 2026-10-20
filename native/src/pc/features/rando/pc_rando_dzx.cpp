// The dzs/dzr patch library (pc_rando_dzx.h, docs/RANDOMIZER_PLAN.md step R0.9). Pure: no game
// headers and no game calls (cos_rando_core links it into the unit tests without the game), and no
// allocation (grown copies go to a buffer the caller owns; R0.10 puts it on the archive heap).
#include "pc_rando_dzx.h"

#include <cstring>

namespace {

struct SizeRow {
    char tag[5];
    uint32_t size;
};

// Entry sizes from wwrando's wwlib/dzx.py (DATA_SIZE of each class), checked against the game's
// chunk table (dStage_dt_c_stageLoader / roomLoader, game/src/d/d_stage.cpp). Layer chunks
// (ACT0.., TRE0.., SCO0..) are mapped to their base before the lookup.
constexpr SizeRow kSizes[] = {
    {"ACTR", 0x20}, {"TRES", 0x20}, {"SCOB", 0x24}, {"TGOB", 0x20}, {"TGSC", 0x24}, {"DOOR", 0x24},
    {"TGDR", 0x24}, {"PLYR", 0x20}, {"SCLS", 0x0C}, {"STAG", 0x14}, {"FILI", 0x08}, {"SHIP", 0x10},
    {"RTBL", 0x04}, {"RPAT", 0x0C}, {"RPPN", 0x10}, {"PATH", 0x0C}, {"PPNT", 0x10}, {"EVNT", 0x18},
    {"2DMA", 0x38}, {"2Dma", 0x38}, {"MULT", 0x0C}, {"Pale", 0x2C}, {"Virt", 0x24}, {"FLOR", 0x14},
    {"LBNK", 0x01}, {"SOND", 0x1C}, {"RCAM", 0x14}, {"RARO", 0x14}, {"DMAP", 0x10}, {"EnvR", 0x08},
    {"Colo", 0x0C}, {"LGHT", 0x1C}, {"LGTV", 0x1C}, {"MECO", 0x02}, {"MEMA", 0x04}, {"CAMR", 0x14},
    {"AROB", 0x14},
};

// Chunks whose entries start with the 8-byte actor name read by dStage_searchName.
constexpr const char* kActorTags[] = {"ACTR", "TRES", "SCOB", "TGOB", "TGSC", "DOOR", "TGDR", "PLYR"};

constexpr uint32_t kRowSize = 0xC;
constexpr uint32_t kMaxAdds = 64;

uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

size_t alignUp(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

bool tagEq(const uint8_t* row, const char* tag) {
    return memcmp(row, tag, 4) == 0;
}

// "ACT3" -> "ACTR", "TRE0" -> "TRES", "SCOb" -> "SCOB"; anything else unchanged. The 4th character of
// a layer chunk is a lowercase hex digit 0..b (wwrando's DZxLayer).
void baseTag(const char* tag, char out[5]) {
    memcpy(out, tag, 4);
    out[4] = 0;
    const char c = tag[3];
    const bool layer = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'b');
    if (!layer) {
        return;
    }
    if (memcmp(tag, "ACT", 3) == 0) {
        out[3] = 'R';
    } else if (memcmp(tag, "TRE", 3) == 0) {
        out[3] = 'S';
    } else if (memcmp(tag, "SCO", 3) == 0) {
        out[3] = 'B';
    }
}

bool validTag(const char* tag) {
    return tag != nullptr && strnlen(tag, 5) == 4;
}

bool isActorTag(const char* tag) {
    char base[5];
    baseTag(tag, base);
    for (const char* t : kActorTags) {
        if (memcmp(base, t, 4) == 0) {
            return true;
        }
    }
    return false;
}

const uint8_t* rowAt(const uint8_t* data, uint32_t i) {
    return data + 4 + i * kRowSize;
}

// Header and chunk table checks shared by open and grow.
int checkFile(const uint8_t* data, size_t size, uint32_t* chunkCount) {
    if (data == nullptr || size < 4) {
        return PCR_DZX_BAD_FILE;
    }
    const uint32_t n = rd32(data);
    if (n > (size - 4) / kRowSize) {
        return PCR_DZX_BAD_FILE;
    }
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* row = rowAt(data, i);
        const uint32_t count = rd32(row + 4);
        const uint32_t off = rd32(row + 8);
        char tag[5];
        memcpy(tag, row, 4);
        tag[4] = 0;
        const uint32_t es = pc_rando_dzx_entry_size(tag);
        if (count == 0) {
            continue;
        }
        if (off > size) {
            return PCR_DZX_BAD_FILE;
        }
        if (es != 0 && (uint64_t)count * es > size - off) {
            return PCR_DZX_BAD_FILE;
        }
    }
    *chunkCount = n;
    return PCR_DZX_OK;
}

int lookup(const PcRandoDzx* dzx, const char* tag, uint32_t index, uint8_t** entry, uint32_t* esize) {
    if (dzx == nullptr || dzx->data == nullptr || !validTag(tag)) {
        return PCR_DZX_BAD_FILE;
    }
    const int c = pc_rando_dzx_find(dzx, tag);
    if (c < 0) {
        return PCR_DZX_NO_CHUNK;
    }
    const uint32_t es = pc_rando_dzx_entry_size(tag);
    if (es == 0) {
        return PCR_DZX_UNKNOWN_SIZE;
    }
    const uint8_t* row = rowAt(dzx->data, (uint32_t)c);
    if (index >= rd32(row + 4)) {
        return PCR_DZX_BAD_INDEX;
    }
    *entry = dzx->data + rd32(row + 8) + (size_t)index * es;
    *esize = es;
    return PCR_DZX_OK;
}

int checkFp(const uint8_t* entry, uint32_t es, const PcRandoDzxFingerprint* fp) {
    if (fp == nullptr) {
        return PCR_DZX_OK;
    }
    if (fp->name != nullptr) {
        char name[8] = {};
        strncpy(name, fp->name, sizeof name);
        if (memcmp(entry, name, sizeof name) != 0) {
            return PCR_DZX_MISMATCH;
        }
    }
    if (fp->off >= 0) {
        if (fp->bytes == nullptr || (uint64_t)(uint32_t)fp->off + fp->len > es) {
            return PCR_DZX_BAD_RANGE;
        }
        if (memcmp(entry + fp->off, fp->bytes, fp->len) != 0) {
            return PCR_DZX_MISMATCH;
        }
    }
    return PCR_DZX_OK;
}

struct Plan {
    uint32_t chunkCount;    // rows in src
    uint32_t newRows;       // tags without a chunk in src
    char newTags[kMaxAdds][4];
    size_t shift;           // bytes the data after the table moves (0 or 0x20-aligned)
    size_t size;            // size of the grown copy
};

// Lays out the grown copy: src (shifted when rows are added) up to its end aligned to 4, then each
// grown chunk (old entries then new ones) aligned to 4, in the order of the first add of its tag,
// then 0xFF padding to 0x20.
int planGrow(const uint8_t* src, size_t srcSize, const PcRandoDzxAdd* adds, int nAdds, Plan* p) {
    memset(p, 0, sizeof *p);
    int r = checkFile(src, srcSize, &p->chunkCount);
    if (r != PCR_DZX_OK) {
        return r;
    }
    if (nAdds < 0 || (uint32_t)nAdds > kMaxAdds || (nAdds > 0 && adds == nullptr)) {
        return PCR_DZX_BAD_RANGE;
    }
    PcRandoDzx view = {const_cast<uint8_t*>(src), srcSize, p->chunkCount};
    // New rows go first, since they decide the shift; the layout pass below then matches grow's.
    for (int i = 0; i < nAdds; i++) {
        const PcRandoDzxAdd& a = adds[i];
        if (!validTag(a.tag) || (a.count > 0 && a.entries == nullptr)) {
            return PCR_DZX_BAD_RANGE;
        }
        const uint32_t es = pc_rando_dzx_entry_size(a.tag);
        if (es == 0 || memcmp(a.tag, "RTBL", 4) == 0) {
            return PCR_DZX_UNKNOWN_SIZE; // RTBL entries point at sub-tables; never grown
        }
        bool known = pc_rando_dzx_find(&view, a.tag) >= 0;
        for (uint32_t j = 0; j < p->newRows && !known; j++) {
            known = memcmp(p->newTags[j], a.tag, 4) == 0;
        }
        if (!known) {
            memcpy(p->newTags[p->newRows++], a.tag, 4);
        }
    }
    // Shifting by a multiple of 0x20 keeps every chunk's alignment.
    p->shift = p->newRows == 0 ? 0 : alignUp((size_t)p->newRows * kRowSize, 0x20);
    size_t pos = alignUp(srcSize + p->shift, 4);
    for (int i = 0; i < nAdds; i++) {
        const uint32_t es = pc_rando_dzx_entry_size(adds[i].tag);
        bool seen = false;
        for (int j = 0; j < i; j++) {
            seen = seen || memcmp(adds[j].tag, adds[i].tag, 4) == 0;
        }
        if (!seen) {
            pos = alignUp(pos, 4) + (size_t)pc_rando_dzx_count(&view, adds[i].tag) * es;
        }
        pos += (size_t)adds[i].count * es;
    }
    p->size = alignUp(pos, 0x20);
    return PCR_DZX_OK;
}

} // namespace

const char* pc_rando_dzx_result_name(int result) {
    switch (result) {
    case PCR_DZX_OK: return "ok";
    case PCR_DZX_BAD_FILE: return "bad file";
    case PCR_DZX_NO_CHUNK: return "no chunk";
    case PCR_DZX_BAD_INDEX: return "bad index";
    case PCR_DZX_BAD_RANGE: return "bad range";
    case PCR_DZX_MISMATCH: return "fingerprint mismatch";
    case PCR_DZX_UNKNOWN_SIZE: return "unknown entry size";
    case PCR_DZX_NO_SPACE: return "no space";
    default: return "?";
    }
}

int pc_rando_dzx_tag(const char* base, int layer, char out[5]) {
    if (!validTag(base) || out == nullptr || layer < -1 || layer > 11) {
        return 0;
    }
    memcpy(out, base, 4);
    out[4] = 0;
    if (layer < 0) {
        return 1;
    }
    if (memcmp(base, "ACTR", 4) != 0 && memcmp(base, "TRES", 4) != 0 && memcmp(base, "SCOB", 4) != 0) {
        return 0;
    }
    out[3] = "0123456789ab"[layer];
    return 1;
}

uint32_t pc_rando_dzx_entry_size(const char* tag) {
    if (!validTag(tag)) {
        return 0;
    }
    char base[5];
    baseTag(tag, base);
    for (const SizeRow& r : kSizes) {
        if (memcmp(r.tag, base, 4) == 0) {
            return r.size;
        }
    }
    return 0;
}

int pc_rando_dzx_open(PcRandoDzx* dzx, void* data, size_t size) {
    if (dzx == nullptr) {
        return PCR_DZX_BAD_FILE;
    }
    uint32_t n = 0;
    const int r = checkFile((const uint8_t*)data, size, &n);
    if (r != PCR_DZX_OK) {
        memset(dzx, 0, sizeof *dzx);
        return r;
    }
    dzx->data = (uint8_t*)data;
    dzx->size = size;
    dzx->chunkCount = n;
    return PCR_DZX_OK;
}

int pc_rando_dzx_find(const PcRandoDzx* dzx, const char* tag) {
    if (dzx == nullptr || dzx->data == nullptr || !validTag(tag)) {
        return -1;
    }
    // The first row wins, as in dStage_dt_c_decode.
    for (uint32_t i = 0; i < dzx->chunkCount; i++) {
        if (tagEq(rowAt(dzx->data, i), tag)) {
            return (int)i;
        }
    }
    return -1;
}

uint32_t pc_rando_dzx_count(const PcRandoDzx* dzx, const char* tag) {
    const int c = pc_rando_dzx_find(dzx, tag);
    return c < 0 ? 0 : rd32(rowAt(dzx->data, (uint32_t)c) + 4);
}

uint8_t* pc_rando_dzx_entry(const PcRandoDzx* dzx, const char* tag, uint32_t index) {
    uint8_t* e = nullptr;
    uint32_t es = 0;
    return lookup(dzx, tag, index, &e, &es) == PCR_DZX_OK ? e : nullptr;
}

int pc_rando_dzx_check(const PcRandoDzx* dzx, const char* tag, uint32_t index,
                       const PcRandoDzxFingerprint* fp) {
    uint8_t* e = nullptr;
    uint32_t es = 0;
    const int r = lookup(dzx, tag, index, &e, &es);
    return r != PCR_DZX_OK ? r : checkFp(e, es, fp);
}

int pc_rando_dzx_set_field(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off,
                           const void* bytes, uint32_t len, const PcRandoDzxFingerprint* fp) {
    uint8_t* e = nullptr;
    uint32_t es = 0;
    int r = lookup(dzx, tag, index, &e, &es);
    if (r != PCR_DZX_OK) {
        return r;
    }
    if (bytes == nullptr || (uint64_t)off + len > es) {
        return PCR_DZX_BAD_RANGE;
    }
    r = checkFp(e, es, fp);
    if (r != PCR_DZX_OK) {
        return r;
    }
    memmove(e + off, bytes, len);
    return PCR_DZX_OK;
}

int pc_rando_dzx_set_u8(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, uint8_t v,
                        const PcRandoDzxFingerprint* fp) {
    return pc_rando_dzx_set_field(dzx, tag, index, off, &v, 1, fp);
}

int pc_rando_dzx_set_u16(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, uint16_t v,
                         const PcRandoDzxFingerprint* fp) {
    const uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
    return pc_rando_dzx_set_field(dzx, tag, index, off, b, 2, fp);
}

int pc_rando_dzx_set_u32(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, uint32_t v,
                         const PcRandoDzxFingerprint* fp) {
    uint8_t b[4];
    wr32(b, v);
    return pc_rando_dzx_set_field(dzx, tag, index, off, b, 4, fp);
}

int pc_rando_dzx_set_f32(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, float v,
                         const PcRandoDzxFingerprint* fp) {
    uint32_t u;
    memcpy(&u, &v, 4);
    return pc_rando_dzx_set_u32(dzx, tag, index, off, u, fp);
}

int pc_rando_dzx_suppress(PcRandoDzx* dzx, const char* tag, uint32_t index,
                          const PcRandoDzxFingerprint* fp) {
    if (!validTag(tag)) {
        return PCR_DZX_BAD_FILE;
    }
    if (!isActorTag(tag)) {
        return PCR_DZX_UNKNOWN_SIZE;
    }
    static const uint8_t kEmpty[8] = {};
    return pc_rando_dzx_set_field(dzx, tag, index, 0, kEmpty, sizeof kEmpty, fp);
}

uint16_t pc_rando_dzx_be16(const uint8_t* p) {
    return (uint16_t)(p[0] << 8 | p[1]);
}

uint32_t pc_rando_dzx_be32(const uint8_t* p) {
    return rd32(p);
}

float pc_rando_dzx_bef32(const uint8_t* p) {
    const uint32_t u = rd32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

size_t pc_rando_dzx_grown_size(const void* src, size_t srcSize, const PcRandoDzxAdd* adds, int nAdds,
                               int* result) {
    Plan p;
    const int r = planGrow((const uint8_t*)src, srcSize, adds, nAdds, &p);
    if (result != nullptr) {
        *result = r;
    }
    return r == PCR_DZX_OK ? p.size : 0;
}

int pc_rando_dzx_grow(const void* srcv, size_t srcSize, const PcRandoDzxAdd* adds, int nAdds, void* dstv,
                      size_t cap, size_t* outSize, uint32_t* firstIndex) {
    const uint8_t* src = (const uint8_t*)srcv;
    uint8_t* dst = (uint8_t*)dstv;
    Plan p;
    int r = planGrow(src, srcSize, adds, nAdds, &p);
    if (r != PCR_DZX_OK) {
        return r;
    }
    if (dst == nullptr || cap < p.size) {
        return PCR_DZX_NO_SPACE;
    }
    memset(dst, 0xFF, p.size);

    // 1. The old file, the data after the table moved by p.shift; the new rows after the old ones.
    const size_t tableEnd = 4 + (size_t)p.chunkCount * kRowSize;
    memcpy(dst, src, tableEnd);
    memcpy(dst + tableEnd + p.shift, src + tableEnd, srcSize - tableEnd);
    wr32(dst, p.chunkCount + p.newRows);
    for (uint32_t i = 0; i < p.newRows; i++) {
        uint8_t* row = dst + tableEnd + (size_t)i * kRowSize;
        memcpy(row, p.newTags[i], 4);
        wr32(row + 4, 0);
        wr32(row + 8, 0);
    }
    if (p.shift != 0) {
        // Fix the absolute file offsets that moved: the chunk rows, then RTBL's entry pointers and
        // each sub-table's room-list pointer (several entries may share one sub-table).
        uint32_t fixedSub[256];
        uint32_t nFixedSub = 0;
        for (uint32_t i = 0; i < p.chunkCount; i++) {
            uint8_t* row = dst + 4 + (size_t)i * kRowSize;
            const uint32_t off = rd32(row + 8);
            if (off >= tableEnd) {
                wr32(row + 8, off + (uint32_t)p.shift);
            }
        }
        for (uint32_t i = 0; i < p.chunkCount; i++) {
            uint8_t* row = dst + 4 + (size_t)i * kRowSize;
            if (!tagEq(row, "RTBL")) {
                continue;
            }
            const uint32_t count = rd32(row + 4);
            const uint32_t base = rd32(row + 8);
            for (uint32_t e = 0; e < count; e++) {
                uint8_t* ent = dst + base + (size_t)e * 4;
                const uint32_t sub = rd32(ent);
                if (sub < tableEnd || sub + 8 > srcSize) {
                    return PCR_DZX_BAD_FILE;
                }
                const uint32_t newSub = sub + (uint32_t)p.shift;
                wr32(ent, newSub);
                bool done = false;
                for (uint32_t k = 0; k < nFixedSub; k++) {
                    done = done || fixedSub[k] == newSub;
                }
                if (done) {
                    continue;
                }
                if (nFixedSub == sizeof fixedSub / sizeof fixedSub[0]) {
                    return PCR_DZX_BAD_FILE;
                }
                fixedSub[nFixedSub++] = newSub;
                const uint32_t rooms = rd32(dst + newSub + 4);
                if (rooms >= tableEnd) {
                    wr32(dst + newSub + 4, rooms + (uint32_t)p.shift);
                }
            }
        }
    }

    // 2. Each grown chunk at the end: its old entries, then every add of its tag in order.
    PcRandoDzx out = {dst, p.size, p.chunkCount + p.newRows};
    size_t pos = alignUp(srcSize + p.shift, 4);
    for (int i = 0; i < nAdds; i++) {
        bool seen = false;
        for (int j = 0; j < i; j++) {
            seen = seen || memcmp(adds[j].tag, adds[i].tag, 4) == 0;
        }
        if (seen) {
            continue;
        }
        const char* tag = adds[i].tag;
        const uint32_t es = pc_rando_dzx_entry_size(tag);
        uint8_t* row = dst + 4 + (size_t)pc_rando_dzx_find(&out, tag) * kRowSize;
        const uint32_t oldCount = rd32(row + 4);
        const uint32_t oldOff = rd32(row + 8);
        pos = alignUp(pos, 4);
        const size_t start = pos;
        if (oldCount != 0) {
            memcpy(dst + pos, dst + oldOff, (size_t)oldCount * es);
            pos += (size_t)oldCount * es;
        }
        uint32_t count = oldCount;
        for (int j = i; j < nAdds; j++) {
            if (memcmp(adds[j].tag, tag, 4) != 0) {
                continue;
            }
            if (firstIndex != nullptr) {
                firstIndex[j] = count;
            }
            if (adds[j].count != 0) {
                memcpy(dst + pos, adds[j].entries, (size_t)adds[j].count * es);
            }
            pos += (size_t)adds[j].count * es;
            count += adds[j].count;
        }
        wr32(row + 4, count);
        wr32(row + 8, (uint32_t)start);
    }
    if (outSize != nullptr) {
        *outSize = p.size; // == alignUp(pos, 0x20): planGrow lays out the same way
    }
    return PCR_DZX_OK;
}
