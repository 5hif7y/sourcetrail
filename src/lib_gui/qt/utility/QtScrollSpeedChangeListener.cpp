#include "QtScrollSpeedChangeListener.h"

#include <cmath>

#include <QScrollBar>

#include "ApplicationSettings.h"

QtScrollSpeedChangeListener::QtScrollSpeedChangeListener()
	: m_changeScrollSpeedFunctor([this](float scrollSpeed) { doChangeScrollSpeed(scrollSpeed); })
{
}

void QtScrollSpeedChangeListener::setScrollBar(QScrollBar* scrollbar)
{
	m_scrollBar = scrollbar;
	m_singleStep = scrollbar->singleStep();

	doChangeScrollSpeed(ApplicationSettings::getInstance()->getScrollSpeed());
}

void QtScrollSpeedChangeListener::handleMessage(MessageScrollSpeedChange* message)
{
	m_changeScrollSpeedFunctor(message->scrollSpeed);
}

void QtScrollSpeedChangeListener::doChangeScrollSpeed(float scrollSpeed)
{
	if (m_scrollBar)
	{
		m_scrollBar->setSingleStep(static_cast<int>(std::ceil(m_singleStep * scrollSpeed)));
	}
}
