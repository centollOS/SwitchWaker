// COS_SMOKE=rando COS_RANDO_SMOKE=colors (docs/RANDOMIZER_PLAN.md step R7.5b): the player's custom
// colours (features/rando/pc_rando_player_colors.h, game_hooks/pc_rando_hooks_player_colors.cpp), e.g.
//   run.sh rando --stage M_NewD2:0:0 --env COS_RANDO_SMOKE=colors --env COS_RANDO_FORCE=1 \
//       --env COS_BOOT_ITEMS=2D,38 --env 'COS_RANDO_CUSTOM_COLORS={"Eyes":[255,0,0], ...}'
// (a stage where the sword and the boomerang can be used: not the sea, where Link swims, nor Asoko).
// Once the play scene has run 30 frames:
//   1. The configuration the hooks use is the custom_colors given (COS_RANDO_CUSTOM_COLORS, else the
//      seed's, with its player_in_casual_clothes), parsed again here.
//   2. Link.arc cl.bdl "hitomi": a private copy of Link.arc is read from the disc; its hitomi bytes,
//      recoloured by the pure code when the pupils are chosen (and pc_rando()), must equal the live
//      model data's; otherwise the live bytes must be the disc's.
//   3. common.jpc particle 0x48: its live BSP1 prm colour is the chosen arrow colour (pc_rando()), else
//      the disc file's.
//   4. LkAnm.arc cutfh.brk / cutfm.brk are read from the game's ARAM archive as setSwordModel reads
//      them and edited by the pure code; the hunk's last buffer (cutfh: the Hero's Sword) must equal
//      that, and the disc's bytes without a sword trail colour or without pc_rando().
//   5. Driven pad: B slashes (the sword is drawn, setSwordModel runs the BRK hunk, daPy_swBlur_c::draw
//      draws the trail), then X throws the boomerang (COS_BOOT_ITEMS puts it on X; its trail is drawn
//      by daBoomerang_blur_c::draw). Every colour a hunk drew with equals the chosen one (or the
//      vanilla one); without pc_rando() no hunk ever calls a hook. The pad waits until no event runs
//      (the stage's title card).
//   6. Without COS_RANDO_FORCE but with colours: pc_rando_set_active (no seed) recolours common.jpc at
//      once and hitomi at the createHeap hunk's call (made here as the hunk makes it), checked as in 2
//      and 3; pc_rando_deactivate then puts the disc's bytes back.
// Everything is written to COS_COLORS_DUMP (default $COS_RUN_DIR/player_colors.txt) for
// native/tools/rando/player_colors_check.py, which computes the same values with wwrando's own
// customizer.change_player_custom_colors and gclib on the disc. Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_j3d.h"
#include "features/rando/pc_rando_jpc.h"
#include "features/rando/pc_rando_options.h"
#include "features/rando/pc_rando_player_colors.h"
#include "features/rando/pc_rando_seedfile.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_player_colors.h"

#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "d/d_com_inf_game.h"
#include "res/Object/LkAnm.h"
#include "res/Object/Link.h"

#include <dolphin/dvd.h>
#include <dolphin/pad.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "colors";

enum State { kInit, kQuiet, kSlash, kThrow, kDone };
unsigned sQuiet = 0;
State sState = kInit;
unsigned sStart = 0;
JKRExpHeap* sHeap = nullptr;
FILE* sDump = nullptr;
PcRandoPlayerColors sWant; // parsed here, independently of the hooks
bool sWantOk = false;

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
    return s;
}

void openDump() {
    const char* path = getenv("COS_COLORS_DUMP");
    char buf[1024];
    if (path == nullptr || path[0] == '\0') {
        const char* run = getenv("COS_RUN_DIR");
        if (run == nullptr || run[0] == '\0') {
            return;
        }
        snprintf(buf, sizeof buf, "%s/player_colors.txt", run);
        path = buf;
    }
    sDump = fopen(path, "w");
    if (sDump != nullptr) {
        cosLogf("[cos] rando-smoke colors: dump %s\n", path);
    }
}

// An uncompressed disc file read into malloc memory. NULL on failure.
uint8_t* readDiscFile(const char* path, size_t* size) {
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        return nullptr;
    }
    *size = info.length;
    DVDClose(&info);
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    if (buf == nullptr) {
        return nullptr;
    }
    uint8_t* out = (uint8_t*)malloc(*size);
    if (out != nullptr) {
        memcpy(out, buf, *size);
    }
    sHeap->freeAll();
    return out;
}

