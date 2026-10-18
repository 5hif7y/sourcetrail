#include "Catch2.hpp"

#include "Platform.h"

using namespace utility;

TEST_CASE("platform_get_architecture")
{
	const Platform::Architecture architecture = Platform::getArchitecture();
	
	REQUIRE((architecture == Platform::Architecture::BITS_32 || architecture == Platform::Architecture::BITS_64));
}
