// The player's custom colours, game side (docs/RANDOMIZER_PLAN.md step R7.5b; pure part
// features/rando/pc_rando_player_colors.h, hooks pc/pc_rando_player_colors.h, hunks in
// docs/RANDO_HOOKS.md "R7.5b").
//
// wwrando bakes customizer.change_player_custom_colors into the ISO; the port applies the same edits
// where the game reads the data:
//   - the DOL trail colours: the daPy_swBlur_c::draw and daBoomerang_blur_c::draw hunks ask
//     pc_rando_player_trail_color for the colour each time they draw;
//   - LkAnm.arc cutfh.brk / cutfm.brk: LkAnm.arc is an ARAM archive whose files the player reads with
//     JKRReadIdxResource into a fresh buffer each time it equips the sword (setSwordModel), not
//     through dRes_info_c::loadResource, so the R7.2c raw J3D hook never sees them; the hunk there
//     edits that buffer before J3DAnmLoaderDataBase::load;
//   - common.jpc particle 0x48: a common.jpc editor of R7.1b's registry, registered at each
//     activation (R7.1b restores the file at deactivation). It is registered after createCommon in
//     a normal game, which reaches the BSP1 prm colour the JPA reads live (the one edited here);
//   - Link.arc cl.bdl "hitomi": Link.arc is mounted at boot (dScnLogo), before any activation, and
//     its model data lives until the game ends, so the texture is recoloured in place when the
//     player's heap is made (daPy_lk_c::createHeap, every stage load: the first one of an activation
//     does the work), with the disc's bytes kept in host memory and put back when pc_rando() turns
//     0. J3DTexture::resTIMGChanged rebuilds the texture objects so Aurora sees the new image and
//     palette (the size and palette length may change with the image, as gclib's replace_image does).
// The configuration is read at the first hook call after an activation, not in the listener: under
// COS_RANDO_FORCE the listener runs during static initialization, before the JKR heaps exist.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_player_colors.h"
#include "features/rando/pc_rando_jpc_scene.h"
#include "features/rando/pc_rando_player_colors.h"
#include "features/rando/pc_rando_seedfile.h"

#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTNameTab.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

PcRandoPlayerColors sCfg;
bool sDirty = true; // read the configuration at the next hook call
PcRandoPlayerColorsStatus sStatus;

// The disc's bytes of one recoloured texture.
struct Saved {
    ResTIMG* entry;
    uint8_t header[0x20];
    uint8_t* image;
    std::vector<uint8_t> imageBytes;
    uint8_t* palette;
    std::vector<uint8_t> paletteBytes;
};
J3DModelData* sModel = nullptr;           // cl.bdl's model data, once seen
std::vector<Saved>* sSaved = nullptr;     // host memory
bool sHitomiDone = false;                 // this activation's recolour was tried

void ensureConfig() {
    if (!sDirty) {
        return;
    }
    sDirty = false;
    JKRPcHostAllocScope scope;
    int casual = 0;
    const PcRandoSeed* seed = pc_rando_seed();
    const pcr::Json* cc = nullptr;
    if (seed != nullptr) {
        const pcr::Json* c = seed->options.find("player_in_casual_clothes");
        casual = c != nullptr && c->isBool() && c->asBool() ? 1 : 0;
        cc = seed->options.find("custom_colors");
    }
    std::string log;
    const char* env = getenv("COS_RANDO_CUSTOM_COLORS");
    int any;
    if (env != nullptr && env[0] != '\0') {
        any = pc_rando_player_colors_parse_text(env, casual, &sCfg, &log);
        logf("[cos] rando: player colours: COS_RANDO_CUSTOM_COLORS=%s\n", env);
    } else {
        any = pc_rando_player_colors_parse(cc, casual, &sCfg, &log);
    }
    size_t at = 0;
    while (at < log.size()) {
        const size_t nl = log.find('\n', at);
        const std::string line = log.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
        logf("[cos] rando: player colours: %s\n", line.c_str());
        at = nl == std::string::npos ? log.size() : nl + 1;
    }
    sStatus.active = 1;
    sStatus.any = any > 0 ? 1 : 0;
    if (any > 0) {
        for (int t = 0; t < PCR_TRAIL_COUNT; t++) {
            if (sCfg.hasTrail[t]) {
                logf("[cos] rando: player colours: %s = %02X%02X%02X%02X%s\n", pc_rando_trail_key(t),
                     sCfg.trail[t][0], sCfg.trail[t][1], sCfg.trail[t][2], sCfg.trail[t][3],
                     sCfg.trailHasAlpha[t] ? "" : " (vanilla alpha)");
            }
        }
        if (sCfg.hasEyes) {
            logf("[cos] rando: player colours: Eyes %02X%02X%02X -> %02X%02X%02X (%s)\n", sCfg.eyesBase[0],
                 sCfg.eyesBase[1], sCfg.eyesBase[2], sCfg.eyes[0], sCfg.eyes[1], sCfg.eyes[2],
                 sCfg.casual ? "casual" : "hero");
        }
    }
}

