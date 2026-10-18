#include "ElementComponentKind.h"

template <>
ElementComponentKind intToEnum(int value)
{
	switch (value)
	{
		case ELEMENT_COMPONENT_IS_AMBIGUOUS:
			return ELEMENT_COMPONENT_IS_AMBIGUOUS;
	}
	return ELEMENT_COMPONENT_NONE;
}
