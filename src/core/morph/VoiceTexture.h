#pragma once

#include "../dsp/LinearSmoother.h"

#include <array>

namespace vs
{

// "Carácter" texture for the second voice (Phase 4). Three moderate stages, all scaled by Carácter c in 0..1:
//   body     : low shelf 220 Hz,      +3.0 dB * c
//   presence : peak 3 kHz (Q 0.8),    +2.2 dB * c        (centred in the speech band, not a treble boost)
//   warmth   : parallel soft saturation of the band below 1.8 kHz only (sibilants are not saturated):
//              y = tanh(k x) * 0.1 / tanh(0.1 k),  k = 2 + 4 c,  mixed at 0.7 c  (unity gain at amplitude 0.1, about -20 dBFS)
// c = 0 is an exact bypass. Provisional values, not tuned by listening.
class VoiceTexture
{
public:
    static constexpr int maxChannels = 8;
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setAmount (float c01) { amountSmoother.setTarget (c01); }
    void process (float* const* io, int numChannels, int numSamples);

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float run (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    void design (float c);
    double fs = 48000.0;
    int channels = 0;
    LinearSmoother amountSmoother;
    std::array<Biquad, maxChannels> body {}, presence {};
    std::array<float, maxChannels> lp {};
    float lpA = 0.0f, mix = 0.0f, k = 2.0f, satNorm = 0.05f;
    float coefB[2][3] {}, coefA[2][2] {};
};

} // namespace vs
