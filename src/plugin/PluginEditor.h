#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace vs
{

class MeterBar final : public juce::Component
{
public:
    explicit MeterBar (juce::String labelText) : label (std::move (labelText)) {}
    void setLevel (float linearPeak);
    void setClipped (bool c) { if (c != clipped) { clipped = c; repaint(); } }
    std::function<void()> onClipClicked;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClipClicked) onClipClicked(); }

private:
    juce::String label;
    float displayDb = -60.0f;
    bool clipped = false;
};

class VocalShiftEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit VocalShiftEditor (VocalShiftProcessor&);
    ~VocalShiftEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& title, const char* paramId);
    void attachKnob (Knob&, const char* paramId);
    void setUiMode (bool pro);
    void setEngineMode (EngineMode m);
    void refreshEasyPage();
    void rebuildPresetMenu();
    void onPresetChosen();
    void savePreset();
    void loadPreset();
    juce::String statusSummary() const;
    EngineMode engineMode() const;

    VocalShiftProcessor& proc;
    juce::AudioProcessorValueTreeState& state;

    juce::Label title, subtitle;
    juce::TextButton easyTab, proTab, saveButton, loadButton;
    juce::ToggleButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    juce::ComboBox presetBox;

    // Easy page
    juce::TextButton identityButton, morphButton;
    juce::Label modeDescription, status;
    Knob easyIntensity, easySecond, easyMix, easyGain;

    // Pro page
    Knob proSemitones, proFine, proPreserve, proFormant, proBrightness, proMix, proGain;
    juce::ToggleButton safetyButton;
    std::unique_ptr<ButtonAttachment> safetyAttachment;
    juce::Label engineInfo;

    juce::Label roadmap;
    MeterBar inMeter { "IN" }, outMeter { "OUT" };

    bool proMode = false;
    EngineMode shownEngineMode = EngineMode::identityPitch;
    juce::String shownPresetId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresetFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalShiftEditor)
};

} // namespace vs