// A file of a disc archive, read into a private mount (as the cosmetic smoke does).
uint8_t* readArcFile(const char* arcPath, const char* name, size_t* outSize) {
    void* buf = JKRDvdRipper::loadToMainRAM(arcPath, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        cosLogf("[cos] rando-smoke colors: %s: cannot mount a private copy\n", arcPath);
        sHeap->freeAll();
        return nullptr;
    }
    for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst(); l != nullptr; l = l->getNext()) {
        if (l->getObject() == arc) {
            JKRFileLoader::getVolumeList().remove(l);
            break;
        }
    }
    uint8_t* out = nullptr;
    void* res = arc->getResource(0, name);
    if (res != nullptr) {
        *outSize = arc->getResSize(res);
        out = (uint8_t*)malloc(*outSize);
        if (out != nullptr) {
            memcpy(out, res, *outSize);
        }
    }
    arc->unmountFixed();
    sHeap->freeAll();
    return out;
}

bool chosen() {
    return pc_rando() && sWantOk;
}

// Step 1.
void checkConfig() {
    // The colours the hooks must use: COS_RANDO_CUSTOM_COLORS, else the active seed's custom_colors
    // (and its player_in_casual_clothes), read here independently of the hooks.
    const char* env = getenv("COS_RANDO_CUSTOM_COLORS");
    std::string text = env != nullptr ? env : "";
    std::string source = "COS_RANDO_CUSTOM_COLORS";
    int casual = 0;
    const PcRandoSeed* seed = pc_rando_seed();
    if (seed != nullptr) {
        const pcr::Json* c = seed->options.find("player_in_casual_clothes");
        casual = c != nullptr && c->isBool() && c->asBool() ? 1 : 0;
        const pcr::Json* cc = seed->options.find("custom_colors");
        if (text.empty() && cc != nullptr) {
            text = pcr::jsonDump(*cc, -1);
            source = "the seed's custom_colors";
        }
    }
    std::string log;
    sWantOk = pc_rando_player_colors_parse_text(text.c_str(), casual, &sWant, &log) > 0;
    if (sDump != nullptr) {
        fprintf(sDump, "config %d %s\n", casual, text.empty() ? "{}" : text.c_str());
        fprintf(sDump, "rando %d\n", pc_rando());
    }
    if (!pc_rando()) {
        return;
    }
    const PcRandoPlayerColors* live = pc_rando_player_colors_config();
    bool same = live->casual == casual && live->hasEyes == sWant.hasEyes && memcmp(live->eyes, sWant.eyes, 3) == 0;
    for (int t = 0; t < PCR_TRAIL_COUNT; t++) {
        same = same && live->hasTrail[t] == sWant.hasTrail[t] &&
               (!sWant.hasTrail[t] || memcmp(live->trail[t], sWant.trail[t], 4) == 0);
    }
    randoSmokeCheck(kName, ("the hooks' configuration = " + source).c_str(), same);
    randoSmokeCheck(kName, "the status says a configuration was read", pc_rando_player_colors_status()->active == 1);
}

struct FindHitomi {
    int index = -1;
    PcRandoTexture tex{};
};

void findHitomi(int index, const char* name, const PcRandoTexture* tex, int openResult, void* ctx) {
    FindHitomi* f = (FindHitomi*)ctx;
    if (f->index < 0 && openResult == PCR_TEX_OK && name != nullptr && strcmp(name, PCR_PLAYER_HITOMI) == 0) {
        f->index = index;
        f->tex = *tex;
    }
}

