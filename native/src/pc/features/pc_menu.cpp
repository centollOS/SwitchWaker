// The in-game options menu (native/include/pc/pc_settings.h; native/README.md, "Options menu").
//
// Opens with Minus on the Switch (any connected controller), L+R+Z on a GameCube-style pad or
// in a COS_INPUT script, or F1 on the Mac's keyboard; B (Esc), or the same combination, closes it.
// While it is open the game is paused (in the PLAY scene: the hit-stop pause timer of
// dScnPly_ply_c, which stops the actors, the camera, messages, the environment and scene changes)
// and gets no controller input (Aurora's PADBlockInput; the buttons still held when it closes are
// ignored until released). Other scenes (title, name entry) keep running behind it, without input.
//
// One ImGui window with three tabs, Gráficos, Rendimiento and Depuración, navigated by the
// controller (D-pad or stick: row and value; A: change / confirm; B: back / close; L/R: tab), the
// keyboard (arrows, Enter, Esc, Q/E) or the mouse (click a tab, a row, a value). The menu reads the
// controllers itself (SDL gamepads, labels of the face buttons respected, so A is A on a Switch-style
// layout too), plus the COS_INPUT script in COS_SMOKE=options-menu (only there: other scripts press
// L+R+Z for the game).
//
// Every row is a setting of pc_settings.h (the environment variable it already had) or an action
// (warp, screenshot, reload). Settings with PC_SETTING_RESTART apply at the next start; the others
// apply at once through the setters of pc_gpu_opts.h, pc_dynres.h, pc_main.cpp (COS_FB_SCALE),
// pc_frame.cpp (COS_PERF_EVERY) and the Switch layer (COS_SWITCH_GPU_PROFILE). The graphics settings
// marked per mode keep one value for handheld and one for docked; the "Perfil a editar" row picks
// which one the rows show and change, and pc_settings_poll_mode applies the other mode's values when
// the console is docked or undocked.
//
// COS_SMOKE=options-menu (with COS_INPUT and usually COS_BOOT_STAGE): drives the menu through the
// script and checks the "#expect" lines of the script file (see smokeParse below): the menu opened,
// the game stayed paused while it was open, the settings in effect, the lines of the settings file,
// the EFB size, the stage after a warp; exit 0 at the "#expect end" frame when all held.
#include "pc/pc_settings.h"
#include "pc/pc_dynres.h"
#include "pc/pc_gpu_opts.h"

#include "pc_internal.h"

#include "pc/pc_controls.h"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_save.h"
#include "d/d_item_data.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
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
#include <unistd.h>
#include <vector>

