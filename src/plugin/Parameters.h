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
// v3: second voice and revised DSP
inline constexpr const char* route = "route";           // 0 = transformar, 1 = segunda voz
inline constexpr const char* secondGain = "secondGain"; // Pro: second voice level relative to the original (dB)
inline constexpr const char* pan = "pan";               // Pro: second voice balance (-100..100)
inline constexpr const char* offsetMs = "offsetMs";     // Pro: artistic delay of the second voice (0..100 ms)
inline constexpr const char* improved = "improved";
// v4: Urban Spatial Engine
inline constexpr const char* spatial = "spatial";           // Pro: spatial processing + Caracter texture on the second voice
inline constexpr const char* spaceWidth = "spaceWidth";     // Pro trims (percent of what Magia asks for)
inline constexpr const char* spaceReverb = "spaceReverb";
inline constexpr const char* spaceDelay = "spaceDelay";     // Pro: revised DSP (off = exact behaviour of earlier versions)

// Every automatable parameter except bypass (presets never touch bypass).
inline constexpr const char* presetParamIds[] = { mode, semitones, fineCents, character, magic, formantShift,
                                                  brightness, mix, outputGain, safetyClip, autoLevel,
                                                  route, secondGain, pan, offsetMs, improved,
                                                  spatial, spaceWidth, spaceReverb, spaceDelay };

// Non-automatable state stored as ValueTree properties.
inline constexpr const char* propUiMode = "uiMode";         // "easy" | "pro"
inline constexpr const char* propPresetId = "presetId";
inline constexpr const char* propStateVersion = "stateVersion";
inline constexpr const char* propUiWidth = "uiWidth";
inline constexpr const char* propUiHeight = "uiHeight";
inline constexpr const char* legacyPropEngineMode = "engineMode"; // v1 only, migrated to the "mode" parameter

// v1: first prototype (formantPreserve parameter, engineMode property)
// v2: macros (character, magic), mode as automatable parameter, autoLevel
// v3: second voice (route, secondGain, pan, offsetMs) and the revised-DSP switch (improved).
//     States and presets older than v3 are migrated with route = transform and improved = off,
//     so they sound exactly like they did.
// v4: Urban Spatial Engine (spatial, spaceWidth, spaceReverb, spaceDelay); the second-voice fader range grew to
//     -60 (silence) .. +6 dB. States and presets older than v4 are migrated with spatial = off (Phase 3 sound).
inline constexpr int currentStateVersion = 4;

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
    std::atomic<float>* route = nullptr;
    std::atomic<float>* secondGain = nullptr;
    std::atomic<float>* pan = nullptr;
    std::atomic<float>* offsetMs = nullptr;
    std::atomic<float>* improved = nullptr;
    std::atomic<float>* spatial = nullptr;
    std::atomic<float>* spaceWidth = nullptr;
    std::atomic<float>* spaceReverb = nullptr;
    std::atomic<float>* spaceDelay = nullptr;

    void bind (juce::AudioProcessorValueTreeState& apvts);
    VoiceParams read() const;
    bool isBypassed() const { return bypass->load() >= 0.5f; }
};

// Message thread: write a VoiceParams into the host-visible parameters.
void applyToParameters (juce::AudioProcessorValueTreeState& apvts, const VoiceParams& p);

} // namespace vs::params
