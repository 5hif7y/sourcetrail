#ifndef QT_META_TYPES_H
#define QT_META_TYPES_H

// Qt meta-type declarations for core types used in Qt signal/slot connections.
// These are separated from the core headers to keep lib Qt-free.

#include <QMetaType>

#include "Id.h"
#include "TabIds.h"
#include "LanguageType.h"
#include "SourceGroupType.h"

Q_DECLARE_METATYPE(Id)
Q_DECLARE_METATYPE(TabId)
Q_DECLARE_METATYPE(LanguageType)
Q_DECLARE_METATYPE(SourceGroupType)

#endif // QT_META_TYPES_H