// Step 2.
void checkHitomi() {
    size_t size = 0;
    uint8_t* cl = readArcFile("/res/Object/Link.arc", "cl.bdl", &size);
    if (!randoSmokeCheck(kName, "Link.arc cl.bdl read from the disc", cl != nullptr)) {
        return;
    }
    FindHitomi disc;
    pc_rando_j3d_visit_textures(cl, size, 0, findHitomi, &disc);
    if (!randoSmokeCheck(kName, "the disc's cl.bdl has a hitomi texture", disc.index >= 0)) {
        free(cl);
        return;
    }
    const std::vector<uint8_t> discImage(disc.tex.image, disc.tex.image + disc.tex.imageSize);
    const std::vector<uint8_t> discPal(disc.tex.palette, disc.tex.palette + (size_t)disc.tex.numColors * 2);
    const int discColors = disc.tex.numColors;
    PcRandoTexture want = disc.tex;
    int r = PCR_TEX_SKIPPED;
    if (chosen() && sWant.hasEyes) {
        r = pc_rando_player_colors_hitomi(&want, &sWant);
        randoSmokeCheck(kName, "the pure recolour of the disc's hitomi", r == PCR_TEX_OK);
    }
    J3DModelData* md = (J3DModelData*)dComIfG_getObjectRes("Link", dRes_INDEX_LINK_BDL_CL_e);
    J3DTexture* tex = md != nullptr ? md->getTexture() : nullptr;
    JUTNameTab* names = md != nullptr ? md->getTextureName() : nullptr;
    int liveCount = 0;
    bool same = tex != nullptr && names != nullptr;
    for (u16 i = 0; same && i < tex->getNum(); i++) {
        const char* name = names->getName(i);
        if (name == nullptr || strcmp(name, PCR_PLAYER_HITOMI) != 0) {
            continue;
        }
        PcRandoTexture t;
        if (pc_rando_player_tex_open(&t, (uint8_t*)tex->getResTIMG(i)) != PCR_TEX_OK) {
            same = false;
            break;
        }
        liveCount++;
        const std::string img = md5Hex(t.image, t.imageSize);
        const std::string pal = md5Hex(t.palette, (size_t)t.numColors * 2);
        cosLogf("[cos] rando-smoke colors: live hitomi %u: format %d palette %d colours %d img %s pal %s\n",
                (unsigned)i, t.format, t.palFormat, t.numColors, img.c_str(), pal.c_str());
        if (sDump != nullptr) {
            fprintf(sDump, "hitomi %d %d %d colors %d img %s pal %s\n", liveCount - 1, t.format, t.palFormat,
                    t.numColors, img.c_str(), pal.c_str());
        }
        if (r == PCR_TEX_OK) {
            same = t.numColors == want.numColors && memcmp(t.image, want.image, t.imageSize) == 0 &&
                   memcmp(t.palette, want.palette, (size_t)t.numColors * 2) == 0;
        } else {
            same = t.numColors == discColors && t.imageSize == discImage.size() &&
                   memcmp(t.image, discImage.data(), t.imageSize) == 0 &&
                   memcmp(t.palette, discPal.data(), discPal.size()) == 0;
        }
    }
    randoSmokeCheck(kName, "the live model data has hitomi", liveCount >= 1);
    randoSmokeCheck(kName, r == PCR_TEX_OK ? "live hitomi = the disc's recoloured" : "live hitomi = the disc's",
                    same);
    const PcRandoPlayerColorsStatus* st = pc_rando_player_colors_status();
    if (r == PCR_TEX_OK) {
        randoSmokeCheck(kName, "the createHeap hunk recoloured it", st->hitomiApplied >= 1 && st->hitomiResult == 0);
    } else {
        randoSmokeCheck(kName, "no hitomi recolour", st->hitomiApplied == 0);
    }
    free(cl);
}

