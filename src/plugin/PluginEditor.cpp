#include "PluginEditor.h"

namespace vs
{

using namespace juce;
namespace pal = palette;

static String u8s (const char* s) { return String::fromUTF8 (s); }

static constexpr int factoryIdOffset = 1;
static constexpr int userIdOffset = 1000;

// ---------------------------------------------------------------------------
VsLookAndFeel::VsLookAndFeel()
{
    setColour (Slider::rotarySliderFillColourId, pal::blue);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxTextColourId, pal::text);
    setColour (Label::textColourId, pal::text);
    setColour (ComboBox::backgroundColourId, pal::panelHi);
    setColour (ComboBox::textColourId, pal::text);
    setColour (ComboBox::outlineColourId, pal::groove);
    setColour (PopupMenu::backgroundColourId, pal::panel);
    setColour (PopupMenu::textColourId, pal::text);
    setColour (PopupMenu::highlightedBackgroundColourId, pal::blue.darker (0.3f));
    setColour (TextButton::buttonColourId, pal::panelHi);
    setColour (TextButton::textColourOffId, pal::text);
    setColour (TextButton::textColourOnId, pal::text);
}

Font VsLookAndFeel::getTextButtonFont (TextButton&, int height) { return FontOptions ((float) jmin (15, height - 8), Font::plain); }

void VsLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& colour, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = colour;
    if (highlighted) c = c.brighter (0.08f);
    if (down) c = c.darker (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (pal::groove.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void VsLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float diameter = jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre (diameter, diameter).reduced (diameter * 0.06f);
    const auto centre = area.getCentre();
    const float radius = area.getWidth() * 0.5f;
    const float ring = jmax (3.0f, radius * 0.11f);
    const auto colour = s.findColour (Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();

    // knob body: soft vertical gradient, subtle rim (depth without reflections)
    const float bodyR = radius - ring * 1.9f;
    g.setGradientFill (ColourGradient (pal::panelHi.brighter (0.05f), centre.x, centre.y - bodyR, pal::panel.darker (0.3f), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2, 1.2f);

    // track + progress arc
    const float arcR = radius - ring * 0.5f;
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (pal::groove);
    g.strokePath (track, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));
    const float angle = startAngle + pos * (endAngle - startAngle);
    Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (enabled ? colour : colour.withAlpha (0.25f));
    g.strokePath (value, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));
    if (enabled && s.isMouseOverOrDragging())
    {
        g.setColour (colour.withAlpha (0.18f));
        g.strokePath (value, PathStrokeType (ring * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // pointer
    Path pointer;
    const float pl = bodyR * 0.62f;
    pointer.addRoundedRectangle (-1.8f, -bodyR + 4.0f, 3.6f, pl, 1.8f);
    g.setColour (enabled ? pal::text : pal::dim);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void VsLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto colour = s.findColour (Slider::rotarySliderFillColourId);
    const float cx = (float) x + (float) w * 0.5f;
    const float grooveW = 8.0f;
    const float top = (float) y + 6.0f, bottom = (float) (y + h) - 6.0f;
    auto groove = Rectangle<float> (cx - grooveW * 0.5f, top, grooveW, bottom - top);

    // dB scale: ticks drawn at fixed dB values converted through the slider's own mapping
    g.setFont (FontOptions (10.5f));
    for (double db : { 6.0, 0.0, -12.0, -24.0, -36.0, -60.0 })
    {
        const float p = (float) s.valueToProportionOfLength (db);
        const float ty = bottom - p * (bottom - top);
        const bool major = db == 0.0 || db == -12.0 || db == -60.0;
        g.setColour (db == -12.0 ? colour.withAlpha (0.9f) : pal::dim.withAlpha (major ? 0.9f : 0.55f));
        g.drawLine (cx + grooveW * 0.5f + 5.0f, ty, cx + grooveW * 0.5f + (major ? 14.0f : 10.0f), ty, 1.0f);
        const String label = db <= -60.0 ? String::fromUTF8 ("−∞") : (db > 0 ? "+" : (db < 0 ? String::fromUTF8 ("−") : String())) + String ((int) std::abs (db));
        g.drawText (label, Rectangle<float> (cx + grooveW * 0.5f + 16.0f, ty - 7.0f, 30.0f, 14.0f), Justification::centredLeft);
    }

    g.setColour (pal::groove);
    g.fillRoundedRectangle (groove, grooveW * 0.5f);
    auto filled = groove.withTop (pos);
    const bool hot = s.getValue() > 0.0;
    g.setGradientFill (ColourGradient (hot ? pal::amber : colour.brighter (0.2f), 0, filled.getY(), colour.darker (0.4f), 0, filled.getBottom(), false));
    g.fillRoundedRectangle (filled, grooveW * 0.5f);

    // handle
    auto handle = Rectangle<float> (cx - 17.0f, pos - 9.0f, 34.0f, 18.0f);
    g.setGradientFill (ColourGradient (pal::panelHi.brighter (0.25f), 0, handle.getY(), pal::panel, 0, handle.getBottom(), false));
    g.fillRoundedRectangle (handle, 5.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (handle, 5.0f, 1.2f);
    g.setColour (hot ? pal::amber : colour);
    g.fillRoundedRectangle (handle.withSizeKeepingCentre (22.0f, 2.5f), 1.2f);
}

void VsLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto sw = r.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? pal::teal.withAlpha (0.85f) : pal::groove);
    g.fillRoundedRectangle (sw, 8.0f);
    g.setColour (pal::text);
    g.fillEllipse (sw.getX() + (on ? sw.getWidth() - 14.0f : 2.0f), sw.getY() + 2.0f, 12.0f, 12.0f);
    g.setColour (highlighted ? pal::text : pal::dim.brighter (0.3f));
    g.setFont (FontOptions (12.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (6), Justification::centredLeft, 2);
}

// ---------------------------------------------------------------------------
void MeterBar::setLevel (float linearPeak)
{
    const float db = linearPeak > 0.0f ? jmax (-60.0f, Decibels::gainToDecibels (linearPeak)) : -60.0f;
    const float next = db > displayDb ? db : jmax (db, displayDb - 0.7f);
    if (std::abs (next - displayDb) > 0.05f)
    {
        displayDb = next;
        repaint();
    }
}

void MeterBar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float norm = jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);
    auto fillColour = [&] { return displayDb > -1.0f ? pal::red : (displayDb > -12.0f ? pal::amber : pal::teal); };
    g.setFont (FontOptions (10.5f, Font::bold));
    if (horizontal)
    {
        auto lab = r.removeFromLeft (30.0f);
        g.setColour (pal::dim);
        g.drawText (label, lab, Justification::centredLeft);
        auto led = r.removeFromRight (30.0f).reduced (4.0f, 5.0f);
        g.setColour (clipped ? pal::red : pal::groove);
        g.fillRoundedRectangle (led, 3.0f);
        auto bar = r.reduced (0.0f, 6.0f);
        g.setColour (pal::groove);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (fillColour());
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * norm), 3.0f);
    }
    else
    {
        auto labelArea = r.removeFromBottom (16.0f);
        g.setColour (pal::dim);
        g.drawText (label, labelArea, Justification::centred);
        r = r.reduced (4.0f, 2.0f);
        g.setColour (pal::groove);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (fillColour());
        g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * norm), 3.0f);
    }
}

