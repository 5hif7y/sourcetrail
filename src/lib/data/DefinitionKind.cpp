#include "DefinitionKind.h"

namespace
{

const DefinitionKind DEFINITION_KINDS[] = {
	DefinitionKind::NONE,
	DefinitionKind::IMPLICIT,
	DefinitionKind::EXPLICIT
};

}

template<>
DefinitionKind intToEnum(int value)
{
	return lookupEnum(value, DEFINITION_KINDS, DefinitionKind::NONE);
}
