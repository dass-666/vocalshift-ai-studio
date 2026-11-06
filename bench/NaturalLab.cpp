// NaturalLab (Fase 5): laboratorio de comparación Fase 4 (Signalsmith) vs motor experimental PSOLA.
// Renderiza por VocalChain (sin host), mide y exporta paquetes A/B. NO emite veredictos perceptuales.
//
//   vs_lab --out <dir> [--input voz.wav] [--seed 42]
// Salidas:
//   METRICS.md          tabla de métricas por motor e intervalo
//   ab/<caso>__A|B.wav  pares con orden fijo documentado en MANIFEST.md (A = Fase 4, B = experimental)
//   blind/<hex>.wav     mismos pares con nombre aleatorio; la clave está en blind_KEY.md (no abrir antes de escuchar)
// Igualación de nivel: UNA sola ganancia por archivo (RMS global igual al de la entrada), nunca por regiones.

#include <core/VocalChain.h>
#include <core/dsp/HybridPitchShifter.h>
#include <core/analysis/PitchDetector.h>
#include "../tests/Signals.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <random>

using namespace juce;
using Sig = std::vector<float>;

namespace
{
constexpr double kRate = 48000.0;
#ifndef VS_GIT_COMMIT
#define VS_GIT_COMMIT "unknown"
#endif
const char* const kEngineNames[3] = { "Fase 4 (Signalsmith)", "PSOLA experimental", "Hibrido experimental" };


// Vocal + fricativa sintética ("a" - "sh" - "a") para vigilar consonantes.
Sig makeTake (double f0)
{
    auto v1 = vstest::vowel (kRate, f0, 1.5, 0.25f, 5.0, 30.0);
    auto v2 = vstest::vowel (kRate, f0 * 1.12, 1.5, 0.25f, 5.0, 30.0);
    auto n = vstest::whiteNoise ((int) (0.25 * kRate), 0.12f, 5);
    // paso alto simple (diferencia) para parecerse a /s/
    for (size_t i = n.size() - 1; i > 0; --i) n[i] = n[i] - 0.9f * n[i - 1];
    Sig x;
    x.insert (x.end(), v1.begin(), v1.end());
    x.insert (x.end(), n.begin(), n.end());
    x.insert (x.end(), v2.begin(), v2.end());
    return x;
}

Sig render (const Sig& in, int engine, float st, double& cpuPct, int& latency, int& activeEngine, int block = 512, bool trim = true)
{
    vs::EngineSettings e;
    e.semitones = st; e.formantPreservation = 1.0f; e.magic01 = 0.0f; e.autoLevel = false; e.safetyClip = false;
    e.pitchEngine = engine == 1 ? 1 : 0;
    vs::VocalChain chain (engine == 2 ? std::unique_ptr<vs::IPitchShifter> (std::make_unique<vs::HybridPitchShifter>()) : nullptr);
    chain.setEngineSettings (e, false);
    chain.prepare (kRate, 1, block);
    latency = chain.latencySamples();
    activeEngine = engine == 2 ? 2 : chain.activeEngineIndex();
    Sig x = in;
    x.resize (in.size() + (size_t) latency, 0.0f);
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t p = 0; p < x.size(); p += (size_t) block)
    {
        float* ptr = x.data() + p;
        chain.process (&ptr, 1, (int) std::min<size_t> ((size_t) block, x.size() - p));
    }
    cpuPct = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / (x.size() / kRate);
    return trim ? Sig (x.begin() + latency, x.end()) : x;
}

double rms (const Sig& x, size_t a, size_t b)
{
    double s = 0;
    for (size_t i = a; i < b && i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / std::max<size_t> (1, b - a));
}

struct Metrics { double f0Err = 0, jitterCents = 0, vibratoRatio = 0, centroidRatio = 0, levelLossDb = 0, peak = 0, consonantRatio = 0, durationDiffMs = 0; };

