// COS_FPS_OVERLAY: a small frame-rate panel in the screen's top-left corner, drawn with Aurora's
// ImGui (which presents over the game picture; COS_SHOT images leave it out). Off by default on
// every host; the options menu (Rendimiento > "Contador de FPS") or COS_FPS_OVERLAY=1 turns it on.
//
// COS_FPS_OVERLAY_DETAIL=compact keeps only the frame rate and the game thread's time on one line.
//
// Every half second it shows the presents per second over that half second (one per game frame, two
// for the split frames of COS_FPS60_TEST), the game thread's busy time per frame (the frame
// minus the pace wait, without aurora_end_frame), and on the Switch the render worker's busy time
// per presented frame and its Queue::Submit part (cos_switch_gfx_stats), and what Dawn's GL replay
// issued per presented frame: draws, pipeline changes and sampled-texture binds, and the time of the
// glDraw* calls (where Mesa validates state) and of setting the state before them; and the GPU time
// per frame of the frames read back in the half second (GL_TIME_ELAPSED_EXT, a few frames late).
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
    bool measured = false; // a half-second window closed (fps and game ms are valid)
    uint64_t windowStartNs = 0;
    unsigned int frames = 0;     // presents
    unsigned int gameFrames = 0; // game frames (fewer than presents with COS_FPS60_TEST)
    uint64_t busyNs = 0;
    double fps = 0;
    double gameMs = 0;
#if defined(__SWITCH__)
    CosSwitchGfxStats start{};
    double workerMs = 0;
    double submitMs = 0;
    double draws = 0;
    double texBinds = 0;
    double pipelines = 0;
    double drawCallMs = 0;
    double stateMs = 0;
    bool workerValid = false;
    double gpuMs = 0;
    int gpuState = 0; // CosSwitchGfxStats::gpuTimerState; 1 with frames read back: gpuMs is valid
    bool gpuValid = false;
#endif
} sOverlay;

void overlayUpdate(uint64_t now) {
    OverlayState& s = sOverlay;
    const uint64_t elapsed = now - s.windowStartNs;
    s.fps = s.frames * 1e9 / (double)elapsed;
    s.gameMs = s.gameFrames != 0 ? s.busyNs / 1e6 / s.gameFrames : 0;
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
        s.pipelines = (double)(cur.glPipelines - s.start.glPipelines) / presents;
        s.drawCallMs = (cur.glDrawCallNs - s.start.glDrawCallNs) / 1e6 / presents;
        s.stateMs = (cur.glPipelineNs - s.start.glPipelineNs + cur.glBindGroupNs - s.start.glBindGroupNs +
                     cur.glImmediatesNs - s.start.glImmediatesNs + cur.glVertexStateNs - s.start.glVertexStateNs) /
                    1e6 / presents;
    }
    const uint64_t gpuFrames = cur.gpuFrames - s.start.gpuFrames;
    s.gpuState = (int)cur.gpuTimerState;
    s.gpuValid = gpuFrames != 0;
    if (s.gpuValid) {
        s.gpuMs = (cur.gpuTotalNs - s.start.gpuTotalNs) / 1e6 / gpuFrames;
    }
    s.start = cur;
#endif
    s.windowStartNs = now;
    s.frames = 0;
    s.gameFrames = 0;
    s.busyNs = 0;
    s.measured = true;
}

} // namespace

void overlayFrame(uint64_t busyNs, bool gameFrame) {
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
    if (gameFrame) {
        s.gameFrames++;
        s.busyNs += busyNs;
    }
    if (now - s.windowStartNs >= kWindowNs) {
        overlayUpdate(now);
    }

    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##cos_fps_overlay", nullptr, kFlags)) {
        if (!s.measured) {
            // Turned on a moment ago (options menu): nothing measured yet.
            ImGui::TextUnformatted("-- fps");
            ImGui::End();
            return;
        }
        if (gConfig.fpsOverlayCompact) {
            // COS_FPS_OVERLAY_DETAIL=compact (options menu): one line.
            ImGui::Text("%.1f fps  %.1f ms", s.fps, s.gameMs);
            ImGui::End();
            return;
        }
        ImGui::Text("%.1f fps", s.fps);
        ImGui::Text("game %.1f ms", s.gameMs);
#if defined(__SWITCH__)
        if (s.workerValid) {
            ImGui::Text("render %.1f ms (submit %.1f)", s.workerMs, s.submitMs);
            ImGui::Text("draws %.0f, pipelines %.0f, tex binds %.0f", s.draws, s.pipelines, s.texBinds);
            ImGui::Text("gl draw calls %.1f ms, state %.1f ms", s.drawCallMs, s.stateMs);
        } else {
            ImGui::TextUnformatted("render -");
        }
        if (s.gpuValid) {
            ImGui::Text("gpu %.1f ms", s.gpuMs);
        } else {
            ImGui::Text("gpu %s", s.gpuState == 2 ? "no timer" : s.gpuState == 3 ? "off" : "-");
        }
#endif
    }
    ImGui::End();
}

} // namespace pc
