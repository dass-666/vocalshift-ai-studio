# Arquitectura

```
src/core/        C++ puro, sin JUCE: se prueba de forma aislada (vs_core)
  dsp/           IdentityEngine: IPitchShifter (interfaz) + SignalsmithPitchShifter, LinearSmoother
  morph/         MorphEngine: ToneShaper (shelf, DSP) + INeuralVoiceConverter (solo interfaz, sin implementar)
  enhance/       VocalEnhancer ("Magia": compresor + aire)
  mix/           VocalMixEngine (dry retardado, mezcla, bypass, ganancia, soft clip) + LevelMatcher (nivel automático)
  analysis/      VocalAnalysis: YIN (F0 + aperiodicidad), RMS, centroide. Offline / tests por ahora
  state/         VoiceParams (macros), MacroMapping (macros -> EngineSettings), presets de fábrica
  VocalChain     Cadena completa: entrada -> pitch/formantes -> brillo -> mezcla -> salida
src/plugin/      PluginHost (JUCE): parámetros APVTS, estado versionado, PresetManager, EasyUI/ProUI
tests/           CoreTests (DSP) y PluginTests (procesador real, sin host)
bench/           QualityBench: renders A/B igualados en RMS + mediciones
```

## Flujo de señal

```
Macros (Pitch, Carácter, Magia, modo) ──> mapMacros() ──> EngineSettings

entrada ──┬─> IPitchShifter (tono + formantes) ─> ToneShaper (brillo) ─> VocalEnhancer (Magia)
          │        ─> LevelMatcher (nivel automático, referencia = dry) ─> wet ─┐
          └─> línea de retardo = latencia del motor ───────────────────> dry ───┴─> VocalMixEngine ─> salida
```

## Reglas del hilo de audio (verificadas)

- `prepare()` es el único sitio que reserva memoria. La prueba `realtime: zero heap allocations` sustituye
  `operator new` y comprueba 0 asignaciones durante `process()`/`setParams()` con tamaños de bloque variables.
- Sin locks, archivos ni red en `processBlock`. Presets y archivos solo en el hilo de mensajes.
- Parámetros leídos con atómicos; suavizado lineal (50 ms tono/formantes, 30 ms mezcla/ganancia, 20 ms bypass).
- Red de seguridad: entradas NaN/inf se sustituyen por 0; si la salida tuviera valores no finitos se silencia el
  bloque y se reinicia el motor (contador visible en modo Pro).
- La latencia es fija tras `prepare()` y se reporta con `setLatencySamples()`. El bypass sigue procesando para que la
  latencia no cambie y la reactivación no haga clic.

## Motor intercambiable

`IPitchShifter` define `prepare / reset / setSettings / process / latencySamples`. Para comparar otro motor
(PSOLA para voz monofónica, WORLD-like, Rubber Band, comercial), se implementa la interfaz y se pasa a `VocalChain`.
Las pruebas DSP y QualityBench funcionan igual con cualquier motor, lo que permite una comparación A/B objetiva.

## Motor neuronal (futuro, no implementado)

`src/core/morph/NeuralVoiceConverter.h` fija el contrato: carga de modelo y conversión por *hop* fuera del hilo de
audio, comunicación por FIFOs sin bloqueo, fallback a DSP/dry con fundido si el trabajador no llega a tiempo, y
latencia fija declarada. Primero modo **Studio** (render offline de una región); modo **Live** solo si las mediciones
en hardware objetivo lo permiten.

## Módulos reservados (sin código todavía, a propósito)

- **HarmonyEngine**: N voces = N instancias de `IPitchShifter` con intervalos diatónicos (requiere F0 + tonalidad de
  `VocalAnalysis`), variaciones humanas de tiempo/afinación, paneo por voz en `VocalMixEngine`.
- **Doblaje**: copias con micro-desafinación y desfase temporal variable.
- **Edición por frases**: segmentación sonoro/no sonoro desde `VocalAnalysis`; requiere modelo de región del host
  (ARA o captura) — decisión pendiente.
