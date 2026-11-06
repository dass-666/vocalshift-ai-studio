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

static VoiceParams urbanSpatialPsola()
{
    VoiceParams p = urbanSpatial();
    p.engine = 1;
    return p;
}

// Fase 5 presets: second voice with the Phase 4 spatial engine and distinct settings. All of them are
// engineering starting points, not tuned by ear (see docs/FASE5_NATURAL_VOICE.md).
static VoiceParams spatialSecond (float st, float character, float magic, float gainDb, float offsetMs,
                                  float formantOffset = 0.0f, float brightnessOffset = 0.0f, float pan = 0.0f)
{
    VoiceParams p = make (VoiceMode::keepIdentity, st, character, magic, formantOffset, brightnessOffset, VoiceRoute::secondVoice);
    p.secondGainDb = gainDb;
    p.offsetMs = offsetMs;
    p.pan = pan;
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

        { "second_voice_natural", "Segunda Voz Natural", "Segunda voz +7 st más discreta (−14 dB), poco espacio, desfase de 18 ms y color suave. Punto de partida, sin ajustar al oído.",
          spatialSecond (7, 30, 40, -14.0f, 18.0f) },
        { "melodic_rap", "Rap Melódico", "Segunda voz una octava arriba (−11 dB), eco corto presente y 25 ms de desfase.",
          spatialSecond (12, 50, 60, -11.0f, 25.0f, 0.0f, 1.0f) },
        { "pop_bright", "Pop Brillante", "Segunda voz +12 st con brillo extra (+2 dB) y espacio amplio.",
          spatialSecond (12, 40, 65, -10.0f, 12.0f, 0.5f, 2.0f) },
        { "dark_vocal", "Dark Vocal", "Segunda voz −5 st, formantes −1.5 st y brillo −3 dB: más grave y cerrada.",
          spatialSecond (-5, 70, 50, -10.0f, 20.0f, -1.5f, -3.0f) },
        { "rock_texture", "Rock Texturizado", "Segunda voz +7 st con Carácter al máximo (cuerpo, presencia, saturación) y poco espacio.",
          spatialSecond (7, 100, 30, -9.0f, 10.0f) },
        { "dream_atmosphere", "Dream Atmosphere", "Segunda voz +12 st a −8 dB con Magia al máximo: ancha, mucha reverb.",
          spatialSecond (12, 30, 100, -8.0f, 30.0f) },
        { "high_creative", "Agudo Creativo", "Segunda voz +19 st (−13 dB) en modo creativo: efecto, no una armonía natural.",
          spatialSecond (19, 60, 70, -13.0f, 15.0f, 1.0f, 2.0f) },
        { "exp_psola_second_voice", "[EXPERIMENTAL] Segunda voz +7 con motor PSOLA", "Igual que Urbano Espacial pero con el motor PSOLA experimental. Sin validar al oído: no usar en producción.",
          urbanSpatialPsola() },

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
