#pragma once

#include <cstdint>
#include <string>

#include "nlohmann/json.hpp"

namespace Algiz {
	struct Options {
		int addressFamily = -1;
		std::string ip;
		uint16_t port = 0;
		nlohmann::json jsonObject;

		static Options parse(int argc, char * const *argv);
	};
}
