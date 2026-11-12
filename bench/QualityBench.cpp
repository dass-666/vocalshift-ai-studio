// QualityBench: renders loudness-matched A/B files for listening tests and
// prints objective measurements (F0, latency, CPU). It renders through the real
// VocalShiftProcessor, so the files are what the plugin produces.
//
// Usage:
//   vs_bench --out <dir> [--input voice.wav] [--rate 48000] [--block 512]
// Without --input it renders synthetic vowels (they are NOT a substitute for
// human voices: use --input with legally usable recordings for real judgements).

#include "../src/plugin/PluginProcessor.h"
#include "../src/plugin/PluginEditor.h"
#include "../tests/Signals.h"

#include <core/analysis/PitchDetector.h>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <chrono>

using namespace juce;

namespace
{
struct Source
{
    String name;
    AudioBuffer<float> audio;
    double rate;
};

AudioBuffer<float> process (const AudioBuffer<float>& in, double rate, int block,
                            const std::function<void (vs::VocalShiftProcessor&)>& setup, double& cpuSeconds, int& latency)
{
    vs::VocalShiftProcessor p;
    AudioProcessor::BusesLayout layout;
    const auto set = in.getNumChannels() == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo();
    layout.inputBuses.add (set);
    layout.outputBuses.add (set);
    p.setBusesLayout (layout);
    setup (p);
    p.setNonRealtime (true);
    p.prepareToPlay (rate, block);
    latency = p.getLatencySamples();

    // Feed latency worth of silence at the end, then drop the first `latency` samples:
    // the result is time-aligned with the input, as a PDC-compensating host would play it.
    const int total = in.getNumSamples() + latency;
    AudioBuffer<float> out (in.getNumChannels(), total);
    out.clear();
    for (int c = 0; c < in.getNumChannels(); ++c)
        out.copyFrom (c, 0, in, c, 0, in.getNumSamples());

    MidiBuffer midi;
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
        p.processBlock (view, midi);
    }
    cpuSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    AudioBuffer<float> aligned (in.getNumChannels(), in.getNumSamples());
    for (int c = 0; c < in.getNumChannels(); ++c)
        aligned.copyFrom (c, 0, out, c, latency, in.getNumSamples());
    return aligned;
}

float bufferRms (const AudioBuffer<float>& b)
{
    double s = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            s += (double) b.getSample (c, i) * b.getSample (c, i);
    return (float) std::sqrt (s / jmax (1, b.getNumSamples() * b.getNumChannels()));
}

bool writeWav (const File& f, const AudioBuffer<float>& b, double rate)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    if (os == nullptr)
        return false;
    auto writer = wav.createWriterFor (os, AudioFormatWriterOptions{}
                                               .withSampleRate (rate)
                                               .withNumChannels (b.getNumChannels())
                                               .withBitsPerSample (24));
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

