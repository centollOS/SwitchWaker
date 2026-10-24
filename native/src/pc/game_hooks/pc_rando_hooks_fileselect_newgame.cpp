// The file select's "Normal / Randomizer" question on a blank slot (docs/RANDOMIZER_PLAN.md step R8.1;
// behaviour in features/rando/pc_rando_fsnewgame.h, the hooks in pc/pc_rando_hooks.h, the hunks in
// game/src/d/d_file_select.cpp listed in docs/RANDO_HOOKS.md "## R8.1"). The game code keeps the pane
// work (the corrupted-slot prompt's panes); this file decides, writes the texts and drives the seed
// generation through the options menu's job (pc_rando_menu.h). Every entry point runs in a
// JKRPcHostAllocScope: the strings here outlive the name scene's heap.
#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_fsnewgame.h"
#include "features/rando/pc_rando_genjob.h"
#include "features/rando/pc_rando_menu.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_options.h"
#include "features/rando/pc_rando_settings.h"
#include "features/rando/pc_rando_sidecars.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_text.h"
#include "features/rando/pc_rando_wrap.h"
#include "pc/pc_harness.h"
#include "pc/pc_rando_hooks.h"
#include "pc/pc_settings.h"
#include "pc_internal.h"

#include "d/d_com_inf_game.h"

#include "JAZelAudio/JAZelAudio_SE.h"
#include "m_Do/m_Do_audio.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <unistd.h>

