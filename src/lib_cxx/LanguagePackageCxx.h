#ifndef LANGUAGE_PACKAGE_CXX_H
#define LANGUAGE_PACKAGE_CXX_H

#include "LanguagePackage.h"

class LanguagePackageCxx: public LanguagePackage
{
public:
	std::vector<std::shared_ptr<IndexerBase>> instantiateSupportedIndexers() const override;
};

#endif	  // LANGUAGE_PACKAGE_CXX_H
