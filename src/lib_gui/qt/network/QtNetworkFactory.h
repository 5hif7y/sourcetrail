#ifndef QT_NETWORK_FACTORY_H
#define QT_NETWORK_FACTORY_H

#include "NetworkFactory.h"

class QtNetworkFactory: public NetworkFactory
{
public:
	QtNetworkFactory();
	~QtNetworkFactory() override;

	std::shared_ptr<IDECommunicationController> createIDECommunicationController(
		StorageAccess* storageAccess) const override;
};

#endif	  // QT_NETWORK_FACTORY_H
