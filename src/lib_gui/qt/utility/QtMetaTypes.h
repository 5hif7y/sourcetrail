#ifndef QT_META_TYPES_H
#define QT_META_TYPES_H

// Qt meta-type declarations for core types used in Qt signal/slot connections.
// These are separated from the core headers to keep lib Qt-free.

#include <QMetaType>

#include "Id.h"
#include "TabIds.h"
#include "LanguageType.h"
#include "SourceGroupType.h"

#include "FilePath.h"
#include "RefreshInfo.h"
#include "ErrorCountInfo.h"
#include "GroupType.h"

Q_DECLARE_METATYPE(Id)
Q_DECLARE_METATYPE(TabId)
Q_DECLARE_METATYPE(LanguageType)
Q_DECLARE_METATYPE(SourceGroupType)
Q_DECLARE_METATYPE(FilePath)
Q_DECLARE_METATYPE(RefreshMode)
Q_DECLARE_METATYPE(GroupType)
Q_DECLARE_METATYPE(ErrorCountInfo)

#endif // QT_META_TYPES_H
