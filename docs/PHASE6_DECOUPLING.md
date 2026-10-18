# Arquitectura Desacoplada (Fase 6)

## Resumen

Hasta la iteración actual, el entorno gráfico de Sourcetrail (`Sourcetrail.exe`) estaba fuertemente acoplado a la lógica de indexación de los lenguajes (ej. C/C++ usando LLVM y Java usando JDT). Esto significaba que cualquier cambio o mejora en un indexador requería recompilar toda la interfaz, y cualquier falla (crash) en un indexador cerraba toda la aplicación.

En la **Fase 6**, logramos **desacoplar** completamente la GUI de la lógica de indexación pesada.

## Cambios Realizados

1. **Extracción de Dependencias**: 
   - Se removió la vinculación de `lib_cxx` y `lib_java` de los módulos `Sourcetrail_lib` y `Sourcetrail_lib_gui`. 
   - El ejecutable principal (`src/app/main.cpp`) ya no incluye ni inicializa `LanguagePackageCxx` ni `LanguagePackageJava`.

2. **Responsabilidad delegada al CLI**:
   - `sourcetrail_cli.exe` es ahora el único encargado de instanciar e inicializar los paquetes de lenguajes compilados estáticamente.

3. **Proxy de Indexación en la GUI**:
   - Se modificó `SourceGroupProxy` (`src/lib/project/SourceGroupProxy.cpp`). 
   - En lugar de comunicarse con los indexadores en memoria, este proxy ahora genera un comando `IndexerCommandCustom` que invoca a `sourcetrail_cli.exe` en segundo plano para realizar el trabajo pesado.
   - De esta manera, si la indexación consume demasiada memoria o crashea, el proceso afectado será `sourcetrail_cli.exe`, pero la GUI (`Sourcetrail.exe`) permanecerá estable y responsiva.

4. **Limpieza de Core (`src/lib`)**:
   - Se erradicaron las dependencias de librerías GUI (Qt) dentro de `src/lib/`. Por ejemplo, `QtResources` fue abstraído con un `ResourceProvider` inyectable. `Q_DECLARE_METATYPE` y `QtThreadedFunctor` fueron purgados o reemplazados por equivalentes agnósticos.

## Estado Actual
Tanto `sourcetrail_cli.exe` como `Sourcetrail.exe` compilan sin errores. El sistema es ahora mucho más robusto y sienta un precedente estable para la inminente migración hacia un sistema de Plugins Dinámicos (Fase 7), donde `sourcetrail_cli` cargará los indexadores desde archivos `.dll` externos.
