// The seed rows of the options menu's "Randomizer" tab (docs/RANDOMIZER_PLAN.md step R4.3; API and
// behaviour in pc_rando_menu.h). The generation itself runs in pc_rando_genjob.cpp.
#include "pc_rando_menu.h"

#include "pc_rando_fileselect.h"
#include "pc_rando_fsnewgame.h"
#include "pc_rando_genjob.h"
#include "pc_rando_options.h"
#include "pc_rando_settings.h"
#include "pc_rando_sidecars.h"
#include "pc_rando_slot.h"
#include "pc_rando_text_ui.h"

#include "pc/pc_harness.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_settings.h"

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

#include <chrono>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

// pc_jkr_heap.cpp (pc/game_hooks/jkr_heap.h, which needs JKRHeap.h): the worker's allocations are
// host memory, not blocks of the game's current JKRHeap.
void JKRPcBeginHostAlloc();
void JKRPcEndHostAlloc();

namespace pc {

bool pc_ui_spanish(); // pc_precompile.cpp (pc_internal.h): the menu's language
// pc_menu.cpp (pc_internal.h; R8.2): the tab's pages and whether a file is played.
void menuOpenPage(int tab, int page);
bool menuInPlay();

const char* const kRandoSeedKey = "COS_RANDO_SEED";

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

const char* L(bool es, const char* s, const char* e) {
    return es ? s : e;
}

// D16: the randomizer was USA only until R7.6; R7.6a opened it for the European disc (VERSION_PAL).
// The constant stays so a region can be closed again in one place per file.
constexpr bool kRegionOk = true;

bool sInit = false;
bool sBusy = false;
RandoGenLast sLast;
std::string sPermalinkStatus; // the permalink row's value after an OK ("" none)

std::string firstLine(const std::string& s) {
    const size_t e = s.find('\n');
    return e == std::string::npos ? s : s.substr(0, e);
}

std::string version() {
#if defined(__SWITCH__)
    return cos_switch_version();
#else
    return "dev";
#endif
}

uint64_t entropy() {
    const uint64_t a = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
    const uint64_t b = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    return a ^ (b * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)getpid() << 32);
}

std::string currentSeed() {
    return rando::textSanitize(rando::TextKind::Seed, pc_settings_get(kRandoSeedKey));
}

// Stores a seed name; false when the environment locks the setting.
bool storeSeed(const std::string& seed) {
    if (seed == pc_settings_get(kRandoSeedKey)) {
        return true;
    }
    return pc_settings_set(kRandoSeedKey, seed.c_str()) == 0;
}

// ---- Seed... ---------------------------------------------------------------------------------------

void seedTyped(bool ok, const char* text, void*) {
    if (!ok) {
        return;
    }
    if (!storeSeed(text)) {
        logf("[cos] rando menu: seed \"%s\" not stored: %s is set by the environment\n", text, kRandoSeedKey);
        return;
    }
    logf("[cos] rando menu: seed \"%s\"\n", text);
}

std::string seedValue(bool es) {
    const std::string s = currentSeed();
    return s.empty() ? L(es, "(aleatoria)", "(random)") : s;
}

std::string seedActivate(bool es) {
    if (pc_settings_locked(kRandoSeedKey)) {
        return L(es, "Semilla: fijada por el entorno", "Seed: set by the environment");
    }
    static std::string initial; // the request keeps the pointer
    initial = currentSeed();
    RandoTextRequest req;
    req.kind = rando::TextKind::Seed;
    req.titleEs = "Semilla";
    req.titleEn = "Seed";
    req.initial = initial.c_str();
    req.done = seedTyped;
    return randoTextOpen(req) ? "" : L(es, "Ya hay una entrada de texto abierta", "A text input is already open");
}

// ---- Random seed -----------------------------------------------------------------------------------

std::string randomActivate(bool es) {
    if (pc_settings_locked(kRandoSeedKey)) {
        return L(es, "Semilla: fijada por el entorno", "Seed: set by the environment");
    }
    const std::string s = pcr::randomSeedName(entropy());
    storeSeed(s);
    logf("[cos] rando menu: random seed \"%s\"\n", s.c_str());
    return std::string(L(es, "Semilla: ", "Seed: ")) + s;
}

// ---- Paste permalink -------------------------------------------------------------------------------

void permalinkTyped(bool ok, const char* text, void*) {
    if (!ok) {
        return;
    }
    std::string msg;
    randoApplyPermalink(text, pc_ui_spanish(), &msg);
    sPermalinkStatus = msg;
}

std::string permalinkValue(bool es) {
    return sPermalinkStatus.empty() ? L(es, "A: escribir", "A: type") : sPermalinkStatus;
}

