#ifndef TOKEN_COMPONENT_STATIC_H
#define TOKEN_COMPONENT_STATIC_H

#include "TokenComponent.h"

class TokenComponentStatic: public TokenComponent
{
public:
	std::shared_ptr<TokenComponent> copy() const override;
};

#endif	  // TOKEN_COMPONENT_STATIC_H
