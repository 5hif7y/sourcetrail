#ifndef MESSAGE_LISTENER_BASE_H
#define MESSAGE_LISTENER_BASE_H

#include "MessageBase.h"

#include <string>

class MessageListenerBase
{
public:
	MessageListenerBase();
	virtual ~MessageListenerBase();

	// Idempotent. Classes whose destructor can race with message dispatch may
	// call this at the start of their destructor, before their vtable changes.
	void unregister();

	virtual std::string getType() const = 0;

	virtual void handleMessageBase(MessageBase* message) = 0;

	virtual TabId getSchedulerId() const;

private:
	bool m_registered = false;
};

#endif	  // MESSAGE_LISTENER_BASE_H
