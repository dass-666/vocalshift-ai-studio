#pragma once

#include "../dsp/LinearSmoother.h"
#include "../morph/ToneShaper.h"

namespace vs
{

// "Magia" (enhancement) stage, deliberately simple and measurable:
//   1. Stereo-linked feed-forward compressor (RMS detector, soft knee).
//        ratio     = 1 + 3 * magic       (1:1 .. 4:1)
//        threshold = -12 - 12 * magic    (dBFS)
//        makeup    = half the static reduction a 0 dBFS signal would get, max +6 dB
//   2. "Air" high shelf at 10 kHz, +5 dB * magic.
// magic = 0 is an exact bypass of both stages.
// This is NOT a "professional vocal chain"; it is a provisional enhancement stage
// to be tuned (or replaced) after listening tests.
class VocalEnhancer
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setMagic (float magic01) { magicSmoother.setTarget (magic01); }

    // In-place, real-time safe.
    void process (float* const* io, int numChannels, int numSamples);

    // Last block's maximum gain reduction (dB, >= 0), for tests/metering.
    float lastGainReductionDb() const { return lastGrDb; }

private:
    ToneShaper air;
    LinearSmoother magicSmoother;
    double fs = 48000.0;
    int channels = 0;
    float env = 0.0f;           // mean-square envelope
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float gainSmoothed = 1.0f, gainCoeff = 0.0f;
    float lastGrDb = 0.0f;
};

} // namespace vs
