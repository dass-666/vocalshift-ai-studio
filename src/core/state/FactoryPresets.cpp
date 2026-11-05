#include "FactoryPresets.h"

#include <algorithm>

namespace vs
{

float VoiceParams::totalSemitones() const
{
    return std::clamp (semitones + fineCents / 100.0f, -24.0f, 24.0f);
}

static VoiceParams make (float st, float preserve, float formant, float bright, float gainDb = 0.0f)
{
    VoiceParams p;
    p.semitones = st;
    p.formantPreservation = preserve;
    p.formantShift = formant;
    p.brightnessDb = bright;
    p.outputGainDb = gainDb;
    return p;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    // Every value here is a real, implemented parameter. Nothing in a preset
    // depends on unimplemented processing.
    static const std::vector<FactoryPreset> presets {
        { "neutral", "Neutro (sin cambio)", "Punto de partida: sin transposicion ni cambio de timbre.",
          EngineMode::identityPitch, make (0, 100, 0, 0) },

        { "high_natural", "Agudo natural", "+4 semitonos con formantes preservados.",
          EngineMode::identityPitch, make (4, 100, 0, 0) },

        { "high_intense", "Agudo intenso", "+9 semitonos; los formantes siguen parcialmente el tono (60 % preservado).",
          EngineMode::identityPitch, make (9, 60, 0, 1.5f, -1.0f) },

        { "low_natural", "Grave natural", "-4 semitonos con formantes preservados.",
          EngineMode::identityPitch, make (-4, 100, 0, 0) },

        { "low_deep", "Grave profundo", "-9 semitonos, formantes 75 % preservados y desplazados -1.5 st.",
          EngineMode::identityPitch, make (-9, 75, -1.5f, -1.5f) },

        { "bright_voice", "Voz brillante", "Sin cambio de tono: formantes +2.5 st y brillo +4 dB (Morph DSP).",
          EngineMode::voiceMorphDsp, make (0, 100, 2.5f, 4.0f, -1.5f) },

        { "dark_voice", "Voz oscura", "Sin cambio de tono: formantes -2.5 st y brillo -4 dB (Morph DSP).",
          EngineMode::voiceMorphDsp, make (0, 100, -2.5f, -4.0f) },

        { "creative_extreme", "Efecto creativo extremo", "-12 semitonos, formantes siguen el tono y bajan 4 st extra: voz de monstruo.",
          EngineMode::voiceMorphDsp, make (-12, 0, -4.0f, 2.0f) },
    };
    return presets;
}

const FactoryPreset* findFactoryPreset (const std::string& id)
{
    const auto& p = factoryPresets();
    auto it = std::find_if (p.begin(), p.end(), [&] (const FactoryPreset& x) { return x.id == id; });
    return it == p.end() ? nullptr : &*it;
}

} // namespace vs
