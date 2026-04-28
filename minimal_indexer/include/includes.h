#ifndef INDEXER_INCLUDES_H
#define INDEXER_INCLUDES_H

#include "AppPath.h"
#include "UserPaths.h"

inline void setupPlatform(int argc, char* argv[]) {}

inline void setupApp(int argc, char* argv[])
{
    // Minimal mock for indexer standalone
    AppPath::setSharedDataDirectoryPath(FilePath(L"."));
    UserPaths::setUserDataDirectoryPath(FilePath(L"."));
}

#endif // INDEXER_INCLUDES_H
