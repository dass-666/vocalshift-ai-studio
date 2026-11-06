#include "VocalMixEngine.h"

#include <algorithm>
#include <cmath>

namespace vs
{

void VocalMixEngine::prepare (double sampleRate, int numChannels, int maxBlockSize, int latencySamples)
{
    fs = sampleRate;
    channels = numChannels;
    latency = std::max (0, latencySamples);
    delayLength = latency + std::max (1, maxBlockSize) + 1;
    delay.assign ((size_t) numChannels, std::vector<float> ((size_t) delayLength, 0.0f));
    wetDelayLength = (int) std::ceil (sampleRate * maxOffsetMs / 1000.0) + 4;
    wetDelay.assign ((size_t) numChannels, std::vector<float> ((size_t) wetDelayLength, 0.0f));
    writePos = wetWritePos = 0;
    clipCount = 0;

    dryGainSmoother.prepare (sampleRate, 0.03);
    wetGainSmoother.prepare (sampleRate, 0.03);
    panSmoother.prepare (sampleRate, 0.03);
    gainSmoother.prepare (sampleRate, 0.03);
    bypassSmoother.prepare (sampleRate, 0.02);
    for (auto* s : { &dryGainSmoother, &wetGainSmoother, &panSmoother, &gainSmoother, &bypassSmoother })
        s->snapTo (s->getTarget());
    wetDelayCurrent = wetDelayTarget;
}

void VocalMixEngine::reset()
{
    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);
    for (auto& d : wetDelay)
        std::fill (d.begin(), d.end(), 0.0f);
    writePos = wetWritePos = 0;
}

void VocalMixEngine::setRouting (bool secondVoice, float mix01, float secondGainDb, float pan01, float offsetMs)
{
    if (secondVoice)
    {
        dryGainSmoother.setTarget (1.0f);
        wetGainSmoother.setTarget (secondGainDb <= faderSilenceDb ? 0.0f : std::pow (10.0f, secondGainDb / 20.0f));
        panSmoother.setTarget (pan01);
        wetDelayTarget = (float) (std::clamp (offsetMs, 0.0f, maxOffsetMs) * 0.001 * fs);
    }
    else
    {
        dryGainSmoother.setTarget (1.0f - mix01);
        wetGainSmoother.setTarget (mix01);
        panSmoother.setTarget (0.0f);
        wetDelayTarget = 0.0f;
    }
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
    constexpr float threshold = softClipThreshold;
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
    constexpr float slewPerSample = 0.02f; // max 2 % pitch bend while the offset knob moves
    long clipped = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dG = dryGainSmoother.next();
        const float wG = wetGainSmoother.next();
        const float pan = panSmoother.next();
        const float g = gainSmoother.next();
        const float b = bypassSmoother.next();

        // Slew-limited extra delay of the processed path (0 in the transform route).
        const float diff = wetDelayTarget - wetDelayCurrent;
        wetDelayCurrent += std::clamp (diff, -slewPerSample, slewPerSample);
        const float dl = wetDelayCurrent;
        const int di = (int) dl;
        const float frac = dl - (float) di;

        const float gain = 1.0f + (g - 1.0f) * (1.0f - b);

        for (int c = 0; c < nCh; ++c)
        {
            auto& wd = wetDelay[(size_t) c];
            wd[(size_t) wetWritePos] = wet[c][i];
            int r0 = wetWritePos - di;
            if (r0 < 0) r0 += wetDelayLength;
            int r1 = r0 - 1;
            if (r1 < 0) r1 += wetDelayLength;
            const float w = frac > 0.0f ? wd[(size_t) r0] * (1.0f - frac) + wd[(size_t) r1] * frac : wd[(size_t) r0];

            float pc = 1.0f; // balance-style pan: centre = unity on both channels
            if (nCh == 2)
                pc = c == 0 ? (pan > 0.0f ? 1.0f - pan : 1.0f) : (pan < 0.0f ? 1.0f + pan : 1.0f);

            const float d = dry[c][i];
            const float processed = dG * d + wG * pc * w;
            float y = processed + (d - processed) * b;
            // Bypass means "unchanged signal": output gain and clipper fade out with it.
            y *= gain;
            if (safetyClip && b < 1.0f)
            {
                if (std::abs (y) > softClipThreshold)
                    ++clipped;
                y = softClip (y);
            }
            out[c][i] = y;
        }
        if (++wetWritePos == wetDelayLength)
            wetWritePos = 0;
    }
    if (clipped > 0)
        clipCount.fetch_add (clipped, std::memory_order_relaxed);
}

} // namespace vs
