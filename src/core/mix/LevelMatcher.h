#pragma once

namespace vs
{

// Automatic loudness compensation: slowly matches the RMS of the processed signal
// to the RMS of the (latency-aligned) original. Fixes the level loss measured with
// large transpositions + preserved formants, and makes A/B comparisons fair.
//
// Two behaviours:
//  legacy (robust = false, exactly dec340d):
//    ~400 ms RMS windows, gain smoothed ~300 ms, limits [-6, +18] dB, frozen while the
//    original's 400 ms RMS is below -50 dBFS.
//  robust (robust = true):
//    * gated measurement: the two RMS values are only updated while the voice is active, so
//      pauses never dilute the measurement and the gain never moves during silence;
//    * activity = 30 ms short-term level above -50 dBFS (opens) / -56 dBFS (closes, with a
//      200 ms hold) and not more than 15 dB below the running level (so short consonants and
//      gaps between words freeze the gain instead of being "corrected"). A dip longer than
//      150 ms is accepted as a genuinely quieter passage and measured again;
//    * gain limited to [-6, +12] dB, smoothed ~300 ms.
// Only the processed branch is ever level-matched; the original is never touched, so there is a
// single automatic level control in the whole chain.
class LevelMatcher
{
public:
    void prepare (double sampleRate);
    void reset();
    void setEnabled (bool e) { enabled = e; }
    void setRobust (bool r) { robust = r; }

    // Applies the gain to wet in place. dry is only analysed.
    void process (const float* const* dry, float* const* wet, int numChannels, int numSamples);

    float currentGainDb() const;
    bool voiceActive() const { return active; }

private:
    float refMs = 0.0f, wetMs = 0.0f, gain = 1.0f;
    float msCoeff = 0.0f, gainCoeff = 0.0f, shortCoeff = 0.0f;
    float shortMs = 0.0f;
    int holdSamples = 0, holdCounter = 0, dipCount = 0, dipMaxSamples = 0;
    bool enabled = true, robust = false, active = false;
};

} // namespace vs
