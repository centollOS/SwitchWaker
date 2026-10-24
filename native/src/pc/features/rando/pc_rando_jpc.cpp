// pc_rando_jpc.cpp - particle-file (JPC) library (docs/RANDOMIZER_PLAN.md, step R7.1a). See the
// header for the format; the gclib file each piece ports is named next to it (gclib d9322523,
// gclib/jpc.py, jpa.py, jchunk.py, jpa_chunks/*.py, bti.py).
#include "pc_rando_jpc.h"

#include <string.h>

namespace {

constexpr uint32_t kListOffset = 0x20;     // jpc.py PARTICLE_LIST_OFFSET[JPAC1_00]
constexpr uint32_t kParticleHeader = 0x20; // jpa.py PARTICLE_HEADER_SIZE[JPAC1_00]
constexpr uint32_t kTdb1Ids = 0xC;         // tdb1.py TDB1_ID_LIST_OFFSET[JPAC1_00]
constexpr uint32_t kBsp1Keys = 0x60;       // bsp1.py BSP1_JPC100.DATA_SIZE (texture index keys start)
constexpr uint32_t kTex1Bti = 0x20;        // tex1.py: the BTI header starts at chunk + 0x20

uint16_t rd16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
void wr16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

using Bytes = std::vector<uint8_t>;

// fs_helpers.align_data_to_nearest with b'\0'.
void padTo(Bytes& b, size_t align) {
    while (b.size() % align != 0) {
        b.push_back(0);
    }
}

// The smallest block sizes that hold every field gclib's bunfoe classes read (DATA_SIZE).
uint32_t minBlockSize(const char* magic) {
    if (memcmp(magic, "BEM1", 4) == 0) {
        return 0x8C; // BEM1_JPC100: 8 + 4 + 0x80
    }
    if (memcmp(magic, "BSP1", 4) == 0) {
        return kBsp1Keys; // 8 + 4 + 0x54
    }
    if (memcmp(magic, "ESP1", 4) == 0) {
        return 0x6C; // 8 + 4 + 0x60
    }
    if (memcmp(magic, "ETX1", 4) == 0) {
        return 0x30; // 8 + 4 + 0x24
    }
    if (memcmp(magic, "SSP1", 4) == 0) {
        return 0x54; // 8 + 4 + 0x48
    }
    if (memcmp(magic, "TDB1", 4) == 0) {
        return kTdb1Ids;
    }
    return 8; // KFA1, FLD1 and unknown blocks: gclib's plain JPAChunk
}

// One BSP1 colour key table: true when it fits in the block (bsp1.py read_chunk_specific_data).
bool keyTableFits(const uint8_t* blk, uint32_t size, uint32_t offField, uint32_t countField) {
    uint32_t n = blk[countField];
    if (n == 0) {
        return true;
    }
    int16_t off = (int16_t)rd16(blk + offField);
    return off >= 0 && (uint32_t)off + n * 6 <= size;
}

// The size of a BTI's image data (bti.py get_mipmap_offset_and_size(mipmap_count)), or false for an
// image format gclib does not know.
bool btiImageSize(uint8_t format, uint32_t width, uint32_t height, uint32_t mips, uint32_t* out) {
    uint32_t bw, bh, bs;
    switch (format) {
    case 0x0: bw = 8; bh = 8; bs = 32; break; // I4
    case 0x1: bw = 8; bh = 4; bs = 32; break; // I8
    case 0x2: bw = 8; bh = 4; bs = 32; break; // IA4
    case 0x3: bw = 4; bh = 4; bs = 32; break; // IA8
    case 0x4: bw = 4; bh = 4; bs = 32; break; // RGB565
    case 0x5: bw = 4; bh = 4; bs = 32; break; // RGB5A3
    case 0x6: bw = 4; bh = 4; bs = 64; break; // RGBA32
    case 0x8: bw = 8; bh = 8; bs = 32; break; // C4
    case 0x9: bw = 8; bh = 4; bs = 32; break; // C8
    case 0xA: bw = 4; bh = 4; bs = 32; break; // C14X2
    case 0xE: bw = 8; bh = 8; bs = 32; break; // CMPR
    default: return false;
    }
    uint32_t total = 0;
    for (uint32_t i = 0; i < mips; i++) {
        total += ((width + bw - 1) / bw) * ((height + bh - 1) / bh) * bs;
        width /= 2;
        height /= 2;
    }
    *out = total;
    return true;
}

bool usesPalette(uint8_t format) { return format == 0x8 || format == 0x9 || format == 0xA; }

// bti.py get_max_valid_mipmap_count.
uint32_t maxMipmaps(uint32_t width, uint32_t height) {
    if (width == 1 || height == 1) {
        return 1;
    }
    uint32_t n = 0;
    while (true) {
        width /= 2;
        height /= 2;
        if (width == 0 || height == 0) {
            break;
        }
        n++;
    }
    return n;
}

// The BTI checks gclib's BTI.read_header / save_header_changes make (enums, mipmap count) and the
// numbers the TEX1 save needs. bti points at the BTI header (chunk + 0x20).
struct BtiInfo {
    uint8_t format;
    uint32_t mips;      // header value, 0 read as 1
    uint32_t imageSize; // all mipmaps
    uint32_t imageOffset;
    uint32_t paletteOffset;
    uint32_t paletteSize;
};

bool readBti(const uint8_t* bti, BtiInfo* out) {
    uint8_t format = bti[0];
    uint32_t width = rd16(bti + 2), height = rd16(bti + 4);
    if (bti[6] > 2 || bti[7] > 2 || bti[9] > 2 || bti[0x14] > 5 || bti[0x15] > 5) {
        return false; // WrapMode, PaletteFormat, FilterMode
    }
    uint32_t mips = bti[0x18] == 0 ? 1 : bti[0x18];
    if (!btiImageSize(format, width, height, mips, &out->imageSize)) {
        return false; // ImageFormat
    }
    if (mips > maxMipmaps(width, height)) {
        return false; // save_header_changes asserts this
    }
    out->format = format;
    out->mips = mips;
    out->paletteSize = (uint32_t)rd16(bti + 0xA) * 2;
    out->paletteOffset = rd32(bti + 0xC);
    out->imageOffset = rd32(bti + 0x1C);
    return true;
}

// gclib JParticle(read_header_jpc100 + read_chunks) at data+off: fills p (offsets in data) or fails.
int scanParticle(const uint8_t* data, size_t size, uint32_t off, PcRandoJpcParticle* p) {
    if ((size_t)off + kParticleHeader > size) {
        return PCR_JPC_BAD_FILE;
    }
    const uint8_t* h = data + off;
    if (memcmp(h, "JEFFjpa1", 8) != 0 || h[0x17] != 0 || rd16(h + 0x1A) != 0 || rd16(h + 0x1C) != 0) {
        return PCR_JPC_BAD_FILE;
    }
    p->offset = off;
    p->numBlocks = rd32(h + 0xC);
    p->sizeField = rd32(h + 0x10);
    p->numKfa1 = h[0x14];
    p->numFld1 = h[0x15];
    p->numTextures = h[0x16];
    p->id = rd16(h + 0x18);
    p->blocks.clear();
    p->textureIds.clear();
    p->bem1 = p->bsp1 = p->esp1 = p->etx1 = p->ssp1 = p->tdb1 = -1;

    uint32_t bo = off + kParticleHeader;
    for (uint32_t i = 0; i < p->numBlocks; i++) {
        if ((size_t)bo + 8 > size) {
            return PCR_JPC_BAD_FILE;
        }
        PcRandoJpcBlock b;
        memcpy(b.magic, data + bo, 4);
        b.magic[4] = '\0';
        b.offset = bo;
        b.size = rd32(data + bo + 4);
        if (b.size < minBlockSize(b.magic) || (size_t)bo + b.size > size) {
            return PCR_JPC_BAD_FILE;
        }
        const uint8_t* blk = data + bo;
        int index = (int)p->blocks.size();
        if (memcmp(b.magic, "BEM1", 4) == 0) {
            p->bem1 = index;
        } else if (memcmp(b.magic, "BSP1", 4) == 0) {
            if (kBsp1Keys + (uint32_t)blk[0x25] > b.size || !keyTableFits(blk, b.size, 0x10, 0x28) ||
                !keyTableFits(blk, b.size, 0x12, 0x29)) {
                return PCR_JPC_BAD_FILE;
            }
            p->bsp1 = index;
        } else if (memcmp(b.magic, "ESP1", 4) == 0) {
            p->esp1 = index;
        } else if (memcmp(b.magic, "ETX1", 4) == 0) {
            p->etx1 = index;
        } else if (memcmp(b.magic, "SSP1", 4) == 0) {
            p->ssp1 = index;
        } else if (memcmp(b.magic, "TDB1", 4) == 0) {
            // Only the last TDB1 gets its ids read; gclib fails to save any other one.
            if (p->tdb1 >= 0 || kTdb1Ids + (uint32_t)p->numTextures * 2 > b.size) {
                return PCR_JPC_BAD_FILE;
            }
            p->tdb1 = index;
            for (uint32_t t = 0; t < p->numTextures; t++) {
                p->textureIds.push_back(rd16(blk + kTdb1Ids + t * 2));
            }
        }
        p->blocks.push_back(b);
        bo += b.size;
    }
    if (p->tdb1 < 0) {
        return PCR_JPC_BAD_FILE; // jpa.py read_chunks: self.tdb1.read_texture_ids
    }
    p->size = bo - off;
    return PCR_JPC_OK;
}

int scanTexture(const uint8_t* data, size_t size, uint32_t off, PcRandoJpcTexture* t, BtiInfo* bti) {
    if ((size_t)off + 8 > size || memcmp(data + off, "TEX1", 4) != 0) {
        return PCR_JPC_BAD_FILE;
    }
    t->offset = off;
    t->size = rd32(data + off + 4);
    if (t->size < kTex1Bti + 0x20 || (size_t)off + t->size > size) {
        return PCR_JPC_BAD_FILE;
    }
    // tex1.py: read_str_until_null_character(0xC) inside the chunk's data.
    t->filename.clear();
    for (uint32_t i = 0xC; i < t->size && data[off + i] != 0; i++) {
        t->filename.push_back((char)data[off + i]);
    }
    if (!readBti(data + off + kTex1Bti, bti)) {
        return PCR_JPC_BAD_FILE;
    }
    return PCR_JPC_OK;
}

// A block in gclib's saved layout (JChunk.save + the block's save_chunk_specific_data).
void normalizeBlock(const uint8_t* blk, const PcRandoJpcBlock& b, const PcRandoJpcParticle& p, Bytes& out) {
    size_t start = out.size();
    if (memcmp(b.magic, "BSP1", 4) == 0) {
        // bsp1.py BSP1_JPC100.save_chunk_specific_data: fields up to 0x60 kept, then the texture
        // index keys, the prm keys and the env keys, each table 4-aligned; empty tables get offset 0.
        out.insert(out.end(), blk, blk + kBsp1Keys);
        uint32_t texKeys = blk[0x25];
        out.insert(out.end(), blk + kBsp1Keys, blk + kBsp1Keys + texKeys);
        padTo(out, 4); // start is 0x20-aligned, so chunk-relative alignment is the same
        static const uint32_t kOff[2] = {0x10, 0x12}, kCount[2] = {0x28, 0x29};
        for (int t = 0; t < 2; t++) {
            uint32_t n = blk[kCount[t]];
            uint16_t newOff = 0;
            if (n != 0) {
                int16_t src = (int16_t)rd16(blk + kOff[t]);
                newOff = (uint16_t)(out.size() - start);
                out.insert(out.end(), blk + src, blk + src + n * 6);
                padTo(out, 4);
            }
            wr16(&out[start + kOff[t]], newOff);
        }
    } else if (memcmp(b.magic, "TDB1", 4) == 0) {
        // tdb1.py: cut after the header, then the ids (rewritten again by the JPC save).
        out.insert(out.end(), blk, blk + kTdb1Ids);
        for (uint16_t id : p.textureIds) {
            out.push_back((uint8_t)(id >> 8));
            out.push_back((uint8_t)id);
        }
    } else {
        // Every other block: its bunfoe fields read and written back unchanged, the rest kept.
        out.insert(out.end(), blk, blk + b.size);
    }
    padTo(out, 0x20); // JPAChunk JPAC1_00 padding_alignment_size, b'\0'
    wr32(&out[start + 4], (uint32_t)(out.size() - start));
}

// jpa.py JParticle.save (JPAC1_00).
void normalizeParticle(const uint8_t* data, const PcRandoJpcParticle& p, Bytes& out) {
    out.assign(data + p.offset, data + p.offset + kParticleHeader);
    for (const PcRandoJpcBlock& b : p.blocks) {
        normalizeBlock(data + b.offset, b, p, out);
    }
    // num_chunks, size (kept), counts, id are written back as read; unknown_6 and unknown_7 are
    // written as u32s at 0x1A and 0x1C, which zeroes 0x1A..0x1F.
    memset(&out[0x1A], 0, 6);
}

// tex1.py TEX1.save: the chunk's bytes kept, the BTI header written back (palettes_enabled from the
// format, max_lod from the mipmap count, mipmap_count 0 -> 1), the image data at chunk + 0x40 and
// the palette after it (palette formats only), with no truncation, then 0x20 padding.
void normalizeTexture(const uint8_t* chunk, uint32_t size, const BtiInfo& bti, Bytes& out) {
    out.assign(chunk, chunk + size);
    uint8_t* h = &out[kTex1Bti];
    h[8] = usesPalette(bti.format) ? 1 : 0;
    uint32_t maxLod = (bti.mips - 1) * 8;
    h[0x17] = (uint8_t)(maxLod > 0xFF ? 0xFF : maxLod);
    h[0x18] = (uint8_t)bti.mips;
    // BTI.read: read_bytes(offset, length) gives what lies there, possibly short.
    auto part = [&](uint32_t off, uint32_t len, Bytes& dst) {
        uint64_t from = (uint64_t)kTex1Bti + off;
        dst.clear();
        if (from < size) {
            uint64_t end = from + len < size ? from + len : size;
            dst.assign(chunk + from, chunk + end);
        }
    };
    Bytes image, palette;
    part(bti.imageOffset, bti.imageSize, image);
    part(bti.paletteOffset, bti.paletteSize, palette);
    size_t at = kTex1Bti + 0x20;
    auto put = [&](const Bytes& src) {
        if (out.size() < at + src.size()) {
            out.resize(at + src.size());
        }
        if (!src.empty()) {
            memcpy(&out[at], src.data(), src.size());
        }
        at += src.size();
    };
    put(image);
    if (usesPalette(bti.format)) {
        put(palette);
    }
    padTo(out, 0x20);
    wr32(&out[4], (uint32_t)out.size());
}

int scanAll(const uint8_t* data, size_t size, std::vector<PcRandoJpcParticle>* particles,
            std::vector<PcRandoJpcTexture>* textures, std::vector<BtiInfo>* btis) {
    if (data == nullptr || size < kListOffset) {
        return PCR_JPC_BAD_FILE;
    }
    if (memcmp(data, "JPAC1-00", 8) != 0) {
        return memcmp(data, "JPAC", 4) == 0 ? PCR_JPC_BAD_VERSION : PCR_JPC_BAD_FILE;
    }
    uint32_t numParticles = rd16(data + 8), numTextures = rd16(data + 0xA);
    uint32_t off = kListOffset;
    std::vector<PcRandoJpcParticle> ps;
    for (uint32_t i = 0; i < numParticles; i++) {
        PcRandoJpcParticle p;
        int r = scanParticle(data, size, off, &p);
        if (r != PCR_JPC_OK) {
            return r;
        }
        for (const PcRandoJpcParticle& q : ps) {
            if (q.id == p.id) {
                return PCR_JPC_DUPLICATE;
            }
        }
        off += p.size;
        ps.push_back(static_cast<PcRandoJpcParticle&&>(p));
    }
    std::vector<PcRandoJpcTexture> ts;
    std::vector<BtiInfo> bs;
    for (uint32_t i = 0; i < numTextures; i++) {
        PcRandoJpcTexture t;
        BtiInfo bti;
        int r = scanTexture(data, size, off, &t, &bti);
        if (r != PCR_JPC_OK) {
            return r;
        }
        for (const PcRandoJpcTexture& u : ts) {
            if (u.filename == t.filename) {
                return PCR_JPC_DUPLICATE;
            }
        }
        off += t.size;
        ts.push_back(t);
        bs.push_back(bti);
    }
    for (const PcRandoJpcParticle& p : ps) {
        for (uint16_t id : p.textureIds) {
            if (id >= ts.size()) {
                return PCR_JPC_BAD_FILE; // jpc.py: self.textures[texture_id]
            }
        }
    }
    if (particles != nullptr) {
        *particles = static_cast<std::vector<PcRandoJpcParticle>&&>(ps);
    }
    if (textures != nullptr) {
        *textures = static_cast<std::vector<PcRandoJpcTexture>&&>(ts);
    }
    if (btis != nullptr) {
        *btis = static_cast<std::vector<BtiInfo>&&>(bs);
    }
    return PCR_JPC_OK;
}

// The colour field of a block, or NULL.
const uint8_t* colorField(const uint8_t* base, const PcRandoJpcParticle& p, int which) {
    int blk;
    uint32_t off;
    switch (which) {
    case PCR_JPC_BSP1_PRM: blk = p.bsp1; off = 0x2C; break;
    case PCR_JPC_BSP1_ENV: blk = p.bsp1; off = 0x30; break;
    case PCR_JPC_SSP1_PRM: blk = p.ssp1; off = 0x48; break;
    case PCR_JPC_SSP1_ENV: blk = p.ssp1; off = 0x4C; break;
    default: return nullptr;
    }
    if (base == nullptr || blk < 0 || (size_t)blk >= p.blocks.size()) {
        return nullptr;
    }
    return base + p.blocks[blk].offset + off;
}

// The BSP1 key table of which: its first key and count.
const uint8_t* keyTable(const uint8_t* base, const PcRandoJpcParticle& p, int which, uint32_t* count) {
    *count = 0;
    if (base == nullptr || p.bsp1 < 0 || (size_t)p.bsp1 >= p.blocks.size() ||
        (which != PCR_JPC_BSP1_PRM && which != PCR_JPC_BSP1_ENV)) {
        return nullptr;
    }
    const uint8_t* blk = base + p.blocks[p.bsp1].offset;
    bool env = which == PCR_JPC_BSP1_ENV;
    *count = blk[env ? 0x29 : 0x28];
    return blk + (int16_t)rd16(blk + (env ? 0x12 : 0x10));
}

} // namespace

