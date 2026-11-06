#pragma once

#include "FactoryPresets.h"

namespace vs
{

// Low-level engine settings. This is the boundary between "what the user asked for"
// (VoiceParams / macros) and "what each DSP block does". Separated concerns:
//   pitch      -> semitones
//   formants   -> formantPreservation + formantShiftSemitones
//   timbre     -> brightnessDb
//   enhancement-> magic01 (compression + air + de-esser), autoLevel
//   routing    -> secondVoice, mix01 / secondGainDb / pan / offsetMs
struct EngineSettings
{
    float semitones = 0.0f;
    float formantPreservation = 1.0f;   // 0..1
    float formantShiftSemitones = 0.0f; // -12..12
    float brightnessDb = 0.0f;          // -12..12
    float magic01 = 0.0f;               // 0..1
    float mix01 = 1.0f;                 // transform route
    float outputGainDb = 0.0f;
    bool safetyClip = true;
    bool autoLevel = false;
    // Routing
    bool secondVoice = false;
    float secondGainDb = -9.0f;         // relative to the (unity-gain) original
    float pan = 0.0f;                   // -1..1, second voice only
    float offsetMs = 0.0f;              // 0..100, second voice only
    // Revised DSP (consonant preservation, de-esser, gated level matching)
    bool improved = false;
    // Phase 4: Urban Spatial Engine + Caracter texture (second voice, improved, spatial). All 0 = off.
    float spatialAmount = 0.0f;    // 0 or 1 (route / switch), smoothed downstream
    float spaceWidth = 0.0f;       // 0..0.95
    float spaceReverb = 0.0f;      // linear return, 0..0.6
    float spaceRt60 = 1.2f;        // s
    float spaceSlap = 0.0f;        // linear return, 0..0.3
    float spaceModMs = 0.4f;       // ms
    float textureAmount = 0.0f;    // 0..1 (Caracter) -> VoiceTexture
    int pitchEngine = 0;           // Phase 5: 0 = Signalsmith (Phase 4 reference), 1 = experimental PSOLA
};

// Mapping constants (PROVISIONAL: chosen by engineering judgement, not yet
// validated by listening tests). Kept here so they can be tuned in one place.
struct MacroMappingConstants
{
    // --- Legacy mapping (dec340d, used when improved = false) -----------------------------
    // Conservar identidad
    static constexpr float identityMaxFollow = 0.25f;     // max fraction of the pitch shift that formants follow
    static constexpr float identityMaxExtraFormant = 1.0f; // st, in the pitch direction
    static constexpr float identityMaxBrightness = 1.5f;   // dB, in the pitch direction
    // Transformación radical
    static constexpr float radicalMaxFollow = 1.0f;
    static constexpr float radicalMaxExtraFormant = 6.0f;
    static constexpr float radicalMaxBrightness = 6.0f;

    // --- Revised mapping (improved = true) --------------------------------------------------
    // Total formant shift (st) = Character * ( ratio * pitchSemitones + base * direction )
    // so the timbre change scales with the size of the pitch interval instead of being constant.
    static constexpr float identityFormantRatio = 0.25f;  // formants move 25 % of the interval at Character 100 %
    static constexpr float identityFormantBase = 0.4f;    // st at Character 100 % even with 0 st pitch
    static constexpr float identityBrightness = 1.0f;     // dB at Character 100 %
    static constexpr float radicalFormantRatio = 0.7f;    // < 1: formants never move further than the pitch
    static constexpr float radicalFormantBase = 2.0f;
    static constexpr float radicalBrightness = 2.5f;
};

// Magia (second voice, spatial): m = Magia / 100, perceptual curves (not linear):
//   width   = 0.95 m^1.4          30 % -> 0.18   70 % -> 0.58   100 % -> 0.95
//   reverb  = 0.55 m^1.5          30 % -> 0.09   70 % -> 0.32   100 % -> 0.55  (linear return, re the second voice)
//   rt60    = 0.8 + 1.0 m  s
//   slap    = 0.28 * clamp((m - 0.4) / 0.6)^1.2   (off below 40 %)   70 % -> 0.12   100 % -> 0.28
//   mod     = 0.2 + 0.5 m  ms
//   Magia 0 -> all zero (no spatial processing at all).  Pro trims scale width / reverb / slap.
// Revised or legacy mapping depending on p.improved.
EngineSettings mapMacros (const VoiceParams& p);

} // namespace vs
