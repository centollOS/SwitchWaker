// The in-game options menu (native/include/pc/pc_settings.h; native/README.md, "Options menu").
//
// Opens with Minus on the Switch (any connected controller), L+R+Z on a GameCube-style pad or
// in a COS_INPUT script, or F1 on the Mac's keyboard; B (Esc), or the same combination, closes it.
// While it is open the game is paused (in the PLAY scene: the hit-stop pause timer of
// dScnPly_ply_c, which stops the actors, the camera, messages, the environment and scene changes)
// and gets no controller input (Aurora's PADBlockInput; the buttons still held when it closes are
// ignored until released). Other scenes (title, name entry) keep running behind it, without input.
//
// One ImGui window with six tabs, Gráficos, Controles, Rendimiento, Calidad de vida, Sistema and
// Depuración (in that order on screen, kTabOrder; the developer's tab last), navigated by the controller (D-pad or stick: row and value;
// A: change / confirm; B: back / close; L/R: tab), the keyboard (arrows, Enter, Esc, Q/E) or the mouse
// (click a tab, a row, a value). The menu reads the controllers itself (SDL gamepads, labels of the
// face buttons respected, so A is A on a Switch-style layout too), plus the COS_INPUT script in
// COS_SMOKE=options-menu (only there: other scripts press L+R+Z for the game); so the button remap of
// the Controles tab (pc_button_remap.h, issue #15) never changes the menu's own buttons.
//
// Every row is a setting of pc_settings.h (the environment variable it already had) or an action
// (warp, screenshot, reload). Settings with PC_SETTING_RESTART apply at the next start; the others
// apply at once through the setters of pc_gpu_opts.h, pc_dynres.h, pc_main.cpp (COS_FB_SCALE),
// pc_frame.cpp (COS_PERF_EVERY) and the Switch layer (COS_SWITCH_GPU_PROFILE). The graphics settings
// marked per mode keep one value for handheld and one for docked; the "Perfil a editar" row picks
// which one the rows show and change, and pc_settings_poll_mode applies the other mode's values when
// the console is docked or undocked.
//
// Updates (pc_update.h, issues #27/#28; docs/AUTO_UPDATE_PLAN.md): with Sistema > "Buscar actualizaciones al
// arrancar" on, the first frame asks GitHub for the latest release in the background. A newer one the
// player was not told to forget brings up a popup outside the PLAY scene (title, file select): update now,
// later, or not about this version (COS_UPDATE_SKIP, a setting without a row). Found during play, it is a
// short notice instead. The Sistema tab's "Actualización" row checks, installs and restarts. A short
// notice also says what the start found (the first start of an installed version, a rollback).
//
// COS_SMOKE=options-menu (with COS_INPUT and usually COS_BOOT_STAGE): drives the menu through the
// script and checks the "#expect" lines of the script file (see smokeParse below): the menu opened,
// the game stayed paused while it was open, the settings in effect, the lines of the settings file,
// the EFB size, the stage after a warp; exit 0 at the "#expect end" frame when all held.
#include "pc/pc_settings.h"
#include "pc/pc_dynres.h"
#include "pc/pc_gpu_opts.h"
#include "pc/pc_hd_textures.h"
#include "pc/game_hooks.h"

#include "pc_internal.h"

#include "pc/pc_button_remap.h"
#include "pc/pc_controls.h"
#include "pc/pc_qol.h"
#include "pc/pc_update.h"
#include "features/pc_button_remap_map.h"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include <math.h>
#include "d/d_s_play.h"
#include "d/d_save.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"
#include "m_Do/m_Do_audio.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_name.h"

#include <imgui.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#if defined(__SWITCH__)
#include "cos_switch.h"
#include "dk_aurora.h"  // aurora_switch_dk_set_fxaa
#endif

// Aurora's <dolphin/pad.h> (the game's own comes first on the include path).
extern "C" struct SDL_Gamepad* PADGetSDLGamepadForIndex(u32 index);
extern "C" void PADBlockInput(bool block);

namespace pc {

namespace {

// ---- input ---------------------------------------------------------------------------------------

enum Key : uint32_t {
    kUp = 1u << 0,
    kDown = 1u << 1,
    kLeft = 1u << 2,
    kRight = 1u << 3,
    kConfirm = 1u << 4,
    kBack = 1u << 5,
    kTabLeft = 1u << 6,
    kTabRight = 1u << 7,
    kCombo = 1u << 8, // open / close
};

// GameCube PAD_* bits (dolphin/pad.h) as the COS_INPUT script writes them.
constexpr uint16_t kPadLeft = 0x0001, kPadRight = 0x0002, kPadDown = 0x0004, kPadUp = 0x0008,
                   kPadZ = 0x0010, kPadR = 0x0020, kPadL = 0x0040, kPadA = 0x0100, kPadB = 0x0200;

uint32_t readScript() {
    uint16_t b = 0;
    int8_t sx = 0, sy = 0;
    if (!inputScriptAt(pc_frame_count(), &b, &sx, &sy)) {
        return 0;
    }
    uint32_t k = 0;
    if ((b & (kPadL | kPadR | kPadZ)) == (kPadL | kPadR | kPadZ)) {
        return kCombo;
    }
    k |= (b & kPadUp) || sy > 60 ? kUp : 0;
    k |= (b & kPadDown) || sy < -60 ? kDown : 0;
    k |= (b & kPadLeft) || sx < -60 ? kLeft : 0;
    k |= (b & kPadRight) || sx > 60 ? kRight : 0;
    k |= (b & kPadA) ? kConfirm : 0;
    k |= (b & kPadB) ? kBack : 0;
    k |= (b & kPadL) ? kTabLeft : 0;
    k |= (b & kPadR) ? kTabRight : 0;
    return k;
}

// The face button labelled `label` (A or B) on this controller: a Switch-style layout has A on the
// right (east), an Xbox layout at the bottom (south).
SDL_GamepadButton labelled(SDL_Gamepad* gp, SDL_GamepadButtonLabel label) {
#if defined(__SWITCH__)
    // Aurora's SDL 3 shim (switch/aurora/sdl3_shim) maps the Switch's A to south and B to east, and
    // has no button labels.
    (void)gp;
    return label == SDL_GAMEPAD_BUTTON_LABEL_A ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
#endif
    const SDL_GamepadButton buttons[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
                                         SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH};
    for (SDL_GamepadButton b : buttons) {
        if (SDL_GetGamepadButtonLabel(gp, b) == label) {
            return b;
        }
    }
    return label == SDL_GAMEPAD_BUTTON_LABEL_A ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
}

uint32_t readGamepads() {
    uint32_t k = 0;
    for (u32 port = 0; port < 4; port++) {
        SDL_Gamepad* gp = PADGetSDLGamepadForIndex(port);
        if (gp == nullptr) {
            continue;
        }
#if defined(__SWITCH__)
        // Minus alone: the SDL shim shows every controller as a Pro Controller, whose mapping gives
        // Minus (BACK) to no game button.
        bool combo = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_BACK);
#else
        const bool lt = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000;
        const bool rt = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000;
        // ZL+ZR+Minus: elsewhere BACK can be a game button (an NSO GameCube controller's Z).
        bool combo = lt && rt && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_BACK);
        // A GameCube controller through SDL: L and R are the triggers, Z the right shoulder.
        combo = combo || (lt && rt && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
#endif
        if (combo) {
            k |= kCombo;
            continue;
        }
        const int lx = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX);
        const int ly = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
        k |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) || ly < -20000 ? kUp : 0;
        k |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ly > 20000 ? kDown : 0;
        k |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || lx < -20000 ? kLeft : 0;
        k |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || lx > 20000 ? kRight : 0;
        k |= SDL_GetGamepadButton(gp, labelled(gp, SDL_GAMEPAD_BUTTON_LABEL_A)) ? kConfirm : 0;
        k |= SDL_GetGamepadButton(gp, labelled(gp, SDL_GAMEPAD_BUTTON_LABEL_B)) ? kBack : 0;
        k |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ? kTabLeft : 0;
        k |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? kTabRight : 0;
    }
    return k;
}

uint32_t readKeyboard() {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    if (keys == nullptr || count <= SDL_SCANCODE_F1) {
        return 0;
    }
    uint32_t k = 0;
    k |= keys[SDL_SCANCODE_F1] ? kCombo : 0;
    k |= keys[SDL_SCANCODE_UP] ? kUp : 0;
    k |= keys[SDL_SCANCODE_DOWN] ? kDown : 0;
    k |= keys[SDL_SCANCODE_LEFT] ? kLeft : 0;
    k |= keys[SDL_SCANCODE_RIGHT] ? kRight : 0;
    k |= keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE] ? kConfirm : 0;
    k |= keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE] ? kBack : 0;
    k |= keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_PAGEUP] ? kTabLeft : 0;
    k |= keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_PAGEDOWN] ? kTabRight : 0;
    return k;
}

// ---- warp destinations ---------------------------------------------------------------------------

struct Warp {
    const char* stage;
    int room;
    int point;
    const char* label; // nullptr: the stage name
    const char* labelEn;
};

// The main places, by their Spanish and English names (the sea's rooms are its 7x7 grid squares, A1 = 1).
const Warp kMainWarps[] = {
    {"sea", 44, 206, "Isla Initia (Outset)", "Outset Island"},
    {"LinkRM", 0, 0, "Casa del héroe", "The hero's house"},
    {"sea", 11, 0, "Isla Taura (Windfall)", "Windfall Island"},
    {"sea", 13, 0, "Isla del Dragón (Dragon Roost)", "Dragon Roost Island"},
    {"M_NewD2", 0, 0, "Caverna del Dragón", "Dragon Roost Cavern"},
    {"sea", 41, 0, "Isla del Bosque (Forest Haven)", "Forest Haven"},
    {"Omori", 0, 3, "Refugio del Bosque (interior)", "Forest Haven (inside)"},
    {"kindan", 0, 0, "Bosque Prohibido", "Forbidden Woods"},
    {"sea", 26, 0, "Torre de los Dioses (mar)", "Tower of the Gods (sea)"},
    {"Siren", 0, 0, "Torre de los Dioses", "Tower of the Gods"},
    {"sea", 1, 0, "Fortaleza Maldita (mar)", "Forsaken Fortress (sea)"},
    {"MajyuE", 0, 0, "Fortaleza Maldita", "Forsaken Fortress"},
    {"Hyrule", 0, 0, "Castillo sumergido", "Sunken castle"},
    {"sea", 45, 0, "Isla Lápida (Headstone)", "Headstone Island"},
    {"M_Dai", 0, 0, "Templo de la Tierra", "Earth Temple"},
    {"sea", 4, 0, "Isla del Vendaval (Gale)", "Gale Isle"},
    {"kaze", 15, 15, "Templo del Viento", "Wind Temple"},
};

// Every stage of the disc with the start native/tools/boot_sweep.py --list picks.
const Warp kAllWarps[] = {
#define W(s, r, p) {s, r, p, nullptr, nullptr},
    W("ADMumi", 0, 100)
    W("A_R00", 0, 0)
    W("A_nami", 0, 0)
    W("A_umikz", 0, 0)
    W("Abesso", 0, 0)
    W("Abship", 0, 0)
    W("Adanmae", 0, 0)
    W("Amos_T", 0, 0)
    W("Asoko", 0, 0)
    W("Atorizk", 0, 0)
    W("Cave01", 0, 0)
    W("Cave02", 0, 0)
    W("Cave03", 0, 0)
    W("Cave04", 0, 0)
    W("Cave05", 0, 0)
    W("Cave06", 0, 0)
    W("Cave07", 0, 0)
    W("Cave08", 0, 0)
    W("Cave09", 0, 0)
    W("Cave10", 1, 0)
    W("Cave11", 1, 0)
    W("Comori", 0, 0)
    W("DmSpot0", 0, 0)
    W("E3ROOP", 0, 0)
    W("ENDumi", 0, 0)
    W("Ebesso", 0, 0)
    W("Edaichi", 0, 0)
    W("Ekaze", 0, 0)
    W("Fairy01", 0, 0)
    W("Fairy02", 0, 0)
    W("Fairy03", 0, 0)
    W("Fairy04", 0, 0)
    W("Fairy05", 0, 0)
    W("Fairy06", 0, 0)
    W("GTower", 0, 0)
    W("GanonA", 0, 0)
    W("GanonB", 0, 0)
    W("GanonC", 0, 0)
    W("GanonD", 0, 0)
    W("GanonE", 0, 0)
    W("GanonJ", 1, 0)
    W("GanonL", 0, 0)
    W("GanonM", 0, 0)
    W("GanonN", 0, 0)
    W("H_test", 0, 0)
    W("Hyroom", 0, 0)
    W("Hyrule", 0, 0)
    W("ITest61", 0, 0)
    W("ITest62", 0, 0)
    W("ITest63", 0, 0)
    W("I_SubAN", 9, 0)
    W("I_TestM", 0, 0)
    W("I_TestR", 0, 0)
    W("KATA_HB", 0, 0)
    W("KATA_RM", 18, 1)
    W("K_Test2", 0, 0)
    W("K_Test3", 0, 0)
    W("K_Test4", 0, 0)
    W("K_Test5", 0, 0)
    W("K_Test6", 0, 0)
    W("K_Test8", 0, 0)
    W("K_Test9", 0, 0)
    W("K_Testa", 0, 0)
    W("K_Testb", 0, 0)
    W("K_Testc", 0, 0)
    W("K_Testd", 0, 0)
    W("K_Teste", 0, 0)
    W("Kaisen", 0, 0)
    W("LinkRM", 0, 1)
    W("LinkUG", 0, 1)
    W("M2ganon", 0, 0)
    W("M2tower", 0, 16)
    W("M_Dai", 0, 0)
    W("M_DaiB", 0, 0)
    W("M_DaiMB", 12, 0)
    W("M_Dra09", 9, 0)
    W("M_DragB", 0, 0)
    W("M_NewD2", 0, 0)
    W("MajyuE", 0, 0)
    W("MiniHyo", 0, 0)
    W("MiniKaz", 0, 0)
    W("Mjtower", 0, 16)
    W("Msmoke", 0, 0)
    W("Mukao", 0, 0)
    W("Nitiyou", 0, 0)
    W("Obombh", 0, 0)
    W("Obshop", 1, 0)
    W("Ocean", 0, 0)
    W("Ocmera", 0, 0)
    W("Ocrogh", 0, 0)
    W("Ojhous", 0, 0)
    W("Ojhous2", 1, 0)
    W("Omasao", 0, 0)
    W("Omori", 0, 0)
    W("Onobuta", 0, 0)
    W("Opub", 0, 0)
    W("Orichh", 0, 0)
    W("Otkura", 0, 0)
    W("PShip", 0, 0)
    W("PShip2", 0, 0)
    W("PShip3", 0, 0)
    W("Pdrgsh", 0, 0)
    W("Pfigure", 0, 0)
    W("Pjavdou", 0, 0)
    W("Pnezumi", 0, 0)
    W("ShipD", 0, 0)
    W("Siren", 0, 0)
    W("SirenB", 0, 0)
    W("SirenMB", 23, 0)
    W("SubD42", 0, 0)
    W("SubD43", 0, 0)
    W("SubD44", 0, 0)
    W("SubD45", 0, 0)
    W("SubD51", 0, 0)
    W("SubD71", 0, 0)
    W("TEST", 0, 0)
    W("TF_01", 0, 0)
    W("TF_02", 0, 0)
    W("TF_03", 0, 0)
    W("TF_04", 0, 0)
    W("TF_05", 0, 0)
    W("TF_06", 0, 0)
    W("TF_07", 1, 0)
    W("TyuTyu", 0, 0)
    W("VrTest", 0, 0)
    W("WarpD", 0, 0)
    W("Xboss0", 0, 0)
    W("Xboss1", 0, 0)
    W("Xboss2", 0, 0)
    W("Xboss3", 0, 0)
    W("figureA", 0, 0)
    W("figureB", 0, 0)
    W("figureC", 0, 0)
    W("figureD", 0, 0)
    W("figureE", 0, 0)
    W("figureF", 0, 0)
    W("figureG", 0, 0)
    W("kazan", 0, 0)
    W("kaze", 15, 15)
    W("kazeB", 0, 0)
    W("kazeMB", 6, 0)
    W("kenroom", 0, 0)
    W("kinBOSS", 0, 0)
    W("kinMB", 10, 0)
    W("kindan", 0, 0)
    W("ma2room", 0, 0)
    W("ma3room", 0, 0)
    W("majroom", 0, 0)
    W("morocam", 0, 0)
    W("sea", 1, 0)
    W("sea_E", 0, 0)
    W("sea_T", 44, 0)
    W("tincle", 0, 0)
#undef W
};