float medianF0 (const AudioBuffer<float>& b, double rate)
{
    vs::YinPitchDetector yin (rate, 50.0f, 2000.0f);
    return yin.medianF0 (b.getReadPointer (0), b.getNumSamples());
}
} // namespace

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);
    auto arg = [&] (const String& key, const String& def) {
        const int i = args.indexOf (key);
        return i >= 0 && i + 1 < args.size() ? args[i + 1] : def;
    };

    const File outDir = File::getCurrentWorkingDirectory().getChildFile (arg ("--out", "bench-out"));
    const double rate = arg ("--rate", "48000").getDoubleValue();
    const int block = arg ("--block", "512").getIntValue();
    outDir.createDirectory();

    if (args.contains ("--screenshot"))
    {
        // Renders the real editor (Easy and Pro pages) to PNG, for documentation/visual checks.
        vs::VocalShiftProcessor p;
        p.prepareToPlay (48000, 512);
        p.getPresetManager().applyFactoryPreset ("urban_spatial");
        for (auto mode : { "easy", "pro" })
        {
            p.getState().state.setProperty (vs::params::propUiMode, mode, nullptr);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (940, 700);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            File f = outDir.getChildFile (String ("ui_") + mode + ".png");
            f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
        }
        // Easy page at the real pixel density of Windows 100 % and 150 % scaling, and with a voice playing
        // (1 s of a synthetic vowel through the processor, so meters, halo and the singer's light react).
        p.getState().state.setProperty (vs::params::propUiMode, "easy", nullptr);
        auto snap = [&] (const char* name, float scale) {
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (940, 700);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            File f = outDir.getChildFile (name);
            f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
        };
        snap ("ui_easy_100.png", 1.0f);
        snap ("ui_easy_150.png", 1.5f);
        {
            auto v = vstest::vowel (48000.0, 220.0, 1.0, 0.25f);
            AudioBuffer<float> buf (2, 512);
            MidiBuffer midi;
            for (size_t pos = 0; pos + 512 <= v.size(); pos += 512)
            {
                for (int c = 0; c < 2; ++c)
                    buf.copyFrom (c, 0, v.data() + pos, 512);
                p.processBlock (buf, midi);
            }
        }
        snap ("ui_easy_voz.png", 2.0f);
        {
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (940, 700);
            for (auto* ch : ed->getChildren())
                if (auto* id = dynamic_cast<vs::IdentityPanel*> (ch))
                {
                    id->createComponentSnapshot (id->getLocalBounds(), true, 1.0f);
                    auto t0 = std::chrono::steady_clock::now();
                    for (int i = 0; i < 200; ++i) id->createComponentSnapshot (id->getLocalBounds(), true, 1.0f);
                    std::printf ("TIMING identity frame (cached): %.3f ms\n", std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / 200.0);
                    t0 = std::chrono::steady_clock::now();
                    for (int i = 0; i < 20; ++i) { id->setBounds (id->getBounds().withWidth (id->getWidth() + (i % 2 ? 1 : -1))); id->createComponentSnapshot (id->getLocalBounds(), true, 1.0f); }
                    std::printf ("TIMING identity rebuild (resize): %.3f ms\n", std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / 20.0);
                }
        }
        std::printf ("Capturas en %s\n", outDir.getFullPathName().toRawUTF8());
        return 0;
    }

    std::vector<Source> sources;
    if (args.contains ("--input"))
    {
        AudioFormatManager fm;
        fm.registerBasicFormats();
        const File inFile = File::getCurrentWorkingDirectory().getChildFile (arg ("--input", ""));
        std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (inFile));
        if (reader == nullptr)
        {
            std::printf ("No se pudo leer %s\n", inFile.getFullPathName().toRawUTF8());
            return 1;
        }
        Source s { inFile.getFileNameWithoutExtension(), AudioBuffer<float> ((int) jmin (2u, reader->numChannels), (int) reader->lengthInSamples), reader->sampleRate };
        reader->read (&s.audio, 0, (int) reader->lengthInSamples, 0, true, s.audio.getNumChannels() > 1);
        sources.push_back (std::move (s));
    }
    else
    {
        for (auto [name, f0] : { std::pair<const char*, double> { "synth_vowel_120Hz", 120.0 }, { "synth_vowel_220Hz", 220.0 } })
        {
            auto v = vstest::vowel (rate, f0, 4.0, 0.25f);
            Source s { name, AudioBuffer<float> (1, (int) v.size()), rate };
            s.audio.copyFrom (0, 0, v.data(), (int) v.size());
            sources.push_back (std::move (s));
        }
    }

    String report;
    report << "# QualityBench\n\nRender: " << String (rate) << " Hz input rate (or file rate), block " << block
           << ". Archivos alineados en latencia e igualados en RMS con el original (columna 'ajuste').\n\n"
           << "| Fuente | Caso | F0 entrada | F0 salida | Error (cents) | Ajuste RMS (dB) | CPU (% tiempo real, 1 núcleo) | Latencia |\n"
           << "|---|---|---|---|---|---|---|---|\n";

    for (auto& src : sources)
    {
        const float dryRms = bufferRms (src.audio);
        const float inF0 = medianF0 (src.audio, src.rate);
        const double seconds = src.audio.getNumSamples() / src.rate;
        writeWav (outDir.getChildFile (src.name + "__00_original.wav"), src.audio, src.rate);

        struct Case { String label; std::function<void (vs::VocalShiftProcessor&)> setup; float expectSt; };
        std::vector<Case> cases;
        auto setP = [] (vs::VocalShiftProcessor& p, const char* id, float v) {
            auto* prm = p.getState().getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        };
        // Pure pitch engine: Character 0 (formants fully preserved), Magia 0, auto level off.
        // The 'Ajuste RMS' column therefore shows the engine's own level change.
        for (int st : { -24, -12, -7, -3, 3, 7, 12, 19, 24 })
            cases.push_back ({ String ("pitch_") + (st > 0 ? "+" : "") + String (st) + "st_preserve100",
                               [st, setP] (vs::VocalShiftProcessor& p) {
                                   setP (p, vs::params::semitones, (float) st);
                                   setP (p, vs::params::character, 0);
                                   setP (p, vs::params::magic, 0);
                                   setP (p, vs::params::autoLevel, 0);
                               }, (float) st });
        // Experiment 01 (reference macros): isolate the effect of mode, Character and Magia.
        // (run with improved = off: exactly the sound of experiment 01 / commit dec340d)
        cases.push_back ({ "exp01_A_ref_identidad_+7_65_70", [setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::improved, 0); }, 7.0f });
        cases.push_back ({ "exp01_B_ref_radical_+7_65_70", [setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::improved, 0); setP (p, vs::params::mode, 1); }, 7.0f });
        cases.push_back ({ "exp01_C_identidad_+7_65_magia0", [setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::improved, 0); setP (p, vs::params::magic, 0); }, 7.0f });
        cases.push_back ({ "exp01_D_identidad_+7_caracter0_70", [setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::improved, 0); setP (p, vs::params::character, 0); }, 7.0f });

        // Experiment 02 (same take, equal RMS): see docs/EXPERIMENT_02_SEGUNDA_VOZ.md. A is the original file.
        // F0 error is only meaningful for single-voice cases; second-voice files hold two voices (expectSt = 999).
        auto second = [setP] (vs::VocalShiftProcessor& p, float st, float character, float magic) {
            setP (p, vs::params::route, 1);
            setP (p, vs::params::semitones, st);
            setP (p, vs::params::character, character);
            setP (p, vs::params::magic, magic);
        };
        cases.push_back ({ "exp02_B_transformar_+7_actual", [setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::improved, 0); }, 7.0f });
        cases.push_back ({ "exp02_C_transformar_+7_mejorado", [] (vs::VocalShiftProcessor&) {}, 7.0f });
        cases.push_back ({ "exp02_D_segunda_voz_+7_-9dB", [second] (vs::VocalShiftProcessor& p) { second (p, 7, 65, 70); }, 999.0f });
        cases.push_back ({ "exp02_E_segunda_voz_+12_-9dB", [second] (vs::VocalShiftProcessor& p) { second (p, 12, 65, 70); }, 999.0f });
        cases.push_back ({ "exp02_F_segunda_voz_+7_caracter0", [second] (vs::VocalShiftProcessor& p) { second (p, 7, 0, 70); }, 999.0f });
        cases.push_back ({ "exp02_G_segunda_voz_+7_caracter65", [second] (vs::VocalShiftProcessor& p) { second (p, 7, 65, 70); }, 999.0f });
        cases.push_back ({ "exp02_H_segunda_voz_+7_magia0", [second] (vs::VocalShiftProcessor& p) { second (p, 7, 65, 0); }, 999.0f });
        cases.push_back ({ "exp02_I_segunda_voz_+7_magia70", [second] (vs::VocalShiftProcessor& p) { second (p, 7, 65, 70); }, 999.0f });
        // Experiment 03 (Phase 4): see docs/EXPERIMENT_03_URBANO_ESPACIAL.md. Same take, equal RMS.
        auto urban = [setP] (vs::VocalShiftProcessor& p, float character, float magic, bool spatial, float delayTrim) {
            p.getPresetManager().applyFactoryPreset ("urban_spatial"); // route, +7 st, -12 dB, identity
            setP (p, vs::params::character, character);
            setP (p, vs::params::magic, magic);
            setP (p, vs::params::spatial, spatial ? 1.0f : 0.0f);
            setP (p, vs::params::spaceDelay, delayTrim);
        };
        cases.push_back ({ "exp03_A_fase3_segunda_voz_+7", [setP] (vs::VocalShiftProcessor& p) { p.getPresetManager().applyFactoryPreset ("second_voice_7"); }, 999.0f });
        cases.push_back ({ "exp03_B_urbano_+7_c65_m70_-12dB", [urban] (vs::VocalShiftProcessor& p) { urban (p, 65, 70, true, 100); }, 999.0f });
        cases.push_back ({ "exp03_C_magia0", [urban] (vs::VocalShiftProcessor& p) { urban (p, 65, 0, true, 100); }, 999.0f });
        cases.push_back ({ "exp03_D_magia100", [urban] (vs::VocalShiftProcessor& p) { urban (p, 65, 100, true, 100); }, 999.0f });
        cases.push_back ({ "exp03_E_caracter0", [urban] (vs::VocalShiftProcessor& p) { urban (p, 0, 70, true, 100); }, 999.0f });
        cases.push_back ({ "exp03_F_caracter100", [urban] (vs::VocalShiftProcessor& p) { urban (p, 100, 70, true, 100); }, 999.0f });
        cases.push_back ({ "exp03_G_espacial_desactivado", [urban] (vs::VocalShiftProcessor& p) { urban (p, 65, 70, false, 100); }, 999.0f });
        cases.push_back ({ "exp03_H_anchura_y_reverb_sin_eco", [urban] (vs::VocalShiftProcessor& p) { urban (p, 65, 70, true, 0); }, 999.0f });
        // Comparison across intervals (improved path) for the engine-naturalness question.
        for (int st : { 3, 7, 12, 19, -5, -12 })
            cases.push_back ({ String ("exp02_intervalo_") + (st > 0 ? "+" : "") + String (st) + "st_mejorado",
                               [st, setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::semitones, (float) st); }, (float) st });
        for (int st : { 3, 7, 12, 19, -5, -12 })
            cases.push_back ({ String ("exp02_intervalo_") + (st > 0 ? "+" : "") + String (st) + "st_actual",
                               [st, setP] (vs::VocalShiftProcessor& p) { setP (p, vs::params::improved, 0); setP (p, vs::params::semitones, (float) st); }, (float) st });
        for (const auto& fp : vs::factoryPresets())
            if (fp.id != "neutral" && fp.id != "reference")
                cases.push_back ({ String ("preset_") + fp.id,
                                   [id = fp.id] (vs::VocalShiftProcessor& p) { p.getPresetManager().applyFactoryPreset (id); },
                                   fp.params.totalSemitones() });

        for (auto& c : cases)
        {
            double cpu = 0;
            int latency = 0;
            auto out = process (src.audio, src.rate, block, c.setup, cpu, latency);
            const float wetRms = bufferRms (out);
            const float match = wetRms > 0 ? dryRms / wetRms : 1.0f;
            out.applyGain (match);
            const float outF0 = medianF0 (out, src.rate);
            const double expected = inF0 * std::pow (2.0, c.expectSt / 12.0);
            const bool single = c.expectSt < 900.0f;
            const double err = single && outF0 > 0 && inF0 > 0 ? vstest::centsBetween (expected, outF0) : 0.0;
            writeWav (outDir.getChildFile (src.name + "__" + c.label + ".wav"), out, src.rate);
            report << "| " << src.name << " | " << c.label << " | " << String (inF0, 1) << " | " << (single ? String (outF0, 1) : String ("n/a (2 voces)"))
                   << " | " << (single ? String (err, 1) : String ("n/a")) << " | " << String (Decibels::gainToDecibels (match), 1)
                   << " | " << String (100.0 * cpu / seconds, 1) << " | " << latency << " (" << String (1000.0 * latency / src.rate, 1) << " ms) |\n";
        }
    }
    report << "\nNota: F0 por YIN (mediana). Las métricas no miden naturalidad; para eso hacen falta escuchas.\n";
    outDir.getChildFile ("RESULTS.md").replaceWithText (report);
    std::printf ("%s", report.toRawUTF8());
    std::printf ("\nArchivos en %s\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}
