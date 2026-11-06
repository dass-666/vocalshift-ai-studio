#pragma once

#include "../dsp/LinearSmoother.h"

#include <array>

namespace vs
{

// RBJ high-shelf with smoothed gain. Used for "Brillo" (2.5 kHz, MorphEngine DSP part)
// and for the "air" stage of Magia (10 kHz). It is NOT voice conversion.
class ToneShaper
{
public:
    static constexpr int maxChannels = 8;

    void prepare (double sampleRate, int numChannels, float shelfFrequencyHz = 2500.0f);
    void reset();
    void setBrightnessDb (float db) { gainSmoother.setTarget (db); }

    // In-place. Real-time safe.
    void process (float* const* io, int numChannels, int numSamples);

private:
    void updateCoefficients (float gainDb);

    struct State { float z1 = 0.0f, z2 = 0.0f; };
    std::array<State, maxChannels> state {};
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float coeffGainDb = 0.0f;
    float frequencyHz = 2500.0f;
    double fs = 48000.0;
    int channels = 0;
    LinearSmoother gainSmoother;
};

} // namespace vs
