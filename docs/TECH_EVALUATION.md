# Evaluación tecnológica (resumen)

Leyenda: **Verificado** = comprobado en el archivo de licencia del código fijado o en la fuente oficial (05-11-2026).
**Pendiente** = basado en conocimiento previo, hay que confirmarlo antes de incorporarlo.

## Incorporado

### JUCE 9.0.4 — Verificado
- Doble licencia: AGPLv3 **o** JUCE 9 EULA (`LICENSE.md` del repositorio y juce.com/legal/juce-9-licence).
- EULA: Starter gratis hasta **20 000 USD** de ingresos/financiación en los últimos 12 meses; Indie hasta 300 000 USD
  (40 USD/mes o 800 USD perpetuo); Pro sin límite. Al superar el límite hay que subir de nivel.
- La EULA de JUCE 9 no exige pantalla "Made with JUCE". Incluye derecho de auditoría y uso de nombre/logo con opt-out.
- **Decisión pendiente del propietario:** distribuir cerrado bajo la EULA (Starter mientras < 20k USD) o abierto bajo
  AGPLv3 (obliga a publicar todo el código bajo AGPL). El repositorio es público y sin licencia propia por ahora;
  eso es compatible con ambas opciones, pero conviene decidir antes de vender o distribuir binarios.
- Desactivado en el build: navegador web, CURL, splash, analytics (`JUCE_REPORT_APP_USAGE=0`).

### VST 3 SDK 3.8 — Verificado
- Steinberg publicó VST 3.8 bajo **MIT** (29-10-2025). JUCE 9.0.4 lo incluye. Ya no hace falta el acuerdo de licencia
  dual GPLv3/propietario de versiones anteriores. El logo VST tiene sus propias guías de uso.

### Signalsmith Stretch 1.3.2 + Linear 0.6.6 — Verificado (MIT)
- Pitch multi-octava en streaming, formantes con compensación de tono y estimación automática de F0, sin dependencias.
- **Medido aquí** (ver QUALITY.md): F0 dentro de ±1 cent de ±3 a ±24 st con un tono armónico estable; con vocales
  sintéticas con vibrato, hasta 8 cents a −12 st y 22 cents a −24 st; 0 asignaciones en
  proceso, ~1.5–2.5 % de un núcleo en estéreo 48 kHz, latencia 150 ms.
- **Limitaciones conocidas:** método espectral (ventana 120 ms): suaviza transitorios y consonantes (pre-eco de
  ~10–30 ms medido en el ataque); la corrección de formantes es menos precisa que métodos monofónicos (lo dice su
  README); pérdida de nivel con transposiciones grandes y formantes preservados (hasta ~13 dB a +24 st en sintético).
- No damos su calidad por supuesta: está detrás de `IPitchShifter` y puede sustituirse.

## Alternativas DSP a evaluar en el siguiente bloque

| Opción | Interés para voz | Licencia | Estado |
|---|---|---|---|
| PSOLA/TD-PSOLA propio (con marcas de pitch de `VocalAnalysis`) | Muy bueno para voz monofónica, transitorios nítidos, baja latencia posible; falla en voz rasposa/susurro | Propia | Pendiente de prototipo |
| Híbrido: PSOLA en zonas sonoras + paso directo/espectral en consonantes | Mejor conservación de consonantes | Propia | Pendiente |
| Rubber Band Library v3 (R3) | Alta calidad, formantes | GPL o licencia comercial de pago | Pendiente: no compatible con distribución cerrada sin pagar |
| WORLD vocoder (análisis F0/envolvente/aperiodicidad) | Control fino de formantes, base para morph | BSD modificada (pendiente de confirmar) | Pendiente; latencia alta, más apto para modo Studio |
| Elastique / Zplane | Calidad comercial de referencia | Comercial | Solo como referencia de comparación |

## IA (solo investigación, nada incorporado)

### ONNX Runtime — Pendiente
- Licencia MIT (pendiente de confirmar en la versión elegida). En Windows: CPU, DirectML (Microsoft lo mantiene ya
  en modo de mantenimiento; verificar), Windows ML y CUDA/TensorRT para NVIDIA. Tamaño de DLL relevante para un plugin.
- Requisito de diseño: inferencia en hilo/proceso aparte (ver ARCHITECTURE.md). Medir peor caso por *hop* antes de
  prometer un modo Live.

### Arquitecturas tipo RVC — Investigación futura
Separar siempre cuatro cosas, cada una con su licencia o permiso:
1. **Código** (p. ej. el repositorio RVC WebUI declara MIT — pendiente de verificar versión y dependencias como
   fairseq/HuBERT, RMVPE, etc.).
2. **Pesos preentrenados** (HuBERT/ContentVec, vocoders): licencias propias, a veces no comerciales.
3. **Datos de entrenamiento**: derechos de las grabaciones.
4. **Derechos de la voz**: consentimiento documentado de la persona cuya voz se modela.

Ningún modelo se descarga ni se entrena en este bloque.
