#include "LevelMatcher.h"

#include <algorithm>
#include <cmath>

namespace vs
{

void LevelMatcher::prepare (double sampleRate)
{
    msCoeff = (float) std::exp (-1.0 / (sampleRate * 0.4));
    gainCoeff = (float) std::exp (-1.0 / (sampleRate * 0.3));
    reset();
}

void LevelMatcher::reset()
{
    refMs = wetMs = 0.0f;
    gain = 1.0f;
}

float LevelMatcher::currentGainDb() const
{
    return 20.0f * std::log10 (gain);
}

void LevelMatcher::process (const float* const* dry, float* const* wet, int numChannels, int numSamples)
{
    constexpr float gateMs = 1.0e-5f;   // -50 dBFS
    constexpr float minGain = 0.5011872f; // -6 dB
    constexpr float maxGain = 7.943282f;  // +18 dB
    const float k = 1.0f / (float) std::max (1, numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        float d2 = 0.0f, w2 = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            d2 += dry[c][i] * dry[c][i];
            w2 += wet[c][i] * wet[c][i];
        }
        refMs = d2 * k + msCoeff * (refMs - d2 * k);
        wetMs = w2 * k + msCoeff * (wetMs - w2 * k);

        float target = gain; // hold by default
        if (! enabled)
            target = 1.0f;
        else if (refMs > gateMs && wetMs > 1.0e-9f)
            target = std::clamp (std::sqrt (refMs / wetMs), minGain, maxGain);

        gain = target + gainCoeff * (gain - target);
        for (int c = 0; c < numChannels; ++c)
            wet[c][i] *= gain;
    }
}

} // namespace vs
