#pragma once

#include <functional>
#include <string>

namespace Algiz {
	long parseLong(const std::string &, int base = 10);
	long parseLong(const char *, int base = 10);
	unsigned long parseUlong(const std::string &, int base = 10);
	unsigned long parseUlong(const char *, int base = 10);
	std::string toLower(std::string);
	std::string toUpper(std::string);

	template<typename R, typename O1, typename O2, typename... A>
	auto bind(O1 &obj, R(O2::*func)(A...)) {
		return std::function<R(A...)>([&obj, func](A && ...args) -> R {
			return std::invoke(func, obj, std::forward<A>(args)...);
		});
	}
}
