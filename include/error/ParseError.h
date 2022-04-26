#pragma once

#include <stdexcept>
#include <string>

namespace Algiz {
	struct ParseError: std::runtime_error {
		explicit ParseError(const std::string &message): std::runtime_error(message) {}
	};
}
