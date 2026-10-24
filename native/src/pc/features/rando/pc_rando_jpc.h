// pc_rando_jpc.h - particle-file (JPC) library (docs/RANDOMIZER_PLAN.md, step R7.1a). Pure: no game
// headers, no game calls. A port of gclib's jpc.py / jpa.py / jpa_chunks (MIT, the commit wwrando
// 1.10.0 pins), JPAC1-00 only (the version Wind Waker uses; JPAC2-10 is refused). Used by the particle
// bank hook (R7.1b) to merge particles into a scene's Pscene%03d.jpc and by the colour editors of the
// enemy palettes (R7.2e) and cosmetics (R7.5b). Every value is in the disc's big-endian format.
//
// File format (JPAC1-00):
//   0x00 "JPAC1-00", 0x08 u16 particle count, 0x0A u16 texture count, 0x0C..0x1F kept as is;
//   0x20 the particles, back to back, then the TEX1 textures, back to back (no gap: gclib reads the
//   first TEX1 right where the last particle's blocks end).
//   Particle: 0x20-byte header {"JEFFjpa1", u32 ?, u32 block count, u32 size (wrong in some files;
//   gclib and this library use the size of what was read instead), u8 KFA1 count, u8 FLD1 count,
//   u8 texture count, u8 0, u16 particle id, u16 0, u16 0, u16 ?}, then its blocks, each
//   {char magic[4], u32 size, ...}: BEM1 dynamics, BSP1 base shape, ESP1 extra shape, SSP1 sweep
//   shape, ETX1 indirect texture, KFA1 keys, FLD1 fields, TDB1 texture ids (u16 each at +0xC, one
//   per texture of the header count; they index the file's TEX1 list).
//   TEX1: {"TEX1", u32 size, u32 ?, char filename[0x14] (NUL-terminated, garbage may follow), then a
//   BTI header (0x20 bytes) and its image and palette data}.
//
// Two ways to use it:
//   - in place: pc_rando_jpc_scan lists the particles and textures of a raw file without copying it,
//     and the colour accessors read and write a particle's colours where they are (the size never
//     changes). This is what a colour editor on a scene's bytes uses.
//   - rebuilt: pc_rando_jpc_load makes a PcRandoJpcFile whose particles and textures are already in
//     gclib's saved layout (what gclib's JParticle.save / TEX1.save write: each block padded to 0x20,
//     BSP1 key tables laid out again, TDB1 cut after its ids, header bytes 0x1A..0x1F zero, the BTI
//     header's palette flag, max LOD and mipmap count normalised). pc_rando_jpc_copy_particle is
//     wwrando's tweaks.py add_inter_dungeon_warp_pots copy (deepcopy + add_particle + add every
//     texture the destination lacks), and pc_rando_jpc_save writes JPC.save()'s bytes (TDB1 ids
//     recomputed by filename), so a file saved here equals the one wwrando writes to its ISO.
//
// The PcRandoJpcFile containers allocate (std::vector): in the game that is the current JKR heap, so
// the caller (R7.1b) picks the heap; pc_rando_jpc_save_into writes into caller memory.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

enum PcRandoJpcResult {
    PCR_JPC_OK = 0,
    PCR_JPC_BAD_FILE,     // truncated or malformed header, particle, block or texture
    PCR_JPC_BAD_VERSION,  // not "JPAC1-00" (JPAC2-10 is not supported)
    PCR_JPC_DUPLICATE,    // two particles with one id, or two textures with one filename
    PCR_JPC_NO_PARTICLE,  // no particle with this id
    PCR_JPC_EXISTS,       // the destination already holds this particle id
    PCR_JPC_NO_TEXTURE,   // a TDB1 filename is missing from the file's textures
    PCR_JPC_NO_BLOCK,     // the particle has no block of the kind asked (BSP1 / SSP1)
    PCR_JPC_BAD_INDEX,    // key index out of the table
    PCR_JPC_NO_SPACE,     // the output buffer is too small
};

// A short name for logs ("ok", "bad file", ...).
const char* pc_rando_jpc_result_name(int result);

// One block of a particle. offset is from the start of the buffer the particle was found in.
struct PcRandoJpcBlock {
    char magic[5]; // 4 characters and a NUL
    uint32_t offset;
    uint32_t size; // the block's own size field (what gclib reads)
};

// A particle found by pc_rando_jpc_scan (offsets in that buffer) or held by a PcRandoJpcFile
// (offsets in its own bytes, offset 0).
struct PcRandoJpcParticle {
    uint32_t offset;     // the "JEFFjpa1" header
    uint32_t size;       // header + blocks as read (gclib's true size)
    uint32_t sizeField;  // the header's size field (kept, not trusted)
    uint16_t id;
    uint8_t numKfa1;
    uint8_t numFld1;
    uint8_t numTextures; // TDB1 id count
    uint32_t numBlocks;
    std::vector<PcRandoJpcBlock> blocks;
    // Index in blocks of the block of that kind (the last one when a kind repeats, as gclib's
    // setattr), -1 when absent. Every particle has a TDB1 (gclib needs it).
    int bem1, bsp1, esp1, etx1, ssp1, tdb1;
    std::vector<uint16_t> textureIds; // TDB1 ids, as stored
};

