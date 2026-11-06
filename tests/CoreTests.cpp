// DSP / engine tests on the JUCE-free core (vs_core).
// Every threshold is an engineering acceptance criterion, not a perceptual claim.

#include "Signals.h"
#include "TestFramework.h"

#include <core/VocalChain.h>
#include <core/analysis/PitchDetector.h>
#include <core/dsp/SignalsmithPitchShifter.h>
#include <core/mix/VocalMixEngine.h>
#include <core/mix/LevelMatcher.h>
#include <core/dsp/DeEsser.h>
#include <core/space/SpatialEngine.h>
#include <core/morph/VoiceTexture.h>
#include <core/enhance/VocalEnhancer.h>
#include <core/state/MacroMapping.h>
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

// Low-level engine settings: measures the pitch/formant engine alone
// (no Magia, no auto level, no clipper). Macros are tested separately.
vs::EngineSettings pitchParams (float st, float preservation = 100.0f)
{
    vs::EngineSettings p;
    p.semitones = st;
    p.formantPreservation = preservation / 100.0f;
    p.magic01 = 0.0f;
    p.autoLevel = false;
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
        chain.setEngineSettings (pitchParams (st, 0.0f), false);
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
        p.semitones += cents / 100.0f;
        chain.setEngineSettings (p, false);
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
            chain.setEngineSettings (pitchParams (st, 0.0f), false);
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
    chain.setEngineSettings (pitchParams (12.0f), false);
    chain.prepare (48000, 2, 512);
    Signal z ((size_t) 48000 * 3, 0.0f);
    auto out = run (chain, { z, z }, 512);
    const float peak = std::max (vs::peakAbs (out[0].data(), (int) z.size()), vs::peakAbs (out[1].data(), (int) z.size()));
    MEASURE ("peak output on digital silence = %g", peak);
    CHECK (peak < 1.0e-6f);

    // Silence after signal must decay to silence (no stuck tails / noise).
    vs::VocalChain chain2;
    chain2.setEngineSettings (pitchParams (-7.0f), false);
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
            vs::EngineSettings p;
            p.semitones = (float) ((int) (rng() % 4801) - 2400) / 100.0f;
            p.formantPreservation = (float) (rng() % 101) / 100.0f;
            p.formantShiftSemitones = (float) ((int) (rng() % 25) - 12);
            p.brightnessDb = (float) ((int) (rng() % 25) - 12);
            p.magic01 = (float) (rng() % 101) / 100.0f;
            p.autoLevel = (rng() % 2) == 0;
            p.mix01 = (float) (rng() % 101) / 100.0f;
            p.outputGainDb = 0.0f;
            p.safetyClip = clip;
            chain.setEngineSettings (p, (rng() % 13) == 0);
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
    chain.setEngineSettings (pitchParams (5.0f), false);
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
        p.formantOffset = (float) ((pos / 8192) % 7 - 3);
        p.brightnessOffset = 3.0f;
        p.character = (float) ((pos / 3000) % 101);
        p.magic = (float) ((pos / 5000) % 101);
        p.mode = (pos / 7000) % 2 ? vs::VoiceMode::radical : vs::VoiceMode::keepIdentity;
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
        chain.setEngineSettings (pitchParams (5.0f), false);
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
        chain.setEngineSettings (pitchParams (7.0f, 0.0f), false);
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
        p.formantShiftSemitones = formantShift;
        chain.setEngineSettings (p, false);
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
    p.mix01 = 0.0f;
    p.outputGainDb = -6.0f;
    chain.setEngineSettings (p, false);
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
    chain.setEngineSettings (pitchParams (7.0f), false);
    chain.prepare (fs, 1, 256);
    const int toggleAt = (int) fs; // toggle to bypass at 1 s
    auto out = run (chain, { x }, 256, [&] (int pos) {
        chain.setEngineSettings (pitchParams (7.0f), pos >= toggleAt);
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
        chain.setEngineSettings (p, false);
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
        // Second-voice presets contain the untouched original as well; their pitch is checked in
        // the dedicated second-voice tests (a monophonic detector is meaningless on a two-voice mix).
        if (preset.params.totalSemitones() != 0 && preset.params.route == vs::VoiceRoute::transform)
            CHECK_MSG (std::abs (movedCents - preset.params.totalSemitones() * 100) < 20.0, "%s", preset.id.c_str());
    }
}

TEST ("performance: real-time factor (stereo, 48 kHz, 512-sample blocks)")
{
    const double fs = 48000;
    const int seconds = 10;
    auto v = vowel (fs, 200, seconds);
    {
        vs::VocalChain chain;
        chain.setParams (vs::VoiceParams {}, false); // reference macros incl. Magia + auto level
        chain.prepare (fs, 2, 512);
        std::vector<Signal> in { v, v };
        const auto t0 = std::chrono::steady_clock::now();
        auto out = run (chain, in, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        MEASURE ("full chain, reference macros (+7, 65 %%, 70 %%): %.1f%% of one core", 100.0 * secs / seconds);
        CHECK (secs < seconds * 0.5);
    }
    for (auto [st, form] : { std::pair<float, float> { 0, 0 }, { 7, 0 }, { -12, 2 } })
    {
        vs::VocalChain chain;
        auto p = pitchParams (st);
        p.formantShiftSemitones = form;
        chain.setEngineSettings (p, false);
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

// ---------------------------------------------------------------------------
// Macros (Pitch / Carácter / Magia) and the two voice modes
// ---------------------------------------------------------------------------
TEST ("macros: reference defaults and mapping rules")
{
    vs::VoiceParams def;
    CHECK (def.semitones == 7.0f && def.character == 65.0f && def.magic == 70.0f);
    CHECK (def.mode == vs::VoiceMode::keepIdentity);
    CHECK (def.route == vs::VoiceRoute::transform && def.secondGainDb == -9.0f && def.pan == 0.0f && def.offsetMs == 0.0f);
    def.improved = false; // this test documents the legacy (dec340d) mapping rules

    const auto e = vs::mapMacros (def);
    MEASURE ("reference (identity): semis %.2f, formants follow %.1f %%, extra formant %+.2f st, brightness %+.2f dB, magic %.2f",
             e.semitones, (1 - e.formantPreservation) * 100, e.formantShiftSemitones, e.brightnessDb, e.magic01);
    auto rad = def;
    rad.mode = vs::VoiceMode::radical;
    const auto r = vs::mapMacros (rad);
    MEASURE ("reference (radical):  semis %.2f, formants follow %.1f %%, extra formant %+.2f st, brightness %+.2f dB",
             r.semitones, (1 - r.formantPreservation) * 100, r.formantShiftSemitones, r.brightnessDb);

    // Character 0 = timbre untouched in both modes.
    for (auto m : { vs::VoiceMode::keepIdentity, vs::VoiceMode::radical })
    {
        auto p = def;
        p.mode = m;
        p.character = 0;
        const auto z = vs::mapMacros (p);
        CHECK (z.formantPreservation == 1.0f && z.formantShiftSemitones == 0.0f && z.brightnessDb == 0.0f);
    }
    // Radical changes the timbre more than identity, for the same Character.
    CHECK (r.formantPreservation < e.formantPreservation);
    CHECK (r.formantShiftSemitones > e.formantShiftSemitones);
    CHECK (r.brightnessDb > e.brightnessDb);
    // Identity mode keeps formants mostly in place.
    CHECK (e.formantPreservation >= 0.75f);
    // Monotonic in Character; direction follows the pitch sign.
    float prev = -100;
    for (int c = 0; c <= 100; c += 10)
    {
        auto p = rad;
        p.character = (float) c;
        const float shift = vs::mapMacros (p).formantShiftSemitones;
        CHECK (shift >= prev);
        prev = shift;
    }
    auto down = rad;
    down.semitones = -7;
    CHECK (vs::mapMacros (down).formantShiftSemitones < 0 && vs::mapMacros (down).brightnessDb < 0);
    // Continuity through 0 st (no timbre jump when automating pitch across 0).
    float last = 0;
    bool first = true;
    float maxJump = 0;
    for (int i = -200; i <= 200; ++i)
    {
        auto p = rad;
        p.semitones = 0;
        p.fineCents = (float) i * 0.5f;
        const float v = vs::mapMacros (p).formantShiftSemitones;
        if (! first) maxJump = std::max (maxJump, std::abs (v - last));
        last = v;
        first = false;
    }
    MEASURE ("max formant step for 0.5-cent pitch steps across 0 st: %.3f st", maxJump);
    CHECK (maxJump < 0.1f);
}

TEST ("macros: reference settings shift F0 by +7 st in both modes; radical moves timbre further")
{
    const double fs = 48000;
    const auto v = vowel (fs, 200, 3.0, 0.3f, 0.0, 0.0);
    vs::YinPitchDetector yin (fs);
    const float inF0 = yin.medianF0 (v.data(), (int) v.size());
    auto inMid = segment (v, (int) fs, 8192);
    const float cIn = vs::spectralCentroidHz (inMid.data(), (int) inMid.size(), fs);
    float centroid[2] = {};
    for (int m = 0; m < 2; ++m)
    {
        vs::VoiceParams p; // reference: +7, 65 %, 70 %
        p.mode = m ? vs::VoiceMode::radical : vs::VoiceMode::keepIdentity;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        auto out = run (chain, { v }, 512)[0];
        auto mid = segment (out, chain.latencySamples() + (int) fs, (int) fs);
        const float f0 = yin.medianF0 (mid.data(), (int) mid.size());
        const double err = centsBetween (inF0 * std::pow (2.0, 7.0 / 12.0), f0);
        auto cm = segment (out, chain.latencySamples() + (int) fs, 8192);
        centroid[m] = vs::spectralCentroidHz (cm.data(), (int) cm.size(), fs);
        MEASURE ("%s: F0 error %+.2f cents, centroid %.0f Hz (input %.0f Hz), finite=%d",
                 m ? "radical " : "identity", err, centroid[m], cIn, (int) allFinite (out));
        CHECK (std::abs (err) < 15.0);
        CHECK (allFinite (out));
    }
    CHECK (centroid[1] > centroid[0]);
}

TEST ("magia: 0 % is an exact bypass; 70 % compresses dynamics and adds air")
{
    const double fs = 48000;
    // Vowel alternating loud (-6 dBFS) and quiet (-26 dBFS) every 0.5 s.
    auto v = vowel (fs, 180, 4.0, 0.5f, 5.0, 20.0);
    for (size_t i = 0; i < v.size(); ++i)
        if ((i / (size_t) (fs / 2)) % 2 == 1)
            v[i] *= 0.1f;
    auto segRmsRatioDb = [&] (const Signal& x) {
        // loud segment 2 (1.0..1.5 s) vs quiet segment 3 (1.5..2.0 s), skipping 100 ms edges
        const int a = (int) (fs * 1.1), b = (int) (fs * 1.6), len = (int) (fs * 0.3);
        return 20.0f * std::log10 (vs::rms (x.data() + a, len) / vs::rms (x.data() + b, len));
    };

    for (float magic : { 0.0f, 0.7f, 1.0f })
    {
        vs::VocalEnhancer enh;
        enh.setMagic (magic);
        enh.prepare (fs, 1);
        Signal out = v;
        float* ptr = out.data();
        for (size_t pos = 0; pos < out.size(); pos += 512)
        {
            float* p = ptr + pos;
            enh.process (&p, 1, (int) std::min<size_t> (512, out.size() - pos));
        }
        auto mid = segment (out, (int) (fs * 1.1), 8192);
        auto midIn = segment (v, (int) (fs * 1.1), 8192);
        const float cOut = vs::spectralCentroidHz (mid.data(), (int) mid.size(), fs);
        const float cIn = vs::spectralCentroidHz (midIn.data(), (int) midIn.size(), fs);
        MEASURE ("magic %.0f %%: loud/quiet ratio in %.1f dB -> out %.1f dB; centroid %.0f -> %.0f Hz; finite=%d",
                 magic * 100, segRmsRatioDb (v), segRmsRatioDb (out), cIn, cOut, (int) allFinite (out));
        CHECK (allFinite (out));
        if (magic == 0.0f)
            CHECK (out == v); // bit-exact bypass
        else
        {
            CHECK (segRmsRatioDb (out) < segRmsRatioDb (v) - 3.0f); // at least 3 dB less dynamic range
            CHECK (cOut > cIn);                                    // more high-frequency energy
            CHECK (vs::peakAbs (out.data(), (int) out.size()) < 2.0f);
        }
    }
}

TEST ("nivel automatico: compensates level loss at +12 st and never boosts silence")
{
    const double fs = 48000;
    Signal x ((size_t) (fs * 6), 0.0f);
    const auto v = vowel (fs, 180, 3.0, 0.3f);
    std::copy (v.begin(), v.end(), x.begin()); // 3 s voice, 3 s digital silence
    const float inRms = vs::rms (x.data() + (int) fs, (int) fs);
    for (bool autoLevel : { false, true })
    {
        auto e = pitchParams (12.0f, 100.0f);
        e.autoLevel = autoLevel;
        vs::VocalChain chain;
        chain.setEngineSettings (e, false);
        chain.prepare (fs, 1, 512);
        auto out = run (chain, { x }, 512)[0];
        const int lat = chain.latencySamples();
        const float outRms = vs::rms (out.data() + lat + (int) (fs * 1.5), (int) fs);
        auto tail = segment (out, lat + (int) (fs * 4.5), (int) fs);
        const float tailPeak = vs::peakAbs (tail.data(), (int) tail.size());
        MEASURE ("autoLevel=%d: output level vs input %+.2f dB, gain %.1f dB, peak in silence %g",
                 (int) autoLevel, 20 * std::log10 (outRms / inRms), chain.autoLevelGainDb(), tailPeak);
        if (autoLevel)
            CHECK (std::abs (20 * std::log10 (outRms / inRms)) < 1.5f);
        CHECK (tailPeak < 1.0e-5f);
    }
}

TEST ("macros: switching mode and moving macros during playback stays finite and bounded")
{
    const double fs = 48000;
    auto v = vowel (fs, 210, 4.0);
    vs::VocalChain chain;
    chain.prepare (fs, 2, 256);
    std::mt19937 rng (11);
    auto out = run (chain, { v, v }, 256, [&] (int pos) {
        vs::VoiceParams p;
        p.mode = ((pos / 9600) % 2) ? vs::VoiceMode::radical : vs::VoiceMode::keepIdentity;
        p.semitones = (float) ((int) (rng() % 49) - 24);
        p.character = (float) (rng() % 101);
        p.magic = (float) (rng() % 101);
        p.safetyClip = false;
        chain.setParams (p, false);
    });
    const float peak = std::max (vs::peakAbs (out[0].data(), (int) v.size()), vs::peakAbs (out[1].data(), (int) v.size()));
    MEASURE ("random macros every 256 samples, mode toggled every 200 ms: peak %.3f (input peak %.3f)",
             peak, vs::peakAbs (v.data(), (int) v.size()));
    CHECK (allFinite (out[0]) && allFinite (out[1]));
    CHECK (peak < 4.0f);
}

// ===========================================================================
// PHASE 3: second voice, robust auto level, consonant preservation
// ===========================================================================
#include "GoldenLegacy.h"

namespace
{
// Magnitude (linear amplitude) of a sinusoid of frequency f in x[start, start+len), Goertzel/DFT bin.
double toneAmp (const Signal& x, double fs, double f, int start, int len)
{
    start = std::clamp (start, 0, (int) x.size());
    len = std::clamp (len, 0, (int) x.size() - start);
    double re = 0, im = 0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * vstest::kPi * i / std::max (1, len - 1)); // Hann
        const double ph = 2 * vstest::kPi * f * i / fs;
        re += w * x[(size_t) (start + i)] * std::cos (ph);
        im += w * x[(size_t) (start + i)] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / (0.5 * std::max (1, len));
}
double dbOf (double a) { return 20.0 * std::log10 (a + 1e-12); }

Signal sine (double fs, double f, double seconds, float amp)
{
    Signal s ((size_t) (fs * seconds));
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = amp * (float) std::sin (2 * vstest::kPi * f * (double) i / fs);
    return s;
}

double rmsDb (const Signal& x, int start, int len)
{
    start = std::clamp (start, 0, (int) x.size());
    len = std::clamp (len, 1, std::max (1, (int) x.size() - start));
    double s = 0;
    for (int i = 0; i < len; ++i) s += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
    return 10 * std::log10 (s / len + 1e-12);
}

vs::VoiceParams secondVoiceParams (float st = 7.0f)
{
    vs::VoiceParams p;
    p.route = vs::VoiceRoute::secondVoice;
    p.semitones = st;
    p.autoLevel = true;
    return p;
}

Signal speechLike (double fs)
{
    // 0.4 s vowel, 0.2 s pause, 0.1 s fricative noise, 0.3 s vowel (same phrase as the golden reference)
    Signal x ((size_t) (fs * 1.2), 0.f);
    auto v1 = vowel (fs, 190, 0.4, 0.3f, 5, 25);
    auto v2 = vowel (fs, 230, 0.3, 0.25f, 5, 25);
    auto n = whiteNoise ((int) (fs * 0.1), 0.05f, 3);
    std::copy (v1.begin(), v1.end(), x.begin());
    std::copy (n.begin(), n.end(), x.begin() + (size_t) (fs * 0.6));
    std::copy (v2.begin(), v2.end(), x.begin() + (size_t) (fs * 0.7));
    return x;
}
} // namespace

TEST ("regression: route=transform, improved=false is bit-for-bit the previous (dec340d) behaviour")
{
    const double fs = 48000;
    const auto x = speechLike (fs);
    for (int mode = 0; mode < 2; ++mode)
    {
        vs::VoiceParams p;
        p.mode = mode ? vs::VoiceMode::radical : vs::VoiceMode::keepIdentity;
        if (mode) { p.mixPercent = 70; p.outputGainDb = -3; }
        p.improved = false;
        p.route = vs::VoiceRoute::transform;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        const auto o = run (chain, { x }, 512)[0];
        const float* ref = mode ? golden::golden1 : golden::golden0;
        const size_t nref = mode ? sizeof (golden::golden1) / sizeof (float) : sizeof (golden::golden0) / sizeof (float);
        const int w = (int) (fs * 0.02);
        double maxDiff = 0;
        size_t k = 0;
        for (size_t i = 0; i + (size_t) w <= o.size() && k < nref; i += (size_t) w, ++k)
            maxDiff = std::max (maxDiff, std::abs (rmsDb (o, (int) i, w) - (double) ref[k]));
        MEASURE ("mode %d: %zu windows compared, max |RMS difference| = %.3f dB", mode, k, maxDiff);
        CHECK (k == nref);
        CHECK_MSG (maxDiff < 0.05, "mode %d differs from the reference by %.3f dB", mode, maxDiff);
    }
}

TEST ("second voice: both routes present, original untransposed at 0 dB, second voice +7 st at -9 dB")
{
    const double fs = 48000;
    const double f0 = 300.0, f7 = f0 * std::pow(2.0, 7.0 / 12.0);
    const auto x = sine (fs, f0, 3.0, 0.2f);
    auto p = secondVoiceParams();
    p.character = 0; p.magic = 0; // isolate routing/pitch from timbre and Magia
    vs::VocalChain chain;
    chain.setParams (p, false);
    chain.prepare (fs, 1, 512);
    const auto o = run (chain, { x }, 512)[0];
    const int s0 = (int) (fs * 1.5), len = (int) (fs * 1.0);
    const double aOrig = dbOf (toneAmp (o, fs, f0, s0, len)) - dbOf (0.2);
    const double aSecond = dbOf (toneAmp (o, fs, f7, s0, len)) - dbOf (0.2);
    const double aNoise = dbOf (toneAmp (o, fs, f0 * 1.19, s0, len)) - dbOf (0.2); // unrelated bin
    MEASURE ("original %.2f dB (target 0), second voice %.2f dB (target -9), unrelated bin %.1f dB", aOrig, aSecond, aNoise);
    CHECK (std::abs (aOrig) < 0.5);
    CHECK (std::abs (aSecond + 9.0) < 2.0);
    CHECK (aNoise < -40.0);
    // Second voice is quieter than the original, never louder.
    CHECK (aSecond < aOrig - 5.0);
}

TEST ("second voice: gains are independent")
{
    const double fs = 48000;
    const double f0 = 300.0, f7 = f0 * std::pow(2.0, 7.0 / 12.0);
    const auto x = sine (fs, f0, 3.0, 0.2f);
    double orig[2], second[2];
    int idx = 0;
    for (float g : { -9.0f, -18.0f })
    {
        auto p = secondVoiceParams();
        p.character = 0; p.magic = 0; p.secondGainDb = g;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        const auto o = run (chain, { x }, 512)[0];
        orig[idx] = dbOf (toneAmp (o, fs, f0, (int) (fs * 1.5), (int) fs));
        second[idx] = dbOf (toneAmp (o, fs, f7, (int) (fs * 1.5), (int) fs));
        ++idx;
    }
    MEASURE ("second gain -9 -> -18 dB: original changed %+.2f dB, second voice changed %+.2f dB", orig[1] - orig[0], second[1] - second[0]);
    CHECK (std::abs (orig[1] - orig[0]) < 0.3);
    CHECK (std::abs ((second[1] - second[0]) + 9.0) < 0.6);
}

TEST ("second voice: alignment of both routes, artistic offset, reported latency never changes")
{
    const double fs = 48000;
    // Broadband but periodic-free reference: noise bursts. Pitch 0 so route B ~ route A in time.
    const auto x = whiteNoise ((int) (fs * 3.0), 0.1f, 11);
    int latency = -1;
    for (float offsetMs : { 0.0f, 10.0f, 40.0f })
    {
        auto p = secondVoiceParams (0.0f);
        p.character = 0; p.magic = 0; p.autoLevel = false; p.offsetMs = offsetMs; p.secondGainDb = -6;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        if (latency < 0) latency = chain.latencySamples();
        CHECK (chain.latencySamples() == latency); // offset must never leak into the reported latency
        const auto o = run (chain, { x }, 512)[0];
        // residual = out - delayed original = the second voice alone
        const int L = chain.latencySamples();
        Signal res (o.size(), 0.0f);
        for (size_t i = (size_t) L; i < o.size(); ++i)
            res[i] = o[i] - x[i - (size_t) L];
        // find the lag (relative to the delayed original) where the residual correlates the most
        const int a = (int) (fs * 1.0), n = (int) (fs * 1.5);
        int bestLag = 0; double best = -1;
        const int maxLag = (int) (fs * 0.06);
        for (int lag = -200; lag <= maxLag; ++lag)
        {
            double c = 0, e1 = 0, e2 = 0;
            for (int i = a; i < a + n; ++i)
            {
                const double r = res[(size_t) i];
                const double d = x[(size_t) (i - L - lag)];
                c += r * d; e1 += r * r; e2 += d * d;
            }
            const double nc = c / std::sqrt (e1 * e2 + 1e-30);
            if (nc > best) { best = nc; bestLag = lag; }
        }
        const double expected = offsetMs * 0.001 * fs;
        MEASURE ("offset %.0f ms: second voice lags the original by %d samples (expected %.0f), corr %.3f, latency %d",
                 offsetMs, bestLag, expected, best, latency);
        CHECK_MSG (std::abs (bestLag - expected) <= 3.0, "offset %.0f ms measured %d samples", offsetMs, bestLag);
        CHECK (best > 0.8);
    }
}

TEST ("second voice: silence stays silent, no noise and no gain leak")
{
    const double fs = 48000;
    for (auto route : { vs::VoiceRoute::transform, vs::VoiceRoute::secondVoice })
    {
        auto p = secondVoiceParams();
        p.route = route;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 2, 512);
        Signal z ((size_t) (fs * 2), 0.0f);
        const auto o = run (chain, { z, z }, 512);
        const float pk = std::max (vs::peakAbs (o[0].data(), (int) z.size()), vs::peakAbs (o[1].data(), (int) z.size()));
        MEASURE ("route %d: silence in -> peak %.3g, auto level %.2f dB", (int) route, pk, chain.autoLevelGainDb());
        CHECK (pk == 0.0f);
        CHECK (std::abs (chain.autoLevelGainDb()) < 0.01f);
    }
}

TEST ("second voice: no clipping with the original at -6 dBFS (summing is gain-staged, the clipper stays idle)")
{
    const double fs = 48000;
    auto v = vowel (fs, 200, 3.0, 0.5f); // -6 dBFS peak
    for (float st : { 7.0f, 12.0f, 19.0f })
    {
        auto p = secondVoiceParams (st);
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        const auto o = run (chain, { v }, 512)[0];
        const float pk = vs::peakAbs (o.data(), (int) o.size());
        MEASURE ("+%.0f st at -9 dB: output peak %.3f (%.1f dBFS), samples altered by the clipper: %ld",
                 st, pk, 20 * std::log10 (pk), chain.clipAlteredSamples());
        CHECK (chain.clipAlteredSamples() == 0);
        CHECK (pk < 0.891f);
    }
}

TEST ("second voice: mono and stereo, pan is a balance (hard right silences the second voice on the left)")
{
    const double fs = 48000;
    const double f0 = 300.0, f7 = f0 * std::pow(2.0, 7.0 / 12.0);
    const auto x = sine (fs, f0, 2.5, 0.2f);
    auto p = secondVoiceParams();
    p.character = 0; p.magic = 0; p.pan = 100;
    vs::VocalChain chain;
    chain.setParams (p, false);
    chain.prepare (fs, 2, 512);
    const auto o = run (chain, { x, x }, 512);
    const int s0 = (int) (fs * 1.2), len = (int) fs;
    const double l7 = dbOf (toneAmp (o[0], fs, f7, s0, len)), r7 = dbOf (toneAmp (o[1], fs, f7, s0, len));
    const double l0 = dbOf (toneAmp (o[0], fs, f0, s0, len)), r0 = dbOf (toneAmp (o[1], fs, f0, s0, len));
    MEASURE ("pan +100: second voice L %.1f dB / R %.1f dB; original L %.1f dB / R %.1f dB", l7, r7, l0, r0);
    CHECK (l7 < r7 - 40.0);
    CHECK (std::abs (l0 - r0) < 0.3); // the original is never panned
    // mono works too
    vs::VocalChain m;
    p.pan = 0;
    m.setParams (p, false);
    m.prepare (fs, 1, 512);
    const auto om = run (m, { x }, 512)[0];
    CHECK (allFinite (om));
    CHECK (std::abs (dbOf (toneAmp (om, fs, f7, s0, len)) - r7) < 4.5); // centre vs hard-right of a balance: <= +3 dB, plus tolerance
}

TEST ("second voice: variable host buffers give the same sound")
{
    const double fs = 48000;
    const auto x = speechLike (fs);
    Signal ref;
    for (int pass = 0; pass < 4; ++pass)
    {
        vs::VocalChain chain;
        chain.setParams (secondVoiceParams(), false);
        chain.prepare (fs, 1, 1024);
        Signal o;
        if (pass == 0) o = run (chain, { x }, 512)[0];
        else if (pass == 1) o = run (chain, { x }, 64)[0];
        else if (pass == 2) o = run (chain, { x }, 1024)[0];
        else o = run (chain, { x }, 700, nullptr, 5)[0];
        if (pass == 0) { ref = o; continue; }
        double d = 0, e = 0;
        for (size_t i = 0; i < o.size(); ++i) { d += (double) (o[i] - ref[i]) * (o[i] - ref[i]); e += (double) ref[i] * ref[i]; }
        const double db = 10 * std::log10 ((d + 1e-20) / (e + 1e-20));
        MEASURE ("pass %d vs 512-sample blocks: difference %.1f dB", pass, db);
        CHECK_MSG (db < -60.0, "pass %d differs by %.1f dB", pass, db);
    }
}

TEST ("second voice: sample rates 44.1 / 48 / 96 kHz keep both voices and a constant ~150 ms latency")
{
    // A vowel (harmonic, voice-like) is used on purpose: with a single pure sine the formant
    // compensation has no envelope to follow and the level of the shifted sine varies by up to
    // ~17 dB with frequency at 44.1 kHz (observed in a probe; not a voice case).
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const auto x = vowel (fs, 200.0, 3.0, 0.3f);
        auto p = secondVoiceParams();
        p.character = 0; p.magic = 0;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        const auto o = run (chain, { x }, 512)[0];
        const int L = chain.latencySamples();
        Signal res (o.size(), 0.0f);
        for (size_t i = (size_t) L; i < o.size(); ++i) res[i] = o[i] - x[i - (size_t) L];
        const int s0 = (int) (fs * 1.5), len = (int) fs;
        // original untouched
        double dOrig = 0, eOrig = 0;
        const Signal sil (1, 0.f); (void) sil;
        const double wetDb = rmsDb (res, s0, len) - rmsDb (x, s0 - L, len);
        const double ms = 1000.0 * L / fs;
        // F0 of the second voice alone
        vs::YinPitchDetector yin (fs);
        auto seg = segment (res, s0, len);
        const float f = yin.medianF0 (seg.data(), (int) seg.size());
        const double cents = f > 0 ? centsBetween (200.0, f) : 0.0;
        (void) dOrig; (void) eOrig;
        MEASURE ("%.0f Hz: second voice %.2f dB relative to the original (target ~-9), its F0 moved %+.0f cents (target +700), latency %.1f ms",
                 fs, wetDb, cents, ms);
        CHECK (std::abs (wetDb + 9.0) < 3.0);
        CHECK (std::abs (cents - 700.0) < 40.0);
        CHECK (ms > 120.0 && ms < 180.0);
    }
}

TEST ("second voice: switching route, bypass and macros during playback is click-free and bounded")
{
    const double fs = 48000;
    const auto x = sine (fs, 300.0, 4.0, 0.2f);
    const float inStep = 2 * (float) vstest::kPi * 300.0f / (float) fs * 0.2f;
    vs::VocalChain chain;
    chain.setParams (vs::VoiceParams {}, false);
    chain.prepare (fs, 1, 256);
    vs::VoiceParams p;
    p.character = 0; p.magic = 0;
    long altered = 0;
    const auto o = run (chain, { x }, 256, [&] (int pos) {
        p.route = ((pos / 12000) % 2) ? vs::VoiceRoute::secondVoice : vs::VoiceRoute::transform;
        const bool byp = ((pos / 30000) % 3) == 2;
        chain.setParams (p, byp);
    })[0];
    float maxStep = 0;
    for (size_t i = 1; i < o.size(); ++i) maxStep = std::max (maxStep, std::abs (o[i] - o[i - 1]));
    altered = chain.clipAlteredSamples();
    MEASURE ("route toggled every 250 ms + bypass: max sample step %.4f (input sine step %.4f), clip-altered samples %ld", maxStep, inStep, altered);
    CHECK (allFinite (o));
    CHECK (maxStep < 6 * inStep);
}

TEST ("second voice: automating Pitch / Carácter / Magia stays finite and bounded")
{
    const double fs = 48000;
    auto v = vowel (fs, 200, 3.0);
    std::mt19937 rng (7);
    vs::VocalChain chain;
    chain.setParams (secondVoiceParams(), false);
    chain.prepare (fs, 2, 512);
    const auto o = run (chain, { v, v }, 256, [&] (int) {
        auto p = secondVoiceParams ((float) ((int) (rng() % 49) - 24));
        p.character = (float) (rng() % 101);
        p.magic = (float) (rng() % 101);
        p.mode = (rng() & 1) ? vs::VoiceMode::radical : vs::VoiceMode::keepIdentity;
        chain.setParams (p, false);
    });
    const float pk = std::max (vs::peakAbs (o[0].data(), (int) v.size()), vs::peakAbs (o[1].data(), (int) v.size()));
    MEASURE ("random macros every 256 samples in second-voice mode: peak %.3f", pk);
    CHECK (allFinite (o[0]) && allFinite (o[1]));
    CHECK (pk < 2.0f);
}

TEST ("second voice: no heap allocation or non-finite output in process() while changing route")
{
    const double fs = 48000;
    auto v = vowel (fs, 200, 1.0);
    vs::VocalChain chain;
    chain.setParams (secondVoiceParams(), false);
    chain.prepare (fs, 2, 512);
    Signal a = v, b = v;
    float* ptrs[2] = { a.data(), b.data() };
    g_allocCount = 0;
    g_trackAllocs = true;
    for (int blk = 0; blk < 60; ++blk)
    {
        auto p = secondVoiceParams ((float) (blk % 13));
        p.route = (blk % 4 < 2) ? vs::VoiceRoute::secondVoice : vs::VoiceRoute::transform;
        p.offsetMs = (float) (blk % 50);
        p.pan = (float) ((blk % 21) * 10 - 100);
        chain.setParams (p, false);
        chain.process (ptrs, 2, 512);
    }
    g_trackAllocs = false;
    MEASURE ("allocations during 60 blocks: %ld", g_allocCount.load());
    CHECK (g_allocCount.load() == 0);
}

TEST ("performance: second voice (stereo, 48 kHz, 512-sample blocks) CPU cost")
{
    const double fs = 48000;
    const int seconds = 10;
    auto v = vowel (fs, 200, seconds);
    vs::VocalChain chain;
    chain.setParams (secondVoiceParams(), false);
    chain.prepare (fs, 2, 512);
    std::vector<Signal> in { v, v };
    const auto t0 = std::chrono::steady_clock::now();
    run (chain, in, 512);
    const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    MEASURE ("second voice +7: %.3f s CPU for %d s audio -> %.1f %% of one core (this machine)", cpu, seconds, 100.0 * cpu / seconds);
    CHECK (cpu / seconds < 0.25);
}

TEST ("level: robust matcher keeps the gain still in pauses and consonants (legacy dilutes the measurement)")
{
    const double fs = 48000;
    // Reference = vowel, 1 s pause, vowel again; processed branch is 6 dB quieter.
    const int n = (int) (fs * 4.0);
    Signal dry ((size_t) n, 0.f), wet ((size_t) n, 0.f);
    auto v = vowel (fs, 200, 1.2, 0.3f);
    std::copy (v.begin(), v.end(), dry.begin());
    std::copy (v.begin(), v.end(), dry.begin() + (size_t) (fs * 2.2));
    for (size_t i = 0; i < dry.size(); ++i) wet[i] = dry[i] * 0.5f;
    // low-level hiss in the pause on both branches (below the -50 dBFS gate)
    auto hiss = whiteNoise ((int) (fs * 1.0), 0.001f, 5);
    for (size_t i = 0; i < hiss.size(); ++i) { dry[(size_t) (fs * 1.2) + i] += hiss[i]; wet[(size_t) (fs * 1.2) + i] += hiss[i]; }
    for (bool robust : { false, true })
    {
        vs::LevelMatcher lm;
        lm.prepare (fs);
        lm.setRobust (robust);
        Signal w = wet;
        double gAfterVowel = 0, gEndPause = 0, gMaxDriftInPause = 0;
        const int blk = 128;
        for (int pos = 0; pos + blk <= n; pos += blk)
        {
            const float* d[1] = { dry.data() + pos };
            float* ww[1] = { w.data() + pos };
            lm.process (d, ww, 1, blk);
            if (pos + blk == (int) (fs * 1.2) / blk * blk) gAfterVowel = lm.currentGainDb();
            if (pos > (int) (fs * 1.45) && pos < (int) (fs * 2.15))
                gMaxDriftInPause = std::max (gMaxDriftInPause, std::abs ((double) lm.currentGainDb() - gAfterVowel));
            if (pos + blk >= (int) (fs * 2.15) && gEndPause == 0) gEndPause = lm.currentGainDb();
        }
        MEASURE ("%s: gain after first vowel %.2f dB (ideal +6.02), max drift inside the pause %.2f dB, gain entering vowel 2: %.2f dB",
                 robust ? "robust" : "legacy", gAfterVowel, gMaxDriftInPause, gEndPause);
        if (robust)
        {
            CHECK (std::abs (gAfterVowel - 6.02) < 1.0);
            CHECK (gMaxDriftInPause < 0.2);
        }
    }
}

TEST ("level: robust matcher never moves the gain on a short consonant dip between vowels")
{
    const double fs = 48000;
    // vowel (0.8 s) -> 60 ms fricative 20 dB quieter -> vowel (0.8 s); wet is 4 dB down
    Signal dry, wet;
    auto v = vowel (fs, 200, 0.8, 0.3f);
    auto f = whiteNoise ((int) (fs * 0.06), 0.02f, 4);
    dry.insert (dry.end(), v.begin(), v.end());
    const size_t fStart = dry.size();
    dry.insert (dry.end(), f.begin(), f.end());
    dry.insert (dry.end(), v.begin(), v.end());
    wet = dry;
    for (auto& s : wet) s *= 0.63f;
    vs::LevelMatcher lm;
    lm.prepare (fs);
    lm.setRobust (true);
    double gBefore = 0, gMin = 1e9, gMax = -1e9;
    const int blk = 64;
    for (int pos = 0; pos + blk <= (int) dry.size(); pos += blk)
    {
        const float* d[1] = { dry.data() + pos };
        float* w[1] = { wet.data() + pos };
        lm.process (d, w, 1, blk);
        if (pos + blk <= (int) fStart) gBefore = lm.currentGainDb();
        if (pos >= (int) fStart && pos < (int) (fStart + f.size()) + 2000)
        {
            gMin = std::min (gMin, (double) lm.currentGainDb());
            gMax = std::max (gMax, (double) lm.currentGainDb());
        }
    }
    MEASURE ("gain before the consonant %.2f dB (ideal +4.0); during/after: min %.2f max %.2f", gBefore, gMin, gMax);
    CHECK (std::abs (gBefore - 4.0) < 1.0);
    CHECK (gMax - gMin < 0.3);
}

TEST ("consonants: fricative noise keeps its original spectral position after +7 st when improved")
{
    const double fs = 48000;
    // 6 kHz-centred noise burst: band-limited by differencing white noise twice (high-pass tilt) then masked
    auto nz = whiteNoise ((int) (fs * 1.5), 0.2f, 9);
    for (size_t i = nz.size() - 1; i > 1; --i) nz[i] = nz[i] - 1.6f * nz[i - 1] + 0.8f * nz[i - 2]; // emphasises highs
    auto centroid = [&] (const Signal& s, int start, int len) {
        double num = 0, den = 0;
        for (double f = 500; f < 20000; f += 250)
        {
            const double a = toneAmp (s, fs, f, start, len);
            num += f * a * a; den += a * a;
        }
        return num / (den + 1e-30);
    };
    double cIn = 0, cLegacy = 0, cNew = 0;
    for (int improved = 0; improved < 2; ++improved)
    {
        vs::VoiceParams p;
        p.improved = improved != 0;
        p.autoLevel = false; p.magic = 0; p.character = 0;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        const auto o = run (chain, { nz }, 512)[0];
        const int L = chain.latencySamples();
        const int s0 = (int) (fs * 0.6) + L - L; // analyse the same stretch of the processed output
        if (improved == 0) { cIn = centroid (nz, s0 - L, 24000); cLegacy = centroid (o, s0, 24000); }
        else cNew = centroid (o, s0, 24000);
    }
    MEASURE ("spectral centroid: original %.0f Hz, legacy +7 st %.0f Hz, improved +7 st %.0f Hz", cIn, cLegacy, cNew);
    CHECK (cLegacy > cIn * 1.05);                          // the old engine does shift the noise upwards
    CHECK (std::abs (cNew - cIn) < std::abs (cLegacy - cIn)); // the improved path is closer to the original
}


TEST ("de-esser: attenuates sibilance, leaves vowels alone, Magia 0 % is an exact bypass")
{
    const double fs = 48000;
    auto sib = whiteNoise ((int) (fs * 1.0), 0.15f, 21);
    for (size_t i = sib.size() - 1; i > 1; --i) sib[i] = sib[i] - 1.6f * sib[i - 1] + 0.8f * sib[i - 2]; // bright noise
    const auto vow = vowel (fs, 200, 1.0, 0.3f);
    auto runDe = [&] (const Signal& x, float amount, float* maxGr = nullptr) {
        vs::DeEsser d;
        d.prepare (fs, 1);
        d.setAmount (amount);
        d.prepare (fs, 1); // snap the amount smoother
        Signal y = x;
        float gr = 0;
        for (size_t pos = 0; pos < y.size(); pos += 256)
        {
            float* p = y.data() + pos;
            d.process (&p, 1, (int) std::min<size_t> (256, y.size() - pos));
            gr = std::max (gr, d.lastReductionDb());
        }
        if (maxGr) *maxGr = gr;
        return y;
    };
    float grSib = 0, grVow = 0;
    const auto s0 = runDe (sib, 0.0f), s70 = runDe (sib, 0.7f, &grSib), v70 = runDe (vow, 0.7f, &grVow);
    CHECK (s0 == sib); // exact bypass at 0 %
    const double sibDelta = rmsDb (s70, 24000, 20000) - rmsDb (sib, 24000, 20000);
    const double vowDelta = rmsDb (v70, 24000, 20000) - rmsDb (vow, 24000, 20000);
    MEASURE ("sibilant noise: %.1f dB (max reduction %.1f dB); vowel: %.2f dB (max reduction %.2f dB)", sibDelta, grSib, vowDelta, grVow);
    CHECK (sibDelta < -3.0);
    CHECK (std::abs (vowDelta) < 0.3);
}

TEST ("diagnosis: pitch engine measurements at +3 / +7 / +12 / +19 and -5 / -12 st (vowel with vibrato)")
{
    // Reproducible measurements of the real engine, used to decide what is worth changing.
    // Nothing here is a perceptual claim.
    const double fs = 48000;
    const auto x = vowel (fs, 200.0, 3.0, 0.3f, 5.0, 30.0);
    vs::YinPitchDetector yin (fs);
    auto track = [&] (const Signal& s, int from) {
        std::vector<double> cents;
        const int win = 4096, hop = 1200;
        for (int i = from; i + win < (int) s.size(); i += hop)
        {
            const auto est = yin.analyse (s.data() + i, win);
            if (est.frequencyHz > 0 && est.aperiodicity < 0.3f) cents.push_back (1200.0 * std::log2 (est.frequencyHz / 200.0));
        }
        return cents;
    };
    auto spread = [] (const std::vector<double>& c) {
        if (c.size() < 2) return 0.0;
        double m = 0; for (double v : c) m += v; m /= (double) c.size();
        double s = 0; for (double v : c) s += (v - m) * (v - m);
        return std::sqrt (s / (double) c.size());
    };
    const double inSpread = spread (track (x, 14400));
    for (float st : { 3.0f, 7.0f, 12.0f, 19.0f, -5.0f, -12.0f })
    {
        vs::VoiceParams p;
        p.semitones = st; p.character = 0; p.magic = 0; p.autoLevel = false; p.improved = true;
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 1, 512);
        auto o = run (chain, { x }, 512)[0];
        const int L = chain.latencySamples();
        const auto tr = track (o, 14400 + L);
        double mean = 0; for (double c : tr) mean += c; mean /= std::max<size_t> (1, tr.size());
        const double sp = spread (tr);
        // level and bandwidth
        const double lvl = rmsDb (o, 14400 + L, 48000) - rmsDb (x, 14400, 48000);
        MEASURE ("%+5.0f st: mean F0 error %+6.1f cents, vibrato spread %5.1f cents (input %.1f => retained %.0f %%), level change %+5.2f dB, %zu voiced frames",
                 st, mean - st * 100.0, sp, inSpread, 100.0 * sp / std::max (1e-9, inSpread), lvl, tr.size());
        CHECK (tr.size() > 10);
        CHECK (std::abs (mean - st * 100.0) < 40.0);
    }
}