const char* pc_rando_jpc_result_name(int result) {
    switch (result) {
    case PCR_JPC_OK: return "ok";
    case PCR_JPC_BAD_FILE: return "bad file";
    case PCR_JPC_BAD_VERSION: return "bad version";
    case PCR_JPC_DUPLICATE: return "duplicate";
    case PCR_JPC_NO_PARTICLE: return "no particle";
    case PCR_JPC_EXISTS: return "exists";
    case PCR_JPC_NO_TEXTURE: return "no texture";
    case PCR_JPC_NO_BLOCK: return "no block";
    case PCR_JPC_BAD_INDEX: return "bad index";
    case PCR_JPC_NO_SPACE: return "no space";
    default: return "?";
    }
}

int pc_rando_jpc_scan(const uint8_t* data, size_t size, std::vector<PcRandoJpcParticle>* particles,
                      std::vector<PcRandoJpcTexture>* textures) {
    return scanAll(data, size, particles, textures, nullptr);
}

bool pc_rando_jpc_has_particle(const uint8_t* data, size_t size, uint16_t id) {
    std::vector<PcRandoJpcParticle> ps;
    if (scanAll(data, size, &ps, nullptr, nullptr) != PCR_JPC_OK) {
        return false;
    }
    for (const PcRandoJpcParticle& p : ps) {
        if (p.id == id) {
            return true;
        }
    }
    return false;
}

