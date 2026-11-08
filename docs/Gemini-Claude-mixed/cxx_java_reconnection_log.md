# Bitácora de Reconexión de Soporte para C++ y Java (MSVC)

**Autores**: Gemini 3.8 Flash & Claude Opus (Colaboración Mixta)  
**Fecha**: 3 de Septiembre de 2026  
**Entorno**: Windows 10, MSVC 19.50 (Visual Studio 2025 Community), CMake 3.27+, Java 25 Adoptium, Apache Maven 3.9.16.

---

## 1. Resumen Ejecutivo

Se restableció con éxito total la compilación y ejecución de:
1. **Soporte de C++**: LLVM/Clang 21.1.2 integrado como plugin dinámico (`sourcetrail_cxx_plugin.dll`, 51.8 MB).
2. **Soporte de Java**: JNI bridge plugin (`sourcetrail_java_plugin.dll`, 273 KB) y Java Indexer (`java-indexer.jar`, 61.8 KB) compilado con Apache Maven.
3. **Ejecutables**: `Sourcetrail.exe` (GUI) y `sourcetrail_cli.exe` (CLI), reconociendo y cargando ambos plugins de manera automática en el arranque desde la carpeta `plugins/`.

Todo se logró **sin recompilar ni regenerar librerías con vcpkg** (respetando la restricción de no volver a pasar por las 14 horas de compilación de LLVM).

---

## 2. Diagnóstico de C++ (LLVM/Clang) y Soluciones Aplicadas

### Causa Raíz 1: Error `RelWithDebInfo` en Generador Visual Studio
* **Síntoma**:
  ```text
  CMake Error in CMakeLists.txt:
    IMPORTED_LOCATION not set for imported target "clangBasic" configuration "RelWithDebInfo".
  ```
* **Causa real**:
  En `CMakeLists.txt`, la línea 50 definía:
  ```cmake
  set(defaultBuildConfigurations ${defaultBuildType} "Debug" "RelWithDebInfo" "MinSizeRel")
  ```
  Al usar el generador multi-config de Visual Studio, CMake intentaba resolver los 4 perfiles. Dado que vcpkg sólo compila librerías estáticas para `Debug` y `Release`, los archivos de configuración de Clang (`ClangTargets-release.cmake` y `ClangTargets-debug.cmake`) no proveían ubicaciones para `RelWithDebInfo` ni `MinSizeRel`.
* **Solución**:
  Se forzó en el nivel superior de `CMakeLists.txt`:
  ```cmake
  set(defaultBuildConfigurations "Release" "Debug")
  set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "Multi-config available build types" FORCE)
  set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
  set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
  ```

### Causa Raíz 2: Conflicto de Rutas `debug/lib/cmake/clang` vs `lib/cmake/clang`
* **Síntoma**:
  ```text
  clangBasic.lib : error LNK2038: mismatch detected for 'RuntimeLibrary': value 'MDd_DynamicDebug' doesn't match value 'MD_DynamicRelease'
  clangBasic.lib : error LNK2038: mismatch detected for '_ITERATOR_DEBUG_LEVEL': value '2' doesn't match value '0'
  ```
* **Causa real**:
  `find_package(Clang CONFIG REQUIRED)` de CMake buscaba en las rutas de vcpkg y encontraba primero `build/vcpkg_installed/x64-windows/debug/lib/cmake/clang/ClangConfig.cmake`. Al cargar la configuración Debug en una compilación Release, forzaba el enlazado con las librerías debug de Clang compiladas con `/MDd`.
* **Solución**:
  En `CMakeLists.txt`, se forzó explícitamente `Clang_DIR` a la carpeta de Release antes de llamar a `find_package(Clang)`:
  ```cmake
  if (isVcpkgBuild)
      set(Clang_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/cmake/clang" CACHE PATH "Path to ClangConfig.cmake" FORCE)
  endif()
  ```

### Causa Raíz 3: Desbordamiento de la Tabla de Exportaciones (`LNK1106`)
* **Síntoma**:
  ```text
  sourcetrail_cxx_plugin.exp : fatal error LNK1106: invalid file or disk full: cannot seek to 0x6552FC29
  ```
* **Causa real**:
  En `src/lib_cxx/CMakeLists.txt`, la propiedad `WINDOWS_EXPORT_ALL_SYMBOLS ON` estaba activada en un plugin que enlaza estáticamente todo Clang y LLVM. El linker intentó exportar más de 100,000 símbolos en el archivo `.exp`, alcanzando 1.7 GB y rompiendo el límite de Windows PE.
* **Solución**:
  Se removió `WINDOWS_EXPORT_ALL_SYMBOLS ON` de `src/lib_cxx/CMakeLists.txt`. El plugin sólo necesita exportar su función de entrada:
  `extern "C" __declspec(dllexport) void sourcetrail_plugin_init()`.
  Para las clases utilitarias requeridas por la GUI (`IncludeDirective` e `IncludeProcessing`), se exportaron específicamente usando la macro `SOURCETRAIL_CXX_EXPORT`.