std::string permalinkActivate(bool es) {
    // permalink.txt prefills the field (R4.2's import), so a permalink copied to the SD card only
    // needs OK.
    std::string path;
    const rando::PermalinkInfo info = randoPermalinkImport(&path);
    static std::string initial; // the request keeps the pointer
    initial = info.status == rando::PermalinkStatus::Ok ? info.text : std::string();
    RandoTextRequest req;
    req.kind = rando::TextKind::Permalink;
    req.titleEs = "Permalink";
    req.titleEn = "Permalink";
    req.initial = initial.c_str();
    req.done = permalinkTyped;
    if (!randoTextOpen(req)) {
        return L(es, "Ya hay una entrada de texto abierta", "A text input is already open");
    }
    return initial.empty() ? "" : L(es, "Permalink leído de permalink.txt", "Permalink read from permalink.txt");
}

// ---- page 0 (R8.2): the game being played -----------------------------------------------------------

const PcRandoSeed* activeSeed() {
    return pc_rando() ? pc_rando_slot_active_seed() : nullptr;
}

// A seed's "Name1 Name2" (pcr::seedHashWords encodes its permalink: cached per seed hash).
std::string hashWords(const PcRandoSeed& seed) {
    static uint32_t sKey = 0;
    static bool sHave = false;
    static std::string sWords;
    const uint32_t key = (uint32_t)pc_rando_seed_hash32(seed);
    if (!sHave || key != sKey) {
        sWords = pcr::seedHashWords(seed);
        sKey = key;
        sHave = true;
    }
    return sWords;
}

bool shownRando() {
    return activeSeed() != nullptr;
}

std::string gameValue(bool es) {
    switch (randoCurrentGame()) {
    case RandoGameKind::Rando: return "Randomizer";
    case RandoGameKind::Normal: return L(es, "Partida normal", "Normal game");
    case RandoGameKind::None: break;
    }
    return L(es, "Ninguna partida cargada", "No game loaded");
}

std::string activeSeedValue(bool) {
    const PcRandoSeed* s = activeSeed();
    return s != nullptr ? s->seed : "";
}

std::string activeHashValue(bool) {
    const PcRandoSeed* s = activeSeed();
    return s != nullptr ? hashWords(*s) : "";
}

std::string nextValue(bool es) {
    return L(es, "A: abrir", "A: open");
}

std::string nextActivate(bool) {
    menuOpenPage(PC_SETTING_TAB_RANDO, kRandoPageNext);
    return "";
}

// ---- Generate --------------------------------------------------------------------------------------

std::string generateValue(bool es) {
    if (sBusy) {
        const pcr::GenJobProgress p = pcr::genJobProgress();
        char b[96];
        if (p.total > 0) {
            snprintf(b, sizeof b, "%s %d%%", L(es, "Generando...", "Generating..."), p.done * 100 / p.total);
        } else {
            snprintf(b, sizeof b, "%s", L(es, "Generando...", "Generating..."));
        }
        return b;
    }
    if (const PcRandoSeed* pending = pc_rando_slot_pending(); pending != nullptr) {
        char h[16];
        snprintf(h, sizeof h, "%08x", (unsigned)pc_rando_seed_hash32(*pending));
        if (sLast.have && sLast.ok && sLast.hash32 == h) {
            return std::string(L(es, "Lista: ", "Ready: ")) + sLast.hash;
        }
        return std::string(L(es, "Lista: ", "Ready: ")) + pending->seed;
    }
    if (sLast.have && !sLast.ok) {
        return std::string("Error: ") + sLast.error;
    }
    return L(es, "A: generar", "A: generate");
}

std::string generateActivate(bool es) {
    std::string msg;
    randoGenerate(es, &msg);
    return msg;
}

void threadEnter() {
    JKRPcBeginHostAlloc();
}

void threadExit() {
    JKRPcEndHostAlloc();
}

} // namespace

