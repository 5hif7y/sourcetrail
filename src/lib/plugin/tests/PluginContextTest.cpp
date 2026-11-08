#include "PluginContext.h"

#include <cassert>
#include <string>
#include <vector>

using namespace st::plugin;

int main()
{
	{
		Context context;
		std::vector<int> order;
		context.effect([&order]() {
			order.push_back(1);
			return [&order]() { order.push_back(-1); };
		});
		context.effect([&order]() {
			order.push_back(2);
			return [&order]() { order.push_back(-2); };
		});
		context.dispose();
		assert((order == std::vector<int>{1, 2, -2, -1}));
	}

	{
		Context root;
		root.provide<int>("answer", std::make_shared<int>(42));
		assert(root.require<int>("answer") == 42);

		Component child;
		child.id = "child";
		child.inject = {"answer"};
		child.apply = [](Context& context) {
			context.effect([]() { return []() {}; });
		};

		Fiber fiber = root.use(child);
		assert(fiber.active());
		assert(fiber.context() != nullptr);
		fiber.unload();
		assert(!fiber.active());
		assert(root.require<int>("answer") == 42);
	}

	{
		Context root;
		Component missing;
		missing.id = "missing";
		missing.inject = {"not-provided"};
		Fiber fiber = root.use(missing);
		assert(!fiber.active());
	}

	{
		Context root;
		root.provide<int>("number", std::make_shared<int>(7));
		const int before = root.require<int>("number");
		{
			Context child(&root);
			child.provide<int>("number", std::make_shared<int>(9));
			assert(child.require<int>("number") == 9);
		}
		assert(root.require<int>("number") == before);
	}

	return 0;
}
