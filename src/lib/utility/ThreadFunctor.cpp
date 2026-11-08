#include "ThreadFunctor.h"

ThreadFunctorFactory& ThreadFunctorFactory::getInstance()
{
	static ThreadFunctorFactory instance;
	return instance;
}

void ThreadFunctorFactory::setFactory(std::function<std::unique_ptr<ThreadFunctor>()> factory)
{
	m_factory = std::move(factory);
}

std::unique_ptr<ThreadFunctor> ThreadFunctorFactory::create()
{
	if (m_factory)
		return m_factory();
	return std::make_unique<ThreadFunctor>();
}
