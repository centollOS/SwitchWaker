// pc_rando_j3d.h - J3D and BTI files for the randomizer's recolours (docs/RANDOMIZER_PLAN.md steps R7.2c:
// textures; R7.2d: material (MAT3, MDL3) and animation (TRK1) colours).
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
    // R7.2d (errors, mismatch and conflicts above count these chunks too)
    int matColors;  // MAT3 tev / konst colour list entries rewritten
    int mdlEntries; // MDL3 entries whose TEV colour register commands were rewritten from MAT3
    int trkAnims;   // TRK1 colour animations recoloured
    int trkRebuilt; // TRK1 chunks rewritten in gclib's layout
    int trkInPlace; // TRK1 chunks edited in place (gclib's layout did not fit the file)
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

// palettes.py shift_all_colors_in_mat3 on the file's MAT3 chunk (the last one; none: returns 0): every
// tev colour (RGBA s16; skipped when r, g or b is negative) and tev konst colour (RGBA u8) a material
// uses gets hsv_shift_color on its r, g, b (alpha kept). gclib gives each material its own copy of the
// values and saves the distinct ones; in place, each list entry that some material uses is shifted
// once from the disc's value, which is the same set of colours per material. Unset colours (index
// 0xFFFF) are skipped (wwrando would fail on them; none exist in the group files). Returns the number
// of list entries rewritten (<0: unreadable).
int pc_rando_j3d_shift_mat3(uint8_t* data, size_t size, int hShift, int vShift, PcRandoRecolorStats* st);

// What gclib's J3D.save does to MDL3 (palettes.py's own shift_all_colors_in_mdl3 is overwritten by it):
// MDL3.generate_from_mat3 rebuilds every entry's display list from its material, so the TEV colour
// register commands carry the MAT3 colours: tev colours 0..2 as TEV_REGISTERL/H 1..3 (r, a and g, b
// masked to 11 bits, the H command repeated twice more: the "duplicated high command"), konst colours
// 0..3 as TEV_REGISTERL/H 0..3 with the konst bit. In place, the disc's commands (same registers in
// the same order, else the entry is a mismatch and left alone) get those payloads. Run after
// pc_rando_j3d_shift_mat3. Returns the number of entries rewritten (<0: unreadable).
int pc_rando_j3d_sync_mdl3(uint8_t* data, size_t size, PcRandoRecolorStats* st);

// palettes.py shift_all_colors_in_trk1 on the file's TRK1 chunk, then gclib's TRK1 save: animations
// grouped by material name (register ones, then konst ones), "cc.brk" animation 1 skipped; for each
// animation, every key time of R, G or B gets the closest key's values (& 0xFF) shifted and written to
// the keys at exactly that time. The chunk is then written in gclib's layout (gclib deduplicates track
// data, so the result is never larger than the disc's on the group files), and when that does not fit
// the values are written in place (conflicts counted when shared data cannot hold them). `fileName`
// is the file's name in its archive. Returns the number of animations recoloured (<0: unreadable).
int pc_rando_j3d_shift_trk1(uint8_t* data, size_t size, const char* fileName, int hShift, int vShift,
                            PcRandoRecolorStats* st);

// The J3D branch of palettes.py shift_colors_for_file_group: TEX1, MAT3, MDL3 and TRK1 in that order.
// Returns 1 when something changed.
int pc_rando_j3d_shift_file(uint8_t* data, size_t size, const char* fileName, int hShift, int vShift,
                            PcRandoRecolorStats* st);

// One text line per material, MDL3 entry and TRK1 animation of a J3D file (the palettes smoke dumps
// them; native/tools/rando/palette_check.py builds the same lines with gclib):
//   mat <i> tev <c0> <c1> <c2> <c3> konst <k0> <k1> <k2> <k3>      (each r,g,b,a or -)
//   mdl <i> [<register hex2>:<payload hex6> ...]                     (TEV colour register commands)
//   trk <reg|konst> <i> <material> <color id> r <tangent> <keys> g ... b ... a ...
//                                                                    (keys time:value:in:out,...)
// Returns the number of lines (<0: a chunk is unreadable; its lines are left out).
typedef void (*PcRandoLineFn)(const char* line, void* ctx);
int pc_rando_j3d_color_lines(const uint8_t* data, size_t size, PcRandoLineFn fn, void* ctx);

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
