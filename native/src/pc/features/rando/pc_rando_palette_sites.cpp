// The enemy palettes outside the archives (pc_rando_palette_sites.h, pc/pc_rando_palette_code.h;
// docs/RANDOMIZER_PLAN.md step R7.2e). Pure: no game headers.
#include "pc_rando_palette_sites.h"

#include "pc/pc_rando.h"
#include "pc_rando_color.h"
#include "pc_rando_gen_palettes.h"
#include "pc_rando_jpc.h"
#include "pc_rando_jpc_scene.h"
#include "pc_rando_palettes.h"
#include "pc_rando_texture.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

// ---- the code sites: the decompiled values (game/src/d/actor), which are the REL bytes wwrando
// shifts (palette_check.py compares them with the disc) ----
const uint8_t kTn[12][3] = {
    // yoroi_break prim[]
    {0x9F, 0xC1, 0x95}, {0xBC, 0xCF, 0xC8}, {0xBC, 0x98, 0x42}, {0xF0, 0xF5, 0xEB}, {0x0C, 0x14, 0x0A},
    {0x50, 0x00, 0x14},
    // yoroi_break env[]
    {0x3C, 0x40, 0x50}, {0x60, 0x24, 0x18}, {0x1E, 0x15, 0x36}, {0x9E, 0x94, 0x87}, {0xA8, 0x7D, 0x6C},
    {0xEB, 0xD8, 0x82},
};
const uint8_t kThread[1][3] = {{0xFF, 0x64, 0x00}}; // mo2, boko, st, gnd
const uint8_t kRatTail[1][3] = {{0xC8, 0xC8, 0xC8}}; // nz, npc_nz
const uint8_t kCc[5][3] = {
    {0x8D, 0xDE, 0x43}, {0xEC, 0x4A, 0x22}, {0x5B, 0xAD, 0xC8}, {0x28, 0x10, 0x27}, {0xFF, 0xC3, 0x3C},
};
const uint8_t kCcSplit[1][3] = {{40, 16, 39}};
const uint8_t kBgn[2][3] = {{0xD2, 0x32, 0x5A}, {0xD2, 0x32, 0x5A}};
const uint8_t kBgn3[1][3] = {{0xBC, 0x98, 0x42}};

const PcRandoPaletteSiteInfo kSites[PCR_PALSITE_COUNT] = {
    {"tn", "Darknut", 12, kTn},
    {"mo2", "Moblin", 1, kThread},
    {"boko", "Moblin", 1, kThread},
    {"st", "Stalfos", 1, kThread},
    {"nz", "Rat", 1, kRatTail},
    {"npc_nz", "Rat", 1, kRatTail},
    {"cc", "ChuChu", 5, kCc},
    {"cc_split", "ChuChu", 1, kCcSplit},
    {"bgn", "Puppet Ganon", 2, kBgn},
    {"bgn3", "Puppet Ganon", 1, kBgn3},
    {"gnd", "Ganondorf", 1, kThread},
};
constexpr const char* kMantGroup = "Darknut";

PcRandoPaletteCodeStats sStats;

// The palette group of a site (PCR_PALSITE_COUNT: d_a_mant), -1 when the table lacks it.
int siteGroup(int site) {
    static int cache[PCR_PALSITE_COUNT + 1];
    static bool init = false;
    if (!init) {
        init = true;
        for (int i = 0; i < PCR_PALSITE_COUNT; i++) {
            cache[i] = pcr::paletteGroupIndex(kSites[i].group);
        }
        cache[PCR_PALSITE_COUNT] = pcr::paletteGroupIndex(kMantGroup);
    }
    return site >= 0 && site <= PCR_PALSITE_COUNT ? cache[site] : -1;
}

// Shifted values cached per site entry, recomputed when the group's shift changes.
struct CodeCache {
    bool valid;
    int h, v;
    uint8_t rgb[3];
};
CodeCache sCode[PCR_PALSITE_COUNT][12];
bool sMismatchLogged[PCR_PALSITE_COUNT][12];

// d_a_mant
uint8_t* const* sMant = nullptr;
int sMantCount = 0;
struct MantCache {
    bool valid;
    int h, v;
    const uint8_t* orig;
    uint8_t src[0x20];
    alignas(32) uint8_t pal[0x20];
};
MantCache sMantCache[PCR_PALSITE_MANT_SLOTS];

// ---- particles ----
void shiftRgba(PcRandoJpcRgba* c, int h) {
    pcr::hsvShiftColor(&c->r, &c->g, &c->b, h, 0); // shift_all_colors_in_particle: v_shift = 0
}

