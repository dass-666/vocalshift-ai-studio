# Probar VocalShift AI Studio en FL Studio (Windows 11 x64)

> Estado: el plugin pasa pluginval, pero **nadie lo ha probado todavía dentro de FL Studio**. Esta guía es para
> esa primera prueba. Anota cualquier fallo.

## 1. Descargar
1. En GitHub: repositorio `dass-666/vocalshift-ai-studio` → pestaña **Actions** → la última ejecución verde de
   *Build and validate* → sección **Artifacts** → `VocalShift-AI-Studio-VST3-Windows-x64`.
2. Descomprime el ZIP. Dentro hay una carpeta llamada `VocalShift AI Studio.vst3` (es una carpeta, no un archivo).

## 2. Instalar
Copia la carpeta completa `VocalShift AI Studio.vst3` a:
```
C:\Program Files\Common Files\VST3\
```
(Windows pedirá permiso de administrador.) Windows puede marcar el archivo como descargado de Internet; si FL no lo
carga, clic derecho sobre el ZIP → Propiedades → *Desbloquear* antes de descomprimir. El binario no está firmado.

## 3. Escanear en FL Studio
*Options → Manage plugins* → comprueba que `C:\Program Files\Common Files\VST3` esté en las rutas → **Find
installed plugins** (o *Find more plugins*). Busca "VocalShift AI Studio" en la lista y márcalo como favorito (estrella).

## 4. Insertar en el Mixer
1. Arrastra una voz (WAV) al Playlist o al Channel rack y envíala a una pista del **Mixer** (por ejemplo Insert 1).
2. En la pista del Mixer, en un slot vacío → *Select* → **VocalShift AI Studio**.
3. Reproduce. Una instancia nueva arranca en los **valores de referencia** (+7 st, Carácter 65 %, Magia 70 %,
   Conservar identidad). Para comparar con el original usa **Bypass** o el preset *Neutro*.

## 5. Pruebas rápidas (10 minutos)
1. **Pitch**: +3, +7, +12 y −7. La duración no debe cambiar.
2. **Carácter** de 0 % a 100 % en *Conservar identidad* y después en *Transformación radical*.
3. **Magia** 0 % vs 70 %: con el nivel automático el volumen debería parecerse; escucha el pulido y las "s".
4. Recorre los **presets** del selector.
5. **Bypass** varias veces durante la reproducción: no debe haber clics ni saltos de tiempo.
6. **Mezcla** al 50 % y **Ganancia**: observa los medidores IN/OUT y el indicador CLIP (clic para borrarlo).
7. **Automatización**: clic derecho sobre Pitch → *Create automation clip*. El modo también es automatizable.
8. Guarda el proyecto, ciérralo y ábrelo: los valores, el modo y el preset deben volver igual.
9. Cambia el tamaño de la ventana y alterna Fácil/Pro (en Pro se ven los valores efectivos de cada macro).
10. Exporta (*File → Export → WAV*) y compara con lo que oías en reproducción.

El protocolo detallado con puntuaciones está en `EXPERIMENT_01_VOZ_REAL.md`.

## 5b. Pruebas de Segunda voz (v0.3, 10 minutos)

1. Carga la pista de voz en el Mixer, inserta el plugin y elige el preset **"Segunda voz +7 (referencia)"** (o pulsa **Segunda voz**).
2. Canta: debes oír tu voz igual que sin plugin y, detrás, una segunda voz más alta y más baja de volumen. Con el transporte parado y reiniciado debe seguir alineada.
3. Pro: mueve Volumen 2.ª voz (−9 por defecto), Pan y Desfase artístico (0–100 ms). Cambiar el desfase no debe mover la latencia que FL compensa.
4. Cambia entre Transformar y Segunda voz mientras suena: no debe haber clics.
5. Abre un proyecto de la versión anterior: debe sonar como antes (el interruptor "DSP mejorado" queda apagado en esos proyectos). Compáralo con "DSP mejorado" encendido.
6. Cambia el tamaño de búfer y la frecuencia de muestreo del proyecto y repite 2.

**No se ha probado la v0.3 en FL Studio.** Hasta que lo hagas tú, solo está comprobada con pluginval y las pruebas automáticas.

## 6. Qué reportar
Versión de FL Studio, tamaño de buffer (*Options → Audio settings*), frecuencia de muestreo, CPU que muestra FL,
y cualquier: fallo al escanear, cuelgue, clic, desincronización con otras pistas, valores que no se restauran.

## Limitaciones conocidas
- Latencia de 150 ms: FL la compensa en reproducción, pero **no sirve para monitorear en directo** mientras grabas.
- Consonantes y ataques pueden sonar suavizados, sobre todo con transposiciones grandes.
- El nivel automático compensa la pérdida de volumen por transposición; si lo desactivas (Pro), compensa con *Ganancia*.
- Transformación radical es DSP (formantes + brillo). La IA neuronal no está implementada.
- El mapeo de Carácter y Magia es provisional, pendiente de tus escuchas.