// ===========================================================================
// PHASE 4: fader, Urban Spatial Engine, Magia / Caracter redesign
// ===========================================================================
namespace
{
vs::VoiceParams urbanParams()
{
    return vs::findFactoryPreset ("urban_spatial")->params;
}

double corrLR (const Signal& l, const Signal& r, int a, int n)
{
    double lr = 0, ll = 0, rr = 0;
    for (int i = a; i < a + n; ++i) { lr += (double) l[(size_t) i] * r[(size_t) i]; ll += (double) l[(size_t) i] * l[(size_t) i]; rr += (double) r[(size_t) i] * r[(size_t) i]; }
    return lr / std::sqrt (ll * rr + 1e-30);
}

// Renders the second voice alone (output minus the delayed original) for a stereo chain.
std::vector<Signal> secondVoiceOnly (const vs::VoiceParams& p, const Signal& x, double fs, int block = 512)
{
    vs::VocalChain chain;
    chain.setParams (p, false);
    chain.prepare (fs, 2, block);
    auto o = run (chain, { x, x }, block);
    const int L = chain.latencySamples();
    for (auto& ch : o)
        for (size_t i = ch.size(); i-- > 0;)
            ch[i] = i >= (size_t) L ? ch[i] - x[i - (size_t) L] : 0.0f;
    return o;
}
}