// ---------------------------------------------------------------------------
VocalShiftEditor::VocalShiftEditor (VocalShiftProcessor& p)
    : AudioProcessorEditor (p), proc (p), state (p.getState())
{
    setLookAndFeel (&look);

    title.setText ("VocalShift AI Studio", dontSendNotification);
    title.setFont (FontOptions (21.0f, Font::bold));
    subtitle.setText (u8s ("v" VS_VERSION_STRING " · DSP, sin IA neuronal"), dontSendNotification);
    subtitle.setColour (Label::textColourId, pal::dim);
    subtitle.setFont (FontOptions (12.0f));
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);

    easyTab.setButtonText (u8s ("Fácil"));
    proTab.setButtonText ("Pro");
    addAndMakeVisible (easyTab);
    addAndMakeVisible (proTab);
    easyTab.onClick = [this] { setUiMode (false); };
    proTab.onClick = [this] { setUiMode (true); };

    bypassButton.setButtonText ("Bypass");
    bypassAttachment = std::make_unique<ButtonAttachment> (state, params::bypass, bypassButton);
    addAndMakeVisible (bypassButton);

    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this] { onPresetChosen(); };
    addAndMakeVisible (presetBox);
    saveButton.setButtonText ("Guardar");
    loadButton.setButtonText ("Cargar");
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    identityButton.setButtonText ("Conservar identidad");
    radicalButton.setButtonText (u8s ("Transformación radical"));
    identityButton.onClick = [this] { setVoiceMode (VoiceMode::keepIdentity); };
    radicalButton.onClick = [this] { setVoiceMode (VoiceMode::radical); };
    transformButton.setButtonText ("Transformar");
    secondVoiceButton.setButtonText ("Segunda voz");
    transformButton.onClick = [this] { setRoute (VoiceRoute::transform); };
    secondVoiceButton.onClick = [this] { setRoute (VoiceRoute::secondVoice); };
    for (auto* b : { &identityButton, &radicalButton, &transformButton, &secondVoiceButton })
        addAndMakeVisible (b);
    modeDescription.setColour (Label::textColourId, pal::dim);
    modeDescription.setFont (FontOptions (12.5f));
    modeDescription.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (modeDescription);
    status.setColour (Label::textColourId, pal::dim);
    status.setFont (FontOptions (12.0f));
    status.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (status);

    // The three macros
    setupKnob (pitchKnob, "PITCH", u8s ("Altura de la segunda voz (semitonos)."), params::semitones, true, pal::blue);
    setupKnob (characterKnob, u8s ("CARÁCTER"), u8s ("Cuerpo, presencia, color y textura."), params::character, true, pal::violet);
    setupKnob (magicKnob, "MAGIA", u8s ("Espacio y acabado: anchura, reverb, eco."), params::magic, true, pal::teal);

    // Fader
    fader.setSliderStyle (Slider::LinearVertical);
    fader.setTextBoxStyle (Slider::TextBoxBelow, false, 96, 24);
    fader.setColour (Slider::rotarySliderFillColourId, pal::teal);
    fader.setColour (Slider::textBoxTextColourId, pal::text);
    fader.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    fader.setColour (Slider::textBoxBackgroundColourId, pal::panelHi.withAlpha (0.7f));
    faderAttachment = std::make_unique<SliderAttachment> (state, params::secondGain, fader);
    fader.setDoubleClickReturnValue (true, -12.0);
    fader.onDoubleClickReset = [this] { return restoreFaderToPreset(); };
    faderLabel.setText (u8s ("SEGUNDA VOZ"), dontSendNotification);
    faderLabel.setJustificationType (Justification::centred);
    faderLabel.setFont (FontOptions (13.0f, Font::bold));
    addChildComponent (fader);
    addChildComponent (faderLabel);
    addChildComponent (secondMeter);

    // Pro
    setupKnob (proFine, "Ajuste fino", {}, params::fineCents, false, pal::blue);
    setupKnob (proFormant, "Formantes", {}, params::formantShift, false, pal::violet);
    setupKnob (proBrightness, "Brillo", {}, params::brightness, false, pal::violet);
    setupKnob (proWidth, "Anchura", {}, params::spaceWidth, false, pal::teal);
    setupKnob (proReverb, "Reverb", {}, params::spaceReverb, false, pal::teal);
    setupKnob (proDelay, "Eco corto", {}, params::spaceDelay, false, pal::teal);
    setupKnob (proMix, "Mezcla", {}, params::mix, false, pal::blue);
    setupKnob (proPan, u8s ("Pan 2.ª voz"), {}, params::pan, false, pal::blue);
    setupKnob (proOffset, u8s ("Desfase"), {}, params::offsetMs, false, pal::blue);
    setupKnob (proGain, "Ganancia salida", {}, params::outputGain, false, pal::blue);
    setupToggle (autoLevelButton, autoLevelAttachment, u8s ("Nivel automático"), params::autoLevel);
    setupToggle (safetyButton, safetyAttachment, u8s ("Protección de picos"), params::safetyClip);
    setupToggle (spatialButton, spatialAttachment, u8s ("Procesamiento espacial"), params::spatial);
    setupToggle (improvedButton, improvedAttachment, u8s ("DSP mejorado (apagado = versiones anteriores)"), params::improved);
    setupToggle (engineButton, engineAttachment, u8s ("Motor experimental PSOLA (sin validar al oído)"), params::engine);
    engineInfo.setColour (Label::textColourId, pal::dim);
    engineInfo.setFont (FontOptions (12.0f));
    engineInfo.setJustificationType (Justification::topLeft);
    addChildComponent (engineInfo);

    roadmap.setText (u8s ("Pendiente (sin controles): IA neuronal · Armonías · Edición por frases · Modo Live de baja latencia"), dontSendNotification);
    roadmap.setColour (Label::textColourId, pal::dim.withAlpha (0.7f));
    roadmap.setFont (FontOptions (11.0f));
    addAndMakeVisible (roadmap);

    for (auto* m : { &inMeter, &outMeter })
    {
        m->onClipClicked = [this] { proc.clearClipIndicators(); };
        addAndMakeVisible (m);
    }

    rebuildPresetMenu();
    proMode = state.state.getProperty (params::propUiMode, "easy").toString() == "pro";
    shownVoiceMode = voiceMode();
    shownRoute = route();
    shownImproved = state.getRawParameterValue (params::improved)->load() >= 0.5f;
    refreshModeButtons();
    setUiMode (proMode);

    const int w = state.state.getProperty (params::propUiWidth, 940);
    const int h = state.state.getProperty (params::propUiHeight, 700);
    setResizable (true, true);
    setResizeLimits (800, 640, 1700, 1100);
    setSize (jlimit (800, 1700, w), jlimit (640, 1100, h));

    timerCallback();
    startTimerHz (30);
}

