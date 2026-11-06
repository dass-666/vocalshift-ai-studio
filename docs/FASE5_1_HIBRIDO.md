# Fase 5.1 — Auditoría de PSOLA y motor híbrido experimental

Estado: **experimental y aislado**. El motor por defecto (Fase 4) y el preset Urbano Espacial no cambian. Nada de esto está validado al oído.

## 1. Auditoría de PSOLA (pruebas reproducibles en `vs_core_tests` y `vs_lab`)
| Punto revisado | Resultado | Evidencia |
|---|---|---|
| Rango de F0 | **Defecto real**: F0 de entrada > 450 Hz no se transponía (pasaba sin cambio). Corregido a 700 Hz. Error ≤ 0,3 cents de 80 a 690 Hz con +7 st | test "psola audit: input F0 80-690 Hz" |
| Errores de octava | 0 saltos ≥ 500 cents en vocal con vibrato, voz grave (80 Hz), voz con jitter del 10 %, dos voces simultáneas y salto abrupto 200→400 Hz | `vs_lab` (METRICS) + sondeo de auditoría |
| Ruido | Con SNR de 40 a 6 dB el 94 % de los granos son con voz y el F0 de salida coincide (< 0,7 Hz); a 0 dB la detección falla (entra como sin voz, sin errores numéricos) | sondeo `vf` |
| Cambios abruptos de periodicidad, F0 fuera de rango | Sin NaN, pico acotado, saltos de muestra ≤ entrada; fuera de rango (40 Hz, 1 kHz) pasa sin transponer (límite documentado) | sondeo |
| Anclaje a picos positivos | Introduce un desfase temporal de hasta medio período: retardo medido −25 muestras (200 Hz), +16 (110 Hz), −21 (300 Hz) frente a 7200 informadas. Es inherente a TD-PSOLA; no se corrigió | test "timing offset stays within half a period" |
| Huecos entre granos / normalización | RMS por período constante (variación < 0,3 %) en −12/−7/−5/+5 st con vocal estable | sondeo |
| Transiciones voz/consonante | salto máximo de muestra = el de la entrada (relación 1,00) | test "transitions" |
| Vibrato | 100 cents a 7 Hz seguido sin cuadros fuera de ±60 cents (0/231) | test "wide vibrato" |
| Automatización brusca (±9 st cada 100 ms) | sin discontinuidades nuevas (0,91 × salto de entrada) | test |
| Latencia | informada 7200 = referencia; desfase de ±medio período (arriba) | `vs_lab` |
Lo que NO se pudo comprobar: voces humanas reales (respiración, falsete, fry real, sibilancia mezclada, grabaciones con ruido de sala).

## 2. Motor híbrido (`HybridPitchShifter`, aislado: sólo `vs_lab`, pruebas e inyección en `VocalChain`)
- Ejecuta Signalsmith (referencia) y PSOLA en paralelo con la misma latencia y mezcla por muestra.
- PSOLA sólo donde su análisis es fiable (periodicidad + estabilidad del período entre saltos de 5 ms; confianza 0–1 por muestra de salida).
- Protecciones: confianza ≥ 0,8 durante 40 ms para entrar; < 0,5 durante 10 ms para salir; permanencia mínima 80 ms; pendiente lineal de 20 ms (nunca conmutación dura).
- Resultado en la toma sintética (vocal–ruido–vocal): 8 conmutaciones en 4 vocales, 70 % de la señal con PSOLA, salto máximo de muestra idéntico al de Signalsmith solo, latencia 7200, sin diferencias entre tamaños de bloque (0), sin asignaciones en `process()`.
- **Riesgo medido**: en la zona de fundido de 20 ms las dos salidas no están alineadas en fase; en ventanas de 10 ms el nivel cae como máximo −2,8 dB (−5,1 dB en +12 st en un sondeo) respecto a Signalsmith. No cancela del todo, pero puede notarse.
- **Coste**: CPU ≈ suma de ambos motores (≈ 2,1–2,9 % vs 1,2–1,6 % del Signalsmith solo en el contenedor).
- **Ventaja real medida**: la entrada fuera de rango de PSOLA (< 65 Hz o > 700 Hz) cae a Signalsmith, que sí la transpone. Las consonantes y transitorios del híbrido son los de Signalsmith (no mejoran).
- Decisión: **no se conecta al VST3**. No hay evidencia de que mejore la voz real y duplicaría el coste. Si la escucha favorece a PSOLA en vocales, el siguiente paso sería alinear la fase en el fundido.

## 3. Espacial (investigación; sin cambios de código)
- Compatibilidad mono (6 s de vocal, suma L+R vs la voz original sin segunda voz): Urbano Espacial +0,3 dB, Dream Atmosphere +0,8 dB, Melodic Rap −0,3 dB, Segunda Voz +7 +0,6 dB. No hay cancelación de fase medible en la suma mono.
- Correlación L/R 0,92–1,00; relación lateral/central −13,7 a −21,8 dB.
- Movimiento estéreo suave y ducking de reverb: **no implementados**. Diseño propuesto: micro-retardo modulado por LFO muy lento sólo en el retorno de reverb (apagado por defecto, parámetro nuevo con ID nuevo, estado v6) y ducking con detector de la voz seca (máx. 6 dB, apagado por defecto, bypass bit-exacto). Requiere decisión de Diego sobre prioridad.
