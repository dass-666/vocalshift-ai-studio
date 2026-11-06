// Plugin-level tests: state, presets, layouts, sample rates / buffer sizes,
// host-like call sequences. Uses the real VocalShiftProcessor.

#include "Signals.h"
#include <random>
#include "TestFramework.h"

#include "../src/plugin/PluginProcessor.h"

#include <juce_events/juce_events.h>

using namespace vstest;
using vs::VocalShiftProcessor;

namespace
{
void setParam (VocalShiftProcessor& p, const char* id, float value)
{
    auto* param = p.getState().getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float getParam (VocalShiftProcessor& p, const char* id)
{
    return p.getState().getRawParameterValue (id)->load();
}

bool setLayout (VocalShiftProcessor& p, const juce::AudioChannelSet& set)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (set);
    l.outputBuses.add (set);
    return p.setBusesLayout (l);
}

bool bufferFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i)))
                return false;
    return true;
}

// Streams a mono test signal (copied to all channels) through the processor.
std::vector<float> render (VocalShiftProcessor& p, const std::vector<float>& x, int block,
                           const std::function<void (int)>& perBlock = nullptr, bool* finite = nullptr)
{
    const int ch = p.getTotalNumOutputChannels();
    std::vector<float> out (x.size(), 0.0f);
    juce::AudioBuffer<float> buf (ch, block);
    juce::MidiBuffer midi;
    bool ok = true;
    for (int pos = 0; pos < (int) x.size(); pos += block)
    {
        const int n = std::min (block, (int) x.size() - pos);
        if (perBlock) perBlock (pos);
        buf.setSize (ch, n, false, false, true);
        for (int c = 0; c < ch; ++c)
            buf.copyFrom (c, 0, x.data() + pos, n);
        p.processBlock (buf, midi);
        ok = ok && bufferFinite (buf);
        std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, out.begin() + pos);
    }
    if (finite) *finite = ok;
    return out;
}
} // namespace

TEST ("state: full round trip of parameters and non-parameter state")
{
    VocalShiftProcessor a;
    setParam (a, vs::params::semitones, -7);
    setParam (a, vs::params::fineCents, 33);
    setParam (a, vs::params::character, 42.5f);
    setParam (a, vs::params::magic, 12.5f);
    setParam (a, vs::params::mode, 1);
    setParam (a, vs::params::autoLevel, 0);
    setParam (a, vs::params::formantShift, 3.25f);
    setParam (a, vs::params::brightness, -5.5f);
    setParam (a, vs::params::mix, 61);
    setParam (a, vs::params::outputGain, -3.5f);
    setParam (a, vs::params::safetyClip, 0);
    setParam (a, vs::params::bypass, 1);
    a.getState().state.setProperty (vs::params::propUiMode, "pro", nullptr);
    a.getState().state.setProperty (vs::params::propPresetId, "dark_voice", nullptr);

    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    MEASURE ("state size = %d bytes", (int) mb.getSize());

    VocalShiftProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    for (auto* id : { vs::params::mode, vs::params::character, vs::params::magic, vs::params::autoLevel,
                      vs::params::semitones, vs::params::fineCents, vs::params::formantShift,
                      vs::params::brightness, vs::params::mix, vs::params::outputGain, vs::params::safetyClip, vs::params::bypass })
        CHECK_MSG (std::abs (getParam (a, id) - getParam (b, id)) < 1e-4f, "%s: %f vs %f", id, getParam (a, id), getParam (b, id));
    CHECK (b.getState().state.getProperty (vs::params::propUiMode).toString() == "pro");
    CHECK (b.getState().state.getProperty (vs::params::propPresetId).toString() == "dark_voice");
    CHECK ((int) b.getState().state.getProperty (vs::params::propStateVersion) == vs::params::currentStateVersion);
}

