#include "DeEsser.h"

#include <algorithm>
#include <cmath>

namespace vs
{

static float coeffFor (double fs, double seconds) { return (float) std::exp (-1.0 / (fs * seconds)); }

void DeEsser::prepare (double sampleRate, int numChannels)
{
    channels = std::min (numChannels, maxChannels);
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
    hp.fill (bq);
    amountSmoother.prepare (sampleRate, 0.05);
    amountSmoother.snapTo (amountSmoother.getTarget());
    attack = coeffFor (sampleRate, 0.001);
    release = coeffFor (sampleRate, 0.040);
    gainCoeff = coeffFor (sampleRate, 0.002);
    reset();
}

void DeEsser::reset()
{
    for (auto& f : hp) f.z1 = f.z2 = 0.0f;
    env = 0.0f;
    gain = 1.0f;
    lastGr = 0.0f;
}

void DeEsser::process (float* const* io, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    lastGr = 0.0f;
    constexpr float kneeDb = 6.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = amountSmoother.next();
        float hi[maxChannels];
        float e = 0.0f;
        for (int c = 0; c < nCh; ++c)
        {
            hi[c] = hp[(size_t) c].run (io[c][i]); // filters always run: no state jump when enabling
            e = std::max (e, hi[c] * hi[c]);
        }
        env = e + (e > env ? attack : release) * (env - e);
        float grDb = 0.0f;
        if (amount > 0.0f)
        {
            const float over = 10.0f * std::log10 (env + 1.0e-12f) - thresholdDb;
            const float slope = 1.0f - 1.0f / ratio;
            if (over > kneeDb * 0.5f)
                grDb = over * slope;
            else if (over > -kneeDb * 0.5f)
            {
                const float x = over + kneeDb * 0.5f;
                grDb = slope * x * x / (2.0f * kneeDb);
            }
            grDb = std::min (grDb, maxReductionDb) * amount;
        }
        lastGr = std::max (lastGr, grDb);
        const float target = std::pow (10.0f, -grDb / 20.0f);
        gain = target + gainCoeff * (gain - target);
        // low + gain * high = x - (1 - gain) * high
        const float k = 1.0f - gain;
        if (k != 0.0f)
            for (int c = 0; c < nCh; ++c)
                io[c][i] -= k * hi[c];
    }
}

} // namespace vs
