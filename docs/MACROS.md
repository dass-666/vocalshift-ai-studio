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

---

# Fase 3: mapeo revisado (`improved`) y Segunda voz

Desde la v0.3 existe el parámetro Pro **DSP mejorado** (`improved`). **Apagado** = comportamiento exacto de
las versiones anteriores (tabla de arriba; probado bit a bit con una referencia de dec340d en
`regression: route=transform, improved=false ...`). **Encendido** (valor de una instancia nueva) = lo que sigue.
Los proyectos y presets guardados antes de la v0.3 se cargan con `improved` apagado y suenan como antes.

## Por qué se revisó el mapeo (hecho medido, no percepción)

Con +7 st y Carácter 65 %, el modo radical anterior desplazaba los formantes **8.45 st en total** (65 % × 7 = 4.55 st que
siguen al tono + 3.9 st extra), es decir **más que el propio intervalo**. Eso empuja la envolvente espectral más lejos
que el tono: es la causa técnica *probable* de que el modo suene "demasiado artificial" (inferencia; falta una escucha
dirigida que la confirme). En identidad eran 1.8 st (1.1 + 0.65), un cambio casi constante que no crecía con el intervalo.

## Fórmulas del mapeo revisado

Con `c` = Carácter/100, `s` = semitonos totales (Pitch + ajuste fino), `d` = dirección (+1 / −1, rampa continua entre
−1 y 0 st):

```
formantes (st) = c · ( ratio · s  +  base · d )          se aplican con compensación total (formantPreservation = 1)
brillo (dB)    = d · brillo · c
```

| | ratio | base (st) | brillo (dB) |
|---|---|---|---|
| Conservar identidad | 0.25 | 0.4 | 1.0 |
| Transformación radical | 0.7 | 2.0 | 2.5 |

`ratio < 1` garantiza que los formantes **nunca se mueven más que el tono**. `base` mantiene audible Carácter incluso a 0 st.
Todo se recorta a ±12 st / ±12 dB y los ajustes Pro se suman después.

Valores efectivos a +7 st, Carácter 65 %:

| | Anterior | Revisado |
|---|---|---|
| Identidad: movimiento total de formantes | 1.8 st | **1.40 st** |
| Identidad: brillo | +0.98 dB | **+0.65 dB** |
| Radical: movimiento total de formantes | **8.45 st** | **4.49 st** |
| Radical: brillo | +3.9 dB | **+1.63 dB** |

La proporción 0.25 del intervalo es una **inferencia** (relación antropométrica aproximada entre longitud del tracto y
tono en voces reales). **No está validada con escuchas.** Las constantes son provisionales y viven en
`MacroMapping.h` (`MacroMappingConstants`). No se usan curvas no lineales: no hay evidencia que las justifique todavía.

## Magia con DSP mejorado

Misma compresión y aire de antes, más un **de-esser real** por banda (`DeEsser`): divide en 5 kHz (la suma de las dos
bandas reproduce la señal), detecta la banda alta y la atenúa 4:1 sobre −34 dBFS, hasta 9 dB, escalado por Magia.
Magia 0 % = bypass exacto (probado). Se aplica **antes** del aire para que el aire no amplifique las sibilantes.
Medido en ruido sibilante sintético: −5.7 dB; en una vocal sintética: 0.00 dB. Umbrales provisionales.
Magia 0 % sigue dejando la voz procesada "sin terminar" sin desactivar el pitch (el pitch es otra etapa).

## Consonantes (`ConsonantBlend`, solo con DSP mejorado)

El motor espectral multiplica por el factor de pitch **toda** la frecuencia, también el ruido de "s", "sh", "f". Medido:
un ruido fricativo con centroide 14.6 kHz sale a 16.8 kHz con +7 st (viejo) y a 14.6 kHz con el nuevo camino.
Por encima de 4.5 kHz y **solo mientras el original es sibilante** (≥ −6 dB de su potencia sobre 4.5 kHz; 0 por debajo de −12 dB)
la señal procesada se sustituye por el original alineado. Límite: las oclusivas (p, t, k) son de banda ancha y corta
duración y solo se tratan en parte. **Que suene mejor con voz real no está comprobado.**

## Nivel automático robusto (solo con DSP mejorado)

Mide solo mientras hay voz (histéresis −50 / −56 dBFS, retención 200 ms), congela la ganancia en pausas y ante huecos
de más de 15 dB respecto al nivel corriente (consonantes, silencios entre palabras); si el hueco dura más de 150 ms se
acepta como pasaje más suave. Límites −6 / +12 dB (antes +18). Solo se nivela la rama procesada; el original nunca se toca,
de modo que hay un único control automático de nivel en toda la cadena.

## Segunda voz

Selector **Transformar / Segunda voz**, independiente del modo Identidad / Radical (cuatro combinaciones, un solo motor).

```
entrada ─┬─ retardo de latencia (solo retardo, 0 dB, sin filtros) ───────────────┐
         └─ motor de pitch → consonantes → timbre → de-esser → Magia → nivel ─ (desfase) ─ ganancia ─ pan ─┴─ suma → ganancia de salida → soft clip
```

- Ruta A (principal): sin transposición, 0 dB, solo retardada por la latencia del motor (7200 muestras a 48 kHz).
- Ruta B: Pitch, Carácter y Magia (los tres macros), −9 dB respecto a la A (**provisional**), centro, desfase 0 ms.
- Tres retardos distintos: **latencia interna entre rutas** (la compensa la ruta A para que ambas lleguen alineadas: medido 0 muestras de error),
  **latencia reportada al host** (150 ms, constante e independiente de ruta, desfase o ajustes: probado) y **desfase artístico**
  (solo ruta B, 0–100 ms, no cambia la latencia reportada; medido 480 muestras para 10 ms, 1920 para 40 ms).
- El cambio de ruta es un fundido cruzado de ganancias de 30 ms: la alineación no se mueve al cambiar.
- Modo Fácil: tres macros + selector. Modo Pro: volumen de la segunda voz (−30..0 dB), pan (balance), desfase artístico, mezcla
  (solo en Transformar), ganancia de salida, DSP mejorado. La humanización queda fuera de este bloque (no se ofrece ningún control).
- Protección: la suma original (0 dB) + segunda voz (−9 dB) puede subir hasta ~+2.5 dB; el soft clip es la última red.
  Con el original a −6 dBFS no se alteró ninguna muestra (contador `clipAlteredSamples`, probado a +7, +12 y +19 st), así que el limitador no oculta errores de ganancia.

---

# Fase 4: Magia y Carácter en Segunda voz

Magia pasa a controlar el espacio (anchura, reverb, eco corto, modulación) y Carácter añade cuerpo, presencia y calidez cuando `Ruta = Segunda voz`,
`DSP mejorado` y `Procesamiento espacial` están activos. Fórmulas, valores efectivos y límites: `FASE4_URBAN_SPATIAL.md`.
Con el espacio apagado (o en Transformar) todo se comporta como en la Fase 3.
