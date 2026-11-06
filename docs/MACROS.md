# Macroperillas: qué hace cada una de verdad

La interfaz Fácil se reduce a un selector de modo y tres perillas. Las tres están conectadas a DSP real; las pruebas
`macros: ...` del núcleo y del plugin comprueban que cada una cambia la señal. **El mapeo es provisional**: se eligió
por criterio de ingeniería y hay que calibrarlo con escuchas de voz real (ver `EXPERIMENT_01_VOZ_REAL.md`). Todas las
constantes están en un solo sitio: `src/core/state/MacroMapping.h`.

Valores de referencia (estado inicial de una instancia nueva y preset "Referencia"): **Pitch +7 st, Carácter 65 %,
Magia 70 %, modo Conservar identidad, nivel automático activado.**

## Separación de responsabilidades

```
Pitch    ──> tono (semitonos + ajuste fino) ─────────────────────────> motor de pitch (Signalsmith)
Carácter ─┬> formantes: % que siguen al tono + desplazamiento extra ──> motor de pitch (formantes)
          └> timbre: brillo (shelf 2.5 kHz) ────────────────────────> ToneShaper
Magia    ──> mejoras: compresión + aire (shelf 10 kHz) ──────────────> VocalEnhancer
(siempre)──> nivel automático ───────────────────────────────────────> LevelMatcher
Modo     ──> cambia los máximos que alcanza Carácter
```

## Tabla de mapeo (c = Carácter/100, m = Magia/100, d = dirección del tono)

`d` = +1 si el tono sube o no cambia, −1 si baja (con una rampa continua entre −1 y 0 st para que automatizar el tono
no produzca saltos de timbre).

| Resultado | Conservar identidad | Transformación radical |
|---|---|---|
| % del cambio de tono que siguen los formantes | 25 % × c | 100 % × c |
| Desplazamiento extra de formantes | d × 1 st × c | d × 6 st × c |
| Brillo (shelf 2.5 kHz) | d × 1.5 dB × c | d × 6 dB × c |
| Compresión (Magia) | ratio 1 + 3m, umbral −12 − 12m dBFS, rodilla 6 dB | igual |
| Aire (Magia) | +5 dB × m a 10 kHz | igual |

Con los valores de referencia (+7, 65 %, 70 %):

- **Conservar identidad:** los formantes siguen el 16 % del tono (+1.1 st) más 0.65 st extra; brillo +1 dB; 3.1:1; aire +3.5 dB.
- **Transformación radical:** los formantes siguen el 65 % (+4.6 st) más 3.9 st extra; brillo +3.9 dB.

Los ajustes del modo Pro ("Ajuste formantes" y "Ajuste brillo") se **suman** al resultado de las macros. Ya no son
controles independientes que compitan con ellas.

## Qué no es

- *Carácter* en modo radical cambia el tamaño aparente del tracto vocal y el brillo. **No convierte la voz en otra
  persona**; eso requiere el motor neuronal, que no está implementado.
- *Magia* es una compresión y un realce sencillos y medibles. **No es una cadena vocal profesional** ni hace
  de-essing, EQ adaptativa ni reducción de ruido.
- El nivel automático iguala el **RMS** con constantes de tiempo lentas (~0.4 s). No es una medida de sonoridad
  LUFS: puede dejar diferencias percibidas de 1–2 dB.

## Mediciones (Linux, vocales sintéticas)

| Medida | Resultado |
|---|---|
| F0 con los valores de referencia, en ambos modos | error < 2 cents |
| Centroide espectral (entrada 890 Hz) | identidad 1042 Hz, radical 1568 Hz |
| Rango dinámico de una señal de 20 dB alto/bajo | Magia 70 %: 15.2 dB; Magia 100 %: 14.4 dB |
| Nivel a +12 st con formantes preservados | sin nivel automático −2.6 dB; con él −0.03 dB |
| Silencio después de la voz | 0 exacto (el nivel automático se congela por debajo de −50 dBFS) |
| CPU de la cadena completa con los valores de referencia | 2.4 % de un núcleo (estéreo, 48 kHz) |

El "aire" casi no mueve el centroide de las vocales sintéticas (+3 Hz), porque tienen poca energía por encima de
8 kHz. Su efecto real hay que juzgarlo con voz grabada.
