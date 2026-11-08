#ifndef STORAGE_STATS_H
#define STORAGE_STATS_H

#include "TimeStamp.h"

struct StorageStats
{
	StorageStats() = default;

	size_t nodeCount = 0;
	size_t edgeCount = 0;

	size_t fileCount = 0;
	size_t completedFileCount = 0;
	size_t fileLOCCount = 0;

	TimeStamp timestamp;
};

#endif	  // STORAGE_STATS_H