struct PcRandoJpcTexture {
    uint32_t offset; // the "TEX1" magic
    uint32_t size;   // the chunk's size field
    std::string filename;
};

// Lists the particles and textures of a raw JPC (either vector may be NULL). Checks what gclib's
// JPC100 constructor checks: version, headers, every block and texture in the buffer, known blocks
// large enough for their fields, BSP1 key tables in the block, TDB1 present and its ids valid
// texture indexes, BTI enums and mipmap count, no duplicate ids or filenames.
int pc_rando_jpc_scan(const uint8_t* data, size_t size, std::vector<PcRandoJpcParticle>* particles,
                      std::vector<PcRandoJpcTexture>* textures);

// True when a raw JPC holds a particle id (a cheap scan, no copy). False also for a bad file.
bool pc_rando_jpc_has_particle(const uint8_t* data, size_t size, uint16_t id);

// ---- colours (gclib BSP1_JPC100 / SSP1_JPC100 fields) ----
// base is the buffer the particle's offsets refer to (the raw file for a scan, the particle's bytes
// for a PcRandoJpcFile). Writes stay inside the block: in place, the size never changes.
//   BSP1 0x2C prm colour, 0x30 env colour (RGBA8); key tables of {u16 time, RGBA8} at the s16
//   chunk offsets 0x10 (prm) / 0x12 (env), counts u8 0x28 / 0x29.
//   SSP1 0x48 prm colour, 0x4C env colour.
struct PcRandoJpcRgba {
    uint8_t r, g, b, a;
};

enum PcRandoJpcColor {
    PCR_JPC_BSP1_PRM = 0,
    PCR_JPC_BSP1_ENV,
    PCR_JPC_SSP1_PRM,
    PCR_JPC_SSP1_ENV,
};

int pc_rando_jpc_get_color(const uint8_t* base, const PcRandoJpcParticle& p, int which, PcRandoJpcRgba* out);
int pc_rando_jpc_set_color(uint8_t* base, const PcRandoJpcParticle& p, int which, PcRandoJpcRgba color);
// BSP1 colour animation keys: which is PCR_JPC_BSP1_PRM or PCR_JPC_BSP1_ENV (0 keys for others).
uint32_t pc_rando_jpc_key_count(const uint8_t* base, const PcRandoJpcParticle& p, int which);
int pc_rando_jpc_get_key(const uint8_t* base, const PcRandoJpcParticle& p, int which, uint32_t index,
                         uint16_t* time, PcRandoJpcRgba* color);
int pc_rando_jpc_set_key_color(uint8_t* base, const PcRandoJpcParticle& p, int which, uint32_t index,
                               PcRandoJpcRgba color);

// ---- rebuilt files ----
struct PcRandoJpcFile {
    struct Particle {
        std::vector<uint8_t> bytes;            // gclib's saved layout of this particle
        PcRandoJpcParticle info;               // offsets in bytes
        std::vector<std::string> textureNames; // TDB1 ids as filenames (gclib texture_filenames)
    };
    struct Texture {
        std::vector<uint8_t> bytes; // gclib's saved layout of this TEX1 chunk
        std::string filename;
    };
    uint8_t header[0x20]; // the file's first 0x20 bytes (counts rewritten on save)
    std::vector<Particle> particles;
    std::vector<Texture> textures;
};

// Reads a raw JPC into f (replacing what it held). Result as pc_rando_jpc_scan.
int pc_rando_jpc_load(PcRandoJpcFile* f, const uint8_t* data, size_t size);

// Index of the particle with this id / the texture with this filename, or -1.
int pc_rando_jpc_find_particle(const PcRandoJpcFile& f, uint16_t id);
int pc_rando_jpc_find_texture(const PcRandoJpcFile& f, const std::string& filename);

// Appends a copy of src's particle id to dst, then a copy of every texture of that particle whose
// filename dst lacks, in the particle's TDB1 order (wwrando tweaks.py add_inter_dungeon_warp_pots).
// EXISTS (dst unchanged) when dst already has the id; wwrando skips those.
int pc_rando_jpc_copy_particle(PcRandoJpcFile* dst, const PcRandoJpcFile& src, uint16_t id);

// The byte size pc_rando_jpc_save writes.
size_t pc_rando_jpc_saved_size(const PcRandoJpcFile& f);
// gclib JPC.save(): header with the new counts, the particles with TDB1 ids recomputed from their
// filenames, 0x20 alignment, the textures, 0x20 alignment. NO_TEXTURE when a filename is missing.
int pc_rando_jpc_save_into(const PcRandoJpcFile& f, uint8_t* out, size_t cap, size_t* written);
int pc_rando_jpc_save(const PcRandoJpcFile& f, std::vector<uint8_t>* out);