VocalShiftEditor::~VocalShiftEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalShiftEditor::setupKnob (Knob& k, const String& text, const String& hint, const char* paramId, bool big, Colour colour)
{
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 96, big ? 24 : 20);
    k.slider.setColour (Slider::rotarySliderFillColourId, colour);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setColour (Slider::textBoxBackgroundColourId, pal::panelHi.withAlpha (0.7f));
    k.slider.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
    k.slider.setVelocityBasedMode (false);
    k.slider.setScrollWheelEnabled (true);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (big ? FontOptions (17.0f, Font::bold) : FontOptions (12.5f, Font::plain));
    k.label.setColour (Label::textColourId, big ? pal::text : pal::dim.brighter (0.4f));
    k.hint.setText (hint, dontSendNotification);
    k.hint.setJustificationType (Justification::centredTop);
    k.hint.setColour (Label::textColourId, pal::dim);
    k.hint.setFont (FontOptions (12.0f));
    addChildComponent (k.slider);
    addChildComponent (k.label);
    addChildComponent (k.hint);
    k.attachment = std::make_unique<SliderAttachment> (state, paramId, k.slider); // also sets double click = parameter default
}

void VocalShiftEditor::setupToggle (ToggleButton& b, std::unique_ptr<ButtonAttachment>& att, const String& text, const char* paramId)
{
    b.setButtonText (text);
    att = std::make_unique<ButtonAttachment> (state, paramId, b);
    addChildComponent (b);
}

