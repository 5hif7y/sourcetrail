#ifndef THREAD_FUNCTOR_H
#define THREAD_FUNCTOR_H

#include <functional>
#include <memory>

// Abstract interface for marshaling function calls to a specific thread (e.g., GUI thread).
// The default implementation runs functions immediately on the calling thread.
class ThreadFunctor
{
public:
	virtual ~ThreadFunctor() = default;
	virtual void operator()(std::function<void()> func) { func(); }
};

class ThreadFunctorFactory
{
public:
	static ThreadFunctorFactory& getInstance();
	
	void setFactory(std::function<std::unique_ptr<ThreadFunctor>()> factory);
	std::unique_ptr<ThreadFunctor> create();

private:
	ThreadFunctorFactory() = default;
	std::function<std::unique_ptr<ThreadFunctor>()> m_factory;
};

#endif // THREAD_FUNCTOR_H
