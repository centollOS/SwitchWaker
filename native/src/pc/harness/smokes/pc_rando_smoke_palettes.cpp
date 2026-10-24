// COS_SMOKE=rando COS_RANDO_SMOKE=palettes (docs/RANDOMIZER_PLAN.md steps R7.2c, R7.2d): the raw J3D
// hook and the enemy palettes on whole files (features/rando/pc_rando_palettes.h), e.g.
//   run.sh rando --stage Fairy01:0:0 --env COS_RANDO_SMOKE=palettes --env COS_RANDO_FORCE=1 \
//          --env COS_RANDO_PALETTE_TEST=120,-20
// Once the play scene has run 10 frames, every palette group's /res/Object/<RARC>.arc (group order,
// each once) and Nzg.arc are loaded through the game's own path (dComIfG_setObjectRes, so
// dRes_info_c::loadResource runs the raw J3D hook and the post-load hook), four at a time, and freed
// again. For each loaded archive, every .bdl .bmd .bmt .brk .bti file's bytes as the game holds them
// (after the loaders) give one line per texture header in COS_PALETTE_DUMP (default
// $COS_RUN_DIR/palettes.txt):
//     tex <arc> <file> <index> <name|-> <format> <palette format> img <md5> pal <md5|->
// (MD5 of the header's image bytes, every mipmap, and of its palette for palette formats; names with
// "toon", which wwrando skips, give "tex <arc> <file> <index> <name> toon"), for every J3D file its
// material and animation colours (R7.2d, pc_rando_j3d_color_lines: MAT3 tev / konst colours, MDL3 TEV
// colour register commands, TRK1 colour animations)
//     j3d <arc> <file> mat|mdl|trk ...
// plus
//     rathole Nzg kana_00.bdl cull <u32 at 0xC80> gen <u8 at 0xFCE>
// native/tools/rando/palette_check.py builds the same lines with wwrando's own PaletteRandomizer
// (shift_all_colors_in_tex1 / _mat3 / _mdl3 / _trk1 / _bti and gclib's save) and
// make_rat_holes_visible_from_behind run with gclib on the disc, and compares. Checks here:
//   - every archive loaded, the dump written;
//   - under pc_rando() with the test shift: group files were recoloured with no read error, no
//     same-name mismatch and no shared-data conflict; material colours, MDL3 entries and TRK1
//     animations were recoloured, every TRK1 in gclib's layout (none in place); kana_00.bdl patched (cull 0, gen 0x04) and every
//     material of its loaded J3DModelData has cull mode GX_CULL_NONE;
//   - without pc_rando(): the patchers never ran, kana_00.bdl keeps the disc's values (2, 0x44) and
//     its materials cull back faces.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_gen_palettes.h"
#include "features/rando/pc_rando_j3d.h"
#include "features/rando/pc_rando_options.h"
#include "features/rando/pc_rando_palettes.h"
#include "features/rando/pc_rando_resources.h"
#include "pc/pc_rando.h"

#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "d/d_com_inf_game.h"
#include "d/d_resorce.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "palettes";
constexpr size_t kBatch = 4;
constexpr unsigned kMaxWait = 900;

enum State { kInit, kLoad, kWait, kDone };
State sState = kInit;
std::vector<std::string> sArcs;
size_t sNext = 0;
std::vector<std::string> sBatch;
unsigned sWait = 0;
FILE* sDump = nullptr;
int sLoaded = 0, sFailed = 0, sFiles = 0, sTextures = 0, sUnreadable = 0;
int sRatCull = -1, sRatGen = -1, sRatMatOk = -1;
int sRatMats = 0;

void cosLogf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void cosLogf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

std::string md5Hex(const uint8_t* p, size_t n) {
    uint8_t d[16];
    pcr::md5(p, n, d);
    char s[33];
    for (int i = 0; i < 16; i++) {
        snprintf(s + i * 2, 3, "%02x", d[i]);
    }
    return std::string(s, 32);
}