int pc_rando_jpc_get_color(const uint8_t* base, const PcRandoJpcParticle& p, int which, PcRandoJpcRgba* out) {
    const uint8_t* c = colorField(base, p, which);
    if (c == nullptr) {
        return PCR_JPC_NO_BLOCK;
    }
    out->r = c[0];
    out->g = c[1];
    out->b = c[2];
    out->a = c[3];
    return PCR_JPC_OK;
}

int pc_rando_jpc_set_color(uint8_t* base, const PcRandoJpcParticle& p, int which, PcRandoJpcRgba color) {
    uint8_t* c = const_cast<uint8_t*>(colorField(base, p, which));
    if (c == nullptr) {
        return PCR_JPC_NO_BLOCK;
    }
    c[0] = color.r;
    c[1] = color.g;
    c[2] = color.b;
    c[3] = color.a;
    return PCR_JPC_OK;
}

uint32_t pc_rando_jpc_key_count(const uint8_t* base, const PcRandoJpcParticle& p, int which) {
    uint32_t n;
    keyTable(base, p, which, &n);
    return n;
}

int pc_rando_jpc_get_key(const uint8_t* base, const PcRandoJpcParticle& p, int which, uint32_t index,
                         uint16_t* time, PcRandoJpcRgba* color) {
    uint32_t n;
    const uint8_t* k = keyTable(base, p, which, &n);
    if (k == nullptr) {
        return PCR_JPC_NO_BLOCK;
    }
    if (index >= n) {
        return PCR_JPC_BAD_INDEX;
    }
    k += index * 6;
    if (time != nullptr) {
        *time = rd16(k);
    }
    if (color != nullptr) {
        color->r = k[2];
        color->g = k[3];
        color->b = k[4];
        color->a = k[5];
    }
    return PCR_JPC_OK;
}

