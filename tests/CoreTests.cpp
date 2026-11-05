// DSP / engine tests on the JUCE-free core (vs_core).
// Every threshold is an engineering acceptance criterion, not a perceptual claim.

#include "Signals.h"
#include "TestFramework.h"

#include <core/VocalChain.h>
#include <core/analysis/PitchDetector.h>
#include <core/dsp/SignalsmithPitchShifter.h>
#include <core/mix/VocalMixEngine.h>
#include <core/state/FactoryPresets.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>

using namespace vstest;

// ---------------------------------------------------------------------------
// Allocation tracking: counts heap allocations while `g_trackAllocs` is set.
static std::atomic<bool> g_trackAllocs { false };
static std::atomic<long> g_allocCount { 0 };

void* operator new (std::size_t n)
{
    if (g_trackAllocs.load (std::memory_order_relaxed))
        g_allocCount.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

// ---------------------------------------------------------------------------
namespace
{
using Signal = std::vector<float>;

vs::VoiceParams pitchParams (float st, float preservation = 100.0f)
{
    vs::VoiceParams p;
    p.semitones = st;
    p.formantPreservation = preservation;
    p.safetyClip = false; // measure the engine, not the clipper
    return p;
}

// Runs a mono or stereo signal through a VocalChain with a fixed or random block size.
std::vector<Signal> run (vs::VocalChain& chain, const std::vector<Signal>& in, int blockSize,
                         const std::function<void (int)>& perBlock = nullptr, uint32_t randomBlocksSeed = 0)
{
    const int ch = (int) in.size();
    const int n = (int) in[0].size();
    std::vector<Signal> out = in;
    std::mt19937 rng (randomBlocksSeed);
    std::vector<float*> ptrs ((size_t) ch);
    int pos = 0;
    while (pos < n)
    {
        int len = blockSize;
        if (randomBlocksSeed != 0)
            len = 1 + (int) (rng() % (uint32_t) blockSize);
        len = std::min (len, n - pos);
        if (perBlock) perBlock (pos);
        for (int c = 0; c < ch; ++c)
            ptrs[(size_t) c] = out[(size_t) c].data() + pos;
        chain.process (ptrs.data(), ch, len);
        pos += len;
    }
    return out;
}

Signal segment (const Signal& x, int start, int len)
{
    start = std::clamp (start, 0, (int) x.size());
    len = std::clamp (len, 0, (int) x.size() - start);
    return Signal (x.begin() + start, x.begin() + start + len);
}

bool allFinite (const Signal& x)
{
    return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v); });
}

int onsetIndex (const Signal& x, float threshold)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (std::abs (x[i]) > threshold)
            return (int) i;
    return -1;
}

int lastIndexAbove (const Signal& x, float threshold)
{
    for (int i = (int) x.size() - 1; i >= 0; --i)
        if (std::abs (x[(size_t) i]) > threshold)
            return i;
    return -1;
}
} // namespace

// ---------------------------------------------------------------------------
TEST ("pitch: F0 moves by requested interval (220 Hz harmonic tone, 48 kHz)")
{
    const double fs = 48000;
    const auto tone = harmonicTone (fs, 220.0, 3.0);
    vs::YinPitchDetector yin (fs, 40.0f, 3000.0f);
    const float inF0 = yin.medianF0 (tone.data(), (int) tone.size());
    MEASURE ("input median F0 = %.2f Hz", inF0);

    for (float st : { -24.f, -12.f, -7.f, -3.f, 3.f, 7.f, 12.f, 24.f })
    {
        vs::VocalChain chain;
        chain.setParams (pitchParams (st, 0.0f), false);
        chain.prepare (fs, 1, 512);
        auto out = run (chain, { tone }, 512);
        const int lat = chain.latencySamples();
        // Analyse the steady-state middle second.
        auto mid = segment (out[0], lat + (int) fs, (int) fs);
        const float outF0 = yin.medianF0 (mid.data(), (int) mid.size());
        const double errCents = centsBetween (inF0 * std::pow (2.0, st / 12.0), outF0);
        MEASURE ("%+5.0f st: out F0 = %8.2f Hz, error = %+6.2f cents", st, outF0, errCents);
        CHECK_MSG (outF0 > 0 && std::abs (errCents) < 15.0, "st=%.0f err=%.2f cents", st, errCents);
    }
}

