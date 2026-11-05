#pragma once

#include "dsp/LinearSmoother.h"
#include "dsp/PitchShifter.h"
#include "mix/VocalMixEngine.h"
#include "morph/ToneShaper.h"
#include "state/FactoryPresets.h"

#include <atomic>
#include <memory>
#include <vector>

namespace vs
{

// Full V0 signal path, independent of JUCE:
//   in -> IdentityEngine (pitch + formants) -> ToneShaper (brightness) -> wet
//   in -> latency-matched dry delay -> dry
//   VocalMixEngine(dry, wet) -> out
// prepare() may allocate; everything else is real-time safe.
class VocalChain
{
public:
    explicit VocalChain (std::unique_ptr<IPitchShifter> engine = nullptr);

    void prepare (double sampleRate, int numChannels, int maxBlockSize);
    void reset();

    // Real-time safe: just updates smoothing targets.
    void setParams (const VoiceParams& p, bool bypassed);

    // In-place processing; numSamples may exceed maxBlockSize (internally chunked).
    void process (float* const* io, int numChannels, int numSamples);

    int latencySamples() const { return latency; }
    const char* engineName() const { return shifter->engineName(); }

    // Peak levels of the last processed block (linear), for metering.
    float lastInputPeak() const { return inPeak; }
    float lastOutputPeak() const { return outPeak; }
    // Number of blocks where a non-finite sample was produced and muted.
    int nonFiniteEvents() const { return nonFiniteCount.load(); }

private:
    void processChunk (float* const* io, int numChannels, int offset, int n);

    std::unique_ptr<IPitchShifter> shifter;
    ToneShaper tone;
    VocalMixEngine mixer;

    LinearSmoother semitoneSmoother, preservationSmoother, formantSmoother;

    std::vector<std::vector<float>> inScratch, wetScratch, dryScratch;
    std::vector<const float*> inPtrs;
    std::vector<float*> wetPtrs, dryPtrs, ioPtrs;

    int channels = 0, maxBlock = 0, latency = 0;
    float inPeak = 0.0f, outPeak = 0.0f;
    std::atomic<int> nonFiniteCount { 0 };
    bool needsReset = false;
};

} // namespace vs
