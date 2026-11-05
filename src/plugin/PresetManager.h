#pragma once

#include <core/state/FactoryPresets.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace vs
{

// PresetManager: factory presets + user preset files (.vspreset, versioned XML).
// Message thread only (touches files and notifies the host).
class PresetManager
{
public:
    static constexpr const char* fileExtension = ".vspreset";
    static constexpr int presetFormatVersion = 1;

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    bool applyFactoryPreset (const std::string& id);

    // User presets
    static juce::File userPresetDirectory();
    juce::Result saveUserPreset (const juce::File& file, const juce::String& name);
    juce::Result loadUserPreset (const juce::File& file);
    juce::Array<juce::File> listUserPresets() const;

    // Serialisation helpers (public for tests).
    juce::ValueTree toPresetTree (const juce::String& name) const;
    juce::Result fromPresetTree (const juce::ValueTree& tree);

    juce::String currentPresetId() const;

private:
    juce::AudioProcessorValueTreeState& apvts;
};

} // namespace vs
