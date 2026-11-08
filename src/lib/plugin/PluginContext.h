#ifndef SOURCETRAIL_PLUGIN_CONTEXT_H
#define SOURCETRAIL_PLUGIN_CONTEXT_H

// Minimal C++ core for the Cordis-style plugin model described in
// docs/Claude-Code/21_arquitectura_plugin_cordis_2026-10-18.md.
//
// This is the F1 building block:
//  - Effect: an exception-safe, single-shot inverse (LIFO disposal).
//  - Context: service provision/lookup through reversible effects.
//  - Component/Fiber: declared dependencies, activation and deactivation.
//
// It is header-only and Qt-free so it can be used by the embedded CLI, the
// desktop app and the language packages without pulling new dependencies.

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace st
{
namespace plugin
{
using Dispose = std::function<void()>;

class Effect
{
public:
	Effect() = default;
	explicit Effect(Dispose dispose): m_dispose(std::move(dispose)) {}

	~Effect()
	{
		reset();
	}

	Effect(Effect&& other) noexcept: m_dispose(std::move(other.m_dispose))
	{
		other.m_dispose = nullptr;
	}

	Effect& operator=(Effect&& other) noexcept
	{
		if (this != &other)
		{
			reset();
			m_dispose = std::move(other.m_dispose);
			other.m_dispose = nullptr;
		}
		return *this;
	}

	Effect(const Effect&) = delete;
	Effect& operator=(const Effect&) = delete;

	void reset()
	{
		if (m_dispose)
		{
			Dispose dispose = std::move(m_dispose);
			m_dispose = nullptr;
			dispose();
		}
	}

	explicit operator bool() const
	{
		return static_cast<bool>(m_dispose);
	}

private:
	Dispose m_dispose;
};

enum class FiberState
{
	Inactive,
	Active
};

class Context;

struct Component
{
	std::string id;
	std::vector<std::string> inject;
	std::function<void(Context&)> apply;
};

class Fiber
{
public:
	Fiber() = default;
	Fiber(std::unique_ptr<Context> context, FiberState state);
	~Fiber();

	Fiber(Fiber&&) noexcept = default;
	Fiber& operator=(Fiber&&) noexcept = default;
	Fiber(const Fiber&) = delete;
	Fiber& operator=(const Fiber&) = delete;

	bool active() const
	{
		return m_state == FiberState::Active;
	}

	Context* context()
	{
		return m_context.get();
	}

	void unload();

private:
	std::unique_ptr<Context> m_context;
	FiberState m_state = FiberState::Inactive;
};

class Context
{
public:
	explicit Context(Context* parent = nullptr): m_parent(parent) {}
	~Context();

	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;
	Context(Context&&) = delete;
	Context& operator=(Context&&) = delete;

	template <class T>
	void provide(const std::string& key, std::shared_ptr<T> service)
	{
		effect([this, key, service]() {
			std::any previous;
			bool hadPrevious = false;
			auto it = m_services.find(key);
			if (it != m_services.end())
			{
				previous = it->second;
				hadPrevious = true;
			}
			m_services[key] = std::any(service);

			return [this, key, hadPrevious, previous]() {
				if (hadPrevious)
				{
					m_services[key] = previous;
				}
				else
				{
					m_services.erase(key);
				}
			};
		});
	}

	template <class T>
	T& require(const std::string& key)
	{
		auto it = m_services.find(key);
		if (it != m_services.end())
		{
			std::shared_ptr<T>* service = std::any_cast<std::shared_ptr<T>>(&it->second);
			if (!service || !*service)
			{
				throw std::runtime_error("service type mismatch: " + key);
			}
			return **service;
		}
		if (m_parent)
		{
			return m_parent->require<T>(key);
		}
		throw std::runtime_error("missing service: " + key);
	}

	bool has_service(const std::string& key) const
	{
		if (m_services.find(key) != m_services.end())
		{
			return true;
		}
		return m_parent ? m_parent->has_service(key) : false;
	}

	template <class F>
	void effect(F&& body)
	{
		m_effects.emplace_back(body());
	}

	void dispose();

	Fiber use(const Component& component);

private:
	Context* m_parent = nullptr;
	std::vector<Effect> m_effects;
	std::unordered_map<std::string, std::any> m_services;
};

inline Context::~Context()
{
	dispose();
}

inline void Context::dispose()
{
	while (!m_effects.empty())
	{
		Effect effect = std::move(m_effects.back());
		m_effects.pop_back();
		effect.reset();
	}
}

inline Fiber Context::use(const Component& component)
{
	std::unique_ptr<Context> child = std::make_unique<Context>(this);
	for (const std::string& dependency: component.inject)
	{
		if (!has_service(dependency))
		{
			return Fiber(std::move(child), FiberState::Inactive);
		}
	}
	if (component.apply)
	{
		component.apply(*child);
	}
	return Fiber(std::move(child), FiberState::Active);
}

inline Fiber::Fiber(std::unique_ptr<Context> context, FiberState state):
	m_context(std::move(context)), m_state(state)
{
}

inline Fiber::~Fiber()
{
	unload();
}

inline void Fiber::unload()
{
	if (m_context)
	{
		m_context->dispose();
	}
	m_state = FiberState::Inactive;
}
}	 // namespace plugin
}	 // namespace st

#endif	  // SOURCETRAIL_PLUGIN_CONTEXT_H
