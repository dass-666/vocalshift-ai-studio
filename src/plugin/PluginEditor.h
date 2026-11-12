#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace vs
{

// Visual identity "Dark Rose / Neon Violet" (v0.6 redesign). Neon is reserved for what is active or shows a value;
// backgrounds and panels stay dark so long sessions are not tiring.
namespace palette
{
    inline const juce::Colour bg { 0xff090811 };         // window background
    inline const juce::Colour panel { 0xff171322 };      // graphite panels
    inline const juce::Colour panelHi { 0xff201a2e };    // raised surfaces (buttons, boxes)
    inline const juce::Colour panelEdge { 0xff2a2238 };  // 1 px bevels
    inline const juce::Colour violetDeep { 0xff28183d }; // background gradients
    inline const juce::Colour groove { 0xff241c33 };     // knob / fader / meter tracks
    inline const juce::Colour rose { 0xffff4ca8 };       // primary accent
    inline const juce::Colour violet { 0xffa56cff };     // secondary accent
    inline const juce::Colour roseSoft { 0xffffc0df };   // highlights
    inline const juce::Colour text { 0xfff4effa };
    inline const juce::Colour dim { 0xff9a90ae };
    inline const juce::Colour warn { 0xffffb347 };
    inline const juce::Colour red { 0xffff4d5e };
}

// Custom look: hardware-style knobs with a gradient arc and halo, vertical fader with a dB scale, pill switches and
// segmented selectors. Everything is drawn in code (no bitmaps), so it stays sharp at any size and Windows scaling.
class VsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VsLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
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

// Segmented LED meter (vertical or horizontal) with a clip indicator; click clears the clip.
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

// Central identity panel: deep gradient, neon halos and concentric rings that breathe with the output level.
// It reserves a slot for a face image (setFaceImage); without one it draws only the light design.
// The static background is cached in an image, so the 30 Hz animation only repaints the rings.
class IdentityPanel final : public juce::Component
{
public:
    void setLevel (float linearPeak);
    void setFaceImage (juce::Image img) { face = std::move (img); cache = {}; repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Point<float> ringCentre() const;

private:
    // Original holographic singer, drawn in code for VocalShift (not traced from any photo or product).
    // All shapes are authored in a 1000-unit box and mapped to the panel in resized().
    struct Singer
    {
        juce::Path silhouette, frontContour, upperLip, lowerLip, mouth, teeth, brow, eyeLines, eyeFill, jawLine, hairMass;
        std::vector<juce::Path> strands, strandTails; // tails are drawn thinner and fainter so the ends taper
        std::vector<float> strandWidth, strandAlpha;
        juce::Point<float> mouthPoint, cheek, eyeSocket, underJaw;
        float unit = 1.0f; // pixels per design unit
    };
    void buildSinger();
    juce::Point<float> u (float x, float y) const; // design units -> pixels
    void renderBackground();
    juce::Image cache, face;
    Singer singer;
    juce::AffineTransform toPanel;
    float level = 0.0f; // 0..1 smoothed
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
    void nudgePitch (float semis);
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
    void paintPanel (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent) const;

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
    juce::TextButton pitchDownButton, pitchUpButton;     // -12 / +12 semitone shortcuts
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

    // Easy page (three columns: VOZ | identity | PITCH)
    IdentityPanel identity;
    juce::Rectangle<int> easyVoicePanel, easyPitchPanel, pitchKnobArea;

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
