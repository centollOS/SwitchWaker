// The player's custom colours (pc_rando_player_colors.h, docs/RANDOMIZER_PLAN.md step R7.5b).
// Pure: no game headers, no game calls.
#include "pc_rando_player_colors.h"

#include "pc_rando_j3d.h"
#include "pc_rando_jpc.h"

#include <cstdio>
#include <cstring>
#include <vector>

const uint8_t kPcRandoTrailVanilla[PCR_TRAIL_COUNT][4] = {
    {0xFF, 0xFF, 0xFF, 0x96}, // n_color0$5158 (daPy_swBlur_c::draw)
    {0xFF, 0xFF, 0x7B, 0x96}, // y_color0$5159
    {0x5A, 0xFF, 0x5A, 0x96}, // g_color0$5160
    {0xFF, 0xFF, 0x7B, 0x96}, // color0$4442 (daBoomerang_blur_c::draw)
    {0xFF, 0xFF, 0xFF, 0x64}, // common.jpc 0x48 BSP1 prm colour on the USA disc
};

namespace {

const char* const kTrailKeys[PCR_TRAIL_COUNT] = {
    "sword_slash_trail_color", "elixir_soup_sword_trail_color", "parrying_sword_trail_color",
    "boomerang_trail_color",   "arrow_trail_color",
};

struct LinkColor {
    const char* name;
    uint32_t rgb;
};

// ref/wwrando/assets/link_metadata.txt (MIT), in file order.
const LinkColor kHero[] = {
    {"Hair", 0xFFEF10},       {"Skin", 0xF7DB9C},  {"Mouth", 0xF74963}, {"Eyes", 0x10514A},
    {"Sclera", 0xFFFFFF},     {"Hat", 0x5AB24A},   {"Tunic", 0x5AB24A}, {"Undershirt", 0xADE342},
    {"Pants", 0xFFFFFF},      {"Boots", 0x944908}, {"Belt", 0x633008},  {"Belt Buckle", 0xFFEF10},
};
const LinkColor kCasual[] = {
    {"Hair", 0xFFEF10},         {"Skin", 0xF7DB9C},     {"Mouth", 0xF74963}, {"Eyes", 0x10514A},
    {"Sclera", 0xFFFFFF},       {"Shirt", 0x4A75AD},    {"Shirt Emblem", 0xFFFFD6},
    {"Armbands", 0x63696B},     {"Pants", 0xFFA608},    {"Shoes", 0x63696B}, {"Shoe Soles", 0xD6BA39},
};

const LinkColor* table(int casual, int* n) {
    if (casual) {
        *n = (int)(sizeof kCasual / sizeof kCasual[0]);
        return kCasual;
    }
    *n = (int)(sizeof kHero / sizeof kHero[0]);
    return kHero;
}

void addLog(std::string* log, const std::string& line) {
    if (log != nullptr) {
        log->append(line);
        log->push_back('\n');
    }
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// "RRGGBB[AA]" with an optional "#" or "0x" prefix: *digits is 6 or 8.
bool parseHex(const std::string& s, uint32_t* value, int* digits) {
    size_t i = 0;
    if (s.size() >= 1 && s[0] == '#') {
        i = 1;
    } else if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    }
    const size_t n = s.size() - i;
    if (n != 6 && n != 8) {
        return false;
    }
    uint32_t v = 0;
    for (; i < s.size(); i++) {
        const int d = hexDigit(s[i]);
        if (d < 0) {
            return false;
        }
        v = (v << 4) | (uint32_t)d;
    }
    *value = v;
    *digits = (int)n;
    return true;
}

// A colour value: out = RGBA; *hasAlpha 1 when an alpha was given. allowAlpha: trail keys.
bool parseColor(const pcr::Json& v, bool allowAlpha, uint8_t out[4], int* hasAlpha) {
    *hasAlpha = 0;
    if (v.isArray()) {
        const size_t n = v.size();
        if (n != 3 && !(allowAlpha && n == 4)) {
            return false;
        }
        for (size_t i = 0; i < n; i++) {
            const pcr::Json& e = v.at(i);
            if (!e.isInteger()) {
                return false;
            }
            const int64_t x = e.asInt();
            if (x < 0 || x > 255) {
                return false;
            }
            out[i] = (uint8_t)x;
        }
        if (n == 4) {
            *hasAlpha = 1;
        } else {
            out[3] = 0;
        }
        return true;
    }
    if (v.isString()) {
        uint32_t x = 0;
        int digits = 0;
        if (!parseHex(v.text(), &x, &digits) || (digits == 8 && !allowAlpha)) {
            return false;
        }
        if (digits == 8) {
            out[0] = (uint8_t)(x >> 24);
            out[1] = (uint8_t)(x >> 16);
            out[2] = (uint8_t)(x >> 8);
            out[3] = (uint8_t)x;
            *hasAlpha = 1;
        } else {
            out[0] = (uint8_t)(x >> 16);
            out[1] = (uint8_t)(x >> 8);
            out[2] = (uint8_t)x;
            out[3] = 0;
        }
        return true;
    }
    if (v.isInteger()) {
        const int64_t x = v.asInt();
        // customizer.py: parse_hex_color 0x000000-0xFFFFFF, parse_hex_color_with_alpha up to 0xFFFFFFFF.
        if (x < 0 || x > (allowAlpha ? 0xFFFFFFFFll : 0xFFFFFFll)) {
            return false;
        }
        if (allowAlpha) {
            out[0] = (uint8_t)(x >> 24);
            out[1] = (uint8_t)(x >> 16);
            out[2] = (uint8_t)(x >> 8);
            out[3] = (uint8_t)x;
            *hasAlpha = 1;
        } else {
            out[0] = (uint8_t)(x >> 16);
            out[1] = (uint8_t)(x >> 8);
            out[2] = (uint8_t)x;
            out[3] = 0;
        }
        return true;
    }
    return false;
}

inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
inline uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
inline void wr16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

constexpr uint32_t kTrk1 = ('T' << 24) | ('R' << 16) | ('K' << 8) | '1';
constexpr size_t kAnimSize = 4 * 6 + 4; // gclib ColorAnimation.DATA_SIZE

// A J3D file's size as its header gives it, clipped to the buffer.
size_t j3dSize(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 0x20) {
        return 0;
    }
    const uint32_t s = rd32(data + 8);
    return s >= 0x20 && s < size ? s : size;
}