FILE* openDump() {
    const char* path = getenv("COS_PALETTE_DUMP");
    char buf[1024];
    if (path == nullptr || path[0] == '\0') {
        const char* run = getenv("COS_RUN_DIR");
        if (run == nullptr || run[0] == '\0') {
            return nullptr;
        }
        snprintf(buf, sizeof buf, "%s/palettes.txt", run);
        path = buf;
    }
    FILE* f = fopen(path, "w");
    if (f != nullptr) {
        cosLogf("[cos] rando-smoke palettes: dump %s\n", path);
    }
    return f;
}

void buildList() {
    for (int g = 0; g < pcr::paletteGroupCount(); g++) {
        const pcr::PaletteGroup& grp = pcr::paletteGroup(g);
        for (int r = 0; r < grp.numRarcs; r++) {
            bool seen = false;
            for (const std::string& a : sArcs) {
                seen = seen || strcasecmp(a.c_str(), grp.rarcs[r].name) == 0;
            }
            if (!seen) {
                sArcs.emplace_back(grp.rarcs[r].name);
            }
        }
    }
    sArcs.emplace_back(PCR_RAT_HOLE_ARC);
}

struct VisitCtx {
    const char* arc;
    const char* file;
};

void colorLine(const char* line, void* vctx) {
    const VisitCtx* c = (const VisitCtx*)vctx;
    if (sDump != nullptr) {
        fprintf(sDump, "j3d %s %s %s\n", c->arc, c->file, line);
    }
}

void visit(int index, const char* name, const PcRandoTexture* tex, int openResult, void* vctx) {
    const VisitCtx* c = (const VisitCtx*)vctx;
    sTextures++;
    const char* nm = (name == nullptr || name[0] == '\0') ? "-" : name;
    if (strstr(nm, "toon") != nullptr) {
        // wwrando never touches them (lighting textures; their headers are placeholders the game
        // replaces with dDlst_list_c's toon images): no bytes to compare.
        if (sDump != nullptr) {
            fprintf(sDump, "tex %s %s %d %s toon\n", c->arc, c->file, index, nm);
        }
        return;
    }
    if (openResult != PCR_TEX_OK) {
        sUnreadable++;
        if (sDump != nullptr) {
            fprintf(sDump, "tex %s %s %d %s err %s\n", c->arc, c->file, index, nm,
                    pc_rando_tex_result_name(openResult));
        }
        return;
    }
    const std::string img = md5Hex(tex->image, tex->imageSize);
    std::string pal = "-";
    if (pc_rando_tex_uses_palette(tex->format)) {
        pal = md5Hex(tex->palette, tex->palette != nullptr ? (size_t)tex->numColors * 2 : 0);
    }
    if (sDump != nullptr) {
        fprintf(sDump, "tex %s %s %d %s %d %d img %s pal %s\n", c->arc, c->file, index, nm, tex->format,
                tex->palFormat, img.c_str(), pal.c_str());
    }
}

void checkRatModel() {
    J3DModelData* md = (J3DModelData*)dComIfG_getObjectRes(PCR_RAT_HOLE_ARC, PCR_RAT_HOLE_FILE);
    if (md == nullptr) {
        cosLogf("[cos] rando-smoke palettes: %s has no loaded %s\n", PCR_RAT_HOLE_ARC, PCR_RAT_HOLE_FILE);
        sRatMatOk = 0;
        return;
    }
    const u8 want = pc_rando() ? 0 : 2; // GX_CULL_NONE / GX_CULL_BACK
    sRatMatOk = 1;
    sRatMats = md->getMaterialNum();
    for (u16 i = 0; i < md->getMaterialNum(); i++) {
        J3DMaterial* m = md->getMaterialNodePointer(i);
        const u8 cull = m->getColorBlock() != nullptr ? m->getColorBlock()->getCullMode() : 0xFF;
        cosLogf("[cos] rando-smoke palettes: kana_00.bdl material %u cull mode %u\n", i, cull);
        if (cull != want) {
            sRatMatOk = 0;
        }
    }
}

