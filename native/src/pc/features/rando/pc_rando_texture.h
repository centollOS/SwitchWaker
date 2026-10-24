// pc_rando_texture.h - GX texture codec of the randomizer (docs/RANDOMIZER_PLAN.md, step R7.2b).
// Pure: no game headers and no game calls, so the unit tests link it without the game. A port of
// gclib's texture_utils.py and bti.py (MIT, LagoLunatic/gclib at d9322523, the commit wwrando 1.10.0
// uses), kept byte-identical to them: wwrando's enemy palettes (R7.2c-R7.2e) and cosmetics (R7.5b)
// encode their textures with this code, and their results are compared with wwrando's own output.
//
// Formats: I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8 ("RGBA32" in gclib), C4, C8, C14X2 and CMPR; the
// palette formats IA8, RGB565 and RGB5A3. Every byte is big-endian, as on the disc. Images are RGBA
// bytes, row-major, 4 bytes per pixel. Blocks that bleed past the image edge are decoded and dropped,
// and encoded with gclib's filler values (0xF / 0xFF / 0xFFFF / 0x3FFF, CMPR index 0).
//
// What differs from gclib, on purpose:
//   - decoding a palette index that is past the palette gives (0,0,0,0) inside the image (gclib
//     raises there; it happens only on broken files);
//   - palette formats are encoded only from an image that already fits the palette
//     (pc_rando_tex_encode_indexed: gclib's generate_new_palettes_from_images + encode_mipmap_image).
//     gclib's encode_image first runs imagequant on C4/C8 images and a median cut on C14X2 images
//     with more than 16384 colours; neither is ported (wwrando's palette shift never re-encodes a
//     palette image, it only rewrites the palette).
//
// The colour shift itself (gclib hsv_shift_color) is R7.2a's (features/rando/pc_rando_color.h); the
// texture walk here takes it as a callback so the two steps stay independent.
#pragma once

#include <stddef.h>
#include <stdint.h>

// GX image formats (gclib gx_enums.ImageFormat; the BTI header's byte 0).
enum PcRandoTexFormat : uint8_t {
    PCR_TEX_I4 = 0x0,
    PCR_TEX_I8 = 0x1,
    PCR_TEX_IA4 = 0x2,
    PCR_TEX_IA8 = 0x3,
    PCR_TEX_RGB565 = 0x4,
    PCR_TEX_RGB5A3 = 0x5,
    PCR_TEX_RGBA8 = 0x6,
    PCR_TEX_C4 = 0x8,
    PCR_TEX_C8 = 0x9,
    PCR_TEX_C14X2 = 0xA,
    PCR_TEX_CMPR = 0xE,
};

// GX palette formats (gclib gx_enums.PaletteFormat; the BTI header's byte 9).
enum PcRandoPalFormat : uint8_t {
    PCR_PAL_IA8 = 0,
    PCR_PAL_RGB565 = 1,
    PCR_PAL_RGB5A3 = 2,
};

enum PcRandoTexResult {
    PCR_TEX_OK = 0,
    PCR_TEX_SKIPPED,         // greyscale format or palette: wwrando leaves it as it is
    PCR_TEX_BAD_FORMAT,      // unknown image or palette format
    PCR_TEX_BAD_SIZE,        // zero size, a buffer too small, or a header outside the file
    PCR_TEX_TOO_MANY_COLORS, // the image has more colours than the palette format holds
    PCR_TEX_UNSUPPORTED,     // a palette format where the operation needs re-quantizing
};

// A short name for logs ("ok", "skipped", ...).
const char* pc_rando_tex_result_name(int result);

// Format facts (gclib BLOCK_WIDTHS / BLOCK_HEIGHTS / BLOCK_DATA_SIZES); 0 for an unknown format.
int pc_rando_tex_known_format(int format);
int pc_rando_tex_block_width(int format);
int pc_rando_tex_block_height(int format);
int pc_rando_tex_block_size(int format);
int pc_rando_tex_uses_palette(int format);   // C4, C8, C14X2
int pc_rando_tex_max_colors(int format);     // 16, 256, 16384 for palette formats, else 0
int pc_rando_tex_is_grey_format(int format); // I4, I8, IA4, IA8 (gclib GREYSCALE_IMAGE_FORMATS)
int pc_rando_tex_is_grey_palette(int palFormat); // IA8 (gclib GREYSCALE_PALETTE_FORMATS)

// Bytes of one mipmap level of w x h (whole blocks), and the offset, size and dimensions of mipmap
// `mip` of a texture (gclib BTI.get_mipmap_offset_and_size: each level halves w and h, rounded
// down). pc_rando_tex_image_size is the size of mipmaps 0..mips-1 together.
size_t pc_rando_tex_level_size(int format, int w, int h);
size_t pc_rando_tex_mip(int format, int w, int h, int mip, int* mipW, int* mipH, size_t* mipSize);
size_t pc_rando_tex_image_size(int format, int w, int h, int mips);

// Colours.
void pc_rando_tex_decode_color(uint16_t raw, int palFormat, uint8_t rgba[4]);
uint16_t pc_rando_tex_encode_color(const uint8_t rgba[4], int palFormat);
// count colours of a palette (2 bytes each) to RGBA, and back.
void pc_rando_tex_decode_palette(const uint8_t* palette, int count, int palFormat, uint8_t* rgba);
void pc_rando_tex_encode_palette(const uint8_t* rgba, int count, int palFormat, uint8_t* palette);

