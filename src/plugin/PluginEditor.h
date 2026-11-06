#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace vs
{

// Palette: graphite / black with electric blue, violet and teal accents.
namespace palette
{
    inline const juce::Colour bg { 0xff0d0f14 };
    inline const juce::Colour panel { 0xff161a22 };
    inline const juce::Colour panelHi { 0xff1d222c };
    inline const juce::Colour groove { 0xff262c38 };
    inline const juce::Colour text { 0xffeceff5 };
    inline const juce::Colour dim { 0xff8890a2 };
    inline const juce::Colour blue { 0xff3d7bff };
    inline const juce::Colour violet { 0xff9b6bff };
    inline const juce::Colour teal { 0xff2fd4c4 };
    inline const juce::Colour amber { 0xffffb347 };
    inline const juce::Colour red { 0xffe5484d };
}

// Custom look: ring knobs with a progress arc, vertical fader with a dB scale, flat toggles.
class VsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VsLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
};

// A slider whose double click can be redirected (the fader restores the preset's value, knobs the parameter default).
class ResettableSlider final : public juce::Slider
{
public:
    using juce::Slider::Slider;
    std::function<bool()> onDoubleClickReset; // return true if handled
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (onDoubleClickReset && onDoubleClickReset())
            return;
        juce::Slider::mouseDoubleClick (e);
    }
};

class MeterBar final : public juce::Component
{
public:
    explicit MeterBar (juce::String labelText, bool horizontalBar = false)
        : label (std::move (labelText)), horizontal (horizontalBar) {}
    void setLevel (float linearPeak);
    void setClipped (bool c) { if (c != clipped) { clipped = c; repaint(); } }
    std::function<void()> onClipClicked;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClipClicked) onClipClicked(); }

private:
    juce::String label;
    bool horizontal = false;
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

    // Test hooks
    juce::Slider& getFader() { return fader; }
    juce::String getFaderText() { return fader.getTextFromValue (fader.getValue()); }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        ResettableSlider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label, hint;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& title, const juce::String& hint, const char* paramId, bool big, juce::Colour colour);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<ButtonAttachment>&, const juce::String& text, const char* paramId);
    void setUiMode (bool pro);
    void setVoiceMode (VoiceMode m);
    void setRoute (VoiceRoute r);
    VoiceRoute route() const;
    void refreshModeButtons();
    void layoutVisibility();
    void rebuildPresetMenu();
    void syncPresetBox();
    void onPresetChosen();
    void savePreset();
    void loadPreset();
    bool restoreFaderToPreset();
    juce::String effectiveSummary() const;
    VoiceMode voiceMode() const;

    VocalShiftProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    VsLookAndFeel look;

    juce::Label title, subtitle;
    juce::TextButton easyTab, proTab, saveButton, loadButton;
    juce::ToggleButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    juce::ComboBox presetBox;

    juce::TextButton identityButton, radicalButton;
    juce::TextButton transformButton, secondVoiceButton; // route selector (parameter "route")
    juce::Label modeDescription, status;

    // The three macros (Easy and Pro share them) and the second-voice fader
    Knob pitchKnob, characterKnob, magicKnob;
    ResettableSlider fader { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    std::unique_ptr<SliderAttachment> faderAttachment;
    juce::Label faderLabel;
    MeterBar secondMeter { juce::String::fromUTF8 ("2.ª") };

    // Pro page
    Knob proFine, proFormant, proBrightness, proMix, proGain, proPan, proOffset, proWidth, proReverb, proDelay;
    juce::ToggleButton safetyButton, autoLevelButton, improvedButton, spatialButton, engineButton;
    std::unique_ptr<ButtonAttachment> safetyAttachment, autoLevelAttachment, improvedAttachment, spatialAttachment, engineAttachment;
    juce::Label engineInfo;
    juce::Rectangle<int> proPanels[6], faderStrip;
    const char* proPanelTitles[6] = { "PITCH", "TIMBRE", "DINÁMICA", "ESPACIO", "MEZCLA", "CALIDAD Y DIAGNÓSTICO" };

    juce::Label roadmap;
    MeterBar inMeter { "IN", true }, outMeter { "OUT", true };

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
