# Roadmap

Ordenado por valor esperado / riesgo. Nada de esto está implementado; la UI lo muestra solo como texto.

## Bloque siguiente recomendado (V1.1: calidad antes que funciones)
1. **Escuchas con voces reales** (protocolo en QUALITY.md) y prueba en FL Studio. Sin esto, cualquier optimización es a ciegas.
2. ~~Compensación automática de nivel~~ — hecho (LevelMatcher). Pendiente: calibrar Carácter/Magia con el Experimento 01.
3. **Prototipo PSOLA** detrás de `IPitchShifter` + `VocalAnalysis` en tiempo real (YIN incremental) y comparación
   A/B frente a Signalsmith con el mismo banco de pruebas. Decidir motor por escucha, no por suposición.
4. **Preservación de consonantes**: detección de segmentos no sonoros/transitorios y mezcla con la señal sin procesar.
5. CI: firma de código (requiere certificado de pago) y pruebas en más hosts.

## Después
- **HarmonyEngine**: 1–4 voces con intervalos diatónicos (tonalidad/escala), humanización de tiempo y afinación, paneo.
- **Doblaje**: 2–4 copias con micro-desafinación y desfases variables.
- **Edición por frases**: requiere decidir modelo de host (ARA2 en FL Studio — verificar soporte — o captura interna).
- **Modo Live de baja latencia**: solo con un motor que lo permita (PSOLA) y mediciones que lo respalden.
- **AI Voice Morph neuronal**: primero modo Studio (render offline en trabajador), con modelos de licencia clara y
  consentimiento de voz documentado. Ver TECH_EVALUATION.md.
- **Mezcla automática** (EQ/compresión/de-esser vocal).
