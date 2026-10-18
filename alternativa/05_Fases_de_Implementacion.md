# Plan de Ejecución: Fases de Implementación

Rehacer un proyecto del tamaño de Sourcetrail desde cero requiere un roadmap estricto para no perderse en los detalles. El desarrollo debe ser iterativo.

## Fase 1: MVP Core (Backend Puro)
*Objetivo:* Leer código, parsearlo, y generar la base de datos de grafos sin interfaz gráfica.

1. Iniciar un proyecto en Rust (`cargo new neo-sourcetrail`).
2. Configurar el esquema de la base de datos con `rusqlite`.
3. Integrar `tree-sitter` (ej. `tree-sitter-python`) y escribir un walker recursivo que extraiga ASTs y resuelva nombres básicos.
4. Desarrollar la CLI (Command Line Interface). 
*Hito 1:* Ejecutar `neo-sourcetrail index ./mi-proyecto-python` y observar cómo se genera el archivo `.db` correctamente estructurado.

## Fase 2: MVP Frontend (Visor Estático)
*Objetivo:* Visualizar el grafo generado en la Fase 1.

1. Inicializar Tauri con React/Svelte (`npm create tauri-app`).
2. Configurar el IPC de Tauri para consultar la base de datos de SQLite desde Rust y enviar JSONs al Frontend.
3. Integrar React Flow o Cytoscape.js para renderizar los nodos y las aristas enviados por Rust.
*Hito 2:* Abrir la UI de Tauri y poder ver el grafo de dependencias de las clases de Python escaneadas en la Fase 1.

## Fase 3: Indexado Avanzado (C++)
*Objetivo:* Resolver el problema más difícil: C/C++.

1. Crear un sistema de plugins o un servicio esclavo en Rust que controle `libclang`.
2. Implementar la detección de dependencias usando `compile_commands.json` nativamente en Rust.
3. Resolver los casos borde del AST de Clang (templates, macros) y unificarlos en el modelo relacional universal de la DB.
*Hito 3:* Escanear un gran proyecto de C++ (ej. el código fuente del propio Sourcetrail) y visualizarlo fluidamente.

## Fase 4: Experiencia de Usuario (IDE y Code Viewer)
*Objetivo:* Hacer que la herramienta sea productiva para el programador.

1. Integrar **Monaco Editor** en el panel lateral para poder ver el código fuente (no solo las cajas del grafo).
2. Sincronizar el grafo con el editor (hacer clic en un nodo resalta la línea en Monaco Editor, y viceversa).
3. Añadir el buscador global interactivo con Fuzzy Search (Levenshtein) para saltar rápidamente entre nodos.
*Hito 4:* El usuario puede navegar código y entender su estructura cómodamente sin salir de la app.

## Fase 5: Integración Ecosistema (LSP y Extensiones)
*Objetivo:* Acercar Neo-Sourcetrail al editor del usuario.

1. En lugar de una app independiente que requiere indexar manualmente, construir plugins/extensiones para **VS Code** e **IntelliJ**.
2. La extensión extrae datos vivos del Language Server Protocol (LSP) y envía el grafo al panel web embebido (WebView) dentro de VSCode, eliminando por completo el paso de "Re-Indexar".
*Hito 5 (Meta Final):* Sourcetrail deja de ser una app de escritorio externa y se convierte en una ventana inmersiva nativa del IDE del programador.