constexpr int kMainWarpCount = (int)(sizeof(kMainWarps) / sizeof(kMainWarps[0]));
constexpr int kAllWarpCount = (int)(sizeof(kAllWarps) / sizeof(kAllWarps[0]));

const Warp& warpAt(int i) {
    return i < kMainWarpCount ? kMainWarps[i] : kAllWarps[i - kMainWarpCount];
}

// ---- built-in settings ---------------------------------------------------------------------------

#if defined(__SWITCH__)
constexpr bool kSwitch = true;
#else
constexpr bool kSwitch = false;
#endif

// The menu's language (pc_ui_spanish, as the shader loading screen): the Spanish text or the English one.
const char* T(const char* es, const char* en) {
    return pc_ui_spanish() || en == nullptr ? es : en;
}
std::string T(const std::string& es, const std::string& en) {
    return pc_ui_spanish() ? es : en;
}
const char* settingLabel(const PcSettingDesc* d) { return T(d->label, d->labelEn); }
const char* settingHelp(const PcSettingDesc* d) { return T(d->help, d->helpEn); }
const char* modeName(PcOperationMode mode) {
    return T(pc_settings_mode_name(mode), mode == PC_MODE_DOCKED ? "Docked" : "Handheld");
}

void applyFbScale(const char*, const char* v, void*) {
    const float scale = (float)atof(v);
    setFrameBufferScale(scale);
    pc_dynres_configure(pc_settings_get("COS_DYNRES"), scale);
    pc_fps60_hold("internal resolution changed"); // 60 fps step E: the EFB is resized
}
void applyDynres(const char*, const char* v, void*) {
    pc_dynres_configure(v, (float)atof(pc_settings_get("COS_FB_SCALE")));
    pc_fps60_hold("dynamic resolution changed");
}
void applyDof(const char*, const char* v, void*) { pc_dof_set(strcmp(v, "0") != 0); }
void applyAniso(const char*, const char* v, void*) { pc_aniso_set(atoi(v)); }
void applyCameraInvertX(const char*, const char* v, void*) { pc_camera_invert_x_set(strcmp(v, "1") == 0); }
void applyCameraInvertY(const char*, const char* v, void*) { pc_camera_invert_y_set(strcmp(v, "1") == 0); }
void applyCameraShake(const char*, const char* v, void*) { pc_camera_shake_set(strcmp(v, "1") == 0); }
void applyHeatHaze(const char*, const char* v, void*) { pc_heat_haze_set(strcmp(v, "1") == 0); }
void applyQol(const char* key, const char* v, void*) { pc_qol_set(key, v); }
void applyGpuProfile(const char*, const char* v, void*) {
#if defined(__SWITCH__)
    cos_switch_set_gpu_profile(v);
#else
    (void)v;
#endif
}
void applyFxaa(const char*, const char* v, void*) {
#if defined(__SWITCH__)
    aurora_switch_dk_set_fxaa(strcmp(v, "1") == 0);
#else
    (void)v;
#endif
}
void applyOverlay(const char*, const char* v, void*) { gConfig.fpsOverlay = strcmp(v, "1") == 0; }
void applyOverlayDetail(const char*, const char* v, void*) {
    gConfig.fpsOverlayCompact = strcmp(v, "compact") == 0;
}
void applyPerfEvery(const char*, const char* v, void*) { perfSetEvery((unsigned int)atoi(v)); }
void applyPerfLog(const char*, const char* v, void*) { gConfig.perfLog = strcmp(v, "0") != 0; }
void applyHitch(const char*, const char* v, void*) { perfSetHitch((unsigned int)atoi(v)); }
void applyHdStats(const char*, const char* v, void*) { pc_hd_textures_set_stats_every((unsigned int)atoi(v)); }
void applyGpuGroups(const char*, const char* v, void*) { pc_gpu_groups_set(atoi(v)); }
// Gráficos > Gamma (COS_GAMMA): the points added to 2.2 (the game's colours are encoded for a GameCube TV,
// about 2.4, and the Switch's panel shows them too bright); the present draws colour^((2.2 + change) / 2.2), see
// switch/deko/shaders/present_gamma.wgsl. Only the deko3d NRO has the present pass.
void applyGamma(const char*, const char* v, void*) {
#if defined(__SWITCH__)
    cos_switch_set_gamma((2.2f + (float)atof(v)) / 2.2f);
#else
    (void)v;
#endif
}
void applyFps60(const char*, const char* v, void*) { pc_fps60_set(strcmp(v, "1") == 0); }

#define CHOICES(name) name, (int)(sizeof(name) / sizeof(name[0]))

const PcSettingChoice kFbScale[] = {
#if !defined(__SWITCH__)
    {"0", "Tamaño de la ventana", "Window size"},
#endif
    {"1.0", "854x480 (1.0)"},  {"1.125", "960x540 (1.125)"}, {"1.25", "1067x600 (1.25)"},
    {"1.5", "1280x720 (1.5)"}, {"2.0", "1707x960 (2.0)"},    {"2.25", "1920x1080 (2.25)"},
};
const PcSettingChoice kAspect[] = {{"16:9", "16:9 (panorámica)", "16:9 (widescreen)"}, {"4:3", "4:3 (GameCube)"}, {"16:10", "16:10"}};
const PcSettingChoice kOnOff[] = {{"0", "Desactivado", "Off"}, {"1", "Activado", "On"}};
const PcSettingChoice kAniso[] = {{"1", "x1 (desactivado)", "x1 (off)"}, {"2", "x2"}, {"4", "x4"}, {"8", "x8"}, {"16", "x16"}};
const PcSettingChoice kHdMaxSize[] = {{"auto", "Automático (512 / 1024)", "Automatic (512 / 1024)"}, {"256", "256"}, {"512", "512"},
                                      {"1024", "1024"}, {"full", "Sin límite", "No limit"}};
// "auto" (the default): on while the 60 fps row is on (docs/FPS60_PLAN.md step E), off at 30 fps;
// "0" keeps it off even with 60 fps (the user's choice is respected); "1" on at 30 fps too.
const PcSettingChoice kDynres[] = {{"auto", "Con 60 fps", "With 60 fps"},
                                   {"0", "Desactivada", "Off"},
                                   {"1", "Siempre", "Always"}};
const PcSettingChoice kGpuProfile[] = {
    {"460", "460,8 MHz", "460.8 MHz"}, {"384", "384 MHz"}, {"default", "Del sistema (307,2 MHz)", "System default (307.2 MHz)"}};
const PcSettingChoice kDetail[] = {{"full", "Completo", "Full"}, {"compact", "Compacto", "Compact"}};
const PcSettingChoice kPrecompile[] = {
    {"boot", "Arranque (boot)", "Boot (boot)"}, {"full", "Completo (full)", "Full (full)"}, {"all", "Todos, sin pantalla (all)", "All, no screen (all)"}, {"off", "Desactivado", "Off"}};
const PcSettingChoice kPrecompileScreen[] = {
    {"auto", "Automática", "Automatic"}, {"priority", "Solo prioritarios", "Priority only"},
    {"always", "Siempre", "Always"}, {"never", "Nunca", "Never"}};
const PcSettingChoice kPerfEvery[] = {
    {"0", "Desactivado", "Off"}, {"30", "Cada 30 cuadros", "Every 30 frames"},
    {"60", "Cada 60 cuadros", "Every 60 frames"}, {"120", "Cada 120 cuadros", "Every 120 frames"},
    {"300", "Cada 300 cuadros", "Every 300 frames"}};
const PcSettingChoice kGpuGroups[] = {{"0", "Desactivados", "Off"}, {"1", "Por grupo", "Per group"},
                                      {"2", "Por material J3D", "Per J3D material"}};
const PcSettingChoice kShowHide[] = {{"1", "Mostrar", "Show"}, {"0", "Ocultar", "Hide"}};
const PcSettingChoice kGamma[] = {{"0", "Original (sin cambio)", "Original (no change)"},
                                  {"0.10", "+0,10"}, {"0.20", "+0,20 (recomendado)", "+0.20 (recommended)"},
                                  {"0.30", "+0,30"}, {"0.40", "+0,40"}};
const PcSettingChoice kHitch[] = {{"0", "Desactivadas", "Off"}, {"33", "Más de 33 ms", "Over 33 ms"},
                                  {"40", "Más de 40 ms", "Over 40 ms"}, {"50", "Más de 50 ms", "Over 50 ms"},
                                  {"100", "Más de 100 ms", "Over 100 ms"}};
// Calidad de vida > Vela (COS_QOL_SAIL, pc_qol.h)
const PcSettingChoice kQolSail[] = {{"0", "Original", "Original"},
                                    {"swift", "Vela rápida (Swift Sail)", "Swift Sail"},
                                    {"brisk", "Vela ligera (Brisk Sail)", "Brisk Sail"},
                                    {"wind", "Vela normal con viento a favor", "Normal sail, wind at your back"}};
const PcSettingChoice kHdStats[] = {{"0", "Desactivadas", "Off"}, {"150", "Cada 150 cuadros", "Every 150 frames"},
                                    {"300", "Cada 300 cuadros", "Every 300 frames"},
                                    {"600", "Cada 600 cuadros", "Every 600 frames"}};

