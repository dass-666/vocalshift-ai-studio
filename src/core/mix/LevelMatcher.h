#pragma once

namespace vs
{

// Automatic loudness compensation: slowly matches the RMS of the processed signal
// to the RMS of the (latency-aligned) dry signal. Fixes the level loss measured
// with large transpositions + preserved formants, and makes A/B comparisons fair.
//  - ~400 ms RMS windows, gain smoothed with a ~300 ms time constant
//  - gain limited to [-6, +18] dB
//  - frozen (holds its last value) while the dry signal is below -50 dBFS,
//    so silence, breaths and reverb tails are never pumped up.
class LevelMatcher
{
public:
    void prepare (double sampleRate);
    void reset();
    void setEnabled (bool e) { enabled = e; }

    // Applies the gain to wet in place. dry is only analysed.
    void process (const float* const* dry, float* const* wet, int numChannels, int numSamples);

    float currentGainDb() const;

private:
    float refMs = 0.0f, wetMs = 0.0f, gain = 1.0f;
    float msCoeff = 0.0f, gainCoeff = 0.0f;
    bool enabled = true;
};

} // namespace vs