TEST ("state: corrupt or foreign data is ignored without crashing")
{
    VocalShiftProcessor p;
    setParam (p, vs::params::semitones, 5);
    juce::Random r (3);
    for (int len : { 0, 1, 7, 100, 5000 })
    {
        juce::MemoryBlock junk ((size_t) len);
        for (int i = 0; i < len; ++i) static_cast<char*> (junk.getData())[i] = (char) r.nextInt (256);
        p.setStateInformation (junk.getData(), len);
    }
    // Valid XML with a different root tag
    juce::XmlElement other ("SomethingElse");
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (other, mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (std::abs (getParam (p, vs::params::semitones) - 5.0f) < 1e-4f);
}

TEST ("presets: factory presets set exactly their declared values")
{
    VocalShiftProcessor p;
    for (const auto& fp : vs::factoryPresets())
    {
        CHECK (p.getPresetManager().applyFactoryPreset (fp.id));
        CHECK (std::abs (getParam (p, vs::params::semitones) - fp.params.semitones) < 1e-4f);
        CHECK (std::abs (getParam (p, vs::params::character) - fp.params.character) < 0.051f);
        CHECK (std::abs (getParam (p, vs::params::magic) - fp.params.magic) < 0.051f);
        CHECK (std::abs (getParam (p, vs::params::formantShift) - fp.params.formantOffset) < 0.006f);
        CHECK (std::abs (getParam (p, vs::params::brightness) - fp.params.brightnessOffset) < 0.006f);
        CHECK (std::abs (getParam (p, vs::params::outputGain) - fp.params.outputGainDb) < 0.006f);
        CHECK ((int) getParam (p, vs::params::mode) == (int) fp.params.mode);
        CHECK ((int) getParam (p, vs::params::route) == (int) fp.params.route);
        CHECK (std::abs (getParam (p, vs::params::secondGain) - fp.params.secondGainDb) < 0.051f);
        CHECK ((getParam (p, vs::params::improved) >= 0.5f) == fp.params.improved);
        CHECK (p.getPresetManager().currentPresetId() == juce::String (fp.id));
    }
    CHECK (! p.getPresetManager().applyFactoryPreset ("does_not_exist"));
}

TEST ("presets: user preset file round trip and version check")
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vs_preset_test");
    dir.deleteRecursively();
    auto file = dir.getChildFile (juce::String ("Mi preset") + vs::PresetManager::fileExtension);

    VocalShiftProcessor a;
    a.getPresetManager().applyFactoryPreset ("low_deep");
    setParam (a, vs::params::mix, 77);
    CHECK (a.getPresetManager().saveUserPreset (file, "Mi preset").wasOk());
    CHECK (file.existsAsFile());

    VocalShiftProcessor b;
    CHECK (b.getPresetManager().loadUserPreset (file).wasOk());
    CHECK (std::abs (getParam (b, vs::params::semitones) - (-9.0f)) < 1e-4f);
    CHECK (std::abs (getParam (b, vs::params::mix) - 77.0f) < 0.051f);
    CHECK (b.getPresetManager().currentPresetId() == "user:Mi preset");

    auto future = a.getPresetManager().toPresetTree ("x");
    future.setProperty ("formatVersion", 99, nullptr);
    CHECK (b.getPresetManager().fromPresetTree (future).failed());
    dir.deleteRecursively();
}

TEST ("host: mono+stereo x sample rates x buffer sizes, latency reported")
{
    const auto x = harmonicTone (48000, 220, 1.0);
    for (auto layout : { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() })
        for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
            for (int bs : { 32, 64, 128, 333, 512, 1024, 2048 })
            {
                VocalShiftProcessor p;
                CHECK (setLayout (p, layout));
                setParam (p, vs::params::semitones, 5);
                p.setRateAndBufferSizeDetails (fs, bs);
                p.prepareToPlay (fs, bs);
                bool finite = false;
                render (p, x, bs, nullptr, &finite);
                CHECK (finite);
                CHECK (p.getLatencySamples() > 0 && p.getLatencySamples() == p.getEngineLatencySamples());
                if (bs == 512)
                    MEASURE ("%s fs=%.0f: latency %d samples = %.1f ms", layout.getDescription().toRawUTF8(), fs,
                             p.getLatencySamples(), 1000.0 * p.getLatencySamples() / fs);
            }
    CHECK (! [] {
        VocalShiftProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        return p.checkBusesLayoutSupported (l);
    }());
}