struct Trk1 {
    const uint8_t* c; // the chunk
    size_t size;      // chunk size
    uint16_t regCount;
    uint16_t valueCount[4];
    uint32_t animsOffset;
    uint32_t valuesOffset[4];
};

bool openTrk1(const uint8_t* data, size_t size, Trk1* t) {
    size = j3dSize(data, size);
    PcRandoJ3dChunk ch;
    if (size == 0 || !pc_rando_j3d_find(data, size, kTrk1, &ch) || ch.size < 0x58) {
        return false;
    }
    t->c = data + ch.offset;
    t->size = ch.size;
    t->regCount = rd16(t->c + 0x0C);
    for (int i = 0; i < 4; i++) {
        t->valueCount[i] = rd16(t->c + 0x10 + i * 2);
        t->valuesOffset[i] = rd32(t->c + 0x38 + i * 4);
        if (t->valuesOffset[i] > t->size || (t->size - t->valuesOffset[i]) / 2 < t->valueCount[i]) {
            return false;
        }
    }
    t->animsOffset = rd32(t->c + 0x20);
    if (t->regCount > 0 &&
        (t->animsOffset > t->size || (t->size - t->animsOffset) / kAnimSize < t->regCount)) {
        return false;
    }
    return true;
}

struct Track {
    int count;
    int index;
    int stride; // 3 (TangentType.IN), 4 (IN_OUT); 1 for a single key
};

bool readTrack(const Trk1& t, int anim, int channel, Track* tr) {
    if (anim < 0 || anim >= t.regCount || channel < 0 || channel > 3) {
        return false;
    }
    const uint8_t* p = t.c + t.animsOffset + (size_t)anim * kAnimSize + (size_t)channel * 6;
    tr->count = rd16(p);
    tr->index = rd16(p + 2);
    const uint16_t tangent = rd16(p + 4);
    if (tr->count == 1) {
        tr->stride = 1;
    } else if (tangent == 0) {
        tr->stride = 3;
    } else if (tangent == 1) {
        tr->stride = 4;
    } else {
        return false;
    }
    const long end = tr->count == 1 ? tr->index + 1 : tr->index + (long)tr->count * tr->stride;
    return tr->count >= 1 && end <= t.valueCount[channel];
}

// The values slot of key `key` (count 1: the value itself; else index + key * stride + 1).
int valueSlot(const Track& tr, int key) {
    return tr.count == 1 ? tr.index : tr.index + key * tr.stride + 1;
}

} // namespace

