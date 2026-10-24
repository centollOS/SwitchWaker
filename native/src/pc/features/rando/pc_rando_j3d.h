// pc_rando_j3d.h - J3D and BTI files for the randomizer's recolours (docs/RANDOMIZER_PLAN.md step R7.2c;
// R7.2d adds material and animation colours here).
//
// Works on the raw big-endian bytes of a .bdl/.bmd/.bmt/.brk (J3D) or .bti file, in place: the port
// recolours them right before the game's loaders read them (the raw J3D hook,
// pc_rando_resources.h), where wwrando re-saves the file with gclib. gclib's TEX1 save relays the
// texture data out (one copy per name and format); the port keeps the disc's layout and writes the
// same image and palette bytes into it, so every texture header reads the bytes gclib's file has.
//
// J3D: magic "J3D?" at 0, the chunk count at 0x0C, chunks from 0x20 ({magic, u32 size, ...}), read
// as gclib J3D.read does (it stops early at the end of the file: Bk.arc bk_boko.bmt claims more chunks
// than it has). TEX1: u16 count at 0x08, the 0x20-byte BTI headers at the u32 0x0C, the name table
// (JUTNameTab: u16 count, u16 pad, {u16 hash, u16 offset}) at the u32 0x10.
//
// Pure: no game headers (cos_rando_core links it into the unit tests).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_texture.h"

// One chunk of a J3D file.
struct PcRandoJ3dChunk {
    uint32_t magic; // e.g. 'TEX1' as a big-endian four-character code
    size_t offset;  // from the file start
    size_t size;    // the chunk's size field, clipped to the file
};

// 1 when data starts with "J3D".
int pc_rando_j3d_is_j3d(const uint8_t* data, size_t size);
// The chunks of a J3D file in file order, at most `max`; returns how many there are (0: not J3D).
int pc_rando_j3d_chunks(const uint8_t* data, size_t size, PcRandoJ3dChunk* out, int max);
// The last chunk with `magic` (gclib keeps the last one per type), or 0 with *chunk untouched.
int pc_rando_j3d_find(const uint8_t* data, size_t size, uint32_t magic, PcRandoJ3dChunk* chunk);

// Recolour counters of one file (or summed over many).
struct PcRandoRecolorStats {
    int textures;  // texture headers seen (TEX1 entries, or 1 for a BTI)
    int shifted;   // texture headers whose image or palette was rewritten
    int skipped;   // headers left alone: "toon" names, greyscale formats or IA8 palettes
    int errors;    // headers that could not be read or shifted (bad offsets, sizes, formats)
    int mismatch;  // a same-name header with another format or size than the name's first (skipped)
    int conflicts; // headers whose bytes differ from gclib's after the edit (data shared across names
                   // with different results; the in-place layout cannot hold both)
};

// The hsv shift wwrando applies (gclib hsv_shift_color, R7.2a); alpha passes through.
void pc_rando_j3d_shift_rgba(uint8_t rgba[4], int hShift, int vShift);

// palettes.py shift_all_colors_in_tex1 on the file's TEX1 chunk (none: nothing to do, returns 0):
// textures by name in first-seen order; names containing "toon" skipped; the first texture of a name
// is shifted (pc_rando_tex_shift) from the disc's bytes and its result written to every texture of the
// name (palette formats: the palette only; others: the image, and num_colors set to 0 as gclib's
// replace_image does). Returns the number of headers rewritten (<0 when the chunk is unreadable).
int pc_rando_j3d_shift_tex1(uint8_t* data, size_t size, int hShift, int vShift, PcRandoRecolorStats* st);

// palettes.py shift_all_colors_in_bti on a .bti file (the header at 0). Returns 1 when shifted.
int pc_rando_bti_shift(uint8_t* data, size_t size, int hShift, int vShift, PcRandoRecolorStats* st);

// The file kinds wwrando recolours (palettes.py: .bdl .bti .bmd .bmt .brk, by extension, exact case).
enum PcRandoRecolorKind { PCR_RECOLOR_NONE = 0, PCR_RECOLOR_J3D, PCR_RECOLOR_BTI };
int pc_rando_recolor_kind(const char* fileName);

// Every texture of a J3D file's TEX1 (index, name) or of a BTI file (index 0, name ""), opened over
// `data` (tex->image / tex->palette point into it). Returns the count visited (<0: unreadable).
typedef void (*PcRandoTexVisitor)(int index, const char* name, const PcRandoTexture* tex, int openResult,
                                  void* ctx);
int pc_rando_j3d_visit_textures(uint8_t* data, size_t size, int isBti, PcRandoTexVisitor fn, void* ctx);

// tweaks.py make_rat_holes_visible_from_behind on Nzg.arc kana_00.bdl: the MAT3 cull mode (u32 at
// 0xC80) to 0 and the MDL3 GEN_MODE byte at 0xFCE to 0x04. Returns 1 when patched, 0 when it already
// was, -1 when the file is not the disc's (size, or other values there: left alone).
#define PCR_RAT_HOLE_ARC "Nzg"
#define PCR_RAT_HOLE_FILE "kana_00.bdl"
int pc_rando_rat_hole_patch(uint8_t* data, size_t size);