std::vector<double> f0TrackCents (const Sig& x, size_t a, size_t b)
{
    vs::YinPitchDetector yin (kRate, 50.0f, 1500.0f);
    std::vector<double> t;
    for (size_t p = a; p + 4096 <= b; p += 512)
    {
        const auto est = yin.analyse (x.data() + p, 4096);
        if (est.frequencyHz > 0) t.push_back (1200.0 * std::log2 (est.frequencyHz / 100.0));
    }
    return t;
}

Metrics measure (const Sig& in, const Sig& out, float st)
{
    Metrics m;
    const size_t v1a = 12000, v1b = (size_t) (1.45 * kRate); // primera vocal, sin bordes
    auto tin = f0TrackCents (in, v1a, v1b), tout = f0TrackCents (out, v1a, v1b);
    auto med = [] (std::vector<double> v) { if (v.empty()) return 0.0; std::sort (v.begin(), v.end()); return v[v.size() / 2]; };
    auto spread = [] (std::vector<double> v) { if (v.size() < 4) return 0.0; std::sort (v.begin(), v.end());
                                               return 0.5 * (v[(size_t) (0.95 * (v.size() - 1))] - v[(size_t) (0.05 * (v.size() - 1))]); };
    m.f0Err = med (tout) - med (tin) - 100.0 * st;
    m.vibratoRatio = spread (tin) > 1e-6 ? spread (tout) / spread (tin) : 0.0;
    // jitter: desviación de la pista respecto a su media móvil de 5 cuadros
    double acc = 0; int n = 0;
    for (size_t i = 2; i + 2 < tout.size(); ++i) { double avg = 0; for (int k = -2; k <= 2; ++k) avg += tout[i + k]; avg /= 5; acc += (tout[i] - avg) * (tout[i] - avg); ++n; }
    m.jitterCents = n ? std::sqrt (acc / n) : 0.0;
    m.centroidRatio = vs::spectralCentroidHz (out.data() + 24000, 4096, kRate) / std::max (1.0f, vs::spectralCentroidHz (in.data() + 24000, 4096, kRate));
    m.levelLossDb = 20.0 * std::log10 (rms (out, v1a, v1b) / std::max (1e-9, rms (in, v1a, v1b)));
    for (float v : out) m.peak = std::max (m.peak, (double) std::abs (v));
    const size_t ca = (size_t) (1.55 * kRate), cb = (size_t) (1.7 * kRate);
    m.consonantRatio = 20.0 * std::log10 (rms (out, ca, cb) / std::max (1e-9, rms (in, ca, cb)));
    auto lastAbove = [] (const Sig& x) { for (size_t i = x.size(); i-- > 0;) if (std::abs (x[i]) > 0.01f) return (double) i; return 0.0; };
    m.durationDiffMs = 1000.0 * (lastAbove (out) - lastAbove (in)) / kRate;
    return m;
}

bool writeWav (const File& f, const Sig& s)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    auto w = wav.createWriterFor (os, AudioFormatWriterOptions{}.withSampleRate (kRate).withNumChannels (1).withBitsPerSample (24));
    if (w == nullptr) return false;
    AudioBuffer<float> b (1, (int) s.size());
    b.copyFrom (0, 0, s.data(), (int) s.size());
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

Sig matchedTo (Sig x, const Sig& ref) // una sola ganancia global
{
    const double g = rms (ref, 0, ref.size()) / std::max (1e-9, rms (x, 0, x.size()));
    for (auto& v : x) v = (float) (v * g);
    return x;
}

// ---- Auditoría ampliada (Fase 5.1) ---------------------------------------------------------------
struct Cont { int octaveJumps = 0; double maxJumpCents = 0; int voiced = 0; };

Cont continuity (const Sig& x, size_t a, size_t b)
{
    vs::YinPitchDetector yin (kRate, 50.0f, 1500.0f);
    Cont c;
    double prev = 0;
    for (size_t p = a; p + 2048 <= b; p += 512)
    {
        const auto e = yin.analyse (x.data() + p, 2048);
        if (e.frequencyHz <= 0) { prev = 0; continue; }
        ++c.voiced;
        if (prev > 0)
        {
            const double d = std::abs (1200.0 * std::log2 (e.frequencyHz / prev));
            if (d >= 500.0) ++c.octaveJumps;
            c.maxJumpCents = std::max (c.maxJumpCents, d);
        }
        prev = e.frequencyHz;
    }
    return c;
}

