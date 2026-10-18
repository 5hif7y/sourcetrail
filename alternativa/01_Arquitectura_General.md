# Arquitectura General: Neo-Sourcetrail 2.0

Si tuviera la oportunidad de rehacer Sourcetrail desde cero en la actualidad, abandonaría la arquitectura basada puramente en C++ y Qt, la cual es pesada, difícil de empaquetar y propensa a problemas de ABI (como hemos experimentado). En su lugar, apostaría por una arquitectura híbrida y moderna que maximice el rendimiento, la mantenibilidad y la facilidad de crear interfaces espectaculares.

## El Stack Tecnológico Ideal

### 1. Backend: Rust
Rust es el lenguaje ideal para reemplazar el núcleo (Core) de Sourcetrail.
- **Rendimiento y Seguridad:** Ofrece un rendimiento equivalente a C++, pero sin los errores de memoria (segfaults) ni los problemas de punteros colgados.
- **Concurrencia:** El procesamiento de millones de líneas de código requiere pararelismo extremo. Con librerías como `Rayon`, Rust hace que la concurrencia sea trivial y segura.
- **Ecosistema (Cargo):** El gestor de paquetes de Rust elimina la necesidad de luchar contra CMake, vcpkg y configuraciones complejas de enlazado. Las librerías de terceros se integran limpiamente.

### 2. Frontend: Tauri + React (o Svelte)
Qt es robusto, pero el desarrollo web moderno avanza mucho más rápido en términos de UI/UX.
- **Por qué Tauri:** A diferencia de Electron (que incluye un Chromium completo y consume muchísima RAM), Tauri utiliza el webview nativo del sistema operativo. Esto permite tener una interfaz web dinámica consumiendo recursos mínimos.
- **Interfaz (React/Svelte):** Permite usar bibliotecas de renderizado gráfico avanzadas (como D3.js, Cytoscape.js o React Flow) para dibujar el grafo de dependencias de manera interactiva, suave y estética.

### 3. Base de Datos: SQLite (Optimizado)
El almacenamiento local sigue siendo mejor con SQLite, pero lo integraríamos de manera más eficiente:
- Utilizaríamos `rusqlite` en Rust.
- Podríamos considerar arquitecturas orientadas a grafos incrustadas (embedded graph databases), pero un esquema relacional bien indexado en SQLite sigue siendo imbatible en portabilidad y velocidad si se estructuran correctamente los IDs.

## Arquitectura Multi-Proceso (Client-Server Local)
Para evitar que un fallo en un parser destruya la interfaz gráfica, el sistema se dividiría internamente:
1. **Indexer Workers:** Procesos independientes que parsean código (si un parser de C++ falla con un Segfault, el worker muere, pero la UI sigue viva y simplemente lo reporta).
2. **Core Server (Tauri Backend):** Coordina la base de datos, consolida los grafos y se comunica con la UI.
3. **Web UI:** Se encarga exclusivamente del renderizado.

---
> [!NOTE]
> Siguiente documento recomendado: `02_Backend_y_Parsers.md` para ver cómo resolveríamos el análisis de código.
