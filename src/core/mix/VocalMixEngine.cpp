#include "VocalMixEngine.h"

#include <algorithm>
#include <cmath>

namespace vs
{

void VocalMixEngine::prepare (double sampleRate, int numChannels, int maxBlockSize, int latencySamples)
{
    channels = numChannels;
    latency = std::max (0, latencySamples);
    delayLength = latency + std::max (1, maxBlockSize) + 1;
    delay.assign ((size_t) numChannels, std::vector<float> ((size_t) delayLength, 0.0f));
    writePos = 0;

    mixSmoother.prepare (sampleRate, 0.03);
    gainSmoother.prepare (sampleRate, 0.03);
    bypassSmoother.prepare (sampleRate, 0.02);
    mixSmoother.snapTo (mixSmoother.getTarget());
    gainSmoother.snapTo (gainSmoother.getTarget());
    bypassSmoother.snapTo (bypassSmoother.getTarget());
}

void VocalMixEngine::reset()
{
    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);
    writePos = 0;
}

void VocalMixEngine::setOutputGainDb (float db)
{
    gainSmoother.setTarget (std::pow (10.0f, db / 20.0f));
}

void VocalMixEngine::pushDry (const float* const* in, float* const* dryOut, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    int pos = writePos;
    for (int c = 0; c < nCh; ++c)
    {
        auto& d = delay[(size_t) c];
        pos = writePos;
        for (int i = 0; i < numSamples; ++i)
        {
            d[(size_t) pos] = in[c][i];
            int readPos = pos - latency;
            if (readPos < 0)
                readPos += delayLength;
            dryOut[c][i] = d[(size_t) readPos];
            if (++pos == delayLength)
                pos = 0;
        }
    }
    writePos = pos;
}

float VocalMixEngine::softClip (float x)
{
    constexpr float threshold = 0.891250938f; // -1 dBFS
    constexpr float range = 1.0f - threshold;
    const float a = std::abs (x);
    if (a <= threshold)
        return x;
    const float y = threshold + range * std::tanh ((a - threshold) / range);
    return x < 0.0f ? -y : y;
}

void VocalMixEngine::mixInto (const float* const* wet, const float* const* dry, float* const* out,
                              int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = mixSmoother.next();
        const float g = gainSmoother.next();
        const float b = bypassSmoother.next();
        for (int c = 0; c < nCh; ++c)
        {
            const float d = dry[c][i];
            const float processed = d + (wet[c][i] - d) * m;
            float y = (processed + (d - processed) * b);
            // Bypass means "unchanged signal": output gain and clipper fade out with it.
            const float gain = 1.0f + (g - 1.0f) * (1.0f - b);
            y *= gain;
            if (safetyClip && b < 1.0f)
                y = softClip (y);
            out[c][i] = y;
        }
    }
}

} // namespace vs