void randoMenuInit() {
    if (sInit) {
        return;
    }
    sInit = true;
    static const PcSettingDesc seed = {
        kRandoSeedKey,
        "Semilla",
        "El nombre de la semilla del randomizer (vacío: una aleatoria al generar)",
        PC_SETTING_TAB_RANDO,
        0,
        nullptr,
        0,
        "",
        nullptr,
        nullptr,
        9500,
        "Seed",
        "The randomizer's seed name (empty: a random one when generating)",
    };
    pc_settings_register(&seed);

    pcr::GenJobThreadHooks hooks;
    hooks.enter = threadEnter;
    hooks.exit = threadExit;
    pcr::genJobSetThreadHooks(hooks);

    // R8.2: page 0, the game being played (R4.5's "Spoiler log..." at -100, pc_rando_hooks_spoiler.cpp).
    randoMenuAddAction({"Partida actual", "Current game",
                        "La partida que se está jugando: Randomizer, normal, o ninguna (título, selección de archivo)",
                        "The game being played: Randomizer, normal, or none (title screen, file select)", gameValue,
                        nullptr, -300});
    randoMenuAddAction({"Semilla", "Seed", "El nombre de la semilla de esta partida",
                        "The seed name of the game being played", activeSeedValue, nullptr, -290, shownRando});
    randoMenuAddAction({"Hash", "Hash",
                        "El hash de la semilla de esta partida (el que muestra wwrando en la selección de archivo)",
                        "The hash of the game's seed (the one wwrando shows on the file select)", activeHashValue,
                        nullptr, -280, shownRando});
    randoMenuAddAction({"Opciones de la siguiente partida...", "Next game options...",
                        "A: semilla, opciones y Generar: solo cuentan para la próxima partida Randomizer nueva",
                        "A: seed, options and Generate: they only act on the next new Randomizer file", nextValue,
                        nextActivate, -10});
    // Page 1, before the options (order < kRandoOrderLate), then Generate after them.
    randoMenuAddAction({"Semilla...", "Seed...",
                        "A: escribir el nombre de la semilla (vacío: una aleatoria al generar)",
                        "A: type the seed name (empty: a random one when generating)", seedValue, seedActivate,
                        10});
    randoMenuAddAction({"Semilla aleatoria", "Random seed", "A: un nombre de semilla aleatorio, como wwrando",
                        "A: a random seed name, as wwrando makes", nullptr, randomActivate, 20});
    randoMenuAddAction({"Pegar permalink", "Paste permalink",
                        "A: semilla y opciones de un permalink de wwrando 1.10.0 (lee user/rando/permalink.txt)",
                        "A: seed and options from a wwrando 1.10.0 permalink (reads user/rando/permalink.txt)",
                        permalinkValue, permalinkActivate, 30});
    randoMenuAddAction({"Generar", "Generate",
                        "A: genera la semilla; la próxima partida nueva la usa. Registros en user/rando",
                        "A: generates the seed; the next new game uses it. Logs in user/rando", generateValue,
                        generateActivate, 80});
    randoFileSelectMenuInit(); // R4.4: "Clean unused seed files" (order 150)
    randoFsNewgameMenuInit();  // R8.1: "Ask on New Game" (order 90, setting COS_RANDO_ASK_NEWGAME)
}

RandoGameKind randoCurrentGame() {
    if (activeSeed() != nullptr) {
        return RandoGameKind::Rando;
    }
    return menuInPlay() ? RandoGameKind::Normal : RandoGameKind::None;
}

std::string randoMenuPageHeader(int page, bool es, std::string* sub) {
    if (page != kRandoPageNext) {
        sub->clear();
        return "";
    }
    std::string line = L(es, "Estos ajustes se aplican al próximo archivo Randomizer nuevo, no a la partida actual.",
                         "These settings apply to the next new Randomizer file, not to the game being played.");
    if (sBusy) {
        line += L(es, " Generando una semilla...", " Generating a seed...");
    } else if (const PcRandoSeed* p = pc_rando_slot_pending(); p != nullptr) {
        line += std::string(L(es, " Semilla pendiente: ", " Pending seed: ")) + p->seed + " (" + hashWords(*p) + ")";
    } else {
        line += L(es, " Ninguna semilla pendiente.", " No pending seed.");
    }
    *sub = line;
    return L(es, "Opciones de la siguiente partida", "Next game options");
}

bool randoGenBusy() {
    return sBusy;
}

const RandoGenLast& randoGenLast() {
    return sLast;
}

