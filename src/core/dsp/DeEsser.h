#pragma once

#include "LinearSmoother.h"

#include <array>

namespace vs
{

// Band-split de-esser for the processed path (part of "Magia" in the revised DSP).
//   high = HP(x) at 5 kHz (2nd-order Butterworth), low = x - high   (the split sums back exactly)
//   detector: stereo-linked envelope of `high` (attack 1 ms, release 40 ms)
//   gain reduction on `high` only: ratio 4:1 above -34 dBFS (soft knee 6 dB), at most 9 dB
//   amount scales with Magia: 0 % = exact bypass, 100 % = full depth.
// Voiced material has little energy above 5 kHz, so it is left alone; sibilants are attenuated
// instead of being amplified by the later "air" shelf. Thresholds are PROVISIONAL (engineering
// values, not tuned by listening).
class DeEsser
{
public:
    static constexpr int maxChannels = 8;
    static constexpr float crossoverHz = 5000.0f;
    static constexpr float thresholdDb = -34.0f;
    static constexpr float ratio = 4.0f;
    static constexpr float maxReductionDb = 9.0f;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setAmount (float a01) { amountSmoother.setTarget (a01); }
    void process (float* const* io, int numChannels, int numSamples);
    float lastReductionDb() const { return lastGr; }

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
    std::array<Biquad, maxChannels> hp {};
    LinearSmoother amountSmoother;
    int channels = 0;
    float env = 0.0f, attack = 0.0f, release = 0.0f, gainCoeff = 0.0f, gain = 1.0f, lastGr = 0.0f;
};

} // namespace vs
