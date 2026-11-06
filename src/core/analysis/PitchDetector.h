#pragma once

#include <vector>

namespace vs
{

// VocalAnalysis (V0): offline / worker-thread analysis helpers.
// Not used on the audio thread yet. Used by tests and QualityBench to verify
// that the F0 actually moved by the requested interval.
struct PitchEstimate
{
    float frequencyHz = 0.0f; // 0 if unvoiced
    float aperiodicity = 1.0f; // YIN d'(tau) at the chosen lag: ~0 periodic, ~1 noise
};

class YinPitchDetector
{
public:
    YinPitchDetector (double sampleRate, float minHz = 50.0f, float maxHz = 1500.0f, float threshold = 0.12f);

    // Analyse one frame (length >= 2 * sampleRate / minHz recommended).
    PitchEstimate analyse (const float* frame, int length) const;

    // Median F0 over voiced frames of a whole signal, 0 if none.
    float medianF0 (const float* signal, int length, int frameLength = 4096, int hop = 1024) const;

private:
    double fs;
    float minHz, maxHz, threshold;
};

// Simple frame-energy helpers.
float rms (const float* x, int n);
float peakAbs (const float* x, int n);

// Spectral centroid of a frame (Hann window, naive DFT on a decimated grid). Offline only.
float spectralCentroidHz (const float* x, int n, double sampleRate);

} // namespace vs
