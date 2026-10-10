#pragma once

// 60 fps step E (docs/FPS60_PLAN.md): paint B's budget guard and the steady-30 fallback. Pure logic,
// so cos_pc_tests checks it on the host; pc_frame.cpp feeds it the game thread's times.
//
// A split game frame runs, from paint A's wake (the end of its pace wait) to the next paint A's:
// paint A + logic (the elapsed time at the decision, pc_frame_split, before paint A is presented),
// the split's present, the draw pass, paint B's wait (until one retrace after paint A's wake), paint
// B, and the tail (paint B's present, the end and start of the frame, up to the next paint A's
// wait). The game keeps its speed only if all of it fits in two retraces; a paint B that does not
// fit makes the game frame late (the game slows down) and its present miss its retrace. So before
// the split the guard predicts the rest from running estimates and presents the frame once (no
// split, as at 30 fps) when the prediction is over the budget, or when the GPU cannot draw two
// presents a game frame (the caller's gpuLimited).
//
// Skips scattered over the frames give an uneven 40-50 presents/s (judder), worse to the eye than
// a steady 30: when more than fallbackRatio of the last `window` eligible frames were bad (paint B
// skipped, or a split frame that overran), the guard presents every frame once for fallbackFrames,
// then tries paint B again. A fallback that comes back within healthyFrames of the retry doubles
// the next one (up to fallbackMaxFrames); healthyFrames without one resets it. A fallback ends early
// when the prediction has stayed earlyHeadroomNs under both the budget and the prediction of the
// frame that started it for earlyRetryFrames (the scene got lighter: no 24 s wait at 30 after a
// heavy spot), unless the GPU is the limit. After a fallback the estimates only split frames refresh
// (the split's present, paint B) start over, so the first frame after it is a probe.

#include <cstdint>

namespace pc {

struct Fps60BudgetConfig {
    uint64_t retraceNs = 16'683'333;   // one retrace (paint B's wait ends one after paint A's wake)
    uint64_t budgetNs = 32'366'667;    // two retraces less 1 ms of margin (COS_FPS60_BUDGET_MS)
    uint64_t lateNs = 34'366'667;      // a split frame this long (two retraces + 1 ms) overran
    unsigned int window = 60;          // eligible frames looked at (2 s of game frames)
    unsigned int minWindow = 20;       // fewer than this since a reset: no fallback yet
    double fallbackRatio = 0.25;       // bad frames over this share: fallback
    unsigned int fallbackFrames = 90;  // 3 s at 30 game frames a second
    unsigned int fallbackMaxFrames = 720; // 24 s
    unsigned int healthyFrames = 600;  // 20 s
    uint64_t earlyHeadroomNs = 3'000'000; // an early retry needs the prediction this far under...
    unsigned int earlyRetryFrames = 60;   // ...for this many frames in a row (2 s)
};

// A running estimate biased high: it rises by half the gap, falls by a sixteenth, so the estimate
// sits near the samples' upper range and a lighter scene is followed over ~1 s. A lone spike (over
// twice the estimate + 4 ms: a pipeline built, a page fault, a disc read) is set aside: it counts
// only when another spike came within the last ~2 samples (a recurring cost, e.g. every other frame
// heavy), so one hitch does not skip the following paints B.
struct Fps60Estimate {
    double ns = 0.0;
    double spikes = 0.0; // recent spikes, decaying by 3/4 a sample
    bool valid = false;
    void add(double sample) {
        if (!valid) {
            ns = sample;
            valid = true;
            return;
        }
        const double recent = spikes;
        spikes *= 0.75;
        if (sample > ns * 2.0 + 4e6) {
            spikes += 1.0;
            if (recent < 0.5) {
                return; // a lone spike: set aside
            }
        }
        if (sample > ns) {
            ns += (sample - ns) * 0.5;
        } else {
            ns += (sample - ns) * (1.0 / 16.0);
        }
    }
    double get() const { return valid ? ns : 0.0; }
};

enum class Fps60Verdict {
    Split,      // paint B
    SkipBudget, // presented once: the prediction is over the budget
    SkipGpu,    // presented once: the GPU cannot draw two presents a game frame
    Fallback,   // presented once: steady 30 for a while
};

class Fps60Budget {
public:
    Fps60BudgetConfig cfg;
    Fps60Estimate present; // the split: aurora_end_frame + aurora_begin_frame
    Fps60Estimate draw;    // after the split (or the decision) to the end of the draw pass
    Fps60Estimate paintB;  // paint B without its wait
    Fps60Estimate tail;    // paint B's (or the draw pass's) end to the next paint A's wait