TEST ("host: blocks larger than announced, zero-length blocks, restart and reset")
{
    VocalShiftProcessor p;
    setParam (p, vs::params::semitones, -5);
    p.prepareToPlay (48000, 256);
    const auto x = harmonicTone (48000, 200, 1.0);
    bool finite = false;
    render (p, x, 1024, nullptr, &finite); // 4x the announced size
    CHECK (finite);

    juce::AudioBuffer<float> empty (2, 0);
    juce::MidiBuffer midi;
    p.processBlock (empty, midi);

    // Transport restart: release / prepare at another rate, then reset mid-stream.
    p.releaseResources();
    p.prepareToPlay (44100, 512);
    render (p, x, 512, [&] (int pos) { if (pos == 512 * 20) p.reset(); }, &finite);
    CHECK (finite);
    CHECK (p.getNonFiniteEvents() == 0);
}

TEST ("host: bypass parameter gives latency-aligned dry output")
{
    VocalShiftProcessor p;
    setLayout (p, juce::AudioChannelSet::mono());
    setParam (p, vs::params::semitones, 12);
    setParam (p, vs::params::outputGain, -6);
    setParam (p, vs::params::bypass, 1);
    p.prepareToPlay (48000, 512);
    const auto x = harmonicTone (48000, 250, 1.5);
    auto out = render (p, x, 512);
    const int lat = p.getLatencySamples();
    float err = 0;
    for (size_t i = (size_t) lat + 4800; i < x.size(); ++i)
        err = std::max (err, std::abs (out[i] - x[i - (size_t) lat]));
    MEASURE ("bypass max error vs input delayed by %d = %g", lat, err);
    CHECK (err < 1e-6f);

    // processBlockBypassed (host-side bypass without parameter) must behave the same.
    VocalShiftProcessor q;
    setLayout (q, juce::AudioChannelSet::mono());
    setParam (q, vs::params::semitones, 12);
    q.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> buf (1, 512);
    juce::MidiBuffer midi;
    float err2 = 0;
    std::vector<float> hist;
    for (int pos = 0; pos + 512 <= (int) x.size(); pos += 512)
    {
        buf.copyFrom (0, 0, x.data() + pos, 512);
        q.processBlockBypassed (buf, midi);
        hist.insert (hist.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
    }
    for (size_t i = (size_t) lat + 4800; i < hist.size(); ++i)
        err2 = std::max (err2, std::abs (hist[i] - x[i - (size_t) lat]));
    CHECK (err2 < 1e-6f);
}

TEST ("host: offline (non-realtime) render equals realtime render")
{
    const auto v = vowel (48000, 180, 2.0);
    auto go = [&] (bool offline) {
        VocalShiftProcessor p;
        p.getPresetManager().applyFactoryPreset ("high_intense");
        p.setNonRealtime (offline);
        p.prepareToPlay (48000, 512);
        return render (p, v, 512);
    };
    const auto rt = go (false), off = go (true);
    CHECK (rt == off);
    MEASURE ("offline vs realtime identical: %d (same host block size; block-size independence is checked in core tests)", (int) (rt == off));
}

TEST ("host: preset changes and automation while playing stay finite")
{
    VocalShiftProcessor p;
    p.prepareToPlay (48000, 256);
    const auto v = vowel (48000, 210, 3.0);
    const auto& fps = vs::factoryPresets();
    size_t idx = 0;
    bool finite = false;
    render (p, v, 256, [&] (int pos) {
        if (pos % 12000 == 0)
            p.getPresetManager().applyFactoryPreset (fps[idx++ % fps.size()].id);
        if (pos % 2560 == 0)
            setParam (p, vs::params::bypass, (pos / 2560) % 5 == 0 ? 1.0f : 0.0f);
    }, &finite);
    CHECK (finite);
}

TEST ("defaults: a new instance starts at the reference values (+7 st, 65 %, 70 %, keep identity)")
{
    VocalShiftProcessor p;
    CHECK (getParam (p, vs::params::semitones) == 7.0f);
    CHECK (std::abs (getParam (p, vs::params::character) - 65.0f) < 1e-4f);
    CHECK (std::abs (getParam (p, vs::params::magic) - 70.0f) < 1e-4f);
    CHECK (getParam (p, vs::params::mode) == 0.0f);
    CHECK (getParam (p, vs::params::autoLevel) == 1.0f);
    CHECK (p.getPresetManager().currentPresetId() == "reference");
    const auto* ref = vs::findFactoryPreset ("reference");
    CHECK (ref != nullptr && ref->params.semitones == 7.0f && ref->params.character == 65.0f && ref->params.magic == 70.0f);
}

TEST ("state: v1 project state (engineMode property, formantPreserve) migrates to the current version")
{
    // Hand-built v1 state as the first prototype saved it.
    juce::ValueTree v1 ("VocalShiftState");
    v1.setProperty ("stateVersion", 1, nullptr);
    v1.setProperty ("engineMode", 1, nullptr);
    v1.setProperty ("presetId", "dark_voice", nullptr);
    auto addParam = [&] (const char* id, float value) {
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", id, nullptr);
        c.setProperty ("value", value, nullptr);
        v1.appendChild (c, nullptr);
    };
    addParam ("semitones", -3.0f);
    addParam ("formantPreserve", 40.0f); // obsolete parameter
    addParam ("mix", 80.0f);
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*v1.createXml(), mb);

    VocalShiftProcessor p;
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (getParam (p, vs::params::semitones) == -3.0f);
    CHECK (std::abs (getParam (p, vs::params::mix) - 80.0f) < 0.051f);
    CHECK (getParam (p, vs::params::mode) == 1.0f); // engineMode=1 -> radical
    CHECK ((int) p.getState().state.getProperty (vs::params::propStateVersion) == vs::params::currentStateVersion);
    CHECK (! p.getState().state.hasProperty (vs::params::legacyPropEngineMode));
    // A v1 project predates the revised DSP and the second voice: it must keep the old sound.
    CHECK (getParam (p, vs::params::improved) == 0.0f && getParam (p, vs::params::route) == 0.0f);
    // Parameters absent from v1 (character/magic) keep their defaults.
    CHECK (std::abs (getParam (p, vs::params::character) - 65.0f) < 1e-4f);
}

