# Calidad: pruebas, mediciones y limitaciones

Tres niveles distintos, que no deben confundirse:

| Nivel | Qué significa | Estado |
|---|---|---|
| Calidad técnica | Pruebas automáticas: F0, duración, latencia, NaN, asignaciones, estado | ✅ Pasa (Linux local; Windows en CI) |
| Compatibilidad de host | pluginval y uso real en FL Studio | ✅ pluginval 5 y 10 (Linux). FL Studio: **sin probar** |
| Calidad perceptual | ¿Suena natural con voces humanas? | ❌ **Sin evaluar**. Requiere escuchas |

## Pruebas automatizadas

`ctest` ejecuta dos binarios. Imprimen sus mediciones (`measure:`), así que el log de CI es también un registro.

**vs_core_tests (19 pruebas, motor sin JUCE)**
- F0 a −24, −12, −7, −3, +3, +7, +12, +24 st (tono armónico 220 Hz, YIN). Criterio < 15 cents.
- Ajuste fino +50 / −25 cents. Criterio < 6 cents.
- Latencia reportada = retardo medido; duración de una ráfaga de 0.5 s preservada (44.1/48/96 kHz).
- Silencio digital → salida exactamente 0; la cola vuelve a 0.
- Ruido blanco y onda cuadrada a fondo de escala con automatización aleatoria cada 256 muestras: sin NaN/inf;
  con protección de picos la salida nunca supera 1.0.
- NaN/inf en la entrada no llegan a la salida.
- 0 asignaciones de memoria en `process()`/`setParams()`.
- Salida idéntica bit a bit con bloques de 1, 64, 512, 2048 y tamaños aleatorios; reproducible entre ejecuciones.
- +7 st correcto a 44.1 / 48 / 88.2 / 96 kHz.
- Formantes: con preservación el centroide espectral se mantiene; en modo "seguir" se desplaza (proxy grosero).
- Mezcla 0 % = dry alineado exacto; ganancia exacta.
- Bypass: sin escalón mayor que la señal (seno 220 Hz), salida = entrada retardada (error < 1e-6).
- Saltos aleatorios de ±24 st cada 10 ms: acotado y finito.
- Cada preset: reproducible, distinto del neutro y con el F0 declarado (±20 cents).
- Rendimiento: factor de tiempo real (motor solo y cadena completa con macros de referencia).
- Macros: valores de referencia, reglas de mapeo (Carácter 0 = timbre intacto, radical > identidad, monotonía,
  dirección según el tono, continuidad al cruzar 0 st).
- Macros de referencia en ambos modos: F0 +7 st correcto; el modo radical mueve más el timbre.
- Magia: 0 % = bypass exacto bit a bit; 70 % reduce el rango dinámico ≥ 3 dB y añade energía en agudos.
- Nivel automático: compensa la pérdida a +12 st (±1.5 dB) y nunca sube el silencio.
- Cambio de modo y macros aleatorias durante la reproducción: finito y acotado.

**vs_plugin_tests (12 pruebas, procesador JUCE real)**: valores de referencia en una instancia nueva, migración de estado v1→v2, cada macro y el modo cambian la salida, ida y vuelta del estado completo, datos corruptos ignorados,
presets de fábrica exactos, presets de usuario en disco con control de versión, mono/estéreo × 44.1–96 kHz × bloques
32–2048 (incl. 333), bloques mayores que los anunciados y de 0 muestras, reinicio y `reset()` a mitad, bypass por
parámetro y por host, render offline = render en tiempo real, cambios de preset y bypass durante la reproducción.

## Mediciones (Linux x64, GCC 13, 4 vCPU en la nube, 05-11-2026)

- Error de F0 (tono armónico 220 Hz): entre −1.0 y +0.8 cents de −24 a +24 st. Con vocales sintéticas con vibrato
  (QualityBench): ≤ 5 cents hasta ±7 st, 8 cents a −12 st, 22 cents a −24 st (220 Hz); a −24 st sobre 120 Hz
  (30 Hz) YIN no detecta F0.
