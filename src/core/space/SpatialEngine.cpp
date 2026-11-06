#include "SpatialEngine.h"

#include <algorithm>
#include <cmath>

namespace vs
{

static float onePoleCoeff (double fs, double fc)
{
    return (float) (1.0 - std::exp (-2.0 * 3.14159265358979323846 * fc / fs));
}

void SpatialEngine::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = numChannels;
    const double ms = fs / 1000.0;

    sideSize = (int) std::ceil (30.0 * ms) + 8;
    sideBuf.assign ((size_t) sideSize, 0.0f);
    sideBase1 = (float) (11.0 * ms);
    sideBase2 = (float) (17.0 * ms);
    lfoInc1 = (float) (0.27 / fs);
    lfoInc2 = (float) (0.19 / fs);
    sideHp.a = onePoleCoeff (fs, 120.0);
    sideLp.a = onePoleCoeff (fs, 6500.0);

    preDelay = std::max (1, (int) std::lround (predelayMs * ms));
    preSize = preDelay + 4;
    preBuf.assign ((size_t) preSize, 0.0f);
    sendHp.a = onePoleCoeff (fs, 250.0);
    sendLp.a = onePoleCoeff (fs, 5000.0);
    static constexpr int baseLen[4] = { 1423, 1783, 1973, 2099 }; // mutually prime, at 48 kHz
    for (int i = 0; i < 4; ++i)
    {
        lines[i].len = std::max (16, (int) std::lround (baseLen[i] * fs / 48000.0));
        lines[i].buf.assign ((size_t) lines[i].len, 0.0f);
    }
    lineDamp = onePoleCoeff (fs, 4200.0);

    slapSize = (int) std::ceil (0.215 * fs) + 8;
    slapBufL.assign ((size_t) slapSize, 0.0f);
    slapBufR.assign ((size_t) slapSize, 0.0f);
    for (int c = 0; c < 2; ++c)
    {
        slapHp[c].a = onePoleCoeff (fs, 300.0);
        slapLp[c].a = onePoleCoeff (fs, 3200.0);
    }

    // Targets set before prepare() are kept (the plugin sets them first, like the other blocks).
    for (auto* s : { &amountS, &widthS, &reverbS, &slapS, &modS })
    {
        s->prepare (sampleRate, 0.05);
        s->snapTo (s->getTarget());
    }
    rt60Current = rt60Target;
    updateRt60 (rt60Current);
    reset();
}

long SpatialEngine::memoryBytes() const
{
    long n = (long) (sideBuf.size() + preBuf.size() + slapBufL.size() + slapBufR.size());
    for (auto& l : lines) n += (long) l.buf.size();
    return n * (long) sizeof (float);
}

void SpatialEngine::clearState()
{
    std::fill (sideBuf.begin(), sideBuf.end(), 0.0f);
    std::fill (preBuf.begin(), preBuf.end(), 0.0f);
    std::fill (slapBufL.begin(), slapBufL.end(), 0.0f);
    std::fill (slapBufR.begin(), slapBufR.end(), 0.0f);
    for (auto& l : lines) { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.pos = 0; l.lp = 0.0f; }
    sidePos = prePos = slapPos = 0;
    sideHp.z = sideLp.z = sendHp.z = sendLp.z = 0.0f;
    for (int c = 0; c < 2; ++c) { slapHp[c].z = 0.0f; slapLp[c].z = 0.0f; }
    slapFbL = slapFbR = 0.0f;
    slapTimeL = 0.0f;
}

void SpatialEngine::reset()
{
    clearState();
    lfoPhase1 = 0.0f;
    lfoPhase2 = 0.5f;
    active = wasActive = false;
}

void SpatialEngine::setParams (const SpatialParams& p)
{
    target = p;
    target.amount = std::clamp (p.amount, 0.0f, 1.0f);
    target.width = std::clamp (p.width, 0.0f, 0.95f);
    target.reverbGain = std::clamp (p.reverbGain, 0.0f, 0.6f);
    target.slapGain = std::clamp (p.slapGain, 0.0f, 0.3f);
    target.modDepthMs = std::clamp (p.modDepthMs, 0.0f, 1.0f);
    rt60Target = std::clamp (p.rt60, 0.6f, 2.0f);
    amountS.setTarget (target.amount);
    widthS.setTarget (target.width);
    reverbS.setTarget (target.reverbGain);
    slapS.setTarget (target.slapGain);
    modS.setTarget (target.modDepthMs);
}

void SpatialEngine::updateRt60 (float rt60)
{
    for (auto& l : lines)
        l.gain = std::pow (10.0f, -3.0f * (float) l.len / ((float) fs * rt60));
}

float SpatialEngine::readFrac (const std::vector<float>& b, int size, int writePos, float delay)
{
    // writePos = index of the most recently written sample; delay in samples (>= 1)
    float r = (float) writePos - delay;
    while (r < 0.0f) r += (float) size;
    const int i0 = (int) r;
    const float f = r - (float) i0;
    const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
    return b[(size_t) i0] * (1.0f - f) + b[(size_t) i1] * f;
}