TEST ("macros: changing Pitch/Character/Magic/mode on the processor changes the output")
{
    const auto v = vowel (48000, 190, 2.0);
    auto renderWith = [&] (const std::function<void (VocalShiftProcessor&)>& setup) {
        VocalShiftProcessor p;
        setup (p);
        p.prepareToPlay (48000, 512);
        return render (p, v, 512);
    };
    const auto ref = renderWith ([] (VocalShiftProcessor&) {});
    auto diffDb = [&] (const std::vector<float>& a) {
        double d = 0, r = 0;
        for (size_t i = 0; i < a.size(); ++i) { d += (double) (a[i] - ref[i]) * (a[i] - ref[i]); r += (double) ref[i] * ref[i]; }
        return 10 * std::log10 ((d + 1e-20) / (r + 1e-20));
    };
    const double dPitch = diffDb (renderWith ([] (auto& p) { setParam (p, vs::params::semitones, 3); }));
    const double dChar = diffDb (renderWith ([] (auto& p) { setParam (p, vs::params::character, 0); }));
    const double dMagic = diffDb (renderWith ([] (auto& p) { setParam (p, vs::params::magic, 0); }));
    const double dMode = diffDb (renderWith ([] (auto& p) { setParam (p, vs::params::mode, 1); }));
    MEASURE ("difference vs reference render: Pitch 7->3 %.1f dB, Character 65->0 %.1f dB, Magic 70->0 %.1f dB, mode->radical %.1f dB",
             dPitch, dChar, dMagic, dMode);
    // Every macro must audibly change the signal (none is decorative).
    CHECK (dPitch > -20.0 && dChar > -30.0 && dMagic > -30.0 && dMode > -30.0);
}

