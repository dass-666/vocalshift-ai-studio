#pragma once

#include <string>
#include <vector>

namespace vs
{

// Transformation type. Independent of the voice route below.
enum class VoiceMode
{
    keepIdentity = 0, // "Conservar identidad": formants mostly preserved, gentle timbre change
    radical = 1       // "Transformación radical": formants follow pitch and shift further (DSP only)
};

// Where the processed voice goes.
enum class VoiceRoute
{
    transform = 0,   // the processed signal replaces the voice (dry/wet via Mix)
    secondVoice = 1  // the original stays untouched in front; a processed copy is added behind it
};

// User-facing state (what the host automates / saves). JUCE-independent.
struct VoiceParams
{
    VoiceMode mode = VoiceMode::keepIdentity;
    VoiceRoute route = VoiceRoute::transform;
    float semitones = 7.0f;         // Pitch macro, [-24, 24] (reference value +7)
    float fineCents = 0.0f;         // [-100, 100]
    float character = 65.0f;        // Character macro, [0, 100] % (reference value 65)
    float magic = 70.0f;            // Magic macro, [0, 100] % (reference value 70)
    float formantOffset = 0.0f;     // Pro fine-tune added to the macro result, [-12, 12] st
    float brightnessOffset = 0.0f;  // Pro fine-tune added to the macro result, [-12, 12] dB
    float mixPercent = 100.0f;      // Transform route only, [0, 100] %
    float outputGainDb = 0.0f;      // on the whole output, [-24, 12] dB
    bool safetyClip = true;
    bool autoLevel = true;          // automatic loudness compensation of the processed signal
    // Second-voice route only
    float secondGainDb = -9.0f;     // fader: level of the processed voice (voice + space) re the original, [-60 (silence), +6] dB
    float pan = 0.0f;               // position of the second voice, [-100, 100] (stereo only)
    float offsetMs = 0.0f;          // artistic delay of the second voice, [0, 100] ms
    // Processing generation: true = revised DSP (consonant preservation, de-esser, revised
    // Character mapping, gated level matching). false = exact dec340d behaviour.
    bool improved = true;
    // Phase 4 "Urban Spatial Engine" (second voice only, needs improved): Magia drives width / reverb / slap,
    // Caracter adds body / presence / warmth. false = exact Phase 3 sound.
    bool spatial = true;
    float spaceWidth = 100.0f;      // Pro trims of what Magia asks for, [0, 100] %
    float spaceReverb = 100.0f;
    float spaceDelay = 100.0f;

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
