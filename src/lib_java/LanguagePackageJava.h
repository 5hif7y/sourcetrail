#ifndef LANGUAGE_PACKAGE_JAVA_H
#define LANGUAGE_PACKAGE_JAVA_H

#include "LanguagePackage.h"

class LanguagePackageJava: public LanguagePackage
{
public:
	std::vector<std::shared_ptr<IndexerBase>> instantiateSupportedIndexers() const override;
};

#endif	  // LANGUAGE_PACKAGE_JAVA_H