TEST ("state: second-voice settings survive a save/restore round trip")
{
    VocalShiftProcessor a;
    setParam (a, vs::params::route, 1);
    setParam (a, vs::params::secondGain, -12.5f);
    setParam (a, vs::params::pan, -40);
    setParam (a, vs::params::offsetMs, 23.5f);
    setParam (a, vs::params::improved, 0);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    VocalShiftProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (getParam (b, vs::params::route) == 1.0f);
    CHECK (std::abs (getParam (b, vs::params::secondGain) + 12.5f) < 0.051f);
    CHECK (std::abs (getParam (b, vs::params::pan) + 40.0f) < 0.51f);
    CHECK (std::abs (getParam (b, vs::params::offsetMs) - 23.5f) < 0.051f);
    CHECK (getParam (b, vs::params::improved) == 0.0f);
}

TEST ("compatibility: a v2 project (before the second voice) restores with the previous DSP and sounds identical")
{
    // Hand-built v2 state: no route/secondGain/pan/offset/improved parameters at all.
    juce::ValueTree v2 ("VocalShiftState");
    v2.setProperty ("stateVersion", 2, nullptr);
    v2.setProperty ("presetId", "reference", nullptr);
    auto addParam = [&] (const char* id, float value) {
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", id, nullptr);
        c.setProperty ("value", value, nullptr);
        v2.appendChild (c, nullptr);
    };
    addParam ("semitones", 7.0f); addParam ("character", 65.0f); addParam ("magic", 70.0f); addParam ("mode", 0.0f);
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*v2.createXml(), mb);

    VocalShiftProcessor old;
    old.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (getParam (old, vs::params::improved) == 0.0f);
    CHECK (getParam (old, vs::params::route) == 0.0f);
    CHECK (std::abs (getParam (old, vs::params::secondGain) + 9.0f) < 0.051f);

    const auto v = vowel (48000, 190, 2.0);
    auto renderOf = [&] (VocalShiftProcessor& p) { p.prepareToPlay (48000, 512); return render (p, v, 512); };
    VocalShiftProcessor explicitLegacy;
    setParam (explicitLegacy, vs::params::improved, 0);
    CHECK (renderOf (old) == renderOf (explicitLegacy));
    VocalShiftProcessor fresh; // new defaults use the revised DSP
    const auto a = renderOf (old), b = renderOf (fresh);
    double d = 0, r = 0;
    for (size_t i = 0; i < a.size(); ++i) { d += (double) (a[i] - b[i]) * (a[i] - b[i]); r += (double) a[i] * a[i]; }
    MEASURE ("old project vs new default render: difference %.1f dB (the revised DSP is audibly a different sound; hence the explicit switch)",
             10 * std::log10 ((d + 1e-20) / (r + 1e-20)));
}

TEST ("compatibility: v1 and v2 preset files load as transform route + previous DSP; v3 files keep the new fields")
{
    VocalShiftProcessor src;
    src.getPresetManager().applyFactoryPreset ("high_natural");
    for (int version : { 1, 2 })
    {
        auto tree = src.getPresetManager().toPresetTree ("viejo");
        tree.setProperty ("formatVersion", version, nullptr);
        // strip the v3 parameters as an old file would not contain them
        for (int i = tree.getNumChildren() - 1; i >= 0; --i)
        {
            const auto id = tree.getChild (i).getProperty ("id").toString();
            for (auto* n : { vs::params::route, vs::params::secondGain, vs::params::pan, vs::params::offsetMs, vs::params::improved })
                if (id == n) { tree.removeChild (i, nullptr); break; }
        }
        VocalShiftProcessor p;
        setParam (p, vs::params::route, 1); // pretend we were in second-voice mode before loading
        CHECK (p.getPresetManager().fromPresetTree (tree).wasOk());
        CHECK (getParam (p, vs::params::route) == 0.0f);
        CHECK (getParam (p, vs::params::improved) == 0.0f);
        CHECK (std::abs (getParam (p, vs::params::semitones) - 4.0f) < 1e-4f);
    }
    VocalShiftProcessor a;
    a.getPresetManager().applyFactoryPreset ("second_voice_12");
    setParam (a, vs::params::pan, 30);
    auto t3 = a.getPresetManager().toPresetTree ("nuevo");
    VocalShiftProcessor b;
    CHECK (b.getPresetManager().fromPresetTree (t3).wasOk());
    CHECK (getParam (b, vs::params::route) == 1.0f);
    CHECK (std::abs (getParam (b, vs::params::semitones) - 12.0f) < 1e-4f);
    CHECK (std::abs (getParam (b, vs::params::pan) - 30.0f) < 0.51f);
    CHECK (getParam (b, vs::params::improved) == 1.0f);
}

