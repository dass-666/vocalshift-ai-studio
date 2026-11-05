// Plugin-level tests: state, presets, layouts, sample rates / buffer sizes,
// host-like call sequences. Uses the real VocalShiftProcessor.

#include "Signals.h"
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
    setParam (a, vs::params::formantPreservation, 42.5f);
    setParam (a, vs::params::formantShift, 3.25f);
    setParam (a, vs::params::brightness, -5.5f);
    setParam (a, vs::params::mix, 61);
    setParam (a, vs::params::outputGain, -3.5f);
    setParam (a, vs::params::safetyClip, 0);
    setParam (a, vs::params::bypass, 1);
    a.getState().state.setProperty (vs::params::propEngineMode, 1, nullptr);
    a.getState().state.setProperty (vs::params::propUiMode, "pro", nullptr);
    a.getState().state.setProperty (vs::params::propPresetId, "dark_voice", nullptr);

    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    MEASURE ("state size = %d bytes", (int) mb.getSize());

    VocalShiftProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    for (auto* id : { vs::params::semitones, vs::params::fineCents, vs::params::formantPreservation, vs::params::formantShift,
                      vs::params::brightness, vs::params::mix, vs::params::outputGain, vs::params::safetyClip, vs::params::bypass })
        CHECK_MSG (std::abs (getParam (a, id) - getParam (b, id)) < 1e-4f, "%s: %f vs %f", id, getParam (a, id), getParam (b, id));
    CHECK ((int) b.getState().state.getProperty (vs::params::propEngineMode) == 1);
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
        CHECK (std::abs (getParam (p, vs::params::formantPreservation) - fp.params.formantPreservation) < 0.051f);
        CHECK (std::abs (getParam (p, vs::params::formantShift) - fp.params.formantShift) < 0.006f);
        CHECK (std::abs (getParam (p, vs::params::brightness) - fp.params.brightnessDb) < 0.006f);
        CHECK (std::abs (getParam (p, vs::params::outputGain) - fp.params.outputGainDb) < 0.006f);
        CHECK ((int) p.getState().state.getProperty (vs::params::propEngineMode) == (int) fp.mode);
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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int r = vstest::runAll (argc, argv);
    return r;
}