const PcSettingDesc kBuiltins[] = {
    // Gráficos
    {"COS_FB_SCALE", "Resolución interna",
     "Resolución a la que se dibuja el juego antes de escalar a la pantalla. Más baja = más rápido.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kFbScale), kSwitch ? "1.5" : "0", applyFbScale,
     nullptr, 10,
     "Internal resolution",
     "Resolution the game is drawn at before scaling to the screen. Lower = faster."},
    {"COS_DYNRES", "Resolución dinámica",
     "Baja la resolución del 3D (hasta 2/3) cuando la GPU no llega; el HUD queda nítido. Con 60 fps: solo "
     "mientras los 60 fps están activados.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kDynres), "auto", applyDynres, nullptr, 20,
     "Dynamic resolution",
     "Lowers the 3D resolution (down to 2/3) when the GPU cannot keep up; the HUD stays sharp. With 60 fps: "
     "only while 60 fps is on."},
    {"COS_DOF", "Profundidad de campo", "Desenfoque del paisaje lejano, como en la GameCube.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "1", applyDof, nullptr, 50,
     "Depth of field",
     "Blur of the distant scenery, as on the GameCube."},
    {"COS_FXAA", "Suavizado de bordes (FXAA)",
     "Suaviza los bordes dentados de la imagen final con un filtro FXAA. Cuesta algo de GPU.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_SWITCH_ONLY, CHOICES(kOnOff), "0", applyFxaa, nullptr, 60,
     "Edge smoothing (FXAA)",
     "Smooths the jagged edges of the final picture with an FXAA filter. Costs some GPU time."},
    {"COS_ANISO", "Filtrado anisotrópico",
     "Texturas del suelo y de los edificios más nítidas en perspectiva. Más alto = más carga para la GPU.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kAniso), "1", applyAniso, nullptr, 61,
     "Anisotropic filtering",
     "Sharper ground and building textures at a slant. Higher = more GPU load."},
    {"COS_GAMMA", "Gamma",
     "Oscurece o deja como estaba la imagen. El juego se pensó para una tele de GameCube (gamma ~2,4) y la "
     "pantalla de la Switch (~2,2) la muestra más clara y lavada: +0,20 la corrige. No cambia los menús.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_SWITCH_ONLY, CHOICES(kGamma), "0.20", applyGamma, nullptr, 60,
     "Gamma",
     "Darkens the picture or leaves it as it was. The game was made for a GameCube TV (gamma ~2.4) and "
     "the Switch's panel (~2.2) shows it brighter and washed out: +0.20 corrects it. The menus do not change."},
    {"COS_CAMERA_INVERT_X", "Invertir cámara horizontal",
     "El stick C gira la cámara al revés en horizontal.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "0", applyCameraInvertX, nullptr, 90,
     "Invert camera horizontally",
     "The C stick turns the camera the other way horizontally."},
    {"COS_CAMERA_INVERT_Y", "Invertir cámara vertical",
     "El stick C inclina la cámara al revés en vertical.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "0", applyCameraInvertY, nullptr, 91,
     "Invert camera vertically",
     "The C stick tilts the camera the other way vertically."},
    {"COS_CAMERA_SHAKE", "Temblor de cámara",
     "La cámara tiembla con terremotos, golpes, explosiones y cinemáticas, como en la GameCube. La vibración "
     "del mando no cambia.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "0", applyCameraShake, nullptr, 92,
     "Camera shake",
     "The camera shakes with quakes, hits, explosions and cutscenes, as on the GameCube. The controller "
     "rumble does not change."},
    {"COS_HEAT_HAZE", "Distorsión por calor",
     "La imagen ondula sobre la lava y el fuego (Montaña del Dragón y su caverna), como en la GameCube. "
     "Con 60 fps se ve mal: distorsiona demasiado.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "0", applyHeatHaze, nullptr, 93,
     "Heat haze",
     "The picture wobbles over lava and fire (Dragon Roost and its cavern), as on the GameCube. "
     "At 60 fps it looks wrong: it distorts too much."},
    {"COS_ASPECT", "Relación de aspecto", "Imagen panorámica 16:9 o la 4:3 original de la GameCube.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_RESTART, CHOICES(kAspect), kSwitch ? "16:9" : "4:3", nullptr, nullptr,
     70,
     "Aspect ratio",
     "16:9 widescreen or the GameCube's original 4:3."},
    {"COS_HD_TEXTURES", "Texturas HD", "Usa el paquete de texturas en alta resolución si está instalado.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "0", nullptr, nullptr, 80,
     "HD textures",
     "Uses the high-resolution texture pack if it is installed."},
    {"COS_HD_MAX_SIZE", "Tamaño máx. texturas HD",
     "Lado mayor de las texturas HD. Automático: 512 en portátil, 1024 en sobremesa. Se cambian poco a poco.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kHdMaxSize), "auto", nullptr, nullptr, 81,
     "HD texture max size",
     "Longest side of the HD textures. Automatic: 512 handheld, 1024 docked. They change "
     "gradually."},
    // Rendimiento
    {"COS_SWITCH_GPU_PROFILE", "Perfil de GPU (portátil)",
     "Reloj de la GPU en modo portátil (perfiles oficiales de la consola; la CPU sigue a 1020 MHz).",
     PC_SETTING_TAB_PERFORMANCE, PC_SETTING_SWITCH_ONLY, CHOICES(kGpuProfile), "460", applyGpuProfile, nullptr, 10,
     "GPU profile (handheld)",
     "GPU clock in handheld mode (the console's official profiles; the CPU stays at 1020 MHz)."},
    {"COS_FPS_OVERLAY", "Contador de FPS", "Panel de cuadros por segundo y tiempos en la esquina.",
     PC_SETTING_TAB_PERFORMANCE, 0, CHOICES(kOnOff), "0", applyOverlay, nullptr, 20,
     "FPS counter",
     "Frames per second and timings panel in the corner."},
    {"COS_FPS_OVERLAY_DETAIL", "Detalle del contador",
     "Completo: tiempos de render, draws y GPU. Compacto: solo FPS y tiempo del juego.",
     PC_SETTING_TAB_PERFORMANCE, 0, CHOICES(kDetail), "full", applyOverlayDetail, nullptr, 30,
     "Counter detail",
     "Full: render, draw and GPU timings. Compact: only FPS and game time."},
    {"COS_PRECOMPILE", "Precarga de shaders",
     "Qué shaders se compilan al arrancar: arranque (los del inicio y el resto detrás), completo, todos o ninguno.",
     PC_SETTING_TAB_PERFORMANCE, PC_SETTING_RESTART, CHOICES(kPrecompile), kSwitch ? "boot" : "off", nullptr,
     nullptr, 40,
     "Shader preloading",
     "Which shaders are compiled at start: boot (the start-up ones, the rest behind), full, all or "
     "none."},
    {"COS_PRECOMPILE_SCREEN", "Pantalla de carga de shaders",
     "Automática: solo con la caché fría (primer arranque); prioritarios, siempre o nunca.",
     PC_SETTING_TAB_PERFORMANCE, PC_SETTING_RESTART, CHOICES(kPrecompileScreen), "auto", nullptr, nullptr, 50,
     "Shader loading screen",
     "Automatic: only with a cold cache (first start); priority only, always or never."},
    {"COS_FPS60", "60 fps (interpolación)",
     "60 imágenes por segundo interpolando entre cuadros; el juego sigue a 30. Más CPU y batería. Si no "
     "llega, baja la resolución (Resolución dinámica) o vuelve a 30 un rato.",
     PC_SETTING_TAB_PERFORMANCE, PC_SETTING_PER_MODE, CHOICES(kOnOff), "0", applyFps60, nullptr, 60,
     "60 fps (interpolation)",
     "60 images a second by interpolating between frames; the game still runs at 30. More CPU and "
     "battery. When it cannot keep up it lowers the resolution (Dynamic resolution) or drops to 30 for "
     "a while."},
    // Calidad de vida (pc_qol.h, docs/QOL_PLAN.md: BetterWW's and wwrando's patches; all off by default)
    {"COS_QOL_INSTANT_TEXT", "Texto instantáneo",
     "Los textos aparecen enteros y sin pausas; mantener B pasa los mensajes (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 10,
     "Instant text",
     "Text boxes appear whole and without pauses; holding B skips the messages (BetterWW)."},
    {"COS_QOL_FAST_ROLL", "Voltereta más rápida",
     "La voltereta sale siempre a casi toda velocidad, también desde parado (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 20,
     "Faster roll",
     "The roll always starts at nearly full speed, also from standing (BetterWW)."},
    {"COS_QOL_FAST_BLOCKS", "Empujar bloques más rápido",
     "Los bloques se empujan y arrastran más deprisa (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 30,
     "Faster block moving",
     "Blocks are pushed and pulled faster (BetterWW)."},
    {"COS_QOL_FAST_GRAPPLE", "Garfio más rápido",
     "El garfio sale, se enrolla y se suelta más deprisa, como en Wind Waker HD (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 40,
     "Faster grappling hook",
     "The grappling hook flies, wraps and lets go faster, as in Wind Waker HD (BetterWW)."},
    {"COS_QOL_FAST_CLIMB", "Gatear y trepar más rápido",
     "Más deprisa al gatear, trepar, subir escaleras y enredaderas y pegarse a la pared; la cámara llega "
     "antes al hablar (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 50,
     "Faster crawling and climbing",
     "Faster crawling, climbing, ladders and vines and sidling; the camera gets there sooner when "
     "talking (BetterWW)."},
    {"COS_QOL_FAST_BALLAD", "Balada del vendaval rápida",
     "El viaje con la Balada del vendaval empieza en cuanto aparece el tornado, sin la subida del barco "
     "(BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 60,
     "Faster Ballad of Gales",
     "The Ballad of Gales warp starts as soon as the cyclone appears, without the boat's rise "
     "(BetterWW)."},
    {"COS_QOL_SKIP_INTRO", "Saltar la introducción",
     "Una partida nueva empieza en Isla Initia, sin la leyenda del principio (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 75,
     "Skip the intro",
     "A new game starts on Outset Island, without the opening legend (BetterWW)."},
    {"COS_QOL_REVEAL_CHART", "Carta náutica completa",
     "La carta náutica muestra todo el mar. No cambia la partida guardada: al desactivarlo vuelve a "
     "mostrar lo explorado (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 80,
     "Full sea chart",
     "The sea chart shows the whole sea. The save does not change: turned off, it shows what was "
     "explored again (BetterWW)."},
    {"COS_QOL_TINGLE_CHESTS", "Cofres de Tingle sin Tingle Tuner",
     "Los cofres de Tingle aparecen con cualquier bomba, y con la brújula salen en el mapa (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 90,
     "Tingle chests without Tingle Tuner",
     "Tingle chests appear with any bomb, and with the compass they show on the map (BetterWW)."},
    {"COS_QOL_SAIL", "Vela",
     "Rápida: el viento sopla siempre a favor y el barco va al doble, como en Wind Waker HD. Ligera: igual "
     "y frena antes. Vela normal con viento a favor: solo el viento (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kQolSail), "0", applyQol, nullptr, 100,
     "Sail",
     "Swift: the wind always blows from behind and the boat goes twice as fast, as in Wind Waker HD. "
     "Brisk: the same, and it stops sooner. Normal sail, wind at your back: only the wind (BetterWW)."},
    {"COS_QOL_FREE_BOAT", "Barco sin restricciones",
     "El Mascarón Rojo no te hace volver por no haber avanzado en la historia (solo en el borde del mar) y "
     "deja subir aunque el viento no sople a favor. Puedes llegar antes de tiempo a sitios de la historia "
     "(BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 105,
     "Unrestricted boat",
     "The King of Red Lions does not turn you back for not having got far enough in the story (only at "
     "the edge of the sea) and lets you board whatever the wind. You can reach story places early "
     "(BetterWW)."},
    {"COS_QOL_SWING_TURN", "Girar al balancearse",
     "El stick gira a Link mientras se balancea en una cuerda o con el garfio (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 110,
     "Turn while swinging",
     "The stick turns Link while he swings on a rope or the grappling hook (BetterWW)."},
    {"COS_QOL_NO_SONG_REPLAY", "Canciones sin repetición",
     "Tras tocar una canción con la batuta, Link no la vuelve a dirigir (BetterWW).",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 120,
     "No song replays",
     "After a song is played with the baton, Link does not conduct it again (BetterWW)."},
    {"COS_QOL_FIX_BUGS", "Arreglar fallos del juego original",
     "Arregla fallos del juego original que BetterWW corrige: la abuela sin botella vacía, Beedle comprando "
     "Jalea azul, la Cámara Plus en el botón, Morths fuera del mapa, enemigos muertos con flechas de luz "
     "(Stalfos, Minimoblin, Poes de Jalhalla, Magtail) y el disparador de Ganon Fantasma.",
     PC_SETTING_TAB_QOL, 0, CHOICES(kOnOff), "0", applyQol, nullptr, 130,
     "Fix bugs of the original game",
     "Fixes bugs of the original game that BetterWW fixes: the grandmother without an empty bottle, "
     "Beedle buying Blue Chu Jelly, the Deluxe Picto Box on a button, Morths out of the map, enemies "
     "killed with Light Arrows (Stalfos, Miniblins, Jalhalla's Poes, Magtails) and Phantom Ganon's "
     "trigger."},
    // Depuración
    {"COS_PERF_EVERY", "Intervalo del registro perf",
     "Cada cuántos cuadros se escribe una línea [cos] perf en el registro (native/logs/).",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kPerfEvery), "0", applyPerfEvery, nullptr, 105,
     "Perf log interval",
     "How many frames between [cos] perf lines in the log (native/logs/)."},
    {"COS_HITCH_MS", "Líneas de tirones (hitch)",
     "Escribe una línea [cos] hitch en el registro por cada cuadro del juego que tarde más que esto.",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kHitch), "0", applyHitch, nullptr, 107,
     "Hitch lines",
     "Writes a [cos] hitch line to the log for every game frame that takes longer than this."},
    {"COS_GPU_GROUPS", "Temporizadores de GPU por grupo",
     "Mide la GPU por grupo de dibujo (cielo, fondo, opacos, partículas...) en las líneas perf-switch.",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kGpuGroups), "0", applyGpuGroups, nullptr, 100,
     "GPU timers per group",
     "Times the GPU per draw group (sky, background, opaque, particles...) in the perf-switch "
     "lines."},
    {"COS_PERF_LOG", "Líneas perf en el registro",
     "Oculta o muestra las líneas [cos] perf y perf-switch (las mediciones siguen).",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kShowHide), "1", applyPerfLog, nullptr, 110,
     "Perf lines in the log",
     "Hides or shows the [cos] perf and perf-switch lines (measuring goes on)."},
    {"COS_HD_STATS_EVERY", "Estadísticas de texturas HD",
     "Cada cuántos cuadros se escribe una línea [cos] hd-textures (aciertos, cargas, MiB) con las texturas HD activas.",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kHdStats), kSwitch ? "0" : "300", applyHdStats, nullptr, 112,
     "HD texture stats",
     "How many frames between [cos] hd-textures lines (hits, loads, MiB) while HD textures are on."},
    {"COS_USB_LOG", "Registro en directo por USB",
     "Para desarrollo: envía el registro por USB a scripts/switch/usb_log.py mientras juegas. Ocupa el "
     "puerto USB (por ejemplo, SysDVR por USB no podrá usarlo); el registro en la tarjeta SD se escribe siempre.",
     PC_SETTING_TAB_DEBUG, PC_SETTING_RESTART | PC_SETTING_SWITCH_ONLY, CHOICES(kOnOff), "0", nullptr, nullptr, 120,
     "Live log over USB",
     "For development: sends the log over USB to scripts/switch/usb_log.py while you play. It holds "
     "the USB port (SysDVR over USB, for example, cannot use it); the log on the SD card is always written."},
    {"COS_DEBUG_SERVER", "Servidor de depuración (red)",
     "Solo para desarrollo: abre un puerto en la red local, sin contraseña, para scripts/switch/"
     "switchwaker_debug.py (registro, capturas, warps, archivos). Ver docs/DEBUG_SERVER.md.",
     PC_SETTING_TAB_DEBUG, PC_SETTING_RESTART | PC_SETTING_SWITCH_ONLY, CHOICES(kOnOff), "0", nullptr, nullptr, 125,
     "Debug server (network)",
     "For development only: opens a port on the local network, without a password, for scripts/switch/"
     "switchwaker_debug.py (log, screenshots, warps, files). See docs/DEBUG_SERVER.md."},
    // Sistema (pc_update.h, issues #27/#28)
    {"COS_UPDATE_CHECK", "Buscar actualizaciones al arrancar",
     "Al arrancar pregunta a GitHub si hay una versión nueva de SwitchWaker y avisa (necesita internet). No "
     "retrasa el arranque.",
     PC_SETTING_TAB_SYSTEM, PC_SETTING_SWITCH_ONLY, CHOICES(kOnOff), "1", nullptr, nullptr, 10,
     "Check for updates at start",
     "At start, asks GitHub whether there is a new SwitchWaker version and says so (needs the internet). It "
     "does not delay the start."},
    {"COS_UPDATE_SKIP", "Versión que no avisar", "La versión nueva de la que el jugador no quiere aviso al arrancar.",
     PC_SETTING_TAB_SYSTEM, PC_SETTING_HIDDEN, nullptr, 0, "", nullptr, nullptr, 900,
     "Version not to announce", "The new version the player does not want to be told about at start."},
};

// ---- menu state ----------------------------------------------------------------------------------

// Remap: a button remap action (PC_SETTING_REMAP, pc_button_remap.h), whose confirm opens the capture;
// RemapRestore: "Restaurar controles por defecto", the Controles tab's last row.
enum class RowKind {
    EditMode,
    Setting,
    Warp,
    Sailing,
    Screenshot,
    Reload,
    QolBetterWW,
    QolOriginal,
    Remap,
    RemapRestore,
    Update,
    Notes
};

struct Row {
    RowKind kind;
    const PcSettingDesc* desc = nullptr;
    int action = -1; // RowKind::Remap: the pc::remap::Action of desc
};

// Indexed by PcSettingTab; kTabOrder is the order on screen (and of L/R).
const char* const kTabNames[PC_SETTING_TABS] = {"Gráficos", "Rendimiento", "Calidad de vida", "Depuración",
                                                "Controles", "Sistema"};
const char* const kTabNamesEn[PC_SETTING_TABS] = {"Graphics", "Performance", "Quality of life", "Debug",
                                                  "Controls", "System"};
const int kTabOrder[PC_SETTING_TABS] = {PC_SETTING_TAB_GRAPHICS, PC_SETTING_TAB_CONTROLS, PC_SETTING_TAB_PERFORMANCE,
                                        PC_SETTING_TAB_QOL, PC_SETTING_TAB_SYSTEM, PC_SETTING_TAB_DEBUG};

// The position on screen of a tab.
int tabPosition(int tab) {
    for (int i = 0; i < PC_SETTING_TABS; i++) {
        if (kTabOrder[i] == tab) {
            return i;
        }
    }
    return 0;
}

struct Menu {
    bool initialized = false;
    bool open = false;
    bool warpPage = false;
    int tab = 0;
    int row[PC_SETTING_TABS] = {};
    int warpRow = 0;
    bool scrollToRow = false;
    PcOperationMode editMode = PC_MODE_HANDHELD;
    uint32_t prevKeys = 0;
    uint32_t repeatKey = 0;
    unsigned int repeatFrames = 0;
    bool waitRelease = false;
    unsigned int opened = 0;
    bool pendingShot = false;
    std::string toast;
    unsigned int toastUntil = 0;
    // The Controles tab's capture (issue #15): the action waiting for a button (-1: none), armed once
    // every button and trigger was released, started at captureStartMs (monotonic). After it ends,
    // captureRelease makes waitRelease wait for the directions too (a captured D-pad must not move).
    int captureAction = -1;
    bool captureArmed = false;
    uint64_t captureStartMs = 0;
    bool captureRelease = false;
    // Updates (pc_update.h): the check at start was asked; the popup (open, its row, shown once per
    // start); a short notice outside the menu; the restart's second A in the PLAY scene.
    bool updateChecked = false;
    bool popup = false;
    int popupRow = 0;
    bool popupDone = false;
    // The release notes page (Sistema > "Notas de la versión", or the popup): its scroll step this frame
    // in lines (a page is kNotesPage lines) and a jump to the top.
    bool notesPage = false;
    int notesScroll = 0;
    bool notesTop = false;
    bool playNoticeDone = false;
    PcUpdateBootResult bootShown = PC_UPDATE_BOOT_NONE;
    std::string notice;
    unsigned int noticeUntil = 0;
    unsigned int restartArmedUntil = 0;
} m;

void toast(const std::string& text) {
    m.toast = text;
    m.toastUntil = pc_frame_count() + 90;
    writef(STDERR_FILENO, "[cos] menu: %s\n", text.c_str());
}

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

// The PLAY scene exists (the game proper, not the title or the name entry).
bool inPlay() {
    return fpcM_Search(isPlayScene, nullptr) != nullptr;
}

std::vector<Row> rowsOf(int tab) {
    std::vector<Row> rows;
    if (tab == PC_SETTING_TAB_GRAPHICS) {
        rows.push_back({RowKind::EditMode});
    }
    if (tab == PC_SETTING_TAB_QOL) {
        rows.push_back({RowKind::QolBetterWW});
        rows.push_back({RowKind::QolOriginal});
    }
    if (tab == PC_SETTING_TAB_SYSTEM) {
        rows.push_back({RowKind::Update});
        rows.push_back({RowKind::Notes});
    }
    if (tab == PC_SETTING_TAB_DEBUG) {
        rows.push_back({RowKind::Warp});
        rows.push_back({RowKind::Sailing});
        rows.push_back({RowKind::Screenshot});
        rows.push_back({RowKind::Reload});
    }
    for (int i = 0; i < pc_settings_count(); i++) {
        const PcSettingDesc* d = pc_settings_at(i);
        if (d == nullptr || d->tab != tab || (d->flags & PC_SETTING_HIDDEN)) {
            continue;
        }
        if (d->flags & PC_SETTING_REMAP) {
            // Issue #15: a button remap action is not a row that cycles its choices: confirm captures.
            for (int a = 0; a < (int)pc::remap::Action::Count; a++) {
                if (strcmp(d->key, pc::remap::actionKey((pc::remap::Action)a)) == 0) {
                    rows.push_back({RowKind::Remap, d, a});
                }
            }
        } else {
            rows.push_back({RowKind::Setting, d});
        }
    }
    if (tab == PC_SETTING_TAB_CONTROLS) {
        rows.push_back({RowKind::RemapRestore});
    }
    return rows;
}

const char* choiceLabel(const PcSettingDesc* d, const char* value) {
    for (int i = 0; i < d->choiceCount; i++) {
        if (strcmp(d->choices[i].value, value) == 0) {
            return T(d->choices[i].label, d->choices[i].labelEn);
        }
    }
    return value[0] != '\0' ? value : "-";
}

PcOperationMode rowMode(const PcSettingDesc* d) {
    return (d->flags & PC_SETTING_PER_MODE) ? m.editMode : pc_settings_mode();
}

void cycle(const PcSettingDesc* d, int dir) {
    if (pc_settings_locked(d->key)) {
        toast(std::string(settingLabel(d)) + T(": fijado por el entorno", ": set by the environment"));
        return;
    }
    if (d->choiceCount <= 0) {
        return;
    }
    const PcOperationMode mode = rowMode(d);
    const char* cur = pc_settings_get_mode(d->key, mode);
    int idx = -1;
    for (int i = 0; i < d->choiceCount; i++) {
        if (strcmp(d->choices[i].value, cur) == 0) {
            idx = i;
        }
    }
    idx = idx < 0 ? 0 : (idx + dir + d->choiceCount) % d->choiceCount;
    const char* value = d->choices[idx].value;
    writef(STDERR_FILENO, "[cos] menu: %s%s%s -> %s\n", d->key, (d->flags & PC_SETTING_PER_MODE) ? "@" : "",
           (d->flags & PC_SETTING_PER_MODE) ? (mode == PC_MODE_DOCKED ? "docked" : "handheld") : "", value);
    pc_settings_set_mode(d->key, mode, value);
}

