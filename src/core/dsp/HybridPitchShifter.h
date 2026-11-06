#pragma once

#include "PitchShifter.h"
#include "PsolaPitchShifter.h"
#include "SignalsmithPitchShifter.h"

#include <vector>

namespace vs
{

// EXPERIMENTAL, ISOLATED hybrid (Fase 5.1): runs the Signalsmith engine (Phase 4 reference) and the PSOLA engine
// in parallel, with identical latency, and blends them per sample:
//   - PSOLA only where its own analysis is reliable (periodic + stable F0) for a sustained time,
//   - Signalsmith everywhere else (transients, consonants, unvoiced, uncertain pitch).
// Switching protections: qualification time before entering PSOLA, hysteresis, minimum dwell time,
// and a linear gain slew (never a hard switch), so weights are continuous in time.
// NOT connected to the plugin: used by vs_lab and the tests only. Costs both engines' CPU.
class HybridPitchShifter : public IPitchShifter
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlockSize) override;
    void reset() override;
    void setSettings (const PitchSettings& s) override;
    void process (const float* const* in, float* const* out, int numSamples) override;
    int latencySamples() const override { return latency; }
    const char* engineName() const override { return "Hibrido experimental (Fase 5.1)"; }

    // Diagnostics
    float psolaShare() const { return totalSamples > 0 ? (float) psolaWeightSum / (float) totalSamples : 0.0f; }
    long switchCount() const { return switches; }
    float lastWeight() const { return weight; }

    // Tunables (seconds); public for the lab, defaults chosen by engineering judgement.
    double enterThreshold = 0.8, exitThreshold = 0.5;
    double qualifyTime = 0.040, exitTime = 0.010, minDwell = 0.080, slewTime = 0.020;

private:
    SignalsmithPitchShifter sig;
    PsolaPitchShifter psola;
    std::vector<std::vector<float>> scratch;
    int channels = 1, latency = 0;
    double fs = 48000.0;
    float weight = 0.0f;       // 0 = Signalsmith, 1 = PSOLA (this is the blend actually applied)
    bool state = false;        // decided engine
    long qualifyCount = 0, exitCount = 0, dwellCount = 0, switches = 0, totalSamples = 0;
    double psolaWeightSum = 0.0;
};

} // namespace vs
