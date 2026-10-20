// The randomizer's text input on screen (features/rando/pc_rando_text_ui.h; docs/RANDOMIZER_PLAN.md
// step R4.2): the session the options menu hosts, its drawing (ImGui, inside the menu's window), the
// desktop text field, the Switch keyboard applet, and the permalink file import. The grid's logic and
// the character sets are the pure pc_rando_text.cpp (unit test cos_rando_test_R4_2).
#include "features/rando/pc_rando_text_ui.h"

#include "pc_internal.h"

#include <imgui.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

namespace pc {

namespace {

const char* T(const char* es, const char* en) {
    return pc_ui_spanish() ? es : en;
}

struct Session {
    bool active = false;
    RandoTextRequest req;
    std::string titleEs, titleEn; // copies: the request's strings may be temporaries
    rando::TextGrid grid{rando::TextKind::Seed};
    bool swkbd = false;           // the Switch keyboard applet is open
    // the desktop text field
    char field[rando::kPermalinkMaxLength + 1] = {};
    unsigned fieldVersion = ~0u;  // the grid version the field shows
    unsigned fieldId = 0;         // a new ImGui id reloads the field from the grid
    bool focusField = true;
    bool keyboardCaptured = false;
} s;

constexpr bool kDesktopField =
#if defined(__SWITCH__)
    false;
#else
    true;
#endif

const char* kindName(rando::TextKind kind) {
    return kind == rando::TextKind::Seed ? "seed" : "permalink";
}

// Calls `done` once the grid is Done or Cancelled, and ends the session.
void finishIfDone() {
    if (!s.active || s.grid.state() == rando::TextState::Editing) {
        return;
    }
    const bool ok = s.grid.state() == rando::TextState::Done;
    const std::string text = s.grid.text();
    const RandoTextDoneFn done = s.req.done;
    void* user = s.req.user;
    s.active = false;
    s.keyboardCaptured = false;
    writef(STDERR_FILENO, "[cos] rando text: %s %s (%zu characters)\n", kindName(s.grid.kind()),
           ok ? "OK" : "cancelled", text.size());
    if (done != nullptr) {
        done(ok, text.c_str(), user);
    }
}

int charFilter(ImGuiInputTextCallbackData* data) {
    const rando::TextKind kind = *static_cast<const rando::TextKind*>(data->UserData);
    return data->EventChar < 0x80 && rando::textCharAllowed(kind, (char)data->EventChar) ? 0 : 1;
}

#if defined(__SWITCH__)
void pollSwkbd() {
    if (!s.swkbd) {
        return;
    }
    char out[rando::kPermalinkMaxLength + 1];
    const int r = cos_switch_text_input_poll(out, sizeof(out));
    if (r == 2) {
        return;
    }
    s.swkbd = false;
    if (r == 1) {
        s.grid.setText(out);
        s.grid.finish(true);
    } else if (r == 0) {
        s.grid.finish(false);
    } else {
        writef(STDERR_FILENO, "[cos] rando text: the console's keyboard failed; the grid instead\n");
    }
    finishIfDone();
}
#endif

void drawGrid() {
    const float cell = ImGui::GetFontSize() * 1.9f;
    const ImVec4 sel(0.20f, 0.42f, 0.70f, 1.0f);
    const ImVec4 idle(0.16f, 0.18f, 0.22f, 1.0f);
    int clickRow = -1, clickCol = -1;
    for (int r = 0; r < s.grid.rows(); r++) {
        for (int c = 0; c < s.grid.rowLength(r); c++) {
            if (c > 0) {
                ImGui::SameLine();
            }
            const rando::TextCell cl = s.grid.cellAt(r, c);
            char label[48];
            float width = cell;
            switch (cl.action) {
            case rando::TextAction::Char: snprintf(label, sizeof label, "%c##c%d_%d", cl.ch, r, c); break;
            case rando::TextAction::Shift:
                snprintf(label, sizeof label, "%s##shift", s.grid.upper() ? "abc" : "ABC");
                width = cell * 2.5f;
                break;
            case rando::TextAction::Delete:
                snprintf(label, sizeof label, "%s##del", T("Borrar", "Delete"));
                width = cell * 2.5f;
                break;
            case rando::TextAction::Ok:
                snprintf(label, sizeof label, "OK##ok");
                width = cell * 2.5f;
                break;
            case rando::TextAction::Cancel:
                snprintf(label, sizeof label, "%s##cancel", T("Cancelar", "Cancel"));
                width = cell * 2.5f;
                break;
            }
            const bool here = r == s.grid.row() && c == s.grid.col();
            ImGui::PushStyleColor(ImGuiCol_Button, here ? sel : idle);
            if (ImGui::Button(label, ImVec2(width, cell))) {
                clickRow = r;
                clickCol = c;
            }
            ImGui::PopStyleColor();
        }
    }
    if (clickRow >= 0) {
        s.grid.pressCell(clickRow, clickCol);
    }
}

} // namespace

bool randoTextOpen(const RandoTextRequest& request) {
    if (s.active) {
        return false;
    }
    s = Session{};
    s.active = true;
    s.req = request;
    s.titleEs = request.titleEs != nullptr ? request.titleEs : "";
    s.titleEn = request.titleEn != nullptr ? request.titleEn : s.titleEs;
    s.grid = rando::TextGrid(request.kind, request.initial);
    writef(STDERR_FILENO, "[cos] rando text: %s input opened%s\n", kindName(request.kind),
           request.script ? " (COS_INPUT script)" : "");
#if defined(__SWITCH__)
    if (!request.script) {
        char stale[8];
        if (cos_switch_text_input_poll(stale, sizeof(stale)) != 2) { // drop a result nobody took
            s.swkbd = cos_switch_text_input_start(T(s.titleEs.c_str(), s.titleEn.c_str()), s.grid.text().c_str(),
                                                  rando::textMaxLength(request.kind),
                                                  request.kind == rando::TextKind::Seed ? 0 : 1) != 0;
        }
        writef(STDERR_FILENO, "[cos] rando text: %s\n",
               s.swkbd ? "the console's keyboard is open" : "the console's keyboard is not available: the grid");
    }
#endif
    return true;
}

bool randoTextActive() {
    return s.active;
}

bool randoTextScripted() {
    return s.active && s.req.script;
}

bool randoTextKeyboardCaptured() {
    return s.active && s.keyboardCaptured;
}

void randoTextKeys(uint32_t pressed) {
    if (!s.active || s.swkbd) {
        return;
    }
    s.grid.press(pressed);
    finishIfDone();
}

void randoTextCancel() {
    if (s.active) {
        s.grid.finish(false);
        finishIfDone();
    }
}

const rando::TextGrid* randoTextGrid() {
    return s.active ? &s.grid : nullptr;
}

void randoTextDraw(std::string* help) {
    if (!s.active) {
        return;
    }
#if defined(__SWITCH__)
    pollSwkbd();
    if (!s.active) {
        return;
    }
#endif
    const ImVec4 accent(1.00f, 0.80f, 0.30f, 1.0f);
    const ImVec4 dim(0.62f, 0.66f, 0.72f, 1.0f);
    const int max = rando::textMaxLength(s.grid.kind());
    ImGui::TextColored(accent, "%s", T(s.titleEs.c_str(), s.titleEn.c_str()));
    ImGui::SameLine();
    ImGui::TextColored(dim, "(%d/%d)", (int)s.grid.text().size(), max);
    if (s.swkbd) {
        ImGui::TextUnformatted(T("Escribe en el teclado de la consola...", "Type on the console's keyboard..."));
        *help = T("El teclado de la consola está abierto.", "The console's keyboard is open.");
        return;
    }
    if (kDesktopField) {
        if (s.fieldVersion != s.grid.version()) {
            // the grid changed the text: a fresh field (ImGui keeps an active field's own copy)
            snprintf(s.field, sizeof(s.field), "%s", s.grid.text().c_str());
            s.fieldVersion = s.grid.version();
            s.fieldId++;
            s.focusField = s.focusField || s.keyboardCaptured;
        }
        char id[32];
        snprintf(id, sizeof id, "##rando_text%u", s.fieldId);
        ImGui::PushItemWidth(-1.0f);
        if (s.focusField) {
            ImGui::SetKeyboardFocusHere();
            s.focusField = false;
        }
        rando::TextKind kind = s.grid.kind();
        const bool enter = ImGui::InputText(id, s.field, (size_t)max + 1,
                                            ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue,
                                            charFilter, &kind);
        ImGui::PopItemWidth();
        s.keyboardCaptured = ImGui::IsItemActive();
        if (s.grid.text() != s.field) {
            s.grid.setText(s.field);
            s.fieldVersion = s.grid.version(); // typed here: the field already shows it
        }
        if (enter) {
            s.grid.finish(true);
        } else if (s.keyboardCaptured && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            s.grid.finish(false);
        }
    } else {
        // the text, a cursor mark at its end
        ImGui::Text("%s_", s.grid.text().c_str());
    }
    ImGui::Separator();
    drawGrid();
    finishIfDone();
    // two lines at most: the menu's footer has room for no more
    *help = T("A: escribir   B: borrar (vacío: cancelar)   L/R: mayúsculas", "A: type   B: delete (empty: cancel)   L/R: case");
    if (kDesktopField) {
        *help += T("   Teclado: Intro = OK, Esc = cancelar", "   Keyboard: Enter = OK, Esc = cancel");
    }
}

std::string randoUserDir() {
    if (const char* dir = getenv("COS_RANDO_DIR"); dir != nullptr && dir[0] != '\0') {
        return dir;
    }
#if defined(__SWITCH__)
    return COS_SWITCH_ROOT "/user/rando";
#else
    // pc_main.cpp makeUserPaths: <directory of the executable>/user
    char exe[PATH_MAX] = {};
    executablePath(exe, sizeof(exe));
    char real[PATH_MAX];
    const char* path = exe[0] != '\0' && realpath(exe, real) != nullptr ? real : exe;
    const char* slash = strrchr(path, '/');
    const std::string dir = slash != nullptr ? std::string(path, (size_t)(slash - path)) : std::string(".");
    return dir + "/user/rando";
#endif
}

rando::PermalinkInfo randoPermalinkImport(std::string* path) {
    const std::string file = randoUserDir() + "/permalink.txt";
    if (path != nullptr) {
        *path = file;
    }
    const rando::PermalinkInfo info = rando::permalinkReadFile(file.c_str());
    if (info.status == rando::PermalinkStatus::Ok || info.status == rando::PermalinkStatus::OtherVersion) {
        writef(STDERR_FILENO, "[cos] rando: permalink from %s: %s (version %s, seed \"%s\", %zu option bytes)\n",
               file.c_str(), rando::permalinkStatusName(info.status), info.version.c_str(), info.seed.c_str(),
               info.options.size());
    } else {
        writef(STDERR_FILENO, "[cos] rando: permalink from %s: %s\n", file.c_str(),
               rando::permalinkStatusName(info.status));
    }
    return info;
}

} // namespace pc
