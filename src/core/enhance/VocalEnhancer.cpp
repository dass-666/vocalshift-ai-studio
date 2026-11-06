#include "VocalEnhancer.h"

#include <algorithm>
#include <cmath>

namespace vs
{

static float coeffFor (double fs, double seconds)
{
    return (float) std::exp (-1.0 / (fs * seconds));
}

void VocalEnhancer::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = std::min (numChannels, (int) ToneShaper::maxChannels);
    air.prepare (sampleRate, channels, 10000.0f);
    magicSmoother.prepare (sampleRate, 0.05);
    magicSmoother.snapTo (magicSmoother.getTarget());
    air.setBrightnessDb (5.0f * magicSmoother.getCurrent());
    attackCoeff = coeffFor (sampleRate, 0.010);
    releaseCoeff = coeffFor (sampleRate, 0.150);
    gainCoeff = coeffFor (sampleRate, 0.003);
    reset();
}

void VocalEnhancer::reset()
{
    air.reset();
    env = 0.0f;
    gainSmoothed = 1.0f;
    lastGrDb = 0.0f;
}

void VocalEnhancer::process (float* const* io, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    lastGrDb = 0.0f;
    constexpr int sub = 32;
    constexpr float kneeDb = 6.0f;

    for (int start = 0; start < numSamples; start += sub)
    {
        const int n = std::min (sub, numSamples - start);
        const float m = magicSmoother.advance (n);
        air.setBrightnessDb (5.0f * m);

        if (m <= 0.0f && ! magicSmoother.isSmoothing() && gainSmoothed == 1.0f)
        {
            // Exact bypass, but keep the detector running so re-enabling is smooth.
            for (int i = 0; i < n; ++i)
            {
                float ms = 0.0f;
                for (int c = 0; c < nCh; ++c)
                    ms = std::max (ms, io[c][start + i] * io[c][start + i]);
                env = ms + (ms > env ? attackCoeff : releaseCoeff) * (env - ms);
            }
            continue;
        }

        const float ratio = 1.0f + 3.0f * m;
        const float thresholdDb = -12.0f - 12.0f * m;
        // Static reduction a full-scale signal would get, halved: moderate makeup.
        const float makeupDb = std::min (6.0f, 0.5f * (-thresholdDb) * (1.0f - 1.0f / ratio));

        for (int i = 0; i < n; ++i)
        {
            float ms = 0.0f;
            for (int c = 0; c < nCh; ++c)
                ms = std::max (ms, io[c][start + i] * io[c][start + i]);
            env = ms + (ms > env ? attackCoeff : releaseCoeff) * (env - ms);

            const float levelDb = 10.0f * std::log10 (env + 1.0e-12f);
            const float over = levelDb - thresholdDb;
            float grDb = 0.0f;
            if (over > kneeDb * 0.5f)
                grDb = over * (1.0f - 1.0f / ratio);
            else if (over > -kneeDb * 0.5f)
            {
                const float x = over + kneeDb * 0.5f;
                grDb = (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeDb);
            }
            lastGrDb = std::max (lastGrDb, grDb);
            const float target = std::pow (10.0f, (makeupDb - grDb) / 20.0f);
            gainSmoothed = target + gainCoeff * (gainSmoothed - target);
            for (int c = 0; c < nCh; ++c)
                io[c][start + i] *= gainSmoothed;
        }
        if (m <= 0.0f && std::abs (gainSmoothed - 1.0f) < 1.0e-4f)
            gainSmoothed = 1.0f;
    }

    air.process (io, nCh, numSamples);
}

} // namespace vs