void SpatialEngine::process (float* const* io, int numChannels, int numSamples)
{
    if (numChannels < 2 || channels < 2)
        return;
    const bool wantActive = target.amount > 0.0f && (target.width > 0.0f || target.reverbGain > 0.0f || target.slapGain > 0.0f);
    const bool smoothing = amountS.isSmoothing() || widthS.isSmoothing() || reverbS.isSmoothing() || slapS.isSmoothing();
    if (! wantActive && ! smoothing)
    {
        if (wasActive) { clearState(); wasActive = false; }
        active = false;
        return; // exact bypass: io untouched
    }
    active = true;
    wasActive = true;

    // slap time from the host tempo (1/16 note), folded into 70..180 ms
    double slapMs = 115.0;
    if (tempoBpm >= 30.0 && tempoBpm <= 300.0)
    {
        slapMs = 60000.0 / tempoBpm / 4.0;
        while (slapMs < 70.0) slapMs *= 2.0;
        while (slapMs > 180.0) slapMs *= 0.5;
    }
    slapGoalL = (float) (slapMs * fs / 1000.0);
    if (slapTimeL <= 0.0f) slapTimeL = slapGoalL; // first block after prepare/reset: no glide

    constexpr int sub = 32;
    for (int s0 = 0; s0 < numSamples; s0 += sub)
    {
        const int n = std::min (sub, numSamples - s0);
        rt60Current += (rt60Target - rt60Current) * 0.05f;
        updateRt60 (rt60Current);

        for (int i = s0; i < s0 + n; ++i)
        {
            const float amt = amountS.next();
            const float width = widthS.next() * amt;
            const float revG = reverbS.next() * amt;
            const float slapG = slapS.next() * amt;
            const float depth = modS.next() * (float) (fs / 1000.0);
            // max 0.4 % delay change per sample (~7 cents of pitch bend) while the tempo moves
            slapTimeL += std::clamp (slapGoalL - slapTimeL, -0.004f, 0.004f);
            slapTimeR = slapTimeL * 1.13f;

            const float L = io[0][i], R = io[1][i];
            const float m = 0.5f * (L + R);

            // ---- width (mono-sum preserving)
            sideBuf[(size_t) sidePos] = m;
            lfoPhase1 += lfoInc1; if (lfoPhase1 >= 1.0f) lfoPhase1 -= 1.0f;
            lfoPhase2 += lfoInc2; if (lfoPhase2 >= 1.0f) lfoPhase2 -= 1.0f;
            const float d1 = sideBase1 + depth * std::sin (6.2831853f * lfoPhase1);
            const float d2 = sideBase2 - depth * std::sin (6.2831853f * lfoPhase2);
            float sd = 0.5f * (readFrac (sideBuf, sideSize, sidePos, d1) - readFrac (sideBuf, sideSize, sidePos, d2));
            sd = lowpass (sideLp, highpass (sideHp, sd));
            if (++sidePos == sideSize) sidePos = 0;
            const float norm = 1.0f / std::sqrt (1.0f + 0.5f * width * width);
            float outL = (L + width * sd) * norm;
            float outR = (R - width * sd) * norm;

            // ---- reverb (FDN)
            const float send = lowpass (sendLp, highpass (sendHp, m));
            preBuf[(size_t) prePos] = send;
            int rp = prePos - preDelay; if (rp < 0) rp += preSize;
            const float rin = preBuf[(size_t) rp];
            if (++prePos == preSize) prePos = 0;

            float y[4];
            for (int k = 0; k < 4; ++k)
            {
                auto& l = lines[k];
                const float v = l.buf[(size_t) l.pos];
                l.lp += lineDamp * (v - l.lp); // damping
                y[k] = l.lp * l.gain;
            }
            const float h = 0.5f; // orthonormal 4x4 Hadamard
            const float f0 = h * (y[0] + y[1] + y[2] + y[3]);
            const float f1 = h * (y[0] - y[1] + y[2] - y[3]);
            const float f2 = h * (y[0] + y[1] - y[2] - y[3]);
            const float f3 = h * (y[0] - y[1] - y[2] + y[3]);
            const float fb[4] = { f0, f1, f2, f3 };
            for (int k = 0; k < 4; ++k)
            {
                auto& l = lines[k];
                l.buf[(size_t) l.pos] = fb[k] + 0.35f * rin;
                if (++l.pos == l.len) l.pos = 0;
            }
            const float revL = 0.5f * (lines[0].lp + lines[1].lp);
            const float revR = 0.5f * (lines[2].lp + lines[3].lp);
            outL += revG * revL * 1.4f;
            outR += revG * revR * 1.4f;

            // ---- slap
            const float sIn = m;
            slapBufL[(size_t) slapPos] = sIn + slapFbL;
            slapBufR[(size_t) slapPos] = sIn + slapFbR;
            float eL = readFrac (slapBufL, slapSize, slapPos, slapTimeL);
            float eR = readFrac (slapBufR, slapSize, slapPos, slapTimeR);
            eL = lowpass (slapLp[0], highpass (slapHp[0], eL));
            eR = lowpass (slapLp[1], highpass (slapHp[1], eR));
            slapFbL = 0.18f * eL;
            slapFbR = 0.18f * eR;
            if (++slapPos == slapSize) slapPos = 0;
            outL += slapG * eL;
            outR += slapG * eR;

            io[0][i] = outL;
            io[1][i] = outR;
        }
    }
}

} // namespace vs
