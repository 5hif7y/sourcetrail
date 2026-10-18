# Backend y Parsers: Escaneo de Código Inteligente

El corazón de Sourcetrail es su capacidad para entender el código fuente (símbolos, referencias, usos, herencia). En la reescritura, la estrategia de los parsers cambiaría drásticamente para ser más moderna y escalable.

## 1. Adiós a Clang para todo; Hola Tree-sitter

Originalmente, Sourcetrail dependía en gran medida de ASTs construidos a mano o herramientas pesadas como `libclang` (que requiere un compilador entero y la base de datos de compilación `compile_commands.json`).

### El Nuevo Enfoque:
- **Tree-sitter:** Usaríamos Tree-sitter como el motor principal de parsing para lenguajes como Python, Java, JavaScript, TypeScript, Rust, Go, etc.
  - **Ventajas:** Tree-sitter es extremadamente rápido, tolerante a errores (puede parsear código incompleto o con errores de sintaxis) y tiene bindings nativos en Rust (`tree-sitter-rs`).
- **Clang (Opcional/Híbrido para C++):** Dado que C++ es notoriamente difícil de parsear por su sistema de macros e includes, mantendríamos `libclang` solo para análisis profundo de C/C++, pero empaquetado como un proceso esclavo aislado para evitar caídas del núcleo de la aplicación.

## 2. El Pipeline de Indexado en Rust

El motor de indexado en Rust seguiría un modelo "Map-Reduce" utilizando la librería `Rayon` para pararelismo masivo.

1. **Discovery (Descubrimiento):** Escaneo rápido del sistema de archivos respetando `.gitignore`.
2. **AST Parsing (Map):** Múltiples hilos (tantos como núcleos de CPU) leen los archivos y generan ASTs locales usando Tree-sitter.
3. **Symbol Extraction:** Recorrer los ASTs extrayendo Símbolos (Clases, Métodos, Variables) y Referencias (Llamadas, Herencia, Inclusiones).
4. **Resolution (Reduce):** Resolver referencias cruzadas entre archivos. Aquí es donde Rust brilla: usando diccionarios en memoria eficientes (`dashmap` para concurrencia) para vincular referencias a sus definiciones globales.

## 3. Arquitectura de Plugins Real

En la nueva arquitectura, los parsers serían verdaderos plugins asíncronos. En Rust, esto se puede implementar de dos maneras:
- **WebAssembly (WASM):** Los plugins de lenguajes se compilan a `.wasm`. El backend de Rust carga los plugins en un entorno aislado usando un motor como `Wasmtime`. Esto garantiza seguridad total (un plugin no puede tirar la aplicación principal) y permite a la comunidad escribir parsers en cualquier lenguaje (Rust, C++, Go, AssemblyScript) siempre que compile a WASM.
- **Language Server Protocol (LSP):** Alternativamente, en lugar de parsear nosotros mismos, Neo-Sourcetrail podría conectarse a los servidores LSP existentes (ej. `clangd`, `jedi`, `pyright`, `rust-analyzer`) y simplemente extraer la información del grafo a partir de las respuestas LSP (Go to Definition, Find References). Esto reduciría drásticamente el costo de mantenimiento.

---
> [!NOTE]
> Siguiente documento recomendado: `03_Base_de_Datos.md` para ver cómo almacenaríamos todo este grafo.
