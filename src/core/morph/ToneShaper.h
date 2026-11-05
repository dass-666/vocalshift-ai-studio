#pragma once

#include "../dsp/LinearSmoother.h"

#include <array>

namespace vs
{

// MorphEngine (DSP part, V0): "Brillo" = RBJ high-shelf at 2.5 kHz, +-12 dB.
// Together with the formant shift inside the pitch engine, this is the only
// timbre transformation implemented today. It is NOT voice conversion.
class ToneShaper
{
public:
    static constexpr int maxChannels = 8;
    static constexpr float shelfFrequencyHz = 2500.0f;

    void prepare (double sampleRate, int numChannels);
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
    double fs = 48000.0;
    int channels = 0;
    LinearSmoother gainSmoother;
};

} // namespace vs
