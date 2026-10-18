#ifndef LOCATION_TYPE_H
#define LOCATION_TYPE_H

#include "utilityEnum.h"

enum class LocationType
{
	TOKEN = 0,
	SCOPE = 1,
	QUALIFIER = 2,
	LOCAL_SYMBOL = 3,
	SIGNATURE = 4,
	COMMENT = 5,
	ERROR = 6,
	FULLTEXT_SEARCH = 7,
	SCREEN_SEARCH = 8,
	UNSOLVED = 9
};

template <>
LocationType intToEnum(int value);

std::size_t operator << (std::size_t bits, LocationType locationType);

#endif	  // LOCATION_TYPE_H
