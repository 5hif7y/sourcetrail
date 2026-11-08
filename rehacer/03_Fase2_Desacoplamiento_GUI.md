# Fase 2: Desacoplamiento de Qt en el Núcleo (Core)

Para que el núcleo pueda ser consumido por un API en C e integrado en plugins de lenguaje puro, no debe contener ningún rastro de la librería Qt. Todo el código debe estar en C++ estándar o Boost.

## 1. Migración de JSON
El código utilizaba `QJsonObject` y `QJsonDocument` intensivamente para serialización en clases del Core.
- **Acción:** Reemplazar `QJsonObject` por la librería `nlohmann/json` (C++11, header-only, y ya presente en el ecosistema).
- **Acción Alternativa (Ligera):** Si el uso es trivial (ej. enviar cadenas de texto en los CommandProviders), reemplazar la serialización de JSON con `std::string` puro e interpolación de texto.

## 2. Refactorización de Codificación de Textos (`TextCodec`)
Una de las dependencias más profundas de Qt en el Core era `QStringDecoder`, `QStringEncoder` y `QByteArray`.
- **Acción:** Reescribir la clase `TextCodec.cpp` (en `src/lib/utility/text/`).
- **Implementación:** Usar `boost::locale::conv::between` para la conversión de encodings y manipular todo puramente a través de `std::string`.

## 3. Macros del Meta-Objeto de Qt
Archivos base como `Id.h`, `LanguageType.h` incluían `#include <QMetaType>` y la macro `Q_DECLARE_METATYPE` al final del archivo.
- **Acción:** Borrar estas declaraciones de los archivos del núcleo.
- **Solución:** Crear un nuevo archivo centralizado llamado `QtMetaTypes.h` **dentro del directorio `src/lib_gui`**, e incluir ahí todas las declaraciones `Q_DECLARE_METATYPE`. La GUI sigue necesitando estas declaraciones para enviar signals/slots, pero el núcleo ya no está "contaminado" por ellas.

## 4. Tipos Estándar
Reemplazar cualquier uso remanente de `QString` por `std::string` y `QStringList` por `std::vector<std::string>` a lo largo del directorio `src/lib`.

Con esto, garantizamos que compilar el núcleo requiere únicamente de un compilador de C++ estándar y Boost, habilitando la creación transparente de una API en C.
