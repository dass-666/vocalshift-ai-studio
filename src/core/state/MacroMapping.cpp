#include "MacroMapping.h"

#include <algorithm>
#include <cmath>

namespace vs
{

EngineSettings mapMacros (const VoiceParams& p)
{
    using K = MacroMappingConstants;
    EngineSettings e;
    e.semitones = p.totalSemitones();

    const float c = std::clamp (p.character, 0.0f, 100.0f) / 100.0f;
    // Timbre moves in the same direction as the pitch (up = smaller/brighter, down = bigger/darker).
    // With no transposition, Character moves the timbre upwards. Between -1 and 0 st the
    // direction ramps continuously so automating pitch through 0 never jumps the timbre.
    const float s = e.semitones;
    const float dir = s >= 0.0f ? 1.0f : (s <= -1.0f ? -1.0f : 1.0f + 2.0f * s);
    const bool radical = p.mode == VoiceMode::radical;

    if (p.improved)
    {
        // Pure "formants compensated": the spectral envelope moves by exactly `total` semitones.
        const float ratio = radical ? K::radicalFormantRatio : K::identityFormantRatio;
        const float base = radical ? K::radicalFormantBase : K::identityFormantBase;
        const float bright = radical ? K::radicalBrightness : K::identityBrightness;
        const float total = c * (ratio * s + base * dir);
        e.formantPreservation = 1.0f;
        e.formantShiftSemitones = std::clamp (total + p.formantOffset, -12.0f, 12.0f);
        e.brightnessDb = std::clamp (dir * bright * c + p.brightnessOffset, -12.0f, 12.0f);
    }
    else
    {
        const float maxFollow = radical ? K::radicalMaxFollow : K::identityMaxFollow;
        const float maxExtra = radical ? K::radicalMaxExtraFormant : K::identityMaxExtraFormant;
        const float maxBright = radical ? K::radicalMaxBrightness : K::identityMaxBrightness;
        e.formantPreservation = 1.0f - maxFollow * c;
        e.formantShiftSemitones = std::clamp (dir * maxExtra * c + p.formantOffset, -12.0f, 12.0f);
        e.brightnessDb = std::clamp (dir * maxBright * c + p.brightnessOffset, -12.0f, 12.0f);
    }

    e.magic01 = std::clamp (p.magic, 0.0f, 100.0f) / 100.0f;
    e.mix01 = std::clamp (p.mixPercent, 0.0f, 100.0f) / 100.0f;
    e.outputGainDb = std::clamp (p.outputGainDb, -24.0f, 12.0f);
    e.safetyClip = p.safetyClip;
    e.autoLevel = p.autoLevel;
    e.secondVoice = p.route == VoiceRoute::secondVoice;
    e.secondGainDb = std::clamp (p.secondGainDb, -60.0f, 6.0f);
    e.pan = std::clamp (p.pan, -100.0f, 100.0f) / 100.0f;
    e.offsetMs = std::clamp (p.offsetMs, 0.0f, 100.0f);
    e.improved = p.improved;

    if (p.improved && p.spatial && p.route == VoiceRoute::secondVoice)
    {
        const float m = std::clamp (p.magic, 0.0f, 100.0f) / 100.0f;
        const auto trim = [] (float v) { return std::clamp (v, 0.0f, 100.0f) / 100.0f; };
        e.spatialAmount = 1.0f;
        e.spaceWidth = 0.95f * std::pow (m, 1.4f) * trim (p.spaceWidth);
        e.spaceReverb = 0.55f * std::pow (m, 1.5f) * trim (p.spaceReverb);
        e.spaceRt60 = 0.8f + 1.0f * m;
        e.spaceSlap = 0.28f * std::pow (std::clamp ((m - 0.4f) / 0.6f, 0.0f, 1.0f), 1.2f) * trim (p.spaceDelay);
        e.spaceModMs = 0.2f + 0.5f * m;
        e.textureAmount = c;
    }
    return e;
}

} // namespace vs