void setOpen(bool open) {
    if (open == m.open) {
        return;
    }
    m.open = open;
    m.warpPage = false;
    m.notesPage = false;
    m.waitRelease = true;
    m.captureAction = -1;
    PADBlockInput(open);
    if (open) {
        m.opened++;
        if (inPlay()) {
            // The game is frozen while the menu is open, so a rumble running now would never get the
            // game's stop (the pad keeps a rumble until told otherwise), and a warp from the menu ends
            // the scene without it: the motor stops here, as the game's own pause screen does, and
            // the vibration manager's state is reset to match (the game sends new rumbles after).
            dComIfGp_getVibration().Kill();
        }
        m.editMode = pc_settings_mode();
        m.scrollToRow = true;
        writef(STDERR_FILENO, "[cos] menu: open at frame %u (%s)\n", pc_frame_count(),
               inPlay() ? "game paused" : "not in the PLAY scene: the game runs on without input");
        heapReport("options menu opened");
    } else {
        writef(STDERR_FILENO, "[cos] menu: closed at frame %u\n", pc_frame_count());
    }
}

// A warp ordered here (Viajar, Navegar, the debug server's warp) can leave in the middle of conducting
// the Wind Waker, which the game's own ways out of a scene never do: conducting stops its loops itself
// (JAIZelInst::reset from daPy_lk_c's tact procedures). Those loops (metronome, drone, pulse, auftakt)
// play with no actor position, so deleting the scene's actors does not stop them, and a warp while
// conducting left them sounding in the next scene for good. The player conducts on through the fade-out
// (restarting them on each beat), so they are stopped as JAIZelInst::reset does once the next PLAY scene
// runs (warpAudioFrame); the auftakt, which only the next beat's sounds end, as well.
fpc_ProcID sWarpFromScene = fpcM_ERROR_PROCESS_ID_e; // the PLAY scene a warp ordered here leaves

void noteWarp() {
    if (void* scene = fpcM_Search(isPlayScene, nullptr); scene != nullptr) {
        sWarpFromScene = fpcM_GetID(scene);
    }
}

void warpAudioFrame() {
    if (sWarpFromScene == fpcM_ERROR_PROCESS_ID_e) {
        return;
    }
    void* scene = fpcM_Search(isPlayScene, nullptr);
    if (scene == nullptr || fpcM_GetID(scene) == sWarpFromScene || fpcM_IsCreating(fpcM_GetID(scene))) {
        return;
    }
    sWarpFromScene = fpcM_ERROR_PROCESS_ID_e;
    mDoAud_tact_reset();
    mDoAud_seStop(JA_SE_WTAKT_AUFTAKT);
    mDoAud_seStop(JA_SE_WTAKT_AUFTAKT_4);
    mDoAud_seStop(JA_SE_WTAKT_AUFTAKT_6);
}

void doWarp(const Warp& w) {
    if (!inPlay()) {
        toast(T("El viaje solo funciona durante la partida", "Travel only works in the game"));
        return;
    }
    setOpen(false);
    noteWarp();
    writef(STDERR_FILENO, "[cos] menu: warp to %s room %d point %d\n", w.stage, w.room, w.point);
    dComIfGp_setNextStage(w.stage, (s16)w.point, (s8)w.room, -1);
}

// "Navegar (barco, vela y batuta)": the sailing preset (pc_preset.cpp) on the file being played, in
// memory only (the game's own save screen keeps it), then to its spawn on the boat.
void doSailing() {
    if (!inPlay()) {
        toast(T("Navegar solo funciona durante la partida", "Sailing only works in the game"));
        return;
    }
    applySailingPreset(true);
    setOpen(false);
    noteWarp();
    writef(STDERR_FILENO, "[cos] menu: sailing preset, warp to %s room %d point %d\n", kSailingSpawn.stage,
           kSailingSpawn.room, kSailingSpawn.point);
    dComIfGp_setNextStage(kSailingSpawn.stage, (s16)kSailingSpawn.point, (s8)kSailingSpawn.room, -1);
}

// Calidad de vida > "Experiencia BetterWW" / "Valores originales": every QoL row set at once, as if
// changed by hand (saved to settings.ini, each row still editable after). BetterWW's own defaults
// (wwr_ui/randomizer_window.ui at 4501481): everything on but the full sea chart and the unrestricted
// boat; its "remove title and ending videos" also skips the intro and fixes the original's bugs (the
// videos part is not an option here: the port plays no THP video yet, docs/QOL_PLAN.md).
struct QolPresetValue {
    PcQol which;
    const char* betterww;
};
const QolPresetValue kQolPreset[] = {
    {PC_QOL_INSTANT_TEXT, "1"}, {PC_QOL_FAST_ROLL, "1"},      {PC_QOL_FAST_BLOCKS, "1"},
    {PC_QOL_FAST_GRAPPLE, "1"}, {PC_QOL_FAST_CLIMB, "1"},     {PC_QOL_FAST_BALLAD, "1"},
    {PC_QOL_SKIP_INTRO, "1"},   {PC_QOL_REVEAL_CHART, "0"},
    {PC_QOL_TINGLE_CHESTS, "1"}, {PC_QOL_SAIL, "swift"},      {PC_QOL_FREE_BOAT, "0"},
    {PC_QOL_SWING_TURN, "1"},   {PC_QOL_NO_SONG_REPLAY, "1"}, {PC_QOL_FIX_BUGS, "1"},
};
static_assert(sizeof(kQolPreset) / sizeof(kQolPreset[0]) == PC_QOLS, "a QoL option without a preset value");

void applyQolPreset(bool betterww) {
    int locked = 0;
    for (const QolPresetValue& p : kQolPreset) {
        const char* key = pc_qol_key(p.which);
        if (pc_settings_locked(key)) {
            locked++;
            continue;
        }
        pc_settings_set_mode(key, pc_settings_mode(), betterww ? p.betterww : "0");
    }
    writef(STDERR_FILENO, "[cos] menu: QoL preset %s applied (%d rows set by the environment kept)\n",
           betterww ? "BetterWW" : "original", locked);
    toast(betterww ? T("Calidad de vida: valores de BetterWW", "Quality of life: BetterWW's values")
                   : T("Calidad de vida: juego original", "Quality of life: the original game"));
}

// ---- the Controles tab's capture (issue #15, docs/BUTTON_REMAP_DESIGN.md) ------------------------

constexpr uint64_t kCaptureMs = 5000;

uint64_t nowMs() {
    return monotonicNs() / 1000000u;
}

// The label of a remap action's row.
const char* remapActionLabel(int action) {
    const char* key = pc::remap::actionKey((pc::remap::Action)action);
    for (int i = 0; i < pc_settings_count(); i++) {
        const PcSettingDesc* d = pc_settings_at(i);
        if (d != nullptr && strcmp(d->key, key) == 0) {
            return settingLabel(d);
        }
    }
    return key;
}

void startCapture(const Row& r) {
    if (pc_settings_locked(r.desc->key)) {
        toast(std::string(settingLabel(r.desc)) + T(": fijado por el entorno", ": set by the environment"));
        return;
    }
    m.captureAction = r.action;
    m.captureArmed = false;
    m.captureStartMs = nowMs();
    writef(STDERR_FILENO, "[cos] menu: capture for %s\n", r.desc->key);
}

void endCapture(const char* why) {
    writef(STDERR_FILENO, "[cos] menu: capture for %s %s\n", pc::remap::actionKey((pc::remap::Action)m.captureAction),
           why);
    m.captureAction = -1;
    // The button just pressed (or still held) does nothing to the menu until every key is released.
    m.waitRelease = true;
    m.captureRelease = true;
}

// Once per menu frame while a capture runs, before handleInput (which then does nothing else): the
// controllers the remap covers (pc_button_remap_port_input; a GameCube adapter or an Xbox pad keeps
// Aurora's own table, so a press there would not mean the input it names). Not armed until none of
// them holds an assignable input or Minus (the confirm that started it is not taken; a control stuck
// elsewhere cannot keep it from arming); then Minus (BACK) cancels and the first assignable input is
// given to the action (pc_button_remap_assign, which swaps with the action that held it); kCaptureMs
// without a press cancels it. Runs in menuFrame, inside pc_frame_end's host-allocation scope (the assignment's
// strings stay off the game's heaps).
void captureFrame() {
    if (m.captureAction < 0) {
        return;
    }
    bool back = false;
    int input = -1;
    for (int port = 0; port < 4; port++) {
        int portBack = 0;
        const int held = pc_button_remap_port_input(port, &portBack);
        back = back || portBack != 0;
        if (input < 0 && held >= 0) {
            input = held;
        }
    }
    if (!m.captureArmed) {
        m.captureArmed = !back && input < 0;
    } else if (back) {
        endCapture("cancelled (Minus)");
        return;
    } else if (input >= 0) {
        const int action = m.captureAction;
        int swapped = -1;
        if (pc_button_remap_assign(action, input, &swapped) != 0) {
            toast(T("No se pudo cambiar el botón", "Could not change the button"));
        } else if (swapped >= 0) {
            toast(std::string(T("Intercambiado con ", "Swapped with ")) + remapActionLabel(swapped));
        }
        endCapture("done");
        return;
    }
    if (nowMs() - m.captureStartMs >= kCaptureMs) {
        endCapture("cancelled (no press)");
    }
}

// ---- updates (pc_update.h) -----------------------------------------------------------------------

uint32_t pressed(uint32_t keys);
void openNotes();

void* isAfterLogoScene(void* proc, void*) {
    switch (fpcM_GetName(proc)) {
    case fpcNm_MENU_SCENE_e:
    case fpcNm_PLAY_SCENE_e:
    case fpcNm_OPENING_SCENE_e:
    case fpcNm_OPENING2_SCENE_e:
    case fpcNm_TITLE_SCENE_e:
    case fpcNm_ENDING_SCENE_e:
    case fpcNm_NAME_SCENE_e:
    case fpcNm_NAMEEX_SCENE_e:
    case fpcNm_OPEN_SCENE_e:
    case fpcNm_OPEN2_SCENE_e:
        return proc;
    default:
        return nullptr;
    }
}

// A scene after the logo runs: the shader preload is over and the game got going (it confirms a
// version the game installed, pc_update_frame).
bool afterLogo() {
    return fpcM_Search(isAfterLogoScene, nullptr) != nullptr;
}

void notice(const std::string& text) {
    m.notice = text;
    m.noticeUntil = pc_frame_count() + 300;
    writef(STDERR_FILENO, "[cos] menu: notice: %s\n", text.c_str());
}

const char* updateErrorText(PcUpdateError e) {
    switch (e) {
    case PC_UPDATE_ERR_NETWORK: return T("sin conexión con GitHub", "no connection to GitHub");
    case PC_UPDATE_ERR_SERVER: return T("GitHub no dio una respuesta válida", "GitHub gave no usable answer");
    case PC_UPDATE_ERR_NO_ASSET: return T("la versión no trae el zip de esta región", "the release has no zip for this region");
    case PC_UPDATE_ERR_NOT_HERE:
        return T("el juego no está en /switch/switchwaker/switchwaker.nro", "the game is not in /switch/switchwaker/switchwaker.nro");
    case PC_UPDATE_ERR_NO_SPACE: return T("no hay espacio en la tarjeta SD", "not enough space on the SD card");
    case PC_UPDATE_ERR_CHECKSUM: return T("la descarga llegó dañada (SHA-256)", "the download arrived damaged (SHA-256)");
    case PC_UPDATE_ERR_BAD_ZIP: return T("el zip de la versión no es válido", "the release's zip is not valid");
    case PC_UPDATE_ERR_WRITE: return T("no se pudo escribir en la tarjeta SD", "could not write to the SD card");
    case PC_UPDATE_OK: break;
    }
    return T("error", "error");
}

void checkForUpdates(bool atStart) {
    pc_update_check(atStart ? 1 : 0, pc_settings_get("COS_UPDATE_SKIP"));
}

void requestRestart() {
    if (!mDoMemCd_isCardCommNone()) {
        toast(T("Espera: se está guardando la partida", "Wait: the game is being saved"));
        return;
    }
    if (inPlay() && pc_frame_count() >= m.restartArmedUntil) {
        m.restartArmedUntil = pc_frame_count() + 150;
        toast(T("Pulsa A otra vez para reiniciar: se pierde lo que no hayas guardado",
                "Press A again to restart: anything not saved is lost"));
        return;
    }
    if (!pc_update_restart()) {
        toast(T("Cierra el juego y vuelve a abrirlo para usar la versión nueva",
                "Close the game and open it again to run the new version"));
    }
}

// The Sistema tab's "Notas de la versión" row: the notes page, once a check found a newer release.
void activateNotes() {
    if (pc_update_get_notes(nullptr, 0) == 0) {
        toast(T("No hay notas: busca antes una versión nueva (Actualización)",
                "No notes: check for a new version first (Update)"));
        return;
    }
    openNotes();
}

// The Sistema tab's "Actualización" row: A checks, installs or restarts, by the state.
void activateUpdate() {
    PcUpdateInfo info;
    pc_update_get_info(&info);
    switch (info.status) {
    case PC_UPDATE_AVAILABLE:
    case PC_UPDATE_INSTALL_FAILED:
        if (!info.canInstall) {
            toast(std::string(T("No se puede actualizar desde aquí: ", "Cannot update from here: ")) +
                  updateErrorText(PC_UPDATE_ERR_NOT_HERE));
        } else if (pc_update_install()) {
            writef(STDERR_FILENO, "[cos] menu: installing %s\n", info.latest);
        }
        break;
    case PC_UPDATE_INSTALLED:
        requestRestart();
        break;
    case PC_UPDATE_CHECKING:
    case PC_UPDATE_DOWNLOADING:
    case PC_UPDATE_INSTALLING:
        break;
    case PC_UPDATE_DISABLED:
        toast(T("Esta es una versión de desarrollo: no busca actualizaciones",
                "This is a development build: it does not check for updates"));
        break;
    default:
        checkForUpdates(false);
        break;
    }
}

std::string updateValue() {
    PcUpdateInfo info;
    pc_update_get_info(&info);
    char text[128];
    switch (info.status) {
    case PC_UPDATE_IDLE: return T("A: buscar", "A: check");
    case PC_UPDATE_DISABLED: return T("versión de desarrollo", "development build");
    case PC_UPDATE_CHECKING: return T("Buscando...", "Checking...");
    case PC_UPDATE_UP_TO_DATE:
        snprintf(text, sizeof(text), T("Al día (%s)", "Up to date (%s)"), info.current);
        return text;
    case PC_UPDATE_CHECK_FAILED: return T("No se pudo buscar (A: reintentar)", "Check failed (A: retry)");
    case PC_UPDATE_AVAILABLE:
        snprintf(text, sizeof(text), info.canInstall ? T("A: instalar %s", "A: install %s") : T("%s disponible", "%s available"),
                 info.latest);
        return text;
    case PC_UPDATE_DOWNLOADING:
        if (info.total > 0) {
            snprintf(text, sizeof(text), T("Descargando %d %%", "Downloading %d%%"),
                     (int)(std::min<uint64_t>(info.done, info.total) * 100 / info.total));
            return text;
        }
        return T("Descargando...", "Downloading...");
    case PC_UPDATE_INSTALLING: return T("Instalando...", "Installing...");
    case PC_UPDATE_INSTALLED:
        return pc_update_can_restart() ? T("Instalada (A: reiniciar)", "Installed (A: restart)")
                                       : T("Instalada: reinicia el juego", "Installed: restart the game");
    case PC_UPDATE_INSTALL_FAILED: return T("Falló (A: reintentar)", "Failed (A: retry)");
    }
    return "";
}

