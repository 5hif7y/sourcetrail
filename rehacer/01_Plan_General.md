# Reestructuración de Sourcetrail: Plan General

Este documento describe el plan maestro para reestructurar la base de código actual de Sourcetrail (C++ y Qt), transformándola desde una arquitectura monolítica hacia un sistema híbrido, ligero y modular basado en plugins de lenguajes. Este plan está diseñado de forma "limpia" (prolija), ignorando los problemas de prueba y error.

## El Problema del Monolito Actual
En el código base original, todo está fuertemente acoplado:
1. **Core y GUI entrelazados:** El núcleo (`src/lib`) utiliza tipos de Qt (`QString`, `QJsonObject`, `Q_DECLARE_METATYPE`), lo que impide compilar la lógica de Sourcetrail sin arrastrar todo el entorno gráfico.
2. **Indexadores estáticos:** Los parsers de C++ (que incluyen Clang/LLVM, con tiempos de compilación masivos) y Java están acoplados estáticamente al núcleo. Si falla LLVM, falla toda la compilación.
3. **Falta de Extensibilidad:** Añadir un nuevo lenguaje requiere modificar el código central del programa.

## La Solución Objetivo
El objetivo es transformar la arquitectura en 4 capas limpias:
1. **`Sourcetrail_lib` (Core Estático):** Un núcleo puro en C++ estándar y Boost. Libre de Qt.
2. **`Sourcetrail_lib_gui` y `Sourcetrail.exe`:** La capa visual, que enlaza estáticamente con el Core.
3. **`sourcetrail_api.dll`:** Una barrera arquitectónica. Una API en C puro que expone la funcionalidad del Core hacia el exterior.
4. **Plugins Dinámicos (`.dll` / `.so`):** Librerías independientes (ej. `sourcetrail_cxx_plugin.dll`) que se comunican exclusivamente a través de la API en C para registrar capacidades de parseo de lenguajes en tiempo de ejecución.

## Mapa de Documentos
- `02_Fase1_Limpieza_y_Preparacion.md`: Actualización de CMake y consolidación del núcleo.
- `03_Fase2_Desacoplamiento_GUI.md`: Eliminación sistemática de Qt del núcleo (`src/lib`).
- `04_Fase3_API_C_y_Plugins.md`: Implementación de `sourcetrail_api` y el `PluginManager`.
- `05_Fase4_Lenguajes_como_Plugins.md`: Extracción de C++ y Java en librerías dinámicas cargables en runtime.
