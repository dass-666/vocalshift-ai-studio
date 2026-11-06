# Experimento 02: segunda voz y DSP mejorado (escucha)

**Estado:** los archivos se generan automáticamente; **no hay ningún resultado perceptivo todavía**. Los resultados los pones tú.

## Preparar los archivos (misma toma, mismo volumen)

```
vs_bench --out exp02 --input mi_toma.wav
```

Sin `--input` usa vocales sintéticas (solo sirven para comprobar el flujo; **no** valen para juzgar naturalidad).
Cada archivo sale alineado en latencia con el original e igualado en RMS (`RESULTS.md`, columna "Ajuste RMS"). Así
compruebas A/B sin que el volumen engañe. Los de segunda voz contienen dos voces; el RMS se iguala al conjunto.

| Letra | Archivo | Qué es |
|---|---|---|
| A | `*__00_original.wav` | tu toma |
| B | `exp02_B_transformar_+7_actual` | Transformar +7, DSP anterior (referencia de la prueba 01) |
| C | `exp02_C_transformar_+7_mejorado` | Transformar +7 con consonantes, nivel robusto, mapeo y de-esser revisados |
| D | `exp02_D_segunda_voz_+7_-9dB` | original + segunda voz +7 st, −9 dB |
| E | `exp02_E_segunda_voz_+12_-9dB` | original + segunda voz una octava arriba |
| F | `exp02_F_segunda_voz_+7_caracter0` | D con Carácter 0 % |
| G | `exp02_G_segunda_voz_+7_caracter65` | D con Carácter 65 % |
| H | `exp02_H_segunda_voz_+7_magia0` | D con Magia 0 % |
| I | `exp02_I_segunda_voz_+7_magia70` | D con Magia 70 % |

Extra: `exp02_intervalo_{+3,+7,+12,+19,-5,-12}st_mejorado` frente a `..._actual` para juzgar dónde se pierde naturalidad.

## Guía de puntuación (1 = muy malo, 5 = muy bueno; escucha con auriculares, 2–3 veces cada una)

| Letra | Naturalidad | Calidad de la 2.ª voz | Integración con el original | Claridad de palabras | Timbre | Resultado musical | Notas |
|---|---|---|---|---|---|---|---|
| A | | — | — | | | | |
| B | | — | — | | | | |
| C | | — | — | | | | |
| D | | | | | | | |
| E | | | | | | | |
| F | | | | | | | |
| G | | | | | | | |
| H | | | | | | | |
| I | | | | | | | |

Preguntas que cada comparación contesta (y la decisión que habilita):

1. **B vs C:** ¿mejoran consonantes ("s", "t") y naturalidad? → decide si `improved` pasa a ser el valor por defecto definitivo.
2. **D vs E:** ¿la octava funciona mejor que +7 como armonía? → decide el preset por defecto de segunda voz.
3. **F vs G:** ¿Carácter ayuda a que la segunda voz suene a otra persona, o solo a "voz tratada"? → decide el valor de Carácter por defecto en segunda voz.
4. **H vs I:** ¿Magia aporta o ensucia/aplasta la segunda voz? → idem para Magia.
5. **D vs A:** ¿−9 dB es el volumen justo (se oye, pero no compite)? ¿Notas el desfase? Prueba Pro: desfase 0 / 10 / 25 ms y pan ±30 %.
6. Cualquier "s" silbante, "t" empastada, sonido metálico o granular: anota la letra y el segundo.

Si tu toma no tiene "s" claras no podrás responder la pregunta 1: graba una frase con "así sé sentir seis sombras".

## Qué no se puede concluir

Ninguna métrica automática mide naturalidad. Los números de `RESULTS.md` solo comprueban afinación (±1 cent en vocales
sintéticas), nivel, latencia y CPU.
