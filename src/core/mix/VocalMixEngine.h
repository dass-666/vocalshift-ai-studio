#pragma once

#include "../dsp/LinearSmoother.h"

#include <vector>

namespace vs
{

// VocalMixEngine: latency-aligned dry path, dry/wet mix, click-free bypass,
// output gain, optional soft-clip peak protection.
class VocalMixEngine
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlockSize, int latencySamples);
    void reset();

    void setMix (float wet01) { mixSmoother.setTarget (wet01); }
    void setOutputGainDb (float db);
    void setBypassed (bool b) { bypassSmoother.setTarget (b ? 1.0f : 0.0f); }
    void setSafetyClip (bool on) { safetyClip = on; }

    // Writes the input into the dry delay line and the delayed dry into dryOut.
    void pushDry (const float* const* in, float* const* dryOut, int numChannels, int numSamples);

    // out = gain * lerp(lerp(dry, wet, mix), dry, bypass), then soft clip.
    // out may alias wet.
    void mixInto (const float* const* wet, const float* const* dry, float* const* out,
                  int numChannels, int numSamples);

    // Soft clipper: linear below -1 dBFS, smooth knee, never exceeds 1.0.
    static float softClip (float x);

    bool isFullyBypassed() const { return bypassSmoother.getCurrent() >= 1.0f && ! bypassSmoother.isSmoothing(); }

private:
    std::vector<std::vector<float>> delay;
    int delayLength = 1, writePos = 0, latency = 0, channels = 0;
    LinearSmoother mixSmoother, gainSmoother, bypassSmoother;
    bool safetyClip = true;
};

} // namespace vs
