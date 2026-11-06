# Fase 5 — Natural Voice Engine (laboratorio)

Estado: **experimental**. Nada de lo que sigue está validado al oído. El motor por defecto sigue siendo el de la Fase 4.

## 1. Auditoría (hechos comprobados con pruebas)
- Pitch: error de F0 < 5 cents con Signalsmith y PSOLA en +3…+19 y −5/−12 st sobre vocal sintética (`vs_core_tests`, `vs_lab`).
- Latencia: 7200 muestras a 48 kHz (150 ms) en ambos motores; PSOLA se rellena hasta igualar la referencia (si no pudiera, el motor alternativo queda no disponible).
- Defecto previo documentado: ninguno nuevo reproducido como clic medible en el slap por BPM (ver §4).
- Límites que NO se pueden medir con señales sintéticas: naturalidad, "artificialidad" de la segunda voz, aspereza de PSOLA en voces reales (respiración, falsete, ruido mezclado).

## 2. Motor PSOLA experimental (`src/core/dsp/PsolaPitchShifter.*`)
- Seguidor de F0 YIN sobre mezcla mono diezmada (salto 5 ms), refinado a tasa completa; voz 65–450 Hz.
- Tramos con voz: granos Hann de 2 períodos anclados a picos, re-espaciados por 1/ratio. Los formantes se quedan donde estaban; el cambio de formantes remuestrea cada grano.
- Tramos sin voz (consonantes, aire): los granos conservan el espaciado de análisis, por lo que pasan sin transponer.
- Duración conservada por construcción; latencia fija; sin asignación de memoria en `process()`; resultado independiente del tamaño de bloque (diferencia 0 en prueba).
- Limitaciones conocidas: octavas erróneas del F0 pueden producir saltos de tono; voces muy graves (<65 Hz) o muy agudas (>450 Hz) caen en "sin voz"; el pitch-down por debajo de −12 st deja huecos entre granos (se limita el factor a 0,5).
- Selección: Pro → "Motor experimental PSOLA" (parámetro `engine`, por defecto 0 = Fase 4). El cambio de motor corre ambos ~0,3 s y hace un fundido de 100 ms; medido sin clic y sin asignaciones.
- Presets antiguos y proyectos v1–v4 migran a `engine = 0` (misma ruta de audio que la Fase 4; los tests golden siguen pasando).

## 3. Laboratorio (`vs_lab`)
`vs_lab --out dir` produce `METRICS.md`, pares `ab/*__A_fase4.wav` / `__B_psola.wav`, `MANIFEST.md`, y `blind/*.wav` con nombres aleatorios (clave en `blind_KEY.md`). Una sola ganancia global por archivo iguala el RMS de la entrada (sin normalización por regiones). `--perf` genera `PERF.md`. `--input voz.wav` (48 kHz mono) usa una voz real.

## 4. Espacial
Auditoría: la ruta espacial de la Fase 4 se mantiene idéntica a tempo constante. Cambio: el tiempo del slap sigue al BPM del host con una rampa (máx. 0,4 % de cambio de retardo por muestra) en lugar de saltar. Con una senoidal no se midió diferencia de salto (0,0142 sin rampa vs 0,0195 con ella), así que es una medida preventiva, no una corrección demostrada. Movimiento y ducking opcionales **no se implementaron** en esta fase (ver informe).

## 5. Macros
Sin cambios en las curvas de la Fase 4 (Magia: ancho 0,95 m^1,4; reverb 0,55 m^1,5; RT60 0,8+m; slap 0,28·((m−0,4)/0,6)^1,2; modulación 0,2+0,5 m). Ningún control principal nuevo.

## 6. Presets nuevos (ids nuevos; los antiguos intactos)
second_voice_natural, melodic_rap, pop_bright, dark_vocal, rock_texture, dream_atmosphere, high_creative (todos con el motor de la Fase 4) y `exp_psola_second_voice` (motor PSOLA, marcado [EXPERIMENTAL]). Valores de ingeniería, sin ajuste por oído.

## 7. Voz neuronal (solo investigación; no se descargó ni entrenó nada)
Búsqueda web de esta sesión devolvió resultados escasos; lo que sigue es conocimiento previo **no verificado** y debe comprobarse antes de decidir:
| Familia | Idea | Licencia (por verificar) | Latencia / coste | Riesgo |
|---|---|---|---|---|
| RVC (conversión basada en recuperación) | convierte timbre con un modelo por voz | repositorio MIT (verificar pesos y datasets) | requiere GPU para tiempo real; cientos de ms | consentimiento y derechos de la voz objetivo; modelo de 100+ MB; difícil en un VST3 |
| OpenVoice / so-vits | clonación de timbre | verificar (MIT/otras) | offline más viable | mismo riesgo legal |
| DDSP | síntesis armónica diferenciable | Apache-2.0 (verificar) | ligera, apta para CPU, pero requiere entrenar por voz | calidad dependiente del entrenamiento |
Recomendación: no integrar nada neuronal en el VST3 todavía. Si se explora, hacerlo fuera de línea (herramienta separada) con voces propias y consentimiento documentado.

## 8. Cómo comprobar
`ninja -C build vs_core_tests vs_plugin_tests vs_lab` y ejecutar los tests; `vs_lab --out lab-out`; escuchar `blind/` antes de abrir `blind_KEY.md`.