void dumpArchive(const char* arcName) {
    dRes_info_c* info = dComIfG_getObjectResInfo(arcName);
    JKRArchive* arc = info != nullptr ? info->getArchive() : nullptr;
    if (arc == nullptr) {
        sFailed++;
        cosLogf("[cos] rando-smoke palettes: %s: no archive\n", arcName);
        return;
    }
    sLoaded++;
    const u32 n = arc->countFile();
    for (u32 i = 0; i < n; i++) {
        JKRArchive::SDirEntry e;
        if (!arc->getDirEntry(&e, i) || (e.flags & 0x02) != 0) {
            continue;
        }
        const int kind = pc_rando_recolor_kind(e.name);
        if (kind == PCR_RECOLOR_NONE) {
            continue;
        }
        JKRArchive::SDIFileEntry* fe = arc->findIdxResource(i);
        uint8_t* data = fe != nullptr ? (uint8_t*)arc->fetchResource(fe, nullptr) : nullptr; // the cached bytes
        const u32 size = data != nullptr ? arc->getResSize(data) : 0;
        if (data == nullptr || size == 0 || size == 0xFFFFFFFFu) {
            cosLogf("[cos] rando-smoke palettes: %s/%s: no bytes\n", arcName, e.name);
            sUnreadable++;
            continue;
        }
        sFiles++;
        VisitCtx c{arcName, e.name};
        const int v = pc_rando_j3d_visit_textures(data, size, kind == PCR_RECOLOR_BTI, visit, &c);
        if (v < 0) {
            sUnreadable++;
            if (sDump != nullptr) {
                fprintf(sDump, "tex %s %s unreadable\n", arcName, e.name);
            }
        }
        if (kind == PCR_RECOLOR_J3D && pc_rando_j3d_color_lines(data, size, colorLine, &c) < 0) {
            sUnreadable++;
            if (sDump != nullptr) {
                fprintf(sDump, "j3d %s %s unreadable\n", arcName, e.name);
            }
        }
        if (strcasecmp(arcName, PCR_RAT_HOLE_ARC) == 0 && strcmp(e.name, PCR_RAT_HOLE_FILE) == 0 && size > 0xFCE) {
            sRatCull = (int)(((u32)data[0xC80] << 24) | ((u32)data[0xC81] << 16) | ((u32)data[0xC82] << 8) | data[0xC83]);
            sRatGen = data[0xFCE];
            if (sDump != nullptr) {
                fprintf(sDump, "rathole %s %s cull %d gen %d\n", arcName, e.name, sRatCull, sRatGen);
            }
        }
    }
    if (strcasecmp(arcName, PCR_RAT_HOLE_ARC) == 0) {
        checkRatModel();
    }
}