int pc_rando_jpc_set_key_color(uint8_t* base, const PcRandoJpcParticle& p, int which, uint32_t index,
                               PcRandoJpcRgba color) {
    uint32_t n;
    uint8_t* k = const_cast<uint8_t*>(keyTable(base, p, which, &n));
    if (k == nullptr) {
        return PCR_JPC_NO_BLOCK;
    }
    if (index >= n) {
        return PCR_JPC_BAD_INDEX;
    }
    k += index * 6;
    k[2] = color.r;
    k[3] = color.g;
    k[4] = color.b;
    k[5] = color.a;
    return PCR_JPC_OK;
}

int pc_rando_jpc_load(PcRandoJpcFile* f, const uint8_t* data, size_t size) {
    std::vector<PcRandoJpcParticle> ps;
    std::vector<PcRandoJpcTexture> ts;
    std::vector<BtiInfo> bs;
    int r = scanAll(data, size, &ps, &ts, &bs);
    if (r != PCR_JPC_OK) {
        return r;
    }
    memcpy(f->header, data, sizeof f->header);
    f->particles.clear();
    f->textures.clear();
    f->particles.resize(ps.size());
    for (size_t i = 0; i < ps.size(); i++) {
        PcRandoJpcFile::Particle& dst = f->particles[i];
        normalizeParticle(data, ps[i], dst.bytes);
        r = scanParticle(dst.bytes.data(), dst.bytes.size(), 0, &dst.info);
        if (r != PCR_JPC_OK) {
            return r; // cannot happen: the saved layout is a valid particle
        }
        for (uint16_t id : ps[i].textureIds) {
            dst.textureNames.push_back(ts[id].filename);
        }
    }
    f->textures.resize(ts.size());
    for (size_t i = 0; i < ts.size(); i++) {
        normalizeTexture(data + ts[i].offset, ts[i].size, bs[i], f->textures[i].bytes);
        f->textures[i].filename = ts[i].filename;
    }
    return PCR_JPC_OK;
}

