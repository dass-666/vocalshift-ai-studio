#pragma once

#include "dsp/ConsonantBlend.h"
#include "dsp/DeEsser.h"
#include "dsp/LinearSmoother.h"
#include "enhance/VocalEnhancer.h"
#include "mix/LevelMatcher.h"
#include "dsp/PitchShifter.h"
#include "mix/VocalMixEngine.h"
#include "morph/ToneShaper.h"
#include "state/MacroMapping.h"

#include <atomic>
#include <memory>
#include <vector>

namespace vs
{

// Full signal path, independent of JUCE:
//   in -> IdentityEngine (pitch + formants) -> ToneShaper (brightness)
//      -> VocalEnhancer ("Magia": compression + air) -> LevelMatcher (auto level) -> wet
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
    void setParams (const VoiceParams& p, bool bypassed) { setEngineSettings (mapMacros (p), bypassed); }
    void setEngineSettings (const EngineSettings& e, bool bypassed);

    // In-place processing; numSamples may exceed maxBlockSize (internally chunked).
    void process (float* const* io, int numChannels, int numSamples);

    int latencySamples() const { return latency; }
    const char* engineName() const { return shifter->engineName(); }

    // Peak levels of the last processed block (linear), for metering.
    float lastInputPeak() const { return inPeak; }
    float lastOutputPeak() const { return outPeak; }
    // Number of blocks where a non-finite sample was produced and muted.
    int nonFiniteEvents() const { return nonFiniteCount.load(); }
    // Current automatic level compensation gain (dB), for display.
    float autoLevelGainDb() const { return levelGainDb.load(); }
    // Output samples the last-resort soft clip has altered (should stay 0 at sane levels).
    long clipAlteredSamples() const { return mixer.clipAlteredSamples(); }
    // Whether the robust level matcher currently considers the voice active (diagnostics).
    float lastDeEssReductionDb() const { return deEsser.lastReductionDb(); }
    bool levelVoiceActive() const { return levelMatcher.voiceActive(); }

private:
    void processChunk (float* const* io, int numChannels, int offset, int n);

    std::unique_ptr<IPitchShifter> shifter;
    ConsonantBlend consonants;
    DeEsser deEsser;
    ToneShaper tone;
    VocalEnhancer enhancer;
    LevelMatcher levelMatcher;
    VocalMixEngine mixer;

    LinearSmoother semitoneSmoother, preservationSmoother, formantSmoother;

    std::vector<std::vector<float>> inScratch, wetScratch, dryScratch;
    std::vector<const float*> inPtrs;
    std::vector<float*> wetPtrs, dryPtrs, ioPtrs;

    int channels = 0, maxBlock = 0, latency = 0;
    float inPeak = 0.0f, outPeak = 0.0f;
    std::atomic<int> nonFiniteCount { 0 };
    std::atomic<float> levelGainDb { 0.0f };
    bool needsReset = false;
};

} // namespace vs
