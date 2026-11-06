#include "ConsonantBlend.h"

#include <algorithm>
#include <cmath>

namespace vs
{

static float coeffFor (double fs, double seconds)
{
    return (float) std::exp (-1.0 / (fs * seconds));
}

void ConsonantBlend::prepare (double sampleRate, int numChannels)
{
    channels = std::min (numChannels, maxChannels);
    // RBJ 2nd-order Butterworth high-pass
    const double fc = std::min ((double) crossoverHz, 0.35 * sampleRate);
    const double w0 = 2.0 * 3.14159265358979323846 * fc / sampleRate;
    const double cs = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.70710678);
    const double a0 = 1.0 + alpha;
    Biquad bq;
    bq.b0 = (float) ((1.0 + cs) / 2.0 / a0);
    bq.b1 = (float) (-(1.0 + cs) / a0);
    bq.b2 = bq.b0;
    bq.a1 = (float) (-2.0 * cs / a0);
    bq.a2 = (float) ((1.0 - alpha) / a0);
    hpDry.fill (bq);
    hpWet.fill (bq);

    amountSmoother.prepare (sampleRate, 0.05);
    amountSmoother.snapTo (amountSmoother.getTarget());
    envAttack = coeffFor (sampleRate, 0.002);
    envRelease = coeffFor (sampleRate, 0.015);
    wAttack = coeffFor (sampleRate, 0.003);
    wRelease = coeffFor (sampleRate, 0.020);
    reset();
}

void ConsonantBlend::reset()
{
    for (auto& f : hpDry) { f.z1 = f.z2 = 0.0f; }
    for (auto& f : hpWet) { f.z1 = f.z2 = 0.0f; }
    fullEnv = hiEnv = w = lastW = 0.0f;
}

void ConsonantBlend::process (const float* const* dry, float* const* wet, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    const float k = 1.0f / (float) std::max (1, nCh);
    lastW = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = amountSmoother.next();
        float hd[maxChannels], hw[maxChannels];
        float full = 0.0f, hi = 0.0f;
        for (int c = 0; c < nCh; ++c)
        {
            const float d = dry[c][i];
            hd[c] = hpDry[(size_t) c].run (d);
            hw[c] = hpWet[(size_t) c].run (wet[c][i]);
            full += d * d;
            hi += hd[c] * hd[c];
        }
        full *= k;
        hi *= k;
        fullEnv = full + (full > fullEnv ? envAttack : envRelease) * (fullEnv - full);
        hiEnv = hi + (hi > hiEnv ? envAttack : envRelease) * (hiEnv - hi);

        float target = 0.0f;
        if (fullEnv > 1.0e-7f) // ignore anything below -70 dBFS
        {
            const float ratioDb = 10.0f * std::log10 (hiEnv / fullEnv + 1.0e-12f);
            const float t = std::clamp ((ratioDb + 12.0f) / 6.0f, 0.0f, 1.0f);
            target = t * t * (3.0f - 2.0f * t); // smoothstep between -12 and -6 dB
        }
        w = target + (target > w ? wAttack : wRelease) * (w - target);

        const float g = amount * w;
        if (g > 0.0f)
            for (int c = 0; c < nCh; ++c)
                wet[c][i] += g * (hd[c] - hw[c]);
        lastW = std::max (lastW, g);
    }
}

} // namespace vs