namespace pc {

const char* const kRandoAskNewGameKey = "COS_RANDO_ASK_NEWGAME";

namespace {

// D16: USA only until R7.6 (as pc_rando_menu.cpp).
#if defined(VERSION) && VERSION == 3
constexpr bool kRegionOk = false;
#else
constexpr bool kRegionOk = true;
#endif

constexpr size_t kTitleCap = 0x30;   // d_file_select.cpp screenSet: the title boxes' strings
constexpr size_t kMessageCap = 0x80; // and the message pane's
// The message pane's line width for the word wrap, in the message font's pixels (pc_rando_wrap.h);
// measured on the file select's 'mes1' box (shots of the fsnewgame smoke).
constexpr int kMessageLineWidth = 560;
// An error stays on the panel this long unless A or B is pressed (frames, 30 per second).
constexpr int kErrorFrames = 180;

// Question: R8.1's "Start a Randomizer file?"; Second: R8.3's "Make a new seed?".
enum class Phase { Question, Second, Wait, Error };

RandoFsNewgameInfo sInfo;
bool sMenuInit = false;
bool sAsking = false;
Phase sPhase = Phase::Question;
int sChoice = 0; // PC_RANDO_FSNG take_choice values
int sErrorFrames = 0;
std::string sSeedLine; // the message pane's seed line of the current question

// The question's texts. English on the USA game, as its own texts; on the European game they follow
// the game's language (the file select's language choice, dComIfGs_getPalLanguage): Spanish for
// Spanish, English for the other three. The R3.20 overlay holds them while they are read
// (pc_rando_msg.h; UTF-8 in, the font's cp1252 bytes out).
struct MsgText {
    uint16_t no;
    const char* en;
    const char* es;
};
const MsgText kTexts[] = {
    {PCR_FSNG_MSG_QUESTION, "Start a Randomizer file?", "¿Empezar una partida Randomizer?"},
    {PCR_FSNG_MSG_SEED, "Randomizer seed: %s\nHash: %s\nNo starts a normal game.",
     "Semilla Randomizer: %s\nHash: %s\nElige No para una partida normal."},
    {PCR_FSNG_MSG_NEWSEED, "New randomizer seed: %s,\nmade with the Randomizer options.\nNo starts a normal game.",
     "Nueva semilla Randomizer: %s,\ncon las opciones del Randomizer.\nElige No para una partida normal."},
    {PCR_FSNG_MSG_WAIT, "Generating the seed... %d%%", "Generando la semilla... %d%%"},
    {PCR_FSNG_MSG_ERROR, "The seed could not be made.", "No se pudo crear la semilla."},
    {PCR_FSNG_MSG_RANDOM, "random", "aleatoria"},
    {PCR_FSNG_MSG_NEWQ, "Make a new seed?", "¿Crear una semilla nueva?"},
    {PCR_FSNG_MSG_CURPEND, "Current seed: %s\nHash: %s\nNo keeps this seed.",
     "Semilla actual: %s\nHash: %s\nElige No para mantenerla."},
    {PCR_FSNG_MSG_CURNAME, "Current seed: %s\nNo keeps this seed.", "Semilla actual: %s\nElige No para mantenerla."},
};

// The European game in Spanish (PAL language 3, the SDK's OS_LANGUAGE_SPANISH). Never on USA.
bool textSpanish() {
#if defined(VERSION) && VERSION == 3
    return dComIfGs_getPalLanguage() == 3;
#else
    return false;
#endif
}

// A message of the overlay as plain characters (control tags dropped), as fopMsgM_messageGet copies
// a BMG message into a pane's string.
std::string msgText(uint16_t no) {
    const char* src = nullptr;
    const bool es = textSpanish();
    for (const MsgText& t : kTexts) {
        if (t.no == no) {
            src = es ? t.es : t.en;
        }
    }
    if (src == nullptr) {
        return std::string();
    }
    if (pc_rando_msg_add(no, nullptr, src) != PCR_MSG_OK) {
        return src;
    }
    std::string out;
    if (const PcRandoMsg* m = pc_rando_msg_find(no); m != nullptr && m->bytes != nullptr) {
        for (size_t i = 0; i < m->len && m->bytes[i] != 0;) {
            if (m->bytes[i] == 0x1A && i + 1 < m->len) {
                i += m->bytes[i + 1] > 0 ? m->bytes[i + 1] : 1;
                continue;
            }
            out += (char)m->bytes[i++];
        }
    }
    pc_rando_msg_remove(no); // read by the file select only: the game's lookups never see it
    return out;
}

std::string format(uint16_t no, ...) {
    const std::string fmt = msgText(no);
    char buf[512];
    va_list ap;
    va_start(ap, no);
    vsnprintf(buf, sizeof buf, fmt.c_str(), ap);
    va_end(ap);
    return buf;
}

// Wrapped to the message pane with the message font's widths (one line per '\n' part).
std::string wrapMessage(const std::string& s) {
    const pcr::GlyphWidths* w = pc_rando_font_widths();
    if (w == nullptr) {
        return s;
    }
    return pcr::wordWrapString(*w, s, kMessageLineWidth, false);
}

void put(char* dst, size_t cap, const std::string& s, std::string* keep) {
    if (dst != nullptr) {
        snprintf(dst, cap, "%s", s.c_str());
        if (keep != nullptr) {
            *keep = dst;
        }
    }
}

std::string firstLine(const std::string& s) {
    const size_t e = s.find('\n');
    return e == std::string::npos ? s : s.substr(0, e);
}

std::string seedLine() {
    if (const PcRandoSeed* p = pc_rando_slot_pending(); p != nullptr) {
        return wrapMessage(format(PCR_FSNG_MSG_SEED, p->seed.c_str(), pcr::seedHashWords(*p).c_str()));
    }
    std::string seed = rando::textSanitize(rando::TextKind::Seed, pc_settings_get(kRandoSeedKey));
    if (seed.empty()) {
        seed = msgText(PCR_FSNG_MSG_RANDOM);
    }
    return wrapMessage(format(PCR_FSNG_MSG_NEWSEED, seed.c_str()));
}

// R8.3: the seed the second question shows ("Current seed: ..."): the pending seed's name and hash
// words, else the name of the seed setting. Empty when there is no seed at all.
std::string settingSeed() {
    return rando::textSanitize(rando::TextKind::Seed, pc_settings_get(kRandoSeedKey));
}

std::string currentSeedLine() {
    if (const PcRandoSeed* p = pc_rando_slot_pending(); p != nullptr) {
        return wrapMessage(format(PCR_FSNG_MSG_CURPEND, p->seed.c_str(), pcr::seedHashWords(*p).c_str()));
    }
    return wrapMessage(format(PCR_FSNG_MSG_CURNAME, settingSeed().c_str()));
}

uint64_t entropy() {
    const uint64_t a = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
    const uint64_t b = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    return a ^ (b * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)getpid() << 32) ^ (uint64_t)pc_frame_count();
}

// R8.3: a new random seed name in COS_RANDO_SEED, as the menu's "Random seed" row (so the menu shows
// it and the generation below uses it). When the environment sets the seed it stays as it is.
void newSeedName() {
    if (pc_settings_locked(kRandoSeedKey)) {
        writef(STDERR_FILENO, "[cos] rando fsnewgame: %s is set by the environment: its seed \"%s\" is kept\n",
               kRandoSeedKey, settingSeed().c_str());
        return;
    }
    const std::string old = settingSeed();
    std::string s;
    for (int i = 0; i < 8; i++) { // a name different from the current one
        s = pcr::randomSeedName(entropy() + (uint64_t)i * 0x2545F4914F6CDD1Dull);
        if (s != old) {
            break;
        }
    }
    pc_settings_set(kRandoSeedKey, s.c_str());
    sInfo.newSeeds++;
    sInfo.lastNewSeed = s;
    writef(STDERR_FILENO, "[cos] rando fsnewgame: new random seed \"%s\" (was \"%s\")\n", s.c_str(), old.c_str());
}

void sound(u32 se) {
    mDoAud_seStart(se);
}

int toError(const std::string& error, char* title, char* message) {
    sInfo.errors++;
    sInfo.lastError = firstLine(error);
    put(title, kTitleCap, msgText(PCR_FSNG_MSG_ERROR), &sInfo.lastTitle);
    put(message, kMessageCap, wrapMessage(sInfo.lastError), &sInfo.lastMessage);
    writef(STDERR_FILENO, "[cos] rando fsnewgame: generation failed: %s\n", sInfo.lastError.c_str());
    sPhase = Phase::Error;
    sErrorFrames = 0;
    return PC_RANDO_FSNG_WAIT;
}

int back() {
    sound(JA_SE_MSEL_CANCEL_1);
    sInfo.backs++;
    sPhase = Phase::Question;
    sChoice = 0;
    return PC_RANDO_FSNG_BACK;
}

int waitTitle(char* title) {
    const pcr::GenJobProgress p = pcr::genJobProgress();
    const int pct = p.total > 0 ? p.done * 100 / p.total : 0;
    put(title, kTitleCap, format(PCR_FSNG_MSG_WAIT, pct), &sInfo.lastTitle);
    return PC_RANDO_FSNG_WAIT;
}

// Starts the generation of the settings' seed (or waits for the Generate row's running job) and
// enters the wait state; an error that stops it from starting goes to the error state.
int startGeneration(char* title, char* message) {
    sInfo.generated++;
    sInfo.waitFrames = 0;
    if (!randoGenBusy()) { // else the Generate row's job is running: wait for it
        std::string msg;
        if (!randoGenerate(false, &msg)) {
            return toError(msg, title, message);
        }
    }
    writef(STDERR_FILENO, "[cos] rando fsnewgame: generating seed \"%s\" (frame %u)\n", randoGenLast().seed.c_str(),
           pc_frame_count());
    put(message, kMessageCap, seedLine(), &sInfo.lastMessage);
    sPhase = Phase::Wait;
    return waitTitle(title);
}

int usePending() {
    sInfo.usedPending++;
    sChoice = 2;
    writef(STDERR_FILENO, "[cos] rando fsnewgame: the pending seed \"%s\"\n", pc_rando_slot_pending()->seed.c_str());
    return PC_RANDO_FSNG_START;
}

std::string askValue(bool es) {
    if (pc_settings_locked(kRandoAskNewGameKey)) {
        return std::string(randoFsNewgameEnabled() ? (es ? "Sí" : "On") : (es ? "No" : "Off")) +
               (es ? " (entorno)" : " (environment)");
    }
    return randoFsNewgameEnabled() ? (es ? "Sí" : "On") : (es ? "No" : "Off");
}

std::string askActivate(bool es) {
    if (pc_settings_locked(kRandoAskNewGameKey)) {
        return es ? "Fijado por el entorno" : "Set by the environment";
    }
    const bool on = strcmp(pc_settings_get(kRandoAskNewGameKey), "0") != 0;
    pc_settings_set(kRandoAskNewGameKey, on ? "0" : "1");
    return std::string(es ? "Preguntar en partida nueva: " : "Ask on New Game: ") + askValue(es);
}

} // namespace

