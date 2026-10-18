#include "ErrorHelpCallback.h"

std::function<void()> ErrorHelpCallback::s_callback;

void ErrorHelpCallback::setCallback(std::function<void()> callback)
{
	s_callback = std::move(callback);
}

void ErrorHelpCallback::show()
{
	if (s_callback)
	{
		s_callback();
	}
}
