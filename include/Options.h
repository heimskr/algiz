#pragma once

#include <string>

namespace Algiz {
	struct Options {
		std::string ip;
		unsigned short port;
		std::string root;

		static Options parse(int argc, char * const *argv);
	};
}
