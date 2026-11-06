# Experimento 03: Urbano Espacial (escucha)

**Estado:** los archivos se generan solos; **no hay resultados perceptivos** (los pones tú).

```
vs_bench --out exp03 --input mi_frase.wav
```
Todos los archivos salen alineados en latencia e igualados en RMS con el original (columna "Ajuste RMS" de `RESULTS.md`). Tu frase debe incluir "s" y "t" claras y notas largas.

| Letra | Archivo | Qué es |
|---|---|---|
| A | `exp03_A_fase3_segunda_voz_+7` | Segunda voz de la Fase 3 (+7 st, −9 dB, sin espacio) |
| B | `exp03_B_urbano_+7_c65_m70_-12dB` | Urbano Espacial |
| C | `exp03_C_magia0` | B con Magia 0 % |
| D | `exp03_D_magia100` | B con Magia 100 % |
| E | `exp03_E_caracter0` | B con Carácter 0 % |
| F | `exp03_F_caracter100` | B con Carácter 100 % |
| G | `exp03_G_espacial_desactivado` | B con el procesamiento espacial desactivado |
| H | `exp03_H_anchura_y_reverb_sin_eco` | B con anchura y reverb, sin eco corto |

No hay motor alternativo de pitch, así que no hay muestra extra. Nota: A usa −9 dB (valor de la Fase 3) y B −12 dB (valor nominal del preset): comparan el preset tal como se entregaba y como es ahora.
El RMS igualado es el de la mezcla completa: con una capa a −12 dB el ajuste es de ~0.3 dB, es decir, la comparación a volumen igual casi no mueve el nivel.

## Puntuación (1 = muy malo, 5 = muy bueno)

| | Naturalidad | Profundidad | Amplitud | Claridad | Integración con la voz principal | Calidad del timbre | Movimiento musical | Artefactos (5 = ninguno) | Preferencia |
|---|---|---|---|---|---|---|---|---|---|
| A | | | | | | | | | |
| B | | | | | | | | | |
| C | | | | | | | | | |
| D | | | | | | | | | |
| E | | | | | | | | | |
| F | | | | | | | | | |
| G | | | | | | | | | |
| H | | | | | | | | | |

Qué decide cada comparación: **A vs B** (¿Fase 4 mejora la capa?), **C/B/D** (curva de Magia: ¿el 70 % es "claramente producido" y la voz principal sigue dominando? ¿el 100 % es demasiado?),
**E/B/F** (curva de Carácter), **G vs B** (¿cuánto aporta el espacio?), **H vs B** (¿el eco corto ayuda o estorba?). Anota "s" silbantes, ecos que molestan en palabras, sonido "chorus" o metálico, y la pérdida de claridad de la voz principal.
Prueba además en el DAW: fader a −18 / −12 / −6 dB con la canción real.