double db (double a, double b) { return 20.0 * std::log10 (std::max (1e-9, a) / std::max (1e-9, b)); }

// Transitorios: ráfagas tipo "t" (8 ms de ruido decreciente) sobre silencio.
Sig makeBursts()
{
    Sig x ((size_t) (2.5 * kRate), 0.0f);
    auto n = vstest::whiteNoise (400, 0.8f, 11);
    for (int k = 0; k < 4; ++k)
        for (size_t i = 0; i < n.size(); ++i)
            x[(size_t) ((0.4 + 0.5 * k) * kRate) + i] = n[i] * std::exp (-(float) i / 100.0f);
    return x;
}

String auditSections (const Sig& take, bool synthetic)
{
    String md;
    const float st = 7.0f;
    // 1) Octavas y continuidad de F0 (toda la toma)
    md << String::fromUTF8 ("\n## Saltos de octava y continuidad de F0 (+7 st)\n\n| Motor | Cuadros con voz | Saltos ≥ 500 cents | Mayor salto entre cuadros (cents) |\n|---|---|---|---|\n");
    Sig outs[3];
    for (int e = 0; e < 3; ++e)
    {
        double cpu; int lat, act;
        outs[e] = render (take, e, st, cpu, lat, act);
        const auto c = continuity (outs[e], 12000, outs[e].size());
        md << "| " << kEngineNames[e] << " | " << c.voiced << " | " << c.octaveJumps << " | " << String (c.maxJumpCents, 0) << " |\n";
    }
    const auto cin = continuity (take, 12000, take.size());
    md << "| (entrada) | " << cin.voiced << " | " << cin.octaveJumps << " | " << String (cin.maxJumpCents, 0) << " |\n";

    // 2) Energía por regiones (solo toma sintética: se conocen las regiones)
    if (synthetic)
    {
        md << String::fromUTF8 ("\n## Energía por regiones, salida/entrada (dB, +7 st, sin igualar nivel)\n\n| Motor | Vocal 1 | Fricativa | Transición (±20 ms) | Vocal 2 |\n|---|---|---|---|---|\n");
        auto reg = [&] (const Sig& y, double t0, double t1) { return db (rms (y, (size_t) (t0 * kRate), (size_t) (t1 * kRate)), rms (take, (size_t) (t0 * kRate), (size_t) (t1 * kRate))); };
        for (int e = 0; e < 3; ++e)
        {
            const double trans = 0.5 * (reg (outs[e], 1.48, 1.52) + reg (outs[e], 1.73, 1.77));
            md << "| " << kEngineNames[e] << " | " << String (reg (outs[e], 0.3, 1.4), 1) << " | " << String (reg (outs[e], 1.55, 1.7), 1) << " | " << String (trans, 1)
               << " | " << String (reg (outs[e], 1.9, 2.9), 1) << " |\n";
        }
    }

    // 3) Transitorios
    md << String::fromUTF8 ("\n## Transitorios (ráfagas de ruido de 8 ms, +7 st)\n\n| Motor | Pico salida/entrada (dB) | Energía previa a la ráfaga (dB re ráfaga) | Duración a −20 dB (ms) |\n|---|---|---|---|\n");
    const Sig bursts = makeBursts();
    for (int e = 0; e < 3; ++e)
    {
        double cpu; int lat, act;
        const Sig y = render (bursts, e, st, cpu, lat, act);
        double pkIn = 0, pkOut = 0;
        for (size_t i = (size_t) (0.4 * kRate); i < (size_t) (0.5 * kRate); ++i) { pkIn = std::max (pkIn, (double) std::abs (bursts[i])); pkOut = std::max (pkOut, (double) std::abs (y[i])); }
        const size_t on = (size_t) (0.4 * kRate);
        const double pre = db (rms (y, on - (size_t) (0.02 * kRate), on), rms (y, on, on + 400));
        size_t last = on;
        for (size_t i = on; i < on + (size_t) (0.1 * kRate); ++i) if (std::abs (y[i]) > pkOut * 0.1) last = i;
        md << "| " << kEngineNames[e] << " | " << String (db (pkOut, pkIn), 1) << " | " << String (pre, 1) << " | " << String (1000.0 * (last - on) / kRate, 1) << " |\n";
    }

    // 4) Vibrato (extensión en cents, entrada vs salida)
    md << String::fromUTF8 ("\n## Vibrato conservado (primera vocal)\n\n| Motor | Extensión entrada (cents) | Extensión salida (cents) | Razón |\n|---|---|---|---|\n");
    for (int e = 0; e < 3; ++e)
    {
        const auto m = measure (take, outs[e], st);
        auto tin = f0TrackCents (take, 12000, (size_t) (1.45 * kRate));
        std::sort (tin.begin(), tin.end());
        const double ext = tin.size() > 4 ? 0.5 * (tin[(size_t) (0.95 * (tin.size() - 1))] - tin[(size_t) (0.05 * (tin.size() - 1))]) : 0.0;
        md << "| " << kEngineNames[e] << " | " << String (ext, 1) << " | " << String (ext * m.vibratoRatio, 1) << " | " << String (m.vibratoRatio, 2) << " |\n";
    }

    // 5) Latencia medida vs informada (impulso)
    md << String::fromUTF8 ("\n## Latencia (impulso a 0 st, retardo medido del pico)\n\n| Motor | Informada (muestras) | Medida (muestras) |\n|---|---|---|\n");
    for (int e = 0; e < 3; ++e)
    {
        Sig imp ((size_t) (1.0 * kRate), 0.0f);
        const auto burst = vstest::vowel (kRate, 200.0, 0.05, 0.5f, 0.0, 0.0);
        std::copy (burst.begin(), burst.end(), imp.begin() + 4800);
        double cpu; int lat, act;
        const Sig raw = render (imp, e, 0.0f, cpu, lat, act, 512, false);
        // retardo = correlación cruzada por el máximo en una ventana ±3000 muestras alrededor de lat
        double best = -1e30; int bestLag = 0;
        for (int L = lat - 3000; L <= lat + 3000; L += 1)
        {
            double sdot = 0;
            for (size_t i = 0; i < burst.size(); ++i) { const size_t j = 4800 + i + (size_t) L; if (j < raw.size()) sdot += (double) raw[j] * burst[i]; }
            if (sdot > best) { best = sdot; bestLag = L; }
        }
        md << "| " << kEngineNames[e] << " | " << lat << " | " << bestLag << " |\n";
    }

    // 6) Tamaño de búfer: diferencia respecto al bloque de 512
    md << String::fromUTF8 ("\n## Diferencia entre tamaños de búfer (+7 st; referencia = bloque 512)\n\n| Motor | Bloque | Diferencia máx. | Diferencia RMS (dB re señal) |\n|---|---|---|---|\n");
    for (int e = 0; e < 3; ++e)
    {
        double cpu; int lat, act;
        const Sig ref = render (take, e, st, cpu, lat, act, 512);
        for (int blk : { 64, 480, 1024 })
        {
            const Sig y = render (take, e, st, cpu, lat, act, blk);
            double mx = 0, ds = 0;
            for (size_t i = 0; i < y.size() && i < ref.size(); ++i) { const double d = y[i] - ref[i]; mx = std::max (mx, std::abs (d)); ds += d * d; }
            md << "| " << kEngineNames[e] << " | " << blk << " | " << String (mx, 6) << " | " << String (db (std::sqrt (ds / y.size()), rms (ref, 0, ref.size())), 1) << " |\n";
        }
    }
    return md;
}

