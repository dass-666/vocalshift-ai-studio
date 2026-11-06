#pragma once

#include <core/state/FactoryPresets.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace vs::params
{

// Parameter IDs are part of the saved-project format: never rename, only add.
inline constexpr const char* bypass = "bypass";
inline constexpr const char* mode = "mode";             // 0 = conservar identidad, 1 = transformación radical
inline constexpr const char* semitones = "semitones";   // macro "Pitch"
inline constexpr const char* fineCents = "fine";
inline constexpr const char* character = "character";   // macro "Carácter"
inline constexpr const char* magic = "magic";           // macro "Magia"
inline constexpr const char* formantShift = "formantShift"; // Pro: offset added to the macro result
inline constexpr const char* brightness = "brightness";     // Pro: offset added to the macro result
inline constexpr const char* mix = "mix";
inline constexpr const char* outputGain = "outputGain";
inline constexpr const char* safetyClip = "safetyClip";
inline constexpr const char* autoLevel = "autoLevel";

// Every automatable parameter except bypass (presets never touch bypass).
inline constexpr const char* presetParamIds[] = { mode, semitones, fineCents, character, magic, formantShift,
                                                  brightness, mix, outputGain, safetyClip, autoLevel };

// Non-automatable state stored as ValueTree properties.
inline constexpr const char* propUiMode = "uiMode";         // "easy" | "pro"
inline constexpr const char* propPresetId = "presetId";
inline constexpr const char* propStateVersion = "stateVersion";
inline constexpr const char* propUiWidth = "uiWidth";
inline constexpr const char* propUiHeight = "uiHeight";
inline constexpr const char* legacyPropEngineMode = "engineMode"; // v1 only, migrated to the "mode" parameter

// v1: first prototype (formantPreserve parameter, engineMode property)
// v2: macros (character, magic), mode as automatable parameter, autoLevel
inline constexpr int currentStateVersion = 2;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Cached raw pointers for lock-free reads on the audio thread.
struct RawParams
{
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* semitones = nullptr;
    std::atomic<float>* fineCents = nullptr;
    std::atomic<float>* character = nullptr;
    std::atomic<float>* magic = nullptr;
    std::atomic<float>* formantShift = nullptr;
    std::atomic<float>* brightness = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* safetyClip = nullptr;
    std::atomic<float>* autoLevel = nullptr;

    void bind (juce::AudioProcessorValueTreeState& apvts);
    VoiceParams read() const;
    bool isBypassed() const { return bypass->load() >= 0.5f; }
};

// Message thread: write a VoiceParams into the host-visible parameters.
void applyToParameters (juce::AudioProcessorValueTreeState& apvts, const VoiceParams& p);

} // namespace vs::params
