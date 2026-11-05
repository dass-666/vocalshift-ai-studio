#pragma once

#include <core/state/FactoryPresets.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace vs::params
{

// Parameter IDs are part of the saved-project format: never rename, only add.
inline constexpr const char* bypass = "bypass";
inline constexpr const char* semitones = "semitones";
inline constexpr const char* fineCents = "fine";
inline constexpr const char* formantPreservation = "formantPreserve";
inline constexpr const char* formantShift = "formantShift";
inline constexpr const char* brightness = "brightness";
inline constexpr const char* mix = "mix";
inline constexpr const char* outputGain = "outputGain";
inline constexpr const char* safetyClip = "safetyClip";

// Non-automatable state stored as ValueTree properties.
inline constexpr const char* propEngineMode = "engineMode"; // int: vs::EngineMode
inline constexpr const char* propUiMode = "uiMode";         // "easy" | "pro"
inline constexpr const char* propPresetId = "presetId";
inline constexpr const char* propStateVersion = "stateVersion";
inline constexpr const char* propUiWidth = "uiWidth";
inline constexpr const char* propUiHeight = "uiHeight";

inline constexpr int currentStateVersion = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Cached raw pointers for lock-free reads on the audio thread.
struct RawParams
{
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* semitones = nullptr;
    std::atomic<float>* fineCents = nullptr;
    std::atomic<float>* formantPreservation = nullptr;
    std::atomic<float>* formantShift = nullptr;
    std::atomic<float>* brightness = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* safetyClip = nullptr;

    void bind (juce::AudioProcessorValueTreeState& apvts);
    VoiceParams read() const;
    bool isBypassed() const { return bypass->load() >= 0.5f; }
};

// Message thread: write a VoiceParams into the host-visible parameters.
void applyToParameters (juce::AudioProcessorValueTreeState& apvts, const VoiceParams& p);

} // namespace vs::params
