// pc_rando_player_colors.h - the player's custom colours, the part of wwrando's
// customizer.change_player_custom_colors that needs no wwrando asset (docs/RANDOMIZER_PLAN.md step
// R7.5b). Pure: no game headers (cos_rando_core links it into the unit tests).
//
// What wwrando does, and what the port keeps:
//   - Trail colours (sword slash, Elixir Soup slash, parrying slash, boomerang, arrow). wwrando takes
//     them from the player model's metadata.txt only (sword_slash_trail_color, ...), never from the
//     custom_colors option; the vanilla Link's link_metadata.txt has none, so for Link they never
//     change upstream. The port has no custom models, so it reads these five keys from custom_colors
//     itself (a port extension; wwrando ignores keys that are not colour names of the model). What
//     is written where is wwrando's:
//       sword slash / Elixir Soup / parrying: main.dol 0x803F62AC / B0 / B4 (daPy_swBlur_c::draw's
//         n_color0 / y_color0 / g_color0, RGBA) and the r, g, b of key frame 0 / 1 / 2 of the first
//         register colour animation of LkAnm.arc cutfh.brk and cutfm.brk (the forward jab's trail;
//         alpha is not changed there);
//       boomerang: main.dol 0x803F6268 (daBoomerang_blur_c::draw's color0, RGBA);
//       arrow: common.jpc particle 0x48, BSP1 prm colour. wwrando 1.10.0 assigns
//         `particle.bsp1.color_prm`, a field gclib's BSP1_JPC100 does not have (it is `prm_color`),
//         so its JPC.save() writes the vanilla colour back: upstream the arrow colour is a no-op. The
//         port sets the field wwrando meant.
//   - Body colours (custom_colors keyed by the model's colour names, base colours from
//     link_metadata.txt "hero_custom_colors" / "casual_custom_colors"). Every one of them needs a
//     colour mask PNG from wwrando's assets (link_color_masks/), which the port does not ship (plan
//     R7 "Decisions taken at the split": D12 stands), except the pupils: "hitomi" in Link.arc cl.bdl
//     recoloured for the colour named "Eyes" with texture_utils.color_exchange(..., ignore_bright=True)
//     and no mask, which is what customizer.py does for a model without a hitomi mask. (For the
//     vanilla Link wwrando has hitomi_hero_Eyes.png and uses it; the mask-free result differs only
//     where the mask is white, the bright sclera that ignore_bright leaves alone anyway.) Every other
//     colour is skipped and counted.
//   - wwrando's replaced_any follow-ups (the main body texture re-encoded as C8 and the hat UVs moved)
//     are not done: they only matter with a body colour, which needs masks.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_json.h"
#include "pc_rando_texture.h"

#include <string>

enum PcRandoTrail {
    PCR_TRAIL_SWORD_SLASH = 0, // sword_slash_trail_color, main.dol 0x803F62AC
    PCR_TRAIL_ELIXIR_SOUP,     // elixir_soup_sword_trail_color, 0x803F62B0
    PCR_TRAIL_PARRYING,        // parrying_sword_trail_color, 0x803F62B4
    PCR_TRAIL_BOOMERANG,       // boomerang_trail_color, 0x803F6268
    PCR_TRAIL_ARROW,           // arrow_trail_color, common.jpc 0x48 BSP1 prm colour
    PCR_TRAIL_COUNT
};

#define PCR_PLAYER_ARROW_PARTICLE 0x48
#define PCR_PLAYER_HITOMI "hitomi"
#define PCR_PLAYER_EYES "Eyes"

// The trail colours of a vanilla game (the DOL statics; the arrow's is common.jpc's, read from the
// file, so its entry here is the USA disc's value, for logs only).
extern const uint8_t kPcRandoTrailVanilla[PCR_TRAIL_COUNT][4];
// customizer.py's metadata key of a trail ("sword_slash_trail_color", ...).
const char* pc_rando_trail_key(int trail);

// link_metadata.txt: the colour names of "hero_custom_colors" (casual 0) or "casual_custom_colors"
// (casual 1), in file order, and their base colours.
int pc_rando_link_color_count(int casual);
const char* pc_rando_link_color_name(int casual, int index);
void pc_rando_link_color_base(int casual, int index, uint8_t rgb[3]);
// The index of a colour name, or -1.
int pc_rando_link_color_find(int casual, const char* name);

// The colours one activation applies.
struct PcRandoPlayerColors {
    uint8_t hasTrail[PCR_TRAIL_COUNT];
    uint8_t trailHasAlpha[PCR_TRAIL_COUNT]; // 0: a colour given without alpha keeps the vanilla alpha
    uint8_t trail[PCR_TRAIL_COUNT][4];
    int casual;          // player_in_casual_clothes: the "casual" colour set
    uint8_t hasEyes;     // the pupils are recoloured
    uint8_t eyesBase[3]; // link_metadata.txt's "Eyes"
    uint8_t eyes[3];     // the chosen colour
    int skippedMasked;   // model colours that differ from the base but need a mask (not applied)
    int unchanged;       // model colours equal to their base (wwrando skips them too)
    int unknown;         // keys that are neither a model colour nor a trail key (wwrando ignores them)
    int invalid;         // values that are not a colour (ignored)
};

