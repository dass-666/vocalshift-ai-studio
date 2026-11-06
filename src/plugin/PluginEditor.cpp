#include "PluginEditor.h"

namespace vs
{

using namespace juce;

static String u8s (const char* s) { return String::fromUTF8 (s); }

namespace colours
{
    const Colour bg { 0xff15171c };
    const Colour panel { 0xff1e222a };
    const Colour accent { 0xff3fc1c9 };
    const Colour morph { 0xffc98a3f };
    const Colour text { 0xffe6e8ec };
    const Colour dim { 0xff8a90a0 };
    const Colour clip { 0xffe5484d };
}

static constexpr int factoryIdOffset = 1;
static constexpr int userIdOffset = 1000;

// ---------------------------------------------------------------------------
void MeterBar::setLevel (float linearPeak)
{
    const float db = linearPeak > 0.0f ? jmax (-60.0f, Decibels::gainToDecibels (linearPeak)) : -60.0f;
    // Fast attack, ~20 dB/s release at 30 Hz refresh.
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
    auto led = r.removeFromTop (14.0f).reduced (2.0f, 1.0f);
    g.setColour (clipped ? colours::clip : colours::panel.brighter (0.15f));
    g.fillRoundedRectangle (led, 3.0f);
    g.setColour (colours::text);
    g.setFont (FontOptions (10.0f));
    g.drawText (clipped ? "CLIP" : "", led, Justification::centred);

    auto labelArea = r.removeFromBottom (16.0f);
    g.setColour (colours::dim);
    g.drawText (label, labelArea, Justification::centred);

    r = r.reduced (4.0f, 2.0f);
    g.setColour (colours::panel.brighter (0.05f));
    g.fillRoundedRectangle (r, 3.0f);
    const float norm = jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);
    auto fill = r.withTop (r.getBottom() - r.getHeight() * norm);
    g.setColour (displayDb > -1.0f ? colours::clip : (displayDb > -12.0f ? Colours::gold : colours::accent));
    g.fillRoundedRectangle (fill, 3.0f);
}