TEST ("fader: -12 dB, 0 dB, +6 dB and silence set the second voice level exactly; original untouched")
{
    const double fs = 48000;
    const auto x = vowel (fs, 200, 3.0, 0.3f);
    double ref = 0;
    for (float g : { -12.0f, 0.0f, 6.0f, -60.0f })
    {
        auto p = urbanParams();
        p.spatial = false; p.secondGainDb = g;
        const auto o = secondVoiceOnly (p, x, fs);
        const double lvl = rmsDb (o[0], (int) (fs * 1.5), (int) fs);
        if (g == -12.0f) ref = lvl;
        MEASURE ("fader %+5.1f dB: second voice %.2f dB RMS (%+.2f re the -12 dB setting)", g, lvl, lvl - ref);
        if (g == 0.0f) CHECK (std::abs ((lvl - ref) - 12.0) < 0.1);
        if (g == 6.0f) CHECK (std::abs ((lvl - ref) - 18.0) < 0.1);
        if (g == -60.0f) { CHECK (lvl < -100.0); CHECK (vs::peakAbs (o[0].data(), (int) o[0].size()) < 1e-6f); }
    }
}

TEST ("fader: automation is click-free (jumps from -12 to -36 to 0 dB to silence mid-note)")
{
    const double fs = 48000;
    const auto x = sine (fs, 300.0, 4.0, 0.3f);
    vs::VocalChain chain;
    auto p = urbanParams(); p.magic = 0; p.character = 0;
    chain.setParams (p, false);
    chain.prepare (fs, 2, 256);
    const float gains[] = { -12, -36, 0, -60, -12 };
    const auto o = run (chain, { x, x }, 256, [&] (int pos) {
        p.secondGainDb = gains[std::min (4, pos / 40000)];
        chain.setParams (p, false);
    });
    // The original (0 dB) dominates: its own max step is 2*pi*300/48000*0.3 = 0.0118; the fader may add a little.
    float maxStep = 0;
    for (size_t i = 1; i < o[0].size(); ++i) maxStep = std::max (maxStep, std::abs (o[0][i] - o[0][i - 1]));
    MEASURE ("max sample step during fader jumps: %.4f (original alone: 0.0118)", maxStep);
    CHECK (allFinite (o[0]) && allFinite (o[1]));
    CHECK (maxStep < 0.05f);
}