---

## 3. Diagnóstico de Java y Soluciones Aplicadas

### Causa Raíz 1: Maven y JDK
* **Solución**:
  Se detectó Apache Maven en PATH (`C:\Software\apache-maven-3.9.16\bin\mvn`). CMake lo configuró junto al JDK 25 (`C:\Program Files\Eclipse Adoptium\jdk-25.0.3.9-hotspot\`).

### Causa Raíz 2: "Most Vexing Parse" en `JavaPathDetectorLinuxWindowsMac.cpp`
* **Síntoma**:
  ```text
  error C2228: left of '.concatenate' must have class/struct/union
  error C2440: 'return': cannot convert from 'FilePath (__cdecl *)(std::string)' to 'FilePath'
  ```
* **Causa**:
  La línea `FilePath javaPath(std::string(p));` fue interpretada por el compilador como una declaración de función en vez de una instanciación de objeto.
* **Solución**:
  Se corrigió a `FilePath javaPath(p);`.

### Causa Raíz 3: Funciones Huérfanas de Qt en `utilityJava.cpp`
* **Síntoma**:
  ```text
  error C3861: 'qgetenv': identifier not found
  error C3861: 'qputenv': identifier not found
  ```
* **Solución**:
  Se reemplazaron por `std::getenv` y `_putenv_s` (en Windows) / `setenv` (en POSIX), con el include `<cstdlib>`.

### Causa Raíz 4: Include Faltante en `Sourcetrail_lib_gui`
* **Síntoma**:
  ```text
  error C1083: Cannot open include file: 'utilityMaven.h': No such file or directory
  ```
* **Solución**:
  En `src/lib_gui/CMakeLists.txt`, se agregaron `${CMAKE_CURRENT_SOURCE_DIR}/../lib_java/utility` y `${CMAKE_CURRENT_SOURCE_DIR}/../lib_java` a `target_include_directories`.

### Causa Raíz 5: Enlazado de `Sourcetrail_lib_java` en `Sourcetrail.exe`
* **Síntoma**:
  Símbolos no resueltos para `SourceGroupJavaEmpty`, `SourceGroupJavaGradle`, `SourceGroupJavaMaven`, y `utility::mavenGenerateSources`.
* **Solución**:
  En `src/app/CMakeLists.txt`, se agregó:
  ```cmake
  if(BUILD_JAVA_LANGUAGE_PACKAGE)
      target_link_libraries(Sourcetrail PRIVATE Sourcetrail_lib_java)
  endif()
  ```

---

## 4. Arquitectura de Plugins y Directorios de Salida

Se ajustaron las propiedades de destino de los targets en CMake:
* `Sourcetrail_lib_cxx` genera automáticamente en: `${CMAKE_BINARY_DIR}/Release/app/plugins/sourcetrail_cxx_plugin.dll`
* `Sourcetrail_lib_java` genera automáticamente en: `${CMAKE_BINARY_DIR}/Release/app/plugins/sourcetrail_java_plugin.dll`
* `Sourcetrail_java_indexer` genera mediante Maven en: `${CMAKE_BINARY_DIR}/Release/app/data/java/lib/java-indexer.jar`

Al arrancar `Sourcetrail.exe` o `sourcetrail_cli.exe`, `PluginManager::loadPlugins()` escanea la carpeta `plugins/` y carga ambos módulos sin configuración manual requerida.

## 5. Diagnóstico de Carga en Windows: PE Loader vs Subcarpeta `plugins/`

* **Síntoma reportado por el usuario al ejecutar `Sourcetrail.exe`**:
  - *"The code execution cannot proceed because sourcetrail_cxx_plugin.dll was not found."*
  - *"The code execution cannot proceed because sourcetrail_java_plugin.dll was not found."*
* **Causa Raíz**:
  `Sourcetrail.exe` y `Sourcetrail_lib_gui` contienen dependencias de enlazado directo con funciones de ambos módulos (`IncludeProcessing`, `SourceGroupJava*`, etc.). El cargador PE del núcleo de Windows (antes de ejecutar la primera instrucción de `main()`) busca las DLLs requeridas exclusivamente en el directorio donde reside el `.exe` (`app/`), en `System32` o en el `PATH`. No busca en subcarpetas arbitrarias como `plugins/`.
  Al haberse configurado previamente la salida únicamente en `app/plugins/`, el cargador de Windows no las encontraba y abortaba con ventana de error.
* **Solución definitiva en CMake**:
  En `src/lib_cxx/CMakeLists.txt` y `src/lib_java/CMakeLists.txt`:
  1. Se configuró `RUNTIME_OUTPUT_DIRECTORY` hacia `app/` (junto a `Sourcetrail.exe`), satisfaciendo de inmediato al cargador de Windows.
  2. Se configuró un comando `POST_BUILD` que copia idénticamente los `.dll` a `app/plugins/`, permitiendo que el sistema de plugins dinámicos de `PluginManager` siga cargándolos y reconociéndolos también en esa ruta.
