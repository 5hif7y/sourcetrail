# 40. Rust y C# por SCIP, y bases para indexadores propios

**Fecha:** 2026-11-09
**Rama:** `feat/SCIP-Rust-CSharp`, que sale de `feat/SCIP2Sourcetrail` (doc 39), que a su vez sale
de `feat/ABI-writer-v2` (doc 38).
**Plan:** son los pasos 3 y 4 de cuatro:

1. ABI v2 del writer.
2. Convertidor.
3. **Rust y C# por SCIP.**
4. **Bases para los indexadores propios.**

## Resultado

Sourcetrail indexa Rust y C# con dos source groups nuevos:

| Source group | Id | Indexador | Proyecto (primera ruta de fuentes) |
| --- | --- | --- | --- |
| Rust Source Group (rust-analyzer) | `rust_scip` | `rust-analyzer scip` | carpeta de `Cargo.toml` |
| C# Source Group (scip-dotnet) | `csharp_scip` | `scip-dotnet index` (Roslyn) | carpeta (o archivo) de la `.sln`/`.csproj` |

Los dos son source groups de comando personalizado, como el de Odin:

```
scip2sourcetrail index --generate=rust|csharp --project="<ruta>" \
    --scip="<proyecto>_<id del grupo>.scip" %{SOURCE_FILE_PATH_ARGS} \
    --database-file-path="%{DATABASE_FILE_PATH}"
```

Cómo se arma ese comando:
- **El índice SCIP es una caché** junto al `.srctrlprj`. `scip2sourcetrail` lo regenera solo si
  alguna fuente es más nueva. Un lock (`<scip>.lock`) hace que los lotes paralelos esperen al que
  lo genera.
- **Las extensiones y el comando son fijos.** Por defecto se excluyen `target/` en Rust y `bin/` y
  `obj/` en C#.
- **Dónde se busca `scip2sourcetrail`**, en este orden:
  1. `SOURCETRAIL_SCIP2SOURCETRAIL`;
  2. `<app>/data/scip`, donde lo copia `src/scip2sourcetrail/build.ps1`;
  3. un build de cargo en `src/scip2sourcetrail/target`.
- **Desde la skill:** `srctrl-index src rust|csharp <proyecto> <carpeta> <nombre>`.

## Lo que mostraron los índices reales

Los índices de prueba del doc 39 los había escrito a mano. Con `rust-analyzer` 1.97 y
`scip-dotnet` 0.2.16 instalados, aparecieron estas diferencias, y cada una tiene su commit:

| Indexador | Cómo es | Qué hace el convertidor |
| --- | --- | --- |
| rust-analyzer | La raíz del crate es `crate/` | Es el crate mismo (`demo`) |
| rust-analyzer | Los miembros de impl son `impl#[Square]new().` e `impl#[Square][Area]area().` | `Square::new()` y `Square::area() (Area)` |
| rust-analyzer | `impl#[Square]` solo es `Self` | Es `Square`, sin tipo propio. Su referencia cuenta como uso de tipo |
| rust-analyzer | No hay relaciones de implementación | Herencia y override deducidos del nombre del impl |
| rust-analyzer | Un operador es una ocurrencia por carácter | Las ocurrencias contiguas se unen en una |
| rust-analyzer | `relative_path` con `\` en Windows | Las rutas se escriben con `/` |
| rust-analyzer | Los módulos abarcan su archivo entero | 0 referencias sin contexto |
| scip-dotnet | No da `enclosing_range` | El cuerpo se calcula del texto (`text.rs`) |
| scip-dotnet | No da tipos de símbolo | La palabra clave antes del nombre (`class`, `interface`...) |
| scip-dotnet | Los constructores son `.ctor` y `.cctor` | `Square.Square()` y `Square.static Square()` |
| scip-dotnet | El ensamblado viene como paquete | No es un nivel del nombre |

Sourcetrail se queda con el tipo de nodo más alto entre lotes (`SqliteIndexStorage::addNodes`). Por
eso un `class` que se deduce en el lote del archivo que define el tipo le gana al `type` genérico de
los otros lotes.

## Verificación

- **`cargo test` del convertidor:** 24 pruebas, todas en verde.
  - 15 unitarias: nombres, rangos, texto, la caché de `--generate` y el lock.
  - 7 de punta a punta con índices sintéticos.
  - 2 con los indexadores reales sobre `tests/fixtures`, que se saltean solas si el indexador no
    está instalado.
- **CLI de Sourcetrail** (build embebido Release), con los proyectos demo:

  | | Rust | C# |
  | --- | --- | --- |
  | Archivos | 2 de 2 | 2 de 2 (`obj/` excluido) |
  | Nodos | 27 | 19 |
  | Aristas | 50 | 36 |
  | Errores | 0 | 0 |
  | Tiempo | 6 s | 0,5 s con el índice ya en caché |

  - `srctrl callers` coincide con lo esperado en los dos lenguajes.
  - Con `build.ps1`, el CLI encuentra `data/scip/scip2sourcetrail.exe` sin la variable de
    entorno.
- **Índice real del propio convertidor:** las líneas de `callers to_range` coinciden con las de
  `grep`, 12 de 12.
- **`language_registry_test` y `ui_registry_test`:** pasan.
- **CI** (no corrido todavía para esta rama): el job `scip2sourcetrail` instala `rust-analyzer`,
  .NET 10 y `scip-dotnet`, corre las pruebas con los indexadores reales y el smoke test del binding
  de C#. `root-ctest` compila el núcleo con los source groups nuevos en Linux y macOS.

## Lo que no se verificó

- **La GUI.** No hay Qt instalado en la máquina de desarrollo. Los cambios de `QtProjectWizard` y
  `UiPlugin` son dos `case` y dos entradas, iguales a las de Odin.
- **Proyectos grandes.** No se midió el tiempo ni la memoria de `rust-analyzer` sobre un workspace
  grande. Para Rust, cada refresco con cambios vuelve a correr el indexador entero.
- **Texto en C#.** El cálculo de cuerpos ignora las comillas dentro de `{}` en las cadenas
  interpoladas (`$"{a["x"]}"`), los preprocesadores (`#if`) y los genéricos con `=>` raros.