TEST ("space: Magia 0 % means no spatial processing at all (bit-identical to spatial off)")
{
    const double fs = 48000;
    const auto x = vowel (fs, 200, 2.5);
    auto on = urbanParams(); on.magic = 0; on.character = 0;
    auto off = on; off.spatial = false;
    auto render = [&] (const vs::VoiceParams& p) {
        vs::VocalChain c; c.setParams (p, false); c.prepare (fs, 2, 512);
        return run (c, { x, x }, 512);
    };
    CHECK (render (on) == render (off));
    // and the pitch still works at Magia 0: second voice F0 = +7 st
    const auto sv = secondVoiceOnly (on, x, fs);
    vs::YinPitchDetector yin (fs);
    auto seg = segment (sv[0], (int) (fs * 1.0), (int) fs);
    const float f = yin.medianF0 (seg.data(), (int) seg.size());
    MEASURE ("Magia 0 %%: second voice F0 moved %+.0f cents (target +700)", centsBetween (200.0, f));
    CHECK (std::abs (centsBetween (200.0, f) - 700.0) < 40.0);
}

TEST ("space: Magia 70 % adds real spatial processing (width, reverb tail), Magia 100 % more; mono sum preserved")
{
    const double fs = 48000;
    // a short phrase followed by silence so the reverb tail can be measured
    Signal x ((size_t) (fs * 4.0), 0.0f);
    const auto v = vowel (fs, 200, 1.0, 0.3f);
    std::copy (v.begin(), v.end(), x.begin());
    double corr[3], tail[3];
    int idx = 0;
    for (float m : { 0.0f, 70.0f, 100.0f })
    {
        auto p = urbanParams(); p.magic = m; p.character = 0; p.secondGainDb = 0;
        const auto sv = secondVoiceOnly (p, x, fs);
        corr[idx] = corrLR (sv[0], sv[1], (int) (fs * 0.5), (int) (fs * 0.9));
        // second voice energy 0.5 s after the end of the delayed phrase (phrase ends at 1.0 s + 150 ms latency)
        tail[idx] = rmsDb (sv[0], (int) (fs * 1.45), (int) (fs * 0.4));
        MEASURE ("Magia %3.0f %%: L/R correlation %.3f, tail 0.3-0.7 s after the phrase: %.1f dB", m, corr[idx], tail[idx]);
        ++idx;
    }
    CHECK (corr[0] > 0.999);               // no spatial: identical channels
    CHECK (corr[1] < 0.97 && corr[1] > 0.3); // wide but not out of phase
    CHECK (corr[2] < corr[1]);
    CHECK (tail[0] < -100.0);              // nothing after the phrase without space
    CHECK (tail[1] > -75.0);               // a real, audible tail
    CHECK (tail[2] > tail[1]);

    // mono compatibility: L+R keeps the level of the non-spatial render within +-3 dB
    auto p0 = urbanParams(); p0.magic = 70; p0.character = 0; p0.secondGainDb = 0;
    auto pN = p0; pN.spatial = false;
    const auto a = secondVoiceOnly (p0, x, fs), b = secondVoiceOnly (pN, x, fs);
    Signal ma (x.size()), mb (x.size());
    for (size_t i = 0; i < x.size(); ++i) { ma[i] = 0.5f * (a[0][i] + a[1][i]); mb[i] = 0.5f * (b[0][i] + b[1][i]); }
    const double d = rmsDb (ma, (int) (fs * 0.4), (int) (fs * 0.9)) - rmsDb (mb, (int) (fs * 0.4), (int) (fs * 0.9));
    MEASURE ("mono sum level with space vs without: %+.2f dB", d);
    CHECK (std::abs (d) < 3.0);
}

