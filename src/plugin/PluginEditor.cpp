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
    for (auto* b : { &easyTab, &proTab })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
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

    // Easy page
    identityButton.setButtonText ("Identity Pitch");
    morphButton.setButtonText ("Voice Morph (DSP)");
    identityButton.setRadioGroupId (1);
    morphButton.setRadioGroupId (1);
    identityButton.setClickingTogglesState (true);
    morphButton.setClickingTogglesState (true);
    identityButton.onClick = [this] { setEngineMode (EngineMode::identityPitch); };
    morphButton.onClick = [this] { setEngineMode (EngineMode::voiceMorphDsp); };
    addChildComponent (identityButton);
    addChildComponent (morphButton);
    modeDescription.setColour (Label::textColourId, colours::dim);
    modeDescription.setJustificationType (Justification::topLeft);
    addChildComponent (modeDescription);
    status.setColour (Label::textColourId, colours::dim);
    status.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (status);

    setupKnob (easyIntensity, "Intensidad", params::semitones);
    setupKnob (easySecond, "Naturalidad", params::formantPreservation);
    setupKnob (easyMix, "Mezcla", params::mix);
    setupKnob (easyGain, "Ganancia", params::outputGain);

    // Pro page
    setupKnob (proSemitones, u8s ("Transposición"), params::semitones);
    setupKnob (proFine, "Ajuste fino", params::fineCents);
    setupKnob (proPreserve, u8s ("Preservación formantes"), params::formantPreservation);
    setupKnob (proFormant, "Desplaz. formantes", params::formantShift);
    setupKnob (proBrightness, "Brillo", params::brightness);
    setupKnob (proMix, "Mezcla", params::mix);
    setupKnob (proGain, "Ganancia salida", params::outputGain);
    safetyButton.setButtonText (u8s ("Protección de picos (soft clip, techo 0 dBFS)"));
    safetyAttachment = std::make_unique<ButtonAttachment> (state, params::safetyClip, safetyButton);
    addChildComponent (safetyButton);
    engineInfo.setColour (Label::textColourId, colours::dim);
    engineInfo.setJustificationType (Justification::topLeft);
    addChildComponent (engineInfo);

    roadmap.setText (u8s ("Roadmap (NO disponible aún, sin controles): AI Voice Morph neuronal · Armonías · "
                          "Doblaje · Edición por frases · Mezcla automática · Modo Live de baja latencia"),
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
    shownEngineMode = engineMode();
    refreshEasyPage();
    setUiMode (proMode);

    const int w = state.state.getProperty (params::propUiWidth, 900);
    const int h = state.state.getProperty (params::propUiHeight, 560);
    setResizable (true, true);
    setResizeLimits (760, 480, 1600, 1000);
    setSize (jlimit (760, 1600, w), jlimit (480, 1000, h));

    timerCallback(); // populate preset name, status and engine info immediately
    startTimerHz (30);
}

VocalShiftEditor::~VocalShiftEditor()
{
    stopTimer();
}

void VocalShiftEditor::setupKnob (Knob& k, const String& text, const char* paramId)
{
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 20);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    addChildComponent (k.slider);
    addChildComponent (k.label);
    attachKnob (k, paramId);
}

void VocalShiftEditor::attachKnob (Knob& k, const char* paramId)
{
    k.attachment.reset();
    k.attachment = std::make_unique<SliderAttachment> (state, paramId, k.slider);
}

EngineMode VocalShiftEditor::engineMode() const
{
    return (int) state.state.getProperty (params::propEngineMode, 0) == 1 ? EngineMode::voiceMorphDsp
                                                                          : EngineMode::identityPitch;
}

void VocalShiftEditor::setEngineMode (EngineMode m)
{
    state.state.setProperty (params::propEngineMode, (int) m, nullptr);
    shownEngineMode = m;
    refreshEasyPage();
}

void VocalShiftEditor::refreshEasyPage()
{
    const bool morph = shownEngineMode == EngineMode::voiceMorphDsp;
    identityButton.setToggleState (! morph, dontSendNotification);
    morphButton.setToggleState (morph, dontSendNotification);

    if (morph)
    {
        easyIntensity.label.setText (u8s ("Carácter (formantes)"), dontSendNotification);
        attachKnob (easyIntensity, params::formantShift);
        easySecond.label.setText ("Brillo", dontSendNotification);
        attachKnob (easySecond, params::brightness);
        modeDescription.setText (u8s ("Voice Morph (DSP): cambia el timbre desplazando formantes y el brillo, "
                                      "sin cambiar la nota. No es conversión de identidad: el morph neuronal está en roadmap."),
                                 dontSendNotification);
    }
    else
    {
        easyIntensity.label.setText (u8s ("Intensidad (semitonos)"), dontSendNotification);
        attachKnob (easyIntensity, params::semitones);
        easySecond.label.setText (u8s ("Naturalidad (preservar formantes)"), dontSendNotification);
        attachKnob (easySecond, params::formantPreservation);
        modeDescription.setText (u8s ("Identity Pitch: sube o baja la nota manteniendo la duración. "
                                      "Naturalidad = cuánto se mantienen los formantes originales (100 % = timbre propio; 0 % = efecto ardilla/monstruo)."),
                                 dontSendNotification);
    }
    const auto c = morph ? colours::morph : colours::accent;
    for (auto* k : { &easyIntensity, &easySecond })
        k->slider.setColour (Slider::rotarySliderFillColourId, c);
}

