// The spoiler viewer on screen (features/rando/pc_rando_spoiler_ui.h; docs/RANDOMIZER_PLAN.md step
// R4.5): the Randomizer tab's "Spoiler log..." row, the seed and log it loads, and the page the options
// menu hosts (ImGui, inside the menu's window). The model and its navigation are the pure
// pc_rando_spoilerview.cpp (unit test cos_rando_test_R4_5).
#include "features/rando/pc_rando_spoiler_ui.h"

#include "features/rando/pc_rando_menu.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_settings.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_spoiler.h"
#include "features/rando/pc_rando_text_ui.h"
#include "pc/pc_rando.h"
#include "pc_internal.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace pc {

namespace {

const char* T(const char* es, const char* en) {
    return pc_ui_spanish() ? es : en;
}

const char* L(bool es, const char* s, const char* e) {
    return es ? s : e;
}

bool sInit = false;
pcr::SpoilerPager sPager;
RandoSpoilerInfo sInfo;
bool sScroll = false; // the cursor moved: scroll to it

// The seed the page shows: the active one while a randomized file is played (R8.2: the row is on the
// tab's page about the game being played, so never the pending seed of the next game).
const PcRandoSeed* seedToShow(bool* active) {
    if (pc_rando()) {
        if (const PcRandoSeed* s = pc_rando_slot_active_seed(); s != nullptr) {
            *active = true;
            return s;
        }
    }
    *active = false;
    return nullptr;
}

// R8.2: the row is shown only while a randomized file is played.
bool rowShown() {
    bool active = false;
    return seedToShow(&active) != nullptr;
}

bool hidesSpoiler(const PcRandoSeed& seed) {
    const pcr::Json* j = seed.options.find("do_not_generate_spoiler_log");
    return j != nullptr && j->asBool(false);
}

bool readAll(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        out->append(buf, n);
    }
    fclose(f);
    return true;
}

bool sameLines(const std::vector<pcr::SpoilerLine>& a, const std::vector<pcr::SpoilerLine>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].depth != b[i].depth || a[i].text != b[i].text || a[i].item != b[i].item) {
            return false;
        }
    }
    return true;
}

std::string rowValue(bool es) {
    if (sPager.state() != pcr::SpoilerState::Closed) {
        return L(es, "Abierto", "Open");
    }
    bool active = false;
    const PcRandoSeed* seed = seedToShow(&active);
    if (seed == nullptr) {
        return L(es, "(sin partida Randomizer)", "(no Randomizer game)");
    }
    if (hidesSpoiler(*seed)) {
        return L(es, "(semilla sin registro)", "(seed without a log)");
    }
    return std::string(L(es, "A: ver ", "A: view ")) + seed->seed;
}

std::string rowActivate(bool es) {
    std::string msg;
    randoSpoilerOpen(es, &msg);
    return msg;
}

} // namespace

void randoSpoilerInit() {
    if (sInit) {
        return;
    }
    sInit = true;
    // R8.2: on the tab's first page (order < 0), about the randomized file being played.
    randoMenuAddAction({"Ver registro de soluciones...", "Spoiler log...",
                        "A: las esferas de progresión y cada ubicación por zona de la semilla de esta partida. "
                        "Pregunta antes de mostrar nada",
                        "A: the progression spheres and every location by zone of this game's seed. Asks before "
                        "showing anything",
                        rowValue, rowActivate, -100, rowShown});
}

