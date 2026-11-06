#pragma once

#include "../dsp/LinearSmoother.h"

#include <vector>

namespace vs
{

// Urban Spatial Engine: spatial finish of the SECOND VOICE only (it runs on the processed branch,
// before the fader, so the fader scales voice + space together; the original voice never goes through it).
// Stereo only; with one channel it does nothing.
//
//   m = (L + R) / 2
//   Width : side signal s = 0.5 * (D1(m) - D2(m)), D1/D2 = micro-delays of 11 / 17 ms with slow, shallow, out-of-phase
//           modulation, band-limited (high-pass 120 Hz keeps the low end centred, low-pass 6.5 kHz tames brightness).
//           L += g s, R -= g s.   The mono sum L + R is unchanged => no comb cancellation in mono (mono-safe by construction).
//   Reverb: 4-line feedback delay network (orthonormal Hadamard feedback, per-line damping, RT60 set per line gain),
//           fed by m after a fixed 28 ms predelay, high-pass 250 Hz / low-pass 5 kHz on the send. Stable by construction
//           (orthogonal matrix x gains < 1 x low-pass <= 1).
//   Slap  : filtered single echo (high-pass 300 Hz, low-pass 3.2 kHz, feedback 0.18); L/R times differ by 13 %.
//           Time = 1/16 note from the host tempo when known, folded into 70-180 ms; 115 ms otherwise.
//   Output level is normalised so that widening does not make the voice louder (equal-loudness comparisons).
//   All gains are 0 => exact bit-for-bit bypass; coming back from bypass starts from an empty state.
// All values are PROVISIONAL (engineering values, not tuned by listening). Real-time safe after prepare().
struct SpatialParams
{
    float amount = 0.0f;     // master 0..1 (route / on-off switch, smoothed)
    float width = 0.0f;      // 0..0.95
    float reverbGain = 0.0f; // linear return level, 0..0.6
    float rt60 = 1.2f;       // seconds, 0.6..2.0
    float slapGain = 0.0f;   // linear return level, 0..0.3
    float modDepthMs = 0.4f; // micro-delay modulation depth, 0..1 ms
};

class SpatialEngine
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setParams (const SpatialParams& p);
    void setTempoBpm (double bpm) { tempoBpm = bpm; }
    void process (float* const* io, int numChannels, int numSamples);

    // Diagnostics
    bool isActive() const { return active; }
    long memoryBytes() const;
    static constexpr float predelayMs = 28.0f;

private:
    struct OnePole { float a = 0.0f, z = 0.0f; };
    struct Line { std::vector<float> buf; int len = 1, pos = 0; float gain = 0.0f; float lp = 0.0f; };
    static float lowpass (OnePole& f, float x) { f.z += f.a * (x - f.z); return f.z; }
    static float highpass (OnePole& f, float x) { f.z += f.a * (x - f.z); return x - f.z; }
    static float readFrac (const std::vector<float>& b, int size, int writePos, float delay);
    void updateRt60 (float rt60);
    void clearState();

    double fs = 48000.0, tempoBpm = 0.0;
    int channels = 0;
    bool active = false, wasActive = false;

    SpatialParams target;
    LinearSmoother amountS, widthS, reverbS, slapS, modS;
    float rt60Current = 1.2f, rt60Target = 1.2f;

    // width
    std::vector<float> sideBuf;           // mono source history
    int sideSize = 1, sidePos = 0;
    float sideBase1 = 0, sideBase2 = 0;   // samples
    float lfoPhase1 = 0, lfoPhase2 = 0, lfoInc1 = 0, lfoInc2 = 0;
    OnePole sideHp, sideLp;
    // reverb
    std::vector<float> preBuf;
    int preSize = 1, prePos = 0, preDelay = 1;
    OnePole sendHp, sendLp;
    Line lines[4];
    float lineDamp = 0.0f;
    // slap
    std::vector<float> slapBufL, slapBufR;
    int slapSize = 1, slapPos = 0;
    float slapTimeL = 0, slapTimeR = 0;
    OnePole slapHp[2], slapLp[2];
    float slapFbL = 0, slapFbR = 0;
};

} // namespace vs
