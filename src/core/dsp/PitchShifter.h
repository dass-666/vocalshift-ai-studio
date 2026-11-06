#pragma once

// IdentityEngine: abstract pitch/formant shifter.
//
// Any engine (Signalsmith today; PSOLA/WORLD-style or a commercial engine later)
// must implement this interface so it can be swapped after listening tests.
//
// Real-time contract:
//   - prepare()          : message thread only, may allocate.
//   - reset(), setSettings(), process(), latencySamples() : real-time safe
//     (no allocation, no locks, no I/O).

namespace vs
{

struct PitchSettings
{
    float semitones = 0.0f;          // total transposition, clamped to [-24, +24]
    float formantPreservation = 1.0f; // 1 = formants stay where they were (identity), 0 = formants follow pitch
    float formantShiftSemitones = 0.0f; // extra formant (timbre) shift, independent of pitch, [-12, +12]
};

class IPitchShifter
{
public:
    virtual ~IPitchShifter() = default;

    virtual void prepare (double sampleRate, int numChannels, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void setSettings (const PitchSettings& settings) = 0;

    // in and out must not alias. numSamples may be anything in [0, maxBlockSize].
    virtual void process (const float* const* in, float* const* out, int numSamples) = 0;

    // Total input->output delay in samples (constant after prepare()).
    virtual int latencySamples() const = 0;
    virtual const char* engineName() const = 0;
};

} // namespace vs
