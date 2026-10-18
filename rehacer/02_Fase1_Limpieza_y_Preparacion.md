# Fase 1: Limpieza de Dependencias y Consolidación del Build

Antes de escribir código nuevo, debemos preparar el sistema de construcción (CMake) y limpiar dependencias innecesarias para asegurar compilaciones predecibles y rápidas.

## 1. Actualización del Gestor de Paquetes (`vcpkg.json`)
El archivo `vcpkg.json` debe reflejar la realidad del sistema base sin los plugins pesados.
- **Acción:** Comentar u ocultar temporalmente las dependencias de `llvm`, `clang`, y Java.
- **Motivo:** Esto permite que el Core se re-compile en un par de minutos, en lugar de horas, iterando rápidamente en el proceso de desacoplamiento de Qt.

## 2. Refactorización del CMake Central
El archivo `CMakeLists.txt` en la raíz contiene directivas antiguas y mezcladas.
- **Acción:** Mover todas las llamadas a `find_package(Qt...)` hacia `src/lib_gui/CMakeLists.txt`, ya que el Core no debe saber de su existencia.
- **Acción:** Configurar `Sourcetrail_lib` explícitamente como `STATIC` para asegurar que las variables estáticas globales (singletons, contadores de IDs) se compartan correctamente en tiempo de enlazado con el ejecutable principal.

## 3. Aislamiento Físico de Componentes
Si bien el código base ya tiene `src/lib` y `src/lib_gui`, hay archivos en `lib` que pertenecen conceptualmente a `gui`.
- **Acción:** Identificar clases que dependan fuertemente de interfaces gráficas o macros de Qt y moverlas a `src/lib_gui`.
- **Acción:** Re-verificar que las cabeceras base del núcleo incluyan un mecanismo de pimpl (pointer to implementation) o interfaces virtuales puras si necesitan delegar renderizado, evitando `#include <QWidget>` en el backend.

Una vez que el proyecto compile limpiamente con esta configuración estricta, pasamos a la Fase 2.
