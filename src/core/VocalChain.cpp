#include "VocalChain.h"

#include "dsp/SignalsmithPitchShifter.h"

#include <algorithm>
#include <cmath>

namespace vs
{

VocalChain::VocalChain (std::unique_ptr<IPitchShifter> engine)
    : shifter (engine ? std::move (engine) : std::make_unique<SignalsmithPitchShifter>())
{
}

void VocalChain::prepare (double sampleRate, int numChannels, int maxBlockSize)
{
    channels = std::clamp (numChannels, 1, (int) ToneShaper::maxChannels);
    maxBlock = std::max (1, maxBlockSize);

    shifter->prepare (sampleRate, channels, maxBlock);
    latency = shifter->latencySamples();
    alt = std::make_unique<PsolaPitchShifter>();
    alt->setLatencyTarget (latency);
    alt->prepare (sampleRate, channels, maxBlock);
    if (alt->latencySamples() != latency)
        alt.reset(); // cannot align with the reference at this rate: the experimental engine stays unavailable
    xfadeLength = std::max (1, (int) (0.1 * sampleRate));
    primeLength = latency + 2048;
    switchPhase = switchCount = 0;
    targetEngine = alt ? requestedEngine : 0;
    activeEngine = targetEngine;
    consonants.prepare (sampleRate, channels);
    deEsser.prepare (sampleRate, channels);
    texture.prepare (sampleRate, channels);
    space.prepare (sampleRate, channels);
    tone.prepare (sampleRate, channels);
    enhancer.prepare (sampleRate, channels);
    levelMatcher.prepare (sampleRate);
    mixer.prepare (sampleRate, channels, maxBlock, latency);

    semitoneSmoother.prepare (sampleRate, 0.05);
    preservationSmoother.prepare (sampleRate, 0.05);
    formantSmoother.prepare (sampleRate, 0.05);
    semitoneSmoother.snapTo (semitoneSmoother.getTarget());
    preservationSmoother.snapTo (preservationSmoother.getTarget());
    formantSmoother.snapTo (formantSmoother.getTarget());

    auto alloc = [&] (std::vector<std::vector<float>>& v) {
        v.assign ((size_t) channels, std::vector<float> ((size_t) maxBlock, 0.0f));
    };
    alloc (inScratch);
    alloc (wetScratch);
    alloc (dryScratch);
    alloc (altScratch);
    inPtrs.resize ((size_t) channels);
    wetPtrs.resize ((size_t) channels);
    dryPtrs.resize ((size_t) channels);
    ioPtrs.resize ((size_t) channels);
    for (int c = 0; c < channels; ++c)
    {
        inPtrs[(size_t) c] = inScratch[(size_t) c].data();
        wetPtrs[(size_t) c] = wetScratch[(size_t) c].data();
        dryPtrs[(size_t) c] = dryScratch[(size_t) c].data();
    }
    reset();
}

void VocalChain::reset()
{
    shifter->reset();
    if (alt) alt->reset();
    switchPhase = switchCount = 0;
    activeEngine = targetEngine;
    consonants.reset();
    deEsser.reset();
    texture.reset();
    space.reset();
    tone.reset();
    enhancer.reset();
    levelMatcher.reset();
    mixer.reset();
    needsReset = false;
}

void VocalChain::setEngineSettings (const EngineSettings& e, bool bypassed)
{
    requestedEngine = std::clamp (e.pitchEngine, 0, 1);
    targetEngine = alt ? requestedEngine : 0;
    semitoneSmoother.setTarget (std::clamp (e.semitones, -24.0f, 24.0f));
    preservationSmoother.setTarget (std::clamp (e.formantPreservation, 0.0f, 1.0f));
    formantSmoother.setTarget (std::clamp (e.formantShiftSemitones, -12.0f, 12.0f));
    tone.setBrightnessDb (std::clamp (e.brightnessDb, -12.0f, 12.0f));
    enhancer.setMagic (std::clamp (e.magic01, 0.0f, 1.0f));
    levelMatcher.setEnabled (e.autoLevel);
    levelMatcher.setRobust (e.improved);
    consonants.setEnabled (e.improved);
    deEsser.setAmount (e.improved ? std::clamp (e.magic01, 0.0f, 1.0f) : 0.0f);
    texture.setAmount (std::clamp (e.textureAmount, 0.0f, 1.0f));
    {
        SpatialParams sp;
        sp.amount = e.spatialAmount;
        sp.width = e.spaceWidth;
        sp.reverbGain = e.spaceReverb;
        sp.rt60 = e.spaceRt60;
        sp.slapGain = e.spaceSlap;
        sp.modDepthMs = e.spaceModMs;
        space.setParams (sp);
    }
    secondVoiceOn = e.secondVoice;
    secondGainLin = e.secondGainDb <= VocalMixEngine::faderSilenceDb ? 0.0f : std::pow (10.0f, std::clamp (e.secondGainDb, -60.0f, 6.0f) / 20.0f);
    mixer.setRouting (e.secondVoice, std::clamp (e.mix01, 0.0f, 1.0f),
                      std::clamp (e.secondGainDb, -60.0f, 6.0f), std::clamp (e.pan, -1.0f, 1.0f),
                      std::clamp (e.offsetMs, 0.0f, VocalMixEngine::maxOffsetMs));
    mixer.setOutputGainDb (std::clamp (e.outputGainDb, -24.0f, 12.0f));
    mixer.setSafetyClip (e.safetyClip);
    mixer.setBypassed (bypassed);
}

void VocalChain::process (float* const* io, int numChannels, int numSamples)
{
    const int nCh = std::min (numChannels, channels);
    if (needsReset)
        reset();

    inPeak = outPeak = 0.0f;
    for (int c = 0; c < nCh; ++c)
        for (int i = 0; i < numSamples; ++i)
            inPeak = std::max (inPeak, std::abs (io[c][i]));

    for (int offset = 0; offset < numSamples; offset += maxBlock)
        processChunk (io, nCh, offset, std::min (maxBlock, numSamples - offset));

    // Safety net: never let NaN/inf reach the host. Mute the block and reset DSP state.
    bool finite = true;
    for (int c = 0; c < nCh && finite; ++c)
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = io[c][i];
            if (! std::isfinite (v)) { finite = false; break; }
            outPeak = std::max (outPeak, std::abs (v));
        }
    if (! finite)
    {
        for (int c = 0; c < nCh; ++c)
            std::fill (io[c], io[c] + numSamples, 0.0f);
        outPeak = 0.0f;
        ++nonFiniteCount;
        needsReset = true;
    }
}

