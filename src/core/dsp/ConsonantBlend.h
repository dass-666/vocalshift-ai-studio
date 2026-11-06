#pragma once

#include "LinearSmoother.h"

#include <array>

namespace vs
{

// Consonant (fricative) preservation for the processed path.
//
// Problem measured in the spectral pitch engine: it maps every frequency by the pitch ratio,
// including the noise of "s", "sh", "f". A fricative centred at 6 kHz ends up at ~9 kHz after +7 st,
// which is not what a real higher voice does (fricative noise barely moves). Result: shrill, whistly "s".
//
// Fix: above `crossoverHz` the processed signal is replaced by the original (latency-aligned) signal,
// but only while the original is sibilant:
//     wet' = wet + w * ( HP(dry) - HP(wet) )              w = sibilance detector in [0, 1]
// with w = 0 in voiced frames (the processed signal is untouched) and w = 1 in fricatives.
// Sibilance = share of the original's power above the crossover: w = 0 below -12 dB, 1 above -6 dB.
// Voiced vowels have much less than -12 dB there, so the pitched path is not affected.
//
// Limits: plosive bursts (p, t, k) are broadband and short; they are only partly handled. Aspirated or
// very bright singing above the threshold would get original-pitch noise above the crossover.
class ConsonantBlend
{
public:
    static constexpr int maxChannels = 8;
    static constexpr float crossoverHz = 4500.0f;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setEnabled (bool e) { amountSmoother.setTarget (e ? 1.0f : 0.0f); }

    // wet is modified in place; dry is only analysed. Real-time safe.
    void process (const float* const* dry, float* const* wet, int numChannels, int numSamples);

    float lastBlendAmount() const { return lastW; } // max w of the last block, for tests/metering

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
    std::array<Biquad, maxChannels> hpDry {}, hpWet {};
    LinearSmoother amountSmoother;
    int channels = 0;
    float fullEnv = 0.0f, hiEnv = 0.0f, w = 0.0f, lastW = 0.0f;
    float envAttack = 0.0f, envRelease = 0.0f, wAttack = 0.0f, wRelease = 0.0f;
};

} // namespace vs
