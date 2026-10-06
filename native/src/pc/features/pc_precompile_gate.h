#pragma once

// COS_PRECOMPILE_SCREEN=auto behind the logos (pc_precompile.cpp, lateScreenCheck): whether the
// warm-up's builds turned slow enough - a shader cache that lost part of its entries - to bring the
// loading screen up for the rest of the warm-up. Pure logic, so cos_pc_tests checks it on the host.
//
// On a warm boot the warm-up's first frame used to see 3-4 builds of 70-130 ms (they waited for
// the GL context the render worker held for ~300 ms; every one was a shader cache hit), and the old
// rule (3 slow builds, every pipeline left taken at their cost) put a 10 s loading screen on every
// warm boot. Now:
//   - a sample big enough first: at least minSlowBuilds slow builds among the last windowBuilds
//     builds, or minSlowS of slow work measured since the game started;
//   - the slow work left from the measured hit ratio: the pipelines left times the recent builds'
//     slow share times the slow builds' cost, not every pipeline left at the slow cost.

#include <cstdint>

namespace pc {

struct LateGateConfig {
    uint32_t windowBuilds = 16;  // the recent builds whose slow share predicts the rest
    uint32_t minSlowBuilds = 8;  // of those, the slow ones needed...
    double minSlowS = 1.5;       // ...or the slow work measured since the game started
    double screenMinS = 3.0;     // COS_PRECOMPILE_SCREEN_MIN_S: less slow work left needs no screen
};

struct LateGateVerdict {
    bool screen = false;        // bring the loading screen up
    bool sampleEnough = false;  // the sample rule above holds
    uint32_t windowBuilds = 0;  // the recent builds looked at (at least windowBuilds once there are)
    uint32_t windowSlow = 0;    // of those, slow
    double windowSlowS = 0;     // their slow builds' time
    double measuredSlowS = 0;   // every slow build's time since the game started
    double slowShare = 0;       // windowSlow / windowBuilds
    double slowEachMs = 0;      // windowSlowS / windowSlow
    double leftMs = 0;          // the slow work left: left * slowShare * slowEachMs
};

class LateGate {
public:
    explicit LateGate(const LateGateConfig& config = LateGateConfig()) : mConfig(config) {}

    const LateGateConfig& config() const { return mConfig; }
    void setConfig(const LateGateConfig& config) { mConfig = config; }

    // One frame's warm-up builds: how many finished, how many of them were slow (their own shader
    // work over COS_PRECOMPILE_SLOW_MS, the wait for the GL context left out) and those builds'
    // time; left = the warm-up's pipelines still to build.
    LateGateVerdict add(uint32_t builds, uint32_t slowBuilds, double slowS, uint32_t left) {
        if (slowBuilds > builds) {
            slowBuilds = builds;
        }
        if (builds > 0) {
            mRing[mPos] = {builds, slowBuilds, slowBuilds > 0 ? slowS : 0.0};
            mPos = (mPos + 1) % kFrames;
            if (mCount < kFrames) {
                mCount++;
            }
            if (slowBuilds > 0) {
                mMeasuredSlowS += slowS;
            }
        }
        LateGateVerdict v;
        v.measuredSlowS = mMeasuredSlowS;
        for (int i = 0; i < mCount && v.windowBuilds < mConfig.windowBuilds; i++) {
            const Sample& s = mRing[(mPos - 1 - i + kFrames) % kFrames];
            v.windowBuilds += s.builds;
            v.windowSlow += s.slowBuilds;
            v.windowSlowS += s.slowS;
        }
        if (v.windowBuilds == 0) {
            return v;
        }
        v.slowShare = (double)v.windowSlow / v.windowBuilds;
        v.slowEachMs = v.windowSlow > 0 ? v.windowSlowS * 1000.0 / v.windowSlow : 0.0;
        v.leftMs = left * v.slowShare * v.slowEachMs;
        v.sampleEnough = (v.windowBuilds >= mConfig.windowBuilds && v.windowSlow >= mConfig.minSlowBuilds) ||
                         (v.windowSlow > 0 && mMeasuredSlowS >= mConfig.minSlowS - 1e-9);
        v.screen = v.sampleEnough && v.leftMs > mConfig.screenMinS * 1000.0;
        return v;
    }

private:
    struct Sample {
        uint32_t builds;
        uint32_t slowBuilds;
        double slowS;
    };
    static constexpr int kFrames = 128; // frames remembered (a frame that built nothing adds none)
    LateGateConfig mConfig;
    Sample mRing[kFrames] = {};
    int mPos = 0;
    int mCount = 0;
    double mMeasuredSlowS = 0;
};

} // namespace pc
