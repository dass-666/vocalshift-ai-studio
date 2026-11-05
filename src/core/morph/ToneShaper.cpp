#include "ToneShaper.h"

#include <algorithm>
#include <cmath>

namespace vs
{

void ToneShaper::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = std::min (numChannels, maxChannels);
    gainSmoother.prepare (sampleRate, 0.05);
    gainSmoother.snapTo (gainSmoother.getTarget());
    updateCoefficients (gainSmoother.getCurrent());
    reset();
}

void ToneShaper::reset()
{
    for (auto& s : state)
        s = {};
}

void ToneShaper::updateCoefficients (float gainDb)
{
    coeffGainDb = gainDb;
    if (std::abs (gainDb) < 1.0e-4f)
    {
        b0 = 1; b1 = b2 = a1 = a2 = 0;
        return;
    }
    // RBJ cookbook high shelf, S = 1.
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * shelfFrequencyHz / fs;
    const double cosw = std::cos (w0), sinw = std::sin (w0);
    const double alpha = sinw / 2.0 * std::sqrt (2.0);
    const double sqA = std::sqrt (A);

    const double nb0 = A * ((A + 1) + (A - 1) * cosw + 2 * sqA * alpha);
    const double nb1 = -2 * A * ((A - 1) + (A + 1) * cosw);
    const double nb2 = A * ((A + 1) + (A - 1) * cosw - 2 * sqA * alpha);
    const double na0 = (A + 1) - (A - 1) * cosw + 2 * sqA * alpha;
    const double na1 = 2 * ((A - 1) - (A + 1) * cosw);
    const double na2 = (A + 1) - (A - 1) * cosw - 2 * sqA * alpha;

    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
    a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}

void ToneShaper::process (float* const* io, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    constexpr int subBlock = 32;

    for (int start = 0; start < numSamples; start += subBlock)
    {
        const int n = std::min (subBlock, numSamples - start);
        const float g = gainSmoother.advance (n);
        if (g != coeffGainDb)
            updateCoefficients (g);

        if (b1 == 0.0f && b2 == 0.0f && a1 == 0.0f && a2 == 0.0f && b0 == 1.0f)
        {
            // Bypass, but keep filter memory consistent with a unity filter.
            for (int c = 0; c < nCh; ++c)
                state[(size_t) c] = {};
            continue;
        }

        for (int c = 0; c < nCh; ++c)
        {
            auto& s = state[(size_t) c];
            float* x = io[c] + start;
            for (int i = 0; i < n; ++i)
            {
                // Transposed direct form II
                const float in = x[i];
                const float out = b0 * in + s.z1;
                s.z1 = b1 * in - a1 * out + s.z2;
                s.z2 = b2 * in - a2 * out;
                x[i] = out;
            }
        }
    }
}

} // namespace vs
