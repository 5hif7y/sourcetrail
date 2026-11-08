#ifndef RESOURCE_PROVIDER_H
#define RESOURCE_PROVIDER_H

#include <string>
#include <unordered_map>

// Provides resource paths abstracted from the GUI framework.
// Resource keys correspond to the old QtResources constants.
class ResourceProvider
{
public:
	static ResourceProvider& getInstance();

	void registerResource(const std::string& key, const std::string& path);
	const std::string& getResource(const std::string& key) const;

private:
	ResourceProvider() = default;
	std::unordered_map<std::string, std::string> m_resources;
	static const std::string s_empty;
};

#endif // RESOURCE_PROVIDER_H
