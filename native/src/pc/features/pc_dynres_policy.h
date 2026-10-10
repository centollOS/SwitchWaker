#pragma once

// Dynamic resolution (pc_dynres.cpp): the level list and the decision taken every evaluation. Pure
// logic, so cos_pc_tests checks it on the host (the Mac has no GPU timer to drive it).
//
// GPU samples are per present: Aurora's GPU timer gives the GPU time of the frames read back since
// the last game frame, divided by their number (one present a game frame at 30 fps, two with the
// 60 fps mode), so the same p95 is compared with a budget of one present: 30 / 27 ms at 30 fps
// (33.3 ms a present), 15 / 13 ms with 60 fps (16.7 ms a present).

#include <cmath>
#include <cstddef>

namespace pc {

// The levels below `base` (COS_FB_SCALE): 5/6, 3/4 and 2/3 of it, rounded to 1/96 (5 lines of the
// 480-line logical height). 1.5 (handheld 1280x720) gives 1.25, 1.125, 1.0 (the old fixed list
// 1.25, 1.125 plus 854x480); 2.25 (docked 1920x1080) gives 1.875, 1.6875, 1.5 (1280x720 docked,
// ~9 ms of GPU measured there).
// (No std::vector here: cos_pc_tests links without the game's operator new.)
struct DynresLevels {
    float v[4] = {};
    size_t n = 0;
};

inline DynresLevels dynresDefaultLevels(float base) {
    DynresLevels levels;
    levels.v[levels.n++] = base;
    const float fractions[] = {5.f / 6.f, 3.f / 4.f, 2.f / 3.f};
    for (float f : fractions) {
        const float l = std::round(base * f * 96.f) / 96.f;
        if (l > 0.f && l < levels.v[levels.n - 1]) {
            levels.v[levels.n++] = l;
        }
    }
    return levels;
}

struct DynresThresholds {
    double highMs; // p95 above this for two evaluations: one level down
    double lowMs;  // p95 scaled to the level above under this for four evaluations: one level up
};

// 30 fps: COS_DYNRES_HIGH / COS_DYNRES_LOW (30 / 27); 60 fps: COS_DYNRES_HIGH60 / COS_DYNRES_LOW60
// (15 / 13).
inline DynresThresholds dynresThresholds(bool fps60, double high30, double low30, double high60, double low60) {
    return fps60 ? DynresThresholds{high60, low60} : DynresThresholds{high30, low30};
}

struct DynresEval {
    size_t level = 0;          // the level after this evaluation
    unsigned int overCount = 0;
    unsigned int underCount = 0;
    const char* why = nullptr; // set when the level changes
};

// One evaluation (every 30 GPU samples once 60 are in at this level). levels[0] is the base.
inline DynresEval dynresEvaluate(double p95, const DynresThresholds& t, const float* levels, size_t levelCount,
                                 size_t level, unsigned int overCount, unsigned int underCount) {
    DynresEval e;
    e.level = level;
    e.overCount = overCount;
    e.underCount = underCount;
    if (p95 > t.highMs) {
        e.underCount = 0;
        if (++e.overCount >= 2 && level + 1 < levelCount) {
            e.level = level + 1;
            e.why = "GPU-bound";
        }
    } else {
        e.overCount = 0;
        if (level > 0) {
            const double ratio = (double)levels[level - 1] / (double)levels[level];
            const double predicted = p95 * ratio * ratio;
            if (predicted < t.lowMs) {
                if (++e.underCount >= 4) {
                    e.level = level - 1;
                    e.why = "headroom";
                }
            } else {
                e.underCount = 0;
            }
        }
    }
    if (e.level != level) {
        e.overCount = 0;
        e.underCount = 0;
    }
    return e;
}

} // namespace pc