std::string updateHelp() {
    PcUpdateInfo info;
    pc_update_get_info(&info);
    char text[512];
    switch (info.status) {
    case PC_UPDATE_AVAILABLE:
        if (info.canInstall) {
            snprintf(text, sizeof(text),
                     T("%s (tienes la %s). A la descarga (unos 10 MB), la comprueba y cambia los archivos del juego; "
                       "partidas, ajustes y disco no se tocan. Si la versión nueva no arranca, vuelve sola la anterior.",
                       "%s (you have %s). A downloads it (about 10 MB), checks it and replaces the game's files; saves, "
                       "settings and disc are untouched. If the new version does not start, the old one comes back by itself."),
                     info.title[0] != '\0' ? info.title : info.latest, info.current);
        } else {
            snprintf(text, sizeof(text),
                     T("%s (tienes la %s). Esta copia no puede actualizarse sola (%s): copia la carpeta switch del zip de "
                       "la versión sobre la tarjeta SD.",
                       "%s (you have %s). This copy cannot update itself (%s): copy the release zip's switch folder onto "
                       "the SD card."),
                     info.title[0] != '\0' ? info.title : info.latest, info.current, updateErrorText(PC_UPDATE_ERR_NOT_HERE));
        }
        return text;
    case PC_UPDATE_INSTALLED:
        snprintf(text, sizeof(text),
                 pc_update_can_restart()
                     ? T("%s está instalada. A reinicia el juego con ella (guarda antes la partida).",
                         "%s is installed. A restarts the game with it (save your game first).")
                 : kSwitch ? T("%s está instalada. Cierra el juego y ábrelo de nuevo desde el Homebrew Menu.",
                               "%s is installed. Close the game and open it again from the Homebrew Menu.")
                           : T("%s está instalada. Cierra el juego y ábrelo de nuevo.",
                               "%s is installed. Close the game and open it again."),
                 info.latest);
        return text;
    case PC_UPDATE_CHECK_FAILED:
    case PC_UPDATE_INSTALL_FAILED:
        snprintf(text, sizeof(text), "%s: %s (%s)", T("No se pudo", "It failed"), updateErrorText(info.error), info.detail);
        return text;
    case PC_UPDATE_DISABLED:
        return T("Las versiones de desarrollo no buscan actualizaciones.", "Development builds do not check for updates.");
    default:
        snprintf(text, sizeof(text),
                 T("Versión instalada: %s. A pregunta a GitHub si hay una nueva (necesita internet).",
                   "Installed version: %s. A asks GitHub whether there is a new one (needs the internet)."),
                 info.current);
        return text;
    }
}

std::string updateNotes() {
    const size_t n = pc_update_get_notes(nullptr, 0);
    std::string text(n + 1, '\0');
    pc_update_get_notes(text.data(), text.size());
    text.resize(n);
    return text;
}

constexpr int kNotesPage = 10; // lines a page step scrolls

void openNotes() {
    m.notesPage = true;
    m.notesTop = true;
    m.notesScroll = 0;
}

void openUpdateRow() {
    m.tab = PC_SETTING_TAB_SYSTEM;
    m.row[PC_SETTING_TAB_SYSTEM] = 0; // "Actualización"
    setOpen(true);
}

// Once per frame, menu open or not: the check at start, the confirmation of an installed version, the
// popup and the notices.
void updateFrame() {
    pc_update_frame(afterLogo() ? 1 : 0);
    if (!m.updateChecked) {
        m.updateChecked = true;
        if (strcmp(pc_settings_get("COS_UPDATE_CHECK"), "0") != 0) {
            checkForUpdates(true);
        }
    }
    PcUpdateInfo info;
    pc_update_get_info(&info);
    if (info.boot != m.bootShown) {
        m.bootShown = info.boot;
        char text[160];
        switch (info.boot) {
        case PC_UPDATE_BOOT_FIRST_START:
            snprintf(text, sizeof(text), T("SwitchWaker actualizado a %s", "SwitchWaker updated to %s"), info.bootTag);
            notice(text);
            break;
        case PC_UPDATE_BOOT_FAILED_BACK:
            snprintf(text, sizeof(text), T("%s no llegó a arrancar: vuelve a estar la versión anterior",
                                           "%s did not start: the previous version is back"),
                     info.bootTag);
            notice(text);
            break;
        case PC_UPDATE_BOOT_ROLLED_BACK:
            snprintf(text, sizeof(text), T("La instalación de %s no terminó y se ha deshecho",
                                           "The install of %s did not finish and was undone"),
                     info.bootTag);
            notice(text);
            break;
        case PC_UPDATE_BOOT_NONE: break;
        }
    }
    const bool announce = info.status == PC_UPDATE_AVAILABLE && info.fromStart && !info.skipped;
    if (announce && !m.popupDone && !m.open && !m.popup && !inPlay() && afterLogo()) {
        m.popup = true;
        m.popupDone = true;
        m.popupRow = info.canInstall ? 0 : pc_update_get_notes(nullptr, 0) > 0 ? 1 : 2;
        m.waitRelease = true;
        PADBlockInput(true);
        writef(STDERR_FILENO, "[cos] menu: update popup for %s\n", info.latest);
    } else if (announce && !m.popupDone && !m.playNoticeDone && inPlay()) {
        // found during play: no popup over the game, a notice that points at the menu
        m.playNoticeDone = true;
        char text[160];
        snprintf(text, sizeof(text), T("Hay una versión nueva, %s: Menos > Sistema", "A new version, %s: Minus > System"),
                 info.latest);
        notice(text);
    }
}

void closePopup() {
    m.popup = false;
    m.waitRelease = true;
    PADBlockInput(m.open);
}

// The popup's choices: update now (opens the Sistema tab and starts), the release notes (the Sistema tab's
// notes page), later, not this version. The first two only when they can be had.
constexpr int kPopupChoices = 4;

bool popupUsable(int row, const PcUpdateInfo& info) {
    return row == 0 ? info.canInstall != 0 : row == 1 ? pc_update_get_notes(nullptr, 0) > 0 : true;
}

void popupInput(uint32_t keys) {
    const uint32_t p = pressed(keys);
    if (m.waitRelease) {
        if (keys == 0) {
            m.waitRelease = false;
        }
        return;
    }
    PcUpdateInfo info;
    pc_update_get_info(&info);
    if (p & (kUp | kDown)) {
        // the next usable choice that way (popupUsable), staying put at either end
        const int step = (p & kUp) ? -1 : 1;
        for (int r = m.popupRow + step; r >= 0 && r < kPopupChoices; r += step) {
            if (popupUsable(r, info)) {
                m.popupRow = r;
                break;
            }
        }
    }
    if (p & (kBack | kCombo)) {
        writef(STDERR_FILENO, "[cos] menu: update popup: later\n");
        closePopup();
        return;
    }
    if (!(p & kConfirm)) {
        return;
    }
    if (!popupUsable(m.popupRow, info)) {
        return;
    }
    if (m.popupRow == 0) {
        writef(STDERR_FILENO, "[cos] menu: update popup: update now\n");
        closePopup();
        openUpdateRow();
        pc_update_install();
    } else if (m.popupRow == 1) {
        writef(STDERR_FILENO, "[cos] menu: update popup: release notes\n");
        closePopup();
        openUpdateRow();
        openNotes();
    } else if (m.popupRow == 3) {
        writef(STDERR_FILENO, "[cos] menu: update popup: not about %s\n", info.latest);
        pc_settings_set("COS_UPDATE_SKIP", info.latest);
        closePopup();
    } else {
        writef(STDERR_FILENO, "[cos] menu: update popup: later\n");
        closePopup();
    }
}

void activate(const Row& r, int dir) {
    if (m.captureAction >= 0) {
        return;
    }
    switch (r.kind) {
    case RowKind::EditMode:
        m.editMode = m.editMode == PC_MODE_HANDHELD ? PC_MODE_DOCKED : PC_MODE_HANDHELD;
        break;
    case RowKind::Setting:
        cycle(r.desc, dir);
        break;
    case RowKind::Warp:
        if (dir == 0) {
            m.warpPage = true;
            m.scrollToRow = true;
        }
        break;
    case RowKind::Sailing:
        if (dir == 0) {
            doSailing();
        }
        break;
    case RowKind::Screenshot:
        if (dir == 0) {
            m.pendingShot = true;
        }
        break;
    case RowKind::QolBetterWW:
    case RowKind::QolOriginal:
        if (dir == 0) {
            applyQolPreset(r.kind == RowKind::QolBetterWW);
        }
        break;
    case RowKind::Reload:
        if (dir == 0) {
            pc_settings_reload();
            toast(std::string(T("Ajustes recargados de ", "Settings reloaded from ")) + pc_settings_path());
        }
        break;
    case RowKind::Remap:
        if (dir == 0) {
            startCapture(r);
        }
        break;
    case RowKind::RemapRestore:
        if (dir == 0) {
            pc_button_remap_restore_defaults();
            toast(T("Controles por defecto restaurados", "Default controls restored"));
        }
        break;
    case RowKind::Update:
        if (dir == 0) {
            activateUpdate();
        }
        break;
    case RowKind::Notes:
        if (dir == 0) {
            activateNotes();
        }
        break;
    }
}

// Pressed this frame, or held long enough to repeat (directions only).
uint32_t pressed(uint32_t keys) {
    uint32_t edge = keys & ~m.prevKeys;
    const uint32_t dirs = keys & (kUp | kDown | kLeft | kRight);
    if (dirs != 0 && dirs == m.repeatKey) {
        m.repeatFrames++;
        if (m.repeatFrames >= 10 && (m.repeatFrames - 10) % 3 == 0) {
            edge |= dirs;
        }
    } else {
        m.repeatKey = dirs;
        m.repeatFrames = 0;
    }
    m.prevKeys = keys;
    return edge;
}

void handleInput(uint32_t keys) {
    const uint32_t p = pressed(keys);
    if (m.captureAction >= 0) {
        return; // captureFrame reads the controllers
    }
    if (m.waitRelease) {
        if ((keys & ~(m.captureRelease ? 0u : (kUp | kDown | kLeft | kRight))) == 0) {
            m.waitRelease = false;
            m.captureRelease = false;
        }
        return;
    }
    if (!m.open) {
        if (p & kCombo) {
            setOpen(true);
        }
        return;
    }
    if (p & kCombo) {
        setOpen(false);
        return;
    }
    if (m.notesPage) {
        if (p & kBack) {
            m.notesPage = false;
            m.scrollToRow = true;
            return;
        }
        m.notesScroll += (p & kUp) ? -1 : (p & kDown) ? 1 : (p & kLeft) ? -kNotesPage : (p & kRight) ? kNotesPage : 0;
        if (p & kConfirm) {
            PcUpdateInfo info;
            pc_update_get_info(&info);
            if ((info.status == PC_UPDATE_AVAILABLE || info.status == PC_UPDATE_INSTALL_FAILED) && info.canInstall) {
                m.notesPage = false; // back on the Actualización row, which shows the download
                m.row[PC_SETTING_TAB_SYSTEM] = 0;
                activateUpdate();
            }
        }
        return;
    }
    if (m.warpPage) {
        const int count = kMainWarpCount + kAllWarpCount;
        if (p & kBack) {
            m.warpPage = false;
            m.scrollToRow = true;
            return;
        }
        if (p & (kUp | kDown | kLeft | kRight)) {
            const int step = (p & kLeft) ? -10 : (p & kRight) ? 10 : (p & kUp) ? -1 : 1;
            m.warpRow = std::max(0, std::min(count - 1, m.warpRow + step));
            m.scrollToRow = true;
        }
        if (p & kConfirm) {
            doWarp(warpAt(m.warpRow));
        }
        return;
    }
    if (p & kBack) {
        setOpen(false);
        return;
    }
    if (p & (kTabLeft | kTabRight)) {
        m.tab = kTabOrder[(tabPosition(m.tab) + ((p & kTabRight) ? 1 : PC_SETTING_TABS - 1)) % PC_SETTING_TABS];
        m.scrollToRow = true;
        writef(STDERR_FILENO, "[cos] menu: tab %s\n", kTabNames[m.tab]);
    }
    const std::vector<Row> rows = rowsOf(m.tab);
    if (rows.empty()) {
        return;
    }
    int& row = m.row[m.tab];
    row = std::min(row, (int)rows.size() - 1);
    if (p & kUp) {
        row = (row + (int)rows.size() - 1) % (int)rows.size();
        m.scrollToRow = true;
    }
    if (p & kDown) {
        row = (row + 1) % (int)rows.size();
        m.scrollToRow = true;
    }
    if (p & kConfirm) {
        activate(rows[row], rows[row].kind == RowKind::Setting || rows[row].kind == RowKind::EditMode ? 1 : 0);
    }
    if (p & (kLeft | kRight)) {
        const Row& r = rows[row];
        if (r.kind == RowKind::Setting || r.kind == RowKind::EditMode) {
            activate(r, (p & kLeft) ? -1 : 1);
        }
    }
}

// ---- drawing -------------------------------------------------------------------------------------

const ImVec4 kAccent(1.00f, 0.80f, 0.30f, 1.0f);
const ImVec4 kDim(0.62f, 0.66f, 0.72f, 1.0f);
const ImVec4 kWarn(1.00f, 0.55f, 0.45f, 1.0f);

std::string rowLabel(const Row& r) {
    switch (r.kind) {
    case RowKind::EditMode: return T("Perfil a editar", "Profile to edit");
    case RowKind::Warp: return T("Viajar a un escenario...", "Travel to a stage...");
    case RowKind::Sailing: return T("Navegar (barco, vela y batuta)", "Sail (boat, sail and baton)");
    case RowKind::Screenshot: return T("Capturar pantalla", "Take a screenshot");
    case RowKind::Reload: return T("Recargar ajustes del archivo", "Reload settings from the file");
    case RowKind::QolBetterWW: return T("Experiencia BetterWW", "BetterWW experience");
    case RowKind::QolOriginal: return T("Valores originales", "Original values");
    case RowKind::RemapRestore: return T("Restaurar controles por defecto", "Restore default controls");
    case RowKind::Remap: return settingLabel(r.desc);
    case RowKind::Update: return T("Actualización", "Update");
    case RowKind::Notes: return T("Notas de la versión", "Release notes");
    case RowKind::Setting: break;
    }
    std::string s = settingLabel(r.desc);
    if (r.desc->flags & PC_SETTING_PER_MODE) {
        s += m.editMode == PC_MODE_DOCKED ? T(" [sobremesa]", " [docked]") : T(" [portátil]", " [handheld]");
    }
    return s;
}

std::string rowValue(const Row& r) {
    switch (r.kind) {
    case RowKind::EditMode:
        return std::string(modeName(m.editMode)) + (m.editMode == pc_settings_mode() ? T(" (activo)", " (active)") : "");
    case RowKind::Warp: return inPlay() ? T("A: elegir", "A: choose") : T("solo en partida", "in game only");
    case RowKind::Sailing: return inPlay() ? T("A: zarpar", "A: set sail") : T("solo en partida", "in game only");
    case RowKind::Screenshot: return T("A: capturar", "A: capture");
    case RowKind::Reload: return T("A: recargar", "A: reload");
    case RowKind::QolBetterWW:
    case RowKind::QolOriginal: return T("A: aplicar", "A: apply");
    case RowKind::RemapRestore: return T("A: restaurar", "A: restore");
    case RowKind::Update: return updateValue();
    case RowKind::Notes: {
        PcUpdateInfo info;
        pc_update_get_info(&info);
        if (pc_update_get_notes(nullptr, 0) == 0) {
            return "-";
        }
        char text[64];
        snprintf(text, sizeof(text), T("A: ver (%s)", "A: read (%s)"), info.latest);
        return text;
    }
    case RowKind::Remap: {
        if (r.action == m.captureAction) {
            const uint64_t elapsed = std::min<uint64_t>(nowMs() - m.captureStartMs, kCaptureMs);
            // "-" for Minus: the menu's font (ImGui's default, Latin-1) has no U+2212.
            char text[96];
            snprintf(text, sizeof(text), T("Pulsa un botón... (- cancela) %d s", "Press a button... (- cancels) %d s"),
                     (int)((kCaptureMs - elapsed + 999) / 1000));
            return text;
        }
        const int input = pc_button_remap_current(r.action);
        return choiceLabel(r.desc, pc::remap::inputValue((pc::remap::Input)input));
    }
    case RowKind::Setting: break;
    }
    return choiceLabel(r.desc, pc_settings_get_mode(r.desc->key, rowMode(r.desc)));
}

std::string rowNote(const Row& r) {
    if (r.kind != RowKind::Setting && r.kind != RowKind::Remap) {
        return "";
    }
    const PcSettingDesc* d = r.desc;
    if (pc_settings_locked(d->key)) {
        return T("fijado por variable de entorno", "set by an environment variable");
    }
    if ((d->flags & PC_SETTING_SWITCH_ONLY) && !kSwitch) {
        return T("solo Switch", "Switch only");
    }
    if (d->flags & PC_SETTING_RESTART) {
        return pc_settings_restart_pending(d->key) ? T("requiere reiniciar (pendiente)", "needs a restart (pending)")
                                                   : T("requiere reiniciar", "needs a restart");
    }
    if (strcmp(d->key, "COS_HD_TEXTURES") == 0 && !pc_settings_has_subscriber(d->key)) {
        return T("cargador aún no incluido", "loader not included yet");
    }
    if ((d->flags & PC_SETTING_PER_MODE) && m.editMode != pc_settings_mode()) {
        return T("se aplica en ese modo", "applies in that mode");
    }
    return "";
}

