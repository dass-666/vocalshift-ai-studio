# Transformación radical: auditoría y camino hacia conversión de voz neuronal

## Auditoría (hechos)

"Transformación radical" **es solo DSP**: el mismo motor de pitch/formantes que "Conservar identidad", con otras constantes
(`MacroMappingConstants`). Diferencias: cuántos formantes siguen al tono y cuánto se desplazan extra, y el brillo. No cambia
la identidad del hablante ni modela otra voz. Con el mapeo anterior desplazaba los formantes 8.45 st para +7 st de pitch
(más que el intervalo); el mapeo revisado lo limita a 0.7 × intervalo + 2 st × Carácter (4.49 st a +7). El texto de la interfaz lo describe como
"no es conversión de identidad por IA". **No se ha añadido ninguna etiqueta que prometa una transformación humana radical.**

## Camino modular hacia conversión neuronal (propuesta, nada implementado)

Interfaz ya existente: `IPitchShifter`. Propuesta de una segunda interfaz, `IVoiceConverter`, a nivel de bloque offline:

```
class IVoiceConverter { prepare(sr); latencySamples(); convert(in, out, target); }   // sin hilo de audio
```

1. **Offline primero:** render de una frase o toma completa en un hilo de trabajo, resultado cacheado, la ruta B de Segunda voz lee el resultado alineado (la ruta A no cambia).
2. **Modelos autorizados y locales:** solo pesos con licencia que permita uso comercial y redistribución, ejecutados con ONNX Runtime (MIT) en CPU. No se descarga ni se entrena nada en este bloque.
3. **Selector separado:** el modo neuronal sería una tercera opción de motor, no el modo "radical"; hasta que exista, la interfaz no ofrece nada que no funcione.
4. **Riesgos a decidir con Diego antes de empezar:** licencia del modelo, tamaño del instalador, latencia (no apto para directo), consentimiento y uso de voces de terceros.
5. **Criterio de entrada:** una escucha ciega que demuestre que el DSP no alcanza para "segunda voz con otra identidad".

## Evaluación de motores alternativos (documental; no se sustituyó el motor)

- **Hallazgos del diagnóstico** (`diagnosis: pitch engine measurements ...`, vocal sintética con vibrato 5 Hz ±30 ct): el error medio de F0 es < 4 cents a +3, +7, +12, +19, −5 y −12 st; el vibrato se conserva (97–101 %); la pérdida de nivel crece con el intervalo (−3.5 dB a +3 ... −6.1 dB a +19, con compensación de formantes). Los defectos reales encontrados en el camino de la señal fueron: (a) las fricativas se desplazan con el pitch (corregido con `ConsonantBlend`), (b) la pérdida de nivel (cubierta por el nivel robusto), (c) el mapeo radical excesivo. **No se midió ni se afirma** ausencia de artefactos metálicos/granulares: requieren voz real y escucha.
- **Con una sinusoide pura a 44.1 kHz** el nivel del tono desplazado varía hasta 17 dB según la frecuencia (sonda puntual; sin envolvente de formantes no hay qué compensar). No es un caso de voz, pero conviene no usar sinusoides como señal de prueba del motor.
- **PSOLA:** muy natural en voz monofónica con F0 estable y desplazamientos pequeños; frágil con F0 mal detectado, ruido, voz mixta y desplazamientos grandes; necesita detector de F0 y marcas de pitch robustos (YIN actual es offline). **Híbrido** (PSOLA en tramos sonoros + espectral en los sordos) es la opción más prometedora para ±12 st, pero cuesta semanas y sin escucha dirigida no hay motivo para empezar. **Recomendación:** no cambiar el motor ahora; si la escucha del Experimento 02 señala el timbre a +12/+19, prototipar PSOLA tras `IPitchShifter` y comparar A/B ciego.
