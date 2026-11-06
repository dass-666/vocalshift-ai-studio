#pragma once

#include "FactoryPresets.h"

namespace vs
{

// Low-level engine settings. This is the boundary between "what the user asked for"
// (VoiceParams / macros) and "what each DSP block does". Separated concerns:
//   pitch      -> semitones
//   formants   -> formantPreservation + formantShiftSemitones
//   timbre     -> brightnessDb
//   enhancement-> magic01 (compression + air), autoLevel
struct EngineSettings
{
    float semitones = 0.0f;
    float formantPreservation = 1.0f;   // 0..1
    float formantShiftSemitones = 0.0f; // -12..12
    float brightnessDb = 0.0f;          // -12..12
    float magic01 = 0.0f;               // 0..1
    float mix01 = 1.0f;
    float outputGainDb = 0.0f;
    bool safetyClip = true;
    bool autoLevel = false;
};

// Mapping constants (PROVISIONAL: chosen by engineering judgement, not yet
// validated by listening tests). Kept here so they can be tuned in one place.
struct MacroMappingConstants
{
    // Conservar identidad
    static constexpr float identityMaxFollow = 0.25f;     // max fraction of the pitch shift that formants follow
    static constexpr float identityMaxExtraFormant = 1.0f; // st, in the pitch direction
    static constexpr float identityMaxBrightness = 1.5f;   // dB, in the pitch direction
    // Transformación radical
    static constexpr float radicalMaxFollow = 1.0f;
    static constexpr float radicalMaxExtraFormant = 6.0f;
    static constexpr float radicalMaxBrightness = 6.0f;
};

EngineSettings mapMacros (const VoiceParams& p);

} // namespace vs