TEST ("pitch: fine control (+0.5 st and -0.25 st)")
{
    const double fs = 48000;
    const auto tone = harmonicTone (fs, 200.0, 2.5);
    vs::YinPitchDetector yin (fs);
    const float inF0 = yin.medianF0 (tone.data(), (int) tone.size());
    for (float cents : { 50.f, -25.f })
    {
        vs::VocalChain chain;
        auto p = pitchParams (0.0f, 0.0f);
        p.fineCents = cents;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 256);
        auto out = run (chain, { tone }, 256);
        auto mid = segment (out[0], chain.latencySamples() + (int) fs / 2, (int) fs);
        const float outF0 = yin.medianF0 (mid.data(), (int) mid.size());
        const double measured = centsBetween (inF0, outF0);
        MEASURE ("requested %+.0f cents, measured %+.2f cents", cents, measured);
        CHECK_MSG (std::abs (measured - cents) < 6.0, "measured %.2f", measured);
    }
}

TEST ("timing: reported latency matches measured delay; duration preserved")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        // 0.5 s tone burst between silences.
        const int n = (int) (fs * 2.5);
        Signal x ((size_t) n, 0.0f);
        const auto burst = harmonicTone (fs, 220.0, 0.5);
        const int start = (int) (fs * 0.5);
        std::copy (burst.begin(), burst.end(), x.begin() + start);

        for (float st : { 0.f, 7.f, -12.f })
        {
            vs::VocalChain chain;
            chain.setParams (pitchParams (st, 0.0f), false);
            chain.prepare (fs, 1, 480);
            auto out = run (chain, { x }, 480);
            const int lat = chain.latencySamples();
            const int onIn = onsetIndex (x, 0.01f), offIn = lastIndexAbove (x, 0.01f);
            const int onOut = onsetIndex (out[0], 0.01f), offOut = lastIndexAbove (out[0], 0.01f);
            const double onsetErrMs = 1000.0 * (onOut - onIn - lat) / fs;
            const double durIn = (offIn - onIn) / fs, durOut = (offOut - onOut) / fs;
            MEASURE ("fs=%.0f st=%+.0f latency=%d (%.1f ms) onset error=%+.1f ms duration in=%.3f s out=%.3f s",
                     fs, st, lat, 1000.0 * lat / fs, onsetErrMs, durIn, durOut);
            // Spectral processing smears onsets by up to ~half a window; accept +-40 ms.
            CHECK_MSG (std::abs (onsetErrMs) < 40.0, "onset error %.1f ms", onsetErrMs);
            CHECK_MSG (std::abs (durOut - durIn) < 0.08, "duration diff %.3f s", durOut - durIn);
            CHECK ((int) out[0].size() == n);
        }
    }
}

TEST ("safety: silence in -> silence out, no noise floor")
{
    vs::VocalChain chain;
    chain.setParams (pitchParams (12.0f), false);
    chain.prepare (48000, 2, 512);
    Signal z ((size_t) 48000 * 3, 0.0f);
    auto out = run (chain, { z, z }, 512);
    const float peak = std::max (vs::peakAbs (out[0].data(), (int) z.size()), vs::peakAbs (out[1].data(), (int) z.size()));
    MEASURE ("peak output on digital silence = %g", peak);
    CHECK (peak < 1.0e-6f);

    // Silence after signal must decay to silence (no stuck tails / noise).
    vs::VocalChain chain2;
    chain2.setParams (pitchParams (-7.0f), false);
    chain2.prepare (48000, 1, 512);
    Signal x ((size_t) 48000 * 4, 0.0f);
    const auto t = harmonicTone (48000, 180, 1.0);
    std::copy (t.begin(), t.end(), x.begin());
    auto o2 = run (chain2, { x }, 512);
    auto tail = segment (o2[0], 48000 * 3, 48000);
    const float tailPeak = vs::peakAbs (tail.data(), (int) tail.size());
    MEASURE ("peak 2 s after signal end = %g", tailPeak);
    CHECK (tailPeak < 1.0e-5f);
}