// Step 3.
void checkArrow() {
    uint32_t size = 0;
    const uint8_t* live = pc_rando_particles_common_data(&size, nullptr);
    size_t discSize = 0;
    uint8_t* disc = readDiscFile("/res/Particle/common.jpc", &discSize);
    if (!randoSmokeCheck(kName, "common.jpc live and on the disc", live != nullptr && disc != nullptr)) {
        free(disc);
        return;
    }
    std::vector<PcRandoJpcParticle> lp, dp;
    pc_rando_jpc_scan(live, size, &lp, nullptr);
    pc_rando_jpc_scan(disc, discSize, &dp, nullptr);
    PcRandoJpcRgba lc{}, dc{};
    bool found = false;
    for (const PcRandoJpcParticle& p : lp) {
        if (p.id == PCR_PLAYER_ARROW_PARTICLE) {
            found = pc_rando_jpc_get_color(live, p, PCR_JPC_BSP1_PRM, &lc) == PCR_JPC_OK;
        }
    }
    for (const PcRandoJpcParticle& p : dp) {
        if (p.id == PCR_PLAYER_ARROW_PARTICLE) {
            found = found && pc_rando_jpc_get_color(disc, p, PCR_JPC_BSP1_PRM, &dc) == PCR_JPC_OK;
        }
    }
    randoSmokeCheck(kName, "particle 0x48's BSP1 prm colour read", found);
    PcRandoJpcRgba want = dc;
    if (chosen() && sWant.hasTrail[PCR_TRAIL_ARROW]) {
        want.r = sWant.trail[PCR_TRAIL_ARROW][0];
        want.g = sWant.trail[PCR_TRAIL_ARROW][1];
        want.b = sWant.trail[PCR_TRAIL_ARROW][2];
        if (sWant.trailHasAlpha[PCR_TRAIL_ARROW]) {
            want.a = sWant.trail[PCR_TRAIL_ARROW][3];
        }
    }
    cosLogf("[cos] rando-smoke colors: arrow trail prm live %d %d %d %d disc %d %d %d %d\n", lc.r, lc.g, lc.b, lc.a,
            dc.r, dc.g, dc.b, dc.a);
    if (sDump != nullptr) {
        fprintf(sDump, "arrow prm %d %d %d %d\n", lc.r, lc.g, lc.b, lc.a);
    }
    randoSmokeCheck(kName, "arrow trail colour = the chosen one (else the disc's)",
                    lc.r == want.r && lc.g == want.g && lc.b == want.b && lc.a == want.a);
    // Every other byte of common.jpc is the disc's.
    bool rest = size == discSize;
    for (const PcRandoJpcParticle& p : lp) {
        if (p.id == PCR_PLAYER_ARROW_PARTICLE && p.bsp1 >= 0 && rest) {
            const uint32_t at = p.blocks[p.bsp1].offset + 0x2C;
            std::vector<uint8_t> a(live, live + size), b(disc, disc + discSize);
            memset(&a[at], 0, 4);
            memset(&b[at], 0, 4);
            rest = a == b;
        }
    }
    randoSmokeCheck(kName, "the rest of common.jpc = the disc's", rest);
    free(disc);
}

void dumpBrk(const char* name, const uint8_t* data, size_t size) {
    for (int ch = 0; ch < 4; ch++) {
        const int n = pc_rando_brk_key_count(data, size, 0, ch);
        std::string line;
        for (int k = 0; k < n; k++) {
            int16_t v = 0;
            pc_rando_brk_key_value(data, size, 0, ch, k, &v);
            line += " " + std::to_string(v);
        }
        cosLogf("[cos] rando-smoke colors: %s channel %d:%s\n", name, ch, line.c_str());
        if (sDump != nullptr) {
            fprintf(sDump, "brk %s %d%s\n", name, ch, line.c_str());
        }
    }
}

// Step 4 (cutfm here; the hunk's cutfh buffer is compared after the slashes).
std::vector<uint8_t> sCutfhWant;
std::vector<uint8_t> sCutfhDisc;

void readCutf() {
    JKRArchive* anm = dComIfGp_getAnmArchive();
    if (!randoSmokeCheck(kName, "LkAnm.arc mounted", anm != nullptr)) {
        return;
    }
    const struct {
        const char* name;
        u32 index;
    } files[2] = {{"cutfh.brk", dRes_INDEX_LKANM_BRK_CUTFH_e}, {"cutfm.brk", dRes_INDEX_LKANM_BRK_CUTFM_e}};
    for (const auto& f : files) {
        uint8_t* buf = (uint8_t*)JKRAllocFromHeap(sHeap, 0x400, 0x20);
        memset(buf, 0, 0x400);
        JKRReadIdxResource(buf, 0x400, f.index, anm);
        const size_t size = ((size_t)buf[8] << 24) | ((size_t)buf[9] << 16) | ((size_t)buf[10] << 8) | buf[11];
        const bool ok = size >= 0x20 && size <= 0x400 && pc_rando_brk_reg_anim_count(buf, size) >= 1;
        if (!randoSmokeCheck(kName, f.name, ok)) {
            sHeap->freeAll();
            continue;
        }
        std::vector<uint8_t> disc(buf, buf + size);
        std::vector<uint8_t> want = disc;
        if (chosen()) {
            const int w = pc_rando_player_colors_brk(want.data(), want.size(), &sWant);
            randoSmokeCheck(kName, "the pure key frame edit", w >= 0);
        }
        dumpBrk(f.name, want.data(), want.size());
        if (f.index == dRes_INDEX_LKANM_BRK_CUTFH_e) {
            sCutfhWant = want;
            sCutfhDisc = disc;
        }
        sHeap->freeAll();
    }
}