TEST ("space: widening alone leaves the mono sum bit-exact (no comb cancellation by construction)")
{
    const double fs = 48000;
    vs::SpatialEngine se;
    se.prepare (fs, 2);
    vs::SpatialParams sp; sp.amount = 1; sp.width = 0.95f; sp.modDepthMs = 0.7f;
    se.setParams (sp);
    auto n = whiteNoise ((int) (fs * 2), 0.2f, 5);
    Signal l = n, r = n;
    // let the smoothers settle first
    for (int pass = 0; pass < 2; ++pass)
    {
        l = n; r = n;
        for (size_t pos = 0; pos < l.size(); pos += 480)
        {
            float* io[2] = { l.data() + pos, r.data() + pos };
            se.process (io, 2, 480);
        }
    }
    double maxDiff = 0;
    const double norm = 1.0 / std::sqrt (1.0 + 0.5 * 0.95 * 0.95);
    for (size_t i = (size_t) fs; i < l.size(); ++i)
        maxDiff = std::max (maxDiff, std::abs ((double) l[i] + r[i] - 2.0 * n[i] * norm));
    MEASURE ("max |(L+R) - 2*input*norm| with width 0.95: %.2e", maxDiff);
    CHECK (maxDiff < 1e-5);
    CHECK (corrLR (l, r, (int) fs, (int) fs) < 0.9);
}