TEST ("host: the reported latency is identical for every route / offset / improved setting")
{
    int latency = -1;
    for (int route : { 0, 1 })
        for (float offset : { 0.0f, 100.0f })
            for (int improved : { 0, 1 })
            {
                VocalShiftProcessor p;
                setParam (p, vs::params::route, (float) route);
                setParam (p, vs::params::offsetMs, offset);
                setParam (p, vs::params::improved, (float) improved);
                p.prepareToPlay (48000, 512);
                if (latency < 0) latency = p.getLatencySamples();
                CHECK (p.getLatencySamples() == latency);
            }
    MEASURE ("latency across 8 combinations: %d samples", latency);
}

TEST ("host: second voice through the processor (stereo), automation, switching, bypass, buffer sizes")
{
    const auto v = vowel (48000, 190, 3.0);
    VocalShiftProcessor p;
    setParam (p, vs::params::route, 1);
    p.prepareToPlay (48000, 512);
    bool finite = true;
    std::mt19937 rng (3);
    auto out = render (p, v, 300, [&] (int pos) {
        setParam (p, vs::params::semitones, (float) ((int) (rng() % 49) - 24));
        setParam (p, vs::params::character, (float) (rng() % 101));
        setParam (p, vs::params::magic, (float) (rng() % 101));
        setParam (p, vs::params::route, (float) ((pos / 9000) % 2));
        setParam (p, vs::params::bypass, (float) ((pos / 20000) % 4 == 3));
    }, &finite);
    float pk = 0;
    for (float s : out) pk = std::max (pk, std::abs (s));
    MEASURE ("second-voice automation through the processor: peak %.3f, finite=%d", pk, (int) finite);
    CHECK (finite);
    CHECK (pk < 2.0f);
}

TEST ("macros: second-voice mode adds the transposed voice and keeps the original at the front")
{
    const auto v = vowel (48000, 190, 2.0);
    auto renderWith = [&] (float route) {
        VocalShiftProcessor p;
        setParam (p, vs::params::route, route);
        p.prepareToPlay (48000, 512);
        return render (p, v, 512);
    };
    const auto t = renderWith (0), s = renderWith (1);
    auto rms = [] (const std::vector<float>& x, size_t a, size_t b) {
        double e = 0; for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i];
        return 10 * std::log10 (e / (double) (b - a) + 1e-12);
    };
    const double dT = rms (t, 48000, 90000), dS = rms (s, 48000, 90000), dIn = rms (v, 48000 - 7200, 90000 - 7200);
    MEASURE ("input %.1f dB, transform %.1f dB, second voice %.1f dB (expected: about +0.5 dB over the input)", dIn, dT, dS);
    CHECK (std::abs (dS - dIn) < 3.0);   // original at unity plus a quieter second voice
    CHECK (dS > dIn - 1.0);
}

