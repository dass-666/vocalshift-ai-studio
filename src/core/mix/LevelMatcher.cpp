#include "LevelMatcher.h"

#include <algorithm>
#include <cmath>

namespace vs
{

void LevelMatcher::prepare (double sampleRate)
{
    msCoeff = (float) std::exp (-1.0 / (sampleRate * 0.4));
    gainCoeff = (float) std::exp (-1.0 / (sampleRate * 0.3));
    shortCoeff = (float) std::exp (-1.0 / (sampleRate * 0.03));
    holdSamples = (int) (sampleRate * 0.2);
    dipMaxSamples = (int) (sampleRate * 0.15);
    reset();
}

void LevelMatcher::reset()
{
    refMs = wetMs = shortMs = 0.0f;
    gain = 1.0f;
    holdCounter = 0;
    dipCount = 0;
    active = false;
}

float LevelMatcher::currentGainDb() const
{
    return 20.0f * std::log10 (gain);
}

void LevelMatcher::process (const float* const* dry, float* const* wet, int numChannels, int numSamples)
{
    constexpr float minGain = 0.5011872f; // -6 dB
    const float k = 1.0f / (float) std::max (1, numChannels);

    if (! robust)
    {
        constexpr float gateMs = 1.0e-5f;      // -50 dBFS
        constexpr float maxGain = 7.943282f;   // +18 dB
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
        return;
    }

    constexpr float openMs = 1.0e-5f;     // -50 dBFS
    constexpr float closeMs = 2.5e-6f;    // -56 dBFS
    constexpr float maxGain = 3.9810717f; // +12 dB
    constexpr float dipRatio = 0.0316f;   // 15 dB below the running level

    for (int i = 0; i < numSamples; ++i)
    {
        float d2 = 0.0f, w2 = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            d2 += dry[c][i] * dry[c][i];
            w2 += wet[c][i] * wet[c][i];
        }
        d2 *= k;
        w2 *= k;
        shortMs = d2 + shortCoeff * (shortMs - d2);

        // Voice activity with hysteresis, hold and "dip" rejection.
        if (! active)
        {
            if (shortMs > openMs)
            {
                active = true;
                holdCounter = holdSamples;
            }
        }
        else if (shortMs < closeMs)
        {
            if (--holdCounter <= 0)
                active = false;
        }
        else
            holdCounter = holdSamples;

        bool dip = refMs > 0.0f && shortMs < dipRatio * refMs;
        if (dip)
        {
            if (++dipCount > dipMaxSamples)
                dip = false; // sustained: a genuinely quieter passage, not a consonant or a gap
        }
        else
            dipCount = 0;
        if (! enabled)
        {
            gain = 1.0f + gainCoeff * (gain - 1.0f);
        }
        else if (active && ! dip)
        {
            refMs = d2 + msCoeff * (refMs - d2);
            wetMs = w2 + msCoeff * (wetMs - w2);
            if (wetMs > 1.0e-9f && refMs > 0.0f)
            {
                const float target = std::clamp (std::sqrt (refMs / wetMs), minGain, maxGain);
                gain = target + gainCoeff * (gain - target);
            }
        }
        // otherwise: hold the previous gain and leave both running levels untouched

        for (int c = 0; c < numChannels; ++c)
            wet[c][i] *= gain;
    }
}

} // namespace vs