// ---------------------------------------------------------------------------
VocalShiftEditor::VocalShiftEditor (VocalShiftProcessor& p)
    : AudioProcessorEditor (p), proc (p), state (p.getState())
{
    auto& lf = getLookAndFeel();
    lf.setColour (Slider::rotarySliderFillColourId, colours::accent);
    lf.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    lf.setColour (Label::textColourId, colours::text);
    lf.setColour (ComboBox::backgroundColourId, colours::panel);
    lf.setColour (TextButton::buttonColourId, colours::panel);
    lf.setColour (TextButton::buttonOnColourId, colours::accent.darker (0.3f));

    title.setText ("VocalShift AI Studio", dontSendNotification);
    title.setFont (FontOptions (20.0f, Font::bold));
    subtitle.setText (u8s ("v" VS_VERSION_STRING " · prototipo DSP · sin IA neuronal todavía"), dontSendNotification);
    subtitle.setColour (Label::textColourId, colours::dim);
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

    // Voice mode (automatable parameter, shared by both pages)
    identityButton.setButtonText ("Conservar identidad");
    radicalButton.setButtonText (u8s ("Transformación radical"));
    identityButton.onClick = [this] { setVoiceMode (VoiceMode::keepIdentity); };
    radicalButton.onClick = [this] { setVoiceMode (VoiceMode::radical); };
    addAndMakeVisible (identityButton);
    addAndMakeVisible (radicalButton);
    modeDescription.setColour (Label::textColourId, colours::dim);
    modeDescription.setJustificationType (Justification::topLeft);
    addAndMakeVisible (modeDescription);
    status.setColour (Label::textColourId, colours::dim);
    status.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (status);

    // Easy page: three macros
    setupKnob (easyPitch, "Pitch", u8s ("Sube o baja la nota (semitonos). La duración no cambia."), params::semitones, true);
    setupKnob (easyCharacter, u8s ("Carácter"), u8s ("Cuánto cambia el timbre: formantes y brillo."), params::character, true);
    setupKnob (easyMagic, "Magia", u8s ("Pulido: compresión suave + aire en agudos."), params::magic, true);
    setupKnob (easyMix, "Mezcla", {}, params::mix, false);
    setupKnob (easyGain, "Ganancia", {}, params::outputGain, false);

    // Pro page
    setupKnob (proPitch, "Pitch", {}, params::semitones, false);
    setupKnob (proFine, "Ajuste fino", {}, params::fineCents, false);
    setupKnob (proCharacter, u8s ("Carácter"), {}, params::character, false);
    setupKnob (proMagic, "Magia", {}, params::magic, false);
    setupKnob (proFormant, "Ajuste formantes", {}, params::formantShift, false);
    setupKnob (proBrightness, "Ajuste brillo", {}, params::brightness, false);
    setupKnob (proMix, "Mezcla", {}, params::mix, false);
    setupKnob (proGain, "Ganancia salida", {}, params::outputGain, false);
    safetyButton.setButtonText (u8s ("Protección de picos (soft clip, techo 0 dBFS)"));
    safetyAttachment = std::make_unique<ButtonAttachment> (state, params::safetyClip, safetyButton);
    addChildComponent (safetyButton);
    autoLevelButton.setButtonText (u8s ("Nivel automático (iguala el volumen procesado al original)"));
    autoLevelAttachment = std::make_unique<ButtonAttachment> (state, params::autoLevel, autoLevelButton);
    addChildComponent (autoLevelButton);
    engineInfo.setColour (Label::textColourId, colours::dim);
    engineInfo.setJustificationType (Justification::topLeft);
    addChildComponent (engineInfo);

    for (auto* k : { &easyPitch, &easyCharacter, &easyMagic })
        k->slider.setColour (Slider::rotarySliderFillColourId, colours::accent);
    easyMagic.slider.setColour (Slider::rotarySliderFillColourId, colours::morph);
    proMagic.slider.setColour (Slider::rotarySliderFillColourId, colours::morph);

    roadmap.setText (u8s ("Roadmap (NO disponible aún, sin controles): IA neuronal · Armonías · "
                          "Doblaje · Edición por frases · Modo Live de baja latencia"),
                     dontSendNotification);
    roadmap.setColour (Label::textColourId, colours::dim);
    roadmap.setFont (FontOptions (12.0f));
    roadmap.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (roadmap);

    for (auto* m : { &inMeter, &outMeter })
    {
        m->onClipClicked = [this] { proc.clearClipIndicators(); };
        addAndMakeVisible (m);
    }

    rebuildPresetMenu();
    proMode = state.state.getProperty (params::propUiMode, "easy").toString() == "pro";
    shownVoiceMode = voiceMode();
    refreshModeButtons();
    setUiMode (proMode);

    const int w = state.state.getProperty (params::propUiWidth, 900);
    const int h = state.state.getProperty (params::propUiHeight, 580);
    setResizable (true, true);
    setResizeLimits (760, 500, 1600, 1000);
    setSize (jlimit (760, 1600, w), jlimit (500, 1000, h));

    timerCallback(); // populate preset name, status and engine info immediately
    startTimerHz (30);
}

VocalShiftEditor::~VocalShiftEditor()
{
    stopTimer();
}

void VocalShiftEditor::setupKnob (Knob& k, const String& text, const String& hint, const char* paramId, bool big)
{
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 20);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    if (big)
        k.label.setFont (FontOptions (18.0f, Font::bold));
    k.hint.setText (hint, dontSendNotification);
    k.hint.setJustificationType (Justification::centredTop);
    k.hint.setColour (Label::textColourId, colours::dim);
    k.hint.setFont (FontOptions (12.0f));
    addChildComponent (k.slider);
    addChildComponent (k.label);
    addChildComponent (k.hint);
    k.attachment = std::make_unique<SliderAttachment> (state, paramId, k.slider);
}

