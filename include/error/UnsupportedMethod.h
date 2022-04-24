#pragma once

#include <stdexcept>
#include <string>

namespace Algiz {
	struct UnsupportedMethod: std::runtime_error {
		UnsupportedMethod(const std::string &message): std::runtime_error(message) {}
	};
}
