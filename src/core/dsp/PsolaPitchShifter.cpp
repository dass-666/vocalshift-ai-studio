#include "PsolaPitchShifter.h"

#include <algorithm>
#include <cmath>

namespace vs
{
namespace
{
int nextPow2 (int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
constexpr double pi = 3.14159265358979323846;
}

void PsolaPitchShifter::prepare (double sampleRate, int numChannels, int maxBlockSize)
{
    fs = sampleRate;
    channels = std::clamp (numChannels, 1, 8);
    decim = std::max (1, (int) std::lround (fs / 12000.0));
    const double fsD = fs / decim;
    winD = std::max (64, (int) std::lround (0.0427 * fs) / decim);
    winFull = winD * decim;
    lagMinD = std::max (2, (int) std::floor (fsD / maxF0Hz));
    lagMaxD = (int) std::ceil (fsD / 65.0);
    if (lagMaxD > winD - 32) lagMaxD = winD - 32;
    hop = std::max (32, (int) std::lround (0.005 * fs));
    t0Min = fs / maxF0Hz;
    t0Max = fs / 65.0;
    t0Unv = std::round (0.005 * fs);
    hwMaxOut = (int) std::ceil (2.0 * t0Max) + 2;
    lookahead = std::max (winFull / 2, (int) (1.75 * t0Max + 2 * decim + 8));
    minLatency = (int) std::lround (0.135 * fs);
    latency = std::max (latencyTarget, minLatency);

    const int inCap = nextPow2 (latency * 3 + 8192);
    inMask = inCap - 1;
    const int outCap = nextPow2 (hwMaxOut * 4 + 4096);
    outMask = outCap - 1;
    inRing.assign ((size_t) channels, std::vector<float> ((size_t) inCap, 0.0f));
    monoRing.assign ((size_t) inCap, 0.0f);
    acc.assign ((size_t) channels, std::vector<float> ((size_t) outCap, 0.0f));
    wsum.assign ((size_t) outCap, 0.0f);
    dec.assign ((size_t) winD, 0.0f);
    df.assign ((size_t) lagMaxD + 2, 0.0f);
    cmnd.assign ((size_t) lagMaxD + 2, 0.0f);
    trackT0.assign (trackCap, 0.0f);
    trackConf.assign (trackCap, 0.0f);
    confAcc.assign ((size_t) outCap, 0.0f);
    confOut.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
    trackVoiced.assign (trackCap, 0);
    reset();
}

void PsolaPitchShifter::reset()
{
    for (auto& v : inRing) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : acc) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (monoRing.begin(), monoRing.end(), 0.0f);
    std::fill (wsum.begin(), wsum.end(), 0.0f);
    std::fill (confAcc.begin(), confAcc.end(), 0.0f);
    std::fill (confOut.begin(), confOut.end(), 0.0f);
    std::fill (trackConf.begin(), trackConf.end(), 0.0f);
    std::fill (trackVoiced.begin(), trackVoiced.end(), (unsigned char) 0);
    total = outPos = nextHop = 0;
    tS = 0.0;
    placed = voicedPlaced = 0;
}

void PsolaPitchShifter::setSettings (const PitchSettings& s)
{
    const double st = std::clamp ((double) s.semitones, -24.0, 24.0);
    ratio = std::clamp (std::pow (2.0, st / 12.0), 0.5, 4.0);
    const double fsemi = (1.0 - std::clamp ((double) s.formantPreservation, 0.0, 1.0)) * st
                         + std::clamp ((double) s.formantShiftSemitones, -12.0, 12.0);
    formantRatio = std::clamp (std::pow (2.0, fsemi / 12.0), 0.5, 2.0);
}

float PsolaPitchShifter::monoAt (long long i) const
{
    if (i < 0 || i >= total) return 0.0f;
    return monoRing[(size_t) (i & inMask)];
}

bool PsolaPitchShifter::detectLag (double& tauOut, float& conf)
{
    const int N = winD, W = N - lagMaxD;
    for (int tau = 1; tau <= lagMaxD; ++tau)
    {
        float s = 0.0f;
        const float* a = dec.data();
        const float* b = dec.data() + tau;
        for (int j = 0; j < W; ++j)
        {
            const float d = a[j] - b[j];
            s += d * d;
        }
        df[(size_t) tau] = s;
    }
    cmnd[0] = 1.0f;
    float run = 0.0f;
    for (int tau = 1; tau <= lagMaxD; ++tau)
    {
        run += df[(size_t) tau];
        cmnd[(size_t) tau] = run > 1e-12f ? df[(size_t) tau] * (float) tau / run : 1.0f;
    }
    int est = -1;
    for (int tau = lagMinD; tau < lagMaxD; ++tau)
        if (cmnd[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < lagMaxD && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
            est = tau;
            break;
        }
    if (est < 0)
    {
        float best = 1e9f;
        for (int tau = lagMinD; tau < lagMaxD; ++tau)
            if (cmnd[(size_t) tau] < best) { best = cmnd[(size_t) tau]; est = tau; }
        if (best > 0.30f) return false;
    }
    conf = 1.0f - cmnd[(size_t) est];
    double t = est;
    if (est > 1 && est < lagMaxD - 1)
    {
        const double a = cmnd[(size_t) est - 1], b = cmnd[(size_t) est], c = cmnd[(size_t) est + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-9) t += std::clamp (0.5 * (a - c) / den, -1.0, 1.0);
    }
    tauOut = t;
    return true;
}

void PsolaPitchShifter::computeHop (long long h)
{
    const long long c = h * hop;
    const long long start = c - winFull / 2;
    double e = 0.0;
    for (int i = 0; i < winD; ++i)
    {
        float s = 0.0f;
        for (int j = 0; j < decim; ++j) s += monoAt (start + (long long) i * decim + j);
        s /= (float) decim;
        dec[(size_t) i] = s;
        e += (double) s * s;
    }
    const size_t slot = (size_t) (h % trackCap);
    trackVoiced[slot] = 0;
    trackT0[slot] = (float) t0Unv;
    trackConf[slot] = 0.0f;
    if (std::sqrt (e / winD) < 0.002) return;

    double tauD = 0.0;
    float conf = 0.0f;
    if (! detectLag (tauD, conf)) return;

    // Refine at full rate around the decimated estimate (normalised cross-correlation).
    const double lag0 = tauD * decim;
    const int len = std::max (64, (int) lag0);
    const long long a0 = c - len / 2;
    const int lo = std::max ((int) t0Min - 1, (int) std::lround (lag0) - decim - 1);
    const int hi = std::min ((int) t0Max + 1, (int) std::lround (lag0) + decim + 1);
    double best = -2.0, prev = 0.0, next = 0.0;
    int bestL = lo;
    double scores[64];
    const int cnt = std::min (hi - lo + 1, 62);
    for (int k = 0; k < cnt; ++k)
    {
        const int L = lo + k;
        double ab = 0.0, aa = 0.0, bb = 0.0;
        for (int j = 0; j < len; ++j)
        {
            const double x = monoAt (a0 + j), y = monoAt (a0 + j + L);
            ab += x * y; aa += x * x; bb += y * y;
        }
        scores[k] = ab / std::sqrt (aa * bb + 1e-20);
        if (scores[k] > best) { best = scores[k]; bestL = L; }
    }
    if (cnt < 1 || best < 0.5) return;
    double T = bestL;
    const int kb = bestL - lo;
    if (kb > 0 && kb < cnt - 1)
    {
        prev = scores[kb - 1]; next = scores[kb + 1];
        const double den = prev - 2.0 * best + next;
        if (std::abs (den) > 1e-12) T += std::clamp (0.5 * (prev - next) / den, -1.0, 1.0);
    }
    trackT0[slot] = (float) std::clamp (T, t0Min, t0Max);
    trackVoiced[slot] = 1;
    trackConf[slot] = (float) std::clamp ((best - 0.7) / 0.25, 0.0, 1.0);
}

void PsolaPitchShifter::lookup (double ta, bool& voiced, double& t0, float* conf) const
{
    voiced = false;
    t0 = t0Unv;
    if (conf) *conf = 0.0f;
    const long long h = (long long) std::llround (ta / hop);
    if (h < 0 || h >= nextHop) return;
    const size_t sc = (size_t) (h % trackCap);
    if (! trackVoiced[sc]) return;
    float v[3];
    int n = 0;
    for (long long k = h - 1; k <= h + 1; ++k)
        if (k >= 0 && k < nextHop && trackVoiced[(size_t) (k % trackCap)]) v[n++] = trackT0[(size_t) (k % trackCap)];
    voiced = true;
    if (n == 3)
    {
        std::sort (v, v + 3);
        t0 = v[1];
    }
    else
        t0 = trackT0[sc];
    if (conf)
    {
        // Stability: period change between neighbouring 5 ms hops (vibrato is ~1 %; octave jumps are 50-100 %).
        double spread = 0.0;
        for (int k = 0; k < n; ++k) spread = std::max (spread, std::abs ((double) v[k] - t0) / t0);
        const double stab = n == 3 ? std::clamp (1.0 - spread / 0.08, 0.0, 1.0) : 0.3;
        *conf = (float) (trackConf[sc] * stab);
    }
}

int PsolaPitchShifter::peakNear (double ta, double t0) const
{
    const long long lo = (long long) std::llround (ta - t0 * 0.5), hi = (long long) std::llround (ta + t0 * 0.5);
    long long bestI = (long long) std::llround (ta);
    float best = -1e30f;
    for (long long i = lo; i <= hi; ++i)
    {
        const float v = monoAt (i);
        if (v > best) { best = v; bestI = i; }
    }
    return (int) bestI;
}

void PsolaPitchShifter::placeMark()
{
    const double ta = tS - latency;
    bool voiced = false;
    double t0 = t0Unv;
    float conf = 0.0f;
    lookup (ta, voiced, t0, &conf);

    double ts, hw;
    long long m;
    if (voiced)
    {
        ts = t0 / ratio;
        m = peakNear (ta, t0);
        hw = t0; // one period each side; wider grains keep the old periodicity and block pitch-down
    }
    else
    {
        ts = t0;
        m = (long long) std::llround (ta);
        hw = t0;
    }
    ++placed;
    if (voiced) ++voicedPlaced;

    const int H = (int) std::ceil (hw);
    const long long centre = (long long) std::llround (tS);
    const double fr = formantRatio;
    for (int k = -H; k <= H; ++k)
    {
        const long long oi = centre + k;
        if (oi < outPos) continue;
        const double w = 0.5 * (1.0 + std::cos (pi * k / (H + 1.0)));
        const double p = (double) m + (double) k * fr;
        const long long p0 = (long long) std::floor (p);
        const float fracv = (float) (p - (double) p0);
        const size_t slot = (size_t) (oi & outMask);
        for (int c = 0; c < channels; ++c)
        {
            const auto& ring = inRing[(size_t) c];
            auto at = [&] (long long i) { return (i < 0 || i >= total) ? 0.0f : ring[(size_t) (i & inMask)]; };
            const float s = at (p0) * (1.0f - fracv) + at (p0 + 1) * fracv;
            acc[(size_t) c][slot] += (float) w * s;
        }
        wsum[slot] += (float) w;
        confAcc[slot] += (float) w * conf;
    }
    tS += std::max (16.0, ts);
}

void PsolaPitchShifter::process (const float* const* in, float* const* out, int n)
{
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int c = 0; c < channels; ++c)
        {
            inRing[(size_t) c][(size_t) ((total + i) & inMask)] = in[c][i];
            mono += in[c][i];
        }
        monoRing[(size_t) ((total + i) & inMask)] = mono / (float) channels;
    }
    total += n;

    while (nextHop * hop + lookahead <= total)
        computeHop (nextHop++);

    while (tS - hwMaxOut < (double) total)
        placeMark();

    // Pitch down leaves gaps between grains; a floor on the window sum avoids click-prone division by ~0.
    const float wFloor = ratio < 1.0 ? 0.5f : 1e-4f;
    for (int i = 0; i < n; ++i)
    {
        const size_t slot = (size_t) ((outPos + i) & outMask);
        const float w = wsum[slot];
        for (int c = 0; c < channels; ++c)
        {
            out[c][i] = w > 1e-4f ? acc[(size_t) c][slot] / std::max (w, wFloor) : 0.0f;
            acc[(size_t) c][slot] = 0.0f;
        }
        if ((size_t) i < confOut.size())
            confOut[(size_t) i] = w > 1e-4f ? confAcc[slot] / w : 0.0f;
        confAcc[slot] = 0.0f;
        wsum[slot] = 0.0f;
    }
    outPos += n;
}

} // namespace vs