VoiceMode VocalShiftEditor::voiceMode() const
{
    return state.getRawParameterValue (params::mode)->load() >= 0.5f ? VoiceMode::radical : VoiceMode::keepIdentity;
}

VoiceRoute VocalShiftEditor::route() const
{
    return state.getRawParameterValue (params::route)->load() >= 0.5f ? VoiceRoute::secondVoice : VoiceRoute::transform;
}

void VocalShiftEditor::setVoiceMode (VoiceMode m)
{
    if (auto* param = state.getParameter (params::mode))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) m));
        param->endChangeGesture();
    }
    shownVoiceMode = m;
    refreshModeButtons();
}

void VocalShiftEditor::setRoute (VoiceRoute r)
{
    if (auto* param = state.getParameter (params::route))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) r));
        param->endChangeGesture();
    }
    shownRoute = r;
    refreshModeButtons();
}

bool VocalShiftEditor::restoreFaderToPreset()
{
    // Double click: the value of the current factory preset, or the parameter default for user presets.
    float target = -12.0f;
    if (const auto* fp = findFactoryPreset (proc.getPresetManager().currentPresetId().toStdString()))
        target = fp->params.secondGainDb;
    if (auto* param = state.getParameter (params::secondGain))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (target));
        param->endChangeGesture();
    }
    return true;
}