    explicit Fps60Budget(const Fps60BudgetConfig& c = Fps60BudgetConfig()) : cfg(c) { configure(c); }

    // New settings (COS_FPS60_BUDGET_MS): the window is capped at kMaxWindow; forgets the window.
    void configure(const Fps60BudgetConfig& c) {
        cfg = c;
        if (cfg.window > kMaxWindow) {
            cfg.window = kMaxWindow;
        }
        reset();
    }

    // The predicted game frame if paint B is done, with paint A's wake `elapsedNs` ago.
    uint64_t predict(uint64_t elapsedNs) const {
        double beforeB = (double)elapsedNs + present.get() + draw.get();
        if (beforeB < (double)cfg.retraceNs) {
            beforeB = (double)cfg.retraceNs; // paint B waits for its retrace
        }
        const double p = beforeB + paintB.get() + tail.get() - (biasValid_ && bias_ > 0.0 ? bias_ : 0.0);
        return p > 0.0 ? (uint64_t)p : 0;
    }

    // A split frame's real busy time (paint A's wake to the next paint A's wait) against what
    // predict() said at its split: the estimates add waits that overlap (GXDrawDone, the frame
    // tail's backpressure from the GX and render workers, which run beside the game thread), so the
    // sum overshoots (console, Dragon Roost: predicted 35 ms, real 26-28 ms, no late frame). The
    // over-prediction's running average is taken off later predictions; under-predictions count
    // fully at once so a heavier scene is caught at its first sample.
    void observeSplit(uint64_t predictedRawNs, uint64_t actualNs) {
        if (predictedRawNs == 0 || actualNs == 0) {
            return;
        }
        const double err = (double)predictedRawNs - (double)actualNs;
        if (!biasValid_) {
            bias_ = err;
            biasValid_ = true;
        } else if (err < bias_) {
            bias_ = err;
        } else {
            bias_ += (err - bias_) * 0.1;
        }
    }
    double bias() const { return biasValid_ ? bias_ : 0.0; }
    uint64_t lastPredictedRaw() const { return lastPredictedRaw_; }

    // At the split of an eligible frame (60 fps on, two retraces, the menu closed, no transition).
    Fps60Verdict decide(uint64_t elapsedNs, bool gpuLimited, uint64_t* predictedNs) {
        const uint64_t p = predict(elapsedNs);
        lastPredictedRaw_ = p + (uint64_t)(biasValid_ && bias_ > 0.0 ? bias_ : 0.0);
        if (predictedNs != nullptr) {
            *predictedNs = p;
        }
        lastPredicted_ = p;
        retriedEarly_ = false;
        if (fallbackLeft_ > 0) {
            const uint64_t limit = cfg.budgetNs < fallbackStartPredicted_ ? cfg.budgetNs : fallbackStartPredicted_;
            if (!gpuLimited && p + cfg.earlyHeadroomNs < limit) {
                goodRun_++;
            } else {
                goodRun_ = 0;
            }
            if (goodRun_ < cfg.earlyRetryFrames) {
                return Fps60Verdict::Fallback;
            }
            // the scene got lighter: paint B again now (logged by the caller, retriedEarly)
            fallbackLeft_ = 0;
            sinceRetry_ = 0;
            clearWindow();
            forgetSplitEstimates();
            retriedEarly_ = true;
        }
        if (gpuLimited) {
            return Fps60Verdict::SkipGpu;
        }
        return p > cfg.budgetNs ? Fps60Verdict::SkipBudget : Fps60Verdict::Split;
    }

