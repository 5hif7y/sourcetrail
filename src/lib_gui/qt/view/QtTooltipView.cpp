#include "QtTooltipView.h"

#include "QtMainView.h"
#include "QtMainWindow.h"
#include "QtTooltip.h"
#include "QtViewWidgetWrapper.h"
#include "utilityQt.h"
#include "QtResources.h"

using namespace utility;

QtTooltipView::QtTooltipView(ViewLayout* viewLayout): TooltipView(viewLayout)
{
	m_widget = new QtTooltip(utility::getMainWindowforMainView(viewLayout));
}

void QtTooltipView::createWidgetWrapper()
{
	setWidgetWrapper(std::make_shared<QtViewWidgetWrapper>(m_widget));
}

void QtTooltipView::refreshView()
{
	m_onQtThread([=, this]()
	{
		m_widget->setStyleSheet(QtResources::loadStyleSheet(QtResources::TOOLTIP_VIEW_CSS));
	});
}

void QtTooltipView::showTooltip(const TooltipInfo& info, const View* parent)
{
	m_onQtThread([=, this]() {
		if (m_widget->isHovered())
		{
			return;
		}

		m_widget->hide();
		m_widget->setTooltipInfo(info);

		if (parent)
		{
			m_widget->setParentView(QtViewWidgetWrapper::getWidgetOfView(parent)->parentWidget());
		}

		m_widget->show();
	});
}

void QtTooltipView::hideTooltip(bool force)
{
	m_onQtThread([=, this]() { m_widget->hide(force); });
}

bool QtTooltipView::tooltipVisible() const
{
	return m_widget->isVisible();
}
