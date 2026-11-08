#ifndef BASE_INTERPROCESS_DATA_MANAGER_H
#define BASE_INTERPROCESS_DATA_MANAGER_H

#include <string>

#include "SharedMemory.h"
#include "utilityEnum.h"

enum class ProcessId : std::size_t 
{
	NONE = 0,
	
	INVALID = MAX_ENUM_VALUE<ProcessId>
};

class BaseInterprocessDataManager
{
public:
	BaseInterprocessDataManager(
		const std::string& sharedMemoryName,
		size_t initialSharedMemorySize,
		const std::string& instanceUuid,
		ProcessId processId,
		bool isOwner);

	virtual ~BaseInterprocessDataManager() = default;

	ProcessId getProcessId() const;

protected:
	SharedMemory m_sharedMemory;

	const std::string m_instanceUuid;
	const ProcessId m_processId;
};

#endif	  // BASE_INTERPROCESS_DATA_MANAGER_H
