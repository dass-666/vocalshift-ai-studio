#pragma once

#include <string>
#include <vector>

namespace vs
{

// User-facing voice modes. Both are real processing differences (see MacroMapping.h).
enum class VoiceMode
{
    keepIdentity = 0, // "Conservar identidad": formants mostly preserved, gentle timbre change
    radical = 1       // "Transformación radical": formants follow pitch and shift further, strong timbre change
};

// User-facing state (what the host automates / saves). JUCE-independent.
// The three macros are Pitch (semitones), Character and Magic.
struct VoiceParams
{
    VoiceMode mode = VoiceMode::keepIdentity;
    float semitones = 7.0f;         // Pitch macro, [-24, 24] (reference value +7)
    float fineCents = 0.0f;         // [-100, 100]
    float character = 65.0f;        // Character macro, [0, 100] % (reference value 65)
    float magic = 70.0f;            // Magic macro, [0, 100] % (reference value 70)
    float formantOffset = 0.0f;     // Pro fine-tune added to the macro result, [-12, 12] st
    float brightnessOffset = 0.0f;  // Pro fine-tune added to the macro result, [-12, 12] dB
    float mixPercent = 100.0f;      // [0, 100] %
    float outputGainDb = 0.0f;      // [-24, 12] dB
    bool safetyClip = true;
    bool autoLevel = true;          // automatic loudness compensation of the processed signal

    float totalSemitones() const;   // semitones + cents, clamped to [-24, 24]
};

struct FactoryPreset
{
    std::string id;        // stable id, stored in project state
    std::string name;      // UI name (Spanish, UTF-8)
    std::string description;
    VoiceParams params;
};

const std::vector<FactoryPreset>& factoryPresets();
const FactoryPreset* findFactoryPreset (const std::string& id);

} // namespace vs