void editCommon(int, uint8_t* data, size_t size, void*) {
    ensureConfig(); // inside R7.1b's host allocation scope
    sStatus.jpcResult = pc_rando_player_colors_jpc(data, size, &sCfg);
    if (sStatus.jpcResult < 0) {
        logf("[cos] rando: player colours: common.jpc has no particle 0x48 with a BSP1; arrow trail vanilla\n");
    } else if (sStatus.jpcResult > 0) {
        logf("[cos] rando: player colours: arrow trail (common.jpc 0x48 BSP1 prm colour) set\n");
    }
}

// Puts the disc's bytes of every recoloured texture back.
void restoreHitomi() {
    if (sSaved == nullptr) {
        return;
    }
    JKRPcHostAllocScope scope;
    for (Saved& s : *sSaved) {
        memcpy(s.entry, s.header, sizeof s.header);
        memcpy(s.image, s.imageBytes.data(), s.imageBytes.size());
        if (s.palette != nullptr) {
            memcpy(s.palette, s.paletteBytes.data(), s.paletteBytes.size());
        }
        if (sModel != nullptr) {
            sModel->getTexture()->resTIMGChanged(s.entry);
        }
        sStatus.hitomiRestored++;
    }
    if (!sSaved->empty()) {
        logf("[cos] rando: player colours: %u hitomi texture(s) restored\n", (unsigned)sSaved->size());
    }
    delete sSaved;
    sSaved = nullptr;
}

void applyHitomi() {
    if (sHitomiDone || sModel == nullptr) {
        return;
    }
    sHitomiDone = true;
    ensureConfig();
    if (!sCfg.hasEyes) {
        return;
    }
    J3DTexture* tex = sModel->getTexture();
    JUTNameTab* names = sModel->getTextureName();
    if (tex == nullptr || names == nullptr) {
        sStatus.hitomiResult = PCR_TEX_BAD_SIZE;
        return;
    }
    JKRPcHostAllocScope scope;
    // customizer.py: the first "hitomi" texture rendered and recoloured, the result given to each.
    std::vector<uint8_t> image;
    int w = 0, h = 0;
    for (u16 i = 0; i < tex->getNum(); i++) {
        const char* name = names->getName(i);
        if (name == nullptr || strcmp(name, PCR_PLAYER_HITOMI) != 0) {
            continue;
        }
        ResTIMG* entry = tex->getResTIMG(i);
        PcRandoTexture t;
        int r = pc_rando_player_tex_open(&t, (uint8_t*)entry);
        if (r == PCR_TEX_OK && image.empty()) {
            w = t.width;
            h = t.height;
            image.resize((size_t)w * h * 4);
            r = pc_rando_player_tex_render(&t, image.data());
            if (r == PCR_TEX_OK) {
                pc_rando_tex_color_exchange_rgba(image.data(), (size_t)w * h, sCfg.eyesBase, sCfg.eyes, 1);
            } else {
                image.clear();
            }
        }
        if (r == PCR_TEX_OK && (image.empty() || t.width != w || t.height != h)) {
            r = PCR_TEX_BAD_SIZE;
        }
        if (r != PCR_TEX_OK) {
            sStatus.hitomiResult = r;
            logf("[cos] rando: player colours: hitomi texture %u: %s; pupils vanilla\n", (unsigned)i,
                 pc_rando_tex_result_name(r));
            continue;
        }
        Saved s;
        s.entry = entry;
        memcpy(s.header, entry, sizeof s.header);
        s.image = t.image;
        s.imageBytes.assign(t.image, t.image + t.imageSize);
        s.palette = t.palette;
        if (t.palette != nullptr) {
            s.paletteBytes.assign(t.palette, t.palette + (size_t)t.numColors * 2);
        }
        r = pc_rando_player_tex_replace(&t, image.data());
        sStatus.hitomiResult = r;
        if (r != PCR_TEX_OK) {
            logf("[cos] rando: player colours: hitomi texture %u: %s; pupils vanilla\n", (unsigned)i,
                 pc_rando_tex_result_name(r));
            continue;
        }
        // gclib's replace_image sets num_colors; the header is the game's ResTIMG (big-endian).
        uint8_t* hd = (uint8_t*)entry;
        hd[0x0A] = (uint8_t)(t.numColors >> 8);
        hd[0x0B] = (uint8_t)t.numColors;
        if (sSaved == nullptr) {
            sSaved = new std::vector<Saved>();
        }
        sSaved->push_back(std::move(s));
        tex->resTIMGChanged(entry);
        sStatus.hitomiApplied++;
        logf("[cos] rando: player colours: hitomi texture %u recoloured (%d colours)\n", (unsigned)i,
             t.numColors);
    }
}