int pc_rando_jpc_find_particle(const PcRandoJpcFile& f, uint16_t id) {
    for (size_t i = 0; i < f.particles.size(); i++) {
        if (f.particles[i].info.id == id) {
            return (int)i;
        }
    }
    return -1;
}

int pc_rando_jpc_find_texture(const PcRandoJpcFile& f, const std::string& filename) {
    for (size_t i = 0; i < f.textures.size(); i++) {
        if (f.textures[i].filename == filename) {
            return (int)i;
        }
    }
    return -1;
}

int pc_rando_jpc_copy_particle(PcRandoJpcFile* dst, const PcRandoJpcFile& src, uint16_t id) {
    int si = pc_rando_jpc_find_particle(src, id);
    if (si < 0) {
        return PCR_JPC_NO_PARTICLE;
    }
    if (pc_rando_jpc_find_particle(*dst, id) >= 0) {
        return PCR_JPC_EXISTS;
    }
    const PcRandoJpcFile::Particle& p = src.particles[si];
    for (const std::string& name : p.textureNames) {
        if (pc_rando_jpc_find_texture(src, name) < 0) {
            return PCR_JPC_NO_TEXTURE;
        }
    }
    dst->particles.push_back(p);
    for (const std::string& name : p.textureNames) {
        if (pc_rando_jpc_find_texture(*dst, name) < 0) {
            dst->textures.push_back(src.textures[pc_rando_jpc_find_texture(src, name)]);
        }
    }
    return PCR_JPC_OK;
}