VoiceMode VocalShiftEditor::voiceMode() const
{
    return state.getRawParameterValue (params::mode)->load() >= 0.5f ? VoiceMode::radical : VoiceMode::keepIdentity;
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

void VocalShiftEditor::refreshModeButtons()
{
    const bool radical = shownVoiceMode == VoiceMode::radical;
    identityButton.setColour (TextButton::buttonColourId, radical ? colours::panel : colours::accent.darker (0.3f));
    radicalButton.setColour (TextButton::buttonColourId, radical ? colours::morph.darker (0.3f) : colours::panel);
    easyCharacter.slider.setColour (Slider::rotarySliderFillColourId, radical ? colours::morph : colours::accent);
    modeDescription.setText (radical
        ? u8s ("Transformación radical: los formantes siguen al tono y Carácter los empuja más lejos "
               "(hasta 6 st) con brillo de hasta 6 dB. Cambia el carácter de la voz; no es conversión de identidad por IA.")
        : u8s ("Conservar identidad: los formantes se mantienen casi en su sitio. Carácter solo añade un cambio "
               "sutil de timbre (hasta 25 % de seguimiento, 1 st y 1.5 dB)."),
        dontSendNotification);
    repaint();
}

void VocalShiftEditor::setUiMode (bool pro)
{
    proMode = pro;
    state.state.setProperty (params::propUiMode, pro ? "pro" : "easy", nullptr);
    easyTab.setColour (TextButton::buttonColourId, pro ? colours::panel : colours::accent.darker (0.3f));
    proTab.setColour (TextButton::buttonColourId, pro ? colours::accent.darker (0.3f) : colours::panel);

    for (auto* k : { &easyPitch, &easyCharacter, &easyMagic, &easyMix, &easyGain })
    {
        k->slider.setVisible (! pro);
        k->label.setVisible (! pro);
        k->hint.setVisible (! pro);
    }
    for (auto* k : { &proPitch, &proFine, &proCharacter, &proMagic, &proFormant, &proBrightness, &proMix, &proGain })
    {
        k->slider.setVisible (pro);
        k->label.setVisible (pro);
    }
    modeDescription.setVisible (! pro);
    safetyButton.setVisible (pro);
    autoLevelButton.setVisible (pro);
    engineInfo.setVisible (pro);
    resized();
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
    // Shows what the macros actually do right now (same mapping the audio thread uses).
    const auto e = mapMacros (proc.getCurrentVoiceParams());
    if (state.getRawParameterValue (params::bypass)->load() >= 0.5f)
        return "BYPASS activo (salida = entrada, alineada en latencia)";
    String s;
    s << "Efectivo: tono " << (e.semitones >= 0 ? "+" : "") << String (e.semitones, 2) << " st"
      << u8s (" · formantes siguen el ") << String (roundToInt ((1.0f - e.formantPreservation) * 100.0f)) << u8s (" % del tono")
      << " y se desplazan " << String (e.formantShiftSemitones, 1) << " st"
      << u8s (" · brillo ") << String (e.brightnessDb, 1) << " dB"
      << u8s (" · compresión ") << String (1.0f + 3.0f * e.magic01, 1) << ":1, aire +" << String (5.0f * e.magic01, 1) << " dB";
    return s;
}

void VocalShiftEditor::timerCallback()
{
    inMeter.setLevel (proc.consumeInputPeak());
    outMeter.setLevel (proc.consumeOutputPeak());
    inMeter.setClipped (proc.inputClipped());
    outMeter.setClipped (proc.outputClipped());

    const auto mode = voiceMode();
    if (mode != shownVoiceMode)
    {
        shownVoiceMode = mode;
        refreshModeButtons();
    }
    syncPresetBox();

    if (! status.getText().startsWith ("Preset"))
        status.setText (effectiveSummary(), dontSendNotification);

    const double sr = proc.getPreparedSampleRate();
    const int lat = proc.getEngineLatencySamples();
    String info;
    info << "Motor: " << proc.getEngineName() << u8s (" (modo calidad: bloque 120 ms, cálculo repartido)\n")
         << "Latencia reportada al host: " << lat << " muestras";
    if (sr > 0)
        info << " (" << String (1000.0 * lat / sr, 1) << " ms a " << String (sr / 1000.0, 1) << " kHz)";
    info << u8s (" · nivel automático: ") << String (proc.getAutoLevelGainDb(), 1) << " dB";
    info << u8s ("\nNo apto para monitoreo en vivo: no existe todavía un modo de baja latencia.");
    if (proc.getNonFiniteEvents() > 0)
        info << u8s ("\nAviso: se silenciaron ") << proc.getNonFiniteEvents() << u8s (" bloques con valores no finitos.");
    engineInfo.setText (info, dontSendNotification);
}

void VocalShiftEditor::paint (Graphics& g)
{
    g.fillAll (colours::bg);
    auto r = getLocalBounds();
    g.setColour (colours::panel);
    g.fillRect (r.removeFromTop (56));
}

void VocalShiftEditor::resized()
{
    state.state.setProperty (params::propUiWidth, getWidth(), nullptr);
    state.state.setProperty (params::propUiHeight, getHeight(), nullptr);

    auto r = getLocalBounds();
    auto header = r.removeFromTop (56).reduced (12, 8);
    auto titleArea = header.removeFromLeft (260);
    title.setBounds (titleArea.removeFromTop (24));
    subtitle.setBounds (titleArea);
    bypassButton.setBounds (header.removeFromRight (90));
    header.removeFromRight (8);
    loadButton.setBounds (header.removeFromRight (70).reduced (0, 4));
    header.removeFromRight (4);
    saveButton.setBounds (header.removeFromRight (70).reduced (0, 4));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (jmin (260, header.getWidth() / 2)).reduced (0, 4));
    header.removeFromRight (8);
    proTab.setBounds (header.removeFromRight (60).reduced (0, 4));
    easyTab.setBounds (header.removeFromRight (60).reduced (0, 4));

    r.reduce (12, 8);
    roadmap.setBounds (r.removeFromBottom (22));
    status.setBounds (r.removeFromBottom (22));
    auto meters = r.removeFromRight (90);
    inMeter.setBounds (meters.removeFromLeft (45));
    outMeter.setBounds (meters);
    r.removeFromRight (12);

    auto modeRow = r.removeFromTop (36);
    identityButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).reduced (2));
    radicalButton.setBounds (modeRow.reduced (2));

    if (! proMode)
    {
        modeDescription.setBounds (r.removeFromTop (40));
        auto smallRow = r.removeFromBottom (110);
        auto macros = r.reduced (0, 4);
        const int w = macros.getWidth() / 3;
        for (auto* k : { &easyPitch, &easyCharacter, &easyMagic })
        {
            auto cell = macros.removeFromLeft (w).reduced (8, 0);
            k->label.setBounds (cell.removeFromTop (26));
            k->hint.setBounds (cell.removeFromBottom (32));
            const int side = jmin (cell.getWidth(), cell.getHeight(), 230);
            k->slider.setBounds (cell.withSizeKeepingCentre (side, side));
        }
        const int sw = jmin (160, smallRow.getWidth() / 2);
        auto smallArea = smallRow.withSizeKeepingCentre (sw * 2, smallRow.getHeight());
        for (auto* k : { &easyMix, &easyGain })
        {
            auto cell = smallArea.removeFromLeft (sw).reduced (6, 0);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell);
        }
    }
    else
    {
        r.removeFromTop (6);
        auto infoArea = r.removeFromBottom (110);
        safetyButton.setBounds (infoArea.removeFromTop (22));
        autoLevelButton.setBounds (infoArea.removeFromTop (22));
        engineInfo.setBounds (infoArea);
        auto top = r.removeFromTop (r.getHeight() / 2);
        auto layoutRow = [] (Rectangle<int> row, std::initializer_list<Knob*> ks) {
            const int w = row.getWidth() / (int) ks.size();
            for (auto* k : ks)
            {
                auto cell = row.removeFromLeft (w).reduced (4);
                k->label.setBounds (cell.removeFromTop (20));
                k->slider.setBounds (cell);
            }
        };
        layoutRow (top, { &proPitch, &proFine, &proCharacter, &proMagic });
        layoutRow (r, { &proFormant, &proBrightness, &proMix, &proGain });
    }
}

} // namespace vs