bool randoSpoilerOpen(bool es, std::string* msg) {
    auto refuse = [&](const char* text) {
        if (msg != nullptr) {
            *msg = text;
        }
        return false;
    };
    bool active = false;
    const PcRandoSeed* seed = seedToShow(&active);
    if (seed == nullptr) {
        return refuse(L(es, "No se está jugando una partida Randomizer", "No Randomizer game is being played"));
    }
    if (hidesSpoiler(*seed)) {
        return refuse(L(es, "Esta semilla se generó sin registro de soluciones",
                        "This seed was generated without a spoiler log"));
    }
    RandoSpoilerInfo info;
    info.seed = seed->seed;
    info.active = active;
    info.logPath = randoUserDir() + "/" + pcr::logFileName(seed->seed, "Spoiler Log");
    std::vector<pcr::SpoilerLine> zones = pcr::spoilerZonesFromPlacement(seed->location_names, seed->location_item_names);
    std::vector<pcr::SpoilerLine> spheres;
    std::string text, err;
    pcr::SpoilerLog log;
    if (readAll(info.logPath, &text)) {
        info.logFound = pcr::spoilerLogParse(text, &log, &err);
        info.logMatches = info.logFound && log.seed == seed->seed &&
                          (seed->permalink.empty() || log.permalink == seed->permalink);
        if (info.logMatches) {
            info.hash = log.seedHash;
            info.zonesMatchLog = sameLines(zones, log.locations);
            for (pcr::SpoilerLine& l : log.spheres) {
                if (l.depth == 0) {
                    l.text = std::string(L(es, "Esfera ", "Sphere ")) + l.text;
                }
            }
            spheres = std::move(log.spheres);
        }
    } else {
        err = "not found";
    }
    if (info.hash.empty()) {
        const RandoGenLast& g = randoGenLast();
        char h[16];
        snprintf(h, sizeof h, "%08x", (unsigned)pc_rando_seed_hash32(*seed));
        if (g.have && g.ok && g.hash32 == h) {
            info.hash = g.hash;
        }
    }
    int zoneCount = 0;
    for (const pcr::SpoilerLine& l : zones) {
        zoneCount += l.depth == 0;
    }
    writef(STDERR_FILENO,
           "[cos] rando spoiler: %s seed \"%s\" (%s): %zu locations in %d zones; log %s: %s%s\n",
           active ? "active" : "pending", seed->seed.c_str(), info.hash.empty() ? "no hash" : info.hash.c_str(),
           seed->location_names.size(), zoneCount, info.logPath.c_str(),
           info.logMatches ? "spheres shown" : info.logFound ? "another seed's, no spheres" : err.c_str(),
           info.logMatches && !info.zonesMatchLog ? " (its locations differ from the placement's)" : "");
    sInfo = info;
    sPager.open(std::move(spheres), std::move(zones));
    sScroll = true;
    if (msg != nullptr) {
        *msg = "";
    }
    return true;
}

bool randoSpoilerActive() {
    return sPager.state() != pcr::SpoilerState::Closed;
}

void randoSpoilerKeys(uint32_t pressed) {
    const pcr::SpoilerState before = sPager.state();
    if (sPager.press(pressed)) {
        sScroll = true;
    }
    if (before != sPager.state()) {
        writef(STDERR_FILENO, "[cos] rando spoiler: %s\n",
               sPager.state() == pcr::SpoilerState::View     ? "shown"
               : sPager.state() == pcr::SpoilerState::Closed ? "closed"
                                                             : "confirm");
    }
}

void randoSpoilerClose() {
    if (randoSpoilerActive()) {
        sPager.close();
        writef(STDERR_FILENO, "[cos] rando spoiler: closed\n");
    }
}

const RandoSpoilerInfo& randoSpoilerInfo() {
    return sInfo;
}

const pcr::SpoilerPager& randoSpoilerPager() {
    return sPager;
}

