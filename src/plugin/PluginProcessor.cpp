#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace vs
{

using namespace juce;

VocalShiftProcessor::VocalShiftProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalShiftState", params::createLayout()),
      presets (apvts)
{
    raw.bind (apvts);
    apvts.state.setProperty (params::propStateVersion, params::currentStateVersion, nullptr);
    apvts.state.setProperty (params::propUiMode, "easy", nullptr);
    apvts.state.setProperty (params::propPresetId, "reference", nullptr); // defaults == "reference" preset
}

bool VocalShiftProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void VocalShiftProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = jmax (1, getTotalNumOutputChannels());
    // Set targets first: prepare() snaps smoothers to them (no ramp from defaults).
    chain.setParams (raw.read(), raw.isBypassed());
    chain.prepare (sampleRate, channels, jmax (32, samplesPerBlock));
    latencySamples = chain.latencySamples();
    preparedRate = sampleRate;
    setLatencySamples (chain.latencySamples());
}

void VocalShiftProcessor::reset()
{
    chain.reset();
}

void VocalShiftProcessor::runChain (AudioBuffer<float>& buffer, bool forceBypass)
{
    ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int n = buffer.getNumSamples();
    if (n == 0)
        return;

    chain.setParams (raw.read(), forceBypass || raw.isBypassed());
    chain.process (buffer.getArrayOfWritePointers(), jmin (numOut, buffer.getNumChannels()), n);

    const float inP = chain.lastInputPeak();
    const float outP = chain.lastOutputPeak();
    if (inP > inputPeak.load()) inputPeak.store (inP);
    if (outP > outputPeak.load()) outputPeak.store (outP);
    if (inP >= 1.0f) inputClip.store (true);
    if (outP >= 0.999f) outputClip.store (true);
}

void VocalShiftProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    runChain (buffer, false);
}

void VocalShiftProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer&)
{
    // Keep processing so latency stays constant and un-bypassing is click-free.
    runChain (buffer, true);
}

AudioProcessorParameter* VocalShiftProcessor::getBypassParameter() const
{
    return apvts.getParameter (params::bypass);
}

AudioProcessorEditor* VocalShiftProcessor::createEditor()
{
    return new VocalShiftEditor (*this);
}

void VocalShiftProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (params::propStateVersion, params::currentStateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

ValueTree VocalShiftProcessor::migrateState (ValueTree state)
{
    const int version = state.getProperty (params::propStateVersion, 0);
    if (version < 2)
    {
        // v1 -> v2: "engineMode" property became the automatable "mode" parameter;
        // "formantPreserve" was replaced by the Character macro (dropped: no shipped users).
        const int legacyMode = state.getProperty (params::legacyPropEngineMode, 0);
        bool found = false;
        for (auto child : state)
            if (child.hasType ("PARAM") && child.getProperty ("id").toString() == params::mode)
                found = true;
        if (! found)
        {
            ValueTree p ("PARAM");
            p.setProperty ("id", params::mode, nullptr);
            p.setProperty ("value", legacyMode == 1 ? 1.0f : 0.0f, nullptr);
            state.appendChild (p, nullptr);
        }
        state.removeProperty (params::legacyPropEngineMode, nullptr);
    }
    if (version < 3)
    {
        // v2 -> v3: projects saved before the second voice existed must sound as they did:
        // transform route and the previous DSP (the new parameters are absent from the old state, and
        // their defaults are "improved on", so they are written explicitly).
        auto ensure = [&] (const char* id, float value) {
            for (auto child : state)
                if (child.hasType ("PARAM") && child.getProperty ("id").toString() == id)
                    return;
            ValueTree p ("PARAM");
            p.setProperty ("id", id, nullptr);
            p.setProperty ("value", value, nullptr);
            state.appendChild (p, nullptr);
        };
        ensure (params::improved, 0.0f);
        ensure (params::route, 0.0f);
    }
    state.setProperty (params::propStateVersion, params::currentStateVersion, nullptr);
    return state;
}

void VocalShiftProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return; // unknown / corrupt state: keep current values

    auto tree = ValueTree::fromXml (*xml);
    const int version = tree.getProperty (params::propStateVersion, 0);
    if (version > params::currentStateVersion)
        DBG ("VocalShift: state version " << version << " is newer than supported; loading known parameters");
    // Parameters missing from an old state keep their defaults (APVTS behaviour).
    apvts.replaceState (migrateState (tree));
}

} // namespace vs