TEST ("space: reverb is stable, decays, and its RT60 follows the setting; silence in -> silence out")
{
    const double fs = 48000;
    for (float rt : { 0.8f, 1.8f })
    {
        vs::SpatialEngine se;
        se.prepare (fs, 2);
        vs::SpatialParams sp; sp.amount = 1; sp.reverbGain = 0.55f; sp.rt60 = rt;
        se.setParams (sp);
        Signal l ((size_t) (fs * 6), 0.0f), r = l;
        l[200] = r[200] = 1.0f; // impulse after the smoothers have settled
        // settle: process 1 s of silence first
        Signal pre ((size_t) fs, 0.0f), pre2 = pre;
        float* ip[2] = { pre.data(), pre2.data() };
        se.process (ip, 2, (int) fs);
        for (size_t pos = 0; pos < l.size(); pos += 512)
        {
            float* io[2] = { l.data() + pos, r.data() + pos };
            se.process (io, 2, (int) std::min<size_t> (512, l.size() - pos));
        }
        CHECK (allFinite (l) && allFinite (r));
        // Schroeder: backwards-integrated energy decay; fit -5..-25 dB, extrapolate to -60
        std::vector<double> edc (l.size());
        double acc = 0;
        l[200] -= 1.0f; r[200] -= 1.0f; // remove the dry impulse: only the reverb return is measured
        for (size_t i = l.size(); i-- > 0;) { acc += (double) l[i] * l[i] + (double) r[i] * r[i]; edc[i] = acc; }
        auto dbAt = [&] (size_t i) { return 10 * std::log10 (edc[i] / edc[0] + 1e-30); };
        size_t t5 = 0, t25 = 0;
        for (size_t i = 0; i < edc.size(); ++i) { if (! t5 && dbAt (i) < -5) t5 = i; if (! t25 && dbAt (i) < -25) { t25 = i; break; } }
        const double rt60 = 3.0 * (double) (t25 - t5) / fs;
        MEASURE ("rt60 setting %.1f s -> measured %.2f s (EDC fit -5..-25 dB); last 0.5 s energy %.1f dB re total", rt, rt60, dbAt (l.size() - (size_t) (fs * 0.5)));
        CHECK (rt60 > 0.5 * rt && rt60 < 1.5 * rt);
        CHECK (dbAt (l.size() - (size_t) (fs * 0.5)) < -40.0);
    }
    // long noise at maximum settings: finite and bounded (no runaway feedback)
    vs::SpatialEngine se;
    se.prepare (fs, 2);
    vs::SpatialParams sp; sp.amount = 1; sp.width = 0.95f; sp.reverbGain = 0.6f; sp.rt60 = 2.0f; sp.slapGain = 0.3f; sp.modDepthMs = 1.0f;
    se.setParams (sp);
    auto n = whiteNoise ((int) (fs * 60), 0.5f, 8);
    Signal l = n, r = n;
    float pk = 0;
    for (size_t pos = 0; pos < l.size(); pos += 1024)
    {
        float* io[2] = { l.data() + pos, r.data() + pos };
        se.process (io, 2, (int) std::min<size_t> (1024, l.size() - pos));
    }
    for (size_t i = 0; i < l.size(); ++i) pk = std::max ({ pk, std::abs (l[i]), std::abs (r[i]) });
    MEASURE ("60 s of full-scale-ish noise at the maximum settings: peak %.2f (input peak 0.5)", pk);
    CHECK (allFinite (l) && allFinite (r));
    CHECK (pk < 4.0f);
    Signal z ((size_t) (fs * 3), 0.0f), z2 = z;
    se.reset();
    float* zp[2] = { z.data(), z2.data() };
    se.process (zp, 2, (int) z.size());
    CHECK (vs::peakAbs (z.data(), (int) z.size()) == 0.0f && vs::peakAbs (z2.data(), (int) z2.size()) == 0.0f);
}