// shift_all_colors_in_particle on one particle in place.
void shiftParticle(uint8_t* base, const PcRandoJpcParticle& p, int h) {
    for (int which : {PCR_JPC_BSP1_PRM, PCR_JPC_BSP1_ENV}) {
        PcRandoJpcRgba c;
        if (pc_rando_jpc_get_color(base, p, which, &c) == PCR_JPC_OK) {
            shiftRgba(&c, h);
            pc_rando_jpc_set_color(base, p, which, c);
            sStats.particleColors++;
        }
    }
    // The key tables: gclib reads both lists before it writes either (and saves them apart), so read
    // every key first; a table that overlaps the other must still hold its own shift afterwards.
    std::vector<PcRandoJpcRgba> keys[2];
    const int kinds[2] = {PCR_JPC_BSP1_PRM, PCR_JPC_BSP1_ENV};
    for (int k = 0; k < 2; k++) {
        const uint32_t n = pc_rando_jpc_key_count(base, p, kinds[k]);
        keys[k].resize(n);
        for (uint32_t i = 0; i < n; i++) {
            pc_rando_jpc_get_key(base, p, kinds[k], i, nullptr, &keys[k][i]);
            shiftRgba(&keys[k][i], h);
        }
    }
    for (int k = 0; k < 2; k++) {
        for (uint32_t i = 0; i < keys[k].size(); i++) {
            pc_rando_jpc_set_key_color(base, p, kinds[k], i, keys[k][i]);
            sStats.particleColors++;
        }
    }
    for (int k = 0; k < 2; k++) {
        for (uint32_t i = 0; i < keys[k].size(); i++) {
            PcRandoJpcRgba c;
            pc_rando_jpc_get_key(base, p, kinds[k], i, nullptr, &c);
            if (memcmp(&c, &keys[k][i], sizeof c) != 0) {
                sStats.particleConflicts++;
            }
        }
    }
    if (p.ssp1 >= 0) {
        for (int which : {PCR_JPC_SSP1_PRM, PCR_JPC_SSP1_ENV}) {
            PcRandoJpcRgba c;
            if (pc_rando_jpc_get_color(base, p, which, &c) == PCR_JPC_OK) {
                shiftRgba(&c, h);
                pc_rando_jpc_set_color(base, p, which, c);
                sStats.particleColors++;
            }
        }
    }
}

bool sEditorRegistered = false;

void onRando(int active) {
    if (active != 1) {
        sEditorRegistered = false; // R7.1b clears its registry itself when pc_rando() turns 0
        return;
    }
    // Registered on every activation: the editor does nothing for a seed without palettes.
    sEditorRegistered =
        pc_rando_jpc_scene_add_editor(PCR_JPC_SCENE_ALL, pc_rando_palette_particles_edit, nullptr) != 0;
    if (!sEditorRegistered) {
        logf("[cos] rando: palettes: the particle editor could not be registered (R7.1b registry full); "
             "particle colours stay vanilla\n");
    }
}

} // namespace

