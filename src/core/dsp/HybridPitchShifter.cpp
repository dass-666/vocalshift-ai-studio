#include "HybridPitchShifter.h"

#include <algorithm>

namespace vs
{

void HybridPitchShifter::prepare (double sampleRate, int numChannels, int maxBlockSize)
{
    fs = sampleRate;
    channels = std::clamp (numChannels, 1, 8);
    sig.prepare (sampleRate, channels, maxBlockSize);
    latency = sig.latencySamples();
    psola.setLatencyTarget (latency);
    psola.prepare (sampleRate, channels, maxBlockSize);
    if (psola.latencySamples() != latency)
        latency = std::max (latency, psola.latencySamples()); // cannot align: caller must not use the hybrid (checked in tests)
    scratch.assign ((size_t) channels, std::vector<float> ((size_t) std::max (1, maxBlockSize), 0.0f));
    reset();
}

void HybridPitchShifter::reset()
{
    sig.reset();
    psola.reset();
    weight = 0.0f;
    state = false;
    qualifyCount = exitCount = switches = totalSamples = 0;
    dwellCount = (long) (minDwell * fs);
    psolaWeightSum = 0.0;
}

void HybridPitchShifter::setSettings (const PitchSettings& s)
{
    sig.setSettings (s);
    psola.setSettings (s);
}

void HybridPitchShifter::process (const float* const* in, float* const* out, int n)
{
    n = std::min (n, (int) scratch[0].size());
    float* aPtr[8];
    for (int c = 0; c < channels; ++c)
        aPtr[c] = scratch[(size_t) c].data();
    sig.process (in, aPtr, n);   // Signalsmith -> scratch
    psola.process (in, out, n);  // PSOLA       -> out
    const float* conf = psola.lastConfidence();

    const long qualify = (long) (qualifyTime * fs), exitN = (long) (exitTime * fs), dwell = (long) (minDwell * fs);
    const float step = (float) (1.0 / (slewTime * fs));
    for (int i = 0; i < n; ++i)
    {
        const double c = conf[i];
        ++dwellCount;
        if (! state)
        {
            qualifyCount = c >= enterThreshold ? qualifyCount + 1 : 0;
            if (qualifyCount >= qualify && dwellCount >= dwell) { state = true; dwellCount = 0; qualifyCount = 0; ++switches; }
        }
        else
        {
            exitCount = c < exitThreshold ? exitCount + 1 : 0;
            if (exitCount >= exitN && dwellCount >= dwell) { state = false; dwellCount = 0; exitCount = 0; ++switches; }
        }
        const float target = state ? 1.0f : 0.0f;
        weight += std::clamp (target - weight, -step, step);
        for (int ch = 0; ch < channels; ++ch)
            out[ch][i] = (1.0f - weight) * aPtr[ch][i] + weight * out[ch][i];
        psolaWeightSum += weight;
    }
    totalSamples += n;
}

} // namespace vs
