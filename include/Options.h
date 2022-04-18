#pragma once

#include <cstdint>
#include <string>

namespace Algiz {
	struct Options {
		int addressFamily = -1;
		std::string ip;
		uint16_t port = 0;
		std::string root;

		static Options parse(int argc, char * const *argv);
	};
}