bool randoGenerate(bool es, std::string* msg) {
    auto refuse = [&](const char* text) {
        if (msg != nullptr) {
            *msg = text;
        }
        return false;
    };
    if (!kRegionOk) {
        return refuse(L(es, "Randomizer: solo con el disco USA", "Randomizer: USA disc only"));
    }
    if (sBusy) {
        return refuse(L(es, "Ya se está generando una semilla", "A seed is already being generated"));
    }
    std::string seed = currentSeed();
    if (seed.empty()) {
        // wwrando's UI makes a random name for an empty seed field too.
        seed = pcr::randomSeedName(entropy());
        storeSeed(seed);
    }
    pcr::GenRequest req;
    req.seed = seed;
    req.options = randoSettingsOptions(true);
    req.allowMissingHints = false; // R5.3: hints are generated and placed at runtime (pc_rando_gen.h)
    req.version = version();
    pcr::GenJobPaths paths;
    paths.logDir = randoUserDir();
    char card[PATH_MAX];
    if (pc_rando_slot_card_dir(card, sizeof card) != 0) {
        paths.cardDir = card;
    }
    std::string err;
    if (!pcr::genJobStart(req, paths, &err)) {
        logf("[cos] rando gen: not started: %s\n", err.c_str());
        return refuse(L(es, "No se pudo empezar a generar", "Could not start generating"));
    }
    sBusy = true;
    sLast = RandoGenLast();
    sLast.seed = seed;
    sLast.startFrame = pc_frame_count();
    logf("[cos] rando gen: seed \"%s\" started (logs %s, card %s)\n", seed.c_str(), paths.logDir.c_str(),
         paths.cardDir.empty() ? "(none)" : paths.cardDir.c_str());
    if (msg != nullptr) {
        *msg = std::string(L(es, "Generando la semilla ", "Generating seed ")) + seed;
    }
    return true;
}

void randoMenuFrame() {
    randoFileSelectOverlay(); // R4.4: the file select's randomizer line (D15)
    if (!sBusy) {
        return;
    }
    pcr::GenJobResult res;
    if (!pcr::genJobTake(&res)) {
        return;
    }
    sBusy = false;
    RandoGenLast& l = sLast;
    l.have = true;
    l.ok = res.gen.ok;
    if (!res.gen.sanitizedSeed.empty()) {
        l.seed = res.gen.sanitizedSeed;
    }
    l.hash = res.gen.seedHash;
    l.seconds = res.seconds;
    l.endFrame = pc_frame_count();
    l.spoilerPath = res.spoilerPath;
    l.nonSpoilerPath = res.nonSpoilerPath;
    l.errorPath = res.errorPath;
    l.sidecarPath = res.sidecarPath;
    if (res.gen.ok) {
        char h[16];
        snprintf(h, sizeof h, "%08x", (unsigned)pc_rando_seed_hash32(res.gen.seed));
        l.hash32 = h;
        pc_rando_slot_set_pending(res.gen.seed);
        l.error = res.fileError.empty() ? "" : "not written: " + res.fileError;
        logf("[cos] rando gen: seed \"%s\" ok in %.3f s (frames %u-%u): hash \"%s\" %s%s; pending for the next "
             "new game; sidecar %s; spoiler log %s\n",
             l.seed.c_str(), l.seconds, l.startFrame, l.endFrame, l.hash.c_str(), l.hash32.c_str(),
             res.gen.hintsMissing ? " (no hints: step R5)" : "", l.sidecarPath.empty() ? "(none)" : l.sidecarPath.c_str(),
             l.spoilerPath.empty() ? "(none)" : l.spoilerPath.c_str());
    } else {
        l.error = firstLine(res.gen.error);
        logf("[cos] rando gen: seed \"%s\" failed: %s; error log %s\n", l.seed.c_str(), l.error.c_str(),
             l.errorPath.empty() ? "(none)" : l.errorPath.c_str());
    }
    if (!res.fileError.empty()) {
        logf("[cos] rando gen: could not write %s\n", res.fileError.c_str());
    }
}

bool randoApplyPermalink(const char* permalink, bool es, std::string* msg) {
    const std::string text = rando::textSanitize(rando::TextKind::Permalink, permalink != nullptr ? permalink : "");
    std::string seed;
    pcr::Options o; // options not in the permalink keep wwrando's defaults
    const pcr::PermalinkError pe = pcr::decodePermalink(text, &seed, &o);
    if (pe != pcr::PermalinkError::None) {
        logf("[cos] rando menu: permalink refused: %s\n", pcr::permalinkErrorText(pe));
        if (msg != nullptr) {
            *msg = std::string(L(es, "Permalink no válido: ", "Bad permalink: ")) + pcr::permalinkErrorText(pe);
        }
        return false;
    }
    const int locked = randoSettingsApply(o);
    const bool seedOk = storeSeed(rando::textSanitize(rando::TextKind::Seed, seed.c_str()));
    logf("[cos] rando menu: permalink applied: seed \"%s\"%s, %d option(s) locked by the environment\n",
         seed.c_str(), seedOk ? "" : " (not stored: locked)", locked);
    if (msg != nullptr) {
        if (locked == 0 && seedOk) {
            *msg = std::string(L(es, "Permalink: ", "Permalink: ")) + seed;
        } else {
            *msg = L(es, "Permalink: hay ajustes fijados por el entorno", "Permalink: some settings are set by the environment");
        }
    }
    return locked == 0 && seedOk;
}

} // namespace pc