#if defined(__SWITCH__)
#include "cos_switch.h"
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
}
void applyDynres(const char*, const char* v, void*) {
    pc_dynres_configure(v, (float)atof(pc_settings_get("COS_FB_SCALE")));
}
void applyMist(const char*, const char* v, void*) { pc_mist_lowres_set(atoi(v)); }
void applySky(const char*, const char* v, void*) { pc_sky_lowres_set(atoi(v)); }
void applyDof(const char*, const char* v, void*) { pc_dof_set(strcmp(v, "0") != 0); }
void applyCameraInvertX(const char*, const char* v, void*) { pc_camera_invert_x_set(strcmp(v, "1") == 0); }
void applyCameraInvertY(const char*, const char* v, void*) { pc_camera_invert_y_set(strcmp(v, "1") == 0); }
void applyShadow(const char*, const char* v, void*) {
    pc_shadow_offscreen_set(strcmp(v, "1") == 0    ? PC_SHADOW_OFFSCREEN_SAME
                            : strcmp(v, "gc") == 0 ? PC_SHADOW_OFFSCREEN_GC
                                                   : PC_SHADOW_OFFSCREEN_OFF);
}
void applyGpuProfile(const char*, const char* v, void*) {
#if defined(__SWITCH__)
    cos_switch_set_gpu_profile(v);
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
void applyGpuGroups(const char*, const char* v, void*) { pc_gpu_groups_set(atoi(v)); }

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
const PcSettingChoice kHdMaxSize[] = {{"auto", "Automático (512 / 1024)", "Automatic (512 / 1024)"}, {"256", "256"}, {"512", "512"},
                                      {"1024", "1024"}, {"full", "Sin límite", "No limit"}};
const PcSettingChoice kDynres[] = {{"0", "Desactivada", "Off"}, {"1", "Automática", "Automatic"}};
const PcSettingChoice kLowres[] = {{"0", "Completa", "Full"}, {"2", "1/2"}, {"4", "1/4"}};
const PcSettingChoice kSkyLowres[] = {{"0", "Completa", "Full"}, {"2", "1/2"}};
const PcSettingChoice kShadow[] = {
    {"0", "En el EFB (como la GameCube)", "In the EFB (as on the GameCube)"}, {"1", "Fuera del EFB", "Outside the EFB"}, {"gc", "Fuera del EFB, 256x256", "Outside the EFB, 256x256"}};
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

const PcSettingDesc kBuiltins[] = {
    // Gráficos
    {"COS_FB_SCALE", "Resolución interna",
     "Resolución a la que se dibuja el juego antes de escalar a la pantalla. Más baja = más rápido.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kFbScale), kSwitch ? "1.5" : "0", applyFbScale,
     nullptr, 10,
     "Internal resolution",
     "Resolution the game is drawn at before scaling to the screen. Lower = faster."},
    {"COS_DYNRES", "Resolución dinámica",
     "Baja la resolución del 3D (1.25, 1.125) cuando la GPU no llega a 30 fps; el HUD queda nítido.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kDynres), "0", applyDynres, nullptr, 20,
     "Dynamic resolution",
     "Lowers the 3D resolution (1.25, 1.125) when the GPU cannot hold 30 fps; the HUD stays sharp."},
    {"COS_MIST_LOWRES", "Niebla del bosque",
     "Resolución de la niebla (bosques): 1/4 se ve casi igual y cuesta mucho menos.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kLowres), "4", applyMist, nullptr, 30,
     "Forest mist",
     "Resolution of the mist (forests): 1/4 looks almost the same and costs much less."},
    {"COS_SKY_LOWRES", "Cielo", "Resolución del cielo y las nubes: 1/2 ahorra GPU, bordes de nubes más suaves.",
     PC_SETTING_TAB_GRAPHICS, PC_SETTING_PER_MODE, CHOICES(kSkyLowres), "0", applySky, nullptr, 40,
     "Sky",
     "Resolution of the sky and clouds: 1/2 saves GPU time, with softer cloud edges."},
    {"COS_DOF", "Profundidad de campo", "Desenfoque del paisaje lejano, como en la GameCube.",
     PC_SETTING_TAB_GRAPHICS, 0, CHOICES(kOnOff), "1", applyDof, nullptr, 50,
     "Depth of field",
     "Blur of the distant scenery, as on the GameCube."},
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
    // Depuración
    {"COS_PERF_EVERY", "Intervalo del registro perf",
     "Cada cuántos cuadros se escribe una línea [cos] perf en el registro (native/logs/).",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kPerfEvery), "0", applyPerfEvery, nullptr, 105,
     "Perf log interval",
     "How many frames between [cos] perf lines in the log (native/logs/)."},
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
    {"COS_USB_LOG", "Registro en directo por USB",
     "Para desarrollo: envía el registro por USB a scripts/switch/usb_log.py mientras juegas. Ocupa el "
     "puerto USB (por ejemplo, SysDVR por USB no podrá usarlo); el registro en la tarjeta SD se escribe siempre.",
     PC_SETTING_TAB_DEBUG, PC_SETTING_RESTART | PC_SETTING_SWITCH_ONLY, CHOICES(kOnOff), "0", nullptr, nullptr, 120,
     "Live log over USB",
     "For development: sends the log over USB to scripts/switch/usb_log.py while you play. It holds "
     "the USB port (SysDVR over USB, for example, cannot use it); the log on the SD card is always written."},
    {"COS_SHADOW_OFFSCREEN", "Sombras en tiempo real (prueba A/B)",
     "Prueba de GPU: dónde se dibujan las sombras de los personajes; fuera del EFB evita cortar la pasada principal.",
     PC_SETTING_TAB_DEBUG, 0, CHOICES(kShadow), "0", applyShadow, nullptr, 130,
     "Real-time shadows (A/B test)",
     "GPU test: where character shadows are drawn; outside the EFB avoids splitting the main pass."},
};

// ---- menu state ----------------------------------------------------------------------------------

enum class RowKind { EditMode, Setting, Warp, Sailing, Screenshot, Reload };

struct Row {
    RowKind kind;
    const PcSettingDesc* desc = nullptr;
};

const char* const kTabNames[PC_SETTING_TABS] = {"Gráficos", "Rendimiento", "Depuración"};
const char* const kTabNamesEn[PC_SETTING_TABS] = {"Graphics", "Performance", "Debug"};

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
    if (tab == PC_SETTING_TAB_DEBUG) {
        rows.push_back({RowKind::Warp});
        rows.push_back({RowKind::Sailing});
        rows.push_back({RowKind::Screenshot});
        rows.push_back({RowKind::Reload});
    }
    for (int i = 0; i < pc_settings_count(); i++) {
        const PcSettingDesc* d = pc_settings_at(i);
        if (d != nullptr && d->tab == tab) {
            rows.push_back({RowKind::Setting, d});
        }
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
        toast(std::string(settingLabel(d)) +
              (kSwitch ? T(": fijado por env.txt", ": set in env.txt") : T(": fijado por el entorno", ": set by the environment")));
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
    m.waitRelease = true;
    PADBlockInput(open);
    if (open) {
        m.opened++;
        m.editMode = pc_settings_mode();
        m.scrollToRow = true;
        writef(STDERR_FILENO, "[cos] menu: open at frame %u (%s)\n", pc_frame_count(),
               inPlay() ? "game paused" : "not in the PLAY scene: the game runs on without input");
        heapReport("options menu opened");
    } else {
        writef(STDERR_FILENO, "[cos] menu: closed at frame %u\n", pc_frame_count());
    }
}

