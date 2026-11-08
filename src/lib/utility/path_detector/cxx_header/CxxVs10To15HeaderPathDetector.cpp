#include "CxxVs10To15HeaderPathDetector.h"

#include "FilePath.h"
#include "logging.h"
#include "utility.h"
#include "utilityCxxHeaderDetection.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>

using namespace utility;
using namespace std::string_literals;

using Architecture = Platform::Architecture;


static FilePath getVsInstallPathUsingRegistry(const LegacyVisualStudio::Version &version, bool isExpress, Architecture architecture)
{
#ifdef _WIN32
	std::string keyStr = "SOFTWARE\\";
	if (architecture == Architecture::BITS_32)
	{
		keyStr += "Wow6432Node\\";
	}
	keyStr += "Microsoft\\";
	keyStr += (isExpress ? "VCExpress" : "VisualStudio");
	keyStr += "\\" + version.number;

	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyStr.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		char buffer[512];
		DWORD bufferSize = sizeof(buffer);
		if (RegQueryValueExA(hKey, "InstallDir", nullptr, nullptr, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			if (bufferSize > 0 && buffer[bufferSize - 1] == '\0') {
				bufferSize--;
			}
			std::string value = std::string(buffer, bufferSize) + "../../";
			FilePath path(value);
			if (path.exists())
			{
				LOG_INFO("Found working registry key for VS install path: HKEY_LOCAL_MACHINE\\" + keyStr);
				return path;
			}
			return FilePath();
		}
		RegCloseKey(hKey);
	}
#endif
	return FilePath();
}

CxxVs10To15HeaderPathDetector::CxxVs10To15HeaderPathDetector(const LegacyVisualStudio::Version &version, bool isExpress, Architecture architecture)
	: PathDetector(version.name + (isExpress ? " Express" : " ") + Platform::getArchitectureName(architecture))
	, m_version(version)
	, m_isExpress(isExpress)
	, m_architecture(architecture)
{
}


std::vector<FilePath> CxxVs10To15HeaderPathDetector::doGetPaths() const
{
	const FilePath vsInstallPath = getVsInstallPathUsingRegistry(m_version, m_isExpress, m_architecture);

	// vc++ headers
	std::vector<FilePath> headerSearchPaths;
	if (vsInstallPath.exists())
	{
		for (const std::string& subdirectory: {"vc/include"s, "vc/atlmfc/include"s})
		{
			FilePath headerSearchPath = vsInstallPath.getConcatenated(subdirectory);
			if (headerSearchPath.exists())
			{
				headerSearchPaths.push_back(headerSearchPath.makeCanonical());
			}
		}
	}

	if (!headerSearchPaths.empty())
	{
		utility::append(headerSearchPaths, utility::getWindowsSdkHeaderSearchPaths(m_architecture));
	}

	return headerSearchPaths;
}