const char* pc_rando_trail_key(int trail) {
    return trail >= 0 && trail < PCR_TRAIL_COUNT ? kTrailKeys[trail] : "";
}

int pc_rando_link_color_count(int casual) {
    int n = 0;
    table(casual, &n);
    return n;
}

const char* pc_rando_link_color_name(int casual, int index) {
    int n = 0;
    const LinkColor* t = table(casual, &n);
    return index >= 0 && index < n ? t[index].name : "";
}

void pc_rando_link_color_base(int casual, int index, uint8_t rgb[3]) {
    int n = 0;
    const LinkColor* t = table(casual, &n);
    const uint32_t v = index >= 0 && index < n ? t[index].rgb : 0;
    rgb[0] = (uint8_t)(v >> 16);
    rgb[1] = (uint8_t)(v >> 8);
    rgb[2] = (uint8_t)v;
}

int pc_rando_link_color_find(int casual, const char* name) {
    int n = 0;
    const LinkColor* t = table(casual, &n);
    for (int i = 0; name != nullptr && i < n; i++) {
        if (strcmp(t[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void pc_rando_player_colors_clear(PcRandoPlayerColors* c) {
    memset(c, 0, sizeof *c);
}

int pc_rando_player_colors_any(const PcRandoPlayerColors* c) {
    if (c->hasEyes) {
        return 1;
    }
    for (int i = 0; i < PCR_TRAIL_COUNT; i++) {
        if (c->hasTrail[i]) {
            return 1;
        }
    }
    return 0;
}

int pc_rando_player_colors_parse(const pcr::Json* cc, int casual, PcRandoPlayerColors* out, std::string* log) {
    pc_rando_player_colors_clear(out);
    out->casual = casual ? 1 : 0;
    if (cc == nullptr || cc->isNull()) {
        return 0;
    }
    if (!cc->isObject()) {
        addLog(log, "custom_colors is not an object; ignored");
        out->invalid++;
        return 0;
    }
    // Keys that are neither a model colour nor a trail key.
    for (size_t i = 0; i < cc->size(); i++) {
        const std::string& k = cc->keyAt(i);
        bool known = pc_rando_link_color_find(out->casual, k.c_str()) >= 0;
        for (int t = 0; !known && t < PCR_TRAIL_COUNT; t++) {
            known = k == kTrailKeys[t];
        }
        if (!known) {
            out->unknown++;
            addLog(log, "custom_colors: \"" + k + "\" is not a colour of the " +
                            (out->casual ? "casual" : "hero") + " Link model; ignored");
        }
    }
    // The trails (port extension: customizer.py reads them from the model's metadata.txt).
    for (int t = 0; t < PCR_TRAIL_COUNT; t++) {
        const pcr::Json* v = cc->find(kTrailKeys[t]);
        if (v == nullptr) {
            continue;
        }
        uint8_t rgba[4];
        int hasAlpha = 0;
        if (!parseColor(*v, true, rgba, &hasAlpha)) {
            out->invalid++;
            addLog(log, std::string("custom_colors: ") + kTrailKeys[t] + " is not a colour; ignored");
            continue;
        }
        out->hasTrail[t] = 1;
        out->trailHasAlpha[t] = (uint8_t)hasAlpha;
        memcpy(out->trail[t], rgba, 4);
        if (!hasAlpha) {
            out->trail[t][3] = kPcRandoTrailVanilla[t][3];
        }
    }
    // The model's colours, in metadata order (customizer.py's loop).
    const int n = pc_rando_link_color_count(out->casual);
    for (int i = 0; i < n; i++) {
        const char* name = pc_rando_link_color_name(out->casual, i);
        const pcr::Json* v = cc->find(name);
        if (v == nullptr) {
            continue; // custom_color is None
        }
        uint8_t rgba[4];
        int hasAlpha = 0;
        if (!parseColor(*v, false, rgba, &hasAlpha)) {
            out->invalid++;
            addLog(log, std::string("custom_colors: \"") + name + "\" is not a colour; ignored");
            continue;
        }
        uint8_t base[3];
        pc_rando_link_color_base(out->casual, i, base);
        if (memcmp(rgba, base, 3) == 0) {
            out->unchanged++;
            continue; // custom_color == base_color
        }
        if (strcmp(name, PCR_PLAYER_EYES) == 0) {
            out->hasEyes = 1;
            memcpy(out->eyesBase, base, 3);
            memcpy(out->eyes, rgba, 3);
        } else {
            out->skippedMasked++;
            addLog(log, std::string("custom_colors: \"") + name +
                            "\" needs wwrando's colour masks, which are not shipped; not applied");
        }
    }
    return pc_rando_player_colors_any(out);
}

int pc_rando_player_colors_parse_text(const char* text, int casual, PcRandoPlayerColors* out, std::string* log) {
    pc_rando_player_colors_clear(out);
    out->casual = casual ? 1 : 0;
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    pcr::Json j;
    std::string err;
    if (!pcr::jsonParse(text, &j, &err)) {
        addLog(log, "custom_colors is not JSON (" + err + "); ignored");
        return -1;
    }
    return pc_rando_player_colors_parse(&j, casual, out, log);
}

void pc_rando_player_trail(const PcRandoPlayerColors* c, int trail, uint8_t rgba[4]) {
    if (c == nullptr || trail < 0 || trail >= PCR_TRAIL_COUNT || !c->hasTrail[trail]) {
        return;
    }
    memcpy(rgba, c->trail[trail], 3);
    if (c->trailHasAlpha[trail]) {
        rgba[3] = c->trail[trail][3];
    }
}

// ---- BRK ----

int pc_rando_brk_reg_anim_count(const uint8_t* data, size_t size) {
    Trk1 t;
    return openTrk1(data, size, &t) ? t.regCount : -1;
}

int pc_rando_brk_key_count(const uint8_t* data, size_t size, int anim, int channel) {
    Trk1 t;
    Track tr;
    if (!openTrk1(data, size, &t) || !readTrack(t, anim, channel, &tr)) {
        return -1;
    }
    return tr.count;
}

int pc_rando_brk_key_value(const uint8_t* data, size_t size, int anim, int channel, int key, int16_t* value) {
    Trk1 t;
    Track tr;
    if (!openTrk1(data, size, &t) || !readTrack(t, anim, channel, &tr) || key < 0 || key >= tr.count) {
        return 0;
    }
    *value = (int16_t)rd16(t.c + t.valuesOffset[channel] + (size_t)valueSlot(tr, key) * 2);
    return 1;
}

int pc_rando_brk_set_key_rgb(uint8_t* data, size_t size, int anim, int key, int keys, const uint8_t rgb[3]) {
    Trk1 t;
    if (!openTrk1(data, size, &t)) {
        return -1;
    }
    int slots[3];
    for (int ch = 0; ch < 3; ch++) {
        Track tr;
        if (!readTrack(t, anim, ch, &tr) || tr.count != keys || key < 0 || key >= keys) {
            return 0;
        }
        slots[ch] = valueSlot(tr, key);
        // No other register track of the channel may read the slot.
        for (int other = 0; other < t.regCount; other++) {
            if (other == anim) {
                continue;
            }
            Track o;
            if (!readTrack(t, other, ch, &o)) {
                return 0;
            }
            const int first = o.index;
            const int last = o.count == 1 ? o.index : o.index + o.count * o.stride - 1;
            if (slots[ch] >= first && slots[ch] <= last) {
                return 0;
            }
        }
    }
    uint8_t* c = data + (t.c - data);
    for (int ch = 0; ch < 3; ch++) {
        wr16(c + t.valuesOffset[ch] + (size_t)slots[ch] * 2, (uint16_t)(int16_t)rgb[ch]);
    }
    return 1;
}

int pc_rando_player_colors_brk(uint8_t* data, size_t size, const PcRandoPlayerColors* c) {
    if (c == nullptr) {
        return 0;
    }
    // customizer.py: only when one of the sword trails is chosen; key frame i = trail i.
    int written = 0;
    for (int trail = PCR_TRAIL_SWORD_SLASH; trail <= PCR_TRAIL_PARRYING; trail++) {
        if (!c->hasTrail[trail]) {
            continue;
        }
        const int r = pc_rando_brk_set_key_rgb(data, size, 0, trail, 3, c->trail[trail]);
        if (r <= 0) {
            return -1;
        }
        written++;
    }
    return written;
}

// ---- common.jpc ----

int pc_rando_player_colors_jpc(uint8_t* data, size_t size, const PcRandoPlayerColors* c) {
    if (c == nullptr || !c->hasTrail[PCR_TRAIL_ARROW]) {
        return 0;
    }
    std::vector<PcRandoJpcParticle> particles;
    if (pc_rando_jpc_scan(data, size, &particles, nullptr) != PCR_JPC_OK) {
        return -1;
    }
    for (const PcRandoJpcParticle& p : particles) {
        if (p.id != PCR_PLAYER_ARROW_PARTICLE) {
            continue;
        }
        PcRandoJpcRgba col;
        if (p.bsp1 < 0 || pc_rando_jpc_get_color(data, p, PCR_JPC_BSP1_PRM, &col) != PCR_JPC_OK) {
            return -1;
        }
        col.r = c->trail[PCR_TRAIL_ARROW][0];
        col.g = c->trail[PCR_TRAIL_ARROW][1];
        col.b = c->trail[PCR_TRAIL_ARROW][2];
        if (c->trailHasAlpha[PCR_TRAIL_ARROW]) {
            col.a = c->trail[PCR_TRAIL_ARROW][3];
        }
        return pc_rando_jpc_set_color(data, p, PCR_JPC_BSP1_PRM, col) == PCR_JPC_OK ? 1 : -1;
    }
    return -1;
}

// ---- textures ----

int pc_rando_player_tex_render(const PcRandoTexture* tex, uint8_t* rgba) {
    return pc_rando_tex_decode(tex->format, tex->image, tex->imageSize, tex->palette, tex->palFormat,
                               tex->numColors, tex->width, tex->height, rgba);
}

int pc_rando_player_tex_replace(PcRandoTexture* tex, const uint8_t* rgba) {
    const int mips = tex->mipmapCount < 1 ? 1 : tex->mipmapCount;
    if (!pc_rando_tex_uses_palette(tex->format)) {
        std::vector<uint8_t> out(tex->imageSize);
        const int r = pc_rando_tex_encode_mips(tex->format, rgba, tex->width, tex->height, mips, out.data(),
                                               out.size());
        if (r != PCR_TEX_OK) {
            return r;
        }
        memcpy(tex->image, out.data(), out.size());
        tex->numColors = 0;
        return PCR_TEX_OK;
    }
    std::vector<uint8_t> out(tex->imageSize);
    std::vector<uint8_t> pal((size_t)pc_rando_tex_max_colors(tex->format) * 2);
    int n = 0;
    const int r = pc_rando_tex_encode_indexed(tex->format, tex->palFormat, rgba, tex->width, tex->height, mips,
                                              out.data(), out.size(), pal.data(), &n);
    if (r != PCR_TEX_OK) {
        return r;
    }
    if (n > tex->numColors || tex->palette == nullptr) {
        return PCR_TEX_TOO_MANY_COLORS; // no room for a longer palette in place
    }
    memcpy(tex->image, out.data(), out.size());
    memcpy(tex->palette, pal.data(), (size_t)n * 2);
    tex->numColors = (uint16_t)n;
    return PCR_TEX_OK;
}

int pc_rando_player_colors_hitomi(PcRandoTexture* tex, const PcRandoPlayerColors* c) {
    if (c == nullptr || !c->hasEyes) {
        return PCR_TEX_SKIPPED;
    }
    std::vector<uint8_t> rgba((size_t)tex->width * tex->height * 4);
    int r = pc_rando_player_tex_render(tex, rgba.data());
    if (r != PCR_TEX_OK) {
        return r;
    }
    pc_rando_tex_color_exchange_rgba(rgba.data(), (size_t)tex->width * tex->height, c->eyesBase, c->eyes, 1);
    return pc_rando_player_tex_replace(tex, rgba.data());
}

int pc_rando_player_tex_open(PcRandoTexture* tex, uint8_t* hdr) {
    if (hdr == nullptr) {
        return PCR_TEX_BAD_SIZE;
    }
    const uint32_t imageOffset = rd32(hdr + 0x1C);
    const uint32_t paletteOffset = rd32(hdr + 0x0C);
    const uint16_t numColors = rd16(hdr + 0x0A);
    const int format = hdr[0];
    if (!pc_rando_tex_known_format(format)) {
        return PCR_TEX_BAD_FORMAT;
    }
    const int mips = hdr[0x18] == 0 ? 1 : hdr[0x18];
    const size_t imageEnd = (size_t)imageOffset + pc_rando_tex_image_size(format, rd16(hdr + 2), rd16(hdr + 4), mips);
    const size_t paletteEnd = (size_t)paletteOffset + (size_t)numColors * 2;
    if (imageOffset < 0x20 || imageOffset >= 0x80000000u || (numColors > 0 && paletteOffset >= 0x80000000u)) {
        return PCR_TEX_BAD_SIZE;
    }
    size_t end = imageEnd;
    if (numColors > 0 && paletteEnd > end) {
        end = paletteEnd;
    }
    return pc_rando_tex_from_bti(tex, hdr, end, 0);
}