extern "C" {

void pc_rando_palette_code_init(void) {
    static bool init = false;
    if (init) {
        return;
    }
    init = true;
    if (!pc_rando_add_listener(onRando)) { // called with 1 at once when already active
        logf("[cos] rando: palettes: listener list full, particle colours stay vanilla\n");
    }
}

void pc_rando_palette_particles_edit(int scene, uint8_t* data, size_t size, void* user) {
    (void)user;
    if (scene < 0 || scene > 254 || data == nullptr) {
        return; // common.jpc: palettes.py loops over files/res/Particle/Pscene%03d.jpc only
    }
    const int n = pcr::paletteGroupCount();
    bool any = false;
    int shifts[64][2];
    for (int g = 0; g < n && g < 64; g++) {
        shifts[g][0] = -1;
        int h = 0, v = 0;
        if (pcr::paletteGroup(g).numParticleIds > 0 && pc_rando_palette_shift(g, &h, &v)) {
            shifts[g][0] = h;
            shifts[g][1] = v;
            any = true;
        }
    }
    if (!any) {
        return;
    }
    std::vector<PcRandoJpcParticle> ps;
    const int r = pc_rando_jpc_scan(data, size, &ps, nullptr);
    if (r != PCR_JPC_OK) {
        sStats.particleBad++;
        logf("[cos] rando: palettes: Pscene%03d.jpc: %s, its particle colours stay vanilla\n", scene,
             pc_rando_jpc_result_name(r));
        return;
    }
    sStats.particleFiles++;
    // shift_colors_for_file_group: per group in file order, the file's particles in its order whose id
    // is in the group's list.
    for (int g = 0; g < n && g < 64; g++) {
        if (shifts[g][0] < 0) {
            continue;
        }
        const pcr::PaletteGroup& grp = pcr::paletteGroup(g);
        for (const PcRandoJpcParticle& p : ps) {
            for (int i = 0; i < grp.numParticleIds; i++) {
                if (grp.particleIds[i] == p.id) {
                    shiftParticle(data, p, shifts[g][0]);
                    sStats.particleShifted++;
                    break;
                }
            }
        }
    }
}

const PcRandoPaletteSiteInfo* pc_rando_palette_site(int site) {
    return site >= 0 && site < PCR_PALSITE_COUNT ? &kSites[site] : nullptr;
}

int pc_rando_palette_code_rgb(int site, int index, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!pc_rando() || site < 0 || site >= PCR_PALSITE_COUNT || index < 0 || index >= kSites[site].count ||
        r == nullptr || g == nullptr || b == nullptr) {
        return 0;
    }
    int h = 0, v = 0;
    if (!pc_rando_palette_shift(siteGroup(site), &h, &v)) {
        return 0;
    }
    const uint8_t* want = kSites[site].rgb[index];
    if (*r != want[0] || *g != want[1] || *b != want[2]) {
        // Not the decompiled value (another change got there first): shift what the game has.
        sStats.codeMismatch++;
        if (!sMismatchLogged[site][index]) {
            sMismatchLogged[site][index] = true;
            logf("[cos] rando: palettes: code colour %s[%d] is %02X%02X%02X, not %02X%02X%02X; shifted as it is\n",
                 kSites[site].name, index, *r, *g, *b, want[0], want[1], want[2]);
        }
        pcr::hsvShiftColor(r, g, b, h, v);
        sStats.codeShifted++;
        return 1;
    }
    CodeCache& c = sCode[site][index];
    if (!c.valid || c.h != h || c.v != v) {
        c.rgb[0] = want[0];
        c.rgb[1] = want[1];
        c.rgb[2] = want[2];
        pcr::hsvShiftColor(&c.rgb[0], &c.rgb[1], &c.rgb[2], h, v);
        c.h = h;
        c.v = v;
        c.valid = true;
    }
    *r = c.rgb[0];
    *g = c.rgb[1];
    *b = c.rgb[2];
    sStats.codeShifted++;
    return 1;
}

int pc_rando_palette_mant_register(uint8_t* const* palettes, int count) {
    sMant = palettes;
    sMantCount = count < PCR_PALSITE_MANT_SLOTS ? count : PCR_PALSITE_MANT_SLOTS;
    return 1;
}

uint8_t* pc_rando_palette_mant_orig(int slot) {
    return sMant != nullptr && slot >= 0 && slot < sMantCount ? sMant[slot] : nullptr;
}

uint8_t* pc_rando_palette_mant_tlut(int slot, uint8_t* orig) {
    if (!pc_rando() || orig == nullptr || slot < 0 || slot >= PCR_PALSITE_MANT_SLOTS) {
        return orig;
    }
    int h = 0, v = 0;
    if (!pc_rando_palette_shift(siteGroup(PCR_PALSITE_COUNT), &h, &v)) {
        return orig;
    }
    MantCache& c = sMantCache[slot];
    // Keyed by the shift and the source bytes (the runtime-assets build fills them at start-up).
    if (!c.valid || c.h != h || c.v != v || c.orig != orig || memcmp(c.src, orig, sizeof c.src) != 0) {
        // shift_hardcoded_darknut_colors: decode_palettes(RGB5A3, 16, C4), hsv_shift_palette,
        // generate_new_palettes_from_colors + encode_palette (0x20 bytes).
        uint8_t rgba[16 * 4];
        memcpy(c.src, orig, sizeof c.src);
        pc_rando_tex_decode_palette(orig, 16, PCR_PAL_RGB5A3, rgba);
        pcr::hsvShiftPalette(rgba, 16, 4, h, v);
        pc_rando_tex_encode_palette(rgba, 16, PCR_PAL_RGB5A3, c.pal);
        c.h = h;
        c.v = v;
        c.orig = orig;
        c.valid = true;
        sStats.mantShifted++;
    }
    return c.pal;
}

void pc_rando_palette_code_stats(PcRandoPaletteCodeStats* out) {
    *out = sStats;
    out->editorRegistered = sEditorRegistered ? 1 : 0;
}

} // extern "C"