TEST ("safety: no NaN/inf and bounded peaks under hostile input and automation")
{
    const double fs = 48000;
    const int n = (int) fs * 4;
    auto noise = whiteNoise (n, 1.0f, 7);
    Signal square ((size_t) n);
    for (int i = 0; i < n; ++i)
        square[(size_t) i] = ((i / 60) % 2) ? 1.0f : -1.0f;

    for (bool clip : { false, true })
    {
        vs::VocalChain chain;
        chain.prepare (fs, 2, 256);
        std::mt19937 rng (99);
        auto out = run (chain, { noise, square }, 256, [&] (int) {
            vs::VoiceParams p;
            p.semitones = (float) ((int) (rng() % 49) - 24);
            p.fineCents = (float) ((int) (rng() % 201) - 100);
            p.formantPreservation = (float) (rng() % 101);
            p.formantShift = (float) ((int) (rng() % 25) - 12);
            p.brightnessDb = (float) ((int) (rng() % 25) - 12);
            p.mixPercent = (float) (rng() % 101);
            p.outputGainDb = 0.0f;
            p.safetyClip = clip;
            chain.setParams (p, (rng() % 13) == 0);
        });
        const float peak = std::max (vs::peakAbs (out[0].data(), n), vs::peakAbs (out[1].data(), n));
        MEASURE ("safetyClip=%d: full-scale noise/square, random automation every 256 samples: peak out = %.3f (%.1f dBFS), nonFiniteEvents=%d",
                 (int) clip, peak, 20 * std::log10 (peak), chain.nonFiniteEvents());
        CHECK (allFinite (out[0]) && allFinite (out[1]));
        CHECK (chain.nonFiniteEvents() == 0);
        if (clip)
            CHECK (peak <= 1.0f);
        else
            CHECK_MSG (peak < 8.0f, "peak %.2f", peak); // +18 dBFS hard sanity bound for un-clipped extreme settings
    }

    // NaN injected at the input must never reach the output.
    vs::VocalChain chain;
    chain.setParams (pitchParams (5.0f), false);
    chain.prepare (fs, 1, 128);
    auto x = harmonicTone (fs, 200, 1.0);
    x[1000] = std::numeric_limits<float>::quiet_NaN();
    x[2000] = std::numeric_limits<float>::infinity();
    auto out = run (chain, { x }, 128);
    CHECK (allFinite (out[0]));
}

TEST ("realtime: zero heap allocations inside process() and setParams()")
{
    vs::VocalChain chain;
    chain.prepare (48000, 2, 1024);
    auto a = harmonicTone (48000, 210, 2.0);
    std::vector<Signal> in { a, a };
    std::vector<float*> ptrs (2);
    // warm-up
    ptrs[0] = in[0].data(); ptrs[1] = in[1].data();
    chain.process (ptrs.data(), 2, 1024);

    g_allocCount = 0;
    g_trackAllocs = true;
    int pos = 1024;
    int block = 1;
    while (pos + 1024 < (int) a.size())
    {
        vs::VoiceParams p;
        p.semitones = (float) ((pos / 4096) % 25 - 12);
        p.formantShift = (float) ((pos / 8192) % 7 - 3);
        p.brightnessDb = 3.0f;
        chain.setParams (p, (pos / 20000) % 2 == 1);
        ptrs[0] = in[0].data() + pos; ptrs[1] = in[1].data() + pos;
        chain.process (ptrs.data(), 2, block);
        pos += block;
        block = block % 1024 + 37;
    }
    g_trackAllocs = false;
    MEASURE ("allocations during %d samples of processing = %ld", pos, g_allocCount.load());
    CHECK (g_allocCount.load() == 0);
}

TEST ("determinism: output independent of host block size (incl. random sizes)")
{
    const double fs = 48000;
    const auto v = vowel (fs, 200, 2.0);
    auto render = [&] (int bs, uint32_t seed) {
        vs::VocalChain chain;
        chain.setParams (pitchParams (5.0f), false);
        chain.prepare (fs, 1, 2048);
        return run (chain, { v }, bs, nullptr, seed)[0];
    };
    const auto ref = render (512, 0);
    for (auto [bs, seed] : { std::pair<int, uint32_t> { 64, 0 }, { 2048, 0 }, { 1, 0 }, { 1500, 42 } })
    {
        const auto o = render (bs, seed);
        float maxDiff = 0.0f;
        for (size_t i = 0; i < o.size(); ++i)
            maxDiff = std::max (maxDiff, std::abs (o[i] - ref[i]));
        MEASURE ("block=%d%s: max |diff| vs 512 = %g", bs, seed ? " (random 1..N)" : "", maxDiff);
        // Smoothers advance per 32-sample sub-block; tiny differences are allowed.
        CHECK_MSG (maxDiff < 1.0e-3f, "diff %g", maxDiff);
    }
    // Reproducibility: same settings twice -> bit identical.
    const auto again = render (512, 0);
    CHECK (again == ref);
}