std::string rowHelp(const Row& r) {
    switch (r.kind) {
    case RowKind::EditMode:
        return T("Las opciones marcadas [portátil]/[sobremesa] guardan un valor por modo; se aplican solas al "
                 "conectar o quitar la base.",
                 "Options marked [handheld]/[docked] keep one value per mode; they apply by themselves when the "
                 "console is docked or undocked.");
    case RowKind::Warp:
        return T("Lleva al jugador a otro escenario (lugares principales y todos los del disco).",
                 "Takes the player to another stage (the main places and every one on the disc).");
    case RowKind::Sailing:
        return T("Da a esta partida el barco, la vela (X), la batuta (Y) y la canción del viento, y lleva al jugador "
                 "en barco junto a Isla Taura. Solo en memoria: se guarda únicamente si guardas la partida.",
                 "Gives this game the boat, the sail (X), the baton (Y) and the wind's song, and puts the player "
                 "in the boat next to Windfall Island. In memory only: kept only if you save the game.");
    case RowKind::Screenshot:
        return T("Guarda la imagen del juego (sin el menú) como shot-<cuadro>.png en la carpeta del juego.",
                 "Saves the game's image (without the menu) as shot-<frame>.png in the game's folder.");
    case RowKind::Reload:
        return std::string(T("Vuelve a leer ", "Reads ")) + pc_settings_path() + T(".", " again.");
    case RowKind::QolBetterWW:
        return T("Pone las opciones de esta pestaña como las trae BetterWW (de WideBoner, basado en el "
                 "randomizer wwrando de LagoLunatic): todo activado menos la carta náutica completa y el barco "
                 "sin restricciones; vela rápida. Después se puede cambiar cada opción.",
                 "Sets this tab's options to BetterWW's defaults (by WideBoner, based on LagoLunatic's wwrando "
                 "randomizer): everything on but the full sea chart and the unrestricted boat; Swift Sail. "
                 "Each option can be changed afterwards.");
    case RowKind::QolOriginal:
        return T("Desactiva todas las opciones de esta pestaña: el juego original.",
                 "Turns every option of this tab off: the original game.");
    case RowKind::RemapRestore:
        return T("Vuelve a poner cada acción en su botón de siempre. Fuera del juego: borrar las líneas COS_PAD_ "
                 "de settings.ini hace lo mismo.",
                 "Puts every action back on its usual button. Outside the game: deleting the COS_PAD_ lines of "
                 "settings.ini does the same.");
    case RowKind::Update:
        return updateHelp();
    case RowKind::Notes:
        return T("Lo que trae la versión nueva, de su página en GitHub (en inglés). Aparece cuando la búsqueda "
                 "encuentra una.",
                 "What the new version brings, from its GitHub page. It shows up once a check finds one.");
    case RowKind::Remap:
        if (r.action == m.captureAction) {
            return T("Pulsa el botón que hará esta acción; si ya hacía otra, se intercambian. Menos cancela.",
                     "Press the button that will do this action; if it already did another, the two swap. Minus "
                     "cancels.");
        }
        break;
    case RowKind::Setting: break;
    }
    const char* descHelp = settingHelp(r.desc);
    std::string help = descHelp != nullptr ? descHelp : "";
    if (r.kind == RowKind::Remap) {
        help += T(" A y luego el botón nuevo; si ya tenía otra acción, se intercambian.",
                  " A, then the new button; an action already on it swaps.");
    }
    if (pc_settings_locked(r.desc->key)) {
        // Only the Mac's and Linux's process environment locks a setting (pc_settings.h).
        help += T(" Fijado por una variable de entorno de esta ejecución.",
                  " Set by an environment variable of this run.");
    }
    return help;
}

void drawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    // About 20 px text at 720 lines (the Switch's screen, the Mac's default window).
    const float scale = std::max(1.0f, io.DisplaySize.y / 720.0f * 1.7f);
    const ImVec2 size(std::min(io.DisplaySize.x * 0.9f, 620.0f * scale / 1.6f * 1.6f),
                      io.DisplaySize.y * 0.82f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("##cos_options_menu", nullptr, kFlags)) {
        ImGui::End();
        return;
    }
    ImGui::SetWindowFontScale(scale);
    ImGui::SetWindowFocus();

    ImGui::TextColored(kAccent, "%s", T("Opciones", "Options"));
#if defined(__SWITCH__)
    // the version, so a player can tell which build they run (an issue report, an update)
    ImGui::SameLine();
    ImGui::TextColored(kDim, "SwitchWaker %s", cos_switch_version());
#endif
    const std::string mode = std::string(T("Modo: ", "Mode: ")) + modeName(pc_settings_mode());
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(mode.c_str()).x - ImGui::GetStyle().WindowPadding.x);
    ImGui::TextColored(kDim, "%s", mode.c_str());

    // Tabs.
    // Each tab as wide as its name, the rest of the row shared out evenly (five names in a narrow 4:3 window).
    float namesW = 0.0f;
    for (int t = 0; t < PC_SETTING_TABS; t++) {
        namesW += ImGui::CalcTextSize(T(kTabNames[t], kTabNamesEn[t])).x;
    }
    const float tabSpare = std::max(0.0f, (ImGui::GetContentRegionAvail().x - namesW -
                                           (PC_SETTING_TABS - 1) * ImGui::GetStyle().ItemSpacing.x) /
                                              PC_SETTING_TABS);
    for (int pos = 0; pos < PC_SETTING_TABS; pos++) {
        const int t = kTabOrder[pos];
        if (pos > 0) {
            ImGui::SameLine();
        }
        const bool sel = t == m.tab;
        const char* name = T(kTabNames[t], kTabNamesEn[t]);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.20f, 0.42f, 0.70f, 1.0f) : ImVec4(0.16f, 0.18f, 0.22f, 1.0f));
        if (ImGui::Button(name, ImVec2(ImGui::CalcTextSize(name).x + tabSpare, 0)) && !m.warpPage && !m.notesPage &&
            m.captureAction < 0) {
            m.tab = t;
        }
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    const float footer = ImGui::GetTextLineHeightWithSpacing() * 3.6f;
    std::string help;
    if (ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_NoNav)) {
        // Child windows scale their text by their parent's font scale already.
        float valueX = ImGui::GetContentRegionAvail().x * 0.45f;
        if (m.notesPage) {
            PcUpdateInfo info;
            pc_update_get_info(&info);
            ImGui::TextColored(kAccent, T("Notas de %s   (B: volver)", "Notes of %s   (B: back)"),
                               info.title[0] != '\0' ? info.title : info.latest);
            ImGui::Separator();
            const std::string notes = updateNotes();
            ImGui::PushTextWrapPos(0.0f);
            size_t at = 0;
            while (at < notes.size()) {
                size_t nl = notes.find('\n', at);
                if (nl == std::string::npos) {
                    nl = notes.size();
                }
                const std::string line = notes.substr(at, nl - at);
                at = nl + 1;
                if (line.rfind("# ", 0) == 0) {
                    ImGui::Spacing();
                    ImGui::TextColored(kAccent, "%s", line.c_str() + 2);
                    continue;
                }
                size_t indent = 0;
                while (indent < line.size() && line[indent] == ' ') {
                    indent++;
                }
                const float x = ImGui::GetFontSize() * 0.6f * (float)indent;
                if (x > 0.0f) {
                    ImGui::Indent(x);
                }
                ImGui::TextUnformatted(line.empty() ? " " : line.c_str() + indent);
                if (x > 0.0f) {
                    ImGui::Unindent(x);
                }
            }
            ImGui::PopTextWrapPos();
            if (m.notesTop) {
                ImGui::SetScrollY(0.0f);
                m.notesTop = false;
            } else if (m.notesScroll != 0) {
                ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() + m.notesScroll * ImGui::GetTextLineHeightWithSpacing()));
            }
            m.notesScroll = 0;
            const bool canInstall =
                (info.status == PC_UPDATE_AVAILABLE || info.status == PC_UPDATE_INSTALL_FAILED) && info.canInstall;
            help = canInstall ? T("Arriba/abajo: desplazar (izquierda/derecha: de 10 en 10). A: instalar. B: volver.",
                                  "Up/down: scroll (left/right: 10 lines). A: install. B: back.")
                              : T("Arriba/abajo: desplazar (izquierda/derecha: de 10 en 10). B: volver.",
                                  "Up/down: scroll (left/right: 10 lines). B: back.");
        } else if (m.warpPage) {
            ImGui::TextColored(kAccent, "%s", T("Viajar a...   (B: volver)", "Travel to...   (B: back)"));
            const int count = kMainWarpCount + kAllWarpCount;
            for (int i = 0; i < count; i++) {
                if (i == 0) {
                    ImGui::TextColored(kDim, "%s", T("Lugares principales", "Main places"));
                } else if (i == kMainWarpCount) {
                    ImGui::Separator();
                    ImGui::TextColored(kDim, T("Todos los escenarios del disco (%d)", "Every stage on the disc (%d)"), kAllWarpCount);
                }
                const Warp& w = warpAt(i);
                char label[160];
                snprintf(label, sizeof(label), "%s##w%d", w.label != nullptr ? T(w.label, w.labelEn) : w.stage, i);
                ImGui::PushID(i);
                if (ImGui::Selectable(label, i == m.warpRow)) {
                    m.warpRow = i;
                    doWarp(w);
                }
                ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.62f);
                ImGui::TextColored(kDim, "%s %d:%d", w.stage, w.room, w.point);
                if (i == m.warpRow && m.scrollToRow) {
                    ImGui::SetScrollHereY(0.5f);
                }
                ImGui::PopID();
            }
            help = T("Arriba/abajo: elegir (izquierda/derecha: de 10 en 10). A: viajar. B: volver.",
                     "Up/down: choose (left/right: 10 at a time). A: travel. B: back.");
        } else {
            const std::vector<Row> rows = rowsOf(m.tab);
            float labelW = 0.0f;
            for (const Row& r : rows) {
                labelW = std::max(labelW, ImGui::CalcTextSize(rowLabel(r).c_str()).x);
            }
            valueX = std::min(labelW + ImGui::GetFontSize() * 1.5f, ImGui::GetContentRegionAvail().x * 0.6f);
            int& row = m.row[m.tab];
            row = rows.empty() ? 0 : std::min(row, (int)rows.size() - 1);
            for (int i = 0; i < (int)rows.size(); i++) {
                const Row& r = rows[i];
                const bool locked =
                    (r.kind == RowKind::Setting || r.kind == RowKind::Remap) && pc_settings_locked(r.desc->key);
                ImGui::PushID(i);
                if (locked) {
                    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
                }
                const std::string label = rowLabel(r);
                if (ImGui::Selectable(label.c_str(), i == row, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (row == i) {
                        activate(r, r.kind == RowKind::Setting || r.kind == RowKind::EditMode ? 1 : 0);
                    }
                    row = i;
                }
                ImGui::SameLine(valueX);
                const std::string value = rowValue(r);
                ImGui::TextColored(i == row ? kAccent : ImVec4(1, 1, 1, 1), "%s%s%s", i == row ? "< " : "",
                                   value.c_str(), i == row ? " >" : "");
                if (locked) {
                    ImGui::PopStyleColor();
                }
                const std::string note = rowNote(r);
                if (!note.empty()) {
                    // A short mark here; the whole note is in the help line of the selected row.
                    const bool pending = r.kind == RowKind::Setting && pc_settings_restart_pending(r.desc->key);
                    ImGui::SameLine();
                    ImGui::TextColored(locked ? kWarn : kDim, "%s",
                                       locked    ? T("[fijo]", "[set]")
                                       : pending ? T("[al reiniciar]", "[on restart]")
                                       : (r.desc->flags & PC_SETTING_RESTART) != 0 ? T("[reinicio]", "[restart]")
                                                                                   : "*");
                }
                if (i == row && m.scrollToRow) {
                    ImGui::SetScrollHereY(0.5f);
                }
                ImGui::PopID();
            }
            if (!rows.empty()) {
                help = rowHelp(rows[row]);
                const std::string note = rowNote(rows[row]);
                if (!note.empty()) {
                    help = "(" + note + ") " + help;
                }
            }
        }
        m.scrollToRow = false;
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (!m.toast.empty() && pc_frame_count() < m.toastUntil) {
        ImGui::TextColored(kAccent, "%s", m.toast.c_str());
    } else {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(help.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() -
                         ImGui::GetStyle().WindowPadding.y);
    ImGui::TextColored(kDim, "%s",
                       kSwitch ? T("A: cambiar   B: cerrar   L/R: pestaña   Menos: cerrar",
                                   "A: change   B: close   L/R: tab   Minus: close")
                               : T("A/Intro: cambiar   B/Esc: cerrar   L/R, Q/E: pestaña   F1: cerrar",
                                   "A/Enter: change   B/Esc: close   L/R, Q/E: tab   F1: close"));
    ImGui::End();
}

// The update popup (updateFrame): a small window over the title or the file select.
void drawPopup() {
    PcUpdateInfo info;
    pc_update_get_info(&info);
    ImGuiIO& io = ImGui::GetIO();
    const float scale = std::max(1.0f, io.DisplaySize.y / 720.0f * 1.7f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x * 0.9f, 560.0f * scale), 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::Begin("##cos_update_popup", nullptr, kFlags)) {
        ImGui::End();
        return;
    }
    ImGui::SetWindowFontScale(scale);
    ImGui::SetWindowFocus();
    ImGui::TextColored(kAccent, "%s", T("Hay una versión nueva de SwitchWaker", "A new SwitchWaker version is out"));
    ImGui::Separator();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s", info.title[0] != '\0' ? info.title : info.latest);
    ImGui::TextColored(kDim, T("Tienes la %s. La nueva es la %s.", "You have %s. The new one is %s."), info.current,
                       info.latest[0] == 'v' ? info.latest + 1 : info.latest);
    if (!info.canInstall) {
        ImGui::TextColored(kWarn, "%s", T("Esta copia no puede actualizarse sola: copia la carpeta switch del zip de la "
                                          "versión sobre la tarjeta SD.",
                                          "This copy cannot update itself: copy the release zip's switch folder onto the "
                                          "SD card."));
    }
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    const char* choices[kPopupChoices] = {T("Actualizar ahora", "Update now"),
                                          T("Ver notas de la versión", "Read the release notes"), T("Más tarde", "Later"),
                                          T("No avisar de esta versión", "Don't tell me about this version")};
    for (int i = 0; i < kPopupChoices; i++) {
        const bool sel = i == m.popupRow;
        const bool usable = popupUsable(i, info);
        ImGui::TextColored(!usable ? kDim : sel ? kAccent : ImVec4(1, 1, 1, 1), "%s%s%s", sel ? "> " : "  ", choices[i],
                           sel ? " <" : "");
    }
    ImGui::Spacing();
    ImGui::TextColored(kDim, "%s", kSwitch ? T("A: elegir   B: más tarde", "A: choose   B: later")
                                           : T("A/Intro: elegir   B/Esc: más tarde", "A/Enter: choose   B/Esc: later"));
    ImGui::End();
}

// A short line at the top of the screen (updateFrame's notices), menu open or not.
void drawNotice() {
    if (m.notice.empty() || pc_frame_count() >= m.noticeUntil) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    const float scale = std::max(1.0f, io.DisplaySize.y / 720.0f * 1.5f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.04f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
                                        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("##cos_update_notice", nullptr, kFlags)) {
        ImGui::SetWindowFontScale(scale);
        ImGui::TextColored(kAccent, "%s", m.notice.c_str());
    }
    ImGui::End();
}

// ---- COS_SMOKE=options-menu ----------------------------------------------------------------------

struct Expect {
    std::string kind;
    std::string a;
    std::string b;
};

struct Smoke {
    bool on = false;
    unsigned int endFrame = 0;
    std::vector<Expect> expects;
    unsigned int errors = 0;
    bool havePos = false;
    float pos[3] = {};
    unsigned int openFrames = 0;
    unsigned int pausedChecks = 0;
    bool runCard = false; // "#card run"
    bool title = false;   // "#title"
} sSmoke;

void smokeError(const char* fmt, const std::string& a, const std::string& b = "") {
    sSmoke.errors++;
    char line[512];
    snprintf(line, sizeof(line), fmt, a.c_str(), b.c_str());
    writef(STDERR_FILENO, "[cos] options-menu: FAIL %s\n", line);
}

