#pragma once

#include "PitchShifter.h"

#include <vector>

namespace vs
{

// EXPERIMENTAL pitch/formant engine (Fase 5): time-domain PSOLA.
//   - YIN F0 tracker on a decimated mono mix (5 ms hop), refined at full rate.
//   - Voiced regions: pitch-synchronous Hann grains (peak-anchored) re-spaced by 1/ratio.
//     Formants stay where they were (the grain keeps its spectral envelope); a formant
//     shift resamples each grain.
//   - Unvoiced regions: grains placed at the analysis spacing, so consonants pass through
//     unshifted (no pitch to move).
// Output timeline = input timeline delayed by `latencySamples()`, so the duration is preserved
// and the latency can be padded to equal the reference engine (setLatencyTarget).
// Not validated by listening: it is a lab/pro-selectable prototype, never the default.
class PsolaPitchShifter : public IPitchShifter
{
public:
    // Call before prepare(): the engine pads (never shortens below its own minimum) to this latency.
    void setLatencyTarget (int samples) { latencyTarget = samples; }
    int minimumLatency() const { return minLatency; }

    void prepare (double sampleRate, int numChannels, int maxBlockSize) override;
    void reset() override;
    void setSettings (const PitchSettings& s) override;
    void process (const float* const* in, float* const* out, int numSamples) override;
    int latencySamples() const override { return latency; }
    const char* engineName() const override { return "PSOLA experimental (Fase 5)"; }

    // Diagnostics: fraction of placed grains that were voiced since reset.
    float voicedFraction() const { return placed > 0 ? (float) voicedPlaced / (float) placed : 0.0f; }

private:
    float monoAt (long long i) const;
    void computeHop (long long h);
    bool detectLag (double& tauD, float& conf);
    void lookup (double ta, bool& voiced, double& t0) const;
    int peakNear (double ta, double t0) const;
    void placeMark();

    double fs = 48000.0;
    int channels = 1, latency = 0, latencyTarget = 0, minLatency = 0;
    int decim = 4, winFull = 2048, winD = 512, lagMinD = 26, lagMaxD = 185, hop = 240, lookahead = 1500;
    double t0Min = 100.0, t0Max = 738.0, t0Unv = 240.0;
    int hwMaxOut = 1500;

    std::vector<std::vector<float>> inRing, acc;
    std::vector<float> monoRing, wsum, dec, df, cmnd;
    int inMask = 0, outMask = 0;
    long long total = 0, outPos = 0, nextHop = 0;
    double tS = 0.0;

    static constexpr int trackCap = 1024;
    std::vector<float> trackT0;
    std::vector<unsigned char> trackVoiced;

    double ratio = 1.0, formantRatio = 1.0;
    long placed = 0, voicedPlaced = 0;
};

} // namespace vs
