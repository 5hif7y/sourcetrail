# 39. scip2sourcetrail: convertidor de SCIP a `.srctrldb`

**Fecha:** 2026-11-08
**Rama:** `feat/SCIP2Sourcetrail`, que sale de `feat/ABI-writer-v2`.
**Plan:** es el paso 2 de cuatro (ver doc 38):

1. ABI v2 del writer.
2. **Convertidor**.
3. Rust y C# por SCIP.
4. Las bases para tener indexadores propios.

## Qué es

`src/scip2sourcetrail` es una herramienta en Rust. Lee un `index.scip` y escribe una base de
Sourcetrail a través del writer (ABI v2). Los índices SCIP salen de indexadores construidos sobre el
compilador real de cada lenguaje: `rust-analyzer scip` para Rust y `scip-dotnet` (Roslyn) para C#.
Hay indexadores SCIP para otros lenguajes, y el convertidor sirve para todos.

```sh
scip2sourcetrail index --scip=<index.scip> --database-file-path=<db> \
    [--source-file-path=<archivo>]... [--project-root=<dir>] \
    [--delimiter=<sep>] [--language=<id>] [--stats]
```

Los argumentos son los mismos que usan los indexadores de comando personalizado de Sourcetrail, así
que `%{SOURCE_FILE_PATH}` y `%{SOURCE_FILE_PATH_ARGS}` funcionan sin cambios.

## Diseño

- **Índice completo, escritura parcial.** Cada corrida lee todo el índice, pero escribe solo los
  archivos que se le piden. Nombres, tipos y tipo de definición se calculan sobre el índice entero,
  así que un símbolo es el mismo nodo en todos los lotes. Un archivo pedido que no está en el índice
  recibe un error fatal.
- **Nombres.** Cada descriptor SCIP es un nivel. El paquete, cuando hay (el crate en Rust), es el
  primer nivel.
  - El separador es `::` para Rust y `.` para C#; sale de la herramienta que generó el índice.
  - El desambiguador de método va al postfijo (`Scale()` y `Scale(+1)`), así que las sobrecargas
    de C# quedan como dos símbolos.
  - El nombre depende solo del texto del símbolo SCIP.
- **Tipos de símbolo.** Se toman de `SymbolInformation.kind` (trait → interface, property → field,
  etc.). Si falta, se deducen del sufijo del último descriptor.
- **Definiciones.**
  - El token de la ocurrencia de definición es la ubicación del símbolo, y su `enclosing_range` es
    el cuerpo (scope).
  - Un símbolo que no está definido en ningún documento del índice queda sin tipo de definición, es
    decir, no indexado. Es lo que pasa con `std`, NuGet o los crates de terceros.
- **Referencias.** El origen es la definición más interna cuyo `enclosing_range` contiene la
  referencia; si no hay ninguna, el origen es el archivo. El tipo de referencia:

  | Destino | Referencia |
  | --- | --- |
  | Ocurrencia con rol `Import` | import |
  | Macro | uso de macro |
  | Función o método | llamada |
  | Tipo | uso como tipo |
  | Cualquier otra cosa | uso |

- **Relaciones.** `is_implementation` da herencia si une tipos (`impl Trait for T`,
  `class C : I`), y override si une métodos.
- **Locales y parámetros** pasan a ser símbolos locales. Los `local N` de SCIP llevan la ruta del
  documento en el nombre para que no choquen entre archivos.
- **Diagnósticos** de severidad `Error` pasan a ser errores no fatales.
- **Rangos.** SCIP usa base 0 con el final exclusivo; Sourcetrail usa base 1 con el final
  inclusivo.
- **Rutas.** Se toma la raíz del proyecto de la metadata (`file:///...`, o `--project-root`), se le
  agrega la ruta relativa con `/`, y para comparar se normaliza: barras, `./` y mayúsculas en
  Windows. Se graba la ruta tal como la pasó Sourcetrail, para que el nodo de archivo coincida.
- **Enlace con el writer.** `build.rs` enlaza el writer desde `SODB_WRITER_DIR` o desde
  `build/odin_writer`. En Windows copia la DLL junto a los binarios.

## Arreglo en `srctrl` encontrado en el camino

El camino rápido de `srctrl` borraba todos los `\ts\tp` como si fueran un prefijo y un postfijo
vacíos. En `\ts\tp()` la firma no está vacía, así que el nombre quedaba como `area()` y `callers`
no encontraba nada. Con Clang nunca había pasado, porque siempre graba un prefijo (el tipo de
retorno). Commit `9b81d013`:

- Ahora el resultado coincide con el decodificador recursivo en 6 bases: Unreal, el core de Odin,
  dos de Python y las de prueba de Rust y C#.
- Las 4 bases que ya existían se decodifican igual que antes.
- `refs` en Unreal tarda unos 0,8 s más.

## Verificación

- **`cargo test`:** 14 pruebas, 8 unitarias y 6 de punta a punta.
  - Las de punta a punta arman un índice SCIP en memoria, lo convierten y leen la base con SQLite.
  - Cubren un crate de Rust, un lote con un archivo faltante, sobrecargas de C#, la lectura de un
    `index.scip` desde disco y entradas inválidas.
- **`cargo clippy --all-targets`:** sin avisos.
- **De punta a punta por Sourcetrail:**
  - Un proyecto con un source group de comando personalizado, que llama a `scip2sourcetrail` con
    `%{SOURCE_FILE_PATH}` sobre los dos archivos del crate de prueba.
  - El CLI de Sourcetrail indexó 2 archivos, 20 nodos y 29 aristas, sin errores. Son los mismos
    números que da la conversión directa.
  - `srctrl callers area` devuelve `demo::total()` en `main.rs:6`.
- **CI:** el job `scip2sourcetrail` compila el writer y corre `cargo test --locked` en Linux,
  Windows y macOS.

## Lo que no se sabe hasta el paso 3

Los índices de las pruebas los escribí a mano. Falta ver la salida real de los indexadores, para lo
cual hay que instalar `rust-analyzer` (`rustup component add rust-analyzer`) y el .NET SDK con
`scip-dotnet`.

- **`enclosing_range`:** si rust-analyzer o scip-dotnet no lo completan, casi todas las
  referencias van a colgar del archivo. Se vería en `--stats`, en `references_without_context`.
- **Métodos de las impls de trait en rust-analyzer.** En la prueba usé
  `impl#[Square][Area]area().`, que deja `impl::Square::Area::area()` como nombre. Si el formato
  real es así, conviene un nombre más legible, por ejemplo `Square::area`, con la relación al trait
  como override.
- **Columnas.** SCIP indica el encoding de las posiciones (UTF-8, UTF-16 o UTF-32) y el convertidor
  lo ignora. Con texto que no sea ASCII, las columnas pueden correrse.
- **Rendimiento.** Cada lote vuelve a leer el índice entero. En un proyecto grande puede convenir
  un solo lote con todos los archivos (`getRunInBatch`), o guardar el índice ya procesado.
