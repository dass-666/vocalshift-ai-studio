#pragma once

#include <string>
#include <vector>

namespace vs
{

// Engine-level description of the plugin state, independent of JUCE.
// The JUCE layer maps AudioProcessorValueTreeState parameters onto this.
enum class EngineMode
{
    identityPitch = 0, // change pitch, keep identity (formants preserved)
    voiceMorphDsp = 1  // change timbre/character with DSP (formants + brightness). Not neural.
};

struct VoiceParams
{
    float semitones = 0.0f;           // [-24, 24], integer steps in UI
    float fineCents = 0.0f;           // [-100, 100]
    float formantPreservation = 100.0f; // [0, 100] %
    float formantShift = 0.0f;        // [-12, 12] semitones
    float brightnessDb = 0.0f;        // [-12, 12] dB
    float mixPercent = 100.0f;        // [0, 100] %
    float outputGainDb = 0.0f;        // [-24, 12] dB
    bool safetyClip = true;

    float totalSemitones() const;     // semitones + cents, clamped to [-24, 24]
};

struct FactoryPreset
{
    std::string id;        // stable id, stored in project state
    std::string name;      // UI name (Spanish)
    std::string description;
    EngineMode mode;
    VoiceParams params;
};

const std::vector<FactoryPreset>& factoryPresets();
const FactoryPreset* findFactoryPreset (const std::string& id);

} // namespace vs