void randoFsNewgameMenuInit() {
    if (sMenuInit) {
        return;
    }
    sMenuInit = true;
    static const PcSettingChoice kChoices[] = {{"0", "No", "Off"}, {"1", "Sí", "On"}};
    // On by default (plan R8.1); a harness run (run.sh sets COS_SMOKE or COS_MILESTONE) keeps the
    // pre-R8.1 flow unless it sets the variable, so the recorded input scripts stay valid.
    const bool harness = getenv("COS_SMOKE") != nullptr || getenv("COS_MILESTONE") != nullptr;
    static const PcSettingDesc desc = {
        kRandoAskNewGameKey,
        "Preguntar en partida nueva",
        "Al elegir un archivo vacío, pregunta si la partida es Randomizer (genera la semilla si hace falta)",
        PC_SETTING_TAB_RANDO,
        0,
        kChoices,
        2,
        harness ? "0" : "1",
        nullptr,
        nullptr,
        9510,
        "Ask on New Game",
        "On a blank file, asks whether the new game is a Randomizer one (generates the seed if needed)",
    };
    pc_settings_register(&desc);
    randoMenuAddAction({"Preguntar en partida nueva", "Ask on New Game",
                        "A: al elegir un archivo vacío, pregunta Normal / Randomizer (No: como antes, la "
                        "semilla generada se usa siempre)",
                        "A: on a blank file, asks Normal / Randomizer (Off: as before, a generated seed is "
                        "always used)",
                        askValue, askActivate, 90}); // right before "Seed..." (100)
}