void VocalChain::processChunk (float* const* io, int nCh, int offset, int n)
{
    for (int c = 0; c < nCh; ++c)
    {
        std::copy (io[c] + offset, io[c] + offset + n, inScratch[(size_t) c].begin());
        ioPtrs[(size_t) c] = io[c] + offset;
    }
    // The engine is configured for `channels`; feed silence to any channel the host did not give us.
    for (int c = nCh; c < channels; ++c)
        std::fill (inScratch[(size_t) c].begin(), inScratch[(size_t) c].begin() + n, 0.0f);
    // Input sanitisation: a host can hand us garbage; do not poison the FFT state.
    for (int c = 0; c < nCh; ++c)
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (inScratch[(size_t) c][(size_t) i]))
                inScratch[(size_t) c][(size_t) i] = 0.0f;

    mixer.pushDry (inPtrs.data(), dryPtrs.data(), nCh, n);

    // Engine switch: the new engine runs alongside (priming covers its latency), then a 100 ms equal-power crossfade.
    if (switchPhase == 0 && targetEngine != activeEngine)
    {
        (activeEngine == 0 ? static_cast<IPitchShifter&> (*alt) : *shifter).reset();
        switchPhase = 1;
        switchCount = 0;
    }
    const bool both = switchPhase != 0;
    IPitchShifter& activeE = activeEngine == 1 ? static_cast<IPitchShifter&> (*alt) : *shifter;
    IPitchShifter& otherE = activeEngine == 1 ? *shifter : static_cast<IPitchShifter&> (*alt);

    // Update engine settings every 32 samples while smoothing.
    constexpr int sub = 32;
    for (int s = 0; s < n; s += sub)
    {
        const int len = std::min (sub, n - s);
        PitchSettings ps;
        ps.semitones = semitoneSmoother.advance (len);
        ps.formantPreservation = preservationSmoother.advance (len);
        ps.formantShiftSemitones = formantSmoother.advance (len);
        activeE.setSettings (ps);
        if (both) otherE.setSettings (ps);

        const float* inSub[ToneShaper::maxChannels];
        float* wetSub[ToneShaper::maxChannels];
        float* altSub[ToneShaper::maxChannels];
        for (int c = 0; c < channels; ++c)
        {
            inSub[c] = inPtrs[(size_t) c] + s;
            wetSub[c] = wetPtrs[(size_t) c] + s;
            altSub[c] = altScratch[(size_t) c].data() + s;
        }
        activeE.process (inSub, wetSub, len);
        if (both) otherE.process (inSub, altSub, len);
    }
    if (both)
    {
        for (int i = 0; i < n; ++i)
        {
            float g = 0.0f; // weight of the other (incoming) engine
            if (switchPhase == 2)
                g = std::min (1.0f, (float) switchCount / (float) xfadeLength);
            const float wa = std::cos (1.5707963f * g), wb = std::sin (1.5707963f * g);
            for (int c = 0; c < channels; ++c)
                wetScratch[(size_t) c][(size_t) i] = wa * wetScratch[(size_t) c][(size_t) i] + wb * altScratch[(size_t) c][(size_t) i];
            ++switchCount;
            if (switchPhase == 1 && switchCount >= primeLength) { switchPhase = 2; switchCount = 0; }
            else if (switchPhase == 2 && switchCount >= xfadeLength)
            {
                // The remaining samples of this chunk belong entirely to the new engine.
                activeEngine = 1 - activeEngine;
                switchPhase = 0;
                for (int j = i + 1; j < n; ++j)
                    for (int c = 0; c < channels; ++c)
                        wetScratch[(size_t) c][(size_t) j] = altScratch[(size_t) c][(size_t) j];
                break;
            }
        }
    }

    consonants.process (dryPtrs.data(), wetPtrs.data(), nCh, n);
    tone.process (wetPtrs.data(), nCh, n);
    texture.process (wetPtrs.data(), nCh, n);
    deEsser.process (wetPtrs.data(), nCh, n); // before the compressor/air so sibilance is not boosted
    enhancer.process (wetPtrs.data(), nCh, n);
    levelMatcher.process (dryPtrs.data(), wetPtrs.data(), nCh, n);
    levelGainDb.store (levelMatcher.currentGainDb(), std::memory_order_relaxed);
    space.process (wetPtrs.data(), nCh, n); // after level matching: the reverb tail must not steer the auto level
    if (secondVoiceOn)
    {
        float pk = 0.0f;
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
                pk = std::max (pk, std::abs (wetPtrs[(size_t) c][i]));
        secondPeak = pk * secondGainLin;
    }
    else
        secondPeak = 0.0f;
    mixer.mixInto (wetPtrs.data(), dryPtrs.data(), ioPtrs.data(), nCh, n);
}

} // namespace vs
