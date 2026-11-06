#include "VoiceTexture.h"

#include <algorithm>
#include <cmath>

namespace vs
{

void VoiceTexture::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = std::min (numChannels, maxChannels);
    lpA = (float) (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 1800.0 / fs));
    amountSmoother.prepare (sampleRate, 0.05);
    amountSmoother.snapTo (amountSmoother.getTarget());
    reset();
}

void VoiceTexture::reset()
{
    for (auto& b : body) b.z1 = b.z2 = 0.0f;
    for (auto& b : presence) b.z1 = b.z2 = 0.0f;
    lp.fill (0.0f);
}

void VoiceTexture::design (float c)
{
    const double pi = 3.14159265358979323846;
    {   // RBJ low shelf, 220 Hz, S = 1
        const double A = std::pow (10.0, 3.0 * c / 40.0);
        const double w0 = 2 * pi * 220.0 / fs, cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1) + (A - 1) * cs + sq;
        Biquad b;
        b.b0 = (float) (A * ((A + 1) - (A - 1) * cs + sq) / a0);
        b.b1 = (float) (2 * A * ((A - 1) - (A + 1) * cs) / a0);
        b.b2 = (float) (A * ((A + 1) - (A - 1) * cs - sq) / a0);
        b.a1 = (float) (-2 * ((A - 1) + (A + 1) * cs) / a0);
        b.a2 = (float) (((A + 1) + (A - 1) * cs - sq) / a0);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& t = body[(size_t) ch];
            t.b0 = b.b0; t.b1 = b.b1; t.b2 = b.b2; t.a1 = b.a1; t.a2 = b.a2;
        }
    }
    {   // RBJ peaking, 3 kHz, Q 0.8
        const double A = std::pow (10.0, 2.2 * c / 40.0);
        const double w0 = 2 * pi * 3000.0 / fs, cs = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.8);
        const double a0 = 1 + alpha / A;
        Biquad b;
        b.b0 = (float) ((1 + alpha * A) / a0);
        b.b1 = (float) (-2 * cs / a0);
        b.b2 = (float) ((1 - alpha * A) / a0);
        b.a1 = b.b1;
        b.a2 = (float) ((1 - alpha / A) / a0);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& t = presence[(size_t) ch];
            t.b0 = b.b0; t.b1 = b.b1; t.b2 = b.b2; t.a1 = b.a1; t.a2 = b.a2;
        }
    }
    mix = 0.7f * c;
    k = 2.0f + 4.0f * c;
    satNorm = 0.1f / std::tanh (k * 0.1f);
}

void VoiceTexture::process (float* const* io, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    constexpr int sub = 32;
    for (int s0 = 0; s0 < numSamples; s0 += sub)
    {
        const int n = std::min (sub, numSamples - s0);
        const float c = amountSmoother.advance (n);
        if (c <= 0.0f && ! amountSmoother.isSmoothing())
            continue; // exact bypass (the filters hold their last state; they were flat at c = 0)
        design (c);
        for (int ch = 0; ch < nCh; ++ch)
        {
            float* x = io[ch] + s0;
            for (int i = 0; i < n; ++i)
            {
                float v = presence[(size_t) ch].run (body[(size_t) ch].run (x[i]));
                lp[(size_t) ch] += lpA * (v - lp[(size_t) ch]);
                const float low = lp[(size_t) ch];
                const float sat = std::tanh (k * low) * satNorm; // unity gain at amplitude 0.1
                x[i] = v + mix * (sat - low);
            }
        }
    }
}

} // namespace vs