TEST ("sample rates: +7 st correct at 44.1 / 48 / 88.2 / 96 kHz")
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        const auto tone = harmonicTone (fs, 196.0, 2.5);
        vs::YinPitchDetector yin (fs);
        const float inF0 = yin.medianF0 (tone.data(), (int) tone.size());
        vs::VocalChain chain;
        chain.setParams (pitchParams (7.0f, 0.0f), false);
        chain.prepare (fs, 2, 1024);
        auto out = run (chain, { tone, tone }, 1024);
        auto mid = segment (out[1], chain.latencySamples() + (int) (fs * 0.5), (int) fs);
        const float outF0 = yin.medianF0 (mid.data(), (int) mid.size());
        const double err = centsBetween (inF0 * std::pow (2.0, 7.0 / 12.0), outF0);
        MEASURE ("fs=%.0f: latency %d samples (%.1f ms), +7 st error %+.2f cents",
                 fs, chain.latencySamples(), 1000.0 * chain.latencySamples() / fs, err);
        CHECK (std::abs (err) < 15.0);
    }
}

TEST ("formants: preservation keeps spectral centroid, follow mode moves it")
{
    // Vowel with fixed formants. Centroid is a coarse proxy for the envelope;
    // it is NOT a naturalness measurement.
    const double fs = 48000;
    const auto v = vowel (fs, 150, 2.5, 0.3f, 0.0, 0.0);
    auto inMid = segment (v, (int) fs, 8192);
    const float cIn = vs::spectralCentroidHz (inMid.data(), (int) inMid.size(), fs);
    auto centroidFor = [&] (float st, float pres, float formantShift) {
        vs::VocalChain chain;
        auto p = pitchParams (st, pres);
        p.formantShift = formantShift;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        auto out = run (chain, { v }, 512);
        auto mid = segment (out[0], chain.latencySamples() + (int) fs, 8192);
        return vs::spectralCentroidHz (mid.data(), (int) mid.size(), fs);
    };
    const float cPreserve = centroidFor (12, 100, 0), cFollow = centroidFor (12, 0, 0);
    const float cPreserveDown = centroidFor (-12, 100, 0), cFollowDown = centroidFor (-12, 0, 0);
    const float cShiftUp = centroidFor (0, 100, 4), cShiftDown = centroidFor (0, 100, -4);
    MEASURE ("centroid in=%.0f Hz | +12 preserve=%.0f follow=%.0f | -12 preserve=%.0f follow=%.0f | formant +4=%.0f -4=%.0f",
             cIn, cPreserve, cFollow, cPreserveDown, cFollowDown, cShiftUp, cShiftDown);
    CHECK (std::abs (std::log2 (cPreserve / cIn)) < std::abs (std::log2 (cFollow / cIn)));
    CHECK (std::abs (std::log2 (cPreserveDown / cIn)) < std::abs (std::log2 (cFollowDown / cIn)));
    CHECK (cFollow > cIn * 1.4f);
    CHECK (cFollowDown < cIn * 0.8f);
    CHECK (cShiftUp > cIn && cShiftDown < cIn);
}

TEST ("mix: mix=0 is the latency-aligned dry signal; gain is exact")
{
    const double fs = 48000;
    auto x = harmonicTone (fs, 300, 1.0);
    vs::VocalChain chain;
    auto p = pitchParams (12.0f);
    p.mixPercent = 0.0f;
    p.outputGainDb = -6.0f;
    chain.setParams (p, false);
    chain.prepare (fs, 1, 300);
    auto out = run (chain, { x }, 300);
    const int lat = chain.latencySamples();
    const float g = std::pow (10.0f, -6.0f / 20.0f);
    float maxErr = 0.0f;
    for (size_t i = (size_t) lat; i < x.size(); ++i)
        maxErr = std::max (maxErr, std::abs (out[0][i] - g * x[i - (size_t) lat]));
    MEASURE ("max error vs delayed dry * -6 dB = %g", maxErr);
    CHECK (maxErr < 1.0e-5f);
}

TEST ("bypass: click-free crossfade and exact pass-through once settled")
{
    // Pure sine so that any discontinuity introduced by the toggle stands out.
    const double fs = 48000;
    Signal x ((size_t) (fs * 3));
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.3f * (float) std::sin (2 * kPi * 220.0 * (double) i / fs);
    vs::VocalChain chain;
    chain.setParams (pitchParams (7.0f), false);
    chain.prepare (fs, 1, 256);
    const int toggleAt = (int) fs; // toggle to bypass at 1 s
    auto out = run (chain, { x }, 256, [&] (int pos) {
        chain.setParams (pitchParams (7.0f), pos >= toggleAt);
    });
    const int lat = chain.latencySamples();
    auto maxStep = [&] (const Signal& s, int a, int b) {
        float m = 0;
        for (int i = a + 1; i < b; ++i)
            m = std::max (m, std::abs (s[(size_t) i] - s[(size_t) i - 1]));
        return m;
    };
    const float stepToggle = maxStep (out[0], toggleAt - 512, toggleAt + 4096);
    const float stepWet = maxStep (out[0], lat + 4800, toggleAt - 512);
    const float stepDry = maxStep (x, 1, (int) fs);
    float settledErr = 0;
    for (int i = toggleAt + 4800; i < (int) x.size(); ++i)
        settledErr = std::max (settledErr, std::abs (out[0][(size_t) i] - x[(size_t) (i - lat)]));
    MEASURE ("max sample step: at bypass toggle = %.4f, steady wet = %.4f, dry = %.4f; settled bypass error = %g",
             stepToggle, stepWet, stepDry, settledErr);
    CHECK (stepToggle < 1.5f * std::max (stepWet, stepDry) + 0.002f);
    CHECK (settledErr < 1.0e-6f);
}