- Latencia: 150 ms a cualquier frecuencia de muestreo (7200 muestras a 48 kHz) = 120 ms de ventana + 30 ms por
  repartir el cálculo (`splitComputation`, evita picos de CPU).
- Ataques: con transposición el sonido arranca 10–28 ms antes y termina 37–56 ms después que el original
  (pre-eco/suavizado del método espectral). Con 0 st, exacto.
- CPU: 1.6 % (0 st) a 2.5 % (−12 st + formantes) de un núcleo, estéreo 48 kHz, bloques de 512. No medido en Windows
  ni en el PC del usuario.
- Nivel: con formantes preservados la salida pierde volumen al transponer (QualityBench: ~2 dB a ±3 st, 4–7 dB a
  +12, ~13 dB a +24 st en vocales sintéticas). No hay compensación automática de ganancia todavía.
- pluginval 1.0.4 nivel 5 y nivel 10: SUCCESS (Linux, en proceso, con pruebas de editor bajo Xvfb).

## Limitaciones de calidad identificadas

1. **Transitorios y consonantes** (p, t, k, s) se suavizan/emborronan; esperable en un motor espectral.
2. **Latencia de 150 ms**: no sirve para monitorear mientras se graba. En reproducción FL Studio la compensa (PDC).
3. **Pérdida de volumen** con transposición grande y formantes preservados (ver arriba). Mitigada por el *nivel
   automático* (activado por defecto); sin él sigue ocurriendo.
4. **Formantes**: la estimación es aproximada (lo reconoce el autor de Signalsmith); voces muy graves/agudas o con
   mucha respiración pueden dar un timbre "filtrado".
5. **±24 st** es una prueba de estrés: espera artefactos claros. −24 st sobre una voz grave queda por debajo de 40 Hz.
6. **Voice Morph DSP** cambia el carácter (formantes + brillo), no la identidad de la persona.
7. Las vocales sintéticas no sustituyen a voces reales: ninguna métrica aquí mide naturalidad.
8. **Mapeo de macros provisional**: los máximos de Carácter y la curva de Magia no se han calibrado con escuchas.
9. **Magia** es básica (compresor + shelf). Puede acentuar sibilantes al subir agudos; no hay de-esser.

## QualityBench (renders A/B)

```
vs_bench --out bench-out                      # vocales sintéticas 120 Hz y 220 Hz
vs_bench --out mi-test --input mi_voz.wav     # tu propia grabación (WAV/AIFF/FLAC)
```

Genera `__00_original.wav` y un archivo por caso (±3, ±7, ±12, ±24 st con formantes preservados y cada preset),
**alineados en el tiempo e igualados en RMS** con el original, más `RESULTS.md` con F0, ajuste aplicado, CPU y
latencia. En CI se publica el artefacto `bench-renders-synthetic`.

## Protocolo de escucha (lo que tiene que hacer una persona)

Material: 3–5 frases propias o con permiso (voz masculina y femenina, una frase con muchas consonantes "s/t/k",
una cantada sostenida), 24 bit, sin efectos.

1. Renderiza con `vs_bench --input` (o en FL Studio con el plugin) y escucha con auriculares a volumen fijo.
2. Para cada caso puntúa 1–5: **naturalidad**, **identidad** (¿sigue siendo la misma persona?), **inteligibilidad de
   consonantes**, **artefactos** (metálico, eco, "fase", gorgoteo).
3. Compara a ciegas: original vs ±3, ±7, ±12; anota a partir de qué intervalo deja de ser aceptable.
4. Compara "Naturalidad" 100 % vs 0 % a +7 st: ¿100 % conserva mejor la identidad?
5. Presets: ¿el nombre describe lo que oyes? Anota los que no.
6. Si tienes una herramienta de referencia, renderiza los mismos casos con ella (mismo material, mismo nivel) para
   comparar. Hasta entonces no hay base para afirmar que estamos a su altura.
