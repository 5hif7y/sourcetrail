#ifndef TOKEN_COMPONENT_CONST_H
#define TOKEN_COMPONENT_CONST_H

#include "TokenComponent.h"

class TokenComponentConst: public TokenComponent
{
public:
	std::shared_ptr<TokenComponent> copy() const override;
};

#endif	  // TOKEN_COMPONENT_CONST_H