TEST ("space: slap delay is stable (feedback < 1), follows the host tempo and falls back to 115 ms")
{
    const double fs = 48000;
    for (double bpm : { 0.0, 90.0, 140.0 })
    {
        vs::SpatialEngine se;
        se.prepare (fs, 2);
        se.setTempoBpm (bpm);
        vs::SpatialParams sp; sp.amount = 1; sp.slapGain = 0.3f;
        se.setParams (sp);
        Signal pre ((size_t) fs, 0.0f), pre2 = pre;
        float* ip[2] = { pre.data(), pre2.data() };
        se.process (ip, 2, (int) fs);
        Signal l ((size_t) (fs * 3), 0.0f), r = l;
        l[1000] = r[1000] = 1.0f;
        float* io[2] = { l.data(), r.data() };
        se.process (io, 2, (int) l.size());
        // first echo position on L
        size_t peak = 0; float best = 0;
        for (size_t i = 3000; i < l.size(); ++i) if (std::abs (l[i]) > best) { best = std::abs (l[i]); peak = i; }
        double expectedMs = 115.0;
        if (bpm > 0) { expectedMs = 60000.0 / bpm / 4.0; while (expectedMs < 70) expectedMs *= 2; while (expectedMs > 180) expectedMs *= 0.5; }
        const double ms = (double) (peak - 1000) * 1000.0 / fs;
        double late = 0; for (size_t i = (size_t) (fs * 2); i < l.size(); ++i) late = std::max (late, (double) std::abs (l[i]));
        MEASURE ("tempo %.0f: first echo at %.1f ms (expected ~%.1f ms), level %.3f, residual after 2 s: %.2e", bpm, ms, expectedMs, best, late);
        CHECK (allFinite (l));
        CHECK (std::abs (ms - expectedMs) < 8.0);   // filters add a little group delay
        CHECK (late < 1e-3);
    }
}

TEST ("space: mono hosts are untouched; stereo spatial adds no reported latency")
{
    const double fs = 48000;
    const auto x = vowel (fs, 200, 2.0);
    auto p = urbanParams();
    auto ps = p; ps.spatial = false;
    auto mono = [&] (const vs::VoiceParams& q) { vs::VocalChain c; c.setParams (q, false); c.prepare (fs, 1, 512); return run (c, { x }, 512)[0]; };
    const auto mA = mono (p), mB = mono (ps);
    // In mono only the Caracter texture differs; no width/reverb/echo can exist (single channel): finite and bounded
    CHECK (allFinite (mA));
    vs::VocalChain c1, c2;
    c1.setParams (p, false); c1.prepare (fs, 2, 512);
    c2.setParams (ps, false); c2.prepare (fs, 2, 512);
    MEASURE ("latency with space %d, without %d samples; spatial memory %ld kB", c1.latencySamples(), c2.latencySamples(), c1.spatialMemoryBytes() / 1024);
    CHECK (c1.latencySamples() == c2.latencySamples());
}

