# Fase 4: Urban Spatial Engine, fader de Segunda voz y nueva interfaz (v0.4.0)

**Estado:** implementado y probado con pruebas automáticas (Linux y CI). **No se ha escuchado ni probado en FL Studio.**
Ninguna afirmación de esta página es perceptual: los valores son de ingeniería y deben ajustarse con tu oído.

## Cadena de la Segunda voz

```
entrada ─┬─ retardo de latencia (0 dB, sin filtros) ───────────────────────────────────────────────────┐
         └─ pitch → formantes → consonantes → tono (brillo) → TEXTURA (Carácter) → de-esser → Magia
            (compresión + aire) → nivel automático → ESPACIO (anchura · reverb · eco) ── desfase ─ FADER ─ pan ─┴─ suma → ganancia → soft clip
```

- El espacio va **después** del nivel automático (la cola de reverb no mueve el auto nivel) y **antes** del fader: el fader controla voz + espacio.
- La voz original (ruta A) no pasa por ninguna de estas etapas. Nada se aplica a la mezcla completa.
- Sin latencia añadida: 7200 muestras (150 ms a 48 kHz), igual que la Fase 3 (probado). Solo estéreo; con un canal el espacio no actúa.

## Fader de Segunda voz (parámetro `secondGain`, mismo ID que la Fase 3)

−60…+6 dB; en el extremo inferior (≤ −59.9 dB) la segunda voz queda **muda**. Escala en dB (lineal en dB = curva perceptual estándar).
Suavizado de 30 ms (sin clics, probado con saltos −12 → −36 → 0 → silencio). Por encima de 0 dB el valor se tiñe de ámbar (la suma puede acercarse
al soft clip; la última red sigue siendo el limitador). Doble clic = valor del preset actual (−12 dB en Urbano Espacial). Automatizable desde FL Studio.
Medido: −12 dB → 0 dB = +12.00 dB, +6 dB = +18.00 dB, silencio = −120 dB.

## Urban Spatial Engine (`src/core/space/SpatialEngine`)

| Bloque | Diseño | Valores iniciales |
|---|---|---|
| Anchura | M/S: `s = ½(D1(m) − D2(m))`, `L += g·s`, `R −= g·s`; paso alto 120 Hz y paso bajo 6.5 kHz sobre `s` | micro-retardos 11 y 17 ms, modulación ±0.2–0.7 ms a 0.27 / 0.19 Hz en contrafase |
| Reverb | FDN de 4 líneas, matriz de Hadamard ortonormal, amortiguación por línea; envío filtrado 250 Hz–5 kHz | predelay 28 ms fijo, RT60 0.8–1.8 s |
| Eco corto (slap) | un eco filtrado (300 Hz–3.2 kHz), realimentación 0.18, tiempos L/R con 13 % de diferencia | 1/16 de nota del tempo del DAW, plegado a 70–180 ms; 115 ms si no hay tempo |
| Textura (Carácter) | shelf 220 Hz, campana 3 kHz, saturación paralela solo bajo 1.8 kHz | ver abajo |

Propiedades medidas:
- **Mono seguro por construcción:** `(L+R)` no cambia con la anchura (error máx. 2.5e−8); con todo el espacio a Magia 70 % la suma mono varía −0.9 dB. Correlación L/R: 1.00 (Magia 0 %) → 0.61 (70 %) → 0.22 (100 %).
- **Reverb estable:** RT60 medido (curva de decaimiento de Schroeder) 0.67 s para el ajuste 0.8 s y 1.48 s para 1.8 s (la amortiguación acorta ~20 %); 60 s de ruido a los máximos: pico 0.84, finito.
- **Eco estable:** primer eco a 115.0 / 166.7 / 107.1 ms (sin tempo / 90 / 140 BPM), residual a 2 s < 1e−10.
- **Magia 0 % = cero espacio:** salida idéntica bit a bit a `spatial` apagado (probado). El pitch sigue (+697 cents medidos).
- Memoria del motor: 120 kB; CPU 4.3 % de un núcleo frente a 3.8 % de la Fase 3 (+0.6 puntos; estéreo, 48 kHz, 512 muestras, Linux en la nube). Cero asignaciones en `process()`.
- Limitaciones: la modulación y el predelay no son configurables (el predelay no se mueve con Magia para evitar chasquidos). No hay "filtrado dinámico" ni síntesis granular (no se implementó).

