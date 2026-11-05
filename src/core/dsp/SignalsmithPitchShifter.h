#pragma once

#include "PitchShifter.h"

#include <memory>

namespace vs
{

// Signalsmith Stretch (MIT) wrapped behind IPitchShifter.
// Streaming use: equal input/output sizes -> duration is preserved by construction.
class SignalsmithPitchShifter final : public IPitchShifter
{
public:
    // splitComputation spreads FFT work evenly across blocks (fewer CPU spikes in
    // a DAW) at the cost of one extra analysis interval (~30 ms) of latency.
    explicit SignalsmithPitchShifter (bool splitComputation = true);
    ~SignalsmithPitchShifter() override;

    void prepare (double sampleRate, int numChannels, int maxBlockSize) override;
    void reset() override;
    void setSettings (const PitchSettings& settings) override;
    void process (const float* const* in, float* const* out, int numSamples) override;
    int latencySamples() const override;
    const char* engineName() const override { return "Signalsmith Stretch 1.3.2"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    bool split;
};

} // namespace vs