bool randoFsNewgameEnabled() {
    return kRegionOk && strcmp(pc_settings_get(kRandoAskNewGameKey), "0") != 0;
}

const RandoFsNewgameInfo& randoFsNewgameInfo() {
    return sInfo;
}

} // namespace pc

extern "C" {

int pc_rando_fs_newgame_ask(int blank, int slot, char* title, char* message) {
    using namespace pc;
    JKRPcHostAllocScope hostAlloc;
    sPhase = Phase::Question;
    sChoice = 0;
    if (!blank || !randoFsNewgameEnabled()) {
        sAsking = false;
        sInfo.notAsked++;
        return 0;
    }
    sAsking = true;
    sInfo.asked++;
    sSeedLine = seedLine();
    put(title, kTitleCap, msgText(PCR_FSNG_MSG_QUESTION), &sInfo.lastTitle);
    put(message, kMessageCap, sSeedLine, &sInfo.lastMessage);
    writef(STDERR_FILENO, "[cos] rando fsnewgame: blank slot %d: asked (%s)\n", slot + 1,
           pc_rando_slot_pending() != nullptr ? "pending seed" : "no pending seed");
    return 1;
}

int pc_rando_fs_newgame_asking(void) {
    return pc::sAsking ? 1 : 0;
}

int pc_rando_fs_newgame_yesno(int trigA, int trigB, int cursor, char* title, char* message) {
    using namespace pc;
    JKRPcHostAllocScope hostAlloc;
    switch (sPhase) {
    case Phase::Question:
        if (trigB) {
            return back();
        }
        if (!trigA) {
            return PC_RANDO_FSNG_NONE;
        }
        sound(JA_SE_MSEL_OK_1);
        if (cursor != 0) {
            sInfo.answeredNo++;
            sChoice = 1;
            writef(STDERR_FILENO, "[cos] rando fsnewgame: No: a normal new file%s\n",
                   pc_rando_slot_pending() != nullptr ? " (the pending seed stays pending)" : "");
            return PC_RANDO_FSNG_START;
        }
        sInfo.answeredYes++;
        if (randoGenBusy()) { // the Generate row's job is running: its seed is the new one
            writef(STDERR_FILENO, "[cos] rando fsnewgame: Yes: waiting for the menu's generation\n");
            return startGeneration(title, message);
        }
        if (pc_rando_slot_pending() == nullptr && settingSeed().empty()) {
            // R8.3: no seed at all: no second question, a new seed
            writef(STDERR_FILENO, "[cos] rando fsnewgame: Yes: no seed yet: a new one\n");
            newSeedName();
            return startGeneration(title, message);
        }
        // R8.3: "Make a new seed?" with the current seed, Yes preselected (the game moves the cursor)
        sInfo.asked2++;
        sPhase = Phase::Second;
        sSeedLine = currentSeedLine();
        put(title, kTitleCap, msgText(PCR_FSNG_MSG_NEWQ), &sInfo.lastTitle);
        put(message, kMessageCap, sSeedLine, &sInfo.lastMessage);
        sInfo.question2Message = sInfo.lastMessage;
        writef(STDERR_FILENO, "[cos] rando fsnewgame: Yes: asks for a new seed (current: %s)\n",
               pc_rando_slot_pending() != nullptr ? "the pending seed" : "the seed setting's name");
        return PC_RANDO_FSNG_ASK2;
    case Phase::Second:
        if (trigB) {
            writef(STDERR_FILENO, "[cos] rando fsnewgame: B on the new seed question: back\n");
            return back();
        }
        if (!trigA) {
            return PC_RANDO_FSNG_NONE;
        }
        sound(JA_SE_MSEL_OK_1);
        if (cursor == 0) {
            sInfo.answered2Yes++;
            writef(STDERR_FILENO, "[cos] rando fsnewgame: new seed: Yes\n");
            newSeedName();
            return startGeneration(title, message);
        }
        sInfo.answered2No++;
        writef(STDERR_FILENO, "[cos] rando fsnewgame: new seed: No (keeps the current seed)\n");
        if (pc_rando_slot_pending() != nullptr) {
            return usePending();
        }
        return startGeneration(title, message);
    case Phase::Wait: {
        sInfo.waitFrames++;
        if (randoGenBusy()) {
            if (trigB) {
                writef(STDERR_FILENO, "[cos] rando fsnewgame: B while generating: back (the seed becomes the "
                                      "pending one)\n");
                return back();
            }
            return waitTitle(title);
        }
        const RandoGenLast& l = randoGenLast();
        sInfo.seconds = l.seconds;
        if (!l.ok || pc_rando_slot_pending() == nullptr) {
            return toError(l.error.empty() ? std::string("no seed") : l.error, title, message);
        }
        writef(STDERR_FILENO,
               "[cos] rando fsnewgame: seed \"%s\" (%s) generated in %.3f s, %d frame(s) of wait state\n",
               l.seed.c_str(), l.hash.c_str(), l.seconds, sInfo.waitFrames);
        put(message, kMessageCap, seedLine(), &sInfo.lastMessage);
        sChoice = 2;
        sPhase = Phase::Question;
        return PC_RANDO_FSNG_START;
    }
    case Phase::Error:
        sErrorFrames++;
        if (trigA || trigB || sErrorFrames >= kErrorFrames) {
            return back();
        }
        return PC_RANDO_FSNG_WAIT;
    }
    return PC_RANDO_FSNG_NONE;
}

int pc_rando_fs_newgame_take_choice(void) {
    const int c = pc::sChoice;
    pc::sChoice = 0;
    return c;
}

} // extern "C"
