#ifndef TOKEN_COMPONENT_INHERITANCE_CHAIN_H
#define TOKEN_COMPONENT_INHERITANCE_CHAIN_H

#include "TokenComponent.h"

class TokenComponentInheritanceChain: public TokenComponent
{
public:
	TokenComponentInheritanceChain(const std::vector<Id>& inheritanceEdgeIds)
		: inheritanceEdgeIds(inheritanceEdgeIds)
	{
	}

	std::shared_ptr<TokenComponent> copy() const override
	{
		return std::make_shared<TokenComponentInheritanceChain>(*this);
	}

	const std::vector<Id> inheritanceEdgeIds;
};

#endif	  // TOKEN_COMPONENT_INHERITANCE_CHAIN_H