// Decodes one level of w x h (gclib decode_image) into rgba (w*h*4 bytes). imageSize must hold the
// level; palette/numColors are used by the palette formats only.
int pc_rando_tex_decode(int format, const uint8_t* image, size_t imageSize, const uint8_t* palette,
                        int palFormat, int numColors, int w, int h, uint8_t* rgba);

// Encodes rgba (w*h*4) as one level of a non-palette format (gclib encode_mipmap_image). out must
// hold pc_rando_tex_level_size bytes. Palette formats give PCR_TEX_UNSUPPORTED.
int pc_rando_tex_encode(int format, const uint8_t* rgba, int w, int h, uint8_t* out, size_t outSize);

// gclib encode_image for a non-palette format with `mips` levels: level 0 is rgba and level i is
// rgba resized to (w>>i, h>>i) with Pillow's NEAREST filter, as gclib does. out must hold
// pc_rando_tex_image_size bytes.
int pc_rando_tex_encode_mips(int format, const uint8_t* rgba, int w, int h, int mips, uint8_t* out,
                             size_t outSize);

// Encodes an image that fits the palette as a palette format (gclib generate_new_palettes_from_images
// then encode_mipmap_image per level and encode_palette; see the note at the top on quantizing).
// The palette lists the encoded colours in the order they first appear (rows, then columns). out
// must hold pc_rando_tex_image_size bytes and palette pc_rando_tex_max_colors * 2; *numColors gets
// the palette's length.
int pc_rando_tex_encode_indexed(int format, int palFormat, const uint8_t* rgba, int w, int h, int mips,
                                uint8_t* out, size_t outSize, uint8_t* palette, int* numColors);

// The Pillow NEAREST resize gclib uses for mipmaps (Image.resize, ImagingScaleAffine): dst is
// dw x dh, picked from src (sw x sh). Both RGBA.
void pc_rando_tex_resize_nearest(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh);

// A texture in memory: a BTI header's fields and its image and palette bytes, which are changed in
// place. Does not own the bytes.
struct PcRandoTexture {
    uint8_t format;
    uint8_t palFormat;
    uint16_t width;
    uint16_t height;
    uint16_t numColors;
    uint8_t mipmapCount; // the header's 0 is read as 1, as gclib does
    uint8_t* image;      // every mipmap, pc_rando_tex_image_size bytes
    size_t imageSize;
    uint8_t* palette;    // numColors * 2 bytes (nullptr when numColors is 0)
};

// Opens the BTI header at data[headerOffset] (0x20 bytes, as in a .bti file or a J3D TEX1 entry);
// the image and palette offsets in it are relative to the header, as gclib BTI reads them.
int pc_rando_tex_from_bti(PcRandoTexture* tex, uint8_t* data, size_t size, size_t headerOffset);

// The colour transform of a recolour: changes rgba[0..2] (and may change rgba[3]).
typedef void (*PcRandoTexColorFn)(uint8_t rgba[4], void* ctx);

// wwrando palettes.py shift_all_colors_in_bti / shift_all_colors_in_tex1 (one texture), with the
// colour change as a callback:
//   - greyscale image formats, and palette formats with an IA8 palette: PCR_TEX_SKIPPED, unchanged;
//   - palette formats: only the palette changes (render_palette, the change, replace_palette);
//   - others: mipmap 0 is decoded, every pixel changed, and every mipmap re-encoded from it
//     (render, the change, replace_image), in place at the same size.
// gclib's replace_image also sets num_colors to 0 for non-palette formats and save_header_changes
// rewrites a few header fields; headers are the caller's business (tex->numColors is set to 0 here).
int pc_rando_tex_recolor(PcRandoTexture* tex, PcRandoTexColorFn fn, void* ctx);

// The shift form R7.2c uses: fn(rgba, hShift, vShift) is gclib hsv_shift_color (R7.2a).
typedef void (*PcRandoTexShiftFn)(uint8_t rgba[4], int hShift, int vShift);
int pc_rando_tex_shift(PcRandoTexture* tex, int hShift, int vShift, PcRandoTexShiftFn fn);

// gclib color_exchange without a mask (R7.5b): every pixel takes the replacement's hue and moves its
// saturation and value by the replacement's minus the base's (HSV in wwrando's int percent and
// degree truncations); ignoreBright leaves opaque pixels with r, g and b all above 128 alone.
// count pixels of rgba, changed in place.
void pc_rando_tex_color_exchange_rgba(uint8_t* rgba, size_t count, const uint8_t base[3],
                                      const uint8_t replacement[3], int ignoreBright);

// The same on a texture of a non-palette format (render, color_exchange, replace_image, as
// customizer.py does). Palette formats give PCR_TEX_UNSUPPORTED: gclib would re-quantize them.
int pc_rando_tex_color_exchange(PcRandoTexture* tex, const uint8_t base[3], const uint8_t replacement[3],
                                int ignoreBright);

// Python's colorsys.rgb_to_hsv / hsv_to_rgb in double (no fused multiply-adds, Python's float %),
// which color_exchange needs; R7.2a has its own for hsv_shift_color.
void pc_rando_tex_rgb_to_hsv(double r, double g, double b, double* h, double* s, double* v);
void pc_rando_tex_hsv_to_rgb(double h, double s, double v, double* r, double* g, double* b);