- **Columnas** con caracteres que no son ASCII: el convertidor no aplica el encoding que declara el
  índice.

## Paso 4: bases para indexadores propios

**Cuándo conviene uno propio.** SCIP no distingue una llamada de un uso: el convertidor lo deduce
por el tipo del destino. Tampoco trae calificadores, y en C# no trae ni cuerpos ni tipos de
símbolo. Un indexador propio escribe la base directamente con la ABI v2 y obtiene todo eso del
compilador.

**Lo que ya está:**

| Pieza | Dónde | Estado |
| --- | --- | --- |
| ABI v2 (firmas, tipo de definición, locales, calificadores, ambigüedad) | `src/odin_writer` | Probada en Linux, Windows y macOS (doc 38) |
| Binding de Rust | `src/scip2sourcetrail/src/writer.rs` | En uso en el convertidor |
| Binding de C# (P/Invoke) | `src/odin_writer/bindings/csharp/Sodb.cs` | Smoke test en Windows y en CI |
| Source groups, lotes y despliegue | `SourceGroupSettingsScip`, `build.ps1` | Un indexador propio puede reutilizar el mismo source group cambiando solo el comando |

**Diseño propuesto, sin implementar:**

- **C#:** una consola .NET con `Microsoft.CodeAnalysis.Workspaces.MSBuild` que abra la solución y
  recorra el `SemanticModel` de cada documento pedido.

  | Construcción de Roslyn | En Sourcetrail |
  | --- | --- |
  | `InvocationExpressionSyntax` | llamada |
  | `IdentifierName` sobre un campo o propiedad | uso |
  | Tipos | uso como tipo |
  | `OverriddenMethod` y miembros de interfaz | override |
  | `BaseList` | herencia |
  | Atributos | uso de anotación |
  | `ILocalSymbol` y `IParameterSymbol` | locales |
  | `MemberAccess` | calificador |
  | Candidatos ambiguos | referencia ambigua |

  El cuerpo sale de `DeclaringSyntaxReferences` y la firma va al postfijo (`(int, string)`). Se
  calcula que lleva unas 3 semanas.
- **Rust:** los crates `ra_ap_*` de rust-analyzer (`ra_ap_ide`, `ra_ap_hir`) cargan el workspace con
  `ra_ap_load_cargo` y recorren las definiciones con `hir::Semantics`. Las referencias salen de
  `find_all_refs`, o mejor de un recorrido del árbol con `Semantics::resolve_*`, que dice si cada
  sitio es una llamada o un uso. Se calcula en 4 semanas o más: la API de `ra_ap_*` cambia todas
  las semanas y no es estable.
- **Integración:** el mismo `SourceGroupSettingsScip`, con un modo `--indexer=native` o un ejecutable
  propio. Las pruebas reutilizarían los fixtures de `src/scip2sourcetrail/tests/fixtures`, y lo que
  ya da SCIP serviría de piso: el indexador propio tiene que dar lo mismo o más.
