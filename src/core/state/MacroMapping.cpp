#include "MacroMapping.h"

#include <algorithm>

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
    const float maxFollow = radical ? K::radicalMaxFollow : K::identityMaxFollow;
    const float maxExtra = radical ? K::radicalMaxExtraFormant : K::identityMaxExtraFormant;
    const float maxBright = radical ? K::radicalMaxBrightness : K::identityMaxBrightness;

    e.formantPreservation = 1.0f - maxFollow * c;
    e.formantShiftSemitones = std::clamp (dir * maxExtra * c + p.formantOffset, -12.0f, 12.0f);
    e.brightnessDb = std::clamp (dir * maxBright * c + p.brightnessOffset, -12.0f, 12.0f);

    e.magic01 = std::clamp (p.magic, 0.0f, 100.0f) / 100.0f;
    e.mix01 = std::clamp (p.mixPercent, 0.0f, 100.0f) / 100.0f;
    e.outputGainDb = std::clamp (p.outputGainDb, -24.0f, 12.0f);
    e.safetyClip = p.safetyClip;
    e.autoLevel = p.autoLevel;
    return e;
}

} // namespace vs