// "#expect <kind> <a> [<b>]" lines of the COS_INPUT script:
//   end <frame>              the test ends there (exit 0 if every check held)
//   opened <n>               the menu was opened n times
//   setting <KEY> <value>    pc_settings_get(KEY) (the value in effect)
//   saved <line>             the settings file has this line (e.g. COS_FB_SCALE@handheld=1.5)
//   efb-height <h>           the EFB's pixel height (pc_efb_pixel_size of 640x480)
//   stage <name> <room>      the PLAY scene's stage and room
//   overlay <0|1>            the FPS overlay is shown
//   alloc-failures <n>       JKR allocations that failed since the start (bug B8: 0)
//   riding <0|1>             the player rides the boat with the sail on X (Depuración > Navegar)
//   no-se <id>[,<id>...]     JAudio has none of these sound effects (hex ids) registered (pc_audio_state.cpp)
//   card-roundtrip <stage> [<room>]  the game saved through its save screen: the card's save file (the
//                            run's own card, "#card run") loads back (mDoMemCd_Load, checksum of
//                            the file the game uses, card_to_memory) with the items the player has now
//                            and the return place <stage> (and <room>) (bug B8)
// Other lines: "#items <hex,...>" gives the new file of the debug boot those items
// (COS_BOOT_ITEMS); "#card run" makes memory card A the empty folder <run dir>/card/, so the game
// saves there and never on the user's card.
// Always: while the menu is open in the PLAY scene, the player does not move (paused, no input) - the
// script should hold the stick then.
void smokeParse() {
    FILE* f = gConfig.input != nullptr ? fopen(gConfig.input, "r") : nullptr;
    if (f == nullptr) {
        writef(STDERR_FILENO, "[cos] options-menu: needs COS_INPUT with #expect lines\n");
        pc_exit(PC_EXIT_USAGE);
    }
    char line[512];
    while (fgets(line, sizeof(line), f) != nullptr) {
        if (strncmp(line, "#items ", 7) == 0) {
            char items[256] = {};
            if (sscanf(line + 7, "%255s", items) == 1) {
                setenv("COS_BOOT_ITEMS", items, 1);
            }
            continue;
        }
        if (strncmp(line, "#card run", 9) == 0) {
            sSmoke.runCard = true;
            continue;
        }
        if (strncmp(line, "#title", 6) == 0) {
            sSmoke.title = true; // the menu is only used outside the PLAY scene: no pause to check
            continue;
        }
        char rel[256] = {};
        if (sscanf(line, "#update-release %255s", rel) == 1) {
            // pc_update.h: the release fixture next to the script (native/check/update), checked as
            // version 0.4.2, installed into the run directory
            std::string dir = gConfig.input;
            dir = dir.substr(0, dir.rfind('/') + 1) + rel;
            setenv("COS_UPDATE_URL", ("file://" + dir + "/latest.json").c_str(), 1);
            setenv("COS_UPDATE_VERSION", "0.4.2", 1);
            const char* run = getenv("COS_RUN_DIR");
            std::string sd = std::string(run != nullptr ? run : ".") + "/sd";
            for (const char* part : {"", "/switch", "/switch/switchwaker"}) {
                mkdir((sd + part).c_str(), 0777);
            }
            setenv("COS_UPDATE_DIR", (sd + "/switch/switchwaker").c_str(), 1);
            continue;
        }
        if (strncmp(line, "#expect ", 8) != 0) {
            continue;
        }
        char kind[64] = {}, a[256] = {}, b[256] = {};
        int bAt = 0;
        if (sscanf(line + 8, "%63s %255s %n", kind, a, &bAt) < 2) {
            continue;
        }
        // the rest of the line (update-file's text may hold spaces)
        snprintf(b, sizeof(b), "%s", bAt > 0 ? line + 8 + bAt : "");
        for (size_t n = strlen(b); n > 0 && (b[n - 1] == '\n' || b[n - 1] == '\r' || b[n - 1] == ' '); n--) {
            b[n - 1] = '\0';
        }
        if (strcmp(kind, "end") == 0) {
            sSmoke.endFrame = (unsigned int)atoi(a);
        } else {
            sSmoke.expects.push_back({kind, a, b});
        }
    }
    fclose(f);
    if (sSmoke.endFrame == 0) {
        writef(STDERR_FILENO, "[cos] options-menu: the script has no \"#expect end <frame>\"\n");
        pc_exit(PC_EXIT_USAGE);
    }
    writef(STDERR_FILENO, "[cos] options-menu: %zu checks at frame %u; settings file %s\n", sSmoke.expects.size(),
           sSmoke.endFrame, pc_settings_path());
}

// "#expect card-roundtrip <stage>": the save file the game wrote loads back with the items the player has
// now and the return place `stage`. Blocks the game thread while the card thread reads (as the
// file select's load does over several frames); only at the test's end frame.
void checkCardRoundtrip(const std::string& stage, const std::string& room) {
    alignas(32) static u8 sLoaded[3 * sizeof(card_gamedata)];
    u8 itemsNow[dInvSlot_ItemLast_e];
    for (int slot = 0; slot < dInvSlot_ItemLast_e; slot++) {
        itemsNow[slot] = dComIfGs_getItem(slot);
    }
    const int dataNum = dComIfGs_getDataNum();
    if (!sSmoke.runCard || runCardGciPath() == nullptr || access(runCardGciPath(), R_OK) != 0) {
        smokeError("card-roundtrip: no save file %s%s (needs \"#card run\" and a save through the save screen)",
                   runCardGciPath() != nullptr ? runCardGciPath() : "(no run card)");
        return;
    }
    auto wait = [](const char* what) {
        for (int i = 0; i < 5000 && !mDoMemCd_isCardCommNone(); i++) {
            usleep(2000);
        }
        if (!mDoMemCd_isCardCommNone()) {
            smokeError("card-roundtrip: %s did not finish in 10 s%s", what);
            return false;
        }
        return true;
    };
    if (!wait("the card thread's last command")) {
        return;
    }
    memset(sLoaded, 0, sizeof(sLoaded));
    mDoMemCd_Load();
    if (!wait("the load")) {
        return;
    }
    const u32 load = mDoMemCd_LoadSync(sLoaded, sizeof(sLoaded), 0);
    if (load != 1) {
        smokeError("card-roundtrip: LoadSync %s%s", std::to_string(load));
        return;
    }
    if (!mDoMemCdRWm_TestCheckSumGameData(&sLoaded[dataNum * sizeof(card_gamedata)])) {
        smokeError("card-roundtrip: file %s fails its checksum%s", std::to_string(dataNum + 1));
        return;
    }
    dComIfGs_setCardToMemory(sLoaded, dataNum);
    int same = 0;
    for (int slot = 0; slot < dInvSlot_ItemLast_e; slot++) {
        if (dComIfGs_getItem(slot) != itemsNow[slot]) {
            char what[96];
            snprintf(what, sizeof(what), "slot %d: 0x%02x, had 0x%02x", slot, dComIfGs_getItem(slot), itemsNow[slot]);
            smokeError("card-roundtrip: loaded item %s%s", what);
        } else if (itemsNow[slot] != 0xFF) {
            same++;
        }
    }
    dSv_player_return_place_c& place = g_dComIfG_gameInfo.save.getPlayer().getPlayerReturnPlace();
    if (stage != place.getName() || (!room.empty() && place.getRoomNo() != atoi(room.c_str()))) {
        smokeError("card-roundtrip: return place %s, expected %s",
                   std::string(place.getName()) + " room " + std::to_string(place.getRoomNo()),
                   stage + (room.empty() ? "" : " room " + room));
    }
    writef(STDERR_FILENO, "[cos] options-menu: card-roundtrip: file %d loaded back, %d items as saved, return "
                          "place %s room %d point %d\n",
           dataNum + 1, same, place.getName(), place.getRoomNo(), place.getPoint());
}

bool fileHasLine(const std::string& want) {
    FILE* f = fopen(pc_settings_path(), "r");
    if (f == nullptr) {
        return false;
    }
    char line[512];
    bool found = false;
    while (!found && fgets(line, sizeof(line), f) != nullptr) {
        line[strcspn(line, "\r\n")] = '\0';
        found = want == line;
    }
    fclose(f);
    return found;
}

void smokeFrame(unsigned int frame) {
    if (m.open && inPlay()) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        if (player != nullptr) {
            const cXyz& p = player->current.pos;
            if (sSmoke.havePos && sSmoke.openFrames >= 3) {
                const float d = std::fabs(p.x - sSmoke.pos[0]) + std::fabs(p.y - sSmoke.pos[1]) +
                                std::fabs(p.z - sSmoke.pos[2]);
                sSmoke.pausedChecks++;
                if (d > 0.01f) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%u (moved %.3f)", frame, (double)d);
                    smokeError("the player moved while the menu was open at frame %s%s", buf);
                }
            }
            sSmoke.pos[0] = p.x;
            sSmoke.pos[1] = p.y;
            sSmoke.pos[2] = p.z;
            sSmoke.havePos = true;
        }
        sSmoke.openFrames++;
    } else {
        sSmoke.havePos = false;
        sSmoke.openFrames = 0;
    }
    if (frame < sSmoke.endFrame) {
        return;
    }
    for (const Expect& e : sSmoke.expects) {
        if (e.kind == "opened") {
            if (m.opened != (unsigned int)atoi(e.a.c_str())) {
                smokeError("opened %s times, expected %s", std::to_string(m.opened), e.a);
            }
        } else if (e.kind == "setting") {
            const std::string v = pc_settings_get(e.a.c_str());
            if (v != e.b) {
                smokeError("%s", e.a + " = " + v, "");
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok %s = %s\n", e.a.c_str(), v.c_str());
            }
        } else if (e.kind == "saved") {
            if (!fileHasLine(e.a)) {
                smokeError("the settings file has no line %s%s", e.a);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok file line %s\n", e.a.c_str());
            }
        } else if (e.kind == "efb-height") {
            unsigned int w = 0, h = 0;
            pc_efb_pixel_size(640, 480, &w, &h);
            if (h != (unsigned int)atoi(e.a.c_str())) {
                smokeError("EFB height %s, expected %s", std::to_string(h), e.a);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok EFB %ux%u\n", w, h);
            }
        } else if (e.kind == "stage") {
            const char* name = inPlay() ? dComIfGp_getStartStageName() : "(not in PLAY)";
            const int room = inPlay() ? dComIfGp_getStartStageRoomNo() : -1;
            if (e.a != name || room != atoi(e.b.c_str())) {
                smokeError("stage %s, expected %s", std::string(name) + " " + std::to_string(room), e.a + " " + e.b);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok stage %s room %d\n", name, room);
            }
        } else if (e.kind == "riding") {
            // The sailing preset (Navegar): the player on the boat, the sail on X.
            const bool riding = inPlay() && dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e) != 0 &&
                                dComIfGp_getShipActor() != nullptr;
            const bool sail = dComIfGs_getSelectItem(dItemBtn_X_e) != dInvSlot_NONE_e &&
                              dComIfGs_getItem(dComIfGs_getSelectItem(dItemBtn_X_e)) == dItemNo_SAIL_e;
            if ((riding && sail ? 1 : 0) != atoi(e.a.c_str())) {
                smokeError("riding %s, expected %s", std::string(riding ? "1" : "0") + (sail ? " (sail on X)" : " (no sail on X)"),
                           e.a);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok riding %s\n", e.a.c_str());
            }
        } else if (e.kind == "overlay") {
            if ((gConfig.fpsOverlay ? 1 : 0) != atoi(e.a.c_str())) {
                smokeError("overlay %s, expected %s", gConfig.fpsOverlay ? "1" : "0", e.a);
            }
        } else if (e.kind == "alloc-failures") {
            if (heapAllocFailures() != (unsigned int)atoi(e.a.c_str())) {
                smokeError("%s JKR allocations failed, expected %s", std::to_string(heapAllocFailures()), e.a);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok %u failed JKR allocations\n", heapAllocFailures());
            }
        } else if (e.kind == "card-roundtrip") {
            checkCardRoundtrip(e.a, e.b);
        } else if (e.kind == "no-se") {
            for (const char* p = e.a.c_str(); *p != '\0';) {
                char* end = nullptr;
                const u32 id = (u32)strtoul(p, &end, 16);
                if (end == p) {
                    smokeError("no-se: bad id list %s%s", e.a);
                    break;
                }
                const int n = audioSeRegistered(id);
                char idText[16];
                snprintf(idText, sizeof(idText), "%04x", (unsigned)id);
                if (n != 0) {
                    char line[1536];
                    audioStateLine(line, sizeof(line));
                    smokeError("sound effect %s still registered (%s)", idText, line);
                } else {
                    writef(STDERR_FILENO, "[cos] options-menu: ok no sound effect %s\n", idText);
                }
                p = *end == ',' ? end + 1 : end;
            }
        } else if (e.kind == "update") {
            // pc_update.h's state by name: available, installed, install-failed, up-to-date, ...
            static const char* const kNames[] = {"idle",        "disabled",   "checking",  "up-to-date",
                                                 "available",   "check-failed", "downloading", "installing",
                                                 "installed",   "install-failed"};
            PcUpdateInfo info;
            pc_update_get_info(&info);
            const char* name = (unsigned)info.status < sizeof(kNames) / sizeof(kNames[0]) ? kNames[info.status] : "?";
            if (e.a != name) {
                smokeError("update %s, expected %s", std::string(name) + " (" + info.detail + ")", e.a);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok update %s %s\n", name, info.latest);
            }
        } else if (e.kind == "notes-page") {
            // the release notes page is open (1) or not (0), with these notes' first heading
            const std::string notes = updateNotes();
            const bool ok = (m.notesPage ? 1 : 0) == atoi(e.a.c_str()) && (e.b.empty() || notes.find("# " + e.b) != std::string::npos);
            if (!ok) {
                smokeError("notes page %s, expected %s", std::string(m.notesPage ? "1" : "0") + " notes=" + notes, e.a + " " + e.b);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok notes page %s\n", e.a.c_str());
            }
        } else if (e.kind == "update-file") {
            // a file of the install directory (COS_UPDATE_DIR) whose text starts with this ("-": absent)
            const char* dir = getenv("COS_UPDATE_DIR");
            const std::string path = std::string(dir != nullptr ? dir : "") + "/" + e.a;
            std::string text;
            bool present = false;
            if (FILE* f = fopen(path.c_str(), "rb")) {
                present = true;
                char buf[256];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                    text.append(buf, n);
                }
                fclose(f);
            }
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
                text.pop_back();
            }
            const bool ok = e.b == "-" ? !present : present && text.rfind(e.b, 0) == 0;
            if (!ok) {
                smokeError("update file %s, expected %s", e.a + (present ? " = " + text : " absent"), e.b);
            } else {
                writef(STDERR_FILENO, "[cos] options-menu: ok update file %s %s\n", e.a.c_str(), e.b.c_str());
            }
        } else {
            smokeError("unknown #expect %s%s", e.kind);
        }
    }
    if (sSmoke.pausedChecks == 0 && m.opened > 0 && !sSmoke.title) {
        smokeError("no frame with the menu open in the PLAY scene was checked%s%s", "");
    }
    writef(STDERR_FILENO, "[cos] options-menu: %s at frame %u (%u errors, %u paused frames checked)\n",
           sSmoke.errors == 0 ? "PASS" : "FAIL", frame, sSmoke.errors, sSmoke.pausedChecks);
    pc_exit(sSmoke.errors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void saveShot(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height, void*) {
    saveFramePng(frame, rgb, width, height);
}

} // namespace

bool menuBuiltinSetting(const char* key) {
    for (const PcSettingDesc& d : kBuiltins) {
        if (key != nullptr && strcmp(d.key, key) == 0) {
            return true;
        }
    }
    return false;
}

bool menuSmokeWantsRunCard() {
    return sSmoke.on && sSmoke.runCard;
}

void menuInit() {
    if (m.initialized) {
        return;
    }
    m.initialized = true;
    for (const PcSettingDesc& d : kBuiltins) {
        // A module that registered the key already (e.g. the HD texture loader) keeps its row.
        bool known = false;
        for (int i = 0; i < pc_settings_count(); i++) {
            known = known || strcmp(pc_settings_at(i)->key, d.key) == 0;
        }
        if (!known) {
            pc_settings_register(&d);
        }
    }
    m.editMode = pc_settings_mode();
    sSmoke.on = gConfig.smoke != nullptr && strcmp(gConfig.smoke, "options-menu") == 0;
    if (sSmoke.on) {
        smokeParse();
    }
    writef(STDERR_FILENO, "[cos] menu: %d settings; open with %s; settings file %s\n", pc_settings_count(),
           kSwitch ? "Minus" : "F1 or L+R+Z", pc_settings_path());
}

bool menuOpen() {
    return m.open;
}

void menuSmokeState(int* tab, int* row, int* captureAction) {
    *tab = m.tab;
    *row = m.row[m.tab];
    *captureAction = m.captureAction;
}