void VocalShiftEditor::setUiMode (bool pro)
{
    proMode = pro;
    state.state.setProperty (params::propUiMode, pro ? "pro" : "easy", nullptr);
    easyTab.setToggleState (! pro, dontSendNotification);
    proTab.setToggleState (pro, dontSendNotification);
    easyTab.setColour (TextButton::buttonColourId, pro ? colours::panel : colours::accent.darker (0.3f));
    proTab.setColour (TextButton::buttonColourId, pro ? colours::accent.darker (0.3f) : colours::panel);

    for (auto* k : { &easyIntensity, &easySecond, &easyMix, &easyGain })
    {
        k->slider.setVisible (! pro);
        k->label.setVisible (! pro);
    }
    identityButton.setVisible (! pro);
    morphButton.setVisible (! pro);
    modeDescription.setVisible (! pro);

    for (auto* k : { &proSemitones, &proFine, &proPreserve, &proFormant, &proBrightness, &proMix, &proGain })
    {
        k->slider.setVisible (pro);
        k->label.setVisible (pro);
    }
    safetyButton.setVisible (pro);
    engineInfo.setVisible (pro);
    resized();
}

void VocalShiftEditor::rebuildPresetMenu()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("Presets de fábrica (DSP)");
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
    shownEngineMode = engineMode();
    refreshEasyPage();
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
            shownEngineMode = engineMode();
            refreshEasyPage();
        });
}

String VocalShiftEditor::statusSummary() const
{
    const auto v = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    const float st = jlimit (-24.0f, 24.0f, v (params::semitones) + v (params::fineCents) / 100.0f);
    String s;
    s << u8s ("Activo: tono ") << (st >= 0 ? "+" : "") << String (st, 2) << " st"
      << u8s (" · formantes preservados ") << String (v (params::formantPreservation), 0) << " %"
      << u8s (" · desplaz. formantes ") << String (v (params::formantShift), 1) << " st"
      << u8s (" · brillo ") << String (v (params::brightness), 1) << " dB"
      << u8s (" · mezcla ") << String (v (params::mix), 0) << " %";
    if (v (params::bypass) >= 0.5f)
        s = "BYPASS activo (salida = entrada, alineada en latencia)";
    return s;
}

void VocalShiftEditor::timerCallback()
{
    inMeter.setLevel (proc.consumeInputPeak());
    outMeter.setLevel (proc.consumeOutputPeak());
    inMeter.setClipped (proc.inputClipped());
    outMeter.setClipped (proc.outputClipped());

    // Reflect external state changes (preset load, project restore).
    const auto mode = engineMode();
    if (mode != shownEngineMode)
    {
        shownEngineMode = mode;
        refreshEasyPage();
    }
    const auto presetId = proc.getPresetManager().currentPresetId();
    if (presetId != shownPresetId)
    {
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

    if (! status.getText().startsWith ("Preset"))
        status.setText (statusSummary(), dontSendNotification);

    const double sr = proc.getPreparedSampleRate();
    const int lat = proc.getEngineLatencySamples();
    String info;
    info << "Motor: " << proc.getEngineName() << u8s (" (modo calidad: bloque 120 ms, cálculo repartido)\n")
         << "Latencia reportada al host: " << lat << " muestras";
    if (sr > 0)
        info << " (" << String (1000.0 * lat / sr, 1) << " ms a " << String (sr / 1000.0, 1) << " kHz)";
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
    presetBox.setBounds (header.removeFromRight (jmin (240, header.getWidth() / 2)).reduced (0, 4));
    header.removeFromRight (8);
    proTab.setBounds (header.removeFromRight (60).reduced (0, 4));
    easyTab.setBounds (header.removeFromRight (60).reduced (0, 4));

    r.reduce (12, 8);
    roadmap.setBounds (r.removeFromBottom (24));
    status.setBounds (r.removeFromBottom (22));
    auto meters = r.removeFromRight (90);
    inMeter.setBounds (meters.removeFromLeft (45));
    outMeter.setBounds (meters);
    r.removeFromRight (12);

    if (! proMode)
    {
        auto modeRow = r.removeFromTop (36);
        identityButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).reduced (2));
        morphButton.setBounds (modeRow.reduced (2));
        modeDescription.setBounds (r.removeFromTop (44));
        auto knobs = r.reduced (0, 8);
        const int w = knobs.getWidth() / 4;
        for (auto* k : { &easyIntensity, &easySecond, &easyMix, &easyGain })
        {
            auto cell = knobs.removeFromLeft (w).reduced (6);
            const int side = jmin (cell.getWidth(), cell.getHeight() - 36, 220);
            cell = cell.withSizeKeepingCentre (cell.getWidth(), side + 36);
            k->label.setBounds (cell.removeFromTop (36));
            k->slider.setBounds (cell.withSizeKeepingCentre (side, side));
        }
    }
    else
    {
        auto infoArea = r.removeFromBottom (72);
        safetyButton.setBounds (infoArea.removeFromTop (24));
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
        layoutRow (top, { &proSemitones, &proFine, &proPreserve, &proFormant });
        layoutRow (r, { &proBrightness, &proMix, &proGain });
    }
}

} // namespace vs