void VocalShiftEditor::refreshModeButtons()
{
    const bool radical = shownVoiceMode == VoiceMode::radical;
    const bool second = shownRoute == VoiceRoute::secondVoice;
    auto on = [] (Colour c) { return c.darker (0.25f); };
    identityButton.setColour (TextButton::buttonColourId, radical ? pal::panelHi : on (pal::blue));
    radicalButton.setColour (TextButton::buttonColourId, radical ? on (pal::violet) : pal::panelHi);
    transformButton.setColour (TextButton::buttonColourId, second ? pal::panelHi : on (pal::blue));
    secondVoiceButton.setColour (TextButton::buttonColourId, second ? on (pal::teal) : pal::panelHi);
    characterKnob.slider.setColour (Slider::rotarySliderFillColourId, radical ? pal::amber : pal::violet);
    for (auto* k : { &proMix })
        k->slider.setEnabled (! second);
    for (auto* k : { &proPan, &proOffset, &proWidth, &proReverb, &proDelay })
        k->slider.setEnabled (second);
    spatialButton.setEnabled (second);

    String d;
    if (second)
        d << (state.getRawParameterValue (params::spatial)->load() >= 0.5f && shownImproved
                  ? u8s ("Segunda voz: tu voz queda al frente sin cambios; detrás, una voz procesada ancha y con espacio (Magia). ")
                  : u8s ("Segunda voz: tu voz queda al frente sin cambios; detrás, la voz procesada. "));
    else
        d << u8s ("Transformar: la voz procesada reemplaza a la original. ");
    d << (radical ? u8s ("Radical: los formantes siguen más al tono. Sigue siendo DSP, no conversión por IA.")
                  : u8s ("Identidad: los formantes se mantienen cerca de los originales."));
    modeDescription.setText (d, dontSendNotification);
    layoutVisibility();
    repaint();
}

void VocalShiftEditor::layoutVisibility()
{
    const bool second = shownRoute == VoiceRoute::secondVoice;
    for (auto* c : { (Component*) &fader, (Component*) &faderLabel, (Component*) &secondMeter })
        c->setVisible (second);
}

void VocalShiftEditor::setUiMode (bool pro)
{
    proMode = pro;
    state.state.setProperty (params::propUiMode, pro ? "pro" : "easy", nullptr);
    easyTab.setColour (TextButton::buttonColourId, pro ? pal::panelHi : pal::blue.darker (0.25f));
    proTab.setColour (TextButton::buttonColourId, pro ? pal::blue.darker (0.25f) : pal::panelHi);

    for (auto* k : { &pitchKnob, &characterKnob, &magicKnob })
    {
        k->slider.setVisible (true);
        k->label.setVisible (true);
        k->hint.setVisible (! pro);
    }
    for (auto* k : { &proFine, &proFormant, &proBrightness, &proMix, &proGain, &proPan, &proOffset, &proWidth, &proReverb, &proDelay })
    {
        k->slider.setVisible (pro);
        k->label.setVisible (pro);
    }
    modeDescription.setVisible (! pro);
    for (auto* b : { &safetyButton, &autoLevelButton, &improvedButton, &spatialButton, &engineButton })
        b->setVisible (pro);
    engineInfo.setVisible (pro);
    layoutVisibility();
    resized();
    repaint();
}

void VocalShiftEditor::rebuildPresetMenu()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading (u8s ("Presets de fábrica (DSP)"));
    const auto& fp = factoryPresets();
    for (size_t i = 0; i < fp.size(); ++i)
        presetBox.addItem (u8s (fp[i].name.c_str()), (int) i + factoryIdOffset);

    userPresetFiles = proc.getPresetManager().listUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("Mis presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), i + userIdOffset);
    }
    shownPresetId = {};
}

void VocalShiftEditor::syncPresetBox()
{
    const auto presetId = proc.getPresetManager().currentPresetId();
    if (presetId == shownPresetId)
        return;
    shownPresetId = presetId;
    const auto& fp = factoryPresets();
    int selected = 0;
    for (size_t i = 0; i < fp.size(); ++i)
        if (presetId == String (fp[i].id))
            selected = (int) i + factoryIdOffset;
    if (presetId.startsWith ("user:"))
        for (int i = 0; i < userPresetFiles.size(); ++i)
            if (userPresetFiles[i].getFileNameWithoutExtension() == presetId.fromFirstOccurrenceOf ("user:", false, false))
                selected = i + userIdOffset;
    presetBox.setSelectedId (selected, dontSendNotification);
}

