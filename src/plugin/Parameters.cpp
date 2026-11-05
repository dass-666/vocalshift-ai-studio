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

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { semitones, 1 }, "Transposicion",
        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, attrs (" st", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { fineCents, 1 }, "Ajuste fino",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f, attrs (" ct", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { formantPreservation, 1 }, "Preservacion de formantes",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, attrs (" %", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { formantShift, 1 }, "Desplazamiento de formantes",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, attrs (" st", 1)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { brightness, 1 }, "Brillo",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, attrs (" dB", 1)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { mix, 1 }, "Mezcla",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, attrs (" %", 0)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { outputGain, 1 }, "Ganancia de salida",
        NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, attrs (" dB", 1)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { safetyClip, 1 }, "Proteccion de picos", true));

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
    semitones = get (params::semitones);
    fineCents = get (params::fineCents);
    formantPreservation = get (params::formantPreservation);
    formantShift = get (params::formantShift);
    brightness = get (params::brightness);
    mix = get (params::mix);
    outputGain = get (params::outputGain);
    safetyClip = get (params::safetyClip);
}

VoiceParams RawParams::read() const
{
    VoiceParams p;
    p.semitones = semitones->load();
    p.fineCents = fineCents->load();
    p.formantPreservation = formantPreservation->load();
    p.formantShift = formantShift->load();
    p.brightnessDb = brightness->load();
    p.mixPercent = mix->load();
    p.outputGainDb = outputGain->load();
    p.safetyClip = safetyClip->load() >= 0.5f;
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
    set (semitones, p.semitones);
    set (fineCents, p.fineCents);
    set (formantPreservation, p.formantPreservation);
    set (formantShift, p.formantShift);
    set (brightness, p.brightnessDb);
    set (mix, p.mixPercent);
    set (outputGain, p.outputGainDb);
    set (safetyClip, p.safetyClip ? 1.0f : 0.0f);
}

} // namespace vs::params
