#pragma once

#include "dsp/ConsonantBlend.h"
#include "dsp/DeEsser.h"
#include "dsp/LinearSmoother.h"
#include "enhance/VocalEnhancer.h"
#include "mix/LevelMatcher.h"
#include "dsp/PitchShifter.h"
#include "dsp/PsolaPitchShifter.h"
#include "mix/VocalMixEngine.h"
#include "morph/ToneShaper.h"
#include "morph/VoiceTexture.h"
#include "space/SpatialEngine.h"
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
    const char* engineName() const { return (activeEngine == 1 && alt ? static_cast<const IPitchShifter&> (*alt) : *shifter).engineName(); }
    // Pitch engine actually in use (0 = Phase 4 reference, 1 = experimental PSOLA); lags the request by ~0.3 s while it switches.
    int activeEngineIndex() const { return activeEngine; }
    bool engineSwitching() const { return switchPhase != 0; }

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
    // Host tempo (BPM, 0 = unknown) for the slap delay; call from the audio thread before process().
    void setTempoBpm (double bpm) { space.setTempoBpm (bpm); }
    // Peak of the second voice (voice + space, after the fader) in the last block; 0 outside second-voice mode.
    float lastSecondVoicePeak() const { return secondPeak; }
    long spatialMemoryBytes() const { return space.memoryBytes(); }
    bool spatialActive() const { return space.isActive(); }
    float lastDeEssReductionDb() const { return deEsser.lastReductionDb(); }
    bool levelVoiceActive() const { return levelMatcher.voiceActive(); }

private:
    void processChunk (float* const* io, int numChannels, int offset, int n);

    std::unique_ptr<IPitchShifter> shifter;      // Phase 4 reference engine (engine 0)
    std::unique_ptr<PsolaPitchShifter> alt;      // experimental engine (engine 1), latency padded to the reference
    int activeEngine = 0, targetEngine = 0, requestedEngine = 0;
    int switchPhase = 0;                         // 0 idle, 1 priming the new engine, 2 crossfading
    int switchCount = 0, xfadeLength = 1, primeLength = 1;
    std::vector<std::vector<float>> altScratch;
    PitchSettings lastPitch;
    ConsonantBlend consonants;
    DeEsser deEsser;
    VoiceTexture texture;
    SpatialEngine space;
    float secondGainLin = 0.0f, secondPeak = 0.0f;
    bool secondVoiceOn = false;
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
