#include "PresetManager.h"
#include "Parameters.h"

namespace vs
{

using namespace juce;

static const Identifier presetTag { "VocalShiftPreset" };

PresetManager::PresetManager (AudioProcessorValueTreeState& s) : apvts (s) {}

bool PresetManager::applyFactoryPreset (const std::string& id)
{
    const auto* p = findFactoryPreset (id);
    if (p == nullptr)
        return false;
    params::applyToParameters (apvts, p->params);
    apvts.state.setProperty (params::propPresetId, String (p->id), nullptr);
    return true;
}

File PresetManager::userPresetDirectory()
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
        .getChildFile ("VocalShift AI Studio")
        .getChildFile ("Presets");
}

ValueTree PresetManager::toPresetTree (const String& name) const
{
    ValueTree t (presetTag);
    t.setProperty ("formatVersion", presetFormatVersion, nullptr);
    t.setProperty ("name", name, nullptr);
    for (auto* id : params::presetParamIds)
    {
        ValueTree p ("Param");
        p.setProperty ("id", id, nullptr);
        p.setProperty ("value", apvts.getRawParameterValue (id)->load(), nullptr);
        t.appendChild (p, nullptr);
    }
    return t;
}

Result PresetManager::fromPresetTree (const ValueTree& t)
{
    if (! t.hasType (presetTag))
        return Result::fail ("No es un preset de VocalShift");
    const int version = t.getProperty ("formatVersion", 0);
    if (version < 1 || version > presetFormatVersion)
        return Result::fail ("Version de preset no soportada: " + String (version));

    for (auto child : t)
    {
        const String id = child.getProperty ("id");
        if (id == params::bypass)
            continue; // presets never change bypass
        if (auto* param = apvts.getParameter (id))
        {
            const float v = child.getProperty ("value");
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (v));
            param->endChangeGesture();
        }
    }
    if (version == 1 && t.hasProperty (params::legacyPropEngineMode))
    {
        // v1 preset: mode was a property, not a parameter.
        if (auto* param = apvts.getParameter (params::mode))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) jlimit (0, 1, (int) t.getProperty (params::legacyPropEngineMode))));
    }
    apvts.state.setProperty (params::propPresetId, "user:" + t.getProperty ("name").toString(), nullptr);
    return Result::ok();
}

Result PresetManager::saveUserPreset (const File& file, const String& name)
{
    auto xml = toPresetTree (name).createXml();
    if (xml == nullptr)
        return Result::fail ("No se pudo serializar el preset");
    if (! file.getParentDirectory().createDirectory())
        return Result::fail ("No se pudo crear la carpeta de presets");
    if (! xml->writeTo (file))
        return Result::fail ("No se pudo escribir " + file.getFullPathName());
    return Result::ok();
}

Result PresetManager::loadUserPreset (const File& file)
{
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr)
        return Result::fail ("Archivo de preset invalido");
    return fromPresetTree (ValueTree::fromXml (*xml));
}

Array<File> PresetManager::listUserPresets() const
{
    auto files = userPresetDirectory().findChildFiles (File::findFiles, false, String ("*") + fileExtension);
    files.sort();
    return files;
}

String PresetManager::currentPresetId() const
{
    return apvts.state.getProperty (params::propPresetId, "neutral").toString();
}

} // namespace vs
