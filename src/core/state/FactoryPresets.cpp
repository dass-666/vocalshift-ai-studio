#include "FactoryPresets.h"

#include <algorithm>

namespace vs
{

float VoiceParams::totalSemitones() const
{
    return std::clamp (semitones + fineCents / 100.0f, -24.0f, 24.0f);
}

static VoiceParams make (VoiceMode mode, float st, float character, float magic,
                         float formantOffset = 0.0f, float brightnessOffset = 0.0f,
                         VoiceRoute route = VoiceRoute::transform)
{
    VoiceParams p;
    p.mode = mode;
    p.route = route;
    p.semitones = st;
    p.character = character;
    p.magic = magic;
    p.formantOffset = formantOffset;
    p.brightnessOffset = brightnessOffset;
    p.spatial = false; // the presets that existed before Phase 4 keep their Phase 3 sound
    return p;
}

static VoiceParams urbanSpatial()
{
    VoiceParams p = make (VoiceMode::keepIdentity, 7, 65, 70, 0, 0, VoiceRoute::secondVoice);
    p.secondGainDb = -12.0f;
    p.spatial = true;
    return p;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    using M = VoiceMode;
    // Every value is a real, implemented parameter. Character/Magic mappings are
    // provisional and must be calibrated with human-voice listening tests.
    static const std::vector<FactoryPreset> presets {
        { "urban_spatial", "Urbano Espacial — Segunda voz", "Tu voz al frente y una segunda voz +7 st a −12 dB: ancha, con reverb y eco corto controlados, cuerpo y presencia. Magia manda el espacio, Carácter el color.",
          urbanSpatial() },

        { "reference", "Referencia (+7 · 65 % · 70 %)", "Valores de referencia del proyecto: +7 st, Carácter 65 %, Magia 70 %, conservando identidad.",
          make (M::keepIdentity, 7, 65, 70) },

        { "second_voice_7", "Segunda voz +7 (referencia)", "Tu voz original al frente y una segunda voz +7 st, 9 dB por debajo, centrada.",
          make (M::keepIdentity, 7, 65, 70, 0, 0, VoiceRoute::secondVoice) },

        { "second_voice_12", "Segunda voz +12 (octava)", "Tu voz original al frente y una segunda voz una octava arriba, 9 dB por debajo.",
          make (M::keepIdentity, 12, 65, 70, 0, 0, VoiceRoute::secondVoice) },

        { "neutral", "Neutro (sin cambio)", "Sin transposición, sin cambio de timbre y sin mejoras.",
          make (M::keepIdentity, 0, 0, 0) },

        { "high_natural", "Agudo natural", "+4 st, timbre casi intacto, mejoras suaves.",
          make (M::keepIdentity, 4, 15, 40) },

        { "high_intense", "Agudo intenso", "+9 st en modo radical con carácter moderado.",
          make (M::radical, 9, 35, 60) },

        { "low_natural", "Grave natural", "-4 st, timbre casi intacto, mejoras suaves.",
          make (M::keepIdentity, -4, 15, 40) },

        { "low_deep", "Grave profundo", "-9 st conservando identidad con carácter alto.",
          make (M::keepIdentity, -9, 80, 60) },

        { "bright_voice", "Voz brillante", "Misma nota; modo radical: formantes y brillo hacia arriba.",
          make (M::radical, 0, 45, 50) },

        { "dark_voice", "Voz oscura", "Misma nota; ajustes finos de formantes -2.5 st y brillo -4 dB.",
          make (M::keepIdentity, 0, 0, 50, -2.5f, -4.0f) },

        { "creative_extreme", "Efecto creativo extremo", "-12 st, modo radical al 100 %: voz de monstruo.",
          make (M::radical, -12, 100, 50) },
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
