#ifndef MAVEN_PATH_DETECTOR_UNIX_WINDOWS_H
#define MAVEN_PATH_DETECTOR_UNIX_WINDOWS_H

#include "PathDetector.h"

class MavenPathDetectorUnixWindows : public PathDetector
{
public:
	MavenPathDetectorUnixWindows(const std::string &name);

private:
	std::vector<FilePath> doGetPaths() const override;
};

#endif
