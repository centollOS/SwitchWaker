// The enemy palettes on the game's files (pc_rando_palettes.h, docs/RANDOMIZER_PLAN.md steps R7.2c, R7.2d).
// Pure: no game headers, no game calls.
#include "pc_rando_palettes.h"

#include "pc/pc_rando.h"
#include "pc_rando_gen_palettes.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
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

PcRandoPaletteStats sStats;

// COS_RANDO_PALETTE_TEST
bool sTestRead = false;
bool sTest = false;
int sTestH = 0, sTestV = 0;

void readTest() {
    if (sTestRead) {
        return;
    }
    sTestRead = true;
    const char* e = getenv("COS_RANDO_PALETTE_TEST");
    if (e == nullptr || e[0] == '\0') {
        return;
    }
    int h = 0, v = 0;
    if (sscanf(e, "%d,%d", &h, &v) == 2) {
        sTest = true;
        sTestH = h;
        sTestV = v;
        logf("[cos] rando: palettes: COS_RANDO_PALETTE_TEST: every group shifted by (%d, %d)\n", h, v);
    } else {
        logf("[cos] rando: palettes: COS_RANDO_PALETTE_TEST=%s is not <h>,<v>; ignored\n", e);
    }
}

// The active seed's shifts, parsed once per seed.
const PcRandoSeed* sSeed = nullptr;
uint8_t sSeedHash[16];
bool sSeedHas = false;
std::vector<pcr::PaletteShift> sShifts;

bool seedShifts() {
    const PcRandoSeed* seed = pc_rando_seed();
    if (seed == nullptr) {
        sSeed = nullptr;
        sSeedHas = false;
        return false;
    }
    if (seed != sSeed || memcmp(seed->hash, sSeedHash, sizeof sSeedHash) != 0) {
        sSeed = seed;
        memcpy(sSeedHash, seed->hash, sizeof sSeedHash);
        sSeedHas = false;
        sShifts.clear();
        const pcr::Json* j = seed->extra.find("palettes");
        if (j != nullptr) {
            std::string err;
            if (pcr::palettesFromJson(*j, sShifts, &err)) {
                sSeedHas = true;
            } else {
                logf("[cos] rando: palettes: the seed's palettes are unusable (%s); colours stay vanilla\n",
                     err.c_str());
            }
        }
    }
    return sSeedHas;
}

bool isObjectDir(const PcRandoResFile* f) {
    // dRes_info_c::set records the directory only under pc_rando(); "" when the archive was set up
    // before the activation.
    return f->dir[0] == '\0' || strcmp(f->dir, "/res/Object/") == 0;
}

bool excluded(const pcr::PaletteRarc& r, const char* name) {
    for (int i = 0; i < r.numExcluded; i++) {
        if (strcmp(r.excluded[i], name) == 0) {
            return true;
        }
    }
    return false;
}

void addStats(PcRandoRecolorStats* into, const PcRandoRecolorStats& s) {
    into->textures += s.textures;
    into->shifted += s.shifted;
    into->skipped += s.skipped;
    into->errors += s.errors;
    into->mismatch += s.mismatch;
    into->conflicts += s.conflicts;
    into->matColors += s.matColors;
    into->mdlEntries += s.mdlEntries;
    into->trkAnims += s.trkAnims;
    into->trkRebuilt += s.trkRebuilt;
    into->trkInPlace += s.trkInPlace;
}

// shift_colors_for_file_group's RARC loop, for this one file, over every group holding its archive.
void recolor(PcRandoResFile* f, int kind) {
    if (!isObjectDir(f)) {
        return;
    }
    const int n = pcr::paletteGroupCount();
    for (int g = 0; g < n; g++) {
        const pcr::PaletteGroup& grp = pcr::paletteGroup(g);
        for (int r = 0; r < grp.numRarcs; r++) {
            const pcr::PaletteRarc& rarc = grp.rarcs[r];
            if (strcasecmp(rarc.name, f->arc) != 0) {
                continue;
            }
            int h = 0, v = 0;
            if (!pc_rando_palette_shift(g, &h, &v)) {
                return; // palettes not randomized
            }
            if (excluded(rarc, f->name)) {
                sStats.excluded++;
                continue;
            }
            PcRandoRecolorStats st{};
            if (kind == PCR_RECOLOR_BTI) {
                pc_rando_bti_shift(f->data, f->size, h, v, &st);
                sStats.btiFiles++;
            } else {
                // TEX1, MAT3, MDL3 (as gclib's save regenerates it) and TRK1 (R7.2d)
                pc_rando_j3d_shift_file(f->data, f->size, f->name, h, v, &st);
                sStats.j3dFiles++;
            }
            sStats.files++;
            addStats(&sStats.tex, st);
            if (st.errors || st.mismatch || st.conflicts) {
                logf("[cos] rando: palettes: %s/%s (%s): %d errors, %d mismatches, %d conflicts\n", f->arc,
                     f->name, grp.name, st.errors, st.mismatch, st.conflicts);
            }
        }
    }
}

// The raw list: J3D files before the loaders.
void rawPatcher(PcRandoResFile* f) {
    if (strcasecmp(f->arc, PCR_RAT_HOLE_ARC) == 0 && strcmp(f->name, PCR_RAT_HOLE_FILE) == 0 && isObjectDir(f)) {
        // tweaks.py make_rat_holes_visible_from_behind (randomizer.py:445: every seed)
        const int r = pc_rando_rat_hole_patch(f->data, f->size);
        if (r == 1) {
            sStats.ratHoles++;
        } else if (r < 0) {
            sStats.ratHoleFailed++;
        }
    }
    if (pc_rando_recolor_kind(f->name) == PCR_RECOLOR_J3D) {
        recolor(f, PCR_RECOLOR_J3D);
    }
}

// The post-load list: BTI files (no loader touches them).
void btiPatcher(PcRandoResFile* f) {
    if (pc_rando_recolor_kind(f->name) == PCR_RECOLOR_BTI) {
        recolor(f, PCR_RECOLOR_BTI);
    }
}

} // namespace

extern "C" {

void pc_rando_palettes_init(void) {
    readTest();
    pc_rando_register_raw_patcher(rawPatcher, PCR_RES_J3D);
    pc_rando_register_res_patcher(btiPatcher, PCR_RES_ANY);
}

int pc_rando_palettes_test_override(int* h, int* v) {
    readTest();
    if (sTest) {
        if (h != nullptr) {
            *h = sTestH;
        }
        if (v != nullptr) {
            *v = sTestV;
        }
    }
    return sTest ? 1 : 0;
}

int pc_rando_palette_shift(int group, int* h, int* v) {
    if (!pc_rando() || group < 0 || group >= pcr::paletteGroupCount()) {
        return 0;
    }
    if (pc_rando_palettes_test_override(h, v)) {
        return 1;
    }
    if (!seedShifts() || (size_t)group >= sShifts.size()) {
        return 0;
    }
    *h = sShifts[(size_t)group].h;
    *v = sShifts[(size_t)group].v;
    return 1;
}

void pc_rando_palettes_stats(PcRandoPaletteStats* out) {
    *out = sStats;
}

void pc_rando_palettes_reset_stats(void) {
    memset(&sStats, 0, sizeof sStats);
}

} // extern "C"
