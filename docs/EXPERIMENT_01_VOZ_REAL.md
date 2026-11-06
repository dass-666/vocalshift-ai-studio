# Experimento 01: voz humana real en FL Studio

**Objetivo:** decidir con oído, no con métricas, si los valores de referencia (+7 st, Carácter 65 %, Magia 70 %)
y los dos modos suenan como se espera, y obtener datos para recalibrar `MacroMapping.h`.
Duración estimada: 45–60 minutos.

## 1. Material (10 min)
Graba **en seco** (sin reverb ni efectos), en mono, 24 bit, a 44.1 o 48 kHz, con picos en torno a −6 dBFS:

1. Frase hablada con muchas consonantes: *"Si te sientas tras la puerta, cuatro gatos saltan sin parar."*
2. Frase hablada tranquila de unos 5 segundos.
3. Una nota cantada sostenida de 4 segundos, en un registro cómodo.
4. Una frase cantada corta, con algo de melodía.

Si usas voces de otras personas, pide su permiso.

## 2. Configuración en FL Studio (5 min)
1. Instala el VST3 del último CI en verde (ver `FL_STUDIO_GUIDE.md`).
2. Pon cada grabación en una pista del Playlist, enrutada a **su propio Insert** del Mixer.
3. En cada Insert: **VocalShift AI Studio**, en modo **Fácil**.
4. Escucha con auriculares y deja fijo el volumen del monitor durante toda la prueba.

## 3. Comparaciones (A/B: alterna con Bypass o duplica el canal)

| # | Ajuste | Qué escuchar |
|---|---|---|
| A | Referencia (preset por defecto) | ¿Suena a la misma persona, más aguda? |
| B | A + modo **Transformación radical** | ¿Es claramente otra "talla" de voz? ¿Útil o ridícula? |
| C | A con **Magia 0 %** | ¿Magia mejora o empeora? (pulido vs. aplastado o sibilante) |
| D | A con **Carácter 0 %** | ¿Carácter 65 % suena más natural o menos que 0 %? |
| E | Pitch +3 / +12 / −7 con Carácter 0 % | ¿Hasta dónde es aceptable el motor actual? |
| F | Mueve Pitch, Carácter y Magia con el audio sonando | ¿Hay clics, saltos o zumbidos? |
| G | Guarda el proyecto, ciérralo y ábrelo | ¿Vuelven todos los valores y el modo? |

Puedes generar A–E de forma reproducible fuera de FL Studio:

```
vs_bench --input frase1.wav --out exp01_frase1
```

Esto crea `exp01_A_ref_identidad`, `exp01_B_ref_radical`, `exp01_C_..._magia0` y `exp01_D_..._caracter0`,
alineados e igualados en volumen.

## 4. Hoja de puntuación (1 = malo, 5 = excelente)

| Caso | Material | Naturalidad | Misma persona (solo A, C, D) | Consonantes | Artefactos (5 = ninguno) | Notas |
|---|---|---|---|---|---|---|
| A | 1 | | | | | |
| … | | | | | | |

Anota también:
- Buffer y frecuencia de muestreo de FL Studio, y CPU del plugin.
- Si algún preset no describe lo que se oye.

## 5. Qué decidiremos con los resultados
- Si A puntúa < 3 en "misma persona" → bajar `identityMaxFollow` / `identityMaxExtraFormant`.
- Si B no es suficientemente distinto → subir los máximos del modo radical.
- Si C (sin Magia) suena mejor → reducir compresión/aire o cambiar la curva de Magia.
- Si las consonantes puntúan < 3 en general → priorizar el prototipo PSOLA / preservación de transitorios
  (siguiente bloque técnico).

Devuélveme la hoja (o tus notas) y, si puedes, los WAV originales para incorporarlos a QualityBench.