void finish() {
    if (sDump != nullptr) {
        fclose(sDump);
        sDump = nullptr;
    }
    char what[192];
    snprintf(what, sizeof what, "%d of %zu archives loaded through dRes_control_c (%d files, %d textures)",
             sLoaded, sArcs.size(), sFiles, sTextures);
    randoSmokeCheck(kName, what, sLoaded == (int)sArcs.size() && sFailed == 0);
    randoSmokeCheck(kName, "every texture header readable", sUnreadable == 0);
    PcRandoPaletteStats st;
    pc_rando_palettes_stats(&st);
    PcRandoResStats rs;
    pc_rando_res_stats(&rs);
    cosLogf("[cos] rando-smoke palettes: patcher: %u files (%u J3D, %u BTI, %u excluded), %d textures, %d "
            "shifted, %d skipped, %d errors, %d mismatches, %d conflicts; rat holes %u (failed %u); raw "
            "dispatch %u (%u patched)\n",
            st.files, st.j3dFiles, st.btiFiles, st.excluded, st.tex.textures, st.tex.shifted, st.tex.skipped,
            st.tex.errors, st.tex.mismatch, st.tex.conflicts, st.ratHoles, st.ratHoleFailed, rs.rawDispatched,
            rs.rawPatched);
    cosLogf("[cos] rando-smoke palettes: patcher: %d material colours, %d MDL3 entries, %d TRK1 animations "
            "(%d chunks in gclib's layout, %d in place)\n",
            st.tex.matColors, st.tex.mdlEntries, st.tex.trkAnims, st.tex.trkRebuilt, st.tex.trkInPlace);
    int th = 0, tv = 0;
    const bool test = pc_rando_palettes_test_override(&th, &tv) != 0;
    if (pc_rando()) {
        randoSmokeCheck(kName, "the raw J3D hook dispatched files", rs.rawDispatched > 0 && rs.rawPatched > 0);
        if (test) {
            snprintf(what, sizeof what, "group files recoloured by (%d, %d)", th, tv);
            randoSmokeCheck(kName, what, st.j3dFiles > 0 && st.btiFiles > 0 && st.tex.shifted > 0);
            randoSmokeCheck(kName, "no read error, same-name mismatch or shared-data conflict",
                            st.tex.errors == 0 && st.tex.mismatch == 0 && st.tex.conflicts == 0);
            randoSmokeCheck(kName, "material colours, MDL3 entries and TRK1 animations recoloured",
                            st.tex.matColors > 0 && st.tex.mdlEntries > 0 && st.tex.trkAnims > 0);
            randoSmokeCheck(kName, "every TRK1 written in gclib's layout",
                            st.tex.trkRebuilt > 0 && st.tex.trkInPlace == 0);
        } else {
            randoSmokeCheck(kName, "no palettes in the seed: no file recoloured", st.files == 0);
        }
        randoSmokeCheck(kName, "kana_00.bdl patched (cull 0, gen 0x04)",
                        st.ratHoles >= 1 && st.ratHoleFailed == 0 && sRatCull == 0 && sRatGen == 0x04);
        snprintf(what, sizeof what, "kana_00.bdl's %d materials cull nothing", sRatMats);
        randoSmokeCheck(kName, what, sRatMatOk == 1);
    } else {
        randoSmokeCheck(kName, "vanilla: the patchers never ran",
                        st.files == 0 && st.ratHoles == 0 && st.ratHoleFailed == 0 && rs.rawDispatched == 0);
        randoSmokeCheck(kName, "vanilla: kana_00.bdl as on the disc (cull 2, gen 0x44)",
                        sRatCull == 2 && sRatGen == 0x44);
        snprintf(what, sizeof what, "vanilla: kana_00.bdl's %d materials cull back faces", sRatMats);
        randoSmokeCheck(kName, what, sRatMatOk == 1);
    }
    sState = kDone;
    randoSmokeEnd(kName);
}

} // namespace

void randoSmokePalettes(unsigned int frames) {
    (void)frames;
    switch (sState) {
    case kInit: {
        if (!randoSmokeInPlay(10)) {
            return;
        }
        int h = 0, v = 0;
        const bool test = pc_rando_palettes_test_override(&h, &v) != 0;
        cosLogf("[cos] rando-smoke palettes: pc_rando() %d, stage %s, test shift %s%d,%d\n", pc_rando(),
                dComIfGp_getStartStageName(), test ? "" : "none ", h, v);
        buildList();
        sDump = openDump();
        randoSmokeCheck(kName, "dump opened", sDump != nullptr);
        sState = kLoad;
        return;
    }
    case kLoad:
        sBatch.clear();
        while (sNext < sArcs.size() && sBatch.size() < kBatch) {
            const std::string& a = sArcs[sNext++];
            if (dComIfG_setObjectRes(a.c_str(), 0, nullptr) != 1) {
                cosLogf("[cos] rando-smoke palettes: setObjectRes %s failed\n", a.c_str());
                sFailed++;
                continue;
            }
            sBatch.push_back(a);
        }
        sWait = 0;
        sState = kWait;
        return;
    case kWait: {
        bool pending = false;
        for (const std::string& a : sBatch) {
            const int r = dComIfG_syncObjectRes(a.c_str());
            if (r > 0) {
                pending = true;
            }
        }
        if (pending && ++sWait < kMaxWait) {
            return;
        }
        for (const std::string& a : sBatch) {
            if (dComIfG_syncObjectRes(a.c_str()) != 0) {
                cosLogf("[cos] rando-smoke palettes: %s did not load\n", a.c_str());
                sFailed++;
            } else {
                dumpArchive(a.c_str());
            }
            dComIfG_deleteObjectRes(a.c_str());
        }
        if (sNext < sArcs.size()) {
            sState = kLoad;
        } else {
            finish();
        }
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc
