#pragma once

#include <stdexcept>
#include <string>

namespace Algiz {
	struct ParseError: std::runtime_error {
		ParseError(const std::string &message): std::runtime_error(message) {}
	};
}