void onRando(int active) {
    // No allocation here: under COS_RANDO_FORCE this runs during static initialization.
    restoreHitomi();
    sHitomiDone = false;
    sDirty = true;
    pc_rando_player_colors_clear(&sCfg);
    const uint32_t restored = sStatus.hitomiRestored;
    memset(&sStatus, 0, sizeof sStatus);
    sStatus.hitomiRestored = restored;
    sStatus.hitomiResult = -1;
    sStatus.jpcResult = -2;
    if (!active) {
        return;
    }
    // R7.1b runs it on common.jpc at once when the file is already loaded, else at createCommon.
    sStatus.jpcRegistered = pc_rando_jpc_scene_add_editor(PCR_JPC_SCENE_COMMON, editCommon, nullptr);
    if (!sStatus.jpcRegistered) {
        static const char kMsg[] = "[cos] rando: player colours: particle editor table full, arrow trail vanilla\n";
        (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
    }
    if (sModel != nullptr && pc_rando()) {
        applyHitomi();
    }
}

struct RegisterPlayerColors {
    RegisterPlayerColors() {
        sStatus.hitomiResult = -1;
        sStatus.jpcResult = -2;
        if (!pc_rando_add_listener(onRando)) {
            static const char kMsg[] = "[cos] rando: player colours: listener list full, colours stay vanilla\n";
            (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
        }
    }
} sRegisterPlayerColors;

} // namespace

extern "C" GXColor pc_rando_player_trail_color(int trail, GXColor vanilla) {
    ensureConfig();
    uint8_t rgba[4] = {vanilla.r, vanilla.g, vanilla.b, vanilla.a};
    pc_rando_player_trail(&sCfg, trail, rgba);
    const GXColor out = {rgba[0], rgba[1], rgba[2], rgba[3]};
    if (trail >= 0 && trail < 4) {
        sStatus.trailCalls[trail]++;
        sStatus.trailLast[trail] = out;
    }
    return out;
}

extern "C" void pc_rando_player_colors_cutf_brk(void* buffer, uint32_t cap) {
    ensureConfig();
    uint8_t* data = (uint8_t*)buffer;
    sStatus.brkCalls++;
    if (data == nullptr || cap < 0x20) {
        sStatus.brkWritten = -1;
        return;
    }
    uint32_t size = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | data[11];
    if (size < 0x20 || size > cap) {
        size = cap;
    }
    uint8_t before[0x400];
    const uint32_t keep = size < sizeof before ? size : (uint32_t)sizeof before;
    memcpy(before, data, keep);
    sStatus.brkWritten = pc_rando_player_colors_brk(data, size, &sCfg);
    int changed = 0;
    for (uint32_t i = 0; i < keep; i++) {
        changed += before[i] != data[i] ? 1 : 0;
    }
    sStatus.brkChangedBytes = changed;
    sStatus.brkLastSize = size;
    memcpy(sStatus.brkLast, data, size < sizeof sStatus.brkLast ? size : sizeof sStatus.brkLast);
    if (sStatus.brkWritten < 0) {
        logf("[cos] rando: player colours: the sword tip BRK is not the disc's; its colours stay vanilla\n");
    }
}

extern "C" void pc_rando_player_colors_cl_model(void* modelData) {
    J3DModelData* md = (J3DModelData*)modelData;
    if (md == nullptr) {
        return;
    }
    if (sModel != md) {
        // cl.bdl is resident: one model data for the whole game. A new one would hold the disc's
        // bytes; forget the old saves (they point into it).
        if (sSaved != nullptr) {
            JKRPcHostAllocScope scope;
            delete sSaved;
            sSaved = nullptr;
        }
        sModel = md;
        sHitomiDone = false;
    }
    applyHitomi();
}

extern "C" const PcRandoPlayerColorsStatus* pc_rando_player_colors_status(void) {
    return &sStatus;
}

const PcRandoPlayerColors* pc_rando_player_colors_config(void) {
    ensureConfig();
    return &sCfg;
}