size_t pc_rando_jpc_saved_size(const PcRandoJpcFile& f) {
    size_t n = kListOffset;
    for (const PcRandoJpcFile::Particle& p : f.particles) {
        n += p.bytes.size();
    }
    n = (n + 0x1F) & ~(size_t)0x1F;
    for (const PcRandoJpcFile::Texture& t : f.textures) {
        n += t.bytes.size();
    }
    return (n + 0x1F) & ~(size_t)0x1F;
}

int pc_rando_jpc_save_into(const PcRandoJpcFile& f, uint8_t* out, size_t cap, size_t* written) {
    size_t total = pc_rando_jpc_saved_size(f);
    if (written != nullptr) {
        *written = 0;
    }
    if (out == nullptr || cap < total) {
        return PCR_JPC_NO_SPACE;
    }
    // TDB1 ids from filenames (jpc.py save: [tex.filename for tex in self.textures].index(name)).
    for (const PcRandoJpcFile::Particle& p : f.particles) {
        for (const std::string& name : p.textureNames) {
            if (pc_rando_jpc_find_texture(f, name) < 0) {
                return PCR_JPC_NO_TEXTURE;
            }
        }
    }
    memset(out, 0, total);
    memcpy(out, f.header, sizeof f.header);
    wr16(out + 8, (uint16_t)f.particles.size());
    wr16(out + 0xA, (uint16_t)f.textures.size());
    size_t at = kListOffset;
    for (const PcRandoJpcFile::Particle& p : f.particles) {
        memcpy(out + at, p.bytes.data(), p.bytes.size());
        uint8_t* ids = out + at + p.info.blocks[p.info.tdb1].offset + kTdb1Ids;
        for (size_t i = 0; i < p.textureNames.size(); i++) {
            wr16(ids + i * 2, (uint16_t)pc_rando_jpc_find_texture(f, p.textureNames[i]));
        }
        at += p.bytes.size();
    }
    at = (at + 0x1F) & ~(size_t)0x1F;
    for (const PcRandoJpcFile::Texture& t : f.textures) {
        memcpy(out + at, t.bytes.data(), t.bytes.size());
        at += t.bytes.size();
    }
    if (written != nullptr) {
        *written = total;
    }
    return PCR_JPC_OK;
}

int pc_rando_jpc_save(const PcRandoJpcFile& f, std::vector<uint8_t>* out) {
    out->assign(pc_rando_jpc_saved_size(f), 0);
    size_t n = 0;
    return pc_rando_jpc_save_into(f, out->data(), out->size(), &n);
}
