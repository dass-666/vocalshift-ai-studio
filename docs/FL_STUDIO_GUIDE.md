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
3. Reproduce. Por defecto el preset es *Neutro*: no debe cambiar nada (salvo 150 ms de retardo compensado).

## 5. Pruebas rápidas (10 minutos)
1. **Modo Fácil → Identity Pitch**: gira *Intensidad* a +3, +7, +12 y −7. La duración no debe cambiar.
2. **Naturalidad** a 100 % y luego a 0 % con +7: el 0 % debe sonar a "ardilla".
3. Recorre los 7 **presets** del selector.
4. **Voice Morph (DSP)**: mueve *Carácter* y *Brillo* sin cambio de nota.
5. **Bypass** varias veces durante la reproducción: no debe haber clics ni saltos de tiempo.
6. **Mezcla** al 50 % y **Ganancia**: observa los medidores IN/OUT y el indicador CLIP (clic para borrarlo).
7. **Automatización**: clic derecho sobre *Transposición* en modo Pro → *Create automation clip*.
8. Guarda el proyecto, ciérralo y ábrelo: los valores y el preset deben volver igual.
9. Cambia el tamaño de la ventana del plugin y alterna Fácil/Pro.
10. Exporta (*File → Export → WAV*) y compara con lo que oías en reproducción.

## 6. Qué reportar
Versión de FL Studio, tamaño de buffer (*Options → Audio settings*), frecuencia de muestreo, CPU que muestra FL,
y cualquier: fallo al escanear, cuelgue, clic, desincronización con otras pistas, valores que no se restauran.

## Limitaciones conocidas
- Latencia de 150 ms: FL la compensa en reproducción, pero **no sirve para monitorear en directo** mientras grabas.
- Consonantes y ataques pueden sonar suavizados, sobre todo con transposiciones grandes.
- Al transponer con formantes preservados la salida baja de volumen; compensa con *Ganancia*.
- Voice Morph es DSP (formantes + brillo). La IA neuronal no está implementada.
