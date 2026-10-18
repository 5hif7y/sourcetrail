#ifndef STORAGE_SYMBOL_H
#define STORAGE_SYMBOL_H

#include "DefinitionKind.h"
#include "Id.h"

struct StorageSymbol
{
	StorageSymbol(): id(0), definitionKind(DefinitionKind::NONE) {}

	StorageSymbol(Id id, DefinitionKind definitionKind): id(id), definitionKind(definitionKind) {}

	Id id;
	DefinitionKind definitionKind;
};

#endif	  // STORAGE_SYMBOL_H