TEST ("caracter: texture adds body/presence/warmth (not just treble), 0 % is a bypass, sibilants stay clean")
{
    const double fs = 48000;
    const auto x = vowel (fs, 200, 2.0, 0.2f);
    const auto xLow = vowel (fs, 110, 2.0, 0.2f);
    auto texRun = [&] (float c, const Signal& in) {
        vs::VoiceTexture t; t.setAmount (c); t.prepare (fs, 1);
        Signal y = in;
        for (size_t pos = 0; pos < y.size(); pos += 256) { float* p = y.data() + pos; t.process (&p, 1, (int) std::min<size_t> (256, y.size() - pos)); }
        return y;
    };
    CHECK (texRun (0.0f, x) == x);
    const auto y0 = texRun (0.0f, x), y65 = texRun (0.65f, x), y100 = texRun (1.0f, x);
    auto band = [&] (const Signal& s, double f) { return dbOf (toneAmp (s, fs, f, (int) fs, (int) fs)); };
    // design: low shelf 220 Hz +1.95 dB (c = 0.65) -> measured on the 110 Hz fundamental of a second vowel;
    // peak 3 kHz +1.43 dB -> 15th harmonic of the 200 Hz vowel; 9 kHz must not move.
    const auto b0 = texRun (0.0f, xLow), b65 = texRun (0.65f, xLow);
    const double body = band (b65, 110) - band (b0, 110), pres = band (y65, 3000) - band (y0, 3000), tre = band (y65, 9000) - band (y0, 9000);
    MEASURE ("Caracter 65 %%: body (110 Hz) %+.2f dB (design +1.9), presence (3 kHz) %+.2f dB (design +1.4), treble (9 kHz) %+.2f dB", body, pres, tre);
    CHECK (body > 1.0 && pres > 0.5);
    CHECK (std::abs (tre) < 1.0);            // it is not a treble boost
    CHECK (allFinite (y100));
    // sibilance protection: the saturator only sees < 1.8 kHz
    auto sib = whiteNoise ((int) (fs * 1.0), 0.1f, 4);
    for (size_t i = sib.size() - 1; i > 1; --i) sib[i] = sib[i] - 1.6f * sib[i - 1] + 0.8f * sib[i - 2];
    const double dS = rmsDb (texRun (1.0f, sib), 12000, 24000) - rmsDb (sib, 12000, 24000);
    MEASURE ("Caracter 100 %% on bright noise: %+.2f dB", dS);
    CHECK (std::abs (dS) < 2.0);
}

TEST ("caracter + magia: urban preset at equal loudness differs audibly from Phase 3, loudness stays within 2 dB")
{
    const double fs = 48000;
    const auto x = speechLike (fs);
    auto p3 = urbanParams(); p3.spatial = false; p3.secondGainDb = -12;
    auto p4 = urbanParams();
    const auto a = secondVoiceOnly (p3, x, fs), b = secondVoiceOnly (p4, x, fs);
    const double la = rmsDb (a[0], 7200, (int) (fs * 1.2)), lb = rmsDb (b[0], 7200, (int) (fs * 1.2));
    double d = 0, e = 0;
    for (size_t i = 0; i < a[0].size(); ++i) { d += (double) (a[0][i] - b[0][i]) * (a[0][i] - b[0][i]); e += (double) a[0][i] * a[0][i]; }
    MEASURE ("second voice level: Phase 3 %.1f dB, Phase 4 %.1f dB (%+.1f); difference between renders %.1f dB", la, lb, lb - la, 10 * std::log10 (d / e));
    CHECK (std::abs (lb - la) < 2.5);
    CHECK (10 * std::log10 (d / e) > -12.0);
}

TEST ("urban preset: output stays below the clip threshold; presets switch during playback without clicks or NaN")
{
    const double fs = 48000;
    auto v = vowel (fs, 200, 4.0, 0.5f); // -6 dBFS
    vs::VocalChain chain;
    chain.setParams (urbanParams(), false);
    chain.prepare (fs, 2, 256);
    const auto& presets = vs::factoryPresets();
    const auto o = run (chain, { v, v }, 256, [&] (int pos) {
        chain.setParams (presets[(size_t) ((pos / 6000) % (int) presets.size())].params, false);
    });
    const float pk = std::max (vs::peakAbs (o[0].data(), (int) v.size()), vs::peakAbs (o[1].data(), (int) v.size()));
    MEASURE ("every factory preset in turn every 125 ms: peak %.3f, clip-altered samples %ld", pk, chain.clipAlteredSamples());
    CHECK (allFinite (o[0]) && allFinite (o[1]));
    CHECK (pk < 1.0f);
    // static: urban preset at -6 dBFS input stays clear of the clipper
    vs::VocalChain c2;
    c2.setParams (urbanParams(), false);
    c2.prepare (fs, 2, 512);
    run (c2, { v, v }, 512);
    MEASURE ("urban preset, input -6 dBFS: clip-altered samples %ld", c2.clipAlteredSamples());
    CHECK (c2.clipAlteredSamples() == 0);
}

TEST ("realtime: zero allocations with the spatial engine, fader automation and preset switching")
{
    const double fs = 48000;
    auto v = vowel (fs, 200, 1.0);
    vs::VocalChain chain;
    chain.setParams (urbanParams(), false);
    chain.prepare (fs, 2, 512);
    Signal a = v, b = v;
    float* ptrs[2] = { a.data(), b.data() };
    const auto& presets = vs::factoryPresets();
    g_allocCount = 0; g_trackAllocs = true;
    for (int blk = 0; blk < 80; ++blk)
    {
        auto p = presets[(size_t) (blk % (int) presets.size())].params;
        p.secondGainDb = -60.0f + (float) (blk % 66);
        p.magic = (float) ((blk * 7) % 101);
        chain.setTempoBpm (80.0 + blk);
        chain.setParams (p, false);
        chain.process (ptrs, 2, 512);
    }
    g_trackAllocs = false;
    MEASURE ("allocations during 80 blocks: %ld", g_allocCount.load());
    CHECK (g_allocCount.load() == 0);
}

TEST ("performance: Phase 4 urban preset vs Phase 3 second voice (stereo, 48 kHz, 512 blocks)")
{
    const double fs = 48000;
    const int seconds = 10;
    auto v = vowel (fs, 200, seconds);
    auto timeIt = [&] (const vs::VoiceParams& p) {
        vs::VocalChain chain;
        chain.setParams (p, false);
        chain.prepare (fs, 2, 512);
        const auto t0 = std::chrono::steady_clock::now();
        run (chain, { v, v }, 512);
        return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / seconds;
    };
    auto p3 = urbanParams(); p3.spatial = false;
    timeIt (p3);
    const double c3 = timeIt (p3), c4 = timeIt (urbanParams());
    MEASURE ("Phase 3 second voice %.1f %% of one core, Phase 4 urban preset %.1f %% (%+.1f points)", 100 * c3, 100 * c4, 100 * (c4 - c3));
    CHECK (c4 < 0.25);
}

TEST ("regression: Phase 3 second-voice presets keep their exact Phase 3 sound (spatial off)")
{
    for (const char* id : { "second_voice_7", "second_voice_12" })
    {
        const auto* fp = vs::findFactoryPreset (id);
        CHECK (fp != nullptr && ! fp->params.spatial);
        const auto e = vs::mapMacros (fp->params);
        CHECK (e.spatialAmount == 0.0f && e.textureAmount == 0.0f && e.spaceReverb == 0.0f);
        CHECK (e.secondGainDb == -9.0f);
    }
    const auto u = vs::mapMacros (urbanParams());
    MEASURE ("urban preset mapping: width %.3f, reverb %.3f, rt60 %.2f s, slap %.3f, mod %.2f ms, texture %.2f, fader %.1f dB",
             u.spaceWidth, u.spaceReverb, u.spaceRt60, u.spaceSlap, u.spaceModMs, u.textureAmount, u.secondGainDb);
    CHECK (u.secondVoice && u.secondGainDb == -12.0f && u.semitones == 7.0f);
    // Magia curves: 0 -> nothing; monotonic; 30 % discreet; 100 % stronger but bounded
    float prevW = -1, prevR = -1;
    for (int m = 0; m <= 100; m += 10)
    {
        auto p = urbanParams(); p.magic = (float) m;
        const auto e = vs::mapMacros (p);
        CHECK (e.spaceWidth >= prevW && e.spaceReverb >= prevR);
        CHECK (e.spaceWidth <= 0.95f && e.spaceReverb <= 0.55f && e.spaceSlap <= 0.28f);
        if (m == 0) CHECK (e.spaceWidth == 0.0f && e.spaceReverb == 0.0f && e.spaceSlap == 0.0f);
        if (m == 30) CHECK (e.spaceWidth < 0.25f && e.spaceReverb < 0.12f && e.spaceSlap == 0.0f);
        prevW = e.spaceWidth; prevR = e.spaceReverb;
    }
}


int main (int argc, char** argv)
{
    return vstest::runAll (argc, argv);
}