void pc_rando_player_colors_clear(PcRandoPlayerColors* c);
// 1 when anything is applied (a trail colour or the pupils).
int pc_rando_player_colors_any(const PcRandoPlayerColors* c);

// Reads custom_colors (a JSON object; NULL or not an object: nothing chosen). Model colours: an array
// [r, g, b] (wwrando's format), or a string "RRGGBB" / "#RRGGBB" / "0xRRGGBB", or an integer
// 0xRRGGBB. Trail keys: [r, g, b, a] or [r, g, b], "RRGGBBAA" / "RRGGBB" (same prefixes), or an
// integer 0xRRGGBBAA (customizer.py parse_hex_color_with_alpha). Each problem is appended to *log
// (when not NULL) as one line. Returns 1 when anything is applied.
int pc_rando_player_colors_parse(const pcr::Json* customColors, int casual, PcRandoPlayerColors* out,
                                 std::string* log);
// The same from JSON text ("" = {}). Returns -1 when the text is not JSON.
int pc_rando_player_colors_parse_text(const char* text, int casual, PcRandoPlayerColors* out, std::string* log);

// The colour a trail is drawn with: the chosen one, else `vanilla` (RGBA, changed in place).
void pc_rando_player_trail(const PcRandoPlayerColors* c, int trail, uint8_t rgba[4]);

// ---- BRK (J3D TRK1 register colour animations), on the raw big-endian file, in place ----
// The number of register colour animations, -1 when the file has no readable TRK1.
int pc_rando_brk_reg_anim_count(const uint8_t* data, size_t size);
// The key frame count of a channel (0 r, 1 g, 2 b, 3 a) of register animation `anim`; -1 on error.
int pc_rando_brk_key_count(const uint8_t* data, size_t size, int anim, int channel);
// Key frame `key`'s value of that channel (s16, as gclib's AnimationKeyframe.value); 1 when read.
int pc_rando_brk_key_value(const uint8_t* data, size_t size, int anim, int channel, int key, int16_t* value);
// replace_sword_tip_brk_color_at_frame_index for one file: the r, g, b values of key `key` of
// register animation `anim`. Every channel must have exactly `keys` key frames (customizer.py
// asserts 3) and the value slot must belong to that track alone (no other register track of the
// channel reads it; gclib would split them on save, the in-place file cannot): else nothing is
// written. Returns 1 when written (even with the same values), 0 when refused, -1 when unreadable.
int pc_rando_brk_set_key_rgb(uint8_t* data, size_t size, int anim, int key, int keys, const uint8_t rgb[3]);
// The three sword trails into cutfh.brk / cutfm.brk (key frame i = trail i, animation 0, 3 keys).
// Returns the number of key frames written (0 with no sword trail chosen), -1 when refused.
int pc_rando_player_colors_brk(uint8_t* data, size_t size, const PcRandoPlayerColors* c);

// ---- common.jpc ----
// The arrow trail into particle 0x48's BSP1 prm colour (a colour given without alpha keeps the
// file's). Returns 1 when written, 0 when nothing is chosen, -1 when the particle or its BSP1 is
// missing (file unchanged).
int pc_rando_player_colors_jpc(uint8_t* data, size_t size, const PcRandoPlayerColors* c);

// ---- textures ----
// gclib BTI.render (mipmap 0) into rgba (width * height * 4).
int pc_rando_player_tex_render(const PcRandoTexture* tex, uint8_t* rgba);
// gclib BTI.replace_image(image) of a same-size image, in place: non-palette formats re-encode every
// mipmap; palette formats build the palette from the image (pc_rando_tex_encode_indexed) and need it
// to fit in the room the texture has (at most its current numColors entries; gclib relays the file
// out, the in-place copy cannot grow). On success tex->numColors is the new count (0 for non-palette
// formats); the caller writes it into the header. Nothing is changed on failure.
int pc_rando_player_tex_replace(PcRandoTexture* tex, const uint8_t* rgba);
// The pupils: render, color_exchange(eyesBase -> eyes, ignore_bright), replace_image. Returns
// PCR_TEX_OK, PCR_TEX_SKIPPED when the pupils are not chosen, or the failing codec result.
int pc_rando_player_colors_hitomi(PcRandoTexture* tex, const PcRandoPlayerColors* c);
// Opens the 0x20-byte BTI header at hdr (a ResTIMG in memory, offsets relative to it) whose data
// lies after it in one block.
int pc_rando_player_tex_open(PcRandoTexture* tex, uint8_t* hdr);
