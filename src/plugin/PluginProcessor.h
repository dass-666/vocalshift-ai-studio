#pragma once

#include "Parameters.h"
#include "PresetManager.h"

#include <core/VocalChain.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace vs
{

// PluginHost layer: VST3 integration, parameters, state, latency, metering.
class VocalShiftProcessor final : public juce::AudioProcessor
{
public:
    VocalShiftProcessor();
    ~VocalShiftProcessor() override = default;

    const juce::String getName() const override { return "VocalShift AI Studio"; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;         // double version unused (single precision only)
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; } // reverb tail (RT60 up to 2 s)

    // Factory presets are handled by PresetManager / the editor, not host programs.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() { return apvts; }
    PresetManager& getPresetManager() { return presets; }

    // Metering (audio thread writes, UI thread reads and clears).
    float consumeInputPeak() { return inputPeak.exchange (0.0f); }
    float consumeOutputPeak() { return outputPeak.exchange (0.0f); }
    float consumeSecondVoicePeak() { return secondPeak.exchange (0.0f); }
    bool inputClipped() const { return inputClip.load(); }
    bool outputClipped() const { return outputClip.load(); }
    void clearClipIndicators() { inputClip = false; outputClip = false; }

    int getEngineLatencySamples() const { return latencySamples.load(); }
    double getPreparedSampleRate() const { return preparedRate.load(); }
    const char* getEngineName() const { return chain.engineName(); }
    int getNonFiniteEvents() const { return chain.nonFiniteEvents(); }
    bool getSpatialActive() const { return chain.spatialActive(); }
    float getAutoLevelGainDb() const { return chain.autoLevelGainDb(); }
    VoiceParams getCurrentVoiceParams() const { return raw.read(); }

    // State migration hook (public for tests).
    static juce::ValueTree migrateState (juce::ValueTree state);

private:
    void runChain (juce::AudioBuffer<float>& buffer, bool forceBypass);

    juce::AudioProcessorValueTreeState apvts;
    params::RawParams raw;
    PresetManager presets;
    VocalChain chain;

    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, secondPeak { 0.0f };
    std::atomic<bool> inputClip { false }, outputClip { false };
    std::atomic<int> latencySamples { 0 };
    std::atomic<double> preparedRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalShiftProcessor)
};

} // namespace vs