void randoSpoilerDraw(std::string* help) {
    if (!randoSpoilerActive()) {
        return;
    }
    const ImVec4 accent(1.00f, 0.80f, 0.30f, 1.0f);
    const ImVec4 dim(0.62f, 0.66f, 0.72f, 1.0f);
    const ImVec4 zone(0.55f, 0.80f, 1.00f, 1.0f);
    const ImVec4 warn(1.00f, 0.55f, 0.45f, 1.0f);
    const ImVec4 item(0.85f, 0.88f, 0.92f, 1.0f);
    ImGui::TextColored(accent, "%s", T("Registro de soluciones", "Spoiler log"));
    ImGui::SameLine();
    if (sInfo.hash.empty()) {
        ImGui::TextColored(dim, "%s", sInfo.seed.c_str());
    } else {
        ImGui::TextColored(dim, "%s  (%s)", sInfo.seed.c_str(), sInfo.hash.c_str());
    }
    if (sPager.state() == pcr::SpoilerState::Confirm) {
        ImGui::Separator();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(warn, "%s",
                           T("Esta página revela dónde está cada objeto de la semilla.",
                             "This page reveals where every item of the seed is."));
        ImGui::TextUnformatted(sInfo.active ? T("Semilla: la de la partida en curso.", "Seed: the file being played.")
                                            : T("Semilla: la generada, que usará la próxima partida nueva.",
                                                "Seed: the generated one, which the next new game will use."));
        ImGui::Spacing();
        ImGui::TextColored(accent, "%s", T("¿Mostrar las soluciones?   A: sí   B: no", "Show the spoilers?   A: yes   B: no"));
        ImGui::PopTextWrapPos();
        *help = T("A: mostrar   B: volver sin mostrar nada", "A: show   B: go back without showing anything");
        return;
    }

    // the two sections
    const int sec = sPager.section();
    const char* names[pcr::kSpoilerSectionCount] = {T("Esferas", "Spheres"), T("Ubicaciones por zona", "Locations by zone")};
    for (int s = 0; s < pcr::kSpoilerSectionCount; s++) {
        if (s > 0) {
            ImGui::SameLine();
            ImGui::TextColored(dim, "|");
            ImGui::SameLine();
        }
        const int count = s == pcr::kSpoilerSpheres ? sPager.headings(s) : sPager.entries(s);
        ImGui::TextColored(s == sec ? accent : dim, s == sec ? "[%s: %d]" : "%s: %d", names[s], count);
    }
    ImGui::Separator();

    const std::vector<pcr::SpoilerLine>& lines = sPager.lines();
    if (lines.empty()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(dim, "%s",
                           sec == pcr::kSpoilerSpheres
                               ? (sInfo.logFound ? T("El registro de soluciones de user/rando es de otra semilla: sin esferas.",
                                                     "The spoiler log in user/rando is another seed's: no spheres.")
                                                 : T("No se encontró el registro de soluciones en user/rando: sin esferas.",
                                                     "The spoiler log was not found in user/rando: no spheres."))
                               : T("La semilla no tiene ubicaciones.", "The seed has no locations."));
        ImGui::PopTextWrapPos();
    } else if (ImGui::BeginChild("##spoiler_lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoNav)) {
        // the lines scroll under the fixed title and section line; the items in a column after the
        // longest name (at most 62% of the width: a longer name pushes its own item right)
        const float indent = ImGui::GetFontSize() * 1.2f;
        const float gap = ImGui::GetFontSize() * 1.0f;
        float nameW = 0.0f;
        for (const pcr::SpoilerLine& l : lines) {
            if (!l.item.empty()) {
                nameW = std::max(nameW, indent * (float)l.depth + ImGui::CalcTextSize(l.text.c_str()).x);
            }
        }
        const float itemX = std::min(nameW + gap, ImGui::GetContentRegionAvail().x * 0.62f);
        const int cursor = sPager.cursor();
        for (int i = 0; i < (int)lines.size(); i++) {
            const pcr::SpoilerLine& l = lines[i];
            ImGui::PushID(i);
            const float x0 = ImGui::GetCursorPosX();
            const bool sel = i == cursor;
            ImGui::Selectable("##l", sel, ImGuiSelectableFlags_AllowOverlap);
            ImGui::SameLine(x0 + indent * (float)l.depth);
            const ImVec4 color = l.depth == 0 ? accent : l.depth == 1 ? zone : sel ? accent : ImVec4(1, 1, 1, 1);
            ImGui::TextColored(color, "%s", l.text.c_str());
            if (!l.item.empty()) {
                const float end = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetScrollX();
                if (end + gap * 0.5f > x0 + itemX) {
                    ImGui::SameLine(0.0f, gap * 0.5f);
                } else {
                    ImGui::SameLine(x0 + itemX);
                }
                ImGui::TextColored(sel ? accent : item, "%s", l.item.c_str());
            }
            if (sel && sScroll) {
                ImGui::SetScrollHereY(0.4f);
            }
            ImGui::PopID();
        }
    }
    if (!lines.empty()) {
        ImGui::EndChild();
    }
    sScroll = false;
    *help = T("Arriba/abajo: línea   Izq./der.: esfera/zona   L/R: sección   B: cerrar",
              "Up/down: line   Left/right: sphere or zone   L/R: section   B: close");
}

} // namespace pc
