# Frontend y Visualización (UI)

La visualización es el mayor atractivo de Sourcetrail. La interfaz actual en Qt, aunque funcional, tiene un aspecto "retro" y su motor de dibujado de grafos basado en vistas gráficas 2D personalizadas de Qt es difícil de extender y mantener.

## 1. Stack Web Frontend
Pasaremos a tecnologías web embebidas para la interfaz, servidas por Tauri.
- **Framework:** React o Svelte. Svelte es particularmente atractivo aquí porque su falta de Virtual DOM lo hace increíblemente rápido para animaciones y actualizaciones fluidas, lo cual es crítico al mover nodos de grafos pesados.
- **Estilizado (Aesthetics):** Utilizaríamos CSS moderno con "Glassmorphism", modo oscuro refinado, paletas de colores coherentes y micro-animaciones en los menús y paneles (como lo estipulan los principios de diseño de UI web moderna).

## 2. El Motor de Dibujado de Grafos

El grafo interactivo es la parte más compleja. Escribir un motor de grafos desde cero es innecesario hoy en día.
Opciones:
- **Cytoscape.js:** Muy robusto para grafos grandes y relaciones complejas (usado comúnmente en bioinformática y análisis de redes).
- **React Flow:** Excelente para grafos interactivos donde los nodos necesitan tener interfaces complejas (como mostrar listas de métodos dentro de una clase).
- **Vis.gl / Deck.gl:** Si el proyecto escalara a millones de nodos simultáneos, WebGL es obligatorio. Renderizar en 3D o 2D acelerado por hardware a través de WebGL aseguraría 60FPS sin importar el tamaño del código.

### Diseño del Nodo del Grafo:
A diferencia del Sourcetrail original, donde las clases se muestran como cajas simples, los nodos modernos podrían tener un diseño más rico:
- Mostrar previsualización de código al hacer hover directamente sobre el nodo en el lienzo del grafo.
- Minimapas de la arquitectura global.
- "Heatmaps" sobre los nodos para mostrar dónde hay más commits o complejidad ciclomática, cruzando datos de git.

## 3. Integración de Búsqueda (Fuzzy Search)

La barra superior de búsqueda debe sentirse instantánea.
- La búsqueda ya no sería "match exacto" como suele fallar Qt a veces si no escribes los namespaces correctamente.
- Implementaríamos un motor de **Fuzzy Search** en Rust (usando el algoritmo de distancia de Levenshtein o librerías como `skim` o `nucleo`), enviando resultados instantáneos al frontend web vía Tauri IPC mientras el usuario teclea.

## 4. Paneles y Layout
En lugar de ventanas acoplables de Qt (QDockWidget) rígidas, la UI web usaría un gestor de layouts moderno (estilo VS Code o GoldenLayout) que permita:
1. Panel izquierdo: Búsqueda, Filtros de git, Tipos de relaciones.
2. Panel central: El lienzo del grafo (infinito, paneable, zoomeable).
3. Panel derecho: Visor de código fuente rico con syntax highlighting (usando Monaco Editor de Microsoft, el mismo motor de VS Code).

---
> [!NOTE]
> Siguiente documento recomendado: `05_Fases_de_Implementacion.md` para ver el roadmap.