void dumpTrails() {
    for (int t = 0; t < PCR_TRAIL_COUNT; t++) {
        uint8_t c[4];
        memcpy(c, kPcRandoTrailVanilla[t], 4);
        if (chosen()) {
            pc_rando_player_trail(&sWant, t, c);
        }
        if (sDump != nullptr && t != PCR_TRAIL_ARROW) {
            fprintf(sDump, "trail %s %d %d %d %d\n", pc_rando_trail_key(t), c[0], c[1], c[2], c[3]);
        }
    }
}

// Step 5: the colours the hunks drew with.
void checkDrawn() {
    const PcRandoPlayerColorsStatus* st = pc_rando_player_colors_status();
    for (int t = 0; t < 4; t++) {
        cosLogf("[cos] rando-smoke colors: trail %s: %u hook calls, last %d %d %d %d\n", pc_rando_trail_key(t),
                st->trailCalls[t], st->trailLast[t].r, st->trailLast[t].g, st->trailLast[t].b, st->trailLast[t].a);
    }
    if (!pc_rando()) {
        bool none = st->brkCalls == 0 && st->hitomiApplied == 0;
        for (int t = 0; t < 4; t++) {
            none = none && st->trailCalls[t] == 0;
        }
        randoSmokeCheck(kName, "vanilla: no hunk called a colour hook", none);
        return;
    }
    // The slash trail and the boomerang trail must have been drawn.
    const int drawn[2] = {PCR_TRAIL_SWORD_SLASH, PCR_TRAIL_BOOMERANG};
    for (int t : drawn) {
        char what[128];
        snprintf(what, sizeof what, "%s drawn (%u draws)", pc_rando_trail_key(t), st->trailCalls[t]);
        randoSmokeCheck(kName, what, st->trailCalls[t] > 0);
    }
    for (int t = 0; t < 4; t++) {
        if (st->trailCalls[t] == 0) {
            continue;
        }
        uint8_t c[4];
        memcpy(c, kPcRandoTrailVanilla[t], 4);
        pc_rando_player_trail(&sWant, t, c);
        char what[128];
        snprintf(what, sizeof what, "%s drawn with %02X%02X%02X%02X", pc_rando_trail_key(t), c[0], c[1], c[2], c[3]);
        randoSmokeCheck(kName, what,
                        st->trailLast[t].r == c[0] && st->trailLast[t].g == c[1] && st->trailLast[t].b == c[2] &&
                            st->trailLast[t].a == c[3]);
    }
    // setSwordModel's BRK hunk (the Hero's Sword: cutfh.brk).
    char what[128];
    snprintf(what, sizeof what, "the cutf BRK hunk ran (%u calls, %d key frames, %d bytes changed)", st->brkCalls,
             st->brkWritten, st->brkChangedBytes);
    randoSmokeCheck(kName, what, st->brkCalls > 0 && st->brkWritten >= 0);
    if (st->brkCalls > 0 && !sCutfhWant.empty()) {
        const bool same = st->brkLastSize == sCutfhWant.size() &&
                          memcmp(st->brkLast, sCutfhWant.data(), sCutfhWant.size()) == 0;
        if (!same) {
            size_t at = 0;
            while (at < sCutfhWant.size() && at < sizeof st->brkLast && st->brkLast[at] == sCutfhWant[at]) {
                at++;
            }
            cosLogf("[cos] rando-smoke colors: hunk cutfh %u bytes, expected %zu; first difference at 0x%zx\n",
                    st->brkLastSize, sCutfhWant.size(), at);
        }
        randoSmokeCheck(kName, sWant.hasTrail[0] || sWant.hasTrail[1] || sWant.hasTrail[2]
                                   ? "the hunk's cutfh.brk = the disc's with the chosen key frames"
                                   : "the hunk's cutfh.brk = the disc's",
                        same);
        if (!(sWant.hasTrail[0] || sWant.hasTrail[1] || sWant.hasTrail[2])) {
            randoSmokeCheck(kName, "no byte changed", st->brkChangedBytes == 0 && sCutfhWant == sCutfhDisc);
        }
    }
}

