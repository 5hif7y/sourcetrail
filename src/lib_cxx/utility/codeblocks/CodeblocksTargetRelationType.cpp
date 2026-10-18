#include "CodeblocksTargetRelationType.h"

namespace Codeblocks
{
}	 // namespace Codeblocks

template <>
Codeblocks::TargetRelationType intToEnum(int value)
{
	switch (value)
	{
		case Codeblocks::CODEBLOCKS_TARGET_RELATION_IGNORE_TARGET:
			return Codeblocks::CODEBLOCKS_TARGET_RELATION_IGNORE_TARGET;
		case Codeblocks::CODEBLOCKS_TARGET_RELATION_IGNORE_PROJECT:
			return Codeblocks::CODEBLOCKS_TARGET_RELATION_IGNORE_PROJECT;
		case Codeblocks::CODEBLOCKS_TARGET_RELATION_TARGET_BEFORE_PROJECT:
			return Codeblocks::CODEBLOCKS_TARGET_RELATION_TARGET_BEFORE_PROJECT;
		case Codeblocks::CODEBLOCKS_TARGET_RELATION_TARGET_AFTER_PROJECT:
			return Codeblocks::CODEBLOCKS_TARGET_RELATION_TARGET_AFTER_PROJECT;
	}
	return Codeblocks::CODEBLOCKS_TARGET_RELATION_UNKNOWN;
}
