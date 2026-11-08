#ifndef TEST_FILE_MANAGER_H
#define TEST_FILE_MANAGER_H

#include "FileRegister.h"

class TestFileRegister: public FileRegister
{
public:
	TestFileRegister();
	~TestFileRegister() override;
	bool hasFilePath(const FilePath& filePath) const override;
};

#endif	  // TEST_FILE_MANAGER_H