void menuFrame() {
    if (!m.initialized || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    {
        // 60 fps step E: a docked/handheld change applies the other mode's values (internal
        // resolution, 60 fps, dynres) and resizes the swapchain: presented once around it.
        const PcOperationMode before = pc_settings_mode();
        pc_settings_poll_mode();
        if (pc_settings_mode() != before) {
            pc_fps60_hold("operation mode changed");
        }
    }
    // The COS_INPUT script drives the menu only in its own smoke test: other scripts press L+R+Z
    // for the game (pad-echo).
    const uint32_t keys = (sSmoke.on ? readScript() : 0) | readGamepads() | readKeyboard();
    captureFrame();
    updateFrame();
    if (m.popup) {
        popupInput(keys);
    } else {
        handleInput(keys);
    }
    drawNotice();
    if (m.popup) {
        drawPopup();
    }
    if (!m.open) {
        return;
    }
    if (inPlay()) {
        // The hit-stop pause of the PLAY scene, renewed every frame while the menu is open
        // (dScnPly_Execute returns early while it runs; it ends two frames after the last renewal).
        dScnPly_ply_c::setPauseTimer(2);
    }
    drawMenu();
}

// ---- the Switch debug server (switch/native/source/cos_debug.cpp): its thread asks, the game thread acts
std::mutex sDebugMu;
struct DebugWarp {
    bool pending = false;
    char stage[8] = {};
    int room = 0, point = 0;
    int layer = -1;  // -1: the game's own choice (time of day, story)
} sDebugWarp;
// "event NAME": an event of the stage's event list ordered as the event sweep orders it (the player as the
// ordering actor, pc_event_sweep.cpp), every frame until an event runs, kDebugEventFrames at most. For
// testing a cutscene without a save at that point (issue #3: warp Pjavdou 0 1 8, then event getperl_jab).
constexpr unsigned int kDebugEventFrames = 240;
struct DebugEvent {
    bool pending = false;
    char name[32] = {};
    unsigned int frames = 0;
} sDebugEvent;
// "give NAME|ITEM ...": items through the game's own item-get functions (execItemGet, d_item.cpp: the
// inventory slot, the "got it" bit, the song bits, the bomb count), applied by the game thread once a file
// is played and the player exists, as a chest or a shop would. For testing the QoL options on the player's
// own save (docs/DEBUG_SERVER.md).
struct DebugGive {
    int count = 0;
    u8 items[16] = {};
} sDebugGive;
// "time": the hour (0-24) set at the next frame end of a file being played (< 0: none); "take": the
// dungeon items taken away there (bit 0 the map, bit 1 the compass)
struct DebugTime {
    float hour = -1.0f;
    unsigned take = 0;
    bool clear = false; // "weather clear": the sea's dice weather back to sunny
} sDebugTime;
// "audio": a pc_audio_state.cpp line taken by the game thread at its next frame end (ticket = the request
// count, done = the last request answered).
struct DebugAudio {
    unsigned int requested = 0;
    unsigned int done = 0;
    char line[1536] = {};
} sDebugAudio;
struct DebugStatus {
    unsigned int frame = 0;
    bool play = false;
    char stage[8] = {};
    int room = -1;
} sDebugStatus;

// The names "give" takes (item numbers: d_item_data.h). The Grappling Hook (0x25, d_a_himo2: what
// "Garfio más rápido" changes) is not the Hookshot (0x2F).
struct DebugItemName {
    const char* name;
    u8 item;
};
const DebugItemName kDebugItemNames[] = {
    {"grapple", dItemNo_GRAPPLING_HOOK_e},   {"grappling", dItemNo_GRAPPLING_HOOK_e},
    {"garfio", dItemNo_GRAPPLING_HOOK_e},    {"hookshot", dItemNo_HOOKSHOT_e},
    {"gancho", dItemNo_HOOKSHOT_e},          {"baton", dItemNo_WIND_WAKER_e},
    {"windwaker", dItemNo_WIND_WAKER_e},     {"batuta", dItemNo_WIND_WAKER_e},
    {"requiem", dItemNo_WINDS_REQUIEM_e},    {"ballad", dItemNo_BALLAD_OF_GALES_e},
    {"balada", dItemNo_BALLAD_OF_GALES_e},   {"bombs", dItemNo_BOMB_BAG_e},
    {"bombas", dItemNo_BOMB_BAG_e},          {"sail", dItemNo_SAIL_e},
    {"vela", dItemNo_SAIL_e},                {"telescope", dItemNo_TELESCOPE_e},
    {"bow", dItemNo_BOW_e},                  {"arco", dItemNo_BOW_e},
    {"map", dItemNo_MAP_e},                  {"mapa", dItemNo_MAP_e},
    {"compass", dItemNo_COMPASS_e},          {"brujula", dItemNo_COMPASS_e},
};

const char* debugItemName(u8 item) {
    for (const DebugItemName& n : kDebugItemNames) {
        if (n.item == item) {
            return n.name;
        }
    }
    return "by number";
}

void debugFrameEnd(unsigned int frame) {
    const bool play = inPlay();
    std::lock_guard<std::mutex> lk(sDebugMu);
    sDebugStatus.frame = frame;
    sDebugStatus.play = play;
    snprintf(sDebugStatus.stage, sizeof(sDebugStatus.stage), "%s", play ? dComIfGp_getStartStageName() : "");
    sDebugStatus.room = play ? dComIfGp_getStartStageRoomNo() : -1;
    // a warp waits for the game (a file being played) and for no scene change in progress
    if (sDebugWarp.pending && play && !dComIfGp_isEnableNextStage()) {
        sDebugWarp.pending = false;
        setOpen(false);
        noteWarp();
        writef(STDERR_FILENO, "[cos] debug server: warp to %s room %d point %d layer %d\n", sDebugWarp.stage,
               sDebugWarp.room, sDebugWarp.point, sDebugWarp.layer);
        dComIfGp_setNextStage(sDebugWarp.stage, (s16)sDebugWarp.point, (s8)sDebugWarp.room, (s8)sDebugWarp.layer);
    }
    if (sDebugGive.count > 0 && play && !dComIfGp_isEnableNextStage() && dComIfGp_getPlayer(0) != nullptr) {
        for (int i = 0; i < sDebugGive.count; i++) {
            u8 item = sDebugGive.items[i];
            if (item == dItemNo_BOMB_BAG_e && dComIfGs_getItem(dInvSlot_BOMB_e) != dItemNo_NONE_e) {
                item = dItemNo_BOMB_30_e; // the bag would set a bigger bag back to 30
            }
            execItemGet(item);
            // the game's own "does the player have it" check for that item (-1: none for this item)
            writef(STDERR_FILENO, "[cos] debug server: gave item 0x%02X (%s); the game's check says %d; bombs %u/%u\n",
                   item, debugItemName(item), (int)checkItemGet(item, -1), (unsigned)dComIfGs_getBombNum(),
                   (unsigned)dComIfGs_getBombMax());
        }
        sDebugGive.count = 0;
    }
    if (sDebugTime.hour >= 0.0f && play) {
        // the save's time, which dScnKy_env_light_c::setDaytime copies to mCurTime every frame (15 degrees an hour)
        dComIfGs_setTime(sDebugTime.hour * 15.0f);
        writef(STDERR_FILENO, "[cos] debug server: time set to %.2f h\n", sDebugTime.hour);
        sDebugTime.hour = -1.0f;
    }
    if (sDebugTime.clear && play) {
        // the sea's dice weather (d_kankyo.cpp dKy_wether_dice: every pattern starts cloudy or rainy, and a
        // jump in time starts one): sunny, the pattern ended, the next roll 179 degrees (about 12 h) away
        dScnKy_env_light_c& env = dKy_getEnvlight();
        env.mDiceWeatherMode = DICE_MODE_SUNNY_e;
        env.mDiceWeatherState = DICE_STATE_RESET_e;
        env.mDiceWeatherChangeTime = fmodf(dComIfGs_getTime() + 179.0f, 360.0f);
        writef(STDERR_FILENO, "[cos] debug server: weather clear (next roll at %.1f h)\n",
               env.mDiceWeatherChangeTime / 15.0f);
        sDebugTime.clear = false;
    }
    if (sDebugTime.take != 0 && play) {
        if (sDebugTime.take & 1) {
            dComIfGs_offDungeonItemMap();
        }
        if (sDebugTime.take & 2) {
            dComIfGs_offDungeonItemCompass();
        }
        writef(STDERR_FILENO, "[cos] debug server: took the stage's%s%s; map %d compass %d\n",
               (sDebugTime.take & 1) ? " map" : "", (sDebugTime.take & 2) ? " compass" : "",
               (int)dComIfGs_isDungeonItemMap(), (int)dComIfGs_isDungeonItemCompass());
        sDebugTime.take = 0;
    }
    if (sDebugAudio.requested != sDebugAudio.done) {
        audioStateLine(sDebugAudio.line, sizeof(sDebugAudio.line));
        sDebugAudio.done = sDebugAudio.requested;
        writef(STDERR_FILENO, "[cos] debug server: audio %s\n", sDebugAudio.line);
    }
    if (sDebugEvent.pending && play && !dComIfGp_isEnableNextStage() && dComIfGp_getPlayer(0) != nullptr) {
        if (sDebugEvent.frames > 0 && dComIfGp_event_runCheck()) {
            sDebugEvent.pending = false;
            writef(STDERR_FILENO, "[cos] debug server: event %s running\n", sDebugEvent.name);
        } else if (sDebugEvent.frames++ >= kDebugEventFrames) {
            sDebugEvent.pending = false;
            writef(STDERR_FILENO, "[cos] debug server: event %s did not start\n", sDebugEvent.name);
        } else {
            const s16 idx = dComIfGp_evmng_getEventIdx(sDebugEvent.name);
            if (idx < 0) {
                sDebugEvent.pending = false;
                writef(STDERR_FILENO, "[cos] debug server: no event %s in this stage's list\n", sDebugEvent.name);
            } else {
                // from the play scene's layer, as the sweep (a process made from the root layer is drawn twice)
                layer_class* saved = fpcLy_CurrentLayer();
                fpcLy_SetCurrentLayer(&static_cast<process_node_class*>((void*)fpcM_Search(isPlayScene, nullptr))->mLayer);
                fopAcM_orderOtherEventId(dComIfGp_getPlayer(0), idx, 0xFF, 0xFFFF, 1);
                fpcLy_SetCurrentLayer(saved);
            }
        }
    }
}

void menuFrameEnd(unsigned int frame) {
    debugFrameEnd(frame);
    warpAudioFrame();
    if (m.pendingShot) {
        m.pendingShot = false;
        if (captureFrame(frame, saveShot, nullptr)) {
            char name[64];
            snprintf(name, sizeof(name), T("Captura guardada: shot-%06u.png", "Screenshot saved: shot-%06u.png"), frame);
            toast(name);
        } else {
            toast(T("No se pudo capturar la pantalla", "Could not take the screenshot"));
        }
    }
    if (sSmoke.on) {
        smokeFrame(frame);
    }
}

} // namespace pc

// The warp list, numbered as the menu's Travel page: "n  stage room point  label" lines.
extern "C" int pc_debug_warp_list(char* out, size_t size) {
    size_t used = 0;
    for (int i = 0; i < pc::kMainWarpCount + pc::kAllWarpCount && used < size; i++) {
        const pc::Warp& w = pc::warpAt(i);
        const int n = snprintf(out + used, size - used, "%3d  %-8s room %2d point %3d  %s\n", i, w.stage, w.room, w.point,
                               w.labelEn != nullptr ? w.labelEn : "");
        if (n < 0) {
            break;
        }
        used += (size_t)n;
    }
    return (int)std::min(used, size);
}

// A warp from the debug server: index >= 0 picks the list's entry, else stage / room / point / layer (-1: the
// game's choice; e.g. 8 for Jabun's cave with Jabun, Pjavdou 0 1 8). The game thread applies it once a file is
// being played (pc_menu debugFrameEnd). 0: no such entry.
extern "C" int pc_debug_warp(int index, const char* stage, int room, int point, int layer, char* done, size_t size) {
    pc::Warp w{stage, room, point, nullptr, nullptr};
    if (index >= 0) {
        if (index >= pc::kMainWarpCount + pc::kAllWarpCount) {
            return 0;
        }
        w = pc::warpAt(index);
    }
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    pc::sDebugWarp.pending = true;
    snprintf(pc::sDebugWarp.stage, sizeof(pc::sDebugWarp.stage), "%s", w.stage);
    pc::sDebugWarp.room = w.room;
    pc::sDebugWarp.point = w.point;
    pc::sDebugWarp.layer = index >= 0 ? -1 : layer;
    snprintf(done, size, "%s room %d point %d layer %d", pc::sDebugWarp.stage, w.room, w.point, pc::sDebugWarp.layer);
    return 1;
}

// An event of the current stage's list, by name, from the debug server (see sDebugEvent). 0: name too long.
extern "C" int pc_debug_event(const char* name) {
    if (strlen(name) >= sizeof(pc::sDebugEvent.name)) {
        return 0;
    }
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    snprintf(pc::sDebugEvent.name, sizeof(pc::sDebugEvent.name), "%s", name);
    pc::sDebugEvent.frames = 0;
    pc::sDebugEvent.pending = true;
    return 1;
}

// "give": names or item numbers (decimal or 0x..), queued for the game thread (see sDebugGive). Writes
// what was queued (or the first unknown name) to `done`; 0: an unknown name or too many items.
// "bombs" is the bomb bag (30 bombs, room for 30); with bombs already, 30 more bombs instead (the bag
// would set the room back to 30).
extern "C" int pc_debug_give(const char* const* names, int count, char* done, size_t size) {
    u8 items[16];
    int n = 0;
    size_t len = 0;
    done[0] = '\0';
    for (int i = 0; i < count; i++) {
        int item = -1;
        for (const pc::DebugItemName& d : pc::kDebugItemNames) {
            if (strcasecmp(names[i], d.name) == 0) {
                item = d.item;
            }
        }
        if (item < 0) {
            char* end = nullptr;
            const long v = strtol(names[i], &end, 0);
            if (end != names[i] && *end == '\0' && v >= 0 && v < dItemNo_NONE_e) {
                item = (int)v;
            }
        }
        if (item < 0 || n >= (int)sizeof(items)) {
            snprintf(done, size, "%s", names[i]);
            return 0;
        }
        items[n++] = (u8)item;
        len += (size_t)snprintf(done + len, len < size ? size - len : 0, "%s%s=0x%02X", i > 0 ? " " : "", names[i], item);
    }
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    for (int i = 0; i < n && pc::sDebugGive.count < (int)sizeof(pc::sDebugGive.items); i++) {
        pc::sDebugGive.items[pc::sDebugGive.count++] = items[i];
    }
    return 1;
}

// "time": the hour of the day (0-24, decimals allowed), set once a file is being played; 0: out of range.
extern "C" int pc_debug_time(float hour) {
    if (!(hour >= 0.0f && hour <= 24.0f)) {
        return 0;
    }
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    pc::sDebugTime.hour = hour;
    return 1;
}

// "weather clear": the sea's weather back to sunny, applied once a file is being played.
extern "C" void pc_debug_weather_clear(void) {
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    pc::sDebugTime.clear = true;
}

// "take": the current stage's dungeon map and/or compass (the save bits "give map"/"give compass" set),
// taken once a file is being played; 0: an unknown name.
extern "C" int pc_debug_take(const char* const* names, int count) {
    unsigned take = 0;
    for (int i = 0; i < count; i++) {
        if (strcasecmp(names[i], "map") == 0 || strcasecmp(names[i], "mapa") == 0) {
            take |= 1;
        } else if (strcasecmp(names[i], "compass") == 0 || strcasecmp(names[i], "brujula") == 0) {
            take |= 2;
        } else {
            return 0;
        }
    }
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    pc::sDebugTime.take |= take;
    return 1;
}

// "audio": asks the game thread for a pc_audio_state.cpp line; returns its ticket (pc_debug_audio_done).
extern "C" unsigned int pc_debug_audio_request(void) {
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    return ++pc::sDebugAudio.requested;
}

// The line once the game thread answered `ticket` (1), else 0.
extern "C" int pc_debug_audio_done(unsigned int ticket, char* out, size_t size) {
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    if (pc::sDebugAudio.done < ticket) {
        return 0;
    }
    snprintf(out, size, "%s", pc::sDebugAudio.line);
    return 1;
}

// The game's state at the last frame end: frame, in play, stage, room.
extern "C" int pc_debug_status(char* out, size_t size) {
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    return snprintf(out, size, "frame %u\nstage %s room %d%s\n", pc::sDebugStatus.frame,
                    pc::sDebugStatus.play ? pc::sDebugStatus.stage : "-", pc::sDebugStatus.room,
                    pc::sDebugStatus.play ? "" : " (not playing a file)");
}