void VocalShiftEditor::onPresetChosen()
{
    const int id = presetBox.getSelectedId();
    if (id >= userIdOffset)
    {
        const int idx = id - userIdOffset;
        if (isPositiveAndBelow (idx, userPresetFiles.size()))
        {
            auto r = proc.getPresetManager().loadUserPreset (userPresetFiles[idx]);
            if (r.failed())
                status.setText (r.getErrorMessage(), dontSendNotification);
        }
    }
    else if (id >= factoryIdOffset)
    {
        const auto& fp = factoryPresets();
        const size_t idx = (size_t) (id - factoryIdOffset);
        if (idx < fp.size())
            proc.getPresetManager().applyFactoryPreset (fp[idx].id);
    }
}

void VocalShiftEditor::savePreset()
{
    auto dir = PresetManager::userPresetDirectory();
    dir.createDirectory();
    chooser = std::make_unique<FileChooser> ("Guardar preset", dir, String ("*") + PresetManager::fileExtension);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
        [this] (const FileChooser& fc) {
            auto file = fc.getResult();
            if (file == File())
                return;
            file = file.withFileExtension (PresetManager::fileExtension);
            auto r = proc.getPresetManager().saveUserPreset (file, file.getFileNameWithoutExtension());
            status.setText (r.wasOk() ? "Preset guardado: " + file.getFileName() : r.getErrorMessage(), dontSendNotification);
            rebuildPresetMenu();
        });
}

void VocalShiftEditor::loadPreset()
{
    chooser = std::make_unique<FileChooser> ("Cargar preset", PresetManager::userPresetDirectory(),
                                             String ("*") + PresetManager::fileExtension);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [this] (const FileChooser& fc) {
            auto file = fc.getResult();
            if (! file.existsAsFile())
                return;
            auto r = proc.getPresetManager().loadUserPreset (file);
            status.setText (r.wasOk() ? "Preset cargado: " + file.getFileName() : r.getErrorMessage(), dontSendNotification);
        });
}

String VocalShiftEditor::effectiveSummary() const
{
    const auto e = mapMacros (proc.getCurrentVoiceParams());
    if (state.getRawParameterValue (params::bypass)->load() >= 0.5f)
        return "BYPASS activo (salida = entrada, alineada en latencia)";
    String s;
    s << "Efectivo: tono " << (e.semitones >= 0 ? "+" : "") << String (e.semitones, 2) << " st"
      << u8s (" · formantes ") << String (e.formantShiftSemitones, 1) << " st"
      << u8s (" · brillo ") << String (e.brightnessDb, 1) << " dB";
    if (e.spatialAmount > 0.0f)
        s << u8s (" · anchura ") << String (roundToInt (e.spaceWidth * 100.0f)) << u8s (" % · reverb ") << String (roundToInt (e.spaceReverb * 100.0f))
          << u8s (" % (RT60 ") << String (e.spaceRt60, 1) << u8s (" s) · eco ") << String (roundToInt (e.spaceSlap * 100.0f)) << u8s (" % · cuerpo/presencia ") << String (roundToInt (e.textureAmount * 100.0f)) << " %";
    else
        s << u8s (" · compresión ") << String (1.0f + 3.0f * e.magic01, 1) << ":1, aire +" << String (5.0f * e.magic01, 1) << " dB";
    return s;
}

void VocalShiftEditor::timerCallback()
{
    inMeter.setLevel (proc.consumeInputPeak());
    outMeter.setLevel (proc.consumeOutputPeak());
    secondMeter.setLevel (proc.consumeSecondVoicePeak());
    inMeter.setClipped (proc.inputClipped());
    outMeter.setClipped (proc.outputClipped());

    const auto mode = voiceMode();
    const auto rt = route();
    const bool imp = state.getRawParameterValue (params::improved)->load() >= 0.5f;
    if (mode != shownVoiceMode || rt != shownRoute || imp != shownImproved)
    {
        shownVoiceMode = mode;
        shownRoute = rt;
        shownImproved = imp;
        refreshModeButtons();
    }
    syncPresetBox();

    if (! status.getText().startsWith ("Preset"))
        status.setText (effectiveSummary(), dontSendNotification);

    if (proMode)
    {
        const double sr = proc.getPreparedSampleRate();
        const int lat = proc.getEngineLatencySamples();
        String info;
        info << "Motor: " << proc.getEngineName() << u8s (proc.getEngineName()[0] == 'P' ? " (EXPERIMENTAL, sin validar al oído)\n" : " (calidad, bloque 120 ms)\n")
             << "Latencia reportada al host: " << lat << " muestras";
        if (sr > 0)
            info << " (" << String (1000.0 * lat / sr, 1) << " ms a " << String (sr / 1000.0, 1) << " kHz)";
        info << u8s ("\nNivel automático: ") << String (proc.getAutoLevelGainDb(), 1) << u8s (" dB · espacio: ") << (proc.getSpatialActive() ? "activo" : "inactivo");
        info << u8s ("\nNo apto para monitoreo en vivo: no existe un modo de baja latencia.");
        if (proc.getNonFiniteEvents() > 0)
            info << u8s ("\nAviso: se silenciaron ") << proc.getNonFiniteEvents() << u8s (" bloques con valores no finitos.");
        engineInfo.setText (info, dontSendNotification);
    }
}