## Magia en Segunda voz (curvas, `m = Magia/100`)

| | fórmula | 30 % | 70 % | 100 % |
|---|---|---|---|---|
| Anchura | 0.95 · m^1.4 | 0.18 | 0.58 | 0.95 |
| Reverb (retorno lineal) | 0.55 · m^1.5 | 0.09 (−21 dB) | 0.32 (−10 dB) | 0.55 (−5 dB) |
| RT60 | 0.8 + 1.0 m s | 1.1 | 1.5 | 1.8 |
| Eco | 0.28 · ((m−0.4)/0.6)^1.2, solo > 40 % | 0 | 0.12 (−18 dB) | 0.28 (−11 dB) |
| Modulación | 0.2 + 0.5 m ms | 0.35 | 0.55 | 0.70 |

Además se mantienen la compresión suave y el aire de la Fase 3. Los trims Pro (Anchura, Reverb, Eco corto, 0–100 %) escalan lo que pide Magia; el interruptor
"Procesamiento espacial" apaga espacio y textura. **En Transformar, Magia conserva su comportamiento anterior** (sin espacio). Estados y presets anteriores
a la v0.4 se cargan con el espacio apagado y suenan como en la Fase 3 (probado), incluidos los presets `second_voice_7` y `second_voice_12`.

## Carácter en Segunda voz (`c = Carácter/100`; se suma al mapeo de formantes/brillo de la Fase 3)

| Etapa | fórmula | 65 % medido |
|---|---|---|
| Cuerpo | shelf 220 Hz, +3.0 dB · c | +1.93 dB a 110 Hz (diseño +1.9) |
| Presencia | campana 3 kHz Q 0.8, +2.2 dB · c | +0.82 dB a 3 kHz (diseño +1.4: la saturación resta ~0.6 dB) |
| Calidez | saturación tanh paralela bajo 1.8 kHz, k = 2 + 4c, mezcla 0.7c, ganancia unitaria a −20 dBFS | — |
| Agudos | no se tocan | +0.21 dB a 9 kHz; ruido sibilante +0.12 dB |

Así Carácter no equivale a subir agudos. Carácter 0 % = bypass exacto. No se implementó "filtro dinámico".

## Naturalidad del pitch (honesto)

No hay un motor nuevo. El diagnóstico de la Fase 3 (error de F0 < 4 cents, vibrato conservado 97–101 % con vocal sintética) no explica por sí solo que la 2.ª voz suene "pitch simple":
con una sola copia exacta, bien afinada y sin movimiento, sí suena a transposición. Lo que esta fase ataca es esa *percepción de capa*: textura, micro-movimiento estéreo, profundidad
y nivel, **no el algoritmo**. Si tras escuchar el Experimento 03 el timbre a +7 sigue siendo el problema, el siguiente paso (con evidencia) es un motor PSOLA/híbrido tras `IPitchShifter`
(ver `RADICAL_Y_VOZ_NEURONAL.md`). No se implementó por no tener esa evidencia; no se prometió naturalidad perfecta.

## Interfaz

Implementada (no maqueta): `VsLookAndFeel` con perillas de anillo de progreso, fader vertical con escala dB, interruptores planos, paleta grafito con azul/violeta/turquesa.
- **Fácil:** tres macros (Pitch azul, Carácter violeta, Magia turquesa) + fader de Segunda voz con medidor propio (visible solo en Segunda voz); selector Transformar/Segunda voz y Identidad/Radical arriba.
- **Pro:** paneles Pitch, Timbre, Dinámica, Espacio, Mezcla, Calidad y diagnóstico; el fader sigue accesible.
- Valores numéricos siempre visibles, doble clic restaura (perillas: valor por defecto; fader: valor del preset), rueda del ratón, edición de texto.
- Medidores IN/OUT reales con indicador de clip; nada se anima sin audio. Redimensionable 800×640 a 1700×1100. Capturas: `docs/img/ui_easy.png`, `docs/img/ui_pro.png` (render sin escalado de Windows).
- Pendiente: no hay sección "Modulación" en Pro porque la modulación no es un control (es interna). Un visualizador de espectro/forma de onda no se añadió.
