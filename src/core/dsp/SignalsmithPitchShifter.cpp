#include "SignalsmithPitchShifter.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <algorithm>
#include <cmath>

namespace vs
{

struct SignalsmithPitchShifter::Impl
{
    // Fixed seed: renders are bit-for-bit reproducible for the same input.
    signalsmith::stretch::SignalsmithStretch<float> stretch { 0x5eed };
    int channels = 0;
    int latency = 0;
    PitchSettings current;
    bool hasSettings = false;
};

SignalsmithPitchShifter::SignalsmithPitchShifter (bool splitComputation)
    : impl (std::make_unique<Impl>()), split (splitComputation)
{
}

SignalsmithPitchShifter::~SignalsmithPitchShifter() = default;

void SignalsmithPitchShifter::prepare (double sampleRate, int numChannels, int /*maxBlockSize*/)
{
    impl->channels = std::max (1, numChannels);
    // Quality-first preset (120 ms block, 30 ms interval). No low-latency mode is offered.
    impl->stretch.presetDefault (impl->channels, (float) sampleRate, split);
    impl->stretch.setFormantBase (0.0f); // 0 = automatic F0 estimate inside Signalsmith
    impl->latency = impl->stretch.inputLatency() + impl->stretch.outputLatency();
    impl->hasSettings = false;
    setSettings (impl->current);
    reset();
}

void SignalsmithPitchShifter::reset()
{
    impl->stretch.reset();
}

void SignalsmithPitchShifter::setSettings (const PitchSettings& s)
{
    PitchSettings c = s;
    c.semitones = std::clamp (c.semitones, -24.0f, 24.0f);
    c.formantPreservation = std::clamp (c.formantPreservation, 0.0f, 1.0f);
    c.formantShiftSemitones = std::clamp (c.formantShiftSemitones, -12.0f, 12.0f);

    if (impl->hasSettings
        && c.semitones == impl->current.semitones
        && c.formantPreservation == impl->current.formantPreservation
        && c.formantShiftSemitones == impl->current.formantShiftSemitones)
        return;

    impl->current = c;
    impl->hasSettings = true;

    impl->stretch.setTransposeSemitones (c.semitones);

    // With compensatePitch=true Signalsmith keeps the spectral envelope fixed while
    // the harmonics move. We then move the envelope by:
    //   (1 - preservation) * pitchShift   -> formants partially follow the pitch
    //   + formantShift                    -> independent timbre change (Voice Morph DSP)
    const float formantSemis = (1.0f - c.formantPreservation) * c.semitones + c.formantShiftSemitones;
    // Keep the formant path active whenever anything is transposed, so sweeping
    // "preservation" through 0 never switches algorithm mid-note.
    const bool needsFormantProcessing = c.semitones != 0.0f || c.formantShiftSemitones != 0.0f;
    if (needsFormantProcessing)
        impl->stretch.setFormantSemitones (formantSemis, true);
    else
        impl->stretch.setFormantFactor (1.0f, false); // nothing to do: identity transform
}

void SignalsmithPitchShifter::process (const float* const* in, float* const* out, int numSamples)
{
    if (numSamples <= 0)
        return;
    impl->stretch.process (in, numSamples, out, numSamples);
}

int SignalsmithPitchShifter::latencySamples() const
{
    return impl->latency;
}

} // namespace vs
