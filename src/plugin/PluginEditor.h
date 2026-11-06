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
        juce::Label label, hint;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& title, const juce::String& hint, const char* paramId, bool big);
    void setUiMode (bool pro);
    void setVoiceMode (VoiceMode m);
    void setRoute (VoiceRoute r);
    VoiceRoute route() const;
    void refreshModeButtons();
    void rebuildPresetMenu();
    void syncPresetBox();
    void onPresetChosen();
    void savePreset();
    void loadPreset();
    juce::String effectiveSummary() const;
    VoiceMode voiceMode() const;

    VocalShiftProcessor& proc;
    juce::AudioProcessorValueTreeState& state;

    juce::Label title, subtitle;
    juce::TextButton easyTab, proTab, saveButton, loadButton;
    juce::ToggleButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    juce::ComboBox presetBox;

    // Shared: voice mode selector (parameter "mode")
    juce::TextButton identityButton, radicalButton;
    juce::TextButton transformButton, secondVoiceButton; // route selector (parameter "route")
    juce::Label modeDescription, status;

    // Easy page: the three macros + mix/gain
    Knob easyPitch, easyCharacter, easyMagic; // exactly the three macros (+ the route selector)

    // Pro page
    Knob proPitch, proFine, proCharacter, proMagic, proFormant, proBrightness, proMix, proGain;
    Knob proSecondGain, proPan, proOffset;
    juce::ToggleButton safetyButton, autoLevelButton, improvedButton;
    std::unique_ptr<ButtonAttachment> safetyAttachment, autoLevelAttachment, improvedAttachment;
    juce::Label engineInfo;

    juce::Label roadmap;
    MeterBar inMeter { "IN" }, outMeter { "OUT" };

    bool proMode = false;
    VoiceMode shownVoiceMode = VoiceMode::keepIdentity;
    VoiceRoute shownRoute = VoiceRoute::transform;
    bool shownImproved = true;
    juce::String shownPresetId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresetFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalShiftEditor)
};

} // namespace vs
