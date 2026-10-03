// COS_FPS_OVERLAY: a small frame-rate panel in the screen's top-left corner, drawn with Aurora's
// ImGui (which presents over the game picture; COS_SHOT images leave it out). Off by default; the
// Switch build turns it on (switch/native/source/cos_switch.cpp).
//
// Every half second it shows the frames per second over that half second (game frames, which on
// the Switch are also the presents: one each), the game thread's busy time per frame (the frame
// minus the pace wait, without aurora_end_frame), and on the Switch the render worker's busy time
// per presented frame and its Queue::Submit part (cos_switch_gfx_stats), and what Dawn's GL replay
// issued per presented frame: draws and sampled-texture binds.
#include "pc_internal.h"

#include <imgui.h>

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

namespace pc {

namespace {

constexpr uint64_t kWindowNs = 500ull * 1000000ull;

struct OverlayState {
    bool started = false;
    uint64_t windowStartNs = 0;
    unsigned int frames = 0;
    uint64_t busyNs = 0;
    double fps = 0;
    double gameMs = 0;
#if defined(__SWITCH__)
    CosSwitchGfxStats start{};
    double workerMs = 0;
    double submitMs = 0;
    double draws = 0;
    double texBinds = 0;
    bool workerValid = false;
#endif
} sOverlay;

void overlayUpdate(uint64_t now) {
    OverlayState& s = sOverlay;
    const uint64_t elapsed = now - s.windowStartNs;
    s.fps = s.frames * 1e9 / (double)elapsed;
    s.gameMs = s.frames != 0 ? s.busyNs / 1e6 / s.frames : 0;
#if defined(__SWITCH__)
    CosSwitchGfxStats cur{};
    cos_switch_gfx_stats(&cur);
    const uint64_t presents = cur.workerFrames - s.start.workerFrames;
    s.workerValid = presents != 0;
    if (s.workerValid) {
        s.workerMs = (cur.workerBusyNs - s.start.workerBusyNs) / 1e6 / presents;
        s.submitMs = (cur.workerSubmitNs - s.start.workerSubmitNs) / 1e6 / presents;
        s.draws = (double)(cur.glDraws - s.start.glDraws) / presents;
        s.texBinds = (double)(cur.glTexBinds - s.start.glTexBinds) / presents;
    }
    s.start = cur;
#endif
    s.windowStartNs = now;
    s.frames = 0;
    s.busyNs = 0;
}

} // namespace

void overlayFrame(uint64_t busyNs) {
    if (!gConfig.fpsOverlay || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    OverlayState& s = sOverlay;
    const uint64_t now = monotonicNs();
    if (!s.started) {
        s.started = true;
        s.windowStartNs = now;
#if defined(__SWITCH__)
        cos_switch_gfx_stats(&s.start);
#endif
    }
    s.frames++;
    s.busyNs += busyNs;
    if (now - s.windowStartNs >= kWindowNs) {
        overlayUpdate(now);
    }

    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##cos_fps_overlay", nullptr, kFlags)) {
        ImGui::Text("%.1f fps", s.fps);
        ImGui::Text("game %.1f ms", s.gameMs);
#if defined(__SWITCH__)
        if (s.workerValid) {
            ImGui::Text("render %.1f ms (submit %.1f)", s.workerMs, s.submitMs);
            ImGui::Text("draws %.0f, tex binds %.0f", s.draws, s.texBinds);
        } else {
            ImGui::TextUnformatted("render -");
        }
#endif
    }
    ImGui::End();
}

} // namespace pc
