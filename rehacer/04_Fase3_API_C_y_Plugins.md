# Fase 3: API C (C-API) y Gestor de Plugins

Al extraer los indexadores de lenguaje en librerías dinámicas (`.dll`), surge el problema del "ABI de C++" (Application Binary Interface): si un plugin en C++ se enlaza estáticamente contra un core estático en C++, ambos terminarán con copias independientes de las variables globales (como singletons o colas de mensajes) y la memoria se corromperá al cruzar el límite del DLL. Para evitarlo sin recurrir al pesado `WINDOWS_EXPORT_ALL_SYMBOLS`, implementaremos una API en C puro.

## 1. La Barrera C-API (`sourcetrail_api.dll`)
Crearemos un nuevo módulo llamado `src/lib_api`.
- **Objetivo:** Exponer la funcionalidad clave de `Sourcetrail_lib` (el core) a través de funciones globales usando notación "C" (`extern "C" __declspec(dllexport)`).
- **Ejemplo:** En lugar de que un plugin herede de `SourceGroup` o llame a `SourceGroupFactory::getInstance()->addModule(miModuloCxx)`, el plugin utilizará la API C: `sourcetrail_api_add_source_group_module("cxx", ...)`.
- **Ventaja:** C tiene un ABI universal. Permite compilar los plugins con distintas banderas o versiones del compilador sin romper Sourcetrail.

## 2. Abstracción del Sistema de Factorías
Modificar `LanguagePackageManager` y `SourceGroupFactory` dentro del Core para que puedan ser nutridos dinámicamente:
- No deben conocer `LanguagePackageCxx` o `SourceGroupFactoryModuleCxx` en tiempo de compilación.
- Su única forma de registrar paquetes debe ser a través de una inyección que reciba punteros genéricos desde el C-API y utilice el patrón Strategy o Bridge para delegar el parseo.

## 3. El `PluginManager`
Crear un administrador de plugins dentro de `src/lib`.
- **Responsabilidad:** Al iniciarse el programa, buscar en el directorio `plugins/` cualquier archivo `.dll` o `.so`.
- **Mecanismo:** Utilizar `boost::dll::shared_library`. Para cada plugin, buscar la función simbólica de inicialización exportada unificada, por ejemplo `sourcetrail_plugin_init()`, y ejecutarla. 
- **Flujo:** La ejecución de `sourcetrail_plugin_init()` dentro del plugin llamará a las funciones de `sourcetrail_api.dll` para registrarse en las factorías del Core.