// Perfil de CPU (% de tiempo real en un núcleo), memoria y latencia por configuración.
int runPerf (const File& out)
{
    String md = String::fromUTF8 ("# Perfil de rendimiento (Fase 5)\n\nCPU = tiempo de proceso / duración del audio, 1 núcleo, 10 s de vocal sintética, compilación Release+LTO. Máquina de CI/contenedor: sirve para comparar configuraciones entre sí, no como cifra absoluta para tu PC.\n\n| Configuración | Fs | Bloque | Canales | CPU % | Latencia (muestras) | Latencia (ms) | Memoria espacial (kB) | prepare() (ms) |\n|---|---|---|---|---|---|---|---|---|\n");
    struct Cfg { const char* name; int engine; bool second; };
    for (Cfg c : { Cfg { "Fase 4 · transformar", 0, false }, Cfg { "PSOLA · transformar", 1, false },
                   Cfg { "Fase 4 · 2ª voz + espacial", 0, true }, Cfg { "PSOLA · 2ª voz + espacial", 1, true } })
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (int block : { 64, 512, 1024 })
                for (int ch : { 1, 2 })
                {
                    if ((fs != 48000.0 && (block != 512 || ch != 2)) || (fs == 48000.0 && block != 512 && ch == 1)) continue;
                    vs::VoiceParams p;
                    p.engine = c.engine;
                    if (c.second) { p.route = vs::VoiceRoute::secondVoice; p.spatial = true; p.secondGainDb = -12.0f; }
                    vs::VocalChain chain;
                    chain.setParams (p, false);
                    const auto tp = std::chrono::steady_clock::now();
                    chain.prepare (fs, ch, block);
                    const double prepMs = 1000.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - tp).count();
                    auto v = vstest::vowel (fs, 190.0, 10.0, 0.25f);
                    std::vector<Sig> io ((size_t) ch, v);
                    std::vector<float*> ptr ((size_t) ch);
                    const auto t0 = std::chrono::steady_clock::now();
                    for (size_t pos = 0; pos + (size_t) block <= v.size(); pos += (size_t) block)
                    {
                        for (int k = 0; k < ch; ++k) ptr[(size_t) k] = io[(size_t) k].data() + pos;
                        chain.process (ptr.data(), ch, block);
                    }
                    const double cpu = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / (v.size() / fs);
                    md << "| " << String::fromUTF8 (c.name) << " | " << String (fs, 0) << " | " << block << " | " << ch << " | " << String (cpu, 2) << " | "
                       << chain.latencySamples() << " | " << String (1000.0 * chain.latencySamples() / fs, 1) << " | "
                       << String (chain.spatialMemoryBytes() / 1024.0, 0) << " | " << String (prepMs, 1) << " |\n";
                }
    out.getChildFile ("PERF.md").replaceWithText (md);
    std::printf ("%s\n", md.toRawUTF8());
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto arg = [&] (const String& k, const String& d) { const int i = args.indexOf (k); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    const File out = File::getCurrentWorkingDirectory().getChildFile (arg ("--out", "lab-out"));
    for (auto* d : { "", "ab", "blind" }) out.getChildFile (d).createDirectory();
    if (args.contains ("--perf")) return runPerf (out);
    std::mt19937 rng ((uint32_t) arg ("--seed", "42").getIntValue());

    Sig take = makeTake (190.0);
    if (args.contains ("--input"))
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (File::getCurrentWorkingDirectory().getChildFile (arg ("--input", ""))));
        if (r == nullptr || r->sampleRate != kRate) { std::printf ("Entrada ilegible o no es de 48 kHz\n"); return 1; }
        AudioBuffer<float> b (1, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, false);
        take.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
    }

    const String commit (VS_GIT_COMMIT);
    const uint32_t seed = (uint32_t) arg ("--seed", "42").getIntValue();
    const bool synthetic = ! args.contains ("--input");
    String md = String::fromUTF8 ("# NaturalLab: Fase 4 vs PSOLA experimental vs híbrido experimental\n\nCommit: ") + commit + ", semilla " + String ((int) seed)
              + String::fromUTF8 ("\n\nFuente: ")
              + (synthetic ? String::fromUTF8 ("vocal+fricativa sintética (no es una voz humana)") : String ("archivo del usuario"))
              + String::fromUTF8 (". Pitch puro: Carácter 0, Magia 0, nivel auto apagado. Sin veredictos perceptuales; las métricas sintéticas no deciden qué motor es mejor.\n\n"
                "| Motor | st | Error F0 (cents) | Jitter (cents) | Vibrato conservado | Centroide × | Nivel vocal (dB) | Consonante (dB) | Pico | Dif. duración (ms) | CPU % | Latencia |\n"
                "|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    String manifest = String::fromUTF8 ("# MANIFEST\n\nA = Fase 4 (Signalsmith Stretch 1.3.2, motor por defecto). B = PSOLA experimental. C = híbrido experimental (aislado, no está en el VST3). Misma entrada, mismos parámetros, misma latencia.\nNivel: una ganancia global por archivo hasta igualar el RMS de la entrada.\n")
                      + "Commit: " + commit + ", semilla: " + String ((int) seed) + "\n\n| Caso | Archivo | Algoritmo | Latencia | Parámetros |\n|---|---|---|---|---|\n";
    String key = String::fromUTF8 ("# CLAVE ciega (no abrir antes de escuchar)\n\nCommit: ") + commit + ", semilla: " + String ((int) seed) + "\n\n| Archivo | Caso | Algoritmo |\n|---|---|---|\n";
    struct Blind { String name, label, engine; };
    std::vector<Blind> blind;
    const char* const tags[3] = { "A_fase4", "B_psola", "C_hibrido" };

    for (float st : { 3.f, 7.f, 12.f, 19.f, -5.f, -12.f })
    {
        Sig outs[3];
        const String label = "pitch_" + String (st > 0 ? "+" : "") + String ((int) st) + "st";
        for (int e = 0; e < 3; ++e)
        {
            double cpu = 0; int lat = 0, act = 0;
            outs[e] = render (take, e, st, cpu, lat, act);
            if (act != e) { std::printf ("Motor %d no disponible\n", e); return 2; }
            const auto m = measure (take, outs[e], st);
            md << "| " << kEngineNames[e] << " | " << String (st, 0) << " | " << String (m.f0Err, 1) << " | " << String (m.jitterCents, 1)
               << " | " << String (m.vibratoRatio, 2) << " | " << String (m.centroidRatio, 2) << " | " << String (m.levelLossDb, 1) << " | "
               << String (m.consonantRatio, 1) << " | " << String (m.peak, 2) << " | " << String (m.durationDiffMs, 1) << " | " << String (cpu, 2) << " | " << lat << " |\n";
            outs[e] = matchedTo (outs[e], take);
            const String file = label + "__" + tags[e] + ".wav";
            writeWav (out.getChildFile ("ab/" + file), outs[e]);
            manifest << "| " << label << " | " << file << " | " << kEngineNames[e] << " | " << lat << " | semitones=" << String (st, 0) << String::fromUTF8 (", preservación 100 %, Magia 0 |\n");
            out.getChildFile ("ab/" + file + ".json").replaceWithText ("{ \"algorithm\": \"" + String (kEngineNames[e]) + "\", \"commit\": \"" + commit + "\", \"semitones\": " + String (st, 1)
                + ", \"latency_samples\": " + String (lat) + ", \"rate\": 48000, \"source\": \"" + (synthetic ? "synthetic" : "user") + "\" }\n");
        }
        for (int e = 0; e < 3; ++e)
        {
            char hex[16]; std::snprintf (hex, sizeof hex, "%08x", (unsigned) rng());
            writeWav (out.getChildFile (String ("blind/") + String (hex) + ".wav"), outs[e]);
            blind.push_back ({ String (hex) + ".wav", label, kEngineNames[e] });
        }
    }
    std::shuffle (blind.begin(), blind.end(), rng);
    for (auto& b : blind) key << "| " << b.name << " | " << b.label << " | " << b.engine << " |\n";
    writeWav (out.getChildFile ("ab/00_original.wav"), take);
    md << String::fromUTF8 ("\nNota: Error F0, jitter y vibrato vienen de YIN sobre la primera vocal; 'Consonante' es la variación de RMS en la fricativa (0 = intacta). Ninguna métrica mide naturalidad.\n");
    md << auditSections (take, synthetic);
    out.getChildFile ("METRICS.md").replaceWithText (md);
    out.getChildFile ("MANIFEST.md").replaceWithText (manifest);
    out.getChildFile ("blind_KEY.md").replaceWithText (key);
    std::printf ("%s\n", md.toRawUTF8());
    return 0;
}