// Step 6, in a vanilla run given colours: an activation (no seed) recolours the resident data at
// once, and the deactivation puts the disc's bytes back (what a vanilla file loaded after a randomized
// one sees).
void checkCycle() {
    if (pc_rando() || !sWantOk) {
        return;
    }
    cosLogf("[cos] rando-smoke colors: activation cycle\n");
    pc_rando_set_active(nullptr, nullptr);
    // A file is activated before its first stage loads; the player's createHeap then hands its
    // cl.bdl model data to the hunk. Do what that hunk does, with the same model data.
    pc_rando_player_colors_cl_model(dComIfG_getObjectRes("Link", dRes_INDEX_LINK_BDL_CL_e));
    checkHitomi();
    checkArrow();
    pc_rando_deactivate();
    randoSmokeCheck(kName, "deactivated", !pc_rando());
    checkHitomi();
    checkArrow();
    randoSmokeCheck(kName, "the hitomi bytes were put back", pc_rando_player_colors_status()->hitomiRestored >= 1);
}

// The pad: B taps (draw the sword, slash), then X held (the boomerang aimed) and released (thrown).
uint16_t slashPad(unsigned t) {
    return (t % 16) < 4 ? PAD_BUTTON_B : 0;
}
uint16_t throwPad(unsigned t) {
    const unsigned c = t % 120;
    // Held, the boomerang comes out and the aim view opens; released, it is thrown.
    return c < 45 ? PAD_BUTTON_X : 0;
}

} // namespace

void randoSmokeColors(unsigned int frames) {
    switch (sState) {
    case kInit:
        if (!randoSmokeInPlay(30) || dComIfGp_getPlayer(0) == nullptr) {
            return;
        }
        for (u32 size = 16u << 20; sHeap == nullptr && size >= (4u << 20); size >>= 1) {
            sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
        }
        if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr)) {
            randoSmokeEnd(kName);
        }
        cosLogf("[cos] rando-smoke colors: pc_rando() %d, stage %s\n", pc_rando(), dComIfGp_getStartStageName());
        openDump();
        checkConfig();
        dumpTrails();
        checkHitomi();
        checkArrow();
        readCutf();
        sState = kQuiet;
        sStart = frames;
        return;
    case kQuiet:
        // The stage's intro (title card, camera) holds the player: wait until no event runs.
        if (dComIfGp_event_runCheck()) {
            sQuiet = 0;
        } else {
            sQuiet++;
        }
        if (sQuiet < 30 && frames - sStart < 1800) {
            return;
        }
        cosLogf("[cos] rando-smoke colors: driving the pad from frame %u\n", frames);
        sState = kSlash;
        sStart = frames;
        return;
    case kSlash:
        setDrivenPad(true, slashPad(frames - sStart), 0, 0);
        if (frames - sStart < 160) {
            return;
        }
        setDrivenPad(true, 0, 0, 0);
        sState = kThrow;
        sStart = frames;
        return;
    case kThrow:
        setDrivenPad(true, throwPad(frames - sStart), 0, 0);
        if ((frames - sStart) % 60 == 0) {
            const u8 slot = dComIfGs_getSelectItem(dItemBtn_X_e);
            cosLogf("[cos] rando-smoke colors: throw frame %u: X slot %u item 0x%02X, status0 0x%08X, boomerang draws %u\n",
                    frames - sStart, slot, slot < dInvSlot_ReserveLast_e ? dComIfGs_getItem(slot) : 0xFF,
                    (unsigned)dComIfGp_checkPlayerStatus0(0, 0xFFFFFFFFu), pc_rando_player_colors_status()->trailCalls[3]);
        }
        if (frames - sStart < 360) {
            return;
        }
        setDrivenPad(false, 0, 0, 0);
        checkDrawn();
        if (sDump != nullptr) {
            fclose(sDump);
            sDump = nullptr;
        }
        checkCycle();
        sState = kDone;
        randoSmokeEnd(kName);
    case kDone:
        return;
    }
}

} // namespace pc
