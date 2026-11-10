# Armar la base

El lenguaje lo resuelve el indexador de Sourcetrail. `srctrl-index` solo escribe el `.srctrlprj`
y corre el CLI (`sourcetrail_cli --refresh=all`); lo mismo se puede hacer desde la interfaz de
Sourcetrail, que además arma proyectos Java (Maven, Gradle). La base sale con el mismo esquema
para cualquier lenguaje.

| Lenguaje | Qué necesita el indexador | Comando |
| --- | --- | --- |
| C y C++ | un `compile_commands.json` (los flags de cada archivo) | `srctrl-index cdb <json> <carpeta> <nombre> --headers <src>` |
| Python | la carpeta de fuentes | `srctrl-index src python <fuentes> <carpeta> <nombre>` |
| Odin | la carpeta de fuentes | `srctrl-index src odin <fuentes> <carpeta> <nombre>` |
| Rust | la carpeta de `Cargo.toml`, `rust-analyzer` y `scip2sourcetrail` | `srctrl-index src rust <crate> <carpeta> <nombre>` |
| C# | la carpeta de la `.sln`/`.csproj`, `scip-dotnet` y `scip2sourcetrail` | `srctrl-index src csharp <proyecto> <carpeta> <nombre>` |
| Java | un proyecto Maven o Gradle | desde la interfaz de Sourcetrail |

Probado (2026-10-21) con el CLI de este repo: los tres comandos dan
`callers` correctos sobre un proyecto chico de cada lenguaje.

- La base queda en `<carpeta>/<nombre>.srctrldb`. Agregar la carpeta al `.gitignore`.
- `--threads N`: hilos del indexador (4 por defecto).
- Reindexar solo lo cambiado: `sourcetrail_cli --refresh=updated --threads=4 <carpeta>/<nombre>.srctrlprj`.

## C y C++: el `compile_commands.json`

Sourcetrail indexa C y C++ con Clang, con los mismos flags con los que se compila cada archivo:

| Build | Cómo |
| --- | --- |
| CMake (Ninja o Makefiles) | `cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; queda en `build/` |
| CMake con Visual Studio | el generador VS no lo exporta: configurar aparte con `-G Ninja` desde un "Developer Command Prompt" |
| Meson | `build/compile_commands.json`, siempre |
| Make u otro | `bear -- make` (Linux/MSYS2) o `compiledb make` |
| MSBuild (`.vcxproj`) | la extensión "Clang Power Tools" |
| Unreal Engine | `scripts/unreal_cdb.py` ([unreal](unreal.md)) |

- No hace falta compilar: alcanza con el JSON y los encabezados generados (protobuf, Qt `moc`,
  `config.h`) en su lugar. Correr antes el paso de generación del build si lo hay.
- `--headers <ruta>` (obligatorio, repetible): las carpetas de encabezados cuyos símbolos se
  guardan. Poner solo el código propio; los de terceros se parsean igual. No hay valor por
  defecto: la carpeta del JSON suele ser `build/`, y la base saldría sin los símbolos de los `.h`.
- Para un subconjunto, filtrar el JSON (una lista de `{directory, command|arguments, file}`):
  indexar todo un motor puede tardar horas. Medida: 66 unidades del renderer de Unreal en 14 min
  con 4 hilos, base de 82 MB.

## Rust y C#: por SCIP

Los indexa el compilador de cada lenguaje: `rust-analyzer scip` en Rust, y `scip-dotnet` (Roslyn)
en C#. Los dos escriben un índice SCIP, y `scip2sourcetrail` lo convierte a la base.

- **Instalar:**
  - `rustup component add rust-analyzer`;
  - un .NET SDK y `dotnet tool install -g scip-dotnet`;
  - `src/scip2sourcetrail/build.ps1 -AppDir <carpeta del CLI>`, que copia `scip2sourcetrail` a
    `<app>/data/scip`.
- **El índice se genera una vez por refresco**, junto al `.srctrlprj`, como `<nombre>_1.scip`, y se
  regenera cuando cambia una fuente. Tarda unos segundos en un crate chico y bastante más en un
  workspace grande. Los lotes siguientes lo reutilizan.
- **Lo que no indexan:** `target/`, `bin/` y `obj/`. Un `.rs` que ningún crate incluye queda con un
  error no fatal ("not in the SCIP index").
- **Nombres:**
  - Rust: `crate::módulo::Tipo::método()`. Un método de `impl Trait for T` es
    `T::método() (Trait)`.
  - C#: `Namespace.Clase.Método()`. Las sobrecargas llevan `(+1)`, `(+2)`, etc.
- **La biblioteca estándar, los crates de terceros y NuGet** quedan como no indexados: aparecen
  como destino, pero sin cuerpo.
- **En C#, `scip-dotnet` no da el cuerpo de cada definición.** `scip2sourcetrail` lo saca del texto
  (llaves, `=>`, `;`). Las referencias de `using` y de `namespace` cuelgan del archivo.

## WSL

Los scripts aceptan rutas de WSL y de Windows: `/mnt/c/...` se traduce a `C:/...` cuando el
programa es un `.exe`, y una base pedida como `C:/...` se busca en `/mnt/c/...`. No hace falta
`cygpath` (no viene en WSL); sí hace falta un `sqlite3` (el de la distribución sirve) o
`SRCTRL_SQLITE` apuntando a un `sqlite3.exe`. Si la copia de la skill no trae el `.exe` al lado de
`scripts/` (no se versiona), apuntarlo a mano.

- **El CLI puede no salir.** A veces queda vivo después de escribir la base. La base está completa
  cuando `srctrl <base> stats` responde con archivos indexados; ahí se puede terminar el proceso.
- **`rust-toolchain.toml` con un canal que no está instalado.** El indexador corre el
  `rust-analyzer` de rustup, que intenta descargar ese canal. Antes de indexar:
  `rustup override set stable --path <proyecto>` y después `rustup override unset --path <proyecto>`.
- **Workspaces con dependencias git y submódulos.** `cargo fetch` puede quedarse horas en un
  submódulo (por ejemplo el repo de JS de un bundler). El indexador espera ese fetch. Para medir
  una parte del workspace, reemplazar esas dependencias por stubs locales, o indexar un workspace
  que no incluya esos miembros, y restaurar `Cargo.toml`, `Cargo.lock` y los `Cargo.toml` tocados
  al terminar.
- **Memoria.** Un workspace grande se lleva varios GB: `rust-analyzer` sobre 1.600 archivos y
  400.000 líneas tardó unos 7 minutos y dejó una base de 136 MB.

## Comprobar

```sh
<skill>/scripts/srctrl <carpeta>/<nombre>.srctrldb stats
<skill>/scripts/srctrl <carpeta>/<nombre>.srctrldb sql "SELECT message, translation_unit FROM error WHERE fatal = 1 LIMIT 20"
```

- `srctrl-index` falla con "la base no tiene archivos indexados" si el CLI dejó una base vacía
  (necesita `sqlite3` para contarlos). Con `0/N source files indexed` y "Cannot start indexer
  process because executable is missing" en el log, falta `Sourcetrail_indexer.exe` junto al CLI
  ([compilar](compilar.md)). Con `0/0`, el proyecto no tenía fuentes o no se pudo leer.
- Errores fatales de includes en C/C++: falta un `-I` o un encabezado generado. Las relaciones de
  las unidades que sí compilaron quedan igual.
