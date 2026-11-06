#pragma once

#include "../dsp/LinearSmoother.h"

#include <atomic>
#include <vector>

namespace vs
{

// VocalMixEngine: latency-aligned original ("dry") path, processed ("wet") path,
// two routes, click-free bypass, output gain, last-resort soft clip.
//
//   transform route   : out = (1 - mix) * dry + mix * wet
//   second-voice route: out = dry + gain * pan * delay(wet, offset)       (dry at unity)
//
// The dry path is only delayed (by the engine latency): no filtering, no gain change.
// Route changes are 30 ms crossfades of the two gains, so the alignment never moves.
class VocalMixEngine
{
public:
    static constexpr float maxOffsetMs = 100.0f;
    static constexpr float faderSilenceDb = -59.9f; // at or below: second voice fully muted

    void prepare (double sampleRate, int numChannels, int maxBlockSize, int latencySamples);
    void reset();

    // Targets (smoothed). In the transform route pan and offset ramp back to centre / 0 ms.
    void setRouting (bool secondVoice, float mix01, float secondGainDb, float pan01, float offsetMs);
    void setMix (float wet01) { setRouting (false, wet01, -9.0f, 0.0f, 0.0f); } // legacy helper
    void setOutputGainDb (float db);
    void setBypassed (bool b) { bypassSmoother.setTarget (b ? 1.0f : 0.0f); }
    void setSafetyClip (bool on) { safetyClip = on; }

    // Writes the input into the dry delay line and the delayed dry into dryOut.
    void pushDry (const float* const* in, float* const* dryOut, int numChannels, int numSamples);

    // out may alias wet.
    void mixInto (const float* const* wet, const float* const* dry, float* const* out,
                  int numChannels, int numSamples);

    // Soft clipper: linear below -1 dBFS, smooth knee, never exceeds 1.0.
    static float softClip (float x);
    static constexpr float softClipThreshold = 0.891250938f;

    // Number of output samples the soft clip has altered since prepare() (a sign of gain
    // staging problems, not something to rely on).
    long clipAlteredSamples() const { return clipCount.load (std::memory_order_relaxed); }

    bool isFullyBypassed() const { return bypassSmoother.getCurrent() >= 1.0f && ! bypassSmoother.isSmoothing(); }

private:
    std::vector<std::vector<float>> delay, wetDelay;
    int delayLength = 1, writePos = 0, latency = 0, channels = 0;
    int wetDelayLength = 1, wetWritePos = 0;
    double fs = 48000.0;
    float wetDelayCurrent = 0.0f, wetDelayTarget = 0.0f; // samples
    LinearSmoother dryGainSmoother, wetGainSmoother, panSmoother, gainSmoother, bypassSmoother;
    bool safetyClip = true;
    std::atomic<long> clipCount { 0 };
};

} // namespace vs
