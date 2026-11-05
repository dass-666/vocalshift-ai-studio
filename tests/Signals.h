#pragma once

// Deterministic synthetic test signals (shared by tests and QualityBench).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vstest
{
constexpr double kPi = 3.14159265358979323846;

// Band-limited harmonic tone (additive), all harmonics below 0.45 * fs.
inline std::vector<float> harmonicTone (double fs, double f0, double seconds, float amp = 0.3f,
                                        double vibratoHz = 0.0, double vibratoCents = 0.0)
{
    const int n = (int) (fs * seconds);
    std::vector<float> x ((size_t) n, 0.0f);
    const int harmonics = (int) (0.45 * fs / f0);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double f = f0 * std::pow (2.0, vibratoCents / 1200.0 * std::sin (2 * kPi * vibratoHz * t));
        phase += 2 * kPi * f / fs;
        double s = 0.0;
        for (int h = 1; h <= harmonics; ++h)
            if (h * f < 0.45 * fs)
                s += std::sin (h * phase) / h;
        x[(size_t) i] = (float) (amp * s / 1.6);
    }
    return x;
}

// Crude vowel: glottal-like pulse train shaped by fixed formant resonators.
// Formants (Hz) default to an /a/-like vowel. Spectral envelope is independent of f0.
inline std::vector<float> vowel (double fs, double f0, double seconds, float amp = 0.3f,
                                 double vibratoHz = 5.0, double vibratoCents = 30.0,
                                 double f1 = 730, double f2 = 1090, double f3 = 2440)
{
    const int n = (int) (fs * seconds);
    std::vector<float> src ((size_t) n, 0.0f);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double f = f0 * std::pow (2.0, vibratoCents / 1200.0 * std::sin (2 * kPi * vibratoHz * t));
        phase += f / fs;
        double s = 0.0;
        const int harmonics = (int) (0.45 * fs / f);
        for (int h = 1; h <= harmonics; ++h)
            s += std::sin (2 * kPi * h * phase) / std::pow (h, 0.7);
        src[(size_t) i] = (float) s;
    }
    std::vector<float> y ((size_t) n, 0.0f);
    for (double fc : { f1, f2, f3 })
    {
        const double bw = 90.0;
        const double r = std::exp (-kPi * bw / fs);
        const double a1 = -2 * r * std::cos (2 * kPi * fc / fs), a2 = r * r;
        double y1 = 0, y2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double v = (1 - r) * src[(size_t) i] - a1 * y1 - a2 * y2;
            y2 = y1; y1 = v;
            y[(size_t) i] += (float) v;
        }
    }
    float peak = 0;
    for (float v : y) peak = std::max (peak, std::abs (v));
    for (float& v : y) v = v / (peak > 0 ? peak : 1) * amp;
    return y;
}

inline std::vector<float> whiteNoise (int n, float amp, uint32_t seed = 1)
{
    std::vector<float> x ((size_t) n);
    uint32_t s = seed;
    for (auto& v : x)
    {
        s = s * 1664525u + 1013904223u;
        v = amp * ((float) (s >> 8) / 8388608.0f - 1.0f);
    }
    return x;
}

inline double centsBetween (double fa, double fb) { return 1200.0 * std::log2 (fb / fa); }

} // namespace vstest
