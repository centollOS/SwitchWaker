// pc_rando_dzx.h - patch library for the stage files stage.dzs / room.dzr (docs/RANDOMIZER_PLAN.md,
// step R0.9). Pure: no game headers, no game calls, no allocation (the unit tests link it without the
// game, and R0.10 owns where grown copies live). Every read and write is in the disc's big-endian
// format, on the raw file as dRes_info_c::loadResource leaves it, i.e. before
// dStage_dt_c_offsetToPtr relocates the chunk offsets (game/src/d/d_stage.cpp).
//
// File format (the same as wwrando's wwlib/dzx.py):
//   u32 chunk count, then per chunk a 12-byte row {char tag[4], u32 entry count, u32 file offset of
//   the first entry}; each chunk's entries are fixed-size records (pc_rando_dzx_entry_size).
// The game decodes a tag from the first row that carries it (dStage_dt_c_decode), so a tag appears
// once; layer chunks carry the layer in the 4th character ("ACT3", "TRE0", "SCOb").
//
// Operations:
//   - in place: set_field (with an optional fingerprint, plan D9), suppress (wwrando's remove_entity:
//     the 8-byte name becomes empty, so dStage_searchName finds nothing and no actor is created);
//   - grow: pc_rando_dzx_grow builds a copy with entries appended at the END of their chunk, so the
//     vanilla indexes (wwrando's entries_by_type_and_layer(...)[i]) stay valid. The grown chunk is
//     rewritten at the end of the file and every other byte stays where it was; a tag that has no
//     chunk yet gets a new row, which shifts the data by a 0x20-aligned amount and fixes the file
//     offsets that move with it (the chunk rows and RTBL's room-table pointers, the only absolute
//     offsets in the format; PATH/RPAT point offsets are relative to PPNT/RPPN).
#pragma once

#include <stddef.h>
#include <stdint.h>

enum PcRandoDzxResult {
    PCR_DZX_OK = 0,
    PCR_DZX_BAD_FILE,     // header or chunk table out of the buffer
    PCR_DZX_NO_CHUNK,     // no chunk with this tag
    PCR_DZX_BAD_INDEX,    // entry index out of the chunk
    PCR_DZX_BAD_RANGE,    // field outside the entry
    PCR_DZX_MISMATCH,     // the fingerprint does not match (D9: leave the location vanilla)
    PCR_DZX_UNKNOWN_SIZE, // entry size unknown for this tag, or the chunk cannot grow (RTBL)
    PCR_DZX_NO_SPACE,     // the destination buffer is too small
};

// A short name for logs ("ok", "mismatch", ...).
const char* pc_rando_dzx_result_name(int result);

// The chunk tag of a base type on a layer: base "ACTR"/"TRES"/"SCOB" (or any tag) and layer -1
// (default) gives the base itself; layer 0..11 gives "ACT0".."ACTb" (wwrando's DZxLayer, lowercase
// hex). out gets 4 characters and a NUL. Returns 0 for a bad layer or a base that has no layers.
int pc_rando_dzx_tag(const char* base, int layer, char out[5]);

// The size of one entry of a chunk tag (layer chunks included), 0 when unknown.
uint32_t pc_rando_dzx_entry_size(const char* tag);

// A view over a raw dzs/dzr in memory. Does not own the bytes.
struct PcRandoDzx {
    uint8_t* data;
    size_t size;
    uint32_t chunkCount;
};

// Checks the header and the chunk table (rows and, for known entry sizes, the entries fit).
int pc_rando_dzx_open(PcRandoDzx* dzx, void* data, size_t size);

// The chunk row index of a tag, or -1.
int pc_rando_dzx_find(const PcRandoDzx* dzx, const char* tag);
// The entry count of a tag, 0 when the chunk is missing.
uint32_t pc_rando_dzx_count(const PcRandoDzx* dzx, const char* tag);
// The entry's bytes, or NULL (missing chunk, bad index, unknown size).
uint8_t* pc_rando_dzx_entry(const PcRandoDzx* dzx, const char* tag, uint32_t index);

// The identity check of a patched entry (plan D9). Each part is optional.
struct PcRandoDzxFingerprint {
    const char* name;     // compared with the zero-padded 8-byte name at entry+0; NULL skips it
    int32_t off;          // byte check at entry+off; < 0 skips it
    const uint8_t* bytes; // the expected vanilla bytes
    uint32_t len;
};

// Checks a fingerprint against an entry: OK, MISMATCH, or the lookup's error.
int pc_rando_dzx_check(const PcRandoDzx* dzx, const char* tag, uint32_t index,
                       const PcRandoDzxFingerprint* fp);

// Writes len bytes (already big-endian) at entry+off after checking fp (NULL: no check). Nothing is
// written unless the result is OK.
int pc_rando_dzx_set_field(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off,
                           const void* bytes, uint32_t len, const PcRandoDzxFingerprint* fp);
// Big-endian scalar helpers over set_field.
int pc_rando_dzx_set_u8(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, uint8_t v,
                        const PcRandoDzxFingerprint* fp);
int pc_rando_dzx_set_u16(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, uint16_t v,
                         const PcRandoDzxFingerprint* fp);
int pc_rando_dzx_set_u32(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, uint32_t v,
                         const PcRandoDzxFingerprint* fp);
int pc_rando_dzx_set_f32(PcRandoDzx* dzx, const char* tag, uint32_t index, uint32_t off, float v,
                         const PcRandoDzxFingerprint* fp);

// wwrando's remove_entity: clears the 8-byte name so no actor is created and the following indexes
// do not move. Only for actor-like chunks (ACTR/TRES/SCOB and their layers, TGOB, TGSC, DOOR, TGDR,
// PLYR).
int pc_rando_dzx_suppress(PcRandoDzx* dzx, const char* tag, uint32_t index,
                          const PcRandoDzxFingerprint* fp);

// Big-endian readers for patchers and tests.
uint16_t pc_rando_dzx_be16(const uint8_t* p);
uint32_t pc_rando_dzx_be32(const uint8_t* p);
float pc_rando_dzx_bef32(const uint8_t* p);

// One batch of entries to append to a tag's chunk (created when missing). entries holds
// count * pc_rando_dzx_entry_size(tag) disc-format bytes.
struct PcRandoDzxAdd {
    const char* tag;
    const void* entries;
    uint32_t count;
};

// The size of the grown copy of src for these adds (0 on an error, written to *result if not NULL).
size_t pc_rando_dzx_grown_size(const void* src, size_t srcSize, const PcRandoDzxAdd* adds, int nAdds,
                               int* result);
// Builds the grown copy into dst (cap >= pc_rando_dzx_grown_size; src and dst must not overlap).
// firstIndex[i] (optional) gets the index of adds[i]'s first new entry. Adds to the same tag are
// appended in order. The file is padded to 0x20 with 0xFF, as wwrando saves it.
int pc_rando_dzx_grow(const void* src, size_t srcSize, const PcRandoDzxAdd* adds, int nAdds, void* dst,
                      size_t cap, size_t* outSize, uint32_t* firstIndex);
