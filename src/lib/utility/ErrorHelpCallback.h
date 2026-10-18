#ifndef ERROR_HELP_CALLBACK_H
#define ERROR_HELP_CALLBACK_H

#include <functional>

class ErrorHelpCallback
{
public:
	static void setCallback(std::function<void()> callback);
	static void show();

private:
	static std::function<void()> s_callback;
};

#endif // ERROR_HELP_CALLBACK_H