    // Once per eligible frame outside a fallback, when its outcome is known: bad = paint B skipped,
    // or the split frame overran (lateNs). True when this starts a fallback (log it).
    bool frameResult(bool bad) {
        ring_[pos_] = bad;
        pos_ = (pos_ + 1) % kMaxWindow;
        if (count_ < cfg.window) {
            count_++;
        }
        if (sinceRetry_ < UINT32_MAX) {
            sinceRetry_++;
        }
        if (sinceRetry_ >= cfg.healthyFrames) {
            nextFallback_ = cfg.fallbackFrames; // healthy for a while: back to the short fallback
        }
        if (count_ < cfg.minWindow || (double)badInWindow() <= cfg.fallbackRatio * (double)count_) {
            return false;
        }
        lastBad_ = badInWindow();
        lastCount_ = count_;
        fallbackStartPredicted_ = lastPredicted_;
        goodRun_ = 0;
        fallbackLeft_ = nextFallback_;
        lastFallback_ = nextFallback_;
        fallbacks_++;
        // the next one, if it comes back soon after the retry, lasts twice as long
        nextFallback_ = nextFallback_ * 2 > cfg.fallbackMaxFrames ? cfg.fallbackMaxFrames : nextFallback_ * 2;
        clearWindow();
        return true;
    }

    // Once per game frame of the 60 fps mode (eligible or not). True when a fallback just ended:
    // paint B is tried again from this frame (log it).
    bool tick() {
        if (fallbackLeft_ == 0) {
            return false;
        }
        if (--fallbackLeft_ == 0) {
            sinceRetry_ = 0;
            clearWindow();
            forgetSplitEstimates();
            return true;
        }
        return false;
    }

    // The mode turned off or on, or a long interruption: forget the window and any fallback (the
    // estimates stay).
    void reset() {
        fallbackLeft_ = 0;
        nextFallback_ = cfg.fallbackFrames;
        sinceRetry_ = UINT32_MAX;
        clearWindow();
    }

    bool inFallback() const { return fallbackLeft_ > 0; }
    // The last decide() ended a fallback early (the prediction came down).
    bool retriedEarly() const { return retriedEarly_; }
    unsigned int fallbackLeft() const { return fallbackLeft_; }
    unsigned int lastFallbackFrames() const { return lastFallback_; }
    unsigned int nextFallbackFrames() const { return nextFallback_; }
    unsigned int windowCount() const { return count_; }
    unsigned int lastBad() const { return lastBad_; }
    unsigned int lastCount() const { return lastCount_; }
    unsigned int fallbacks() const { return fallbacks_; }
    unsigned int badInWindow() const {
        unsigned int bad = 0;
        for (unsigned int i = 0; i < count_; i++) {
            bad += ring_[(pos_ + kMaxWindow - 1 - i) % kMaxWindow] ? 1 : 0;
        }
        return bad;
    }

private:
    static constexpr unsigned int kMaxWindow = 256;
    bool ring_[kMaxWindow] = {};
    double bias_ = 0.0;
    bool biasValid_ = false;
    uint64_t lastPredictedRaw_ = 0;
    unsigned int pos_ = 0;
    unsigned int count_ = 0;
    unsigned int fallbackLeft_ = 0;
    unsigned int nextFallback_ = 0;
    unsigned int lastFallback_ = 0;
    uint32_t sinceRetry_ = UINT32_MAX;
    unsigned int lastBad_ = 0, lastCount_ = 0, fallbacks_ = 0;
    uint64_t lastPredicted_ = 0;           // the last decide()'s prediction
    uint64_t fallbackStartPredicted_ = 0;  // the prediction when the fallback started
    unsigned int goodRun_ = 0;             // frames in a row a fallback could have ended
    bool retriedEarly_ = false;

    // The split's present and paint B are measured only in split frames, so during a fallback their
    // estimates are not refreshed: one spike before it (a hitch, a shader build) would otherwise skip
    // paint B after every retry. After a fallback they start over from the first split's samples
    // (0 until then: the first frame after the retry is split, a probe).
    void forgetSplitEstimates() {
        present = Fps60Estimate();
        paintB = Fps60Estimate();
    }

    void clearWindow() {
        count_ = 0;
        pos_ = 0;
    }
};

} // namespace pc
