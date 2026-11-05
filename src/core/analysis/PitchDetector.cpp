#include "PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace vs
{

YinPitchDetector::YinPitchDetector (double sampleRate, float minHz_, float maxHz_, float threshold_)
    : fs (sampleRate), minHz (minHz_), maxHz (maxHz_), threshold (threshold_)
{
}

PitchEstimate YinPitchDetector::analyse (const float* x, int length) const
{
    PitchEstimate result;
    const int maxLag = std::min ((int) (fs / minHz), length / 2);
    const int minLag = std::max (2, (int) (fs / maxHz));
    if (maxLag <= minLag + 2)
        return result;

    const int window = length - maxLag;
    std::vector<double> d ((size_t) maxLag + 1, 0.0);
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i < window; ++i)
        {
            const double diff = (double) x[i] - (double) x[i + tau];
            sum += diff * diff;
        }
        d[(size_t) tau] = sum;
    }

    // Cumulative mean normalised difference
    std::vector<double> dn ((size_t) maxLag + 1, 1.0);
    double running = 0.0;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        running += d[(size_t) tau];
        dn[(size_t) tau] = running > 0.0 ? d[(size_t) tau] * tau / running : 1.0;
    }

    int best = -1;
    for (int tau = minLag; tau < maxLag; ++tau)
    {
        if (dn[(size_t) tau] < threshold)
        {
            while (tau + 1 < maxLag && dn[(size_t) tau + 1] < dn[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        // No dip under threshold: take global minimum but flag as weakly periodic.
        best = minLag;
        for (int tau = minLag; tau < maxLag; ++tau)
            if (dn[(size_t) tau] < dn[(size_t) best])
                best = tau;
        if (dn[(size_t) best] > 0.35)
        {
            result.aperiodicity = (float) dn[(size_t) best];
            return result;
        }
    }

    // Parabolic interpolation
    double tauRefined = best;
    if (best > 1 && best < maxLag)
    {
        const double a = dn[(size_t) best - 1], b = dn[(size_t) best], c = dn[(size_t) best + 1];
        const double denom = a - 2 * b + c;
        if (std::abs (denom) > 1e-12)
            tauRefined = best + 0.5 * (a - c) / denom;
    }
    result.frequencyHz = (float) (fs / tauRefined);
    result.aperiodicity = (float) dn[(size_t) best];
    return result;
}

float YinPitchDetector::medianF0 (const float* signal, int length, int frameLength, int hop) const
{
    std::vector<float> f0s;
    for (int start = 0; start + frameLength <= length; start += hop)
    {
        if (rms (signal + start, frameLength) < 1.0e-3f)
            continue;
        const auto e = analyse (signal + start, frameLength);
        if (e.frequencyHz > 0.0f)
            f0s.push_back (e.frequencyHz);
    }
    if (f0s.empty())
        return 0.0f;
    std::nth_element (f0s.begin(), f0s.begin() + (long) f0s.size() / 2, f0s.end());
    return f0s[f0s.size() / 2];
}

float rms (const float* x, int n)
{
    if (n <= 0)
        return 0.0f;
    double s = 0.0;
    for (int i = 0; i < n; ++i)
        s += (double) x[i] * x[i];
    return (float) std::sqrt (s / n);
}

float peakAbs (const float* x, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; ++i)
        p = std::max (p, std::abs (x[i]));
    return p;
}

float spectralCentroidHz (const float* x, int n, double sampleRate)
{
    // Naive DFT on 1024 bins up to 8 kHz; adequate for offline tests.
    const int bins = 1024;
    const double maxHz = std::min (8000.0, sampleRate / 2.0);
    double num = 0.0, den = 0.0;
    const double pi = 3.14159265358979323846;
    for (int k = 1; k < bins; ++k)
    {
        const double f = maxHz * k / bins;
        const double w = 2.0 * pi * f / sampleRate;
        std::complex<double> acc { 0.0, 0.0 };
        const std::complex<double> rot { std::cos (w), -std::sin (w) };
        std::complex<double> ph { 1.0, 0.0 };
        for (int i = 0; i < n; ++i)
        {
            const double hann = 0.5 - 0.5 * std::cos (2.0 * pi * i / (n - 1));
            acc += ph * (double) x[i] * hann;
            ph *= rot;
        }
        const double mag = std::abs (acc);
        num += f * mag;
        den += mag;
    }
    return den > 0.0 ? (float) (num / den) : 0.0f;
}

} // namespace vs
