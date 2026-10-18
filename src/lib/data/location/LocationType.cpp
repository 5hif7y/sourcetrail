#include "LocationType.h"

using namespace std;

namespace
{

const LocationType LOCATION_TYPES[] = {
	LocationType::TOKEN,
	LocationType::SCOPE,
	LocationType::QUALIFIER,
	LocationType::LOCAL_SYMBOL,
	LocationType::SIGNATURE,
	LocationType::COMMENT,
	LocationType::ERROR,
	LocationType::FULLTEXT_SEARCH,
	LocationType::SCREEN_SEARCH,
	LocationType::UNSOLVED
};

}

template <>
LocationType intToEnum(int value)
{
	return lookupEnum(value, LOCATION_TYPES, LocationType::TOKEN);
}

size_t operator << (size_t bits, LocationType locationType)
{
	return bits << static_cast<int>(locationType);
}