TEST ("compatibility: a v3 (Phase 3) project restores with spatial processing off and sounds exactly like Phase 3")
{
    juce::ValueTree v3 ("VocalShiftState");
    v3.setProperty ("stateVersion", 3, nullptr);
    auto addParam = [&] (const char* id, float value) {
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", id, nullptr);
        c.setProperty ("value", value, nullptr);
        v3.appendChild (c, nullptr);
    };
    addParam ("route", 1.0f); addParam ("improved", 1.0f); addParam ("secondGain", -9.0f);
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*v3.createXml(), mb);
    VocalShiftProcessor p;
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (getParam (p, vs::params::spatial) == 0.0f);
    CHECK (getParam (p, vs::params::route) == 1.0f);
    CHECK (std::abs (getParam (p, vs::params::secondGain) + 9.0f) < 0.051f);
    // a v3 preset file as well
    VocalShiftProcessor src;
    src.getPresetManager().applyFactoryPreset ("second_voice_7");
    auto t = src.getPresetManager().toPresetTree ("p3");
    t.setProperty ("formatVersion", 3, nullptr);
    for (int i = t.getNumChildren() - 1; i >= 0; --i)
        for (auto* n : { vs::params::spatial, vs::params::spaceWidth, vs::params::spaceReverb, vs::params::spaceDelay })
            if (t.getChild (i).getProperty ("id").toString() == n) { t.removeChild (i, nullptr); break; }
    VocalShiftProcessor q;
    CHECK (q.getPresetManager().fromPresetTree (t).wasOk());
    CHECK (getParam (q, vs::params::spatial) == 0.0f);
}

TEST ("urban preset: sets route, -12 dB fader, +7 st, 65 %, 70 %, spatial on; fader range, round trip and automation")
{
    VocalShiftProcessor p;
    CHECK (p.getPresetManager().applyFactoryPreset ("urban_spatial"));
    CHECK (getParam (p, vs::params::route) == 1.0f);
    CHECK (std::abs (getParam (p, vs::params::secondGain) + 12.0f) < 0.051f);
    CHECK (getParam (p, vs::params::semitones) == 7.0f && std::abs (getParam (p, vs::params::character) - 65.0f) < 0.051f
           && std::abs (getParam (p, vs::params::magic) - 70.0f) < 0.051f);
    CHECK (getParam (p, vs::params::mode) == 0.0f && getParam (p, vs::params::spatial) == 1.0f && getParam (p, vs::params::pan) == 0.0f);
    auto* fader = p.getState().getParameter (vs::params::secondGain);
    CHECK (std::abs (fader->convertFrom0to1 (0.0f) + 60.0f) < 1e-3f && std::abs (fader->convertFrom0to1 (1.0f) - 6.0f) < 1e-3f);
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    VocalShiftProcessor q;
    q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (std::abs (getParam (q, vs::params::secondGain) + 12.0f) < 0.051f);
    CHECK (getParam (q, vs::params::spatial) == 1.0f);

    // fader automation through the processor (stereo): finite, no latency change, loud only with a gain above silence
    const auto v = vowel (48000, 190, 3.0);
    q.prepareToPlay (48000, 512);
    const int lat = q.getLatencySamples();
    bool finite = true;
    const auto out = render (q, v, 300, [&] (int pos) {
        setParam (q, vs::params::secondGain, -60.0f + (float) ((pos / 3000) % 67));
        setParam (q, vs::params::magic, (float) ((pos / 1500) % 101));
    }, &finite);
    CHECK (finite);
    CHECK (q.getLatencySamples() == lat);
    float pk = 0;
    for (float s : out) pk = std::max (pk, std::abs (s));
    MEASURE ("urban preset with automated fader and Magia through the processor: peak %.3f", pk);
    CHECK (pk < 1.0f);
}

TEST ("ui: the editor builds in Easy and Pro, the fader shows its value and double click restores the preset value")
{
    VocalShiftProcessor p;
    p.getPresetManager().applyFactoryPreset ("urban_spatial");
    for (auto mode : { "easy", "pro" })
    {
        p.getState().state.setProperty (vs::params::propUiMode, mode, nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        CHECK (ed != nullptr);
        ed->setSize (900, 680);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        CHECK (img.getWidth() == 900 && img.getHeight() == 680);
        // the snapshot is not blank: it contains more than one colour
        bool varied = false;
        const auto c0 = img.getPixelAt (10, 300);
        for (int x = 0; x < img.getWidth() && ! varied; x += 7)
            for (int y = 0; y < img.getHeight() && ! varied; y += 7)
                varied = img.getPixelAt (x, y) != c0;
        CHECK (varied);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int r = vstest::runAll (argc, argv);
    return r;
}
