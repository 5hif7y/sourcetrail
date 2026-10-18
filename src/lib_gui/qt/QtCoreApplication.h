#ifndef QT_COREAPPLICATION_H
#define QT_COREAPPLICATION_H

#include <QCoreApplication>

#include "MessageIndexingStatus.h"
#include "MessageListener.h"
#include "MessageQuitApplication.h"
#include "MessageStatus.h"

class QtCoreApplication
	: public QCoreApplication
	, public MessageListener<MessageQuitApplication>
	, public MessageListener<MessageIndexingStatus>
	, public MessageListener<MessageStatus>
{
	Q_OBJECT

public:
	QtCoreApplication(int argc, char** argv);
	~QtCoreApplication() override = default;

private:
	void handleMessage(MessageQuitApplication* message) override;
	void handleMessage(MessageIndexingStatus* message) override;
	void handleMessage(MessageStatus* message) override;
};

#endif	  // QT_COREAPPLICATION
