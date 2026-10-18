#ifndef DEFINITION_KIND_H
#define DEFINITION_KIND_H

#include "utilityEnum.h"

// these values need to be the same as DefinitionKind in Java code
enum class DefinitionKind
{	 
	NONE = 0,
	IMPLICIT = 1,
	EXPLICIT = 2
};

template<>
DefinitionKind intToEnum(int definitionKind);

#endif	  // DEFINITION_TYPE_H