TEST ("automation: rapid semitone jumps stay bounded and finite")
{
    const double fs = 48000;
    auto v = vowel (fs, 180, 4.0);
    vs::VocalChain chain;
    chain.prepare (fs, 1, 128);
    std::mt19937 rng (5);
    float target = 0;
    auto out = run (chain, { v }, 128, [&] (int pos) {
        if (pos % 480 == 0) target = (float) ((int) (rng() % 49) - 24); // new value every 10 ms
        auto p = pitchParams (target);
        p.safetyClip = false;
        chain.setParams (p, false);
    });
    const float inPeak = vs::peakAbs (v.data(), (int) v.size());
    const float peak = vs::peakAbs (out[0].data(), (int) out[0].size());
    MEASURE ("random +-24 st every 10 ms: in peak %.3f, out peak %.3f (ratio %.2f dB)", inPeak, peak, 20 * std::log10 (peak / inPeak));
    CHECK (allFinite (out[0]));
    CHECK (peak < inPeak * 4.0f); // < +12 dB over input
}

TEST ("presets: every factory preset is reproducible and audibly different from neutral")
{
    const double fs = 48000;
    auto v = vowel (fs, 190, 2.0);
    vs::YinPitchDetector yin (fs);
    const float inF0 = yin.medianF0 (v.data(), (int) v.size());
    auto render = [&] (const vs::VoiceParams& p) {
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        return run (chain, { v }, 512)[0];
    };
    const auto neutral = render (vs::findFactoryPreset ("neutral")->params);
    for (const auto& preset : vs::factoryPresets())
    {
        const auto a = render (preset.params);
        const auto b = render (preset.params);
        double diffEnergy = 0, refEnergy = 0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            diffEnergy += (double) (a[i] - neutral[i]) * (a[i] - neutral[i]);
            refEnergy += (double) neutral[i] * neutral[i];
        }
        const double diffDb = 10 * std::log10 ((diffEnergy + 1e-20) / (refEnergy + 1e-20));
        auto mid = segment (a, (int) (fs * 0.8), (int) fs);
        const float f0 = yin.medianF0 (mid.data(), (int) mid.size());
        const double movedCents = f0 > 0 ? centsBetween (inF0, f0) : 0.0;
        MEASURE ("%-24s reproducible=%d  diff vs neutral=%6.1f dB  F0 moved %+7.1f cents (expected %+5.0f)",
                 preset.id.c_str(), (int) (a == b), diffDb, movedCents, preset.params.totalSemitones() * 100);
        CHECK (a == b);
        if (preset.id != "neutral")
            CHECK_MSG (diffDb > -20.0, "%s too similar to neutral (%.1f dB)", preset.id.c_str(), diffDb);
        if (preset.params.totalSemitones() != 0)
            CHECK_MSG (std::abs (movedCents - preset.params.totalSemitones() * 100) < 20.0, "%s", preset.id.c_str());
    }
}

TEST ("performance: real-time factor (stereo, 48 kHz, 512-sample blocks)")
{
    const double fs = 48000;
    const int seconds = 10;
    auto v = vowel (fs, 200, seconds);
    for (auto [st, form] : { std::pair<float, float> { 0, 0 }, { 7, 0 }, { -12, 2 } })
    {
        vs::VocalChain chain;
        auto p = pitchParams (st);
        p.formantShift = form;
        chain.setParams (p, false);
        chain.prepare (fs, 2, 512);
        std::vector<Signal> in { v, v };
        const auto t0 = std::chrono::steady_clock::now();
        auto out = run (chain, in, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        MEASURE ("st=%+.0f formant=%+.0f: %.3f s CPU for %d s audio -> %.1f%% of one core (this machine)",
                 st, form, secs, seconds, 100.0 * secs / seconds);
        CHECK (secs < seconds * 0.5); // generous: must be far faster than real time
    }
}

int main (int argc, char** argv)
{
    return vstest::runAll (argc, argv);
}
