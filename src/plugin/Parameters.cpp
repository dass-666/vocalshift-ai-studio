#include "Parameters.h"

namespace vs::params
{

using namespace juce;

static String fmt (float v, int decimals, const char* unit)
{
    return String (v, decimals) + unit;
}

AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto attrs = [] (const char* unit, int decimals) {
        return AudioParameterFloatAttributes()
            .withLabel (unit)
            .withStringFromValueFunction ([decimals, unit] (float v, int) { return fmt (v, decimals, unit); });
    };
    const VoiceParams defaults; // reference values: +7 st, Character 65 %, Magic 70 %

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { mode, 2 }, "Modo",
        StringArray { String::fromUTF8 ("Conservar identidad"), String::fromUTF8 ("Transformación radical") },
        (int) defaults.mode));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { semitones, 1 }, "Pitch",
        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), defaults.semitones, attrs (" st", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { fineCents, 1 }, "Ajuste fino",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), defaults.fineCents, attrs (" ct", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { character, 2 }, String::fromUTF8 ("Carácter"),
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.character, attrs (" %", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { magic, 2 }, "Magia",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.magic, attrs (" %", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { formantShift, 1 }, "Ajuste formantes",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), defaults.formantOffset, attrs (" st", 1)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { brightness, 1 }, "Ajuste brillo",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), defaults.brightnessOffset, attrs (" dB", 1)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mix, 1 }, "Mezcla",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaults.mixPercent, attrs (" %", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { outputGain, 1 }, "Ganancia de salida",
        NormalisableRange<float> (-24.0f, 12.0f, 0.01f), defaults.outputGainDb, attrs (" dB", 1)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { safetyClip, 1 }, String::fromUTF8 ("Protección de picos"), defaults.safetyClip));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { autoLevel, 2 }, String::fromUTF8 ("Nivel automático"), defaults.autoLevel));

    return layout;
}

void RawParams::bind (AudioProcessorValueTreeState& s)
{
    auto get = [&] (const char* id) {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };
    bypass = get (params::bypass);
    mode = get (params::mode);
    semitones = get (params::semitones);
    fineCents = get (params::fineCents);
    character = get (params::character);
    magic = get (params::magic);
    formantShift = get (params::formantShift);
    brightness = get (params::brightness);
    mix = get (params::mix);
    outputGain = get (params::outputGain);
    safetyClip = get (params::safetyClip);
    autoLevel = get (params::autoLevel);
}

VoiceParams RawParams::read() const
{
    VoiceParams p;
    p.mode = mode->load() >= 0.5f ? VoiceMode::radical : VoiceMode::keepIdentity;
    p.semitones = semitones->load();
    p.fineCents = fineCents->load();
    p.character = character->load();
    p.magic = magic->load();
    p.formantOffset = formantShift->load();
    p.brightnessOffset = brightness->load();
    p.mixPercent = mix->load();
    p.outputGainDb = outputGain->load();
    p.safetyClip = safetyClip->load() >= 0.5f;
    p.autoLevel = autoLevel->load() >= 0.5f;
    return p;
}

void applyToParameters (AudioProcessorValueTreeState& s, const VoiceParams& p)
{
    auto set = [&] (const char* id, float value) {
        if (auto* param = s.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    set (mode, (float) (int) p.mode);
    set (semitones, p.semitones);
    set (fineCents, p.fineCents);
    set (character, p.character);
    set (magic, p.magic);
    set (formantShift, p.formantOffset);
    set (brightness, p.brightnessOffset);
    set (mix, p.mixPercent);
    set (outputGain, p.outputGainDb);
    set (safetyClip, p.safetyClip ? 1.0f : 0.0f);
    set (autoLevel, p.autoLevel ? 1.0f : 0.0f);
}

} // namespace vs::params
