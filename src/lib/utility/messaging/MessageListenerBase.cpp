#include "MessageListenerBase.h"
#include "MessageQueue.h"

MessageListenerBase::MessageListenerBase()
{
	MessageQueue::getInstance()->registerListener(this);
	m_registered = true;
}

MessageListenerBase::~MessageListenerBase()
{
	unregister();
}

void MessageListenerBase::unregister()
{
	if (m_registered)
	{
		MessageQueue::getInstance()->unregisterListener(this);
		m_registered = false;
	}
}

TabId MessageListenerBase::getSchedulerId() const
{
	return TabId::NONE;
}
