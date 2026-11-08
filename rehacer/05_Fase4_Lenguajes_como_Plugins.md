# Fase 4: Extracción de Lenguajes como Plugins Modulares

Con la API base y el Core estático preparados, el último paso del refactor arquitectónico es extirpar físicamente la funcionalidad de indexación para lenguajes pesados como C++ y Java.

## 1. Separación de Directorios y Librerías Dinámicas
- **Acción:** Convertir las librerías `src/lib_cxx` y `src/lib_java` de su estado original a librerías dinámicas explícitas (`SHARED`).
- **Construcción:** En `src/lib_cxx/CMakeLists.txt`, definir `add_library(sourcetrail_cxx_plugin SHARED ...)`. Asegurarse de que enlacen de manera *externa* contra `sourcetrail_api` para acceder al núcleo de manera segura.

## 2. Creación del Punto de Entrada (PluginInit)
- **Acción:** Para cada lenguaje, crear un archivo `PluginInit.cpp`.
- **Implementación:** 
  ```cpp
  #include "sourcetrail_api.h"
  #include "LanguagePackageCxx.h"

  extern "C" __declspec(dllexport) void sourcetrail_plugin_init() {
      // Registrar el soporte a través del C-API
      sourcetrail_api_add_language_package(crear_proxy_cxx());
  }
  ```
- Este paso aisla por completo la dependencia en Clang/LLVM. Únicamente la librería dinámica `sourcetrail_cxx_plugin.dll` sufrirá el impacto masivo de enlazado de Clang, dejando la GUI y el CLI ultra veloces.

## 3. CMake Automático para Plugins
Para mantener la usabilidad (evitando que el usuario deba copiar DLLs a mano), configurar scripts automáticos en CMake.
- **Acción:** En `src/app/CMakeLists.txt`, agregar un comando `add_custom_command(POST_BUILD)` que detecte y copie automáticamente todas las librerías construidas de la forma `*_plugin.dll` (así como sus dependencias transitivas como las `platforms` de Qt) hacia el directorio `build/Debug/app/plugins/` junto al ejecutable.

## Conclusión de Reestructuración
Tras seguir limpiamente estas cuatro fases, habremos convertido el repositorio original de Sourcetrail en un entorno robusto y flexible. El software arranca al instante y carga el soporte para C++ de manera agnóstica sólo si el módulo LLVM está disponible, permitiendo su uso fluido en entornos limitados o su distribución por piezas.
