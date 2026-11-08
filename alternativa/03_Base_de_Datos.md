# Base de Datos y Almacenamiento

El almacenamiento del grafo de dependencias de código es uno de los cuellos de botella clásicos de Sourcetrail. Sourcetrail utiliza SQLite, lo cual es estupendo para portabilidad, pero la estructura de su esquema puede ser lenta para consultas jerárquicas muy profundas (como encontrar todos los usos de una clase padre y sus hijos).

## 1. Tecnología de Almacenamiento

Mantendríamos **SQLite** por su simplicidad: no requiere que el usuario instale un servidor de base de datos (como Neo4j o PostgreSQL), y es un único archivo (`.srctrldb`), lo que facilita compartir proyectos.

Sin embargo, en el ecosistema Rust, utilizaríamos la biblioteca `rusqlite` y habilitaríamos características modernas de SQLite:
- **WAL (Write-Ahead Logging):** Para permitir lecturas concurrentes por parte de la UI mientras los workers en segundo plano siguen insertando nuevos nodos indexados.
- **Memory-Mapped I/O (mmap):** Para búsquedas ultrarrápidas de grafos grandes.

## 2. Optimización del Esquema Relacional para Grafos

Un grafo se almacena típicamente como dos tablas: `Nodos (Nodes)` y `Aristas (Edges)`.

### Esquema Propuesto:
```sql
CREATE TABLE nodes (
    id INTEGER PRIMARY KEY,
    type SMALLINT NOT NULL,      -- Clase, Metodo, Variable, Archivo, etc.
    name TEXT NOT NULL,
    serialized_name TEXT,        -- Para búsquedas globales exactas
    file_id INTEGER,
    line INTEGER,
    column INTEGER
);

CREATE TABLE edges (
    source_id INTEGER,
    target_id INTEGER,
    type SMALLINT NOT NULL,      -- Herencia, Llamada a función, Inclusión, Uso de tipo
    PRIMARY KEY (source_id, target_id, type)
) WITHOUT ROWID; -- Optimización clave para grafos en SQLite
```

### Consultas Recursivas (CTEs)
Para resolver el problema de Sourcetrail al mostrar jerarquías profundas, aprovecharíamos las CTEs recursivas (`WITH RECURSIVE`) de SQLite. Esto permite a la base de datos resolver la jerarquía completa de herencias o llamadas sin necesidad de hacer múltiples viajes (round-trips) entre el backend y la base de datos.

## 3. Caché Intermedio en Memoria

Dado que Rust es muy eficiente gestionando memoria, para proyectos pequeños/medianos (hasta 2 millones de líneas), podríamos cargar el grafo entero en memoria RAM usando estructuras de grafo especializadas (como la librería `petgraph` de Rust) al iniciar el proyecto.
- **Flujo:** SQLite actúa como persistencia en disco (para no tener que re-escanear), pero en tiempo de ejecución, las búsquedas de UI (que requieren milisegundos para ser fluidas) consultan el `petgraph` en memoria RAM.

---
> [!NOTE]
> Siguiente documento recomendado: `04_Frontend_y_UI.md` para visualizar todo esto.