void doWarp(const Warp& w) {
    if (!inPlay()) {
        toast(T("El viaje solo funciona durante la partida", "Travel only works in the game"));
        return;
    }
    setOpen(false);
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
    writef(STDERR_FILENO, "[cos] menu: sailing preset, warp to %s room %d point %d\n", kSailingSpawn.stage,
           kSailingSpawn.room, kSailingSpawn.point);
    dComIfGp_setNextStage(kSailingSpawn.stage, (s16)kSailingSpawn.point, (s8)kSailingSpawn.room, -1);
}

void activate(const Row& r, int dir) {
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
    case RowKind::Reload:
        if (dir == 0) {
            pc_settings_reload();
            toast(std::string(T("Ajustes recargados de ", "Settings reloaded from ")) + pc_settings_path());
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
    if (m.waitRelease) {
        if ((keys & ~(kUp | kDown | kLeft | kRight)) == 0) {
            m.waitRelease = false;
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
        m.tab = (m.tab + ((p & kTabRight) ? 1 : PC_SETTING_TABS - 1)) % PC_SETTING_TABS;
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
    case RowKind::Setting: break;
    }
    return choiceLabel(r.desc, pc_settings_get_mode(r.desc->key, rowMode(r.desc)));
}

std::string rowNote(const Row& r) {
    if (r.kind != RowKind::Setting) {
        return "";
    }
    const PcSettingDesc* d = r.desc;
    if (pc_settings_locked(d->key)) {
        return kSwitch ? T("fijado por env.txt", "set in env.txt")
                       : T("fijado por variable de entorno", "set by an environment variable");
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
    case RowKind::Setting: break;
    }
    const char* descHelp = settingHelp(r.desc);
    std::string help = descHelp != nullptr ? descHelp : "";
    if (pc_settings_locked(r.desc->key)) {
        help += kSwitch ? T(" Fijado en env.txt: quítalo de allí para cambiarlo aquí.",
                            " Set in env.txt: remove it there to change it here.")
                        : T(" Fijado por una variable de entorno de esta ejecución.",
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
    const std::string mode = std::string(T("Modo: ", "Mode: ")) + modeName(pc_settings_mode());
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(mode.c_str()).x - ImGui::GetStyle().WindowPadding.x);
    ImGui::TextColored(kDim, "%s", mode.c_str());

    // Tabs.
    const float tabW = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / PC_SETTING_TABS;
    for (int t = 0; t < PC_SETTING_TABS; t++) {
        if (t > 0) {
            ImGui::SameLine();
        }
        const bool sel = t == m.tab;
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.20f, 0.42f, 0.70f, 1.0f) : ImVec4(0.16f, 0.18f, 0.22f, 1.0f));
        if (ImGui::Button(T(kTabNames[t], kTabNamesEn[t]), ImVec2(tabW, 0)) && !m.warpPage) {
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
        if (m.warpPage) {
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
                const bool locked = r.kind == RowKind::Setting && pc_settings_locked(r.desc->key);
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
        if (strncmp(line, "#expect ", 8) != 0) {
            continue;
        }
        char kind[64] = {}, a[256] = {}, b[256] = {};
        if (sscanf(line + 8, "%63s %255s %255s", kind, a, b) < 2) {
            continue;
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
        } else {
            smokeError("unknown #expect %s%s", e.kind);
        }
    }
    if (sSmoke.pausedChecks == 0 && m.opened > 0) {
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

void menuFrame() {
    if (!m.initialized || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    pc_settings_poll_mode();
    // The COS_INPUT script drives the menu only in its own smoke test: other scripts press L+R+Z
    // for the game (pad-echo).
    handleInput((sSmoke.on ? readScript() : 0) | readGamepads() | readKeyboard());
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
} sDebugWarp;
struct DebugStatus {
    unsigned int frame = 0;
    bool play = false;
    char stage[8] = {};
    int room = -1;
} sDebugStatus;

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
        writef(STDERR_FILENO, "[cos] debug server: warp to %s room %d point %d\n", sDebugWarp.stage, sDebugWarp.room,
               sDebugWarp.point);
        dComIfGp_setNextStage(sDebugWarp.stage, (s16)sDebugWarp.point, (s8)sDebugWarp.room, -1);
    }
}

void menuFrameEnd(unsigned int frame) {
    debugFrameEnd(frame);
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

// A warp from the debug server: index >= 0 picks the list's entry, else stage / room / point. The game thread
// applies it once a file is being played (pc_menu debugFrameEnd). 0: no such entry.
extern "C" int pc_debug_warp(int index, const char* stage, int room, int point, char* done, size_t size) {
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
    snprintf(done, size, "%s room %d point %d", pc::sDebugWarp.stage, w.room, w.point);
    return 1;
}

// The game's state at the last frame end: frame, in play, stage, room.
extern "C" int pc_debug_status(char* out, size_t size) {
    std::lock_guard<std::mutex> lk(pc::sDebugMu);
    return snprintf(out, size, "frame %u\nstage %s room %d%s\n", pc::sDebugStatus.frame,
                    pc::sDebugStatus.play ? pc::sDebugStatus.stage : "-", pc::sDebugStatus.room,
                    pc::sDebugStatus.play ? "" : " (not playing a file)");
}