void VocalShiftEditor::paint (Graphics& g)
{
    g.fillAll (pal::bg);
    auto r = getLocalBounds();
    auto top = r.removeFromTop (92);
    g.setGradientFill (ColourGradient (pal::panelHi, 0, 0, pal::panel, 0, (float) top.getHeight(), false));
    g.fillRect (top);
    g.setColour (pal::blue.withAlpha (0.55f));
    g.fillRect (top.removeFromBottom (2));

    if (proMode)
    {
        g.setFont (FontOptions (11.5f, Font::bold));
        for (int i = 0; i < 6; ++i)
        {
            if (proPanels[i].isEmpty()) continue;
            auto pr = proPanels[i].toFloat();
            g.setColour (pal::panel);
            g.fillRoundedRectangle (pr, 10.0f);
            g.setColour (pal::groove);
            g.drawRoundedRectangle (pr, 10.0f, 1.0f);
            g.setColour (i == 3 ? pal::teal : (i == 1 ? pal::violet : pal::blue));
            g.drawText (u8s (proPanelTitles[i]), pr.reduced (12.0f, 6.0f).removeFromTop (16.0f), Justification::centredLeft);
        }
    }
    else if (shownRoute == VoiceRoute::secondVoice)
    {
        auto strip = faderStrip.toFloat();
        g.setColour (pal::panel);
        g.fillRoundedRectangle (strip, 14.0f);
        g.setColour (pal::groove);
        g.drawRoundedRectangle (strip, 14.0f, 1.0f);
    }
}

