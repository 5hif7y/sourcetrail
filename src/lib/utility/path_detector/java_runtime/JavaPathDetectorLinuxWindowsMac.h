#ifndef JAVA_PATH_DETECTOR_LINUX_WINDOWS_MAC_H
#define JAVA_PATH_DETECTOR_LINUX_WINDOWS_MAC_H

#include "JavaPathDetector.h"

class JavaPathDetectorLinuxWindowsMac : public JavaPathDetector
{
public:
	JavaPathDetectorLinuxWindowsMac(const std::string &name);

private:
	std::vector<FilePath> doGetPaths() const override;
};

#endif