void VocalShiftEditor::resized()
{
    state.state.setProperty (params::propUiWidth, getWidth(), nullptr);
    state.state.setProperty (params::propUiHeight, getHeight(), nullptr);

    auto r = getLocalBounds();
    auto header = r.removeFromTop (92).reduced (16, 10);
    // top row: logo, preset, Easy/Pro, bypass
    auto row1 = header.removeFromTop (34);
    auto titleArea = row1.removeFromLeft (250);
    title.setBounds (titleArea.removeFromTop (22));
    subtitle.setBounds (titleArea);
    bypassButton.setBounds (row1.removeFromRight (86));
    row1.removeFromRight (8);
    proTab.setBounds (row1.removeFromRight (64).reduced (0, 2));
    easyTab.setBounds (row1.removeFromRight (64).reduced (0, 2));
    row1.removeFromRight (14);
    loadButton.setBounds (row1.removeFromRight (70).reduced (0, 2));
    row1.removeFromRight (4);
    saveButton.setBounds (row1.removeFromRight (70).reduced (0, 2));
    row1.removeFromRight (8);
    presetBox.setBounds (row1.removeFromRight (jmin (300, row1.getWidth() - 10)).reduced (0, 2));
    header.removeFromTop (8);
    // second row: route and mode selectors
    auto row2 = header.removeFromTop (30);
    const int bw = jmin (190, (row2.getWidth() - 24) / 4);
    transformButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
    secondVoiceButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
    row2.removeFromLeft (22);
    identityButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));
    radicalButton.setBounds (row2.removeFromLeft (bw).reduced (2, 0));

    r.reduce (16, 10);
    // bottom strip: meters, status, roadmap
    auto bottom = r.removeFromBottom (74);
    auto meters = bottom.removeFromTop (22);
    inMeter.setBounds (meters.removeFromLeft (meters.getWidth() / 2 - 8));
    meters.removeFromLeft (16);
    outMeter.setBounds (meters);
    status.setBounds (bottom.removeFromTop (24));
    roadmap.setBounds (bottom.removeFromTop (20));
    r.removeFromBottom (6);

    const bool second = shownRoute == VoiceRoute::secondVoice;
    Rectangle<int> faderArea;
    if (second)
        faderArea = r.removeFromRight (jlimit (150, 200, r.getWidth() / 5));

    if (! proMode)
    {
        modeDescription.setBounds (r.removeFromTop (34));
        auto macros = r.reduced (0, 6);
        const int w = macros.getWidth() / 3;
        for (auto* k : { &pitchKnob, &characterKnob, &magicKnob })
        {
            auto cell = macros.removeFromLeft (w).reduced (10, 0);
            k->label.setBounds (cell.removeFromTop (28));
            k->hint.setBounds (cell.removeFromBottom (34));
            const int side = jmin (cell.getWidth(), cell.getHeight() - 30, 270);
            auto knobArea = cell.withSizeKeepingCentre (jmax (60, side), jmax (60, side + 30));
            k->slider.setBounds (knobArea);
        }
    }
    else
    {
        for (auto& p : proPanels) p = {};
        auto grid = r;
        auto rowTop = grid.removeFromTop (grid.getHeight() * 45 / 100);
        grid.removeFromTop (8);
        auto rowMid = grid.removeFromTop (grid.getHeight() * 55 / 100);
        grid.removeFromTop (8);
        auto rowBottom = grid;

        auto place = [] (Rectangle<int> area, std::initializer_list<Knob*> ks, int headerH = 24) {
            area.removeFromTop (headerH);
            area.reduce (6, 2);
            const int w = area.getWidth() / (int) ks.size();
            for (auto* k : ks)
            {
                auto cell = area.removeFromLeft (w).reduced (3, 0);
                k->label.setBounds (cell.removeFromTop (16));
                k->slider.setBounds (cell);
            }
        };
        // row 1: Pitch | Timbre | Dinámica
        const int w1 = rowTop.getWidth();
        proPanels[0] = rowTop.removeFromLeft (w1 * 28 / 100);
        rowTop.removeFromLeft (8);
        proPanels[1] = rowTop.removeFromLeft (w1 * 37 / 100);
        rowTop.removeFromLeft (8);
        proPanels[2] = rowTop;
        place (proPanels[0], { &pitchKnob, &proFine });
        place (proPanels[1], { &characterKnob, &proFormant, &proBrightness });
        {
            auto a = proPanels[2];
            a.removeFromTop (24);
            a.reduce (6, 2);
            auto knobCell = a.removeFromLeft (a.getWidth() / 2).reduced (3, 0);
            magicKnob.label.setBounds (knobCell.removeFromTop (16));
            magicKnob.slider.setBounds (knobCell);
            autoLevelButton.setBounds (a.removeFromTop (a.getHeight() / 2).withSizeKeepingCentre (a.getWidth(), 34));
            safetyButton.setBounds (a.withSizeKeepingCentre (a.getWidth(), 34));
        }
        // row 2: Espacio | Mezcla
        const int w2 = rowMid.getWidth();
        proPanels[3] = rowMid.removeFromLeft (w2 * 42 / 100);
        rowMid.removeFromLeft (8);
        proPanels[4] = rowMid;
        {
            auto a = proPanels[3];
            auto inner = a;
            inner.removeFromTop (24);
            inner.reduce (6, 2);
            spatialButton.setBounds (inner.removeFromBottom (30));
            const int w = inner.getWidth() / 3;
            for (auto* k : { &proWidth, &proReverb, &proDelay })
            {
                auto cell = inner.removeFromLeft (w).reduced (3, 0);
                k->label.setBounds (cell.removeFromTop (16));
                k->slider.setBounds (cell);
            }
        }
        place (proPanels[4], { &proMix, &proPan, &proOffset, &proGain });
        // row 3: Calidad y diagnóstico
        proPanels[5] = rowBottom;
        {
            auto a = proPanels[5];
            a.removeFromTop (24);
            a.reduce (12, 2);
            auto toggles = a.removeFromTop (28);
            improvedButton.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
            engineButton.setBounds (toggles);
            engineInfo.setBounds (a);
        }
    }

    faderStrip = faderArea.reduced (4, 0);
    if (second)
    {
        auto f = faderArea.reduced (14, 10);
        faderLabel.setBounds (f.removeFromTop (22));
        auto m = f.removeFromRight (22);
        secondMeter.setBounds (m.withTrimmedTop (4).withTrimmedBottom (28));
        fader.setBounds (f.withTrimmedTop (4));
    }
}

} // namespace vs
